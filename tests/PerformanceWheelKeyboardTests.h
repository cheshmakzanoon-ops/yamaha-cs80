#pragma once

#include "plugin/PerformanceDeck.h"
#include "plugin/StudioDrawers.h"

#include <cmath>
#include <memory>
#include <vector>

namespace cs80::tests
{
namespace wheel_keyboard_detail
{
inline juce::MouseEvent mouse(juce::Component& c, float x, float y, bool down = true)
{
    const juce::Point<float> point(x, y);
    return { juce::Desktop::getInstance().getMainMouseSource(), point,
             juce::ModifierKeys(down ? juce::ModifierKeys::leftButtonModifier : 0),
             1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c,
             juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, false };
}
inline juce::KeyPress key(int code, int mods = 0) { return {code, mods, 0}; }
inline bool near(float a, float b) { return std::abs(a - b) < 1e-6f; }
}

template <class Check>
void testPerformanceWheelKeyboard(Check check)
{
    using namespace wheel_keyboard_detail;
    std::vector<float> pitchEvents, modEvents;
    cs80::ui::PerformanceWheel pitch("PITCH", true), mod("MOD", false);
    pitch.setSize(44, 176); mod.setSize(44, 176);
    pitch.onValueChange = [&](float value) { pitchEvents.push_back(value); };
    mod.onValueChange = [&](float value) { modEvents.push_back(value); };
    check(pitch.getWantsKeyboardFocus() && mod.getWantsKeyboardFocus(), "both wheel policies are keyboard focusable");
    check(!pitch.getMouseClickGrabsKeyboardFocus() && !mod.getMouseClickGrabsKeyboardFocus(),
          "pointer use does not steal focus from computer-keyboard playing");
    check(pitch.keyPressed(key(juce::KeyPress::upKey)) && near(pitch.getValue(), 0.04f), "Up moves spring pitch by two percent of its travel");
    check(pitch.keyPressed(key(juce::KeyPress::rightKey)) && near(pitch.getValue(), 0.08f), "Right also increases pitch");
    check(pitch.keyPressed(key(juce::KeyPress::leftKey, juce::ModifierKeys::shiftModifier)) && near(pitch.getValue(), 0.076f),
          "Shift makes an arrow adjustment ten times finer");
    check(!pitch.keyStateChanged(false) && near(pitch.getValue(), 0.0f) && !pitchEvents.empty() && pitchEvents.back() == 0.0f,
          "releasing adjustment keys centres pitch without swallowing other key-state observers");
    const auto released = pitchEvents.size();
    pitch.keyStateChanged(false); pitch.focusLost(juce::Component::focusChangedDirectly); pitch.cancelGesture();
    check(pitchEvents.size() == released, "repeated idle cleanup does not notify again");

    check(pitch.keyPressed(key(juce::KeyPress::homeKey)) && pitch.getValue() == -1.0f, "Home reaches the spring lower limit");
    check(pitch.keyPressed(key(juce::KeyPress::endKey)) && pitch.getValue() == 1.0f, "End reaches the spring upper limit");
    check(pitch.keyPressed(key(juce::KeyPress::escapeKey)) && pitch.getValue() == 0.0f, "Escape releases an active keyboard bend");
    check(!pitch.keyPressed(key(juce::KeyPress::escapeKey)), "idle Escape remains available to the editor");
    pitch.keyPressed(key(juce::KeyPress::downKey));
    pitch.focusLost(juce::Component::focusChangedByTabKey);
    check(pitch.getValue() == 0.0f, "Tab focus loss releases keyboard-owned pitch");

    const auto beforeGuards = pitchEvents.size();
    const int modifierMasks[] {juce::ModifierKeys::ctrlModifier, juce::ModifierKeys::altModifier,
                              juce::ModifierKeys::commandModifier,
                              juce::ModifierKeys::shiftModifier | juce::ModifierKeys::ctrlModifier};
    for (int modifiers : modifierMasks)
        check(!pitch.keyPressed(key(juce::KeyPress::upKey, modifiers)) && pitchEvents.size() == beforeGuards,
              "host-modified arrows are neither changed nor swallowed");
    for (int code : {juce::KeyPress::tabKey, juce::KeyPress::F1Key, static_cast<int>('A')})
        check(!pitch.keyPressed(key(code)) && pitchEvents.size() == beforeGuards, "unrelated keys retain their existing route");
    pitch.setEnabled(false);
    check(!pitch.keyPressed(key(juce::KeyPress::upKey)) && pitchEvents.size() == beforeGuards, "disabled wheels do not accept keyboard edits");
    pitch.setEnabled(true);
    {
        juce::Component parent; parent.addChildComponent(pitch); parent.setEnabled(false);
        check(!pitch.keyPressed(key(juce::KeyPress::upKey)) && pitchEvents.size() == beforeGuards, "inherited disabled state also blocks keyboard edits");
        parent.setEnabled(true);
        pitch.keyPressed(key(juce::KeyPress::endKey)); parent.setEnabled(false);
        check(pitch.getValue() == 0.0f, "disabling an ancestor releases an active keyboard bend");
        parent.removeChildComponent(&pitch);
    }

    pitch.setValue(0.7f); const auto idle = pitchEvents.size();
    pitch.focusLost(juce::Component::focusChangedDirectly); pitch.cancelGesture();
    check(pitchEvents.size() == idle && near(pitch.getValue(), 0.7f), "idle display updates never acquire keyboard ownership");
    pitch.setValue(0.0f);
    pitch.keyPressed(key(juce::KeyPress::upKey));
    pitch.mouseDown(mouse(pitch, 20, 0)); const auto pointerCount = pitchEvents.size();
    pitch.focusLost(juce::Component::focusChangedDirectly); pitch.keyStateChanged(false);
    check(pitchEvents.size() == pointerCount && pitch.getValue() > 0.9f, "pointer takeover is not released by stale keyboard cleanup");
    check(!pitch.keyPressed(key(juce::KeyPress::downKey)) && pitchEvents.size() == pointerCount,
          "keyboard input cannot take over an active pointer gesture");
    pitch.mouseUp(mouse(pitch, 20, 0, false));
    check(pitch.getValue() == 0.0f, "pointer release still centres pitch after keyboard takeover");
    pitch.keyPressed(key(juce::KeyPress::upKey)); const auto keyboardCount = pitchEvents.size();
    pitch.mouseUp(mouse(pitch, 20, 0, false)); pitch.mouseDrag(mouse(pitch, 20, 176));
    check(pitchEvents.size() == keyboardCount && near(pitch.getValue(), 0.04f), "stray pointer events cannot end or change a keyboard gesture");
    pitch.cancelGesture();

    check(mod.keyPressed(key(juce::KeyPress::upKey)) && near(mod.getValue(), 0.02f), "latched wheel uses the same two-percent travel step");
    mod.keyPressed(key(juce::KeyPress::rightKey, juce::ModifierKeys::shiftModifier));
    check(near(mod.getValue(), 0.022f), "latched fine step is two-tenths of a percent");
    const auto modCount = modEvents.size();
    mod.keyStateChanged(false); mod.focusLost(juce::Component::focusChangedByTabKey); mod.cancelGesture();
    check(modEvents.size() == modCount && near(mod.getValue(), 0.022f), "latched value survives key release and focus loss without an extra event");
    mod.keyPressed(key(juce::KeyPress::homeKey)); check(mod.getValue() == 0.0f, "latched Home selects zero");
    mod.keyPressed(key(juce::KeyPress::endKey)); check(mod.getValue() == 1.0f, "latched End selects full travel");
    for (int i = 0; i < 100; ++i) mod.keyPressed(key(juce::KeyPress::upKey));
    check(mod.getValue() == 1.0f, "auto-repeat cannot exceed the upper limit");
    for (int i = 0; i < 100; ++i) mod.keyPressed(key(juce::KeyPress::downKey));
    check(mod.getValue() == 0.0f, "auto-repeat cannot exceed the lower limit");

    juce::TemporaryFile fixture("-cs80-wheel-keyboard");
    for (bool circuit : {false, true})
    {
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        host->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        const int disabled[] {cs80::P_parityMode, cs80::P_mpeOn, cs80::P_arpOn, cs80::P_kasScanTiming,
                              cs80::P_voiceDispersion, cs80::P_drift, cs80::P_initBend, cs80::P_portamento};
        for (int p : disabled)
            host->setParamValue(p, 0.0f);
        host->setParamValue(cs80::P_pitchBendRange, 12.0f); host->prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> audio(2, 256); juce::MidiBuffer midi;
        const auto settle = [&] { for (int i = 0; i < 16; ++i) { host->processBlock(audio, midi); midi.clear(); } };
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 0); settle();
        const float base = host->getEngine().getRankInfo(0, 0).oscillatorHz;
        const auto params = host->currentValues();
        auto strip = std::make_unique<cs80::ui::ControllersStrip>(*host); strip->setVisible(true);
        auto& wheel = strip->getPitchWheel();
        check(wheel.getDescription().contains("Keyboard:") && strip->getModWheel().getDescription().contains("Keyboard:"),
              "real controller descriptions retain discoverable keyboard instructions");
        wheel.keyPressed(key(juce::KeyPress::endKey)); settle();
        check(host->getEngine().getRankInfo(0, 0).oscillatorHz > base * 1.5f, "keyboard pitch reaches the running engine");
        wheel.keyStateChanged(false); settle();
        check(std::abs(host->getEngine().getRankInfo(0, 0).oscillatorHz / base - 1.0f) < 0.005f, "keyboard release restores the running engine's pitch");
        wheel.keyPressed(key(juce::KeyPress::endKey)); strip->setVisible(false); settle();
        check(std::abs(host->getEngine().getRankInfo(0, 0).oscillatorHz / base - 1.0f) < 0.005f, "strip hide cancels keyboard ownership through the existing lifecycle path");
        strip->setVisible(true); wheel.keyPressed(key(juce::KeyPress::endKey)); strip.reset(); settle();
        check(std::abs(host->getEngine().getRankInfo(0, 0).oscillatorHz / base - 1.0f) < 0.005f, "owner destruction also releases keyboard-owned pitch");
        auto deck = std::make_unique<cs80::panel::PerformanceDeck>(*host); auto& pedal = deck->getExpressionPedal();
        check(pedal.getWantsKeyboardFocus() && pedal.getDescription().contains("Keyboard:"), "the expression pedal is keyboard reachable and describes its controls");
        pedal.keyPressed(key(juce::KeyPress::homeKey)); pedal.keyStateChanged(false); deck->refresh();
        check(host->getExpressionValue() == 0.0f && pedal.getValue() == 0.0f, "keyboard expression reaches the physical pedal API and survives refresh");
        pedal.keyPressed(key(juce::KeyPress::endKey)); pedal.keyPressed(key(juce::KeyPress::downKey));
        deck.reset(); settle();
        check(near(host->getExpressionValue(), 0.98f), "closing the keyboard-adjusted pedal retains its latched position");
        const auto after = host->currentValues(); bool unchanged = true;
        for (int p = 0; p < cs80::P_numParams; ++p) unchanged &= after[p] == params[p];
        check(unchanged && host->getEngine().getVoiceInfo(0).held, "performance keyboard controls do not edit host parameters or release the held host note");
    }
}
} // namespace cs80::tests
