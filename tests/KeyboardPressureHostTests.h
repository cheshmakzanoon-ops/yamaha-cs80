#pragma once
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <chrono>
#include <cstdio>

namespace cs80::tests
{
namespace pressure_detail
{
inline float pressure(Cs80AudioProcessor& host, int note, int channel = 1, bool heldOnly = true)
{
    for (int i = 0; i < 8; ++i)
    {
        const auto v = host.getEngine().getVoiceInfo(i);
        if (v.active && (!heldOnly || v.held) && v.note == note && v.channel == channel) return v.pressure;
    }
    return -1.0f;
}
inline juce::MidiKeyboardComponent* keyboard(juce::Component& root)
{
    if (auto* result = dynamic_cast<juce::MidiKeyboardComponent*>(&root)) return result;
    for (auto* child : root.getChildren()) if (auto* result = keyboard(*child)) return result;
    return nullptr;
}
}

// Real processor dispatch, including zero-frame callbacks; no synthetic direct
// pressure-to-host mailbox. All storage paths are isolated unused temp fixtures.
template <class Check>
void testKeyboardPressureOwnershipHost(Check check)
{
    juce::TemporaryFile fixture("-cs80-pressure-host");
    for (int frames : { 0, 32 })
    {
        Cs80AudioProcessor host(fixture.getFile());
        // Plan 1.7 (ws2): fresh instances default to the circuit engine; this fixture
        // exercises the legacy-start contract, so it selects Legacy (compat) explicitly.
        host.setEngineMode(Cs80AudioProcessor::EngineMode::legacy);
        host.setParamValue(P_analogNoise, 0.0f);
        host.setParamValue(P_mpeOn, 0.0f);
        host.setParamValue(P_masterSustain, 0.0f);
        host.setParamValue(chParam(0, CP_vR), 0.5f);
        host.setParamValue(chParam(1, CP_vR), 0.5f);
        host.prepareToPlay(48000.0, 32);
        host.serviceKeyboardUI();
        juce::AudioBuffer<float> audio(2, frames);
        juce::MidiBuffer midi;
        auto tick = [&] { host.processBlock(audio, midi); midi.clear(); };
        auto strike = [&](int note)
        { return host.captureKeyboardGesture(1, note, [&] { host.getKeyboardState().noteOn(1, note, 0.8f); }); };
        auto hostOn = [&](int note, int channel = 1)
        { midi.addEvent(juce::MidiMessage::noteOn(channel, note, (juce::uint8) 100), 0); };
        auto reset = [&]
        {
            host.requestPanicFromUI(); tick(); host.serviceKeyboardUI();
        };
        const auto old = strike(60);
        host.setKeyPressureFromUI(old, 0.2f);
        host.getKeyboardState().noteOff(1, 60, 0.0f);
        const auto fresh = strike(60);
        host.setKeyPressureFromUI(old, 0.9f);
        tick();
        check(old && fresh && !(old == fresh) && pressure_detail::pressure(host, 60) == 0.0f,
              "same-epoch release/restrike rejects old gesture pressure after the fresh onset");
        host.setKeyPressureFromUI(fresh, 0.35f); tick();
        check(std::abs(pressure_detail::pressure(host, 60) - 0.35f) < 1e-6f,
              "fresh same-pitch UI gesture remains usable beside attributed UI release tails");
        reset();
        const auto zeroVelocity = host.captureKeyboardGesture(1, 60, [&]
        { host.getKeyboardState().noteOn(1, 60, 0.001f); });
        tick();
        check(!zeroVelocity && pressure_detail::pressure(host, 60) < 0.0f,
              "JUCE velocity quantization to note-off never supplies a pressure onset token");

        for (int lifecycle = 0; lifecycle < 3; ++lifecycle)
        {
            reset(); const auto token = strike(60); tick();
            if (lifecycle == 0) host.prepareToPlay(48000.0, 32);
            if (lifecycle == 1) { host.releaseResources(); host.prepareToPlay(48000.0, 32); }
            if (lifecycle == 2) { host.requestPanicFromUI(); tick(); }
            host.serviceKeyboardUI();
            hostOn(60); tick();
            check(!host.setKeyPressureFromUI(token, 0.95f), "late pressure token is rejected after lifecycle reset and UI acknowledgment");
            tick();
            check(pressure_detail::pressure(host, 60) == 0.0f, "old UI gesture cannot pressure a fresh same-pitch host voice");
        }
        for (int mode : { P_sourcePerformance, P_parityMode })
        {
            reset();
            const auto token = strike(60); host.setKeyPressureFromUI(token, 0.8f);
            host.setParamValue(mode, 1.0f); hostOn(64); tick();
            check(pressure_detail::pressure(host, 60) < 0.0f && pressure_detail::pressure(host, 64) == 0.0f,
                  "effective mode transition discards captured old UI onset/pressure but accepts fresh host MIDI");
            host.serviceKeyboardUI();
            check(!host.setKeyPressureFromUI(token, 1.0f), "effective mode transition revokes the old token after acknowledgment");
            const auto next = strike(67); host.setKeyPressureFromUI(next, 0.4f); tick();
            check(std::abs(pressure_detail::pressure(host, 67) - 0.4f) < 1e-6f,
                  "fresh post-mode-ack UI gesture retains pressure control");
            host.setParamValue(mode, 0.0f); tick(); host.serviceKeyboardUI();
        }

        reset();
        for (int note = 60; note < 68; ++note) hostOn(note);
        tick();
        const auto refused = strike(60); host.setKeyPressureFromUI(refused, 0.9f); tick();
        check(refused && host.getEngine().activeVoices() == 8 && pressure_detail::pressure(host, 60) == 0.0f,
              "queue-accepted ninth onset never claims or pressures a refused host voice");
        reset();
        for (int note = 60; note < 68; ++note) strike(note);
        tick(); hostOn(60); tick(); // All eight are held: host allocation is refused.
        host.getKeyboardState().noteOff(1, 60, 0.0f); tick();
        check(pressure_detail::pressure(host, 60) < 0.0f,
              "refused ninth host onset cannot revoke the accepted UI key release and strand its voice");

        for (bool released : { false, true })
        {
            reset(); hostOn(60, 2); tick();
            if (released) { midi.addEvent(juce::MidiMessage::noteOff(2, 60), 0); tick(); }
            const auto ui = strike(60); host.setKeyPressureFromUI(ui, 0.7f); tick();
            check(pressure_detail::pressure(host, 60) == 0.0f
                      && pressure_detail::pressure(host, 60, 2, false) == 0.0f,
                  "UI pressure conservatively avoids cross-channel host pitch aliases including release tails");
            midi.addEvent(juce::MidiMessage::aftertouchChange(2, 60, 71), 0); tick();
            check(std::abs(pressure_detail::pressure(host, 60, 2, false) - 71.0f / 127.0f) < 1e-6f,
                  "host MIDI poly-aftertouch keeps its existing pitch-group semantics");
        }
        reset();
        const auto superseded = strike(60); tick(); hostOn(60); tick();
        host.setKeyPressureFromUI(superseded, 0.8f); tick();
        bool unchanged = true;
        for (int i = 0; i < 8; ++i)
        { const auto v = host.getEngine().getVoiceInfo(i); if (v.active && v.note == 60) unchanged &= v.pressure == 0.0f; }
        check(unchanged, "later host onset revokes the earlier UI gesture before pressure dispatch");
        for (int cc : { 120, 123 })
        {
            reset(); const auto channelMode = strike(60); tick();
            midi.addEvent(juce::MidiMessage::controllerEvent(1, cc, 0), 0); tick();
            hostOn(60); tick(); host.setKeyPressureFromUI(channelMode, 0.9f); tick();
            check(pressure_detail::pressure(host, 60) == 0.0f,
                  "host CC120/123 revokes UI pressure before a subsequent same-pitch host strike");
        }

        reset();
        const auto beforeArp = strike(60); tick();
        host.setParamValue(P_arpOn, 1.0f); tick();
        const auto duringArp = strike(62); tick();
        host.setKeyPressureFromUI(beforeArp, 0.9f); host.setKeyPressureFromUI(duringArp, 0.9f); tick();
        host.setParamValue(P_arpOn, 0.0f); tick();
        host.setKeyPressureFromUI(duringArp, 0.9f); tick();
        bool arpSafe = true;
        for (int i = 0; i < 8; ++i)
        { const auto v = host.getEngine().getVoiceInfo(i); if (v.active) arpSafe &= v.pressure == 0.0f; }
        check(arpSafe, "effective arp and both arp edges revoke direct-pointer pressure attribution");

        reset(); hostOn(72, 2); tick();
        const auto flood = strike(60); tick();
        bool overflow = false;
        for (int i = 0; i < 300; ++i) overflow |= !host.setKeyPressureFromUI(flood, 0.5f);
        tick();
        check(overflow && pressure_detail::pressure(host, 60) < 0.0f
                  && pressure_detail::pressure(host, 72, 2) == 0.0f,
              "bounded pressure flood uses UI-only recovery and preserves unrelated host-held notes");
        host.serviceKeyboardUI();
        check(!host.setKeyPressureFromUI(flood, 0.9f), "overflow acknowledgment does not revive an old gesture");
    }
}

template <class Check>
void testKeyboardPointerPressure(Check check)
{
    juce::TemporaryFile fixture("-cs80-pressure-pointer");
    Cs80AudioProcessor host(fixture.getFile());
    host.prepareToPlay(48000.0, 32);
    Cs80AudioProcessorEditor editor(host);
    // Keep the supported minimum layout, not the screenshot's 1600x1000.
    // On smaller desktops the upper panel may extend off-screen deliberately:
    // this fixture exposes its tested keybed, not a claim that the whole UI fits.
    editor.setSize(1120, 700);
    const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    check(display != nullptr, "pointer-pressure fixture has a real display for native pointer coordinates");
    const auto usable = display != nullptr ? display->userArea : juce::Rectangle<int>();
    editor.setTopLeftPosition(usable.getX() + (usable.getWidth() - editor.getWidth()) / 2,
                             usable.getBottom() - editor.getHeight());
    // JUCE keyboard hit testing calls reallyContains(), which recurses through
    // Component::contains to the top-level native peer. A detached component
    // cannot receive a real pointer note, even when its bounds look correct.
    editor.addToDesktop(0);
    editor.setVisible(true);
    editor.toFront(true);
    auto* keyboard = pressure_detail::keyboard(editor);
    check(keyboard != nullptr, "actual editor contains the pointer-pressure keyboard");
    if (keyboard == nullptr) return;
    check(editor.getPeer() != nullptr && editor.isShowing() && keyboard->isShowing(),
          "pointer-pressure fixture owns a visible native editor peer for JUCE hit testing");
    auto positionFor = [&](int note, float depth)
    {
        const auto key = keyboard->getRectangleForKey(note);
        return juce::Point<float>(key.getCentreX(), keyboard->getHeight() * depth);
    };
    auto nativePointsReady = [&]
    {
        for (int note : { 60, 62, 64 })
            for (float depth : { 0.65f, 0.85f, 0.95f })
            {
                const auto position = positionFor(note, depth);
                if (!usable.contains(keyboard->localPointToGlobal(position.roundToInt()))
                    || keyboard->getNoteAndVelocityAtPosition(position).note != note) return false;
            }
        return true;
    };
    // macOS additionally checks the actual frontmost NSWindow at each screen
    // point. Let native realization/ordering settle through the normal loop;
    // do not bypass hit testing or inject a pretend note into the keyboard.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!nativePointsReady() && std::chrono::steady_clock::now() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    check(nativePointsReady(), "pointer-pressure native window is frontmost at all tested on-screen key points");
    const auto firstScreenPoint = keyboard->localPointToGlobal(positionFor(60, 0.65f).roundToInt());
    std::printf("Pointer fixture: editor=%d,%d %dx%d; usable=%d,%d %dx%d; key60 screen=%d,%d hit=%d\n",
                editor.getX(), editor.getY(), editor.getWidth(), editor.getHeight(),
                usable.getX(), usable.getY(), usable.getWidth(), usable.getHeight(),
                firstScreenPoint.x, firstScreenPoint.y,
                keyboard->getNoteAndVelocityAtPosition(positionFor(60, 0.65f)).note);
    juce::AudioBuffer<float> audio(2, 0);
    juce::MidiBuffer midi;
    auto tick = [&] { host.processBlock(audio, midi); midi.clear(); };
    auto event = [&](int note, float depth, bool down)
    {
        const auto position = positionFor(note, depth);
        check(usable.contains(keyboard->localPointToGlobal(position.roundToInt())),
              "pointer-pressure fixture event lies inside the actual usable display");
        check(keyboard->getNoteAndVelocityAtPosition(position).note == note,
              "pointer-pressure fixture point resolves to the intended key through real JUCE hit testing");
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position,
            juce::ModifierKeys(down ? juce::ModifierKeys::leftButtonModifier : 0),
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, keyboard, keyboard, juce::Time::getCurrentTime(),
            position, juce::Time::getCurrentTime(), 1, true);
    };
    keyboard->mouseDown(event(60, 0.65f, true));
    keyboard->mouseDrag(event(60, 0.85f, true)); tick();
    check(std::abs(pressure_detail::pressure(host, 60) - 0.5f) < 1e-5f,
          "real JUCE mouse-down onset capture preserves first same-block pointer pressure");
    keyboard->mouseDrag(event(62, 0.65f, true));
    keyboard->mouseDrag(event(62, 0.85f, true)); tick();
    check(std::abs(pressure_detail::pressure(host, 62) - 0.5f) < 1e-5f
              && pressure_detail::pressure(host, 60) < 0.0f,
          "real cross-key drag captures the new JUCE onset rather than reusing the old token");
    keyboard->mouseExit(event(62, 0.85f, true));
    keyboard->mouseEnter(event(62, 0.65f, true));
    keyboard->mouseDrag(event(62, 0.65f, true));
    keyboard->mouseDrag(event(62, 0.85f, true)); tick();
    check(std::abs(pressure_detail::pressure(host, 62) - 0.5f) < 1e-5f,
          "real same-pitch exit/re-entry captures a fresh onset after JUCE releases the previous key");
    host.requestPanicFromUI(); tick(); host.serviceKeyboardUI();
    midi.addEvent(juce::MidiMessage::noteOn(1, 62, (juce::uint8) 100), 0); tick();
    keyboard->mouseDrag(event(62, 0.95f, true)); tick();
    check(pressure_detail::pressure(host, 62) == 0.0f,
          "ongoing same-key pointer drag cannot adopt a fresh host note after reset and acknowledgment");
    keyboard->mouseUp(event(62, 0.95f, false)); tick();
    keyboard->mouseDown(event(64, 0.65f, true));
    keyboard->mouseDrag(event(64, 0.85f, true)); tick();
    check(std::abs(pressure_detail::pressure(host, 64) - 0.5f) < 1e-5f,
          "lifting and making a genuinely fresh pointer strike restores pressure after reset");
    keyboard->mouseUp(event(64, 0.85f, false)); tick();
}
}
