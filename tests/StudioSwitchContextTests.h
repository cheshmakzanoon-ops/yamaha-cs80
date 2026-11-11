#pragma once

#include "plugin/ParameterContextButton.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace cs80::tests
{
namespace studio_switch_detail
{
inline std::function<void()> action(const juce::PopupMenu& menu, const juce::String& prefix)
{
    for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
        if (it.getItem().text.startsWith(prefix) && it.getItem().isEnabled) return it.getItem().action;
    return {};
}
struct Probe : juce::AudioProcessorParameter::Listener
{
    std::vector<int> events;
    void parameterValueChanged(int, float) override { events.push_back(0); }
    void parameterGestureChanged(int, bool start) override { events.push_back(start ? 1 : -1); }
};
inline juce::MouseEvent mouse(juce::Component& c, int modifiers, bool rightHalf = false)
{
    const juce::Point<float> p(rightHalf ? (float)c.getWidth() - 10.0f : 10.0f, 10.0f);
    return {juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys(modifiers),
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, juce::Time::getCurrentTime(),
            p, juce::Time::getCurrentTime(), 1, false};
}
inline bool pump(const std::function<bool()>& done)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(750);
    while (!done() && std::chrono::steady_clock::now() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    return done();
}
inline void paint(juce::Component& c)
{
    // JUCE's queued button click releases its visual down state after a frame.
    // Render the real component offscreen; no native peer or OS event is used.
    juce::Image image(juce::Image::ARGB, c.getWidth(), c.getHeight(), true);
    juce::Graphics g(image); c.paintEntireComponent(g, true);
}
inline void click(ui::ParameterContextButton& b, bool rightHalf = false)
{
    b.mouseDown(mouse(b, juce::ModifierKeys::leftButtonModifier, rightHalf));
    b.mouseUp(mouse(b, 0, rightHalf));
}
struct Presented : ui::ParameterContextButton
{
    Presented() : ParameterContextButton("Test switch")
    {
        setSize(236, 40); setVisible(true); setClickingTogglesState(true);
    }
    void paintButton(juce::Graphics&, bool, bool) override {}
    int menus = 0;
    juce::PopupMenu context;
    std::function<void()> dismissed;
    void presentContextMenu(juce::PopupMenu menu, std::function<void()> complete) override
    {
        ++menus; context = std::move(menu); dismissed = std::move(complete);
    }
    void dismiss()
    {
        auto complete = std::move(dismissed);
        if (complete) complete();
    }
};
struct Bound
{
    Bound(Cs80AudioProcessor& host, int core)
    {
        button = std::make_unique<Presented>(); rebind(host, core);
    }
    void rebind(Cs80AudioProcessor& host, int core)
    {
        attachment.reset();
        const auto id = Cs80AudioProcessor::parameterId(core);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(host.getAPVTS(), id, *button);
        button->bindParameterContext(host, id);
    }
    std::unique_ptr<Presented> button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
}

template<class Check>
void testStudioSwitchContext(Check check)
{
    using namespace studio_switch_detail;
    juce::TemporaryFile fixture("-cs80-studio-switch-context");
    auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    host->setParamValue(P_parityMode, 0.0f);
    const auto id = Cs80AudioProcessor::parameterId(P_outputMode);
    const auto otherId = Cs80AudioProcessor::parameterId(P_subOriginalMode);
    auto* p = host->getAPVTS().getParameter(id);
    const int target = p->getParameterIndex();
    const int other = host->getAPVTS().getParameter(otherId)->getParameterIndex();
    const juce::KeyPress shortcut(juce::KeyPress::F10Key, juce::ModifierKeys::shiftModifier, 0);
    Bound bound(*host, P_outputMode);
    auto& b = *bound.button;
    Probe probe; p->addListener(&probe);
    p->setValueNotifyingHost(0.0f); probe.events.clear();
    b.mouseDown(mouse(b, juce::ModifierKeys::rightButtonModifier));
    b.mouseDrag(mouse(b, juce::ModifierKeys::rightButtonModifier)); b.mouseUp(mouse(b, 0));
    check(b.menus == 1 && !b.isDown() && p->getValue() == 0.0f && probe.events.empty(),
          "explicit right sequence opens one context without ordinary activation or gesture");
    click(b); b.showContextMenu();
    check(!b.keyPressed(shortcut) && !b.keyPressed(juce::KeyPress(juce::KeyPress::returnKey))
              && !b.bindParameterContext(*host, otherId) && b.menus == 1 && probe.events.empty(),
          "active context blocks ordinary input, duplicate contexts and rebinding");
    auto reset = action(b.context, "Reset to default");
    check((bool)reset, "presented menu has a reset callback");
    if (reset) reset();
    check(p->getValue() == 1.0f && probe.events == std::vector<int>({1, 0, -1}),
          "own context action may run before dismissal with one complete gesture");
    b.dismiss(); probe.events.clear();
    check(b.keyPressed(shortcut) && b.menus == 2 && probe.events.empty(), "exact Shift+F10 opens the same context");
    b.dismiss();
    for (int modifiers : {0, (int)juce::ModifierKeys::ctrlModifier, (int)juce::ModifierKeys::altModifier,
                          (int)juce::ModifierKeys::commandModifier,
                          juce::ModifierKeys::shiftModifier | juce::ModifierKeys::ctrlModifier,
                          juce::ModifierKeys::shiftModifier | juce::ModifierKeys::altModifier,
                          juce::ModifierKeys::shiftModifier | juce::ModifierKeys::commandModifier})
        check(!b.keyPressed(juce::KeyPress(juce::KeyPress::F10Key, modifiers, 0)) && b.menus == 2,
              "plain and host-modified F10 remain unhandled");
    check(!b.keyPressed(juce::KeyPress(juce::KeyPress::F1Key)) && !action(b.buildContextMenu(), "Type")
              && !action(b.buildContextMenu(), "What is"), "switch menus do not invent a continuous editor or generic help");

    reset = action(b.buildContextMenu(), "Reset to default");
    auto learn = action(b.buildContextMenu(), "MIDI learn");
    b.mouseDown(mouse(b, juce::ModifierKeys::leftButtonModifier));
    check(b.isDown() && !b.keyPressed(shortcut) && !b.bindParameterContext(*host, otherId),
          "held ordinary pointer owns the switch and blocks context/rebind");
    if (reset) reset(); if (learn) learn();
    check(probe.events.empty() && host->getMidiLearn().learningParameter() == -1,
          "delayed actions cannot edit while ordinary activation is held");
    b.mouseDown(mouse(b, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::rightButtonModifier));
    b.mouseUp(mouse(b, juce::ModifierKeys::leftButtonModifier));
    check(b.isDown() && b.menus == 2 && probe.events.empty(), "blocked mixed-button release preserves the held left sequence");
    b.mouseUp(mouse(b, 0));
    check(p->getValue() == 0.0f && probe.events == std::vector<int>({1, 0, -1}), "ordinary left sequence still toggles once");
    probe.events.clear();
    b.mouseDown(mouse(b, juce::ModifierKeys::leftButtonModifier));
    b.mouseDown(mouse(b, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::rightButtonModifier));
    b.mouseUp(mouse(b, 0));
    check(p->getValue() == 1.0f && probe.events == std::vector<int>({1, 0, -1}), "joint release does not strand ordinary pointer ownership");
    probe.events.clear(); p->setValueNotifyingHost(0.0f); probe.events.clear();
    b.setState(juce::Button::buttonDown);
    if (reset) reset(); if (learn) learn();
    check(!b.keyPressed(shortcut) && !b.bindParameterContext(*host, otherId) && probe.events.empty()
              && host->getMidiLearn().learningParameter() == -1, "held JUCE activation state blocks menus, rebind and delayed actions");
    b.setState(juce::Button::buttonNormal);
    check(b.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)), "ordinary Return remains JUCE queued activation");
    if (reset) reset(); if (learn) learn();
    check(!b.keyPressed(shortcut) && !b.bindParameterContext(*host, otherId) && p->getValue() == 0.0f
              && probe.events.empty() && host->getMidiLearn().learningParameter() == -1,
          "queued Return owns its pending activation before JUCE dispatch");
    check(pump([&] { paint(b); return p->getValue() == 1.0f && !b.isDown(); })
              && probe.events == std::vector<int>({1, 0, -1}), "queued Return completes one attached gesture");
    check(b.keyPressed(shortcut), "context becomes available after queued activation settles"); b.dismiss();
    check(b.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)), "second Return is admitted");
    b.setEnabled(false);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    b.setEnabled(true);
    check(b.keyPressed(shortcut) && p->getValue() == 1.0f, "disabled-before-delivery Return drains ownership without a click"); b.dismiss();
    probe.events.clear();

    p->setValueNotifyingHost(0.0f); probe.events.clear();
    reset = action(b.buildContextMenu(), "Reset to default"); learn = action(b.buildContextMenu(), "MIDI learn");
    b.setEnabled(false);
    if (reset) reset(); if (learn) learn();
    check(!b.keyPressed(shortcut) && !b.keyPressed(juce::KeyPress(juce::KeyPress::returnKey))
              && probe.events.empty() && host->getMidiLearn().learningParameter() == -1,
          "disabled component blocks keyboard dispatch and delayed actions");
    b.setEnabled(true);
    {
        juce::Component parent; parent.addChildComponent(b); parent.setEnabled(false);
        if (reset) reset(); if (learn) learn();
        b.mouseDown(mouse(b, juce::ModifierKeys::rightButtonModifier)); b.mouseUp(mouse(b, 0));
        check(!b.keyPressed(shortcut) && probe.events.empty() && host->getMidiLearn().learningParameter() == -1,
              "ancestor disabled state blocks context routing and delayed edits");
        parent.removeChildComponent(&b);
    }
    if (learn) learn();
    check(host->getMidiLearn().learningParameter() == target && b.getTooltip().contains("Waiting for a MIDI controller"),
          "current learn uses the actual host index and live waiting tooltip");
    auto cancel = action(b.buildContextMenu(), "Cancel MIDI learn");
    host->getMidiLearn().armLearn(other); if (cancel) cancel();
    check(host->getMidiLearn().learningParameter() == other, "stale cancel preserves another target's learn");
    host->getMidiLearn().armLearn(target); if (cancel) cancel();
    check(host->getMidiLearn().learningParameter() == target, "learn ABA invalidates an old cancel of the same target");
    cancel = action(b.buildContextMenu(), "Cancel MIDI learn"); if (cancel) cancel();
    check(host->getMidiLearn().learningParameter() == -1, "fresh cancel clears its current learn");
    learn = action(b.buildContextMenu(), "MIDI learn"); host->setParamValue(P_parityMode, 1.0f); if (learn) learn();
    check(host->getMidiLearn().learningParameter() == -1 && !action(b.buildContextMenu(), "MIDI learn"),
          "PURE blocks both newly built and delayed learn actions");
    host->setParamValue(P_parityMode, 0.0f);
    host->getMidiLearn().assign(42, target);
    auto forget = action(b.buildContextMenu(), "Forget MIDI CC 42");
    host->getMidiLearn().assign(43, target); if (forget) forget();
    check(host->getMidiLearn().controllerFor(target) == 43, "stale forget preserves a new CC");
    host->getMidiLearn().assign(42, target); if (forget) forget();
    check(host->getMidiLearn().controllerFor(target) == 42, "mapping ABA invalidates old forget of the same CC");
    forget = action(b.buildContextMenu(), "Forget MIDI CC 42"); if (forget) forget();
    check(host->getMidiLearn().controllerFor(target) == -1, "fresh forget removes its exact mapping");
    learn = action(b.buildContextMenu(), "MIDI learn");
    host->getMidiLearn().armLearn(other); if (learn) learn();
    check(host->getMidiLearn().learningParameter() == other, "stale arm cannot overwrite newer learn intent");
    host->getMidiLearn().clearAll();

    b.setTooltip("Custom explanation.");
    check(b.getTooltip().contains("Custom explanation.") && b.getTooltip().contains(p->getCurrentValueAsText())
              && b.getDescription().contains("Shift+F10"), "custom tooltip, current parameter text and accessible context hint coexist");
    check(!b.bindParameterContext(*host, "invalid-id") && b.getContextParameterId() == id,
          "invalid ID cannot replace a valid binding");
    reset = action(b.buildContextMenu(), "Reset to default"); learn = action(b.buildContextMenu(), "MIDI learn");
    const auto staleOpen = b.onContextMenu;
    bound.rebind(*host, P_subOriginalMode); bound.rebind(*host, P_outputMode);
    if (reset) reset(); if (learn) learn();
    const int menus = b.menus; if (staleOpen) staleOpen();
    check(p->getValue() == 0.0f && host->getMidiLearn().learningParameter() == -1 && b.menus == menus,
          "binding ABA invalidates stale reset, learn and opener callbacks");
    reset = action(b.buildContextMenu(), "Reset to default"); learn = action(b.buildContextMenu(), "MIDI learn");
    b.keyPressed(shortcut); b.clearParameterContext();
    if (reset) reset(); if (learn) learn();
    check(!b.bindParameterContext(*host, id) && b.buildContextMenu().getNumItems() == 0
              && b.getContextHostParameterIndex() == -1 && p->getValue() == 0.0f,
          "clear invalidates callbacks but preserves live presentation ownership until dismissal");
    b.dismiss(); check(b.bindParameterContext(*host, id), "rebind works after context dismissal");
    b.mouseDown(mouse(b, juce::ModifierKeys::leftButtonModifier)); b.clearParameterContext();
    check(!b.bindParameterContext(*host, id), "clear during ordinary pointer hold cannot release ownership for a rebind");
    b.mouseUp(mouse(b, 0));
    check(p->getValue() == 1.0f && b.bindParameterContext(*host, id), "ordinary release after clear still completes the existing attachment");
    p->setValueNotifyingHost(0.0f);
    reset = action(b.buildContextMenu(), "Reset to default"); learn = action(b.buildContextMenu(), "MIDI learn");
    b.showContextMenu(); auto dismissed = b.dismissed; auto opener = b.onContextMenu;
    bound.attachment.reset(); bound.button.reset();
    if (reset) reset(); if (learn) learn(); if (opener) opener(); if (dismissed) dismissed();
    check(p->getValue() == 0.0f && host->getMidiLearn().learningParameter() == -1,
          "destroyed control makes delayed actions, opener and dismissal harmless");
    p->removeListener(&probe);
    {
        Presented unbound;
        check(unbound.buildContextMenu().getNumItems() == 0 && !unbound.keyPressed(shortcut),
              "unbound control has no context popup or shortcut");
        Bound pending(*host, P_outputMode);
        check(pending.button->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)), "lifetime fixture queues an ordinary Return");
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(p->getValue() == 0.0f, "destroyed queued Return and its ownership-drain callback do not edit");

    // Actual Studio instances and their original ButtonAttachments. Presentation
    // is observed through onContextMenu; these tests never create an OS popup.
    auto editor = std::make_unique<Cs80AudioProcessorEditor>(*host);
    std::vector<ui::ParameterContextButton*> actual;
    std::function<void(juce::Component&)> collect = [&](juce::Component& parent) {
        for (auto* child : parent.getChildren())
        {
            if (auto* control = dynamic_cast<ui::ParameterContextButton*>(child))
                if (control->getName() == "Output mode" || control->getName() == "STORED ORIGINAL MODE")
                    actual.push_back(control);
            collect(*child);
        }
    };
    collect(*editor);
    check(actual.size() == 2, "bounded Studio integration contains exactly original-waveform and output-mode controls");
    std::vector<std::function<void()>> editorCallbacks;
    host->prepareToPlay(48000.0, 32);
    for (int core : {(int)P_subOriginalMode, (int)P_outputMode})
    {
        const auto expectedId = Cs80AudioProcessor::parameterId(core);
        auto it = std::find_if(actual.begin(), actual.end(), [&](auto* c) { return c->getContextParameterId() == expectedId; });
        check(it != actual.end(), "each actual Studio switch factory binds the expected APVTS ID");
        if (it == actual.end()) continue;
        auto& control = **it;
        auto* parameter = host->getAPVTS().getParameter(expectedId);
        const bool output = core == P_outputMode;
        check(control.getContextHostParameterIndex() == parameter->getParameterIndex() && (bool)control.onContextMenu,
              "actual switch exposes its correct host index and context callback");
        host->setParamValue(P_parityMode, 0.0f); host->getMidiLearn().clearAll();
        parameter->setValueNotifyingHost(0.0f);
        check(pump([&] { return !control.getToggleState(); }), "actual ButtonAttachment accepts external OFF without flipping");
        Probe events; parameter->addListener(&events);
        int actualMenus = 0; control.onContextMenu = [&] { ++actualMenus; };
        control.mouseDown(mouse(control, juce::ModifierKeys::rightButtonModifier, false));
        control.mouseDrag(mouse(control, juce::ModifierKeys::rightButtonModifier, true)); control.mouseUp(mouse(control, 0, true));
        check(actualMenus == 1 && parameter->getValue() == 0.0f && events.events.empty(),
              "actual context press/drag/release cannot write selector half or toggle");
        check(control.keyPressed(shortcut) && actualMenus == 2 && events.events.empty(), "actual Shift+F10 routes without changing parameter");
        click(control, false);
        check(parameter->getValue() == 1.0f && control.getToggleState() && events.events == std::vector<int>({1, 0, -1}),
              "actual left-half click preserves clicked-before-attachment ordering and one host gesture");
        events.events.clear();
        click(control, output ? false : true);
        check(parameter->getValue() == (output ? 1.0f : 0.0f)
                  && events.events == (output ? std::vector<int>{} : std::vector<int>{1, 0, -1}),
              "actual repeated output half is a no-op while original-mode ordinary click toggles");
        if (output)
        {
            click(control, true);
            check(parameter->getValue() == 0.0f && events.events == std::vector<int>({1, 0, -1}), "actual right-half click chooses mono once");
            events.events.clear(); click(control, true);
            check(events.events.empty(), "actual repeated mono half adds no host gesture");
            control.mouseDown(mouse(control, juce::ModifierKeys::leftButtonModifier, true));
            control.mouseDown(mouse(control, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::rightButtonModifier, false));
            control.mouseUp(mouse(control, juce::ModifierKeys::leftButtonModifier, false));
            check(!control.keyPressed(juce::KeyPress(juce::KeyPress::leftKey)) && parameter->getValue() == 0.0f,
                  "held output half cannot be stolen by a context press or concurrent key");
            control.mouseUp(mouse(control, 0, false));
            check(parameter->getValue() == 0.0f && events.events.empty(), "blocked opposite context half does not overwrite held mono choice");
        }
        parameter->setValueNotifyingHost(1.0f);
        check(pump([&] { return control.getToggleState(); }), "post-release external ON proves no stale pressed half remains");
        parameter->setValueNotifyingHost(0.0f);
        check(pump([&] { return !control.getToggleState(); }), "post-release external OFF also retains attachment ownership");
        events.events.clear();
        if (output)
        {
            for (int key : {juce::KeyPress::leftKey, juce::KeyPress::rightKey, juce::KeyPress::spaceKey, juce::KeyPress::returnKey})
            {
                const float value = parameter->getValue(); events.events.clear();
                check(control.keyPressed(juce::KeyPress(key)) && parameter->getValue() == 1.0f - value
                          && events.events == std::vector<int>({1, 0, -1}), "actual output arrows, Space and Return retain immediate semantics");
            }
            events.events.clear();
            const float modifiedBefore = parameter->getValue();
            auto modifiedReset = action(control.buildContextMenu(), "Reset to default");
            auto modifiedLearn = action(control.buildContextMenu(), "MIDI learn");
            check(control.keyPressed(juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::altModifier, 0)),
                  "modified output Return retains its original JUCE queued-click fallback");
            if (modifiedReset) modifiedReset(); if (modifiedLearn) modifiedLearn();
            check(!control.keyPressed(shortcut) && parameter->getValue() == modifiedBefore && events.events.empty()
                      && host->getMidiLearn().learningParameter() == -1,
                  "modified output Return also owns its queued interval against context and delayed edits");
            juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
            check(pump([&] { paint(control); return !control.isDown(); }) && parameter->getValue() == modifiedBefore
                      && events.events.empty(), "modified output Return finishes without inventing a selector half or gesture");
        }
        else
        {
            check(!control.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)), "original switch preserves JUCE unhandled Space behavior");
            check(control.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)), "actual original switch still queues Return");
            check(!control.keyPressed(shortcut), "actual queued Return blocks a context before its click is delivered");
            check(pump([&] { paint(control); return parameter->getValue() == 1.0f && !control.isDown(); })
                      && events.events == std::vector<int>({1, 0, -1}), "actual original Return completes one ordinary attached toggle");
        }
        events.events.clear(); const float disabledBefore = parameter->getValue();
        for (bool ancestor : {false, true})
        {
            if (ancestor) editor->setEnabled(false); else control.setEnabled(false);
            for (int key : {juce::KeyPress::leftKey, juce::KeyPress::rightKey, juce::KeyPress::spaceKey, juce::KeyPress::returnKey})
                check(!control.keyPressed(juce::KeyPress(key)) && parameter->getValue() == disabledBefore && events.events.empty(),
                      "actual disabled/ancestor-disabled ordinary key is unhandled and does not mutate");
            check(!control.keyPressed(shortcut), "actual disabled/ancestor-disabled context shortcut is unhandled");
            if (ancestor) editor->setEnabled(true); else control.setEnabled(true);
        }
        const float defaultValue = parameter->convertTo0to1(parameter->convertFrom0to1(parameter->getDefaultValue()));
        const float beforeReset = 1.0f - defaultValue;
        parameter->setValueNotifyingHost(beforeReset); events.events.clear();
        host->getUndoableState().syncNow(); host->getUndoManager().clearUndoHistory();
        auto resetActual = action(control.buildContextMenu(), "Reset to default");
        check((bool)resetActual, "actual context exposes its reset callback"); if (resetActual) resetActual();
        check(parameter->getValue() == defaultValue && control.getToggleState() == (defaultValue >= 0.5f)
                  && events.events == std::vector<int>({1, 0, -1}), "actual reset uses its real default with one balanced gesture and no selector flip");
        if (resetActual) resetActual();
        check(events.events == std::vector<int>({1, 0, -1}), "actual reset at default adds no gesture");
        parameter->removeListener(&events);
        check(host->undo() && parameter->getValue() == beforeReset, "one undo restores actual switch before reset");
        check(host->redo() && parameter->getValue() == defaultValue, "one redo restores actual switch default");
        auto learnActual = action(control.buildContextMenu(), "MIDI learn"); if (learnActual) learnActual();
        check(host->getMidiLearn().learningParameter() == parameter->getParameterIndex(), "actual learn arms its exact APVTS host index");
        std::vector<float> original;
        for (auto* x : host->getParameters()) original.push_back(x->getValue());
        juce::AudioBuffer<float> audio(2, 32); juce::MidiBuffer midi;
        const int ccValue = defaultValue >= 0.5f ? 0 : 127;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, ccValue), 0); host->processBlock(audio, midi);
        bool isolated = true;
        for (int i = 0; i < host->getParameters().size(); ++i)
            if (i != parameter->getParameterIndex()) isolated &= original[(size_t)i] == host->getParameters()[i]->getValue();
        check(parameter->getValue() == beforeReset && isolated && host->getMidiLearn().parameterFor(74) == parameter->getParameterIndex(),
              "real learned MIDI event changes only the selected actual Studio parameter");
        check(control.getTooltip().contains("MIDI CC 74") && control.getTooltip().contains(parameter->getCurrentValueAsText()),
              "actual tooltip reports live mapping and parameter value");
        host->setParamValue(P_parityMode, 1.0f);
        check(control.getTooltip().contains("inactive in PURE CS-80"), "actual tooltip reports retained mapping's PURE inactivity");
        midi.clear(); midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, 127 - ccValue), 0); host->processBlock(audio, midi);
        check(parameter->getValue() == beforeReset, "PURE keeps learned actual switch mapping inert");
        editorCallbacks.push_back(action(control.buildContextMenu(), "Reset to default"));
        control.onContextMenu = {};
    }
    const float originalBeforeDestruction = host->getAPVTS().getParameter(otherId)->getValue();
    const float outputBeforeDestruction = p->getValue();
    editor.reset();
    for (const auto& callback : editorCallbacks) if (callback) callback();
    check(host->getAPVTS().getParameter(otherId)->getValue() == originalBeforeDestruction && p->getValue() == outputBeforeDestruction,
          "actual editor destruction invalidates both controls' retained reset callbacks");
}
}
