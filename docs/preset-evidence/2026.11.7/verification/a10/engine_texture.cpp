// A10 public-API Engine integration fixture, expanded from A02's flute probe.
// Compile separately against the before/after header tree and matching core.
#include "core/Engine.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
using namespace cs80;
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::filesystem::create_directories(argv[1]);
    for (int model : {body::flute, body::bowed, body::clarinet, body::brass})
    for (int rate : {44100, 48000, 96000})
    for (int quality : {0, 1, 2}) {
        ParamValues p;
        p[P_sourcePerformance] = 1;
        p[P_parityMode] = p[P_kasScanTiming] = p[P_analogNoise] = p[P_drift] =
        p[P_voiceDispersion] = p[P_chorusOn] = 0;
        p[P_kbdBrillianceLow] = p[P_kbdBrillianceHigh] = p[P_kbdLevelLow] = p[P_kbdLevelHigh] = 0;
        p[P_mix] = p[P_detune] = p[P_portamento] = p[P_initBend] = p[P_subVco] =
        p[P_subAtVco] = p[P_subVcf] = p[P_subAtVcf] = p[P_subVca] = 0;
        p[P_volume] = .3f;
        p[P_oversampling] = quality == 1 ? 2.f : 1.f;
        p[P_body1Model] = float(model);
        p[P_body1Mix] = 1;
        p[P_body1Brightness] = .5f;
        p[chParam(0, CP_hpfOn)] = p[chParam(0, CP_lpfOn)] = 0;
        p[chParam(0, CP_feet)] = 1;
        p[chParam(0, CP_vA)] = .01f;
        p[chParam(0, CP_vS)] = 1;
        p[chParam(0, CP_initLevel)] = p[chParam(0, CP_afterLevel)] = 0;
        p[chParam(0, CP_initBrill)] = p[chParam(0, CP_afterBrill)] = 0;
        auto engine = std::make_unique<Engine>();
        engine->setParams(p);
        engine->setNonRealtime(quality == 2);
        engine->prepare(rate);
        engine->noteOn(60, .8f);
        std::vector<float> samples(std::size_t(rate) * 4 * 2);
        for (int i = 0; i < rate * 4; i += 113)
            engine->render(samples.data() + i * 2, std::min(113, rate * 4 - i));
        const char* mode = quality == 0 ? "rt" : quality == 1 ? "hq" : "offline";
        const auto path = std::filesystem::path(argv[1]) /
            (std::to_string(model) + "_" + std::to_string(rate) + "_" + mode + ".f32");
        auto* file = std::fopen(path.c_str(), "wb");
        if (!file) return 3;
        const auto written = std::fwrite(samples.data(), sizeof(float), samples.size(), file);
        std::fclose(file);
        if (written != samples.size()) return 4;
        std::printf("model=%d rate=%d mode=%s factor=%d samples=%zu\n", model, rate, mode,
                    engine->getOversamplingFactor(), samples.size());
        std::fflush(stdout);
    }
}
