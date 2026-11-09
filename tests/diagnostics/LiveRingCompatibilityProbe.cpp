// Supplemental raw-float compatibility corpus for docs/LIVE_RING_TIMING.md.
// Compile this same source/toolchain against the base and candidate core,
// invoke with separate output files, then compare those files byte-for-byte.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <cstdio>
#include <vector>

using namespace cs80;
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    FILE* output = std::fopen(argv[1], "wb");
    if (output == nullptr) return 2;
    std::vector<float> audio(2048 * 2);
    int rows = 0;
    bool written = true;
    const auto capture = [&](Engine& engine)
    {
        engine.render(audio.data(), 2048);
        if (std::fwrite(audio.data(), sizeof(float), audio.size(), output) != audio.size())
            written = false;
    };
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        // All shipped programs retain their own policy. No ring timing edit:
        // static source renders and ordinary recalls must remain unchanged.
        for (const auto& preset : programLibrary())
        {
            Engine engine;
            const auto p = preset.values;
            engine.setParams(p); engine.prepare(rate);
            engine.noteOn(48, 0.8f); engine.noteOn(60, 0.7f); engine.noteOn(67, 0.6f);
            capture(engine);
            engine.setParams(p); engine.noteOff(60); capture(engine);
            ++rows;
        }
        for (int policy : {0, 1, 2})
            for (int quality : {0, 2})
            {
                Engine engine;
                ParamValues p;
                p[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
                p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
                p[P_oversampling] = static_cast<float>(quality);
                p[P_ringAmount] = 0.8f; p[P_ringDepth] = 1.0f;
                p[P_ringAttack] = 0.1f; p[P_ringDecay] = 0.2f;
                engine.setParams(p); engine.prepare(rate); engine.noteOn(60, 0.8f);
                for (int event = 0; event < 6; ++event)
                {
                    // Legacy dynamic edits remain latched; future phrases
                    // still take the latest accepted controls in both builds.
                    if (policy == 0)
                    {
                        p[P_ringAttack] = event % 2 != 0 ? 0.001f : 5.0f;
                        p[P_ringDecay] = event % 2 != 0 ? 10.0f : 0.02f;
                    }
                    engine.setParams(p);
                    if (event == 3) engine.noteOff(60);
                    if (event == 4) engine.noteOn(64, 0.7f);
                    capture(engine);
                }
                ++rows;
            }
    }
    if (std::fclose(output) != 0) written = false;
    std::printf("%d corpus rows\n", rows);
    return written ? 0 : 1;
}
