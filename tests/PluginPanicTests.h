#pragma once

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <cmath>
#include <memory>

namespace cs80::tests
{
namespace panic_detail
{
inline juce::Button* findButton(juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (child->getComponentID() == "panic_notes")
            return dynamic_cast<juce::Button*>(child);
        if (auto* found = findButton(*child))
            return found;
    }
    return nullptr;
}

inline bool stopped(Cs80AudioProcessor& host)
{
    if (host.getActiveVoiceCount() != 0)
        return false;
    for (int v = 0; v < 8; ++v)
    {
        const auto voice = host.getEngine().getVoiceInfo(v);
        if (voice.active || voice.held || voice.pedalHeld)
            return false;
    }
    for (int note = 0; note < 128; ++note)
        if (host.isHostKeyDown(note))
            return false;
    return true;
}
} // namespace panic_detail

template <typename Check, typename Flush>
void testPluginPanic(Check check, Flush flush)
{
    juce::TemporaryFile fixture("-cs80-panic-tests");
    constexpr int rate = 8000, blockSize = 32;
    // Legacy/source-performance/isolation all share the existing global hard
    // stop, even though isolation ignores incoming sustain/modern arp controls.
    for (int mode = 0; mode < 3; ++mode)
    {
        Cs80AudioProcessor host(fixture.getFile());
        host.setParamValue(cs80::P_sourcePerformance, mode == 1 ? 1.0f : 0.0f);
        host.setParamValue(cs80::P_parityMode, mode == 2 ? 1.0f : 0.0f);
        host.setParamValue(cs80::P_mpeOn, 1.0f);
        host.setParamValue(cs80::P_masterSustain, 0.0f);
        host.setParamValue(cs80::P_sustainMode, 0.0f);
        host.setExpressionFromUI(0.37f);
        host.prepareToPlay(rate, blockSize);
        host.serviceKeyboardUI();
        juce::AudioBuffer<float> audio(2, blockSize);
        juce::MidiBuffer midi;
        auto tick = [&] { host.processBlock(audio, midi); midi.clear(); };
        tick();
        auto& keyboard = host.getKeyboardState();

        // Set every MIDI sustain latch, and establish both UI and host voices.
        for (int channel = 1; channel <= 16; ++channel)
            midi.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 127), 0);
        for (int channel : { 1, 8, 16 })
            midi.addEvent(juce::MidiMessage::noteOn(channel, 40 + channel, (juce::uint8) 100), 0);
        auto pressureToken = host.captureKeyboardGesture(1, 60, [&] { keyboard.noteOn(1, 60, 0.8f); });
        tick();
        check(host.getActiveVoiceCount() >= 4, "panic fixture contains UI and multichannel host voices");
        keyboard.noteOn(1, 61, 0.8f); // Never consumed: this must not sound after panic.
        host.setKeyPressureFromUI(pressureToken, 0.9f);
        const auto stateBefore = host.getAPVTS().copyState();
        const auto metadataBefore = host.getPresetMetadata();
        const int activeBefore = host.getActiveVoiceCount();
        for (int click = 0; click < 8; ++click)
            host.requestPanicFromUI();
        check(host.getActiveVoiceCount() == activeBefore && host.getEngine().activeVoices() == activeBefore,
              "UI panic request never mutates audio-owned voices before processBlock");
        tick();
        check(panic_detail::stopped(host), "panic clears all existing voices and host-key visuals at the block boundary");
        check(stateBefore.isEquivalentTo(host.getAPVTS().copyState())
                  && host.getPresetName() == metadataBefore.name
                  && host.getCurrentPresetIndex() == metadataBefore.factoryIndex
                  && std::abs(host.getExpressionValue() - 0.37f) < 1.0e-6f,
              "panic preserves all parameters, patch metadata and physical expression position");

        keyboard.noteOn(1, 62, 0.8f);
        tick();
        check(panic_detail::stopped(host), "panic epoch rejects UI strikes until the message thread acknowledges reset");
        host.serviceKeyboardUI();
        bool uiClear = true;
        for (int note = 0; note < 128; ++note)
            uiClear = uiClear && !keyboard.isNoteOnForChannels(0xffff, note);
        check(uiClear, "message-thread panic acknowledgment clears all displayed UI keys");
        pressureToken = host.captureKeyboardGesture(1, 60, [&] { keyboard.noteOn(1, 60, 0.8f); });
        tick();
        check(host.getActiveVoiceCount() == 1, "acknowledgment restores on-screen keyboard playability");

        // Old UI releases/pressure must not claim a new host strike at the same
        // pitch. New host MIDI is intentionally processed after the panic.
        host.setKeyPressureFromUI(pressureToken, 0.95f);
        host.requestPanicFromUI();
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 110), 0);
        tick();
        keyboard.noteOff(1, 60, 0.0f);
        tick();
        bool freshHost = false;
        for (int v = 0; v < 8; ++v)
        {
            const auto voice = host.getEngine().getVoiceInfo(v);
            freshHost = freshHost || (voice.active && voice.held && voice.note == 60
                                     && voice.channel == 1 && voice.pressure == 0.0f);
        }
        check(freshHost && host.isHostKeyDown(60),
              "new same-block host MIDI survives stale UI release and pending pre-panic pressure");
        host.requestPanicFromUI();
        tick();
        host.serviceKeyboardUI();

        // A leftover latch on any channel (including MPE master) would capture
        // one of these fresh note-offs. Inspect actual audio-owned pedal flags.
        for (int channel = 1; channel <= 16; ++channel)
        {
            midi.addEvent(juce::MidiMessage::noteOn(channel, 40 + channel, (juce::uint8) 100), 0);
            midi.addEvent(juce::MidiMessage::noteOff(channel, 40 + channel), 1);
            tick();
            bool pedalHeld = false;
            for (int v = 0; v < 8; ++v)
                pedalHeld = pedalHeld || host.getEngine().getVoiceInfo(v).pedalHeld;
            check(!pedalHeld, "panic clears MIDI sustain latch on every channel before subsequent notes");
        }
        host.requestPanicFromUI();
        tick();
        host.serviceKeyboardUI();

        // Overflow must not hide a panic request or replay the discarded batch.
        for (int i = 0; i < 257; ++i)
            keyboard.noteOn(1, 63, 0.8f);
        host.requestPanicFromUI();
        tick();
        host.serviceKeyboardUI();
        tick();
        check(panic_detail::stopped(host), "panic clears a saturated UI queue without replaying old strikes");

        host.setParamValue(cs80::P_arpOn, 1.0f);
        host.setParamValue(cs80::P_arpRate, 20.0f);
        midi.addEvent(juce::MidiMessage::noteOn(2, 48, (juce::uint8) 100), 0);
        midi.addEvent(juce::MidiMessage::noteOn(16, 55, (juce::uint8) 100), 0);
        tick();
        host.requestPanicFromUI();
        tick();
        bool arpStayedStopped = true;
        for (int block = 0; block < rate / blockSize; ++block)
        {
            tick();
            arpStayedStopped = arpStayedStopped && host.getActiveVoiceCount() == 0;
        }
        check(arpStayedStopped && host.getParamValue(cs80::P_arpOn) == 1.0f,
              "panic clears held arpeggiator keys without changing the enabled arp parameter");
    }

    // Requests made while the host is stopped survive prepare and also work on
    // a zero-frame callback; repeated requests coalesce rather than queue work.
    {
        Cs80AudioProcessor host(fixture.getFile());
        host.requestPanicFromUI();
        host.prepareToPlay(rate, blockSize);
        host.serviceKeyboardUI();
        host.getKeyboardState().noteOn(1, 70, 0.8f);
        juce::AudioBuffer<float> zero(2, 0);
        juce::MidiBuffer empty;
        host.processBlock(zero, empty);
        check(panic_detail::stopped(host), "pending-before-prepare panic executes on a zero-frame block and discards queued UI input");
        host.serviceKeyboardUI();
        host.getKeyboardState().noteOn(1, 70, 0.8f);
        host.processBlock(zero, empty);
        check(host.getActiveVoiceCount() == 1, "zero-frame panic acknowledgment permits subsequent new strikes");
    }

    // Compare to the existing hard-stop primitive, not an invented silence
    // threshold. Delay history and continuous noise must not be reset by PANIC.
    for (bool noise : { false, true })
    {
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        auto reference = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        for (auto* processor : { host.get(), reference.get() })
        {
            processor->setParamValue(cs80::P_analogNoise, noise ? 0.7f : 0.0f);
            processor->setParamValue(cs80::P_fx1Type, noise ? 0.0f : 3.0f);
            processor->setParamValue(cs80::P_fx1Mix, noise ? 0.0f : 1.0f);
            processor->prepareToPlay(rate, 256);
            processor->serviceKeyboardUI();
        }
        juce::AudioBuffer<float> audio(2, 256), expected(2, 256);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8) 100), 0);
        for (int block = 0; block < 32; ++block)
        {
            host->processBlock(audio, midi);
            reference->processBlock(expected, midi);
            midi.clear();
        }
        host->requestPanicFromUI();
        reference->getEngine().allNotesOff(false, 0); // Sequential offline oracle only.
        bool same = true;
        double energy = 0.0;
        for (int block = 0; block < 8; ++block)
        {
            host->processBlock(audio, midi);
            reference->processBlock(expected, midi);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 256; ++sample)
                {
                    const auto value = audio.getSample(channel, sample);
                    same = same && value == expected.getSample(channel, sample) && std::isfinite(value);
                    energy += (double) value * value;
                }
        }
        check(same && host->getActiveVoiceCount() == 0 && energy > 1.0e-9,
              noise ? "panic preserves existing analog-noise policy rather than promising silence"
                    : "panic preserves existing FX tail exactly like the global hard-stop primitive");
    }

    {
        Cs80AudioProcessor host(fixture.getFile());
        host.prepareToPlay(rate, blockSize);
        Cs80AudioProcessorEditor editor(host);
        auto* button = panic_detail::findButton(editor);
        check(button != nullptr, "actual editor exposes the visible PANIC control");
        if (button != nullptr)
        {
            for (auto size : { juce::Point<int>(1120, 700), juce::Point<int>(1600, 1000) })
            {
                editor.setSize(size.x, size.y);
                check(button->isVisible() && button->getWidth() >= 50 && button->getHeight() >= 20
                          && button->getParentComponent()->getLocalBounds().contains(button->getBounds()),
                      "PANIC remains visible and within the supported smallest and design layouts");
            }
            check(button->getWantsKeyboardFocus() && button->getTooltip().contains("FX tails")
                      && button->getTooltip().contains("not output mute")
                      && button->getTooltip().contains("New incoming notes"),
                  "PANIC is keyboard-focusable and explains the retained-tail and new-note policy");
            juce::AudioBuffer<float> audio(2, blockSize);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(16, 64, (juce::uint8) 100), 0);
            host.processBlock(audio, midi);
            midi.clear();
            for (int click = 0; click < 3; ++click)
                button->triggerClick();
            flush(30);
            check(host.getActiveVoiceCount() > 0 && host.getEngine().activeVoices() > 0,
                  "visible PANIC button only posts a request before audio processing");
            host.processBlock(audio, midi);
            check(panic_detail::stopped(host), "visible PANIC button reaches the audio-owned global hard stop");
        }
    }
}
} // namespace cs80::tests
