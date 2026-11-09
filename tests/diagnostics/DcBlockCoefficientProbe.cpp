// Raw pre/post-hoist compatibility diagnostic, not a timing benchmark.
// Compile this same probe against both pinned core source trees and compare
// the binary output files. See docs/DC_BLOCK_COEFFICIENT_HOIST.md.
#include "core/Engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace cs80;

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    auto* file = std::fopen(argv[1], "wb");
    if (file == nullptr) return 2;
    int cases = 0;
    std::size_t totalFrames = 0;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (int policy : { 0, 1, 2 })
            for (float quality : { 0.0f, 1.0f, 2.0f })
            {
                ControlFrame frame;
                frame.hardwareSources = { 3, 8 };
                auto& p = frame.effectiveValues;
                const auto setPolicy = [&](int selected)
                {
                    p[P_parityMode] = selected == 1 ? 1.0f : 0.0f;
                    p[P_sourcePerformance] = selected == 2 ? 1.0f : 0.0f;
                };
                setPolicy(policy);
                p[P_oversampling] = quality;
                p[P_mpeOn] = 1.0f;
                p[P_subVco] = 0.23f; p[P_subVcf] = 0.27f; p[P_subVca] = 0.19f;
                p[P_ringAmount] = 0.35f; p[P_ringDepth] = 0.15f;
                p[P_fx1Type] = 2.0f; p[P_fx1Mix] = 0.4f;
                Engine e;
                e.prepare(rate);
                if (!e.setControlFrame(frame)) return 3;
                e.setExpression(0.56f);
                e.noteOn(48, 0.73f, 2); e.noteOn(67, 0.42f, 3); e.noteOn(84, 0.91f, 4);
                float peak = 0.0f;
                for (int stage = 0; stage < 11; ++stage)
                {
                    if (stage == 1)
                    {
                        e.controlChange(1, 0.37f); e.polyPressure(48, 0.42f, 2);
                        e.pitchBend(3, 0.3f); e.noteOff(48, 2);
                    }
                    if (stage == 2)
                    {
                        e.controlChange(64, 1.0f, 3); e.noteOff(67, 3); e.setRibbon(-0.12f);
                    }
                    if (stage == 3)
                    {
                        p[P_oversampling] = quality > 1.5f ? 0.0f : 2.0f;
                        if (!e.setControlFrame(frame)) return 3;
                        e.noteOn(60, 0.7f, 2);
                    }
                    if (stage == 4) { e.setNonRealtime(true); e.noteOn(62, 0.8f, 2); }
                    if (stage == 5) { e.setNonRealtime(false); e.noteOn(65, 0.6f, 2); }
                    if (stage == 6)
                    {
                        setPolicy((policy + 1) % 3);
                        if (!e.setControlFrame(frame)) return 3;
                        e.noteOn(71, 0.8f, 2);
                    }
                    if (stage == 7) { e.reset(); e.noteOn(74, 0.7f, 2); }
                    if (stage == 8)
                    {
                        e.prepare(rate == 44100.0 ? 96000.0 : rate == 48000.0 ? 44100.0 : 48000.0);
                        e.noteOn(72, 0.65f, 2);
                    }
                    if (stage == 9) e.allNotesOff(true);
                    if (stage == 10) { e.prepare(0.0); e.noteOn(42, 0.9f, 2); }
                    const int frames = 257 + stage * 17;
                    std::vector<float> audio(static_cast<std::size_t>(frames) * 2);
                    e.render(audio.data(), frames);
                    for (float sample : audio)
                    {
                        if (!std::isfinite(sample)) return 4;
                        peak = std::max(peak, std::abs(sample));
                    }
                    if (std::fwrite(audio.data(), sizeof(float), audio.size(), file) != audio.size()) return 5;
                    totalFrames += static_cast<std::size_t>(frames);
                }
                if (!(peak > 1.0e-7f)) return 6;
                ++cases;
            }
    if (std::fclose(file) != 0) return 5;
    std::printf("DC-hoist raw diagnostic: %d non-silent cases, %zu stereo frames\n", cases, totalFrames);
    return 0;
}
