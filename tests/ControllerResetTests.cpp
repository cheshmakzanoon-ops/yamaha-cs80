// CC121 sustain and parameter-selection reset only. This is not a complete
// Reset All Controllers implementation or a hardware-fidelity claim.
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", label.c_str()); }
}
ParamValues patch(bool circuit, int mode = 0, bool gate = false)
{
    ParamValues p;
    p[P_sourcePerformance] = circuit ? 1.0f : 0.0f;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    p[P_sustainMode] = static_cast<float>(mode);
    p[P_sustainAssign] = p[P_footswitchPlugged] = 1.0f;
    p[P_footswitchPressed] = gate ? 1.0f : 0.0f;
    p[P_masterSustain] = p[P_keyRelease] = 0.0f;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_vA)] = p[chParam(rank, CP_fA)] = 0.001f;
        p[chParam(rank, CP_vS)] = 1.0f;
        p[chParam(rank, CP_vR)] = p[chParam(rank, CP_fR)] = 2.0f;
    }
    return p;
}
std::vector<float> render(Engine& e, int frames = 257, int block = 128)
{
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    for (int i = 0; i < frames; i += block)
        e.render(out.data() + i * 2, std::min(block, frames - i));
    check(std::all_of(out.begin(), out.end(), [](float v) { return std::isfinite(v); }),
          "finite controller-reset audio");
    return out;
}
bool same(Engine::RankEnvelopeState a, Engine::RankEnvelopeState b)
{
    return a.amplitude.stage == b.amplitude.stage && a.amplitude.value == b.amplitude.value
        && a.amplitude.elapsed == b.amplitude.elapsed && a.filter.stage == b.filter.stage
        && a.filter.value == b.filter.value && a.filter.elapsed == b.filter.elapsed;
}
void rpn(Engine& e, int channel, int parameter, int value)
{
    e.controlChange(101, 0.0f, channel);
    e.controlChange(100, parameter / 127.0f, channel);
    e.controlChange(6, value / 127.0f, channel);
}
void testPedalRelease()
{
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (bool circuit : {false, true})
            for (int mode : {0, 1})
                for (bool gate : {false, true})
                    for (bool notesOff : {false, true})
                    {
                        Engine reset, pedal;
                        for (auto* e : {&reset, &pedal})
                        {
                            e->prepare(rate); e->setParams(patch(circuit, mode, gate));
                            e->noteOn(60, 0.8f, 2); e->noteOn(64, 0.7f, 3);
                            render(*e);
                            e->controlChange(64, 1.0f, 2); e->controlChange(64, 1.0f, 3);
                            if (notesOff) e->controlChange(123, 0.0f, 2);
                            else e->noteOff(60, 2);
                        }
                        const auto row = std::to_string(static_cast<int>(rate))
                            + (circuit ? " circuit" : " legacy") + " mode=" + std::to_string(mode)
                            + " gate=" + std::to_string(gate) + " CC123=" + std::to_string(notesOff);
                        check(reset.getVoiceInfo(0).pedalHeld, row + " pedal-held fixture");
                        const auto before = reset.getRankEnvelopeState(0, 0);
                        const auto held = reset.getRankEnvelopeState(1, 0);
                        // Channel-mode reset is a command; both payload values release.
                        reset.controlChange(121, gate ? 1.0f : 0.0f, 2);
                        pedal.controlChange(64, 0.0f, 2);
                        check(!reset.getVoiceInfo(0).pedalHeld && !reset.getVoiceInfo(0).held,
                              row + " reset releases only pedal hold");
                        for (int rank = 0; rank < 2; ++rank)
                            check(same(reset.getRankEnvelopeState(0, rank), pedal.getRankEnvelopeState(0, rank)),
                                  row + " reset uses exact pedal-up envelope policy");
                        const auto after = reset.getRankEnvelopeState(0, 0);
                        check(after.amplitude.stage == Adsr::Stage::Release
                                  && after.filter.stage == FilterEnv::Stage::Release,
                              row + " both envelopes enter release");
                        check(after.amplitude.value == before.amplitude.value && after.filter.value == before.filter.value,
                              row + " release is value-continuous");
                        check(reset.getVoiceInfo(1).held && same(held, reset.getRankEnvelopeState(1, 0)),
                              row + " held key and other channel are untouched");
                        check(render(reset, 257, 17) == render(pedal, 257, 31),
                              row + " release audio matches CC64 exactly across block sizes");
                        const auto tail = reset.getRankEnvelopeState(0, 0);
                        reset.controlChange(121, 0.0f, 2);
                        check(same(tail, reset.getRankEnvelopeState(0, 0)), row + " repeat reset cannot restart tail");
                        for (auto* e : {&reset, &pedal})
                        {
                            e->noteOff(64, 3);
                            check(e->getVoiceInfo(1).pedalHeld, row + " other channel pedal stays down");
                            e->noteOn(67, 0.8f, 4);
                        }
                        check(render(reset) == render(pedal), row + " subsequent Sustain II policy is unchanged");
                        reset.noteOn(71, 0.8f, 2); reset.noteOff(71, 2);
                        bool laterPedalHold = false;
                        for (int voice = 0; voice < 8; ++voice)
                        {
                            const auto info = reset.getVoiceInfo(voice);
                            laterPedalHold |= info.note == 71 && info.channel == 2 && info.pedalHeld;
                        }
                        check(!laterPedalHold, row + " reset pedal stays off for later notes without a panic");
                    }
}
void testMpePedals()
{
    for (bool circuit : {false, true})
        for (int master : {1, 16})
            for (bool ownPedal : {false, true})
            {
                Engine e; auto p = patch(circuit); p[P_mpeOn] = 1.0f;
                e.prepare(48000.0); e.setParams(p);
                rpn(e, 1, 6, 3); rpn(e, 16, 6, 3);
                const int member = master == 1 ? 2 : 15;
                const int otherMaster = master == 1 ? 16 : 1;
                const int otherMember = master == 1 ? 15 : 2;
                e.noteOn(60, 0.8f, member); e.noteOn(64, 0.8f, otherMember);
                e.noteOn(67, 0.8f, 8); render(e);
                e.controlChange(64, 1.0f, master); e.controlChange(64, 1.0f, otherMaster);
                e.controlChange(64, 1.0f, 8);
                if (ownPedal) e.controlChange(64, 1.0f, member);
                e.noteOff(60, member); e.noteOff(64, otherMember); e.noteOff(67, 8);
                e.controlChange(121, 0.0f, member);
                check(e.getVoiceInfo(0).pedalHeld, "member reset preserves inherited master pedal");
                if (ownPedal) e.controlChange(64, 1.0f, member);
                e.controlChange(121, 0.0f, master);
                check(e.getVoiceInfo(0).pedalHeld == ownPedal,
                      "master reset preserves independently down member pedal");
                check(e.getVoiceInfo(1).pedalHeld && e.getVoiceInfo(2).pedalHeld,
                      "master reset leaves other zone and conventional gap pedal intact");
                e.controlChange(121, 0.0f, member);
                check(!e.getVoiceInfo(0).pedalHeld, "member reset releases after master is up");
                e.controlChange(121, 0.0f, 8);
                check(!e.getVoiceInfo(2).pedalHeld && e.getVoiceInfo(1).pedalHeld,
                      "gap reset is channel-scoped");
            }
}
template <class Cc, class Zones>
void selectorCases(const std::string& label, Cc cc, Zones zones)
{
    const auto select = [&](int channel, int parameter, int value) {
        cc(channel, 101, 0); cc(channel, 100, parameter); cc(channel, 6, value);
    };
    select(1, 6, 7); select(16, 6, 7);
    for (int channel : {1, 16})
    {
        cc(channel, 121, 0); cc(channel, 6, 2);
        check(zones().lowerMemberCount() == 7 && zones().upperMemberCount() == 7,
              label + " reset deselects MCM without reconfiguring zones");
    }
    // Rebuild the fixture so a baseline MCM failure cannot turn later member
    // channels into gaps and mask the independent range-reset checks.
    select(1, 6, 7); select(16, 6, 7);
    for (int channel : {1, 2, 10, 16})
        for (int firstSelector : {100, 101})
            for (int family : {0, 98, 99})
            {
                const bool upper = channel >= 10, master = channel == 1 || channel == 16;
                const auto range = [&] { return master ? zones().masterRange(upper) : zones().memberRange(upper); };
                select(channel, 0, 24); cc(channel, 38, 50);
                if (family != 0) { cc(channel, family, 1); cc(channel, 6, 3); }
                cc(channel, 121, 127);
                check(range() == 24.5f, label + " reset retains negotiated range");
                cc(channel, 38, 25); cc(channel, 6, 9);
                check(range() == 24.5f, label + " unselected data cannot alter range");
                cc(channel, firstSelector, 0); cc(channel, 6, 10);
                check(range() == 24.5f, label + " reset nulls both selector bytes");
                cc(channel, firstSelector == 100 ? 101 : 100, 0); cc(channel, 38, 75);
                check(range() == 24.5f, label + " reselecting cannot reuse pre-selection coarse data");
                cc(channel, 6, 12); cc(channel, 38, 25);
                check(range() == 12.25f, label + " complete RPN reselection works after reset");
            }
    select(2, 0, 24); select(10, 0, 36);
    cc(2, 121, 0); cc(10, 6, 40);
    check(zones().memberRange(false) == 24.0f && zones().memberRange(true) == 40.0f,
          label + " reset leaves other channel selector active");
    for (int channel : {-1, 0, 17}) cc(channel, 121, 0);
    cc(10, 6, 41);
    check(zones().memberRange(true) == 41.0f, label + " invalid channel reset is inert");
}
void testSelectors()
{
    MpeZoneLayout layout;
    selectorCases("layout", [&](int channel, int cc, int value) { layout.controlChange(channel, cc, value); },
                  [&]() -> const MpeZoneLayout& { return layout; });
    for (bool circuit : {false, true})
    {
        Engine e; auto p = patch(circuit); p[P_mpeOn] = 1.0f;
        e.prepare(48000.0); e.setParams(p);
        selectorCases("engine", [&](int channel, int cc, int value) { e.controlChange(cc, value / 127.0f, channel); },
                      [&]() -> const MpeZoneLayout& { return e.getMpeZones(); });
        rpn(e, 2, 0, 24);
        p[P_mpeOn] = 0.0f; e.setParams(p); e.controlChange(121, 0.0f, 2);
        p[P_mpeOn] = 1.0f; e.setParams(p); e.controlChange(6, 12.0f / 127.0f, 2);
        check(e.getMpeZones().memberRange(false) == 24.0f,
              "reset while MPE is off prevents stale selection when reenabled");
    }
}
void testMalformedAndIsolation()
{
    for (bool circuit : {false, true})
    {
        Engine e; auto p = patch(circuit); p[P_mpeOn] = 1.0f;
        e.prepare(48000.0); e.setParams(p);
        rpn(e, 2, 0, 24); e.noteOn(60, 0.8f, 2); render(e);
        e.controlChange(64, 1.0f, 2); e.noteOff(60, 2);
        for (int channel : {-1, 0, 17}) e.controlChange(121, 0.0f, channel);
        for (float value : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::quiet_NaN()})
            e.controlChange(121, value, 2);
        check(e.getVoiceInfo(0).pedalHeld, "malformed reset cannot release pedal");
        e.controlChange(38, 0.5f, 2);
        check(e.getMpeZones().memberRange(false) == 24.64f,
              "malformed reset cannot null a valid selector or its coarse-data latch");

        // CC121 must not synthesize a CC64 gesture through the ribbon adapter.
        p[P_ribbonCcEnable] = 1.0f; p[P_ribbonCc] = 64.0f; e.setParams(p);
        e.controlChange(64, 1.0f, 2); e.setRibbon(0.0f); render(e, 960);
        e.controlChange(121, 0.0f, 2); render(e, 960);
        check(e.getVoiceInfo(0).ribbonRatio == 1.0f, "reset leaves the independent ribbon position unchanged");
    }
    Engine reset, reference;
    auto p = patch(true); p[P_parityMode] = 1.0f;
    for (auto* e : {&reset, &reference})
    {
        e->prepare(48000.0); e->setParams(p); e->noteOn(60, 0.8f, 2);
        e->controlChange(64, 1.0f, 2); render(*e); e->noteOff(60, 2);
    }
    reset.controlChange(121, 0.0f, 2);
    check(render(reset) == render(reference), "PURE CS-80 audio ignores MIDI pedal/reset controls");
}
void testHeldKeysAndWaitingCircuit()
{
    for (bool circuit : {false, true})
        for (int channel = 1; channel <= 16; ++channel)
        {
            Engine e; e.prepare(48000.0); e.setParams(patch(circuit));
            e.noteOn(60, 0.8f, channel); render(e, 64);
            e.controlChange(64, 1.0f, channel);
            const auto before = e.getRankEnvelopeState(0, 0);
            e.controlChange(121, 0.0f, channel);
            check(e.getVoiceInfo(0).held && same(before, e.getRankEnvelopeState(0, 0)),
                  "reset preserves physically held keys on every addressed channel");
            e.noteOff(60, channel);
            check(!e.getVoiceInfo(0).pedalHeld, "later key-up sees the addressed pedal reset");
        }
    Engine reset, pedal; auto p = patch(true); p[P_kasScanTiming] = 1.0f;
    for (auto* e : {&reset, &pedal})
    {
        e->prepare(48000.0); e->setParams(p);
        for (int note = 40; note < 48; ++note) e->noteOn(note, 0.8f, 2);
        render(*e, 64); e->controlChange(64, 1.0f, 2); e->noteOff(40, 2);
        e->noteOn(72, 0.8f, 3); render(*e, 64);
        check(e->getVoiceInfo(0).note == 40 && e->getVoiceInfo(0).pedalHeld,
              "waiting-key fixture fills every circuit including pedal hold");
    }
    reset.controlChange(121, 0.0f, 2); pedal.controlChange(64, 0.0f, 2);
    check(reset.getVoiceInfo(0).note == 72 && reset.getVoiceInfo(0).held,
          "reset reconsiders waiting keys when a pedal-held circuit is released");
    check(render(reset) == render(pedal), "waiting-key continuation matches ordinary pedal release");
}
void testOtherControllersUnchanged()
{
    for (bool circuit : {false, true})
        for (bool mpe : {false, true})
        {
            Engine reset, reference; auto p = patch(circuit); p[P_mpeOn] = mpe ? 1.0f : 0.0f;
            for (auto* e : {&reset, &reference})
            {
                e->prepare(48000.0); e->setParams(p); e->noteOn(60, 0.8f, 2);
                e->controlChange(1, 0.7f, 2); e->controlChange(11, 0.6f, 2);
                e->controlChange(74, 0.8f, 2); e->channelPressure(2, 0.4f);
                e->pitchBend(2, 0.25f); e->setRibbon(0.1f); render(*e);
            }
            reset.controlChange(121, 0.0f, 2);
            check(render(reset) == render(reference),
                  "narrow reset leaves modulation, expression, pressure, bend, timbre and ribbon unchanged");
        }
}
}
int main()
{
    testPedalRelease(); testMpePedals(); testSelectors(); testMalformedAndIsolation();
    testHeldKeysAndWaitingCircuit(); testOtherControllersUnchanged();
    std::printf("Controller reset: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
