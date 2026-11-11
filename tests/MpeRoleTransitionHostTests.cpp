// Product-policy regression for cancellation on every accepted MCM role change.
// MPE v1.1 section 2.2.3 and Appendix F require entering/leaving-MPE resets;
// extending cancellation to still-MPE zone/master/member transfers is this
// instrument's chosen safety policy, not an additional normative claim.
// Same-role channels and shared manager/controller buses must survive.
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

struct Transition
{
    const char* name;
    int lower, upper, manager, members;
    std::array<int, 4> changed;
    int survivor; // Zero means every channel changes role.
};
constexpr std::array<Transition, 6> transitions {{
    {"lower15->upper15", 15, 0, 16, 15, {1, 2, 15, 16}, 0},
    {"upper15->lower15", 0, 15, 1, 15, {16, 15, 2, 1}, 0},
    {"split7/7->lower15", 7, 7, 1, 15, {9, 14, 15, 16}, 2},
    {"split7/7->upper15", 7, 7, 16, 15, {8, 3, 2, 1}, 15},
    {"split2/2->lower14", 2, 2, 1, 14, {4, 14, 15, 16}, 2},
    {"split2/2->upper14", 2, 2, 16, 14, {13, 3, 2, 1}, 15}
}};

void testSoundingRoleChanges(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
        for (int frames : {0, 32}) for (const auto& transition : transitions)
    {
        context = std::string("sound/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16/" : "/8/") + transition.name + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen);
        host.layout(transition.lower, transition.upper);
        const auto beforeLayout = host.processor->getEngine().getMpeZones();
        const int capacity = sixteen ? 16 : 8;
        std::array<int, 16> channels {};
        for (int i = 0; i < capacity; ++i)
        {
            channels[(size_t) i] = (i % 2 && transition.survivor)
                ? transition.survivor : transition.changed[(size_t) (i / 2) % transition.changed.size()];
            on(host.midi, channels[(size_t) i], 40 + i);
        }
        host.tick(32);
        if (sixteen)
            check(find(*host.processor, 48, channels[8]).engine == &host.processor->getExtensionEngine(),
                  "sixteen-voice role-change witness actually reaches the extension");
        // Each fixture contains genuine released tails, pedal-held releases,
        // and held notes. The remaining held witnesses include role-changing
        // managers as well as members transferred between zones.
        off(host.midi, channels[0], 40); off(host.midi, channels[1], 41); host.tick();
        cc(host.midi, channels[2], 64, 127); cc(host.midi, channels[3], 64, 127);
        off(host.midi, channels[2], 42); off(host.midi, channels[3], 43); host.tick();
        const auto tail = find(*host.processor, 40, channels[0]);
        check(tail.info.active && !tail.info.held && !tail.info.pedalHeld,
              "affected released-tail witness really exists before MCM");
        check(find(*host.processor, 42, channels[2]).info.pedalHeld
                  && find(*host.processor, 43, channels[3]).info.pedalHeld,
              "pedal-held witnesses really exist before MCM");
        std::array<FoundVoice, 16> before;
        for (int i = 0; i < capacity; ++i)
        {
            before[(size_t) i] = find(*host.processor, 40 + i, channels[(size_t) i]);
            check(before[(size_t) i].info.active, "every pre-transition voice is sounding");
        }
        rpn(host.midi, transition.manager, 6, transition.members); host.tick(frames);
        const auto& afterLayout = host.processor->getEngine().getMpeZones();
        for (int i = 0; i < capacity; ++i)
        {
            const int channel = channels[(size_t) i];
            const bool changed = beforeLayout.role(channel) != afterLayout.role(channel);
            check(changed == (!(i % 2) || transition.survivor == 0),
                  "fixture distinguishes exact role changes from same-role survivors");
            const auto now = find(*host.processor, 40 + i, channel);
            if (changed)
            {
                check(!now.info.active, "every role-changing channel stops held notes, pedals and tails immediately");
                check(!host.processor->isHostKeyDown(40 + i), "role change clears channel-owned host display mask");
            }
            else
            {
                const auto& old = before[(size_t) i];
                check(now.engine == old.engine && now.index == old.index && now.info.active
                          && now.info.held == old.info.held && now.info.pedalHeld == old.info.pedalHeld
                          && now.info.bend == old.info.bend && now.info.pressure == old.info.pressure
                          && now.info.velocity == old.info.velocity,
                      "same-role voices retain allocation, held state, released tails and pedal holds");
                check(host.processor->isHostKeyDown(40 + i) == (i >= 4),
                      "same-role host display preserves only actual held keys");
            }
        }
    }
}

void testPrefixAndOrdering(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
        for (bool upper : {false, true}) for (bool prefixAfter : {false, true}) for (int frames : {0, 32})
    {
        context = std::string("prefix/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/to-upper" : "/to-lower")
            + (prefixAfter ? "/after" : "/before") + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen); host.layout(7, 7);
        const int changed = upper ? 3 : 14, survivor = upper ? 15 : 2, manager = upper ? 16 : 1;
        if (sixteen)
        {
            for (int n = 40; n < 48; ++n) on(host.midi, survivor, n);
            host.tick();
        }
        cc(host.midi, survivor, 88, 53);
        on(host.midi, changed, 59);
        if (!prefixAfter) cc(host.midi, changed, 88, 53);
        rpn(host.midi, manager, 6, 15, 11);
        if (prefixAfter) cc(host.midi, changed, 88, 53, 11);
        on(host.midi, changed, 60, 21); on(host.midi, survivor, 64, 21); host.tick(frames);
        check(!find(*host.processor, 59, changed).info.active,
              "same-block pre-MCM transferred onset is stopped in MIDI order");
        const auto current = find(*host.processor, 60, changed);
        check(current.info.held && current.info.velocity == (prefixAfter ? ((80 << 7) | 53) / 16383.0f : 80.0f * (1.0f / 127.0f)),
              "role change clears earlier CC88 and preserves a later same-offset prefix");
        const auto untouched = find(*host.processor, 64, survivor);
        check(untouched.info.held && untouched.info.velocity == ((80 << 7) | 53) / 16383.0f,
              "same-role member preserves its pending CC88 prefix");
        check(host.processor->isHostKeyDown(60) && host.processor->isHostKeyDown(64)
                  && !host.processor->isHostKeyDown(59),
              "post-transfer host note restores only its fresh held mask");
        if (sixteen)
            check(current.engine == &host.processor->getExtensionEngine(),
                  "post-transfer prefix witness really occupies the extension");
    }
}

void testChannelState(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true}) for (bool upper : {false, true})
    {
        context = std::string("channel-state/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/to-upper" : "/to-lower");
        Host host(fixture, circuit, sixteen); host.layout(7, 7);
        const int manager = upper ? 16 : 1, changed = upper ? 3 : 14, survivor = upper ? 15 : 2;
        for (int channel : {changed, survivor})
        {
            host.midi.addEvent(juce::MidiMessage::pitchWheel(channel, 10240), 0);
            host.midi.addEvent(juce::MidiMessage::channelPressureChange(channel, 89), 0);
            cc(host.midi, channel, 74, 101); cc(host.midi, channel, 64, 127);
        }
        rpn(host.midi, changed, 0, 12); host.tick();
        rpn(host.midi, manager, 6, 15); host.tick();
        // A transferred member's selected RPN0 and remembered MSB must not
        // address its newly assigned zone through either data-entry byte.
        cc(host.midi, changed, 38, 50); cc(host.midi, changed, 6, 7); host.tick();
        check(host.processor->getEngine().getMpeZones().memberRange(upper) == 48,
              "transferred channel cannot reuse its old RPN selector or data MSB");
        if (sixteen)
        {
            for (int n = 40; n < 48; ++n) on(host.midi, survivor, n);
            host.tick();
        }
        on(host.midi, changed, 60); on(host.midi, survivor, 64); host.tick(32);
        const auto reset = find(*host.processor, 60, changed), kept = find(*host.processor, 64, survivor);
        check(reset.info.held && reset.info.bend == 0 && reset.info.pressure == 0,
              "fresh transferred-channel note starts with neutral member bend and pressure");
        check(kept.info.held && kept.info.bend == 12 && kept.info.pressure == 89.0f / 127.0f,
              "same-role member preserves cached onset bend and pressure");
        if (sixteen)
            check(reset.engine == &host.processor->getExtensionEngine(),
                  "fresh neutral-expression witness really occupies the extension");
        if (reset.index >= 0 && kept.index >= 0)
            for (int rank = 0; rank < 2; ++rank)
            {
                check(reset.engine->getModernRankOffsets(reset.index, rank).pulseWidth == 0,
                      "fresh transferred-channel note starts with neutral member timbre");
                check(kept.engine->getModernRankOffsets(kept.index, rank).pulseWidth
                          == (101.0f / 127.0f) * modern::kPulseWidthRange,
                      "same-role member preserves cached onset timbre");
            }
        off(host.midi, changed, 60); off(host.midi, survivor, 64); host.tick();
        check(!find(*host.processor, 60, changed).info.pedalHeld,
              "transferred-channel pedal is up for subsequent notes");
        check(find(*host.processor, 64, survivor).info.pedalHeld,
              "same-role member pedal remains down for subsequent notes");

        Host selectors(fixture, circuit, sixteen); selectors.layout(7, 7);
        cc(selectors.midi, survivor, 101, 0); cc(selectors.midi, survivor, 100, 0);
        rpn(selectors.midi, manager, 6, 15); cc(selectors.midi, survivor, 6, 7); selectors.tick();
        check(selectors.processor->getEngine().getMpeZones().memberRange(upper) == 7,
              "same-role member retains its selected RPN across accepted MCM");
    }

    // On a whole-zone reversal both channel 1 and channel 16 stay under MPE,
    // but exchange manager/member roles. Their pedals and selectors are local.
    for (bool circuit : {false, true}) for (bool sixteen : {false, true}) for (bool upper : {false, true})
    {
        context = std::string("manager-member-state/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/to-upper" : "/to-lower");
        Host host(fixture, circuit, sixteen); host.layout(upper ? 15 : 0, upper ? 0 : 15);
        const int oldManager = upper ? 1 : 16, newManager = upper ? 16 : 1, newMember = upper ? 15 : 2;
        cc(host.midi, oldManager, 64, 127); cc(host.midi, newManager, 64, 127);
        rpn(host.midi, oldManager, 0, 12); host.tick();
        rpn(host.midi, newManager, 6, 15); host.tick();
        on(host.midi, oldManager, 60); off(host.midi, oldManager, 60);
        on(host.midi, newMember, 64); off(host.midi, newMember, 64); host.tick();
        check(!find(*host.processor, 60, oldManager).info.pedalHeld,
              "former manager's pedal does not survive its transfer to member");
        check(!find(*host.processor, 64, newMember).info.pedalHeld,
              "former member's pedal cannot become a new-zone manager pedal");
        cc(host.midi, oldManager, 6, 7); host.tick();
        check(host.processor->getEngine().getMpeZones().memberRange(upper) == 48,
              "former manager's RPN0 cannot mutate the new member range");
        cc(host.midi, newManager, 6, 3); host.tick();
        check((upper ? host.processor->getEngine().getMpeZones().upperMemberCount()
                     : host.processor->getEngine().getMpeZones().lowerMemberCount()) == 15,
              "role-changing MCM manager discards its own stale selector after acceptance");
    }
}

void testSharedPitchOwnership(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true}) for (bool upper : {false, true})
    {
        context = std::string("same-pitch/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/to-upper" : "/to-lower");
        Host host(fixture, circuit, sixteen); host.layout(7, 7);
        const int changed = upper ? 3 : 14, survivor = upper ? 15 : 2;
        on(host.midi, changed, 60); on(host.midi, survivor, 60); host.tick();
        rpn(host.midi, upper ? 16 : 1, 6, 15); host.tick();
        check(!find(*host.processor, 60, changed).info.active && find(*host.processor, 60, survivor).info.held
                  && host.processor->isHostKeyDown(60),
              "same pitch retains only the unchanged channel's host ownership bit");
        off(host.midi, survivor, 60); host.tick();
        check(!host.processor->isHostKeyDown(60), "last same-role owner release clears shared-pitch display");
    }
}
void testDeferredKeys(const juce::File& fixture)
{
    for (bool sixteen : {false, true}) for (bool upper : {false, true})
    {
        const int manager = upper ? 16 : 1, changed = upper ? 3 : 14, other = upper ? 15 : 2;
        const int after = 15;
        context = std::string("scanner/circuit") + (sixteen ? "/16" : "/8")
            + (upper ? "/upper" : "/lower");
        Host queued(fixture, true, sixteen);
        queued.layout(7, 7);
        queued.processor->setParamValue(P_kasScanTiming, 1); queued.tick();
        on(queued.midi, changed, 60); on(queued.midi, other, 64); queued.tick();
        check(!find(*queued.processor, 60, changed).info.active
                  && !find(*queued.processor, 64, other).info.active,
              "scanner witness has queued, not yet allocated, notes");
        rpn(queued.midi, manager, 6, after); queued.tick(); queued.tick(256);
        check(!find(*queued.processor, 60, changed).info.active && !queued.processor->isHostKeyDown(60),
              "MCM removes pending scanner onset before it can sound");
        check(find(*queued.processor, 64, other).info.held && queued.processor->isHostKeyDown(64),
              "MCM retains the same-role member's pending scanner onset");

        context.replace(0, 7, "waiting");
        Host waiting(fixture, true, sixteen);
        waiting.layout(7, 7);
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
        for (bool upper : {false, true})
    {
        context = std::string("arp/") + (circuit ? "circuit" : "legacy") + (sixteen ? "/16" : "/8")
            + (upper ? "/upper" : "/lower");
        Host host(fixture, circuit, sixteen);
        const int manager = upper ? 16 : 1, changed = upper ? 3 : 14, other = upper ? 15 : 2;
        const int after = 15;
        host.layout(7, 7);
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
        Host host(fixture, circuit, sixteen); host.layout(7, 7);
        host.processor->serviceKeyboardUI(); // Acknowledge startup before real UI gestures.
        if (sixteen)
        {
            for (int note = 40; note < 48; ++note) on(host.midi, 2, note);
            host.tick();
        }
        auto& keyboard = host.processor->getKeyboardState();
        const auto removed = host.processor->captureKeyboardGesture(14, 60, [&] { keyboard.noteOn(14, 60, .8f); });
        const auto kept = host.processor->captureKeyboardGesture(2, 64, [&] { keyboard.noteOn(2, 64, .8f); });
        host.tick(frames);
        check(removed && kept && find(*host.processor, 60, 14).info.held
                  && find(*host.processor, 64, 2).info.held,
              "real captured UI gestures allocate affected and unchanged held notes");
        check(!host.processor->isHostKeyDown(60) && !host.processor->isHostKeyDown(64),
              "UI ownership fixture is not substituted host MIDI");
        if (sixteen)
            check(find(*host.processor, 60, 14).engine == &host.processor->getExtensionEngine()
                      && find(*host.processor, 64, 2).engine == &host.processor->getExtensionEngine(),
                  "UI ownership witnesses both occupy extension circuits");
        rpn(host.midi, 1, 6, 15); host.tick(frames);
        check(!find(*host.processor, 60, 14).info.active && find(*host.processor, 64, 2).info.held,
              "MCM cancels affected UI note while retaining unchanged UI note");

        on(host.midi, 14, 60); host.tick(frames);
        keyboard.noteOff(14, 60, 0); keyboard.noteOff(2, 64, 0);
        host.processor->setKeyPressureFromUI(removed, .9f); host.tick(frames);
        check(find(*host.processor, 60, 14).info.held && find(*host.processor, 60, 14).info.pressure == 0
                  && host.processor->isHostKeyDown(60),
              "retired UI release and pressure cannot cancel or alter a replacement host note");
        check(!find(*host.processor, 64, 2).info.held,
              "unchanged UI key retains its release obligation after MCM invalidates pressure attribution");
        const auto fresh = host.processor->captureKeyboardGesture(14, 62, [&] { keyboard.noteOn(14, 62, .8f); });
        host.tick(frames);
        check(fresh && find(*host.processor, 62, 14).info.held,
              "role change permits a fresh UI gesture without a global panic acknowledgment");
        keyboard.noteOff(14, 62, 0); host.tick(frames);
        check(!find(*host.processor, 62, 14).info.held && find(*host.processor, 60, 14).info.held,
              "fresh UI release works without disturbing the replacement host note");
    }
}


bool sameAudio(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples()) return false;
    for (int channel = 0; channel < a.getNumChannels(); ++channel)
        for (int frame = 0; frame < a.getNumSamples(); ++frame)
            if (a.getSample(channel, frame) != b.getSample(channel, frame)) return false;
    return true;
}

void conventionalUpperOrigin(Host& host, bool sixteen)
{
    host.layout(2, 2);
    host.processor->setParamValue(P_mpeOn, 0); host.tick();
    if (sixteen)
        for (int note = 40; note < 48; ++note) on(host.midi, 15, note);
    on(host.midi, 15, 60); host.tick(32);
    // The voice captures lower/global routing at its conventional onset.
    // Enabling MPE alone does not reallocate it or change that captured route.
    host.processor->setParamValue(P_mpeOn, 1); host.tick();
    check(host.processor->getEngine().getMpeZones().role(15) == MpeZoneLayout::Role::upperMember,
          "conventional-origin witness is now on an unchanged upper-member channel");
    const auto voice = find(*host.processor, 60, 15);
    check(voice.info.held, "same-policy MPE enable retains conventional-origin held note");
    if (sixteen)
        check(voice.engine == &host.processor->getExtensionEngine(),
              "conventional-origin bus witness really occupies the extension");
}

void testSharedManagerBus(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true}) for (bool settled : {false, true})
    {
        context = std::string("shared-manager-bus/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (settled ? "/settled" : "/in-flight");
        Host actual(fixture, circuit, sixteen), reference(fixture, circuit, sixteen), neutral(fixture, circuit, sixteen);
        for (auto* host : {&actual, &reference, &neutral})
        {
            host->processor->getModernLayer().setSlot(0, {modern::ModSource::pitchBend, modern::ModSource::none,
                                                         modern::ModDestination::pulseWidth, modern::ModTarget::both});
            conventionalUpperOrigin(*host, sixteen);
        }
        for (auto* host : {&actual, &reference})
            host->midi.addEvent(juce::MidiMessage::pitchWheel(1, 12288), 0);
        for (auto* host : {&actual, &reference, &neutral}) host->tick(1);
        if (settled)
            for (int block = 0; block < 32; ++block)
                for (auto* host : {&actual, &reference, &neutral}) host->tick(256);
        const auto before = find(*actual.processor, 60, 15);
        check(before.index >= 0, "shared-bus witness is sounding immediately before MCM");
        if (before.index >= 0)
        {
            const float amount = before.engine->getModernRankOffsets(before.index, 0).pulseWidth;
            check(amount > 0 && (circuit && !settled
                      ? amount < .5f * modern::kPulseWidthRange
                      : std::abs(amount - .5f * modern::kPulseWidthRange) < 1.0e-6f),
                  "Circuit in-flight or Legacy/settled pitch-wheel matrix follows its existing policy");
        }
        rpn(actual.midi, 1, 6, 0); actual.tick(); reference.tick(); neutral.tick();
        const auto after = find(*actual.processor, 60, 15);
        check(after.engine == before.engine && after.index == before.index && after.info.held
                  && actual.processor->isHostKeyDown(60),
              "lower-zone disable preserves unchanged upper-member allocation and host ownership");
        // Compare immediately and as the independent smoothers continue. The
        // no-MCM twin retains every bus and provides the exact continuation.
        for (int frames : {1, 32, 256})
        {
            actual.tick(frames); reference.tick(frames); neutral.tick(frames);
            check(sameAudio(actual.audio, reference.audio),
                  "role-scoped MCM preserves shared manager targets and smoothing continuation exactly");
            const auto a = find(*actual.processor, 60, 15), b = find(*reference.processor, 60, 15);
            if (a.index >= 0 && b.index >= 0)
                for (int rank = 0; rank < 2; ++rank)
                {
                    check(a.engine->getRankInfo(a.index, rank).oscillatorHz
                              == b.engine->getRankInfo(b.index, rank).oscillatorHz,
                          "unchanged conventional-origin member retains actual manager-bent oscillator pitch");
                    check(a.engine->getModernRankOffsets(a.index, rank).pulseWidth
                              == b.engine->getModernRankOffsets(b.index, rank).pulseWidth,
                          "independent shared pitch-wheel matrix bus survives lower-manager departure");
                }
        }
        for (int block = 0; block < 32; ++block)
            for (auto* host : {&actual, &reference, &neutral}) host->tick(256);
        const auto a = find(*actual.processor, 60, 15), n = find(*neutral.processor, 60, 15);
        if (a.index >= 0 && n.index >= 0)
            for (int rank = 0; rank < 2; ++rank)
            {
                const float semitones = 12 * std::log2(a.engine->getRankInfo(a.index, rank).oscillatorHz
                                                      / n.engine->getRankInfo(n.index, rank).oscillatorHz);
                check(std::isfinite(semitones) && std::abs(semitones - 1.0f) < .001f,
                      "settled surviving pitch remains one semitone above an independent neutral-wheel twin");
                check(std::abs(a.engine->getModernRankOffsets(a.index, rank).pulseWidth
                               - .5f * modern::kPulseWidthRange) < 1.0e-6f,
                      "settled pitch-wheel matrix bus independently retains the half-scale wheel");
            }
    }
}

void testOtherSharedBuses(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
        for (auto source : {modern::ModSource::modWheel, modern::ModSource::expression,
                            modern::ModSource::channelPressure, modern::ModSource::ribbon})
    {
        context = std::string("other-shared-buses/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16/" : "/8/") + std::to_string(static_cast<int>(source));
        Host actual(fixture, circuit, sixteen), reference(fixture, circuit, sixteen);
        for (auto* host : {&actual, &reference})
        {
            host->processor->getModernLayer().setSlot(0, {source, modern::ModSource::none,
                                                         modern::ModDestination::pulseWidth, modern::ModTarget::both});
            host->processor->setParamValue(P_ribbonCcEnable, 1);
            host->processor->setParamValue(P_ribbonCc, 33);
            conventionalUpperOrigin(*host, sixteen);
            cc(host->midi, 1, 1, 91); cc(host->midi, 1, 11, 42); cc(host->midi, 1, 33, 100);
            host->midi.addEvent(juce::MidiMessage::channelPressureChange(1, 87), 0);
            host->tick(1);
        }
        const auto witness = find(*actual.processor, 60, 15);
        check(witness.index >= 0 && witness.engine->getModernRankOffsets(witness.index, 0).pulseWidth > 0,
              "shared controller matrix route has a nonzero pre-MCM witness");
        rpn(actual.midi, 1, 6, 0); actual.tick(); reference.tick();
        for (int frames : {1, 32, 256})
        {
            actual.tick(frames); reference.tick(frames);
            check(sameAudio(actual.audio, reference.audio),
                  "scoped role reset leaves shared expression, modulation, pressure and ribbon smoothing unchanged");
            const auto a = find(*actual.processor, 60, 15), b = find(*reference.processor, 60, 15);
            check(a.info.held && b.info.held, "shared-bus same-role voice remains held in both twins");
            if (a.index >= 0 && b.index >= 0)
                for (int rank = 0; rank < 2; ++rank)
                    check(a.engine->getModernRankOffsets(a.index, rank).pulseWidth
                              == b.engine->getModernRankOffsets(b.index, rank).pulseWidth,
                          "shared controller matrix signal continues exactly after unrelated role changes");
        }
    }
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-mpe-role-transitions");
    testSoundingRoleChanges(fixture.getFile());
    testPrefixAndOrdering(fixture.getFile());
    testChannelState(fixture.getFile());
    testSharedPitchOwnership(fixture.getFile());
    testDeferredKeys(fixture.getFile());
    testUiReleaseOwnership(fixture.getFile());
    testSharedManagerBus(fixture.getFile());
    testOtherSharedBuses(fixture.getFile());
    std::cout << "MPE role transitions: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
