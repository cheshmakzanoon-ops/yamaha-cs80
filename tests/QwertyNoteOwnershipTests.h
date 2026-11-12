#pragma once

#include "plugin/HardwareKeyboard.h"

#include <array>
#include <cmath>
#include <initializer_list>
#include <memory>
#include <set>

namespace cs80::tests
{
namespace qwerty_note_detail
{
struct Events final : juce::MidiKeyboardState::Listener
{
    std::array<int, 128> ons {}, offs {};
    void handleNoteOn(juce::MidiKeyboardState*, int, int note, float) override { ++ons[(size_t) note]; }
    void handleNoteOff(juce::MidiKeyboardState*, int, int note, float) override { ++offs[(size_t) note]; }
};

struct Keys
{
    std::set<int> down;
    explicit Keys(cs80::ui::QwertyKeyboard& q) : keyboard(q)
    {
        keyboard.isKeyDown = [this](int code) { return down.count(code) > 0; };
        keyboard.textEntryFocused = [] { return false; };
    }
    bool press(char key, int modifiers = 0)
    {
        down.insert((int) key);
        return keyboard.keyPressed({(int) key, modifiers, (juce::juce_wchar) key}, nullptr);
    }
    void lift(char key)
    {
        down.erase((int) key);
        keyboard.keyStateChanged(false, nullptr);
    }
    cs80::ui::QwertyKeyboard& keyboard;
};

inline int held(const cs80::Engine& engine, int note, int channel = 1)
{
    int result = 0;
    for (int i = 0; i < 8; ++i)
    {
        const auto voice = engine.getVoiceInfo(i);
        if (voice.held && voice.note == note && voice.channel == channel) ++result;
    }
    return result;
}
}

template <class Check>
void testQwertyNoteOwnership(Check check)
{
    using namespace qwerty_note_detail;
    // Real key handlers and MidiKeyboardState callbacks, without an editor,
    // native peer, timer dispatch or an OS keyboard dependency.
    for (bool octaveUp : {false, true})
    for (bool releaseFirst : {false, true})
    {
        juce::MidiKeyboardState state;
        Events events;
        state.addListener(&events);
        {
            cs80::ui::QwertyKeyboard keyboard(state);
            Keys keys(keyboard);
            const char first = octaveUp ? 'k' : 'a';
            const char second = octaveUp ? 'a' : 'k';
            const int note = octaveUp ? 72 : 60;
            check(keys.press(first) && state.isNoteOn(1, note), "first QWERTY key plays its captured pitch");
            keys.press(octaveUp ? 'x' : 'z');
            check(state.isNoteOn(1, note), "octave change does not retune the held key");
            keys.press(second);
            check(keyboard.heldCount() == 2, "both physical QWERTY keys retain ownership of the shared pitch");
            check(events.ons[(size_t) note] == 1, "overlapping QWERTY owners share one logical note-on");
            keys.press(first); keys.press(second);
            check(events.ons[(size_t) note] == 1 && keyboard.heldCount() == 2,
                  "auto-repeat remains inert for either owner after octave change");
            keys.lift(releaseFirst ? first : second);
            check(keyboard.heldCount() == 1 && state.isNoteOn(1, note), "lifting either shared owner keeps the other owner's pitch down");
            check(events.offs[(size_t) note] == 0, "non-final shared owner does not publish note-off");
            keys.lift(releaseFirst ? second : first);
            check(keyboard.heldCount() == 0 && !state.isNoteOn(1, note), "last shared owner releases its captured pitch");
            check(events.offs[(size_t) note] == 1, "last shared owner publishes exactly one note-off");
            keys.press(second);
            check(events.ons[(size_t) note] == 2 && state.isNoteOn(1, note), "a new press after all owners lift retriggers normally");
            keys.lift(second);
        }
        state.removeListener(&events);
    }

    // Lifecycle resets clear MIDI state without manufacturing physical key-ups.
    // A newly pressed alias is fresh input; still-held stale keys must neither
    // retrigger from auto-repeat nor keep/release that fresh logical press.
    for (bool releaseStaleFirst : {false, true})
    {
        juce::MidiKeyboardState state;
        Events events;
        state.addListener(&events);
        {
            cs80::ui::QwertyKeyboard keyboard(state);
            Keys keys(keyboard);
            keys.press('a'); keys.press('z'); state.reset();
            keys.press('a');
            check(events.ons[60] == 1 && !state.isNoteOn(1, 60), "stale held-key auto-repeat cannot undo a MIDI-state reset");
            keys.press('k');
            check(events.ons[60] == 2 && state.isNoteOn(1, 60), "fresh physical alias remains playable after a MIDI-state reset");
            keys.lift(releaseStaleFirst ? 'a' : 'k');
            check(state.isNoteOn(1, 60) == releaseStaleFirst, "stale pre-reset owner neither releases nor prolongs the fresh note");
            keys.lift(releaseStaleFirst ? 'k' : 'a');
            check(!state.isNoteOn(1, 60) && events.offs[60] == 1, "fresh post-reset note publishes one final release");
        }
        state.removeListener(&events);
    }

    for (int finish : {0, 1, 2, 3})
    {
        juce::MidiKeyboardState state;
        Events events;
        state.addListener(&events);
        auto keyboard = std::make_unique<cs80::ui::QwertyKeyboard>(state);
        Keys keys(*keyboard);
        keys.press('a'); keys.press('z'); keys.press('k'); keys.press('s');
        check(state.isNoteOn(1, 60) && state.isNoteOn(1, 50), "cleanup witness includes shared and independent pitches");
        if (finish == 0) keyboard->releaseAll();
        if (finish == 1) keyboard->setEnabled(false);
        if (finish == 2) keyboard.reset();
        if (finish == 3) { keys.down.clear(); keyboard->keyStateChanged(false, nullptr); }
        check(!state.isNoteOn(1, 60) && !state.isNoteOn(1, 50), "all cleanup paths release shared and independent notes");
        check(events.offs[60] == 1 && events.offs[50] == 1, "cleanup publishes one release per logical pitch");
        keyboard.reset();
        check(events.offs[60] == 1 && events.offs[50] == 1, "destruction after cleanup emits no extra releases");
        state.removeListener(&events);
    }

    {
        juce::MidiKeyboardState state;
        Events events;
        state.addListener(&events);
        {
            cs80::ui::QwertyKeyboard keyboard(state);
            Keys keys(keyboard);
            check(!keys.press('a', juce::ModifierKeys::ctrlModifier), "Ctrl shortcuts do not acquire note ownership");
            check(!keys.press('a', juce::ModifierKeys::commandModifier), "Command shortcuts do not acquire note ownership");
            check(!keys.press('a', juce::ModifierKeys::altModifier), "Alt shortcuts do not acquire note ownership");
            keyboard.textEntryFocused = [] { return true; };
            check(!keys.press('a') && keyboard.heldCount() == 0, "text entry does not acquire note ownership");
            keyboard.textEntryFocused = [] { return false; };
            keys.press('a'); keys.press('w'); keys.press('z'); keys.press('s');
            keys.lift('a');
            check(!state.isNoteOn(1, 60) && state.isNoteOn(1, 61) && state.isNoteOn(1, 50),
                  "independent pitches and pre-octave notes retain separate releases");
            const float velocity = keyboard.getVelocity();
            keys.press('c'); check(keyboard.getVelocity() < velocity, "velocity-down shortcut remains functional");
            keys.press('v'); check(std::abs(keyboard.getVelocity() - velocity) < 1.0e-6f, "velocity-up shortcut remains functional");
        }
        state.removeListener(&events);
    }

    juce::TemporaryFile fixture("-cs80-qwerty-note-ownership");
    for (bool circuit : {false, true})
    for (bool sixteen : {false, true})
    for (bool scan : {false, true})
    for (bool releaseFirst : {false, true})
    {
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        host->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        for (int parameter : std::initializer_list<int> {cs80::P_parityMode, cs80::P_mpeOn, cs80::P_arpOn,
                              cs80::P_voiceDispersion, cs80::P_drift, cs80::P_analogNoise})
            host->setParamValue(parameter, 0.0f);
        host->setParamValue(cs80::P_voiceMode, sixteen ? 1.0f : 0.0f);
        host->setParamValue(cs80::P_kasScanTiming, scan ? 1.0f : 0.0f);
        host->prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> audio(2, 256);
        juce::MidiBuffer midi;
        const auto tick = [&] { host->processBlock(audio, midi); midi.clear(); };
        tick(); host->serviceKeyboardUI();
        // Fill primary voices so the 16-voice fixture proves extension routing.
        for (int note = 40; note < (sixteen ? 48 : 41); ++note)
            midi.addEvent(juce::MidiMessage::noteOn(3, note, (juce::uint8) 100), 0);
        tick(); tick();
        const auto parameters = host->currentValues();
        cs80::ui::QwertyKeyboard keyboard(host->getKeyboardState());
        Keys keys(keyboard);
        const auto sounding = [&]() -> const cs80::Engine& {
            return sixteen ? host->getExtensionEngine() : host->getEngine();
        };
        keys.press('a'); tick(); tick();
        check(held(sounding(), 60) == 1, "first QWERTY press reaches the selected physical instrument");
        keys.press('z'); keys.press('k'); tick(); tick();
        check(held(sounding(), 60) == 1, "shared pitch does not allocate duplicate sounding voices");
        keys.lift(releaseFirst ? 'a' : 'k'); tick(); tick();
        check(held(sounding(), 60) == 1, "running engine holds shared pitch until the final physical key lifts");
        keys.lift(releaseFirst ? 'k' : 'a'); tick(); tick();
        check(held(sounding(), 60) == 0, "last physical key releases shared pitch through the audio handoff");
        check(held(host->getEngine(), 40, 3) == 1, "QWERTY ownership leaves unrelated host MIDI notes held");
        check(parameters.v == host->currentValues().v, "QWERTY overlap does not change host parameter or recall values");
    }

    for (bool circuit : {false, true})
    for (bool recall : {false, true})
    for (bool releaseStaleFirst : {false, true})
    {
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        host->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        for (int parameter : std::initializer_list<int> {cs80::P_parityMode, cs80::P_mpeOn, cs80::P_arpOn, cs80::P_kasScanTiming, cs80::P_voiceMode})
            host->setParamValue(parameter, 0.0f);
        host->prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> audio(2, 256);
        juce::MidiBuffer midi;
        const auto tick = [&] { host->processBlock(audio, midi); midi.clear(); };
        tick(); host->serviceKeyboardUI();
        const auto parameters = host->currentValues();
        juce::MemoryBlock saved;
        host->getStateInformation(saved);
        cs80::ui::QwertyKeyboard keyboard(host->getKeyboardState());
        Keys keys(keyboard);
        keys.press('a'); keys.press('z'); tick();
        check(held(host->getEngine(), 60) == 1, "pre-reset QWERTY witness is sounding");
        if (recall) host->setStateInformation(saved.getData(), (int) saved.getSize());
        else host->requestPanicFromUI();
        tick(); host->serviceKeyboardUI();
        check(held(host->getEngine(), 60) == (recall ? 1 : 0),
              "same-policy session recall preserves a held voice; panic cancels it");
        keys.press('a'); tick();
        check(held(host->getEngine(), 60) == (recall ? 1 : 0), "held-key auto-repeat changes neither recalled nor cancelled voices");
        keys.press('k'); tick();
        check(held(host->getEngine(), 60) == 1, "new alias shares a recalled voice or starts a fresh post-panic voice");
        keys.lift(releaseStaleFirst ? 'a' : 'k'); tick();
        check(held(host->getEngine(), 60) == (recall || releaseStaleFirst ? 1 : 0),
              "recalled owners keep sharing; cancelled owners cannot release or prolong a new voice");
        keys.lift(releaseStaleFirst ? 'k' : 'a'); tick();
        check(held(host->getEngine(), 60) == 0, "recalled or post-panic voice releases without a stranded key");
        check(parameters.v == host->currentValues().v, "QWERTY recovery preserves recalled host parameter values");
    }
}
} // namespace cs80::tests
