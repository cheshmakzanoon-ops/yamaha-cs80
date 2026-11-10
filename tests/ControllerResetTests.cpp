// Bounded CC121 defaults using existing global/member/manager routes.
// This is not complete MIDI/MPE conformance or a hardware-fidelity claim.
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
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
bool sameAudio(const std::vector<float>& a, const std::vector<float>& b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](float x, float y) {
        return std::bit_cast<std::uint32_t>(x) == std::bit_cast<std::uint32_t>(y);
    });
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
                        check(sameAudio(render(reset, 257, 17), render(pedal, 257, 31)),
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
                        check(sameAudio(render(reset), render(pedal)), row + " subsequent Sustain II policy is unchanged");
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
    check(sameAudio(render(reset), render(reference)), "PURE CS-80 audio ignores MIDI pedal/reset controls");
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
    check(sameAudio(render(reset), render(pedal)), "waiting-key continuation matches ordinary pedal release");
}
void ordinaryDefaults(Engine& e, int channel)
{
    e.controlChange(1, 0.0f, channel);
    e.controlChange(11, 1.0f, channel);
    e.pitchBend(channel, 0.0f);
    e.channelPressure(channel, 0.0f);
    e.controlChange(64, 0.0f, channel);
}
void primeControls(Engine& e, int channel)
{
    e.controlChange(1, 0.7f, channel); e.controlChange(11, 0.3f, channel);
    e.controlChange(74, 0.8f, channel); e.channelPressure(channel, 0.4f);
    e.pitchBend(channel, 0.25f); e.setRibbon(0.1f);
}
void testDefaultsAndPreservation()
{
    // The reference is the existing ordinary event route, not new reset code.
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (bool circuit : {false, true})
            for (int channel = 1; channel <= 16; ++channel)
            {
                Engine reset, reference;
                auto p = patch(circuit);
                p[P_modWheelSub] = p[P_modWheelVcf] = 0.7f;
                for (auto* e : {&reset, &reference})
                {
                    e->prepare(rate); e->setParams(p); e->noteOn(60, 0.8f, channel);
                    primeControls(*e, channel); render(*e, 511);
                    e->polyPressure(60, 0.9f, channel); // same voice target as channel pressure
                }
                const auto before = reset.getRankEnvelopeState(0, 0);
                reset.controlChange(121, 1.0f, channel);
                ordinaryDefaults(reference, channel);
                check(reset.getVoiceInfo(0).pressure == 0.0f && reset.getVoiceInfo(0).held,
                      "reset clears poly-pressure target without releasing held note");
                check(same(before, reset.getRankEnvelopeState(0, 0)),
                      "reset does not restart or snap held envelopes");
                check(sameAudio(render(reset, 769, 17), render(reference, 769, 63)),
                      "reset follows ordinary smoothed defaults and preserves timbre/ribbon on all channels/rates");
                reset.controlChange(121, 0.0f, channel);
                ordinaryDefaults(reference, channel);
                check(sameAudio(render(reset), render(reference)), "repeated default reset preserves the continuation");
                for (auto* e : {&reset, &reference}) e->noteOn(67, 0.6f, channel);
                check(sameAudio(render(reset), render(reference)), "later notes use the reset global targets");
            }
    // Distinct matrix taps expose the shared channel-pressure target and the
    // deliberately preserved CC74 value, independent of panel touch depth.
    for (bool circuit : {false, true})
    {
        using namespace cs80::modern;
        Engine e; auto p = patch(circuit);
        p[P_mod1Amount] = p[P_mod2Amount] = 1.0f;
        ModernConfig config;
        config.slots[0] = {ModSource::channelPressure, ModSource::none, ModDestination::pan, ModTarget::both};
        config.slots[1] = {ModSource::timbre, ModSource::none, ModDestination::pitch, ModTarget::both};
        e.prepare(48000.0); e.setParams(p); e.setModernConfig(config);
        e.noteOn(60, 0.8f, 2); e.noteOn(64, 0.8f, 3);
        e.controlChange(74, 0.75f, 2); e.channelPressure(2, 0.5f); render(e, 4096);
        check(e.getModernRankOffsets(0, 0).pan > 0.49f, "shared pressure matrix fixture is active");
        e.controlChange(121, 0.0f, 3); render(e, 4096);
        check(std::abs(e.getModernRankOffsets(0, 0).pan) < 1.0e-5f
                  && e.getVoiceInfo(0).pressure == 0.5f,
              "channel-three reset clears shared matrix pressure but retains channel-two voice pressure");
        check(std::abs(e.getModernRankOffsets(0, 0).pitch - 9.0f) < 1.0e-4f,
              "reset preserves the CC74 timbre matrix source");
        e.controlChange(121, 0.0f, 1);
        check(e.getVoiceInfo(0).pressure == 0.0f && e.getVoiceInfo(1).pressure == 0.0f,
              "conventional channel-one reset retains historical pressure broadcast scope");
    }
}
void testMpeExpressionReset()
{
    for (bool circuit : {false, true})
        for (int channel : {1, 2, 3, 8, 14, 15, 16})
        {
            Engine reset, reference; auto p = patch(circuit); p[P_mpeOn] = 1.0f;
            p[P_modWheelSub] = p[P_modWheelVcf] = 0.7f;
            constexpr std::array<int, 5> channels {2, 3, 8, 14, 15};
            for (auto* e : {&reset, &reference})
            {
                e->prepare(48000.0); e->setParams(p);
                rpn(*e, 1, 6, 3); rpn(*e, 16, 6, 3);
                rpn(*e, 1, 0, 12); rpn(*e, 16, 0, 7);
                rpn(*e, 2, 0, 24); rpn(*e, 15, 0, 36);
                for (int i = 0; i < 5; ++i)
                {
                    e->noteOn(48 + i * 3, 0.8f, channels[i]);
                    primeControls(*e, channels[i]);
                }
                e->pitchBend(1, 0.5f); e->pitchBend(16, -0.5f);
                render(*e, 511);
            }
            reset.controlChange(121, 0.0f, channel); ordinaryDefaults(reference, channel);
            for (int i = 0; i < 5; ++i)
            {
                const int vc = channels[i];
                const bool own = vc == channel;
                const bool managed = (channel == 1 && vc < 5) || (channel == 16 && vc > 12);
                const float range = vc < 5 ? 24.0f : 36.0f; // gap retains existing lower-range routing below
                const float initialBend = vc == 8 ? 6.0f : range * 0.25f;
                check(reset.getVoiceInfo(i).bend == (own ? 0.0f : initialBend),
                      "MPE reset clears only addressed member bend, retaining independently bent members");
                check(reset.getVoiceInfo(i).pressure == (own || managed ? 0.0f : 0.4f),
                      "MPE reset pressure follows own/manager scope without crossing zone or gap");
            }
            check(reset.getMpeZones().lowerMemberCount() == 3 && reset.getMpeZones().upperMemberCount() == 3
                      && reset.getMpeZones().memberRange(false) == 24.0f
                      && reset.getMpeZones().memberRange(true) == 36.0f
                      && reset.getMpeZones().masterRange(false) == 12.0f
                      && reset.getMpeZones().masterRange(true) == 7.0f,
                  "controller defaults preserve both negotiated layouts and all bend ranges");
            check(sameAudio(render(reset, 2049, 31), render(reference, 2049, 127)),
                  "MPE reset audio matches existing master/member bend and smoothed pressure routes");
        }
}
void testRibbonAndPurePreservation()
{
    for (bool circuit : {false, true})
        for (int ribbonCc : {1, 11, 64, 121})
        {
            Engine reset, reference;
            auto p = patch(circuit); p[P_ribbonCcEnable] = 1.0f; p[P_ribbonCc] = static_cast<float>(ribbonCc);
            for (auto* e : {&reset, &reference})
            {
                e->prepare(48000.0); e->setParams(p); e->noteOn(60, 0.8f, 2);
                primeControls(*e, 2); render(*e, 511);
            }
            reset.controlChange(121, 0.0f, 2);
            // Disable the reference's adapter only while sending the reference
            // events, then restore the same params. No audio advances between.
            auto disabled = p; disabled[P_ribbonCcEnable] = 0.0f;
            reference.setParams(disabled); ordinaryDefaults(reference, 2); reference.setParams(p);
            check(sameAudio(render(reset, 1024), render(reference, 1024)),
                  "CC1/11/64/121 ribbon mappings do not turn reset into a ribbon gesture");
        }
    for (bool circuit : {false, true})
    {
        Engine reset, reference; auto p = patch(circuit); p[P_parityMode] = 1.0f;
        for (auto* e : {&reset, &reference})
        {
            e->prepare(48000.0); e->setParams(p); e->noteOn(60, 0.8f, 2);
            e->setExpression(0.25f); e->setRibbon(0.2f); e->channelPressure(2, 0.7f); render(*e, 511);
        }
        reset.controlChange(121, 0.0f, 2);
        check(sameAudio(render(reset, 1024), render(reference, 1024)) && reset.getVoiceInfo(0).pressure == 0.7f,
              "PURE reset retains physical expression, pressure, ribbon and exact audio");
    }
    for (bool circuit : {false, true})
    {
        Engine invalid, reference; auto p = patch(circuit);
        for (auto* e : {&invalid, &reference})
        {
            e->prepare(48000.0); e->setParams(p); e->noteOn(60, 0.8f, 2);
            primeControls(*e, 2); render(*e, 511);
        }
        for (int channel : {-1, 0, 17}) invalid.controlChange(121, 0.0f, channel);
        for (float value : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::quiet_NaN()})
            invalid.controlChange(121, value, 2);
        check(sameAudio(render(invalid, 1024), render(reference, 1024)),
              "malformed resets cannot change any primed sound target");
    }
}
void testMalformedChannelMode()
{
    for (bool circuit : {false, true})
        for (int controller : {120, 123})
            for (int channel : {0, 2})
                for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                                      std::numeric_limits<float>::infinity(),
                                      -std::numeric_limits<float>::infinity()})
                {
                    Engine event, reference;
                    for (auto* e : {&event, &reference})
                    {
                        e->prepare(48000.0); e->setParams(patch(circuit));
                        e->noteOn(60, 0.8f, 2); e->noteOn(64, 0.7f, 3);
                        render(*e, 128);
                    }
                    event.controlChange(controller, invalid, channel);
                    check(event.getVoiceInfo(0).held && event.getVoiceInfo(1).held,
                          "non-finite channel-mode message preserves held notes");
                    check(sameAudio(render(event, 512), render(reference, 512)),
                          "non-finite channel-mode message is audio inert");
                    // Finite command payloads retain the explicit channel-zero
                    // application panic and ordinary channel-selective behavior.
                    event.controlChange(controller, 0.0f, channel);
                    check(!event.getVoiceInfo(0).held, "finite mode message releases target");
                    check(event.getVoiceInfo(1).held == (channel != 0),
                          "finite mode message preserves channel scope");
                }
}
void testPrefixChannelReset()
{
    for (int channel = 1; channel <= 16; ++channel)
    {
        HighResolutionVelocity velocity;
        for (int c = 1; c <= 16; ++c) velocity.controlChange(c, 53);
        for (int invalid : {-1, 0, 17}) velocity.resetChannel(invalid);
        velocity.resetChannel(channel); velocity.resetChannel(channel);
        for (int c = 1; c <= 16; ++c)
        {
            const float expected = c == channel ? 80.0f * (1.0f / 127.0f) : ((80 << 7) | 53) / 16383.0f;
            check(velocity.noteOnVelocity(c, 80) == expected, "prefix reset affects only the addressed channel");
            check(velocity.noteOnVelocity(c, 80) == 80.0f * (1.0f / 127.0f), "prefix remains one-shot after reset");
        }
    }
}

}
int main()
{
    testPedalRelease(); testMpePedals(); testSelectors(); testMalformedAndIsolation();
    testHeldKeysAndWaitingCircuit(); testDefaultsAndPreservation(); testMpeExpressionReset();
    testRibbonAndPurePreservation(); testPrefixChannelReset(); testMalformedChannelMode();
    std::printf("Controller reset: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
