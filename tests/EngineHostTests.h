#pragma once
// ws2-engine host contracts: oversampling quality, latency and state. Invoked by
// the UI self-test after JUCE initialization; each fixture owns its processor.

#include "plugin/PluginProcessor.h"

#include <array>
#include <cmath>

namespace cs80::tests
{
template <typename Check>
void testOversamplingHost(Check&& check)
{
    {
        Cs80AudioProcessor host;
        auto* quality = dynamic_cast<juce::AudioParameterChoice*>(
            host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(P_oversampling)));
        // Host 245 at this branch; workstreams integrated before ws2 may append ahead of it.
        check(quality != nullptr && quality->getVersionHint() == 9 && quality->choices.size() == 3
                  && quality->getIndex() == 1 && host.getParameters().indexOf(quality) >= 245,
              "oversampling quality is an appended choice, AU hint 9, default Standard");
        check(host.getParameters()[244] == host.getAPVTS().getParameter("source_performance"),
              "shipped host parameters keep their indexes before the new quality choice");
    }

    // Latency is re-reported after every mode change and equals the engine's.
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (float mode : { 0.0f, 1.0f, 2.0f })
            for (float circuit : { 0.0f, 1.0f })
            {
                Cs80AudioProcessor host;
                host.setParamValue(P_sourcePerformance, circuit);
                host.setParamValue(P_parityMode, 0.0f);
                host.prepareToPlay(rate, 64);
                juce::AudioBuffer<float> block(2, 64);
                juce::MidiBuffer midi;
                host.processBlock(block, midi);
                host.setParamValue(P_oversampling, mode);
                host.processBlock(block, midi);
                const int expected = oversamplingFactor(oversamplingMode(mode), circuit >= 0.5f, false, rate);
                check(host.getEngine().getOversamplingFactor() == expected,
                      "host quality choice reaches the engine factor rule");
                check(host.getLatencySamples() == host.getEngine().getLatencySamples()
                          && host.getLatencySamples() == 63,
                      "reported host latency equals the engine's in every mode");
            }

    // Offline bounce: the circuit engine renders at HQ, legacy keeps its factor.
    for (float circuit : { 0.0f, 1.0f })
    {
        Cs80AudioProcessor host;
        host.setParamValue(P_sourcePerformance, circuit);
        host.setParamValue(P_parityMode, 0.0f);
        host.setParamValue(P_oversampling, 0.0f);
        host.setNonRealtime(true);
        host.prepareToPlay(48000.0, 64);
        juce::AudioBuffer<float> block(2, 64);
        juce::MidiBuffer midi;
        host.processBlock(block, midi);
        check(host.getEngine().getOversamplingFactor() == (circuit >= 0.5f ? 8 : 2),
              "offline prepare: circuit engine HQ, legacy unchanged");
        host.setNonRealtime(false);
        host.processBlock(block, midi);
        check(host.getEngine().getOversamplingFactor() == 2 && host.getLatencySamples() == 63,
              "realtime again without a new prepare restores the selected mode");
        host.setNonRealtime(true);
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
        host.processBlock(block, midi);
        check(host.getEngine().getOversamplingFactor() == (circuit >= 0.5f ? 8 : 2)
                  && host.getEngine().activeVoices() == 1,
              "offline switch inside processBlock precedes the block's MIDI");
    }

    // A session setting: state round trip, but presets neither set nor clear it.
    {
        Cs80AudioProcessor host;
        host.setParamValue(P_oversampling, 2.0f);
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        host.setParamValue(P_oversampling, 0.0f);
        host.setStateInformation(saved.getData(), (int) saved.getSize());
        check(host.getParamValue(P_oversampling) == 2.0f, "session state restores the quality mode");
        host.loadFactoryPreset(5);
        check(host.getParamValue(P_oversampling) == 2.0f, "factory preset recall keeps the quality mode");
        auto userState = host.getAPVTS().copyState();
        userState.getChildWithProperty("id", Cs80AudioProcessor::parameterId(P_oversampling))
            .setProperty("value", 0.0f, nullptr);
        juce::TemporaryFile preset(".cs80xml");
        check(userState.createXml()->writeTo(preset.getFile()), "quality temporary user preset written");
        host.loadUserPreset(preset.getFile());
        check(host.getParamValue(P_oversampling) == 2.0f, "user preset recall keeps the live quality mode");
        auto old = host.getAPVTS().copyState();
        old.removeChild(old.getChildWithProperty("id", Cs80AudioProcessor::parameterId(P_oversampling)), nullptr);
        const auto text = old.createXml()->toString();
        host.setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
        check(host.getParamValue(P_oversampling) == 1.0f, "a state without the quality mode restores Standard");
    }
}
} // namespace cs80::tests
