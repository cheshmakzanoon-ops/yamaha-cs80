#pragma once
#include "plugin/PluginProcessor.h"

namespace cs80::tests
{
template <class Check>
void testVoiceActivityHost(Check check)
{
    juce::TemporaryFile fixture("-cs80-voice-activity");
    for (bool circuit : { false, true })
        for (int frames : { 0, 32 })
    {
        Cs80AudioProcessor host(fixture.getFile());
        host.setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        host.setParamValue(P_parityMode, 0.0f);
        host.setParamValue(P_voiceMode, 1.0f);
        host.setParamValue(P_kasScanTiming, 0.0f);
        host.setParamValue(P_arpOn, 0.0f);
        host.setParamValue(P_mpeOn, 0.0f);
        host.setParamValue(P_analogNoise, 0.0f);
        host.setParamValue(P_masterSustain, 0.0f);
        host.setParamValue(P_sustainMode, 0.0f);
        for (int rank = 0; rank < 2; ++rank) host.setParamValue(chParam(rank, CP_vR), 0.5f);
        host.prepareToPlay(48000.0, 32);
        check(host.getVoiceActivity() == VoiceActivityReading{}, "prepare clears stale voice activity before the first callback");
        juce::AudioBuffer<float> audio(2, frames);
        juce::MidiBuffer midi;
        auto tick = [&] { host.processBlock(audio, midi); midi.clear(); };
        auto fill = [&] {
            for (int i = 0; i < 16; ++i)
                midi.addEvent(juce::MidiMessage::noteOn(i < 8 ? 1 : 2, 48 + i, (juce::uint8) 100), 0);
            tick();
        };
        tick();
        check(host.getVoiceActivity() == VoiceActivityReading{0, 16}, "audio-applied sixteen-voice mode publishes even while silent");
        fill();
        check(host.getVoiceActivity() == VoiceActivityReading{0xffffu, 16} && host.getActiveVoiceCount() == 16,
              "both full instruments publish sixteen active slots");
        bool slots = true;
        for (int i = 0; i < 16; ++i) slots &= host.isVoiceActive(i);
        check(slots && !host.isVoiceActive(-1) && !host.isVoiceActive(16), "processor slot query includes extension and checks bounds");
        midi.addEvent(juce::MidiMessage::allSoundOff(1), 0); tick();
        check(host.getVoiceActivity() == VoiceActivityReading{0xff00u, 16} && host.getActiveVoiceCount() == 8,
              "extension-only activity is not mistaken for silence or primary slots");
        for (int i = 8; i < 16; ++i) midi.addEvent(juce::MidiMessage::noteOff(2, 48 + i), 0);
        tick();
        check(host.getVoiceActivity() == VoiceActivityReading{0xff00u, 16}, "release tails remain active in the extension meter");
        host.requestPanicFromUI(); tick();
        check(host.getVoiceActivity() == VoiceActivityReading{0, 16}, "panic clears both rows without changing applied capacity");
        fill();
        host.setParamValue(P_voiceMode, 0.0f); tick();
        check(host.getVoiceActivity().capacity == 8 && (host.getVoiceActivity().mask & 0xff00u) == 0,
              "eight-voice mode removes stale extension lamps");
        host.setParamValue(P_voiceMode, 1.0f); tick();
        host.requestPanicFromUI(); tick(); fill();
        host.setParamValue(P_parityMode, 1.0f); tick();
        check(host.getVoiceActivity() == VoiceActivityReading{}, "PURE transition clears voices and forces eight-slot telemetry");
        fill();
        check(host.getVoiceActivity() == VoiceActivityReading{0xffu, 8} && host.getActiveVoiceCount() == 8,
              "PURE ignores the stored sixteen-voice request");
        host.prepareToPlay(48000.0, 32);
        check(host.getVoiceActivity() == VoiceActivityReading{}, "re-prepare clears previously sounding voice lamps");
        fill();
        host.releaseResources();
        check(host.getVoiceActivity() == VoiceActivityReading{}, "releaseResources cannot leave stale voice lamps");
    }
}
} // namespace cs80::tests
