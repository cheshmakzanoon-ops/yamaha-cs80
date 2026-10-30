#pragma once

#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace cs80::tests
{
// Existing GUI/host smoke calls this once after JUCE initialization. The core
// oracle has no telemetry/editor. Compare actual mapped host audio bit-for-bit,
// and inspect only produced output channels (unused channels contain NaNs).
template <typename Check>
void testOutputMeterHost(Check&& check)
{
    juce::ScopedNoDenormals noDenormals; // Match the processor's audio FP policy.
    bool observedAboveFullScale = false;
    for (int policy = 0; policy < 3; ++policy) for (int channels : {1, 2})
    {
        Cs80AudioProcessor host;
        ParamValues p;
        p[P_sourcePerformance] = policy == 0 ? 0.0f : 1.0f;
        p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
        p[P_drift] = p[P_voiceDispersion] = p[P_analogNoise] = p[P_chorusOn] = 0;
        p[P_volume] = p[P_mpeOn] = 1;
        for (int rank = 0; rank < 2; ++rank)
        {
            for (int local : {CP_saw, CP_pulse, CP_sine, CP_vS, CP_vLevel,
                              CP_vcfLevel, CP_initLevel, CP_afterLevel})
                p[chParam(rank, local)] = 1;
            p[chParam(rank, CP_vA)] = .001f;
            p[chParam(rank, CP_vD)] = .01f;
            p[chParam(rank, CP_vR)] = .2f;
        }
        host.applyParameterValues(p);
        host.setParamValue(P_parityMode, p[P_parityMode]);
        ControlFrame frame {p, policy == 1 ? std::array<int, 2>{1, 3}
                                           : std::array<int, 2>{13, 13}};
        for (int rank = 0; rank < 2; ++rank) host.selectHardwareSource(rank, frame.hardwareSources[rank]);
        // APVTS normalise/denormalise may round physical float values. Use the
        // actual stored controls, not pre-roundtrip literals, for the DSP oracle.
        frame.effectiveValues = host.currentValues();
        auto layout = host.getBusesLayout();
        layout.outputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono()
                                               : juce::AudioChannelSet::stereo());
        check(host.setBusesLayout(layout), "meter fixture accepts mono/stereo host layout");
        // Match the processor's pre-prepare state too: legacy prepare preserves
        // its existing volume-smoothing start before applying pending APVTS.
        const auto initialEngineValues = host.getEngine().getParams();
        host.prepareToPlay(48000, 32); // Exercise blocks much larger than advertised.
        Engine oracle;
        oracle.setParams(initialEngineValues);
        oracle.prepare(48000);
        check(oracle.setControlFrame(frame), "unmetered core oracle accepts matching control frame");
        auto initial = host.consumeOutputMeter();
        check(initial.samplePeak == 0 && !initial.overRange && !initial.nonFinite,
              "prepare clears old output telemetry");
        std::uint32_t expectedPeak = 0;
        bool expectedOver = false;
        auto consume = [&]
        {
            const auto reading = host.consumeOutputMeter();
            check(std::bit_cast<std::uint32_t>(reading.samplePeak) == expectedPeak
                      && reading.overRange == expectedOver && !reading.nonFinite,
                  "host telemetry equals independent final-output scan, including accumulated blocks");
            const auto empty = host.consumeOutputMeter();
            check(empty.samplePeak == 0 && !empty.overRange && !empty.nonFinite,
                  "extra GUI polling does not replay acknowledged peaks/events");
            expectedPeak = 0; expectedOver = false;
        };
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        const std::array<int, 6> sizes {257, 4097, 127, 8193, 0, 513};
        for (size_t block = 0; block < sizes.size(); ++block)
        {
            const int frames = sizes[block];
            juce::AudioBuffer<float> audio(3, frames);
            for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                std::fill_n(audio.getWritePointer(ch), frames, std::numeric_limits<float>::quiet_NaN());
            juce::MidiBuffer midi;
            check(oracle.setControlFrame(frame), "oracle repeated frame accepted");
            if (block == 0)
            {
                for (int note = 0; note < 8; ++note)
                {
                    const int channel = 2 + note % 2;
                    midi.addEvent(juce::MidiMessage::noteOn(channel, 48 + note, (juce::uint8) 127), 0);
                    oracle.noteOn(48 + note, 1, channel);
                }
            }
            else if (block == 1)
            {
                midi.addEvent(juce::MidiMessage::pitchWheel(2, 9011), 0);
                midi.addEvent(juce::MidiMessage::channelPressureChange(2, 80), 0);
                oracle.pitchBend(2, (9011 - 8192) / 8192.0f);
                oracle.channelPressure(2, 80.0f / 127.0f);
            }
            else if (block == 2)
            {
                midi.addEvent(juce::MidiMessage::controllerEvent(1, 11, 50), 0);
                oracle.controlChange(11, 50.0f / 127.0f, 1);
            }
            else if (block == 3)
            {
                for (int note = 0; note < 4; ++note)
                {
                    const int channel = 2 + note % 2;
                    midi.addEvent(juce::MidiMessage::noteOff(channel, 48 + note), 0);
                    oracle.noteOff(48 + note, channel);
                }
            }
            else if (block == 5)
            {
                for (int channel : {2, 3})
                {
                    midi.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
                    oracle.controlChange(123, 0, channel);
                }
            }
            auto bytes = [](const juce::MidiBuffer& messages)
            {
                std::vector<int> result;
                for (const auto event : messages)
                {
                    result.push_back(event.samplePosition);
                    result.push_back(event.numBytes);
                    for (int i = 0; i < event.numBytes; ++i) result.push_back(event.data[i]);
                }
                return result;
            };
            const auto originalMidi = bytes(midi);
            const auto originalParams = host.currentValues().v;
            host.processBlock(audio, midi);
            std::vector<float> reference((size_t) frames * 2);
            oracle.render(reference.data(), frames);
            bool exact = true;
            for (int sample = 0; sample < frames; ++sample)
                for (int ch = 0; ch < channels; ++ch)
                {
                    const float expected = channels == 1
                        ? .5f * (reference[(size_t) sample * 2] + reference[(size_t) sample * 2 + 1])
                        : reference[(size_t) sample * 2 + (size_t) ch];
                    const auto actual = std::bit_cast<std::uint32_t>(audio.getSample(ch, sample));
                    exact = exact && actual == std::bit_cast<std::uint32_t>(expected);
                    const auto magnitude = actual & 0x7fffffffu;
                    check(magnitude < 0x7f800000u, "ordinary host audio stays finite");
                    expectedPeak = std::max(expectedPeak, magnitude);
                    expectedOver = expectedOver || magnitude > 0x3f800000u;
                    observedAboveFullScale = observedAboveFullScale || magnitude > 0x3f800000u;
                }
            check(exact, "metered host output is bit-exact with unmetered core plus identical channel mapping");
            check(bytes(midi) == originalMidi && host.currentValues().v == originalParams,
                  "output metering leaves host MIDI and all parameters unchanged");
            if (block == 0 || block == 2 || block == 4 || block == 5) consume();
            if (block == 2 && policy == 1 && channels == 2)
            {
                // Opening consumes stale telemetry but must not change sound or
                // configuration. Subsequent blocks still compare against core.
                editor.reset(host.createEditor());
                check(editor != nullptr && host.currentValues().v == originalParams,
                      "opening output-meter UI leaves sound parameters unchanged");
            }
        }
        editor.reset();
        host.releaseResources();
        const auto released = host.consumeOutputMeter();
        check(released.samplePeak == 0 && !released.overRange && !released.nonFinite,
              "release clears pending meter history without touching saved sound");
    }
    check(observedAboveFullScale, "source headroom fixture produces real host samples above full scale");
}
} // namespace cs80::tests
