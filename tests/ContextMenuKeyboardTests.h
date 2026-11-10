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

    {
        cs80::ui::FocusSlider arrows;
        arrows.setRange(0., 1., .1); arrows.setValue(.5);
        int begin = 0, end = 0;
        arrows.onDragStart = [&] { ++begin; }; arrows.onDragEnd = [&] { ++end; };
        const juce::KeyPress right(juce::KeyPress::rightKey);
        arrows.setEnabled(false);
        check(!arrows.keyPressed(right) && arrows.getValue() == .5 && begin == 0 && end == 0,
              "disabled slider rejects arrow edits rather than falling through to JUCE");
        arrows.setEnabled(true); arrows.setValue(.5);
        {
            juce::Component parent; parent.addChildComponent(arrows); parent.setEnabled(false);
            check(!arrows.keyPressed(right) && arrows.getValue() == .5,
                  "inherited disabled slider rejects arrow edits");
            parent.removeChildComponent(&arrows);
        }
        arrows.setValue(.5);
        {
            juce::Slider::ScopedDragNotification gesture(arrows);
            check(!arrows.keyPressed(right) && arrows.getValue() == .5 && begin == 1 && end == 0,
                  "arrow edit cannot alter an in-flight pointer gesture");
        }
        arrows.setValue(.5);
        check(arrows.keyPressed(right) && arrows.getValue() > .5 && begin == 2 && end == 2,
              "fresh enabled arrow edit retains one balanced host gesture");
    }

    // Pointer commands must obey the same no-nested-edit policy as Shift+F10.
    {
        cs80::ui::FocusSlider pointerSlider;
        pointerSlider.setRange(0., 1.); pointerSlider.setValue(.5);
        int pointerMenus = 0, editors = 0, begins = 0, finishes = 0;
        pointerSlider.onContextMenu = [&] { ++pointerMenus; };
        pointerSlider.onTypeValue = [&] { ++editors; };
        pointerSlider.onDragStart = [&] { ++begins; };
        pointerSlider.onDragEnd = [&] { ++finishes; };
        auto event = [&](int mods) {
            const juce::Point<float> point(10.f, 10.f);
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
                juce::ModifierKeys(mods), 1.f, 0.f, 0.f, 0.f, 0.f, &pointerSlider, &pointerSlider,
                juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, false);
        };
        const auto popup = event(juce::ModifierKeys::rightButtonModifier);
        const auto typed = event(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::commandModifier);
        {
            juce::Slider::ScopedDragNotification gesture(pointerSlider);
            pointerSlider.mouseDown(popup);
            check(pointerMenus == 0 && pointerSlider.isGestureInProgress(),
                  "pointer context command cannot interrupt an active slider edit");
            pointerSlider.mouseDown(typed);
            check(editors == 0 && begins == 1 && finishes == 0,
                  "pointer value-entry command cannot nest an active slider edit");
        }
        check(begins == 1 && finishes == 1 && !pointerSlider.isGestureInProgress(),
              "rejected pointer commands preserve balanced existing gesture notifications");
        pointerSlider.mouseDown(popup); pointerSlider.mouseUp(popup);
        pointerSlider.mouseDown(typed); pointerSlider.mouseUp(typed);
        check(pointerMenus == 1 && editors == 1 && begins == 1 && finishes == 1,
              "fresh pointer commands remain available without starting host gestures");
    }

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
    const auto action = [](const juce::PopupMenu& menu, const juce::String& prefix) -> std::function<void()> {
        for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
            if (it.getItem().isEnabled && it.getItem().text.startsWith(prefix)) return it.getItem().action;
        return {};
    };
    const int volumeHost = host->hostParameterIndex(cs80::P_volume);
    const int mixHost = host->hostParameterIndex(cs80::P_mix);
    auto oldReset = action(knob.buildContextMenu(), "Reset to default");
    auto oldLearn = action(knob.buildContextMenu(), "MIDI learn");
    knob.bindParameter(cs80::P_mix); host->setParamValue(cs80::P_mix, .2f);
    const auto mixBefore = host->getParamValue(cs80::P_mix);
    if (oldReset) oldReset();
    check(host->getParamValue(cs80::P_mix) == mixBefore, "old knob reset cannot edit a rebound parameter");
    knob.bindParameter(cs80::P_volume); host->getMidiLearn().cancelLearn();
    if (oldLearn) oldLearn();
    check(host->getMidiLearn().learningParameter() == -1, "knob binding generation rejects A-to-B-to-A stale learn");
    host->getMidiLearn().cancelLearn();
    auto learn = action(knob.buildContextMenu(), "MIDI learn");
    host->setParamValue(cs80::P_parityMode, 1.0f);
    if (learn) learn();
    check(host->getMidiLearn().learningParameter() == -1, "already-open knob learn rechecks PURE at action time");
    host->setParamValue(cs80::P_parityMode, 0.0f);
    host->getMidiLearn().armLearn(volumeHost);
    auto cancel = action(knob.buildContextMenu(), "Cancel MIDI learn");
    host->getMidiLearn().armLearn(mixHost); if (cancel) cancel();
    check(host->getMidiLearn().learningParameter() == mixHost, "stale knob cancel preserves a newer learning target");
    host->getMidiLearn().cancelLearn(); host->getMidiLearn().assign(42, volumeHost);
    auto forget = action(knob.buildContextMenu(), "Forget MIDI CC 42");
    host->getMidiLearn().assign(43, volumeHost); if (forget) forget();
    check(host->getMidiLearn().controllerFor(volumeHost) == 43, "stale knob forget preserves a replacement mapping");
    auto reset = action(knob.buildContextMenu(), "Reset to default");
    host->setParamValue(cs80::P_volume, .2f); const auto before = host->getParamValue(cs80::P_volume);
    knob.setEnabled(false); if (reset) reset();
    check(host->getParamValue(cs80::P_volume) == before, "disabled knob rejects an already-open reset");
    knob.setEnabled(true); host->setParamValue(cs80::P_volume, before);
    { juce::Slider::ScopedDragNotification gesture(boundSlider); if (reset) reset();
      check(host->getParamValue(cs80::P_volume) == before, "knob menu reset cannot nest an active edit gesture"); }

    host->setParamValue(cs80::P_volume,before);
    { juce::Component disabledParent; disabledParent.addChildComponent(knob); disabledParent.setEnabled(false);
      if (reset) reset();
      check(host->getParamValue(cs80::P_volume)==before, "inherited disablement rejects delayed knob reset");
      disabledParent.removeChildComponent(&knob); }
    host->setParamValue(cs80::P_volume,before); boundSlider.setEnabled(false); if (reset) reset();
    check(host->getParamValue(cs80::P_volume)==before, "disabled inner slider rejects delayed knob reset");
    boundSlider.setEnabled(true);
    auto freshReset = action(knob.buildContextMenu(), "Reset to default");
    if (freshReset) freshReset();
    check(host->getParamValue(cs80::P_volume) == cs80::paramDef(cs80::P_volume).def,
          "fresh enabled knob reset still edits the bound parameter");
    host->getMidiLearn().clearParameter(volumeHost); host->getMidiLearn().cancelLearn();
    auto freshLearn = action(knob.buildContextMenu(), "MIDI learn"); if (freshLearn) freshLearn();
    check(host->getMidiLearn().learningParameter() == volumeHost, "fresh knob learn arms its actual parameter");
    auto freshCancel = action(knob.buildContextMenu(), "Cancel MIDI learn"); if (freshCancel) freshCancel();
    check(host->getMidiLearn().learningParameter() == -1, "fresh knob cancellation remains effective");
    host->getMidiLearn().assign(44,volumeHost);
    auto freshForget = action(knob.buildContextMenu(), "Forget MIDI CC 44"); if (freshForget) freshForget();
    check(host->getMidiLearn().controllerFor(volumeHost) == -1, "fresh knob forget remains effective");
    auto doomed = std::make_unique<cs80::ui::ParamKnob>(*host,cs80::P_volume);
    auto destroyedLearn = action(doomed->buildContextMenu(), "MIDI learn"); doomed.reset();
    if (destroyedLearn) destroyedLearn();
    check(host->getMidiLearn().learningParameter() == -1, "deleted knob rejects delayed actions safely");

    juce::Component top; top.setBounds(0,0,600,300); top.addAndMakeVisible(knob);
    knob.setBounds(10,10,120,100);
    const auto editorIn = [&]() -> juce::TextEditor* {
        for (auto* child : top.getChildren()) if (auto* text=dynamic_cast<juce::TextEditor*>(child)) return text;
        return nullptr;
    };
    knob.showValueEntry(); auto* oldEditor=editorIn();
    check(oldEditor!=nullptr,"typed editor fixture opens without a native popup");
    auto oldCommit=oldEditor ? oldEditor->onReturnKey : std::function<void()> {};
    if (oldEditor) oldEditor->setText(cs80::ui::formatParamValue(cs80::P_volume,.2f),juce::dontSendNotification);
    knob.bindParameter(cs80::P_ringDepth); host->setParamValue(cs80::P_ringDepth,.7f);
    const auto targetBeforeTyped=host->getParamValue(cs80::P_ringDepth);
    check(editorIn()==nullptr,"rebinding cancels the old parameter's inline editor");
    if(oldCommit) oldCommit();
    check(host->getParamValue(cs80::P_ringDepth)==targetBeforeTyped,"old inline Return cannot edit a rebound parameter");
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    knob.showValueEntry(); auto* closing=editorIn();
    auto oldClose=closing ? closing->onFocusLost : std::function<void()> {};
    if(oldClose) oldClose();
    knob.showValueEntry(); juce::Component::SafePointer<juce::TextEditor> replacement(editorIn());
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(replacement!=nullptr && editorIn()==replacement.getComponent(),"old queued close cannot dismiss a replacement editor");
    if(editorIn()==nullptr) knob.showValueEntry();
    auto* current=editorIn(); auto commit=current ? current->onReturnKey : std::function<void()> {};
    if(current) current->setText(cs80::ui::formatParamValue(cs80::P_ringDepth,.3f),juce::dontSendNotification);
    knob.setEnabled(false); if(commit) commit();
    check(host->getParamValue(cs80::P_ringDepth)==targetBeforeTyped,"disabled inline editor cannot commit a parameter edit");
    knob.setEnabled(true); juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    knob.showValueEntry(); current=editorIn();
    if(current) {current->setText(cs80::ui::formatParamValue(cs80::P_ringDepth,.3f),juce::dontSendNotification); current->onReturnKey();}
    check(std::abs(host->getParamValue(cs80::P_ringDepth)-.3f)<1.e-6f,"fresh inline Return still commits the current parameter");
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(editorIn()==nullptr,"successful current edit closes its own inline editor");
    top.removeChildComponent(&knob);

    struct ResetProbe : juce::AudioProcessorParameter::Listener {
        std::vector<int> events;
        void parameterValueChanged(int,float) override { events.push_back(0); }
        void parameterGestureChanged(int,bool start) override { events.push_back(start?1:-1); }
    } resetProbe;
    auto* resetParameter=host->getAPVTS().getParameter(Cs80AudioProcessor::parameterId(cs80::P_ringDepth));
    resetParameter->addListener(&resetProbe);
    const auto mouseDouble = [](juce::Component& c,int modifiers) {
        const juce::Point<float> point(2.0f,2.0f);
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(modifiers),
            1.0f,0.0f,0.0f,0.0f,0.0f,&c,&c,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),2,false);
    };
    const auto resetFixture = [&] {host->setParamValue(cs80::P_ringDepth,.2f);resetProbe.events.clear();};
    resetFixture(); knob.mouseDoubleClick(mouseDouble(knob,juce::ModifierKeys::leftButtonModifier));
    check(host->getParamValue(cs80::P_ringDepth)==cs80::paramDef(cs80::P_ringDepth).def
              && resetProbe.events==std::vector<int>({1,0,-1}),"caption double-click reset is one balanced host edit");
    resetFixture(); knob.mouseDoubleClick(mouseDouble(knob,juce::ModifierKeys::rightButtonModifier));
    check(host->getParamValue(cs80::P_ringDepth)==.2f && resetProbe.events.empty(),"popup double-click cannot reset the caption parameter");
    resetFixture(); knob.setEnabled(false); knob.mouseDoubleClick(mouseDouble(knob,juce::ModifierKeys::leftButtonModifier));
    check(host->getParamValue(cs80::P_ringDepth)==.2f && resetProbe.events.empty(),"disabled caption cannot reset its parameter");
    knob.setEnabled(true); resetFixture();
    {juce::Slider::ScopedDragNotification gesture(boundSlider);
     knob.mouseDoubleClick(mouseDouble(knob,juce::ModifierKeys::leftButtonModifier));}
    check(host->getParamValue(cs80::P_ringDepth)==.2f && resetProbe.events==std::vector<int>({1,-1}),
          "caption reset cannot nest or modify an active edit");
    resetFixture(); knob.mouseDoubleClick(mouseDouble(boundSlider,juce::ModifierKeys::leftButtonModifier));
    check(host->getParamValue(cs80::P_ringDepth)==.2f && resetProbe.events.empty(),"slider-origin double-click is not duplicated by its parent");
    resetParameter->removeListener(&resetProbe);

}
} // namespace cs80::tests
