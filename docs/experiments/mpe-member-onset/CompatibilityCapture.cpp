// Own-header/archive baseline witness for unchanged event paths. Not an exhaustive state capture.
#include "core/Engine.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace cs80;
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    FILE* file = std::fopen(argv[1], "wb"); if (!file) return 2;
    int values = 0, bad = 0;
    auto emit = [&](float v) { ++values; if (!std::isfinite(v) || std::fwrite(&v, sizeof v, 1, file) != 1) ++bad; };
    for (int policy : {0, 1, 2}) for (bool mpe : {false, true})
    {
        Engine e; e.prepare(48000.0); ParamValues p;
        p[P_sourcePerformance] = policy != 0; p[P_parityMode] = policy == 2; p[P_mpeOn] = mpe;
        p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0;
        p[P_keyRelease] = .5f; p[P_mod1Amount] = 1; e.setParams(p);
        modern::ModernConfig config;
        config.slots[0] = {modern::ModSource::timbre, modern::ModSource::none, modern::ModDestination::pulseWidth, modern::ModTarget::both};
        e.setModernConfig(config);
        auto expression = [&](int channel, float sign) {
            e.pitchBend(channel, sign * .25f); e.channelPressure(channel, .7f);
            e.controlChange(74, .8f, channel);
        };
        auto render = [&](int frames) {
            std::array<float, 2> audio;
            for (int i = 0; i < frames; ++i) { e.render(audio.data(), 1); emit(audio[0]); emit(audio[1]); }
            for (int voice = 0; voice < 8; ++voice)
            {
                const auto v = e.getVoiceInfo(voice);
                for (float x : {float(v.active), float(v.note), float(v.channel), v.velocity, v.pressure, v.bend,
                                v.ribbonRatio, float(v.held), float(v.pedalHeld), v.touchA, v.touchB, v.touchC}) emit(x);
                for (int rank = 0; rank < 2; ++rank)
                {
                    const auto r = e.getRankInfo(voice, rank);
                    for (float x : {r.hpfHz, r.lpfHz, r.filterEnvelope, r.ampEnvelope, r.oscillatorHz, r.keyVolts,
                                    r.hpfQ, r.lpfQ, float(r.hpfEnabled), float(r.lpfEnabled),
                                    e.getModernRankOffsets(voice, rank).pulseWidth}) emit(x);
                }
            }
        };
        // No pre-note member events; active and released-note routing unchanged.
        e.noteOn(60, .8f, 2); e.noteOn(64, .7f, 3); e.noteOn(67, .9f, 15); render(64);
        expression(2, 1); expression(3, -1); expression(15, 1); render(256);
        e.noteOn(69, .8f, 4); render(128); // A channel with no expression history.
        e.noteOff(60, 2); e.noteOff(64, 3); expression(2, -1); expression(3, 1); render(256);
        p[P_mpeOn] = 0; e.setParams(p); expression(4, 1); e.noteOn(70, .8f, 4); render(128);
        p[P_mpeOn] = 1; e.setParams(p); e.noteOn(72, .8f, 2); render(256); // New cache cleared by mode edge.
        e.reset(); e.noteOn(60, .8f, 2); render(256);
    }
    if (std::fflush(file) != 0) ++bad;
    if (std::fclose(file) != 0) ++bad;
    std::printf("Unchanged MPE paths: 6 scenarios, %d finite serialized values, %d failures\n", values, bad);
    return bad ? 1 : 0;
}
