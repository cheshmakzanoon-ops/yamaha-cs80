// Diagnostic instruction profile, not a native timing benchmark.
// Reuses the stock benchmark's exact source factory controls and notes.
#define main cs80StockBenchmarkMain
#include "src/tools/source_factory_benchmark.cpp"
#undef main
#include <valgrind/callgrind.h>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const std::string scenario = argv[1];
    const int blocks = std::stoi(argv[2]);
    if (blocks < 1 || blocks > 1024) return 2;
    Case c;
    c.rate = 48000; c.block = 256; c.isolated = true;
    c.idle = scenario == "idle_quiet" || scenario == "idle_noise";
    c.noise = scenario == "idle_noise";
    c.moving = scenario == "moving";
    c.selector = scenario == "pair10" ? 10 : 0;
    if (!c.idle && !c.moving && scenario != "pair0" && scenario != "pair10") return 2;
    ScopedDenormals denormals;
    cs80::Engine engine;
    engine.prepare(c.rate);
    std::array<ControlFrame, 11> frames;
    for (int i = 0; i < 11; ++i) frames[i] = makeFrame(c, i);
    require(engine.setControlFrame(frames[c.selector]), "Initial ControlFrame rejected");
    require(engine.getRenderPolicy() == renderPolicy(1.0f, 0.0f), "Unexpected policy");
    for (int rank = 0; rank < 2; ++rank) {
        const auto info = engine.getSourceToneInfo(rank);
        require(info.selector == c.selector && info.route == source_factory::route(rank, c.selector, true), "Unexpected route");
    }
    engine.reset();
    engine.setExpression(0.8f);
    if (!c.idle) for (int note : notes) engine.noteOn(note, 0.8f);
    std::vector<float> audio(c.block * 2);
    const int warmup = static_cast<int>(std::ceil(warmupSeconds * c.rate / c.block));
    for (int i = 0; i < warmup; ++i) {
        if (i == warmup - 32) { CALLGRIND_START_INSTRUMENTATION; }
        require(engine.setControlFrame(frames[c.selector]), "Warmup ControlFrame rejected");
        engine.render(audio.data(), c.block);
    }
    require(engine.activeVoices() == (c.idle ? 0 : 8), "Unexpected warmed voice count");
    int selector = c.selector;
    double checksum = 0;
    CALLGRIND_ZERO_STATS;
    for (int i = 0; i < blocks; ++i) {
        if (c.moving && i % 32 == 0) selector = (selector + 1) % 11;
        const double phase = std::fmod(static_cast<double>(i) * c.block / c.rate * 0.75, 1.0);
        const float expression = 0.15f + 0.8f * static_cast<float>(1.0 - std::abs(2.0 * phase - 1.0));
        const float pressure = 0.1f + 0.8f * static_cast<float>(phase);
        CALLGRIND_TOGGLE_COLLECT;
        const bool accepted = engine.setControlFrame(frames[selector]);
        if (c.moving) {
            engine.setExpression(expression);
            for (int note : notes) engine.polyPressure(note, pressure);
        }
        engine.render(audio.data(), c.block);
        CALLGRIND_TOGGLE_COLLECT;
        require(accepted, "Profile ControlFrame rejected");
        require(engine.activeVoices() == (c.idle ? 0 : 8), "Profile voice count changed");
        for (float value : audio) {
            require(std::isfinite(value), "Nonfinite profile audio");
            checksum += value;
        }
    }
    CALLGRIND_DUMP_STATS;
    CALLGRIND_STOP_INSTRUMENTATION;
    std::cout << "scenario=" << scenario << " blocks=" << blocks << " rate=48000 frames=256 voices="
              << engine.activeVoices() << " checksum=" << std::setprecision(17) << checksum << '\n';
    return 0;
}
