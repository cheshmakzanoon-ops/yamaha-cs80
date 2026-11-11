// Bounded MPE v1.1 section 2.2.3 receiver regression: channels crossing the
// none <-> MPE boundary stop sounding notes and discard channel-owned state.
// Shared expression/modulation/pressure/ribbon/manager buses are not claimed
// to be channel-local, and this is not full MCM or MIDI/MPE conformance.
#include "plugin/PluginProcessor.h"

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
using namespace cs80;
int checks = 0, failures = 0;
std::string context;

void check(bool ok, const char* label)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::cerr << "FAIL: " << label << " [" << context << "]\n";
    }
}

void cc(juce::MidiBuffer& midi, int channel, int number, int value, int offset = 0)
{
    midi.addEvent(juce::MidiMessage::controllerEvent(channel, number, value), offset);
}

void rpn(juce::MidiBuffer& midi, int channel, int number, int value, int offset = 0)
{
    cc(midi, channel, 101, 0, offset);
    cc(midi, channel, 100, number, offset);
    cc(midi, channel, 6, value, offset);
}

void on(juce::MidiBuffer& midi, int channel, int note, int offset = 0)
{
    midi.addEvent(juce::MidiMessage::noteOn(channel, note, (juce::uint8) 80), offset);
}

void off(juce::MidiBuffer& midi, int channel, int note, int offset = 0)
{
    midi.addEvent(juce::MidiMessage::noteOff(channel, note), offset);
}

struct FoundVoice
{
    const Engine* engine = nullptr;
    int index = -1;
    Engine::VoiceInfo info;
};

FoundVoice find(Cs80AudioProcessor& host, int note, int channel)
{
    for (const auto* engine : std::array<const Engine*, 2>{&host.getEngine(), &host.getExtensionEngine()})
        for (int index = 0; index < 8; ++index)
        {
            const auto info = engine->getVoiceInfo(index);
            if (info.active && info.note == note && info.channel == channel)
                return {engine, index, info};
        }
    return {};
}

struct Host
{
    std::unique_ptr<Cs80AudioProcessor> processor;
    juce::AudioBuffer<float> audio {2, 0};
    juce::MidiBuffer midi;

    Host(const juce::File& fixture, bool circuit, bool sixteen)
    {
        processor = std::make_unique<Cs80AudioProcessor>(fixture);
        processor->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit
                                         : Cs80AudioProcessor::EngineMode::legacy);
        for (int parameter : std::array<int, 9>{P_parityMode, P_kasScanTiming, P_arpOn,
                              P_voiceDispersion, P_drift, P_analogNoise,
                              P_velocityCurve, P_pressureCurve, P_pressureThreshold})
            processor->setParamValue(parameter, 0);
        processor->setParamValue(P_mpeOn, 1);
        processor->setParamValue(P_voiceMode, sixteen ? 1 : 0);
        processor->setParamValue(P_keyRelease, 5);
        processor->setParamValue(P_mod1Amount, 1);
        processor->getModernLayer().setSlot(0, {modern::ModSource::timbre, modern::ModSource::none,
                                               modern::ModDestination::pulseWidth, modern::ModTarget::both});
        processor->prepareToPlay(48000, 256);
        tick(); // Accept parameters and activate the extension before negotiation.
    }

    void tick(int frames = 0)
    {
        audio.setSize(2, frames);
        processor->processBlock(audio, midi);
        midi.clear();
        bool finite = true;
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int i = 0; i < frames; ++i)
                finite &= std::isfinite(audio.getSample(channel, i));
        check(finite, "host render remains finite");
    }

    void layout(int lower, int upper)
    {
        rpn(midi, 1, 6, 0); rpn(midi, 16, 6, 0);
        if (lower) rpn(midi, 1, 6, lower);
        if (upper) rpn(midi, 16, 6, upper);
        tick();
    }
};

struct Transition { int before, after; };
constexpr std::array<Transition, 4> transitions {{{0, 2}, {2, 0}, {2, 4}, {4, 2}}};

std::vector<int> affectedChannels(bool upper, Transition transition)
{
    MpeZoneLayout zones;
    auto zone = [&](int channel, int count) {
        zones.controlChange(channel, 101, 0);
        zones.controlChange(channel, 100, 6);
        zones.controlChange(channel, 6, count);
    };
    zone(1, 0); zone(16, 0);
    zone(upper ? 1 : 16, 2);
    zone(upper ? 16 : 1, transition.before);
    std::array<bool, 17> before {};
    for (int channel = 1; channel <= 16; ++channel)
        before[(size_t) channel] = zones.role(channel) != MpeZoneLayout::Role::none;
    zone(upper ? 16 : 1, transition.after);
    std::vector<int> channels;
    for (int channel = 1; channel <= 16; ++channel)
        if (before[(size_t) channel] != (zones.role(channel) != MpeZoneLayout::Role::none))
            channels.push_back(channel);
    return channels;
}

void testSoundingNotes(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
        for (bool upper : {false, true}) for (int frames : {0, 32})
            for (auto transition : transitions)
    {
        context = std::string(circuit ? "circuit" : "legacy") + (sixteen ? "/16" : "/8")
            + (upper ? "/upper/" : "/lower/") + std::to_string(transition.before) + "->"
            + std::to_string(transition.after) + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen);
        host.layout(upper ? 2 : transition.before, upper ? transition.before : 2);
        const auto affected = affectedChannels(upper, transition);
        check(!affected.empty(), "transition fixture changes actual MPE membership");
        if (affected.empty()) continue;
        const int other = upper ? 2 : 15;
        const int capacity = sixteen ? 16 : 8;
        std::array<int, 16> channels {};
        for (int i = 0; i < capacity; ++i)
        {
            channels[(size_t) i] = i % 2 ? other : affected[(size_t) (i / 2) % affected.size()];
            on(host.midi, channels[(size_t) i], 40 + i);
        }
        host.tick(32);
        if (sixteen)
            check(find(*host.processor, 48, channels[8]).engine == &host.processor->getExtensionEngine(),
                  "sixteen-voice witness really reaches the extension");
        // Real released tails, pedal-held releases, and held notes coexist.
        off(host.midi, channels[0], 40); off(host.midi, channels[1], 41); host.tick();
        cc(host.midi, channels[2], 64, 127); cc(host.midi, channels[3], 64, 127);
        off(host.midi, channels[2], 42); off(host.midi, channels[3], 43); host.tick();
        check(find(*host.processor, 40, channels[0]).info.active
                  && !find(*host.processor, 40, channels[0]).info.held
                  && !find(*host.processor, 40, channels[0]).info.pedalHeld,
              "affected released tail is sounding before MCM");
        check(find(*host.processor, 42, channels[2]).info.pedalHeld
                  && find(*host.processor, 43, channels[3]).info.pedalHeld,
              "affected and unaffected pedal-held witnesses are sounding before MCM");
        std::array<Engine::VoiceInfo, 16> before;
        for (int i = 0; i < capacity; ++i)
        {
            before[(size_t) i] = find(*host.processor, 40 + i, channels[(size_t) i]).info;
            check(before[(size_t) i].active, "every allocated pre-transition voice is active");
        }
        rpn(host.midi, upper ? 16 : 1, 6, transition.after);
        host.tick(frames);
        for (int i = 0; i < capacity; ++i)
        {
            const auto after = find(*host.processor, 40 + i, channels[(size_t) i]).info;
            if (i % 2 == 0)
            {
                check(!after.active, "entering/leaving channel stops held notes, pedals and released tails immediately");
                check(!host.processor->isHostKeyDown(40 + i), "MCM clears affected host-held display ownership");
            }
            else
            {
                const auto old = before[(size_t) i];
                check(after.active && after.held == old.held && after.pedalHeld == old.pedalHeld
                          && after.bend == old.bend && after.pressure == old.pressure && after.velocity == old.velocity,
                      "opposite zone retains its held notes, released tails and pedal holds");
                check(host.processor->isHostKeyDown(40 + i) == (i >= 4),
                      "opposite-zone host-held display ownership survives MCM");
            }
        }
    }
}

void testPrefixAndOrdering(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
        for (bool upper : {false, true}) for (bool entering : {false, true})
            for (bool prefixAfter : {false, true}) for (int frames : {0, 32})
    {
        context = std::string("prefix/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/upper" : "/lower")
            + (entering ? "/enter" : "/leave") + (prefixAfter ? "/after" : "/before")
            + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen);
        const int before = entering ? 2 : 4, after = entering ? 4 : 2;
        const int changed = upper ? 13 : 4, other = upper ? 2 : 15;
        host.layout(upper ? 2 : before, upper ? before : 2);
        if (sixteen)
        {
            for (int n = 40; n < 48; ++n) on(host.midi, other, n);
            host.tick();
        }
        cc(host.midi, other, 88, 53);
        if (!prefixAfter) cc(host.midi, changed, 88, 53);
        on(host.midi, changed, 59);
        // Prefix placed after the pre-MCM note belongs to the later strike.
        if (!prefixAfter) cc(host.midi, changed, 88, 53);
        rpn(host.midi, upper ? 16 : 1, 6, after, 11);
        if (prefixAfter) cc(host.midi, changed, 88, 53, 11);
        on(host.midi, changed, 60, 21); on(host.midi, other, 64, 21);
        host.tick(frames);
        check(!find(*host.processor, 59, changed).info.active,
              "same-block pre-MCM onset is stopped in MIDI order");
        const auto current = find(*host.processor, 60, changed);
        check(current.info.held && current.info.velocity == (prefixAfter ? ((80 << 7) | 53) / 16383.0f : 80.0f * (1.0f / 127.0f)),
              "MCM clears only an earlier affected-channel prefix, preserving later same-offset CC88");
        const auto untouched = find(*host.processor, 64, other);
        check(untouched.info.held && untouched.info.velocity == ((80 << 7) | 53) / 16383.0f,
              "MCM preserves the opposite zone's pending velocity prefix");
        check(host.processor->isHostKeyDown(60) && host.processor->isHostKeyDown(64)
                  && !host.processor->isHostKeyDown(59),
              "fresh host note after MCM restores only its own held mask");
        if (sixteen)
            check(current.engine == &host.processor->getExtensionEngine(),
                  "post-MCM prefix witness actually uses the extension");
    }
}

void testChannelState(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true}) for (bool upper : {false, true})
    {
        context = std::string("state/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/upper" : "/lower");
        Host host(fixture, circuit, sixteen);
        const int manager = upper ? 16 : 1, member = upper ? 13 : 4, other = upper ? 2 : 15;
        host.layout(upper ? 2 : 4, upper ? 4 : 2);
        auto expression = [&](int channel) {
            host.midi.addEvent(juce::MidiMessage::pitchWheel(channel, 10240), 0);
            host.midi.addEvent(juce::MidiMessage::channelPressureChange(channel, 89), 0);
            cc(host.midi, channel, 74, 101); cc(host.midi, channel, 64, 127);
        };
        expression(member); expression(other);
        rpn(host.midi, member, 0, 12); host.tick();
        rpn(host.midi, manager, 6, 2); host.tick();
        rpn(host.midi, manager, 6, 4); host.tick();
        // The removed member's RPN0 selector must not survive its re-entry.
        cc(host.midi, member, 6, 7); host.tick();
        check(host.processor->getEngine().getMpeZones().memberRange(upper) == 48,
              "membership transition deselects stale member-channel RPN data");
        if (sixteen)
        {
            for (int n = 40; n < 48; ++n) on(host.midi, other, n);
            host.tick();
        }
        on(host.midi, member, 60); on(host.midi, other, 64); host.tick(32);
        auto reset = find(*host.processor, 60, member), preserved = find(*host.processor, 64, other);
        check(reset.info.held && reset.info.bend == 0 && reset.info.pressure == 0,
              "re-entered member begins with neutral role-bound bend and pressure");
        check(preserved.info.held && preserved.info.bend == 12 && preserved.info.pressure == 89.0f / 127.0f,
              "unchanged member preserves cached onset bend and pressure");
        if (reset.index >= 0 && preserved.index >= 0)
            for (int rank = 0; rank < 2; ++rank)
            {
                check(reset.engine->getModernRankOffsets(reset.index, rank).pulseWidth == 0,
                      "re-entered member begins with neutral role-bound timbre");
                check(preserved.engine->getModernRankOffsets(preserved.index, rank).pulseWidth
                          == (101.0f / 127.0f) * modern::kPulseWidthRange,
                      "unchanged member keeps cached onset timbre");
            }
        off(host.midi, member, 60); off(host.midi, other, 64); host.tick();
        check(!find(*host.processor, 60, member).info.pedalHeld,
              "affected channel pedal is up for subsequent notes");
        check(find(*host.processor, 64, other).info.pedalHeld,
              "unchanged channel pedal remains down for subsequent notes");
    }
    // Managers also cross none <-> MPE and own a channel-local pedal/selector.
    for (bool circuit : {false, true}) for (bool upper : {false, true})
    {
        context = std::string("manager-pedal/") + (circuit ? "circuit" : "legacy") + (upper ? "/upper" : "/lower");
        Host host(fixture, circuit, false);
        const int manager = upper ? 16 : 1, member = upper ? 15 : 2;
        host.layout(upper ? 2 : 0, upper ? 0 : 2);
        cc(host.midi, manager, 64, 127); host.tick();
        rpn(host.midi, manager, 6, 2); host.tick();
        on(host.midi, member, 60); off(host.midi, member, 60); host.tick();
        check(!find(*host.processor, 60, member).info.pedalHeld,
              "entering manager cannot lend its stale conventional pedal to new members");
        cc(host.midi, manager, 64, 127); host.tick();
        rpn(host.midi, manager, 6, 0); host.tick();
        cc(host.midi, manager, 6, 2); host.tick();
        check(host.processor->getEngine().getMpeZones().role(manager) == MpeZoneLayout::Role::none,
              "departing manager's old MCM selector cannot re-enable its zone");
        on(host.midi, manager, 62); off(host.midi, manager, 62); host.tick();
        check(!find(*host.processor, 62, manager).info.pedalHeld,
              "departing manager's own conventional channel pedal is reset");
    }
}

void testNonMembershipChanges(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
    {
        context = std::string("non-membership/") + (circuit ? "circuit" : "legacy") + (sixteen ? "/16" : "/8");
        Host host(fixture, circuit, sixteen); host.layout(4, 2);
        on(host.midi, 4, 60); cc(host.midi, 4, 64, 127); host.tick();
        cc(host.midi, 4, 88, 53); rpn(host.midi, 1, 6, 4); host.tick();
        check(find(*host.processor, 60, 4).info.held && host.processor->isHostKeyDown(60),
              "same-size MCM does not stop an unchanged channel");
        on(host.midi, 4, 62); off(host.midi, 4, 62); host.tick();
        check(find(*host.processor, 62, 4).info.pedalHeld
                  && find(*host.processor, 62, 4).info.velocity == ((80 << 7) | 53) / 16383.0f,
              "same-size MCM retains channel pedal and prefix");
        // Unsupported NRPN data and member-channel RPN6 are not MCMs.
        cc(host.midi, 1, 99, 0); cc(host.midi, 1, 98, 6); cc(host.midi, 1, 6, 0);
        rpn(host.midi, 4, 6, 0); host.tick();
        check(host.processor->getEngine().getMpeZones().lowerMemberCount() == 4
                  && find(*host.processor, 60, 4).info.held && host.processor->isHostKeyDown(60),
              "NRPN and member-channel RPN6 cannot trigger membership reset");

        Host moved(fixture, circuit, sixteen); moved.layout(15, 0);
        for (int channel : {1, 2, 15, 16}) on(moved.midi, channel, 60 + channel);
        moved.tick(); rpn(moved.midi, 16, 6, 15); moved.tick();
        for (int channel : {1, 2, 15, 16})
            check(find(*moved.processor, 60 + channel, channel).info.held
                      && moved.processor->isHostKeyDown(60 + channel),
                  "still-MPE role/zone changes are outside the membership-reset kill boundary");

        Host shared(fixture, circuit, sixteen); shared.layout(4, 2);
        on(shared.midi, 4, 60); on(shared.midi, 15, 60); shared.tick();
        rpn(shared.midi, 1, 6, 2); shared.tick();
        check(!find(*shared.processor, 60, 4).info.active && find(*shared.processor, 60, 15).info.held
                  && shared.processor->isHostKeyDown(60),
              "same-pitch host display retains an unaffected channel's ownership bit");
        off(shared.midi, 15, 60); shared.tick();
        check(!shared.processor->isHostKeyDown(60), "last unaffected release clears the shared-pitch display");
    }
}

void testResizeSurvivors(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
        for (bool upper : {false, true}) for (bool entering : {false, true}) for (int frames : {0, 32})
    {
        context = std::string("resize-survivors/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/upper" : "/lower")
            + (entering ? "/enter" : "/leave") + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen);
        const int manager = upper ? 16 : 1, member = upper ? 15 : 2;
        const int changed = upper ? 13 : 4, other = upper ? 2 : 15;
        const int before = entering ? 2 : 4, after = entering ? 4 : 2;
        host.layout(upper ? 2 : before, upper ? before : 2);
        if (sixteen)
        {
            for (int note = 40; note < 48; ++note) on(host.midi, other, note);
            host.tick();
        }
        const std::array<int, 7> channels {manager, member, changed, 8, other, member, 8};
        for (int i = 0; i < (int) channels.size(); ++i) on(host.midi, channels[(size_t) i], 60 + i);
        host.tick(32);
        off(host.midi, member, 65); off(host.midi, 8, 66); host.tick();
        // The same-zone member and conventional gap each keep their own pedal
        // and an unconsumed prefix, in addition to held and released voices.
        for (int channel : {member, 8})
        {
            cc(host.midi, channel, 64, 127);
            cc(host.midi, channel, 88, 53);
        }
        host.tick();
        std::array<FoundVoice, 7> voices;
        for (int i = 0; i < (int) channels.size(); ++i)
        {
            voices[(size_t) i] = find(*host.processor, 60 + i, channels[(size_t) i]);
            check(voices[(size_t) i].info.active, "resize survivor witness is sounding before MCM");
            if (sixteen)
                check(voices[(size_t) i].engine == &host.processor->getExtensionEngine(),
                      "resize survivor witness really occupies the extension");
        }
        check(!voices[5].info.held && !voices[5].info.pedalHeld
                  && !voices[6].info.held && !voices[6].info.pedalHeld,
              "same-zone and conventional-gap released tails are real pre-MCM witnesses");

        // Exactly one received MCM changes 2 <-> 4 members. The addressed
        // manager and its first two members remain under MPE throughout;
        // channel 8 remains outside MPE. No all-zones-off layout helper here.
        rpn(host.midi, manager, 6, after); host.tick(frames);
        for (int i = 0; i < (int) channels.size(); ++i)
        {
            const auto now = find(*host.processor, 60 + i, channels[(size_t) i]);
            if (i == 2)
            {
                check(!now.info.active && !host.processor->isHostKeyDown(60 + i),
                      "direct resize resets only the witness crossing the membership boundary");
                continue;
            }
            const auto& old = voices[(size_t) i];
            check(now.engine == old.engine && now.index == old.index && now.info.active
                      && now.info.held == old.info.held && now.info.pedalHeld == old.info.pedalHeld
                      && now.info.velocity == old.info.velocity && now.info.bend == old.info.bend
                      && now.info.pressure == old.info.pressure,
                  "direct resize preserves same-zone manager/member, gap and opposite-zone voice ownership");
            check(host.processor->isHostKeyDown(60 + i) == (i < 5),
                  "direct resize preserves unaffected held masks without resurrecting released-tail keys");
        }
        on(host.midi, member, 67); on(host.midi, 8, 68); host.tick();
        for (int i = 0; i < 2; ++i)
        {
            const int channel = i == 0 ? member : 8;
            const auto current = find(*host.processor, 67 + i, channel);
            check(current.info.held && current.info.velocity == ((80 << 7) | 53) / 16383.0f,
                  "unchanged same-zone member and gap retain their pending CC88 prefixes");
            off(host.midi, channel, 67 + i);
        }
        host.tick();
        check(find(*host.processor, 67, member).info.pedalHeld
                  && find(*host.processor, 68, 8).info.pedalHeld,
              "unchanged same-zone member and gap retain their channel-local pedals");
    }
}

void testDeferredKeys(const juce::File& fixture)
{
    for (bool sixteen : {false, true}) for (bool upper : {false, true}) for (bool entering : {false, true})
    {
        const int manager = upper ? 16 : 1, changed = upper ? 13 : 4, other = upper ? 2 : 15;
        const int before = entering ? 2 : 4, after = entering ? 4 : 2;
        context = std::string("scanner/circuit") + (sixteen ? "/16" : "/8")
            + (upper ? "/upper" : "/lower") + (entering ? "/enter" : "/leave");
        Host queued(fixture, true, sixteen);
        queued.layout(upper ? 2 : before, upper ? before : 2);
        queued.processor->setParamValue(P_kasScanTiming, 1); queued.tick();
        on(queued.midi, changed, 60); on(queued.midi, other, 64); queued.tick();
        check(!find(*queued.processor, 60, changed).info.active
                  && !find(*queued.processor, 64, other).info.active,
              "scanner witness has queued, not yet allocated, notes");
        rpn(queued.midi, manager, 6, after); queued.tick(); queued.tick(256);
        check(!find(*queued.processor, 60, changed).info.active && !queued.processor->isHostKeyDown(60),
              "MCM removes pending scanner onset before it can sound");
        check(find(*queued.processor, 64, other).info.held && queued.processor->isHostKeyDown(64),
              "MCM retains the opposite zone's pending scanner onset");

        context.replace(0, 7, "waiting");
        Host waiting(fixture, true, sixteen);
        waiting.layout(upper ? 2 : before, upper ? before : 2);
        const int capacity = sixteen ? 16 : 8;
        for (int i = 0; i < capacity; ++i) on(waiting.midi, other, 40 + i);
        waiting.tick(); on(waiting.midi, changed, 80); on(waiting.midi, other, 84); waiting.tick();
        check(!find(*waiting.processor, 80, changed).info.active
                  && !find(*waiting.processor, 84, other).info.active,
              "full-capacity circuit fixture has two unassigned held keys");
        rpn(waiting.midi, manager, 6, after); waiting.tick();
        for (int i = 0; i < capacity; ++i) off(waiting.midi, other, 40 + i);
        waiting.tick(32);
        check(!find(*waiting.processor, 80, changed).info.active && !waiting.processor->isHostKeyDown(80),
              "removed unassigned key cannot be promoted when a circuit later frees");
        check(find(*waiting.processor, 84, other).info.held && waiting.processor->isHostKeyDown(84),
              "unchanged unassigned key still promotes when a circuit frees");
    }

    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
        for (bool upper : {false, true}) for (bool entering : {false, true})
    {
        context = std::string("arp/") + (circuit ? "circuit" : "legacy") + (sixteen ? "/16" : "/8")
            + (upper ? "/upper" : "/lower") + (entering ? "/enter" : "/leave");
        Host host(fixture, circuit, sixteen);
        const int manager = upper ? 16 : 1, changed = upper ? 13 : 4, other = upper ? 2 : 15;
        const int before = entering ? 2 : 4, after = entering ? 4 : 2;
        host.layout(upper ? 2 : before, upper ? before : 2);
        host.processor->setParamValue(P_arpOn, 1);
        host.processor->setParamValue(P_arpRate, 16);
        host.processor->setParamValue(P_arpOctaves, 1); host.tick();
        on(host.midi, changed, 60); on(host.midi, other, 64); host.tick(32);
        check(find(*host.processor, 60, changed).info.active, "ARP fixture starts a real affected sounding note");
        rpn(host.midi, manager, 6, after); host.tick();
        check(!find(*host.processor, 60, changed).info.active && !host.processor->isHostKeyDown(60),
              "MCM immediately stops affected ARP voice and removes its source key");
        bool sawOther = false, sawRemoved = false;
        for (int block = 0; block < 32; ++block)
        {
            host.tick(256);
            sawOther |= find(*host.processor, 64, other).info.held;
            sawRemoved |= find(*host.processor, 60, changed).info.active;
        }
        check(sawOther && host.processor->isHostKeyDown(64), "unchanged ARP source key continues generating notes");
        check(!sawRemoved, "removed ARP source key never reappears in later cycles");
    }
}

void testUiReleaseOwnership(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true}) for (int frames : {0, 32})
    {
        context = std::string("UI-ownership/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen); host.layout(4, 2);
        host.processor->serviceKeyboardUI(); // Acknowledge startup before real UI gestures.
        if (sixteen)
        {
            for (int note = 40; note < 48; ++note) on(host.midi, 15, note);
            host.tick();
        }
        auto& keyboard = host.processor->getKeyboardState();
        const auto removed = host.processor->captureKeyboardGesture(4, 60, [&] { keyboard.noteOn(4, 60, .8f); });
        const auto kept = host.processor->captureKeyboardGesture(2, 64, [&] { keyboard.noteOn(2, 64, .8f); });
        host.tick(frames);
        check(removed && kept && find(*host.processor, 60, 4).info.held
                  && find(*host.processor, 64, 2).info.held,
              "real captured UI gestures allocate affected and unchanged held notes");
        check(!host.processor->isHostKeyDown(60) && !host.processor->isHostKeyDown(64),
              "UI ownership fixture is not substituted host MIDI");
        if (sixteen)
            check(find(*host.processor, 60, 4).engine == &host.processor->getExtensionEngine()
                      && find(*host.processor, 64, 2).engine == &host.processor->getExtensionEngine(),
                  "UI ownership witnesses both occupy extension circuits");
        rpn(host.midi, 1, 6, 2); host.tick(frames);
        check(!find(*host.processor, 60, 4).info.active && find(*host.processor, 64, 2).info.held,
              "MCM cancels affected UI note while retaining unchanged UI note");

        on(host.midi, 4, 60); host.tick(frames);
        keyboard.noteOff(4, 60, 0); keyboard.noteOff(2, 64, 0);
        host.processor->setKeyPressureFromUI(removed, .9f); host.tick(frames);
        check(find(*host.processor, 60, 4).info.held && find(*host.processor, 60, 4).info.pressure == 0
                  && host.processor->isHostKeyDown(60),
              "retired UI release and pressure cannot cancel or alter a replacement host note");
        check(!find(*host.processor, 64, 2).info.held,
              "unchanged UI key retains its release obligation after MCM invalidates pressure attribution");
        const auto fresh = host.processor->captureKeyboardGesture(4, 62, [&] { keyboard.noteOn(4, 62, .8f); });
        host.tick(frames);
        check(fresh && find(*host.processor, 62, 4).info.held,
              "membership reset permits a fresh UI gesture without a global panic acknowledgment");
        keyboard.noteOff(4, 62, 0); host.tick(frames);
        check(!find(*host.processor, 62, 4).info.held && find(*host.processor, 60, 4).info.held,
              "fresh UI release works without disturbing the replacement host note");
    }
}

void testRejectedProtocolLearn(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true}) for (int frames : {0, 32})
    {
        context = std::string("rejected-protocol-learn/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen); host.layout(4, 2);
        if (sixteen)
        {
            for (int note = 40; note < 48; ++note) on(host.midi, 15, note);
            host.tick();
        }
        on(host.midi, 4, 60); on(host.midi, 2, 64); host.tick();
        if (sixteen)
            check(find(*host.processor, 60, 4).engine == &host.processor->getExtensionEngine(),
                  "rejected-learn reset witness really occupies the extension");
        // Protocol controllers are deliberately not learnable. Rejected CC6
        // mapping must leave normal MCM handling intact, not consume its data.
        cc(host.midi, 1, 101, 0); cc(host.midi, 1, 100, 6); host.tick();
        const bool assigned = host.processor->getMidiLearn().assign(6, host.processor->hostParameterIndex(P_volume));
        check(!assigned && host.processor->getMidiLearn().parameterFor(6) == MidiLearnMap::kNone,
              "MIDI learn rejects data-entry CC6 instead of capturing protocol traffic");
        host.processor->setParamValue(P_volume, .43f); host.tick();
        const float volume = host.processor->getParamValue(P_volume);
        cc(host.midi, 4, 88, 53); cc(host.midi, 1, 6, 2); host.tick(frames);
        check(host.processor->getParamValue(P_volume) == volume,
              "rejected CC6 mapping leaves the unrelated volume parameter unchanged");
        check(host.processor->getEngine().getMpeZones().lowerMemberCount() == 2
                  && host.processor->getEngine().getMpeZones().upperMemberCount() == 2,
              "CC6 still executes the selected MCM after its learn assignment is rejected");
        if (sixteen)
            check(host.processor->getExtensionEngine().getMpeZones().lowerMemberCount() == 2,
                  "unlearnable MCM data entry updates the extension's membership too");
        check(!find(*host.processor, 60, 4).info.active && find(*host.processor, 64, 2).info.held
                  && !host.processor->isHostKeyDown(60) && host.processor->isHostKeyDown(64),
              "rejected protocol learn retains correct affected cancellation and unchanged-note ownership");
        on(host.midi, 4, 62); host.tick();
        check(find(*host.processor, 62, 4).info.held
                  && find(*host.processor, 62, 4).info.velocity == 80.0f * (1.0f / 127.0f),
              "accepted MCM clears the affected channel's pending prefix despite rejected protocol learn");
    }
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-mpe-membership-reset");
    testSoundingNotes(fixture.getFile());
    testPrefixAndOrdering(fixture.getFile());
    testChannelState(fixture.getFile());
    testNonMembershipChanges(fixture.getFile());
    testResizeSurvivors(fixture.getFile());
    testDeferredKeys(fixture.getFile());
    testUiReleaseOwnership(fixture.getFile());
    testRejectedProtocolLearn(fixture.getFile());
    std::cout << "MPE membership reset: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
