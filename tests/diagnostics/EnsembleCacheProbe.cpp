// Ensemble plus existing Drive/Tape caches. Compile this same probe against before
// and after source trees; compare output bytes, not a numerical tolerance.
// This is a software compatibility check, not a CPU or hardware measurement.
#include "core/Engine.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace cs80;
namespace
{
bool write(std::FILE* file, const float* audio, std::size_t samples)
{
    for (std::size_t i = 0; i < samples; ++i)
        if (!std::isfinite(audio[i])) return false;
    return std::fwrite(audio, sizeof(float), samples, file) == samples;
}
}

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    auto* file = std::fopen(argv[1], "wb");
    if (file == nullptr) return 2;
    int slotCases = 0, engineCases = 0;
    std::size_t slotFrames = 0, engineFrames = 0;
    constexpr std::array<std::array<int, 3>, 3> chains {{{1, 5, 4}, {4, 1, 5}, {1, 4, 1}}};
    // Serial processing includes the actual slot lifecycle, loudness matching,
    // mix, clear/type changes, and duplicate independently owned model caches.
    for (float rate : { 88200.0f, 96000.0f, 192000.0f })
        for (int chain = 0; chain < 3; ++chain)
        {
            std::array<fx::FxSlot, 3> slots;
            std::array<fx::SlotControls, 3> controls;
            for (int i = 0; i < 3; ++i)
            {
                slots[static_cast<std::size_t>(i)].prepare(rate);
                auto& c = controls[static_cast<std::size_t>(i)];
                c.model = chains[static_cast<std::size_t>(chain)][static_cast<std::size_t>(i)];
                c.mix = 0.63f; c.a = 0.05f + 0.07f * i; c.b = 0.71f - 0.11f * i;
                c.oversampling = i; c.gainComp = i != 1;
            }
            double energy = 0.0;
            std::vector<float> block;
            // Hold beyond the maximum delay and a buffer wrap, then exercise
            // changes that could leave stale cache entries in active slots.
            for (int stage = 0; stage < 9; ++stage)
            {
                if (stage == 1) { controls[0].a = 1.0f; controls[1].b = 0.04f; }
                if (stage == 2) controls[0].mix = 0.0f;
                if (stage == 3) { controls[0].mix = 0.63f; controls[0].oversampling = 3; }
                if (stage == 4)
                    for (auto& c : controls) { c.model = c.model == 1 ? 4 : c.model == 4 ? 5 : 1; c.gainComp = !c.gainComp; }
                if (stage == 5) for (auto& slot : slots) slot.resetAudio();
                if (stage == 6) for (auto& slot : slots) slot.prepare(rate * 0.5f);
                const float effectiveRate = stage >= 6 ? rate * 0.5f : rate;
                const int frames = stage < 2 ? static_cast<int>(rate * 1.4f) : 8192;
                block.resize(static_cast<std::size_t>(frames) * 2);
                for (int n = 0; n < frames; ++n)
                {
                    if (stage == 7)
                        for (int i = 0; i < 3; ++i)
                        {
                            auto& c = controls[static_cast<std::size_t>(i)];
                            c.a = static_cast<float>((n + i * 13) % 257) / 256.0f;
                            c.b = static_cast<float>((n + i * 17) % 263) / 262.0f;
                        }
                    fx::StereoSample sample { stage == 8 ? 0.0f : static_cast<float>((n * 17) % 127 - 63) / 128.0f,
                                              stage == 8 ? 0.0f : static_cast<float>((n * 29) % 131 - 65) / 256.0f };
                    for (std::size_t i = 0; i < slots.size(); ++i)
                        sample = slots[i].process(controls[i], sample.l, sample.r, 0.0f, effectiveRate);
                    block[static_cast<std::size_t>(2 * n)] = sample.l;
                    block[static_cast<std::size_t>(2 * n + 1)] = sample.r;
                    energy += static_cast<double>(sample.l) * sample.l + static_cast<double>(sample.r) * sample.r;
                }
                if (!write(file, block.data(), block.size())) return 3;
                slotFrames += static_cast<std::size_t>(frames);
            }
            if (!(energy > 1.0e-6)) return 4;
            ++slotCases;
        }

    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (int policy : { 0, 1, 2 })
            for (int chain = 0; chain < 3; ++chain)
            {
                Engine engine;
                ControlFrame frame;
                frame.hardwareSources = { 3, 8 };
                auto& p = frame.effectiveValues;
                p[P_parityMode] = policy == 1 ? 1.0f : 0.0f;
                p[P_sourcePerformance] = policy == 2 ? 1.0f : 0.0f;
                p[P_oversampling] = static_cast<float>(chain);
                p[P_fx1Model] = static_cast<float>(chains[static_cast<std::size_t>(chain)][0]);
                p[P_fx2Model] = static_cast<float>(chains[static_cast<std::size_t>(chain)][1]);
                p[P_fx3Model] = static_cast<float>(chains[static_cast<std::size_t>(chain)][2]);
                p[P_fx1Mix] = 0.5f; p[P_fx2Mix] = 0.6f; p[P_fx3Mix] = 0.3f;
                p[P_fx1ParamA] = p[P_fx2ParamA] = 0.0f;
                p[P_fx1ParamB] = 0.7f; p[P_fx2ParamB] = 0.6f;
                p[P_fxGainCompensation] = 1.0f;
                engine.prepare(rate);
                if (!engine.setControlFrame(frame)) return 5;
                engine.noteOn(48, 0.8f); engine.noteOn(67, 0.6f);
                double energy = 0.0;
                for (int stage = 0; stage < 9; ++stage)
                {
                    if (stage == 1) { p[P_fx1ParamA] = 0.4f; p[P_fx2ParamB] = 0.1f; }
                    if (stage == 2) { p[P_fx1Oversampling] = 3.0f; p[P_fx2Mix] = 0.0f; }
                    if (stage == 3) { p[P_fx2Mix] = 0.6f; p[P_fxGainCompensation] = 0.0f; }
                    if (stage == 4) { std::swap(p[P_fx1Model], p[P_fx2Model]); engine.noteOff(48); }
                    if (stage == 5) { engine.setNonRealtime(true); engine.noteOn(72, 0.7f); }
                    if (stage == 6) { engine.setNonRealtime(false); engine.reset(); engine.noteOn(60, 0.8f); }
                    if (stage == 7) { engine.prepare(rate == 96000.0 ? 48000.0 : 96000.0); engine.noteOn(65, 0.6f); }
                    if (stage == 8) engine.allNotesOff(true);
                    if (!engine.setControlFrame(frame)) return 5;
                    const int frames = stage == 0 ? 8192 : 1024;
                    std::vector<float> block(static_cast<std::size_t>(frames) * 2);
                    engine.render(block.data(), frames);
                    for (float sample : block) energy += static_cast<double>(sample) * sample;
                    if (!write(file, block.data(), block.size())) return 3;
                    engineFrames += static_cast<std::size_t>(frames);
                }
                if (!(energy > 1.0e-6)) return 4;
                ++engineCases;
            }
    if (std::fclose(file) != 0) return 3;
    std::printf("Ensemble/Drive/Tape raw diagnostic: %d serial-slot cases (%zu stereo frames), "
                "%d Engine cases (%zu stereo frames), all non-silent\n",
                slotCases, slotFrames, engineCases, engineFrames);
    return 0;
}
