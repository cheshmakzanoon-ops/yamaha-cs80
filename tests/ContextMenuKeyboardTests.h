#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/UiComponents.h"

namespace cs80::tests
{
// Exercise real JUCE components and slider gesture notifications, but do not
// create a native peer or open a menu. Popup rendering/focus still belongs to
// the full UI smoke gate on a display-capable host.
template <class Check>
void testContextMenuKeyboard(Check check)
{
    cs80::ui::FocusSlider slider;
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setRange(0.0, 1.0, 0.1);
    slider.setValue(0.5, juce::dontSendNotification);
    int menus = 0, help = 0, starts = 0, ends = 0;
    slider.onContextMenu = [&] { ++menus; };
    slider.onHelp = [&] { ++help; };
    slider.onDragStart = [&] { ++starts; };
    slider.onDragEnd = [&] { ++ends; };
    const juce::KeyPress shortcut(juce::KeyPress::F10Key, juce::ModifierKeys::shiftModifier, 0);

    check(slider.keyPressed(shortcut) && menus == 1,
          "Shift+F10 dispatches the focused slider's existing context menu");
    check(slider.getValue() == 0.5 && starts == 0 && ends == 0 && !slider.isGestureInProgress(),
          "opening a keyboard context menu neither edits the value nor starts host automation");
    check(slider.keyPressed(shortcut) && menus == 2, "each shortcut dispatch invokes the menu exactly once");

    // Ctrl and Command may be aliases on this platform. Test each semantic
    // modifier anyway; none of them may broaden the shortcut's ownership.
    const int modifierMasks[] { 0, juce::ModifierKeys::ctrlModifier, juce::ModifierKeys::altModifier,
                          juce::ModifierKeys::commandModifier,
                          juce::ModifierKeys::shiftModifier | juce::ModifierKeys::ctrlModifier,
                          juce::ModifierKeys::shiftModifier | juce::ModifierKeys::altModifier,
                          juce::ModifierKeys::shiftModifier | juce::ModifierKeys::commandModifier };
    const int beforeGuards = menus;
    for (int modifiers : modifierMasks)
    {
        const int before = menus;
        check(!slider.keyPressed(juce::KeyPress(juce::KeyPress::F10Key, modifiers, 0)) && menus == before,
              "plain F10 and host-modified F10 combinations remain unhandled");
    }
    check(!slider.keyPressed(juce::KeyPress(juce::KeyPress::F9Key, juce::ModifierKeys::shiftModifier, 0))
              && menus == beforeGuards, "Shift with a different function key does not open the menu");

    slider.setEnabled(false);
    check(!slider.keyPressed(shortcut) && menus == beforeGuards, "disabled controls do not open or swallow context menus");
    slider.setEnabled(true);
    {
        juce::Component parent;
        parent.addChildComponent(slider);
        parent.setEnabled(false);
        check(!slider.keyPressed(shortcut) && menus == beforeGuards, "an inherited disabled state also blocks the menu");
        parent.removeChildComponent(&slider);
    }
    {
        juce::Slider::ScopedDragNotification gesture(slider);
        check(slider.isGestureInProgress() && starts == 1 && ends == 0,
              "the active-gesture fixture uses JUCE's real drag notifications");
        check(!slider.keyPressed(shortcut) && menus == beforeGuards && starts == 1 && ends == 0,
              "a keyboard menu cannot interrupt or nest an in-flight host gesture");
    }
    check(!slider.isGestureInProgress() && starts == 1 && ends == 1,
          "the existing gesture finishes normally after a rejected menu shortcut");
    check(slider.keyPressed(shortcut) && menus == beforeGuards + 1, "the menu shortcut is available again after a gesture");
    const int beforeNoCallback = menus;
    slider.onContextMenu = {};
    check(!slider.keyPressed(shortcut) && menus == beforeNoCallback, "a control without a context callback does not consume Shift+F10");

    check(slider.keyPressed(juce::KeyPress(juce::KeyPress::F1Key)) && help == 1,
          "the existing F1 help shortcut is preserved");
    check(slider.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)) && slider.getValue() > 0.5,
          "the existing plain arrow adjustment is preserved");
    check(starts == 2 && ends == 2 && !slider.isGestureInProgress(),
          "arrow adjustment still emits exactly one complete host gesture");

    juce::TemporaryFile fixture("-cs80-context-keyboard");
    auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    cs80::ui::ParamKnob knob(*host, cs80::P_mix);
    auto& boundSlider = static_cast<cs80::ui::FocusSlider&>(knob.getSlider());
    check((bool) boundSlider.onContextMenu, "a real parameter control wires the context menu callback");
    check(knob.buildTooltip(false).contains("Shift+F10") && knob.buildTooltip(true).contains("Shift+F10"),
          "both tooltip modes advertise keyboard access to the menu");
    check(boundSlider.getDescription().contains("Shift+F10"), "the accessible description advertises the menu shortcut");
    check(knob.bindParameter(cs80::P_volume) && boundSlider.getDescription().contains("Shift+F10"),
          "rebinding a parameter retains the accessible menu instructions");
}
} // namespace cs80::tests
