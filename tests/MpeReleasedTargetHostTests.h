#pragma once

#include "plugin/PluginProcessor.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace cs80::tests
{
// Actual processBlock dispatch. No editor, native peer or external DAW.
// The event boundary is accepted voice release; scanner/ARP gates are covered
// by the core fixture rather than approximated here with MIDI receipt time.
template <class Check>
void testMpeReleasedTargetHost(Check check)
{
    juce::TemporaryFile fixture("-cs80-mpe-released-targets");
    auto same = [](float a, float b) {
        return std::isfinite(a) && std::isfinite(b)
            && std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
    };
    auto sameAudio = [&](const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b) {
        if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples()) return false;
        for (int channel = 0; channel < a.getNumChannels(); ++channel)
            for (int frame = 0; frame < a.getNumSamples(); ++frame)
                if (!same(a.getSample(channel, frame), b.getSample(channel, frame))) return false;
        return true;
    };
    auto make = [&](bool circuit, bool sixteen) {
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        host->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        host->setParamValue(P_parityMode, 0);
        host->setParamValue(P_voiceMode, sixteen ? 1.0f : 0.0f);
        host->setParamValue(P_mpeOn, 1);
        host->setParamValue(P_kasScanTiming, 0);
        host->setParamValue(P_arpOn, 0);
        host->setParamValue(P_voiceDispersion, 0);
        host->setParamValue(P_drift, 0);
        host->setParamValue(P_analogNoise, 0);
        host->setParamValue(P_ribbonCcEnable, 0); // Exclude the separate global ribbon route.
        host->setParamValue(P_pressureCurve, 0);
        host->setParamValue(P_pressureThreshold, 0);
        host->setParamValue(P_mpeBendRange, 48);
        host->setParamValue(P_keyRelease, 2);
        host->setParamValue(P_mod1Amount, 1);
        host->getModernLayer().setSlot(0, {modern::ModSource::timbre, modern::ModSource::none,
                                          modern::ModDestination::pulseWidth, modern::ModTarget::both});
        host->prepareToPlay(48000.0, 64);
        return host;
    };
    auto expression = [](juce::MidiBuffer& midi, int channel, int frame, bool later) {
        midi.addEvent(juce::MidiMessage::pitchWheel(channel, later ? 6144 : 10240), frame);
        midi.addEvent(juce::MidiMessage::channelPressureChange(channel, later ? 23 : 89), frame);
        midi.addEvent(juce::MidiMessage::controllerEvent(channel, 74, later ? 17 : 101), frame);
    };
    auto zone = [](juce::MidiBuffer& midi, int manager, int members) {
        midi.addEvent(juce::MidiMessage::controllerEvent(manager, 101, 0), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(manager, 100, 6), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(manager, 6, members), 0);
    };
    auto layout = [&](juce::MidiBuffer& midi) {
        zone(midi, 1, 0); zone(midi, 16, 0); zone(midi, 1, 2); zone(midi, 16, 2);
    };
    auto find = [](const Engine& engine, int note, int channel, bool held) {
        for (int i = 0; i < 8; ++i)
        {
            const auto voice = engine.getVoiceInfo(i);
            if (voice.active && voice.held == held && voice.note == note && voice.channel == channel) return i;
        }
        return -1;
    };
    int scenario = 0;
    for (bool circuit : {false, true})
        for (bool sixteen : {false, true})
            for (int frames : {0, 32})
                for (int member : {2, 15})
                    for (bool pedal : {false, true})
                        for (bool zeroVelocity : {false, true})
                            for (bool separated : {false, true})
    {
        std::printf("MPE released host scenario %d: circuit=%d sixteen=%d frames=%d member=%d pedal=%d zero=%d separated=%d\n",
                    ++scenario, int(circuit), int(sixteen), frames, member, int(pedal), int(zeroVelocity), int(separated));
        auto host = make(circuit, sixteen), reference = make(circuit, sixteen);
        juce::AudioBuffer<float> a(2, frames), b(2, frames);
        juce::MidiBuffer midi, other;
        auto tick = [&](int length, bool compare = true) {
            a.setSize(2, length); b.setSize(2, length); a.clear(); b.clear();
            host->processBlock(a, midi); reference->processBlock(b, other);
            midi.clear(); other.clear();
            bool finite = true, sounded = false;
            for (int channel = 0; channel < 2; ++channel)
                for (int frame = 0; frame < length; ++frame)
                {
                    finite &= std::isfinite(a.getSample(channel, frame)) && std::isfinite(b.getSample(channel, frame));
                    sounded |= a.getSample(channel, frame) != 0.0f;
                }
            check(finite, "released-target processor audio is finite");
            if (compare) check(sameAudio(a, b), "released member targets preserve finite continuation audio bits against no-later-event reference");
            return sounded;
        };
        tick(frames); // Activate both engines before their zone negotiation.
        layout(midi); layout(other); tick(frames);
        auto& target = sixteen ? host->getExtensionEngine() : host->getEngine();
        check(target.getMpeZones().role(member) == (member == 2 ? MpeZoneLayout::Role::lowerMember : MpeZoneLayout::Role::upperMember),
              "native recipient has the negotiated member role");
        if (sixteen)
        {
            for (auto* events : {&midi, &other})
                for (int note = 40; note < 48; ++note)
                    events->addEvent(juce::MidiMessage::noteOn(5, note, (juce::uint8)100), 0);
            tick(frames);
            check(!host->getEngine().hasFreeCircuit(), "eight held primary notes force real extension allocation");
        }
        for (auto* events : {&midi, &other})
        {
            expression(*events, member, 0, false);
            events->addEvent(juce::MidiMessage::noteOn(member, 60, (juce::uint8)100), 0);
        }
        tick(frames);
        bool initialSounded = false;
        for (int i = 0; i < 4; ++i) initialSounded |= tick(64);
        check(initialSounded, "native initial comparison includes non-silent combined processor output");
        const int index = find(target, 60, member, true);
        check(index >= 0, "initial target belongs to the intended active engine and member");
        if (index < 0) continue;
        const auto initial = target.getVoiceInfo(index);
        check(same(initial.bend, 12.0f) && same(initial.pressure, 89.0f / 127.0f),
              "native initial bend and pressure targets are nonzero and exact");
        for (int rank = 0; rank < 2; ++rank)
            check(same(target.getModernRankOffsets(index, rank).pulseWidth, (101.0f / 127.0f) * modern::kPulseWidthRange),
                  "native initial member timbre reaches both ranks");
        const int releaseFrame = separated ? 7 : 0, laterFrame = separated ? 21 : 0;
        for (auto* events : {&midi, &other})
        {
            if (pedal) events->addEvent(juce::MidiMessage::controllerEvent(member, 64, 127), 0);
            events->addEvent(zeroVelocity ? juce::MidiMessage::noteOn(member, 60, (juce::uint8)0)
                                         : juce::MidiMessage::noteOff(member, 60), releaseFrame);
        }
        expression(midi, member, laterFrame, true); // Later insertion also matters at one shared offset.
        // At zero frames both offsets clamp to the same boundary. Only the
        // 32-frame separated case demonstrates elapsed time between them.
        tick(frames);
        bool tailSounded = false;
        for (int i = 0; i < 4; ++i) tailSounded |= tick(64);
        check(tailSounded, "native release comparison includes non-silent combined processor output");
        const auto tail = target.getVoiceInfo(index);
        check(tail.active && !tail.held && tail.note == 60 && tail.channel == member && tail.pedalHeld == pedal,
              "native retained-target witness is the original active released or pedal-held voice");
        check(same(tail.bend, initial.bend), "native released member bend target is retained");
        check(same(tail.pressure, initial.pressure), "native released member pressure target is retained");
        for (int rank = 0; rank < 2; ++rank)
            check(same(target.getModernRankOffsets(index, rank).pulseWidth, (101.0f / 127.0f) * modern::kPulseWidthRange),
                  "native released member timbre target is retained in both ranks");
        // Cache retention remains independent of whether a current tail was
        // eligible. A subsequent accepted onset sees the later values.
        midi.addEvent(juce::MidiMessage::noteOn(member, 62, (juce::uint8)100), 0);
        tick(frames, false);
        const int next = find(target, 62, member, true);
        check(next >= 0, "subsequent onset is accepted by the intended engine");
        if (next >= 0)
        {
            const auto fresh = target.getVoiceInfo(next);
            check(same(fresh.bend, -12.0f) && same(fresh.pressure, 23.0f / 127.0f),
                  "native later onset consumes retained raw bend and pressure");
            tick(64, false);
            for (int rank = 0; rank < 2; ++rank)
                check(same(target.getModernRankOffsets(next, rank).pulseWidth, (17.0f / 127.0f) * modern::kPulseWidthRange),
                      "native later onset consumes retained raw timbre");
        }
        // CC121 is deliberately an explicit reset exception, not another
        // ordinary member-expression message. Protect released nonzero state.
        midi.addEvent(juce::MidiMessage::controllerEvent(member, 121, 0), 0);
        tick(frames, false);
        const auto reset = target.getVoiceInfo(index);
        check(reset.active && !reset.held && reset.note == 60 && reset.channel == member,
              "reset exception still observes the original released voice");
        check(same(reset.bend, 0.0f) && same(reset.pressure, 0.0f) && !reset.pedalHeld,
              "native CC121 resets released bend/pressure and member pedal state");
    }
    // Same-offset order before versus after release is not interchangeable.
    for (bool circuit : {false, true}) for (int frames : {0, 32}) for (int member : {2, 15})
    {
        auto host = make(circuit, false);
        juce::MidiBuffer midi; juce::AudioBuffer<float> audio(2, frames);
        auto tick = [&](int length) { audio.setSize(2, length); audio.clear(); host->processBlock(audio, midi); midi.clear(); };
        tick(0); layout(midi); tick(0); expression(midi, member, 0, false);
        midi.addEvent(juce::MidiMessage::noteOn(member, 60, (juce::uint8)100), 0); tick(64);
        const int index = find(host->getEngine(), 60, member, true);
        check(index >= 0, "before-release ordering fixture owns an active member voice");
        expression(midi, member, 0, true);
        midi.addEvent(juce::MidiMessage::noteOff(member, 60), 0); tick(frames);
        // This final-target assertion is deliberately after 128 ms. Circuit's
        // 5 ms controller smoother remains moving at the former 256-frame point.
        // The main paired cases above test the shorter unchanged trajectory.
        tick(6144);
        if (index >= 0)
        {
            const auto tail = host->getEngine().getVoiceInfo(index);
            check(tail.active && !tail.held && same(tail.bend, -12.0f) && same(tail.pressure, 23.0f / 127.0f),
                  "member events inserted before same-offset release remain effective");
            for (int rank = 0; rank < 2; ++rank)
                check(same(host->getEngine().getModernRankOffsets(index, rank).pulseWidth, (17.0f / 127.0f) * modern::kPulseWidthRange),
                      "before-release timbre target continues smoothing after accepted release");
        }
    }
    // The previous repeated-note ownership repair must compose with this one:
    // one member channel can have a primary held note and a released extension.
    for (bool circuit : {false, true}) for (int frames : {0, 32}) for (int member : {2, 15})
    {
        auto host = make(circuit, true);
        juce::MidiBuffer midi; juce::AudioBuffer<float> audio(2, frames);
        auto tick = [&](int length) { audio.setSize(2, length); audio.clear(); host->processBlock(audio, midi); midi.clear(); };
        tick(0); layout(midi); tick(0); expression(midi, member, 0, false);
        midi.addEvent(juce::MidiMessage::noteOn(member, 60, (juce::uint8)100), 0);
        for (int note = 41; note < 48; ++note)
            midi.addEvent(juce::MidiMessage::noteOn(5, note, (juce::uint8)100), 0);
        tick(frames);
        midi.addEvent(juce::MidiMessage::noteOn(member, 60, (juce::uint8)100), 0); tick(256);
        const int primary = find(host->getEngine(), 60, member, true);
        const int extension = find(host->getExtensionEngine(), 60, member, true);
        check(primary >= 0 && extension >= 0, "composition fixture owns same-member notes in both active instruments");
        midi.addEvent(juce::MidiMessage::noteOff(member, 60), 0); expression(midi, member, 0, true); tick(frames);
        tick(6144); // Allow the changed held-primary timbre target to settle.
        if (primary >= 0 && extension >= 0)
        {
            const auto held = host->getEngine().getVoiceInfo(primary);
            const auto tail = host->getExtensionEngine().getVoiceInfo(extension);
            check(held.active && held.held && tail.active && !tail.held,
                  "one repeated-note release leaves primary held and extension released");
            check(same(held.bend, -12.0f) && same(held.pressure, 23.0f / 127.0f),
                  "later member expression reaches the still-held primary onset");
            check(same(tail.bend, 12.0f) && same(tail.pressure, 89.0f / 127.0f),
                  "later member expression does not retarget the released extension onset");
            for (int rank = 0; rank < 2; ++rank)
            {
                check(same(host->getEngine().getModernRankOffsets(primary, rank).pulseWidth, (17.0f / 127.0f) * modern::kPulseWidthRange),
                      "held primary timbre updates despite a same-channel extension tail");
                check(same(host->getExtensionEngine().getModernRankOffsets(extension, rank).pulseWidth, (101.0f / 127.0f) * modern::kPulseWidthRange),
                      "released extension timbre remains independent of the held primary");
            }
        }
    }
}
} // namespace cs80::tests
