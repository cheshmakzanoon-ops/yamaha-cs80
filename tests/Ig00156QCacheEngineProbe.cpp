// Same-toolchain whole-core witness for the IG00156 Q-cache change.
// Compile this same file against each revision's own headers and core library.
// Writes raw audio and public filter histories; compare the two files exactly.
// No JUCE, plugin host, sockets, or GUI initialization.
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    FILE* file = std::fopen(argv[1], "wb");
    if (!file) return 3;
    std::uint64_t frames = 0;
    int cases = 0, modulatedCases = 0;
    bool valid = true;
    using namespace cs80;
    using namespace cs80::modern;
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int quality : { 0, 1, 2 })
            for (int policy : { 0, 1, 2 })
            {
                Engine engine;
                engine.prepare(rate);
                ParamValues p;
                p[P_sourcePerformance] = policy == 0 ? 0.0f : 1.0f;
                p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
                p[P_oversampling] = (float) quality;
                p[P_lfo1Rate] = 7.3f;
                p[P_lfo2Rate] = 11.1f;
                p[P_lfo1Wave] = 1.0f;
                p[P_lfo2Wave] = 2.0f;
                p[P_mod1Amount] = 0.9f;
                p[P_mod2Amount] = -0.8f;
                for (int rank = 0; rank < 2; ++rank)
                {
                    p[chParam(rank, CP_hpfOn)] = 1.0f;
                    p[chParam(rank, CP_lpfOn)] = 1.0f;
                    p[chParam(rank, CP_hpfCutoff)] = 0.2f;
                    p[chParam(rank, CP_lpfCutoff)] = 0.65f;
                    p[chParam(rank, CP_hpfRes)] = 0.55f;
                    p[chParam(rank, CP_lpfRes)] = 0.7f;
                    p[chParam(rank, CP_fIl)] = 0.0f;
                    p[chParam(rank, CP_fAl)] = 0.0f;
                }
                ModernConfig config;
                config.slots[0] = { ModSource::lfo1, ModSource::none, ModDestination::hpResonance, ModTarget::both };
                config.slots[1] = { ModSource::lfo2, ModSource::none, ModDestination::lpResonance, ModTarget::both };
                engine.setModernConfig(config);
                engine.setParams(p);
                engine.noteOn(48, 0.8f);
                engine.noteOn(60, 0.7f);
                engine.noteOn(67, 0.6f);
                std::array<float, 514> audio{};
                double energy = 0.0;
                float previousHp = -1.0f, previousLp = -1.0f;
                bool qChanged = false;
                for (int block = 0; block < 80; ++block)
                {
                    if (block == 12 || block == 24)
                    {
                        p[P_resonance] = block == 12 ? 0.7f : 0.15f;
                        p[P_brilliance] = block == 12 ? 0.5f : -0.3f;
                        engine.setParams(p);
                    }
                    if (block == 30) engine.allNotesOff(false);
                    if (block == 42) engine.noteOn(72, 0.75f);
                    if (block == 50)
                    {
                        p[P_oversampling] = (float) ((quality + 1) % 3);
                        engine.setParams(p);
                        engine.noteOn(55, 0.65f);
                    }
                    if (block == 60)
                    {
                        engine.reset();
                        engine.noteOn(64, 0.7f);
                    }
                    if (block == 70) engine.allNotesOff(true);
                    const int count = std::array<int, 6>{ 1, 17, 64, 113, 256, 257 }[(size_t) block % 6];
                    engine.render(audio.data(), count);
                    for (int i = 0; i < 2 * count; ++i)
                    {
                        valid = std::isfinite(audio[(size_t) i]) && valid;
                        energy += (double) audio[(size_t) i] * audio[(size_t) i];
                    }
                    valid = std::fwrite(audio.data(), sizeof(float), (size_t) (2 * count), file) == (size_t) (2 * count) && valid;
                    frames += (std::uint64_t) count;
                    const auto info = engine.getModernRankOffsets(0, 0);
                    if (block > 0 && block < 12)
                        qChanged = qChanged || info.hpQ != previousHp || info.lpQ != previousLp;
                    previousHp = info.hpQ;
                    previousLp = info.lpQ;
                    for (int voice = 0; voice < 8; ++voice)
                        for (int rank = 0; rank < 2; ++rank)
                        {
                            const auto state = engine.getRankAudioState(voice, rank);
                            const std::array<double, 8> values { state.hpCircuit.input, state.hpCircuit.feedback,
                                state.hpCircuit.bp, state.hpCircuit.lp, state.lpCircuit.input, state.lpCircuit.feedback,
                                state.lpCircuit.bp, state.lpCircuit.lp };
                            for (double value : values) valid = std::isfinite(value) && valid;
                            valid = std::fwrite(values.data(), sizeof(double), values.size(), file) == values.size() && valid;
                        }
                }
                valid = energy > 1.0e-10 && valid;
                if (policy == 1)
                {
                    valid = qChanged && valid;
                    if (qChanged) ++modulatedCases;
                }
                ++cases;
            }
    valid = std::fclose(file) == 0 && valid;
    std::printf("%d cases, %d circuit resonance-modulation witnesses, %llu stereo frames: %s\n",
                cases, modulatedCases, (unsigned long long) frames, valid ? "PASS" : "FAIL");
    return valid ? 0 : 1;
}
