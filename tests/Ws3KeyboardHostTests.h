#pragma once

// ws3-keyboard processor-side checks (Phase 6 + ribbon CC + CC88), run by the
// UI smoke executable after JUCE initialization: appended host parameters,
// Scala/KBM publication and session round trip, MTS SysEx and CC88 through
// processBlock, sixteen-voice routing, ribbon CC, setup parameters surviving
// preset recall. Software contract checks only.

#include "plugin/PluginProcessor.h"

#include <cmath>
#include <vector>

namespace cs80::tests
{
namespace ws3_detail
{
inline void process(Cs80AudioProcessor& host, juce::MidiBuffer& midi, int frames = 256)
{
    juce::AudioBuffer<float> buffer(2, frames);
    buffer.clear();
    host.processBlock(buffer, midi);
    midi.clear();
}
inline float semitones(float hz, float note) { return 12.0f * std::log2(hz / midiToHz(note)); }
} // namespace ws3_detail

template <typename Check>
void testWs3KeyboardHost(Check&& check)
{
    using namespace ws3_detail;
    {
        Cs80AudioProcessor host;
        int previous = -1;
        bool ordered = true, hinted = true, neutral = true;
        for (int index = P_kasScanTiming; index <= P_unisonDetune; ++index)
        {
            auto* p = host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(index));
            const int position = p != nullptr ? host.getParameters().indexOf(p) : -1;
            ordered = ordered && position > previous;
            previous = position;
            hinted = hinted && p != nullptr && p->getVersionHint() == 10;
            neutral = neutral && host.getParamValue(index) == paramDef(index).def;
        }
        check(ordered && hinted && neutral, "ws3 host parameters appended in enum order, AU hint 10, neutral defaults");
        check(host.getParameters().indexOf(host.getAPVTS().getParameter("source_performance"))
                  < host.getParameters().indexOf(host.getAPVTS().getParameter("kas_scan_timing")),
              "ws3 host parameters follow every shipped parameter");
    }

    const juce::String just = "! just\n5-limit just\n12\n16/15\n9/8\n6/5\n5/4\n4/3\n45/32\n3/2\n8/5\n5/3\n9/5\n15/8\n2/1\n";
    {
        Cs80AudioProcessor host;
        host.prepareToPlay(48000.0, 512);
        check(host.loadScalaTuning(just).isEmpty() && host.getScalaDescription() == "5-limit just",
              "Scala text loads and reports its description");
        check(host.loadScalaTuning("nonsense").isNotEmpty() && host.getScalaDescription() == "5-limit just",
              "a malformed scale is rejected and the previous tuning kept");
        host.setParamValue(P_tuningMode, 1.0f);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
        process(host, midi);
        check(std::abs(semitones(host.getEngine().getRankInfo(0, 0).oscillatorHz, 60.0f + 12.0f * std::log2(1.25f))) < 0.05f,
              "Scala tuning reaches the audio thread");
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        Cs80AudioProcessor restored;
        restored.setStateInformation(saved.getData(), (int) saved.getSize());
        check(restored.getScalaDescription() == "5-limit just", "the session keeps its Scala tuning");
        restored.prepareToPlay(48000.0, 512);
        midi.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8) 100), 0);
        process(restored, midi);
        check(std::abs(semitones(restored.getEngine().getRankInfo(0, 0).oscillatorHz, 60.0f + 12.0f * std::log2(1.5f))) < 0.05f,
              "restored session plays its Scala tuning");
        // Setup parameters survive factory-preset recall.
        host.setParamValue(P_velocityCurve, 0.5f);
        host.loadFactoryPreset(3);
        check(host.getParamValue(P_velocityCurve) == 0.5f && host.getParamValue(P_tuningMode) == 1.0f,
              "controller curve and tuning source survive preset recall");
    }
    {
        // MTS SysEx and CC88 through processBlock.
        Cs80AudioProcessor host;
        host.prepareToPlay(48000.0, 512);
        host.setParamValue(P_tuningMode, 2.0f);
        juce::MidiBuffer midi;
        const juce::uint8 sysex[] { 0xF0, 0x7F, 0x7F, 0x08, 0x02, 0x00, 0x01, 69, 69, 0x40, 0x00, 0xF7 };
        midi.addEvent(sysex, (int) sizeof(sysex), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 88, 64), 1);
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8) 100), 2);
        process(host, midi);
        check(std::abs(semitones(host.getEngine().getRankInfo(0, 0).oscillatorHz, 69.5f)) < 0.05f,
              "MTS SysEx in the host MIDI stream retunes the note");
        check(std::abs(host.getEngine().getVoiceInfo(0).velocity - (100.0f * 128.0f + 64.0f) / 16383.0f) < 1e-6f,
              "CC88 refines the next note-on velocity");
        midi.addEvent(juce::MidiMessage::noteOn(1, 70, (juce::uint8) 100), 0);
        process(host, midi);
        check(host.getEngine().getVoiceInfo(1).velocity == 100.0f * (1.0f / 127.0f), "no prefix: 7-bit velocity unchanged");
    }
    {
        // Sixteen voices: the second instrument takes the ninth key.
        Cs80AudioProcessor host;
        host.prepareToPlay(48000.0, 512);
        host.setParamValue(P_voiceMode, 1.0f);
        juce::MidiBuffer midi;
        process(host, midi);
        for (int k = 0; k < 16; ++k) midi.addEvent(juce::MidiMessage::noteOn(1, 40 + k, (juce::uint8) 90), k);
        process(host, midi);
        check(host.getEngine().activeVoices() == 8 && host.getExtensionEngine().activeVoices() == 8,
              "sixteen-voice mode plays sixteen held keys");
        host.setParamValue(P_voiceMode, 0.0f);
        process(host, midi);
        check(host.getExtensionEngine().activeVoices() == 0, "leaving sixteen-voice mode stops the second instrument");
    }
    {
        // Ribbon CC from the host.
        Cs80AudioProcessor host;
        host.prepareToPlay(48000.0, 512);
        host.setParamValue(P_ribbonCcEnable, 1.0f);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 90), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 16, 127), 1);
        process(host, midi);
        check(host.getEngine().getVoiceInfo(0).ribbonRatio == ribbonFrequencyRatio(1.0f), "ribbon CC 16 from the host");
    }
}
} // namespace cs80::tests
