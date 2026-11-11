#pragma once

#include "plugin/PluginProcessor.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <memory>

namespace cs80::tests
{
// Real processBlock MIDI order, no editor/window/external controller or DAW.
template <class Check>
void testMpeMemberOnsetHost(Check check)
{
    juce::TemporaryFile fixture("-cs80-mpe-member-onset");
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
        host->setParamValue(P_pressureCurve, 0);
        host->setParamValue(P_pressureThreshold, 0);
        host->setParamValue(P_mpeBendRange, 48);
        host->setParamValue(P_mod1Amount, 1);
        host->getModernLayer().setSlot(0, {modern::ModSource::timbre, modern::ModSource::none,
                                          modern::ModDestination::pulseWidth, modern::ModTarget::both});
        host->prepareToPlay(48000.0, 64);
        return host;
    };
    auto expression = [](juce::MidiBuffer& midi, int channel, int frame) {
        midi.addEvent(juce::MidiMessage::pitchWheel(channel, 10240), frame);
        midi.addEvent(juce::MidiMessage::channelPressureChange(channel, 89), frame);
        midi.addEvent(juce::MidiMessage::controllerEvent(channel, 74, 101), frame);
    };
    auto zone = [](juce::MidiBuffer& midi, int manager, int members) {
        midi.addEvent(juce::MidiMessage::controllerEvent(manager, 101, 0), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(manager, 100, 6), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(manager, 6, members), 0);
    };
    auto sameAudio = [](const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b) {
        if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples()) return false;
        for (int c = 0; c < a.getNumChannels(); ++c)
            for (int i = 0; i < a.getNumSamples(); ++i)
                if (!std::isfinite(a.getSample(c, i)) || !std::isfinite(b.getSample(c, i))
                    || std::bit_cast<std::uint32_t>(a.getSample(c, i)) != std::bit_cast<std::uint32_t>(b.getSample(c, i))) return false;
        return true;
    };
    auto voice = [](const Engine& e, int note, int channel) {
        for (int i = 0; i < 8; ++i)
            if (const auto v = e.getVoiceInfo(i); v.active && v.held && v.note == note && v.channel == channel) return i;
        return -1;
    };
    for (bool circuit : {false, true})
        for (bool sixteen : {false, true})
            for (int frames : {0, 32})
                for (int timing : {0, 1, 2}) // Same offset, separate offsets, preceding callback.
                    // Zero-frame callbacks clamp both offsets to the same boundary.
                    for (int member : {2, 15, 1}) // Lower, upper, full-upper channel 1.
    {
        auto host = make(circuit, sixteen), reference = make(circuit, sixteen);
        juce::AudioBuffer<float> a(2, frames), b(2, frames);
        juce::MidiBuffer midi, other;
        auto tick = [&] {
            host->processBlock(a, midi); reference->processBlock(b, other);
            midi.clear(); other.clear();
            check(sameAudio(a, b), "host initial expression matches ordinary post-allocation replay by finite audio bits");
        };
        tick(); // Extension is active before negotiation/expression.
        for (auto* m : {&midi, &other})
        {
            zone(*m, 1, 0); zone(*m, 16, 0);
            if (member == 1) zone(*m, 16, 15);
            else { zone(*m, 1, 2); zone(*m, 16, 2); }
        }
        tick();
        if (sixteen)
        {
            for (auto* m : {&midi, &other})
                for (int note = 40; note < 48; ++note)
                    m->addEvent(juce::MidiMessage::noteOn(5, note, (juce::uint8)100), 0);
            tick();
            check(!host->getEngine().hasFreeCircuit(), "eight primary notes force a real extension allocation");
        }
        expression(midi, member, timing == 1 ? 7 : 0);
        if (timing == 2) tick();
        const int onset = timing == 1 ? 21 : 0;
        midi.addEvent(juce::MidiMessage::noteOn(member, 60, (juce::uint8)100), onset);
        other.addEvent(juce::MidiMessage::noteOn(member, 60, (juce::uint8)100), onset);
        expression(other, member, onset);
        tick();
        const auto& target = sixteen ? host->getExtensionEngine() : host->getEngine();
        const int index = voice(target, 60, member);
        check(index >= 0, "pre-note expression note is held by its intended active instrument");
        if (index >= 0)
        {
            check(target.getVoiceInfo(index).bend == 12.0f, "host member bend is present immediately at onset");
            check(target.getVoiceInfo(index).pressure == 89.0f / 127.0f, "host member pressure is present immediately at onset");
        }
        a.setSize(2, 64); b.setSize(2, 64);
        bool sounded = false;
        for (int i = 0; i < 3; ++i)
        {
            tick();
            for (int channel = 0; channel < 2; ++channel)
                for (int frame = 0; frame < a.getNumSamples(); ++frame)
                    sounded |= a.getSample(channel, frame) != 0.0f;
        }
        check(sounded, "host onset audio equality includes a non-silent continuation");
        if (index >= 0)
            for (int rank = 0; rank < 2; ++rank)
                check(target.getModernRankOffsets(index, rank).pulseWidth == (101.0f / 127.0f) * modern::kPulseWidthRange,
                      "host pre-note CC74 reaches both ranks' existing matrix input");
    }
    for (bool circuit : {false, true})
    {
        auto host = make(circuit, false);
        juce::AudioBuffer<float> audio(2, 32); juce::MidiBuffer midi;
        auto tick = [&] { host->processBlock(audio, midi); midi.clear(); };
        tick(); expression(midi, 2, 0); tick();
        host->setParamValue(P_voiceMode, 1); tick();
        for (int note = 40; note < 48; ++note)
            midi.addEvent(juce::MidiMessage::noteOn(5, note, (juce::uint8)100), 0);
        midi.addEvent(juce::MidiMessage::noteOn(2, 60, (juce::uint8)100), 0); tick();
        const auto& extension = host->getExtensionEngine();
        const int index = voice(extension, 60, 2);
        check(index >= 0, "activation-boundary witness actually uses the new extension");
        if (index >= 0)
        {
            check(extension.getVoiceInfo(index).bend == 12.0f && extension.getVoiceInfo(index).pressure == 89.0f / 127.0f,
                  "new extension inherits the primary controller state at activation");
            for (int rank = 0; rank < 2; ++rank)
                check(extension.getModernRankOffsets(index, rank).pulseWidth == (101.0f / 127.0f) * modern::kPulseWidthRange,
                      "new extension inherits current retained CC74 at activation");
        }
        // Existing MIDI-learn consumption still owns learned CC74 events.
        host = make(circuit, false); tick();
        host->getMidiLearn().assign(74, host->hostParameterIndex(P_volume));
        expression(midi, 2, 0);
        midi.addEvent(juce::MidiMessage::noteOn(2, 60, (juce::uint8)100), 0); tick();
        check(std::abs(host->getParamValue(P_volume) - 101.0f / 127.0f) < 1.0e-6f,
              "learned CC74 actually updates its authorized volume parameter");
        const int learned = voice(host->getEngine(), 60, 2);
        check(learned >= 0, "learned-CC witness owns a note");
        if (learned >= 0)
            for (int rank = 0; rank < 2; ++rank)
                check(host->getEngine().getModernRankOffsets(learned, rank).pulseWidth == 0,
                      "learned CC74 remains consumed rather than populating member timbre");
    }
}
} // namespace cs80::tests
