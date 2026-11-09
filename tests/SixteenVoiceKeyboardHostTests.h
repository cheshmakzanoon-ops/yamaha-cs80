#pragma once

#include "plugin/PluginProcessor.h"

#include <array>
#include <cmath>

namespace cs80::tests
{
// Real processor/FIFO routing, not direct Engine allocation. Eight host-held
// primary voices force each UI key into the extension instrument. A rejected
// release used to leave it held because only primary slots were snapshotted.
template <class Check>
void testSixteenVoiceKeyboardOwnershipHost(Check check)
{
    juce::TemporaryFile fixture("-cs80-sixteen-keyboard-host");
    for (bool circuit : { false, true })
        for (int frames : { 0, 32 })
    {
        Cs80AudioProcessor host(fixture.getFile());
        host.setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit
                                   : Cs80AudioProcessor::EngineMode::legacy);
        host.setParamValue(P_voiceMode, 1.0f);
        host.setParamValue(P_parityMode, 0.0f);
        host.setParamValue(P_kasScanTiming, 0.0f);
        host.setParamValue(P_arpOn, 0.0f);
        host.setParamValue(P_mpeOn, 0.0f);
        host.setParamValue(P_analogNoise, 0.0f);
        host.setParamValue(P_masterSustain, 0.0f);
        host.setParamValue(P_sustainMode, 0.0f);
        host.setParamValue(chParam(0, CP_vR), 0.5f);
        host.setParamValue(chParam(1, CP_vR), 0.5f);
        host.prepareToPlay(48000.0, 32);
        host.serviceKeyboardUI();
        juce::AudioBuffer<float> audio(2, frames);
        juce::MidiBuffer midi;
        auto tick = [&] { host.processBlock(audio, midi); midi.clear(); };
        auto require = [&](bool condition, const char* message)
        {
            const auto label = juce::String(circuit ? "Circuit 16 voices: " : "Legacy 16 voices: ") + message;
            check(condition, label.toRawUTF8());
        };
        auto strike = [&](int note)
        { return host.captureKeyboardGesture(1, note, [&] { host.getKeyboardState().noteOn(1, note, 0.8f); }); };
        auto release = [&](int note) { host.getKeyboardState().noteOff(1, note, 0.0f); };
        auto hostOn = [&](int note, int channel = 1)
        { midi.addEvent(juce::MidiMessage::noteOn(channel, note, (juce::uint8) 100), 0); };
        auto held = [](const Engine& engine)
        {
            int count = 0;
            for (int i = 0; i < 8; ++i) count += engine.getVoiceInfo(i).held;
            return count;
        };
        auto pressure = [](const Engine& engine, int note)
        {
            for (int i = 0; i < 8; ++i)
            {
                const auto voice = engine.getVoiceInfo(i);
                if (voice.active && voice.held && voice.note == note) return voice.pressure;
            }
            return -1.0f;
        };
        auto fillPrimary = [&]
        {
            for (int note = 48; note < 56; ++note) hostOn(note);
            tick();
            require(held(host.getEngine()) == 8, "eight host keys occupy primary slots");
        };
        auto reset = [&]
        {
            host.requestPanicFromUI(); tick(); host.serviceKeyboardUI();
        };
        tick(); host.serviceKeyboardUI(); // Settle lifecycle before capturing UI input.
        fillPrimary();

        std::array<KeyPressureToken, 8> tokens;
        for (int i = 0; i < 8; ++i)
        {
            tokens[(size_t) i] = strike(60 + i);
            host.setKeyPressureFromUI(tokens[(size_t) i], 0.2f + 0.05f * i);
        }
        tick();
        require(held(host.getEngine()) == 8 && held(host.getExtensionEngine()) == 8,
                "eight UI keys fill extension without replacing primary host keys");
        for (int i = 0; i < 8; ++i)
            require(tokens[(size_t) i]
                    && std::abs(pressure(host.getExtensionEngine(), 60 + i) - (0.2f + 0.05f * i)) < 1.0e-6f,
                    "extension UI key receives only its captured gesture pressure");

        const auto refused = strike(48); host.setKeyPressureFromUI(refused, 0.95f); tick();
        release(48); tick();
        require(held(host.getEngine()) == 8 && held(host.getExtensionEngine()) == 8
                    && pressure(host.getEngine(), 48) == 0.0f,
                "refused seventeenth UI key neither pressures nor releases a host key");
        for (int note = 60; note < 68; ++note) release(note);
        tick();
        require(held(host.getExtensionEngine()) == 0 && held(host.getEngine()) == 8,
                "all eight extension UI releases reach their accepted onsets");
        for (const auto& token : tokens) host.setKeyPressureFromUI(token, 0.95f);
        tick();
        require(held(host.getExtensionEngine()) == 0 && held(host.getEngine()) == 8,
                "late released pressure cannot revive extension or disturb host holds");

        reset(); fillPrimary();
        const auto shortNote = strike(60); release(60); tick();
        require(shortNote && held(host.getExtensionEngine()) == 0 && held(host.getEngine()) == 8,
                "same-batch ninth UI onset and release cannot leave a stuck extension key");

        reset(); fillPrimary();
        const auto flooded = strike(60); tick();
        bool overflow = false;
        for (int i = 0; i < 300; ++i) overflow |= !host.setKeyPressureFromUI(flooded, 0.5f);
        tick(); host.serviceKeyboardUI();
        require(overflow && held(host.getExtensionEngine()) == 0 && held(host.getEngine()) == 8,
                "overflow recovery releases extension UI notes but preserves unrelated host keys");

        // Both alias directions are significant: the pressure event is sent
        // to both engines, so an unknown same-pitch origin in either blocks it.
        reset(); fillPrimary();
        const auto extensionAlias = strike(48); tick();
        host.setKeyPressureFromUI(extensionAlias, 0.9f); tick();
        require(pressure(host.getEngine(), 48) == 0.0f && pressure(host.getExtensionEngine(), 48) == 0.0f,
                "primary host pitch alias suppresses extension UI pressure");
        reset(); fillPrimary();
        hostOn(60, 2); tick();
        midi.addEvent(juce::MidiMessage::noteOff(1, 48), 0); tick();
        const auto primaryAlias = strike(60); tick(); // Reuse the freed primary slot beside an older extension host note.
        host.setKeyPressureFromUI(primaryAlias, 0.9f); tick();
        require(pressure(host.getEngine(), 60) == 0.0f && pressure(host.getExtensionEngine(), 60) == 0.0f,
                "extension host pitch alias suppresses primary UI pressure");
        midi.addEvent(juce::MidiMessage::aftertouchChange(2, 60, 55), 0); tick();
        require(pressure(host.getEngine(), 60) == 0.0f
                    && std::abs(pressure(host.getExtensionEngine(), 60) - 55.0f / 127.0f) < 1.0e-6f,
                "channel-two host aftertouch reaches extension without changing channel-one primary");
        midi.addEvent(juce::MidiMessage::aftertouchChange(1, 60, 29), 0); tick();
        require(std::abs(pressure(host.getEngine(), 60) - 29.0f / 127.0f) < 1.0e-6f
                    && std::abs(pressure(host.getExtensionEngine(), 60) - 55.0f / 127.0f) < 1.0e-6f,
                "channel-one host aftertouch reaches primary without changing channel-two extension");

        reset(); fillPrimary();
        const auto retired = strike(60); tick();
        host.setParamValue(P_voiceMode, 0.0f); tick();
        host.setKeyPressureFromUI(retired, 0.9f); tick();
        require(held(host.getExtensionEngine()) == 0 && held(host.getEngine()) == 8,
                "leaving sixteen-voice mode retires extension despite late pressure");
        release(60); tick();
        host.setParamValue(P_voiceMode, 1.0f); tick();
        const auto fresh = strike(60); tick();
        host.setKeyPressureFromUI(retired, 0.9f); tick();
        require(pressure(host.getExtensionEngine(), 60) == 0.0f,
                "reactivated extension rejects the retired gesture token");
        host.setKeyPressureFromUI(fresh, 0.4f); tick();
        require(std::abs(pressure(host.getExtensionEngine(), 60) - 0.4f) < 1.0e-6f,
                "reactivated extension accepts a fresh UI gesture");
        release(60); tick();
        require(held(host.getExtensionEngine()) == 0 && held(host.getEngine()) == 8,
                "fresh reactivated UI note releases normally");
    }
}
} // namespace cs80::tests
