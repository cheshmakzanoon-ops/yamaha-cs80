// Isolated whole-core cost fixture, not a plugin callback or a DAW benchmark.
// The imported shipping fixture supplies the exact parameter adapter and notes.
#define main shipping_fixture_main_not_called
#include "fixture/source_factory_benchmark.cpp"
#undef main
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sched.h>

#ifndef LANE_COST_DIAGNOSTIC
#define LANE_COST_DIAGNOSTIC 0
#endif
#ifndef LANE_COST_OBSERVERS
#define LANE_COST_OBSERVERS 0
#endif
static_assert(!LANE_COST_OBSERVERS || LANE_COST_DIAGNOSTIC);
#if LANE_COST_OBSERVERS
// Supplied by a separate counted-only --wrap link. Never ordinary core objects.
extern "C" void cs80_lane_count_reset();
extern "C" unsigned long long cs80_lane_count_hits();
#endif

namespace lane_cost {
constexpr unsigned warm = 1024, measured = 512, blockSize = 64;
constexpr const char* labels[] = {"factory-moving", "factory-held", "panel-moving", "pure-held"};
constexpr std::uint64_t hashInitial = 1469598103934665603ull;
using Pcm = std::array<float, blockSize * 2>;
struct Fp { unsigned csr; unsigned short cw, sw; int error; };
Fp fp() noexcept {
    Fp result{};
    asm volatile("stmxcsr %0; fnstcw %1; fnstsw %2" : "=m"(result.csr), "=m"(result.cw), "=m"(result.sw) : : "memory");
    result.error = errno;
    return result;
}
bool same(Fp a, Fp b) noexcept { return a.csr == b.csr && a.cw == b.cw && a.sw == b.sw && a.error == b.error; }
void hashWord(std::uint64_t& h, std::uint64_t word) noexcept { h ^= word; h *= 1099511628211ull; }
std::uint64_t pcmHash(const Pcm& pcm) noexcept {
    auto h = hashInitial;
    for (float x : pcm) hashWord(h, std::bit_cast<std::uint32_t>(x));
    return h;
}
std::uint64_t frameHash(const ControlFrame& frame) noexcept {
    auto h = hashInitial;
    for (int p = 0; p < P_numParams; ++p) hashWord(h, std::bit_cast<std::uint32_t>(frame.effectiveValues[p]));
    for (int selector : frame.hardwareSources) hashWord(h, selector);
    hashWord(h, frame.modelVersion);
    return h;
}
[[gnu::noinline]] std::uint64_t cpuNs() {
    timespec time{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time)) std::abort();
    return static_cast<std::uint64_t>(time.tv_sec) * 1000000000ull + time.tv_nsec;
}
void printFp(Fp f) {
    std::printf("{\"mxcsr\":%u,\"x87_cw\":%u,\"x87_sw\":%u,\"errno\":%d}", f.csr, f.cw, f.sw, f.error);
}
struct Work {
    const int index;
    const bool moving;
    Case config;
    ControlFrame frame;
    std::unique_ptr<Engine> engine;
    Pcm pcm{};
    explicit Work(int i) : index(i), moving(i == 0 || i == 2), engine(std::make_unique<Engine>()) {
        config.rate = 48000; config.block = blockSize; config.selector = i == 2 ? 13 : 0;
        config.isolated = i == 3; config.sourcePerformance = i != 3;
        config.performanceProfile = moving ? 1 : 0;
        frame = makeFrame(config, config.selector);
        frame.effectiveValues[P_drift] = .37f;
        frame.effectiveValues[P_voiceDispersion] = .18f;
        frame.effectiveValues[P_driftModel] = 0.f;
        engine->prepare(config.rate);
        if (!engine->setControlFrame(frame)) std::abort();
        engine->reset();
        for (int voice = 0; voice < 8; ++voice) engine->noteOn(notes[voice], .8f, moving ? voice + 2 : 1);
    }
    // Cadence and event formula come from profile.cpp. Order is deliberately
    // corrected to shipping PluginProcessor.cpp: normal frame BEFORE new MIDI.
    // EVERY callback includes the frame, not sparse no-op repeats. No bypass.
    [[gnu::noinline]] void step(unsigned b) {
        if (!engine->setControlFrame(frame)) std::abort();
        if (moving && b % 4 == 0) {
            const int voice = (b / 4) % 8;
            const float x = float((b * 13) % 97) / 96.f;
            engine->channelPressure(voice + 2, x);
            engine->pitchBend(voice + 2, x * .1f);
            engine->controlChange(74, x, voice + 2);
        }
        engine->render(pcm.data(), blockSize);
    }
};

#if LANE_COST_DIAGNOSTIC
struct Diagnostic {
    FILE* file{};
    std::uint64_t pcmAll = hashInitial, pcmMeasured = hashInitial, nonzeroMeasured{}, voiceChecks{}, routeChecks{};
    explicit Diagnostic(const char* path) {
        file = std::fopen(path, "wbx");
        if (!file) throw std::runtime_error("trace must be a new writable file");
    }
    ~Diagnostic() { if (file) std::fclose(file); }
    template<class T> void put(const T& v) { if (std::fwrite(&v, sizeof v, 1, file) != 1) std::abort(); }
    void putFp(Fp f) { put(f.csr); put(f.cw); put(f.sw); put(f.error); }
    void observe(Work& w, unsigned b, Fp before, Fp after) {
        // Read-only getters and the next-FP probe cannot seed flags for the next
        // render. Preserve all original fenv state, including x87 and errno.
        fenv_t saved{};
        if (fegetenv(&saved)) std::abort();
        const int savedErrno = errno;
        put(b); putFp(before); putFp(after);
        volatile double one = 1.0, ten = 10.0;
        volatile double probe = one / ten;
        put(std::bit_cast<std::uint64_t>(double(probe))); putFp(fp());
        for (float x : w.pcm) {
            const auto bits = std::bit_cast<std::uint32_t>(x);
            require((bits & 0x7f800000U) != 0x7f800000U, "nonfinite PCM");
            put(bits); hashWord(pcmAll, bits);
            if (b >= warm && b < warm + measured) {
                hashWord(pcmMeasured, bits); nonzeroMeasured += (bits & 0x7fffffffU) != 0;
            }
        }
        require(w.engine->activeVoices() == 8, "wrong active voice count");
        for (int voice = 0; voice < 8; ++voice) {
            const auto v = w.engine->getVoiceInfo(voice);
            require(v.active && v.held && !v.pedalHeld && v.note == notes[voice]
                    && v.channel == (w.moving ? voice + 2 : 1), "wrong held note/channel");
            put(std::uint8_t(v.active)); put(v.note); put(v.channel); put(std::uint8_t(v.held)); put(std::uint8_t(v.pedalHeld));
            for (float x : {v.velocity, v.pressure, v.bend, v.ribbonRatio, v.touchA, v.touchB, v.touchC, v.timbre})
                put(std::bit_cast<std::uint32_t>(x));
            ++voiceChecks;
        }
        for (int rank = 0; rank < 2; ++rank) {
            const auto s = w.engine->getSourceToneInfo(rank);
            require(s.selector == w.config.selector && s.route == source_factory::route(rank, w.config.selector, true), "wrong route");
            put(s.selector); put(static_cast<int>(s.route)); ++routeChecks;
        }
        require(w.engine->getRenderPolicy() == (w.index == 3 ? RenderPolicy::isolation : RenderPolicy::sourcePerformance), "wrong policy");
        require(w.engine->getOversamplingFactor() == 2 && w.engine->hasFilterBatchingKernel(), "unexpected internal rate or no batch kernel");
        put(w.engine->getSourceCompileCount());
        put(static_cast<int>(w.engine->getRenderPolicy())); put(w.engine->getOversamplingFactor());
        require((after.csr & 0xffc0U) == 0x9fc0U && after.cw == 0x037fU, "FP controls changed");
        if (fesetenv(&saved)) std::abort();
        errno = savedErrno;
        require(same(after, fp()), "diagnostic perturbed caller FP/errno");
    }
};
#endif
} // namespace lane_cost
