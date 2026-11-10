#pragma once

#include "plugin/PluginProcessor.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cmath>
#include <memory>

namespace cs80::tests
{
// Real processBlock dispatch, including its event ordering and processor-owned
// expression/prefix state. No editor, native peer or external DAW is involved.
template <class Check>
void testControllerResetHost(Check check)
{
    juce::TemporaryFile fixture("-cs80-controller-reset-host");
    auto make = [&](bool circuit, bool sixteen, bool pure) {
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        host->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        host->setParamValue(P_parityMode, pure ? 1.0f : 0.0f);
        host->setParamValue(P_voiceMode, sixteen ? 1.0f : 0.0f);
        host->setParamValue(P_kasScanTiming, 0.0f);
        host->setParamValue(P_arpOn, 0.0f);
        host->setParamValue(P_mpeOn, 0.0f);
        host->setParamValue(P_voiceDispersion, 0.0f);
        host->setParamValue(P_drift, 0.0f);
        host->setParamValue(P_analogNoise, 0.0f);
        host->setParamValue(P_velocityCurve, 0.0f);
        host->setParamValue(P_pressureCurve, 0.0f);
        host->setParamValue(P_modWheelSub, 0.7f);
        host->setParamValue(P_modWheelVcf, 0.7f);
        host->prepareToPlay(48000.0, 64);
        return host;
    };
    auto voice = [](Cs80AudioProcessor& host, int note) {
        for (const auto* e : std::array<const Engine*, 2>{&host.getEngine(), &host.getExtensionEngine()})
            for (int i = 0; i < 8; ++i)
                if (const auto v = e->getVoiceInfo(i); v.active && v.note == note) return v;
        return Engine::VoiceInfo{};
    };
    auto sameAudio = [](const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b) {
        for (int channel = 0; channel < a.getNumChannels(); ++channel)
            for (int i = 0; i < a.getNumSamples(); ++i)
                if (!std::isfinite(a.getSample(channel, i))
                    || std::bit_cast<std::uint32_t>(a.getSample(channel, i))
                       != std::bit_cast<std::uint32_t>(b.getSample(channel, i))) return false;
        return true;
    };
    // Real host MIDI must use the engine's negotiated MPE CC120 route,
    // including processor-owned held-key display/ownership masks.
    for (bool circuit : {false, true})
        for (bool sixteen : {false, true})
            for (int frames : {0, 32})
                for (int mode : {0, 1, 2, 3, 4})
    {
        const bool enabled = mode != 3;
        const bool pure = mode == 4;
        auto host = make(circuit, sixteen, pure);
        host->setParamValue(P_mpeOn, enabled ? 1.f : 0.f);
        juce::AudioBuffer<float> audio(2, frames);
        juce::MidiBuffer midi;
        auto tick = [&] { host->processBlock(audio, midi); midi.clear(); };
        tick();
        auto cc = [&](int channel, int controller, int value) {
            midi.addEvent(juce::MidiMessage::controllerEvent(channel, controller, value), 0);
        };
        for (int master : {1, 16}) { cc(master, 101, 0); cc(master, 100, 6); cc(master, 6, 2); }
        tick();
        const int channels[] {2, 3, 14, 15, 8};
        const int count = sixteen && !pure ? 16 : 8;
        for (int i = 0; i < count; ++i)
            midi.addEvent(juce::MidiMessage::noteOn(channels[i % 5], 40+i, (juce::uint8) 100), 0);
        tick();
        const int target = mode == 1 ? 16 : mode == 2 ? 2 : 1;
        midi.addEvent(juce::MidiMessage::allSoundOff(target), 0);
        tick();
        for (int i = 0; i < count; ++i)
        {
            const int channel = channels[i % 5];
            const bool affected = channel == target || (enabled && !pure
                && ((target == 1 && (channel == 2 || channel == 3))
                    || (target == 16 && (channel == 14 || channel == 15))));
            check(voice(*host,40+i).active != affected,
                  "host CC120 silences only its active MPE zone or conventional addressed channel");
            check(host->isHostKeyDown(40+i) != affected,
                  "host CC120 held-key display masks match the voices it silences");
        }
    }

    for (bool circuit : {false, true})
        for (bool sixteen : {false, true})
            for (int frames : {0, 32})
    {
        auto host = make(circuit, sixteen, false), reference = make(circuit, sixteen, false);
        juce::AudioBuffer<float> audio(2, frames), otherAudio(2, frames);
        juce::MidiBuffer midi, otherMidi;
        auto tick = [&] { host->processBlock(audio, midi); midi.clear(); };
        auto both = [&] {
            host->processBlock(audio, midi); reference->processBlock(otherAudio, otherMidi);
            midi.clear(); otherMidi.clear();
            check(sameAudio(audio, otherAudio), "CC121 host audio matches ordinary defaults at identical event positions");
        };
        auto event = [&](const juce::MidiMessage& m, int position = 0) {
            midi.addEvent(m, position); otherMidi.addEvent(m, position);
        };
        both();
        const int capacity = sixteen ? 16 : 8;
        for (int i = 0; i < capacity; ++i)
            event(juce::MidiMessage::noteOn(i % 2 == 0 ? 2 : 3, 40 + i, (juce::uint8) 100));
        event(juce::MidiMessage::controllerEvent(2, 1, 91));
        event(juce::MidiMessage::controllerEvent(2, 11, 37));
        event(juce::MidiMessage::controllerEvent(2, 74, 97));
        event(juce::MidiMessage::channelPressureChange(2, 71));
        event(juce::MidiMessage::channelPressureChange(3, 83));
        event(juce::MidiMessage::pitchWheel(2, 12288));
        both();
        for (int i = 0; i < 8; ++i) both();
        midi.addEvent(juce::MidiMessage::controllerEvent(2, 121, 0), 13);
        otherMidi.addEvent(juce::MidiMessage::controllerEvent(2, 1, 0), 13);
        otherMidi.addEvent(juce::MidiMessage::controllerEvent(2, 11, 127), 13);
        otherMidi.addEvent(juce::MidiMessage::pitchWheel(2, 8192), 13);
        otherMidi.addEvent(juce::MidiMessage::channelPressureChange(2, 0), 13);
        otherMidi.addEvent(juce::MidiMessage::controllerEvent(2, 64, 0), 13);
        both();
        check(host->getExpressionValue() == 1.0f, "CC121 publishes full expression instead of stale UI/lifecycle value");
        for (int i = 0; i < capacity; ++i)
            check(voice(*host, 40 + i).held
                      && voice(*host, 40 + i).pressure == (i % 2 == 0 ? 0.0f : 83.0f / 127.0f),
                  "real CC121 reaches addressed primary/extension voices and preserves other-channel pressure/held keys");
        for (int i = 0; i < 8; ++i) both();

        // A new physical gesture after the reset must not be discarded.
        host->setExpressionFromUI(0.4f); reference->setExpressionFromUI(0.4f); both();
        check(host->getExpressionValue() == 0.4f, "later UI expression survives reset without mailbox clearing");
        event(juce::MidiMessage::controllerEvent(2, 121, 0)); both();
        // Reprepare and extension reactivation must reapply the new target.
        for (auto* h : {host.get(), reference.get()}) h->prepareToPlay(48000.0, 64);
        both();
        for (int i = 0; i < capacity; ++i)
            event(juce::MidiMessage::noteOn(2, 40 + i, (juce::uint8) 100));
        both();
        for (auto* h : {host.get(), reference.get()}) h->setParamValue(P_voiceMode, 0.0f);
        both();
        for (auto* h : {host.get(), reference.get()}) h->setParamValue(P_voiceMode, 1.0f);
        both();
        event(juce::MidiMessage::noteOn(2, 80, (juce::uint8) 100)); both();
        check(host->getExpressionValue() == 1.0f, "reset expression survives prepare and extension reactivation");

        // Prefix ordering at equal timestamps (including zero-frame flushes)
        // and separated offsets. In 16 voices these notes use the extension.
        for (bool prefixAfter : {false, true})
            for (bool separated : {false, true})
                for (int channel = 1; channel <= 16; ++channel)
        {
            const int otherChannel = channel % 16 + 1;
            host->requestPanicFromUI(); tick();
            host->setParamValue(P_voiceMode, sixteen ? 1.0f : 0.0f); tick();
            if (sixteen)
            {
                for (int i = 0; i < 8; ++i)
                    midi.addEvent(juce::MidiMessage::noteOn(4, 40 + i, (juce::uint8) 100), 0);
                tick();
            }
            midi.addEvent(juce::MidiMessage::controllerEvent(otherChannel, 88, 53), 0);
            midi.addEvent(juce::MidiMessage::controllerEvent(channel, prefixAfter ? 121 : 88, prefixAfter ? 0 : 53), 0);
            midi.addEvent(juce::MidiMessage::controllerEvent(channel, prefixAfter ? 88 : 121, prefixAfter ? 53 : 0), separated ? 11 : 0);
            midi.addEvent(juce::MidiMessage::noteOn(channel, 60, (juce::uint8) 80), separated ? 21 : 0);
            midi.addEvent(juce::MidiMessage::noteOn(otherChannel, 64, (juce::uint8) 80), separated ? 21 : 0);
            tick();
            const float prefixed = ((80 << 7) | 53) / 16383.0f;
            check(voice(*host, 60).held && voice(*host, 60).velocity == (prefixAfter ? prefixed : 80.0f * (1.0f / 127.0f)),
                  "CC88/reset/note order is retained at same and distinct sample offsets");
            check(voice(*host, 64).held && voice(*host, 64).velocity == prefixed,
                  "reset leaves another channel's pending high-resolution strike intact");
        }
        // MIDI learn maps are not reset or driven by synthetic CC1/11 events.
        host->getMidiLearn().assign(1, P_volume);
        host->getMidiLearn().assign(11, P_brilliance);
        host->setParamValue(P_volume, 0.43f);
        host->setParamValue(P_brilliance, 0.27f);
        tick();
        std::array<float, P_numParams> before;
        for (int i = 0; i < P_numParams; ++i) before[(size_t) i] = host->getParamValue(i);
        const auto generation = host->getMidiLearn().generation();
        const auto program = host->getCurrentProgram();
        host->getMidiLearn().armLearn(P_resonance); // mode messages must not complete learning
        midi.addEvent(juce::MidiMessage::controllerEvent(2, 121, 0), 0); tick();
        bool unchanged = true;
        for (int i = 0; i < P_numParams; ++i) unchanged &= before[(size_t) i] == host->getParamValue(i);
        check(unchanged && host->getCurrentProgram() == program, "CC121 changes no APVTS parameter or program");
        check(host->getMidiLearn().parameterFor(1) == P_volume
                  && host->getMidiLearn().parameterFor(11) == P_brilliance
                  && host->getMidiLearn().generation() == generation + 1
                  && host->getMidiLearn().learningParameter() == P_resonance,
              "CC121 preserves MIDI learn mappings and does not consume an armed learn");
    }
    for (bool circuit : {false, true})
        for (int frames : {0, 32})
    {
        auto host = make(circuit, true, true), reference = make(circuit, true, true);
        juce::AudioBuffer<float> audio(2, frames), otherAudio(2, frames);
        juce::MidiBuffer midi, otherMidi;
        for (auto* h : {host.get(), reference.get()}) h->setExpressionFromUI(0.3f);
        for (auto* m : {&midi, &otherMidi})
        {
            m->addEvent(juce::MidiMessage::noteOn(2, 60, (juce::uint8) 90), 0);
            m->addEvent(juce::MidiMessage::channelPressureChange(2, 83), 0);
            m->addEvent(juce::MidiMessage::controllerEvent(2, 88, 53), 0);
        }
        midi.addEvent(juce::MidiMessage::controllerEvent(2, 121, 0), 11);
        for (auto* m : {&midi, &otherMidi}) m->addEvent(juce::MidiMessage::noteOn(2, 64, (juce::uint8) 80), 21);
        host->processBlock(audio, midi); reference->processBlock(otherAudio, otherMidi);
        check(sameAudio(audio, otherAudio) && host->getExpressionValue() == 0.3f,
              "PURE CC121 retains physical expression and exact audio");
        check(voice(*host, 60).pressure == 83.0f / 127.0f
                  && voice(*host, 64).velocity == ((80 << 7) | 53) / 16383.0f,
              "PURE CC121 retains pressure and pending high-resolution strike");
        midi.clear(); otherMidi.clear();
        for (int i = 0; i < 8; ++i)
        {
            host->processBlock(audio, midi); reference->processBlock(otherAudio, otherMidi);
            check(sameAudio(audio, otherAudio), "PURE reset leaves the continuing render byte-identical");
        }
    }
}
} // namespace cs80::tests
