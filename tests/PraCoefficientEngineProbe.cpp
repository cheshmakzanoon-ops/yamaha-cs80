// JUCE-free PRA coefficient-reuse witness across changing voice mixes.
// Build this same file against each complete source tree, then compare files.
// Audio and public circuit state are serialized fieldwise, never struct padding.
#include "core/Engine.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <type_traits>

namespace {
struct Writer {
    FILE* file;
    bool valid = true;
    std::uint64_t values = 0, hash = 1469598103934665603ull;
    template<class T> void put(T value) {
        if constexpr (std::is_floating_point_v<T>) valid = std::isfinite(value) && valid;
        const auto bytes = std::bit_cast<std::array<unsigned char, sizeof(T)>>(value);
        for (auto byte : bytes) hash = (hash ^ byte) * 1099511628211ull;
        valid = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size() && valid;
        ++values;
    }
};
void snapshot(Writer& w, const cs80::Engine& e) {
    w.put(e.activeVoices());
    w.put(e.getOversamplingFactor());
    w.put(e.getLatencySamples());
    for (int channel = 0; channel < 2; ++channel) {
        const auto state = e.getPraWahState(channel);
        for (const auto& capacitor : state.capacitors) {
            w.put(capacitor.voltage);
            w.put(capacitor.current);
        }
        w.put(state.sampleRate); w.put(state.opticalOhms);
        w.put(state.targetOhms); w.put(state.wetMix);
    }
    for (int voice = 0; voice < 8; ++voice) {
        const auto info = e.getVoiceInfo(voice);
        w.put(info.active); w.put(info.note); w.put(info.held); w.put(info.pedalHeld);
        for (int rank = 0; rank < 2; ++rank) {
            const auto state = e.getRankAudioState(voice, rank);
            w.put(state.phase);
            for (const auto& stage : {state.hpCircuit, state.lpCircuit}) {
                w.put(stage.input); w.put(stage.feedback); w.put(stage.bp); w.put(stage.lp);
            }
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    using namespace cs80;
    if (argc != 2) return 2;
    FILE* file = std::fopen(argv[1], "wb");
    if (!file) return 3;
    Writer w {file};
    std::uint64_t frames = 0;
    int cases = 0;
    constexpr int notes[] {36, 43, 48, 55, 60, 64, 67, 72};
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int policy = 0; policy < 3; ++policy)
            for (bool factory : {false, true}) {
                Engine e;
                ControlFrame frame;
                auto& p = frame.effectiveValues;
                p[P_sourcePerformance] = policy == 0 ? 0.0f : 1.0f;
                p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
                p[P_subVca] = 0.63f;
                p[P_subOriginalMode] = 1.0f;
                p[P_subOriginalSpeed] = 6.7f;
                p[P_subOriginalFunction] = 2.0f;
                p[P_footMode] = 1.0f;
                p[P_volume] = 0.19f;
                frame.hardwareSources = factory ? std::array<int, 2>{0, 10}
                                                : std::array<int, 2>{13, 13};
                e.prepare(rate);
                w.valid = e.setControlFrame(frame) && w.valid;
                std::array<float, 257> input {};
                std::array<float, 514> output {};
                int offset = 0;
                double energy = 0.0;
                for (int block = 0; block < 480; ++block) {
                    const int count = std::array<int, 6>{1, 17, 64, 113, 256, 257}[block % 6];
                    if (block == 16) e.noteOn(notes[0], 0.8f);
                    if (block == 32) for (int i = 1; i < 4; ++i) e.noteOn(notes[i], 0.75f);
                    if (block == 64) for (int i = 4; i < 8; ++i) e.noteOn(notes[i], 0.65f);
                    if (block >= 96 && block < 192) {
                        e.setExpression(static_cast<float>((block * 37) % 97) / 96.0f);
                        p[P_subVca] = static_cast<float>((block * 23) % 103) / 102.0f;
                        w.valid = e.setControlFrame(frame) && w.valid;
                    }
                    if (block == 192) {
                        p[P_subOriginalFunction] = 5.0f; p[P_subExtLevel] = 0.8f;
                        p[P_subVca] = 0.9f; w.valid = e.setControlFrame(frame) && w.valid;
                    }
                    if (block == 240) for (int note : notes) e.noteOff(note);
                    if (block == 272) e.allNotesOff(true);
                    if (block == 288) { e.noteOn(62, 0.7f); e.noteOn(69, 0.6f); }
                    if (block == 320) { p[P_oversampling] = 2.0f; w.valid = e.setControlFrame(frame) && w.valid; e.noteOn(60, 0.8f); }
                    if (block == 352) { p[P_oversampling] = 0.0f; w.valid = e.setControlFrame(frame) && w.valid; e.noteOn(60, 0.8f); }
                    if (block == 384) { e.reset(); e.noteOn(65, 0.8f); }
                    if (block == 416) { e.prepare(rate == 48000.0 ? 44100.0 : 48000.0); e.noteOn(57, 0.8f); }
                    if (block == 448) {
                        p[P_parityMode] = policy == 2 ? 0.0f : 1.0f;
                        w.valid = e.setControlFrame(frame) && w.valid; e.noteOn(67, 0.8f);
                    }
                    for (int i = 0; i < count; ++i) input[i] = static_cast<float>(0.7 * std::sin(0.017 * (offset + i)));
                    e.render(output.data(), count, block >= 192 && block < 224 ? input.data() : nullptr);
                    for (int i = 0; i < 2 * count; ++i) {
                        w.put(output[i]); energy += static_cast<double>(output[i]) * output[i];
                    }
                    snapshot(w, e);
                    offset += count; frames += count;
                }
                w.valid = energy > 1.0e-10 && w.valid;
                ++cases;
            }
    w.valid = std::fclose(file) == 0 && w.valid;
    std::printf("cases=%d frames=%llu values=%llu hash=%016llx valid=%d\n", cases,
                static_cast<unsigned long long>(frames), static_cast<unsigned long long>(w.values),
                static_cast<unsigned long long>(w.hash), w.valid);
    return w.valid ? 0 : 1;
}
