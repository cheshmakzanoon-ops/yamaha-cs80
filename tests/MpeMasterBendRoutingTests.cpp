// MPE master pitch belongs to its zone. This tests post-onset routing only;
// no controller retention or initial-note expression is implemented here.
#include "core/Engine.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace cs80;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", label.c_str()); }
}
ParamValues params(int policy, bool mpe)
{
    ParamValues p;
    p[P_sourcePerformance] = policy != 0 ? 1.0f : 0.0f;
    p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
    p[P_mpeOn] = mpe ? 1.0f : 0.0f;
    p[P_drift] = p[P_voiceDispersion] = p[P_analogNoise] = p[P_detune] = 0.0f;
    p[P_pitchBendRange] = 2.0f; p[P_mpeBendRange] = 48.0f;
    p[P_keyRelease] = 5.0f;
    return p;
}
void zone(Engine& e, int master, int members)
{
    e.controlChange(101, 0.0f, master);
    e.controlChange(100, 6.0f / 127.0f, master);
    e.controlChange(6, members / 127.0f, master);
}
void layout(Engine& e, int lower, int upper)
{
    zone(e, 1, 0); zone(e, 16, 0); zone(e, 1, lower); zone(e, 16, upper);
}
std::vector<float> render(Engine& e, int frames = 3072)
{
    std::vector<float> out(static_cast<size_t>(frames) * 2);
    for (int i = 0; i < frames; i += 128)
        e.render(out.data() + 2 * i, std::min(128, frames - i));
    return out;
}
std::array<float, 2> frequencies(const Engine& e)
{
    return {e.getRankInfo(0, 0).oscillatorHz, e.getRankInfo(0, 1).oscillatorHz};
}
void expectPitch(const Engine& e, const std::array<float, 2>& reference,
                 float semitones, const std::string& label)
{
    for (int rank = 0; rank < 2; ++rank)
    {
        const float actual = 12.0f * std::log2(e.getRankInfo(0, rank).oscillatorHz / reference[rank]);
        check(std::isfinite(actual) && std::abs(actual - semitones) < 0.001f, label);
    }
}
void testLayouts()
{
    const std::array<std::array<int, 2>, 7> layouts {{{15, 0}, {0, 15}, {7, 7}, {2, 2}, {0, 2}, {2, 0}, {0, 0}}};
    for (int policy : {0, 1})
        for (const auto counts : layouts)
        {
            Engine e; e.prepare(48000.0); e.setParams(params(policy, true));
            layout(e, counts[0], counts[1]);
            for (int channel = 1; channel <= 16; ++channel)
            {
                e.reset(); e.noteOn(60, 0.8f, channel); render(e, 64);
                const auto reference = frequencies(e);
                // Every bend in this regression arrives AFTER note allocation.
                // In a full upper zone channel 1 is a member, not a master.
                if (counts[1] != 15) e.pitchBend(1, 0.5f);
                if (counts[1] > 0) e.pitchBend(16, -0.5f);
                const auto out = render(e);
                bool finite = true;
                for (float sample : out) finite &= std::isfinite(sample);
                const bool lower = counts[0] > 0 && channel <= counts[0] + 1;
                const bool upper = counts[1] > 0 && channel >= 16 - counts[1];
                // Disabled channel 1 retains its existing own-channel global-
                // bend fallback; it must not broadcast into other gap channels.
                const float expected = upper ? -1.0f : lower || channel == 1 ? 1.0f : 0.0f;
                const auto label = "policy=" + std::to_string(policy) + " zones="
                    + std::to_string(counts[0]) + "/" + std::to_string(counts[1])
                    + " channel=" + std::to_string(channel);
                check(finite, label + " finite audio");
                expectPitch(e, reference, expected, label + " master bend is zone-scoped");
                if ((channel != 1 || counts[1] == 15) && !(counts[1] > 0 && channel == 16))
                {
                    e.pitchBend(channel, 0.25f); render(e);
                    expectPitch(e, reference, expected + 12.0f,
                                label + " own-channel member bend still applies");
                }
            }
        }
}
void testLifecycle()
{
    for (int policy : {0, 1})
        for (bool tail : {false, true})
        {
            Engine e; auto p = params(policy, true);
            e.prepare(48000.0); e.setParams(p); layout(e, 2, 2);
            e.noteOn(60, 0.8f, 8); render(e, 64);
            if (tail) e.noteOff(60, 8);
            const auto reference = frequencies(e);
            e.pitchBend(1, 0.5f); render(e);
            expectPitch(e, reference, 0.0f, "gap held note/tail ignores master");
            p[P_mpeOn] = 0; e.setParams(p); render(e);
            expectPitch(e, reference, 1.0f, "MPE off preserves historical global bend");
            p[P_mpeOn] = 1; e.setParams(p); render(e);
            expectPitch(e, reference, 0.0f, "MPE on excludes gap again");

            e.reset(); layout(e, 2, 2); e.noteOn(60, 0.8f, 3); render(e, 64);
            if (tail) e.noteOff(60, 3);
            const auto lowerReference = frequencies(e);
            e.pitchBend(1, 0.5f); render(e);
            expectPitch(e, lowerReference, 1.0f, "assigned lower note initially inherits master");
            layout(e, 1, 2); render(e);
            expectPitch(e, lowerReference, 0.0f, "removed member does not retain master inheritance");

            // Assigned voices retain the existing captured-upper convention.
            // This fix deliberately does not introduce live zone retagging.
            e.reset(); layout(e, 2, 2); e.noteOn(60, 0.8f, 14); render(e, 64);
            const auto upperReference = frequencies(e);
            e.pitchBend(1, 0.5f); e.pitchBend(16, -0.5f); render(e);
            layout(e, 14, 0); render(e);
            expectPitch(e, upperReference, -1.0f, "assigned voice keeps captured-upper bend policy");
        }
}
void testModeCompatibility()
{
    for (int policy : {0, 1, 2})
        for (int channel : {1, 2, 8, 16})
        {
            Engine e; e.prepare(48000.0); e.setParams(params(policy, policy == 2));
            e.noteOn(60, 0.8f, channel); render(e, 64);
            const auto reference = frequencies(e);
            e.pitchBend(1, 0.5f); render(e);
            expectPitch(e, reference, policy == 2 ? 0.0f : 1.0f,
                        "non-MPE global bend and PURE isolation unchanged");
        }
}
}
int main()
{
    testLayouts(); testLifecycle(); testModeCompatibility();
    std::printf("MPE master bend routing: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
