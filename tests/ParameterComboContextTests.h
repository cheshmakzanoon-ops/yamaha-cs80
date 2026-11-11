#pragma once

#include "plugin/ParameterComboBox.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace cs80::tests
{
namespace combo_context_detail
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
inline juce::MouseEvent mouse(juce::Component& c, int modifiers)
{
    const juce::Point<float> p(10.0f, 10.0f);
    return {juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys(modifiers),
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, juce::Time::getCurrentTime(),
            p, juce::Time::getCurrentTime(), 1, false};
}
inline bool pump(const std::function<bool()>& done)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(750);
    while (!done() && std::chrono::steady_clock::now() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    return done();
}
// Only presentation is replaced. The real ComboBox queues showPopup, owns
// its active state, navigates and notifies its real ComboBoxAttachment.
struct PresentedCombo : ui::ParameterComboBox
{
    int choicePopups = 0, contextPopups = 0;
    juce::PopupMenu context;
    std::function<void()> dismissed;
    void showPopup() override { ++choicePopups; }
    void presentContextMenu(juce::PopupMenu menu, std::function<void()> complete) override
    {
        ++contextPopups; context = std::move(menu); dismissed = std::move(complete);
    }
    void dismissContext()
    {
        auto complete = std::move(dismissed);
        if (complete) complete();
    }
};
struct Bound
{
    explicit Bound(Cs80AudioProcessor& host, int core)
    {
        combo = std::make_unique<PresentedCombo>();
        combo->setSize(160, 32); combo->setVisible(true);
        combo->addItemList({"OFF", "CHORUS", "PHASER", "TAPE DELAY", "PLATE REVERB", "DRIVE"}, 1);
        rebind(host, core);
    }
    void rebind(Cs80AudioProcessor& host, int core)
    {
        attachment.reset();
        const auto id = Cs80AudioProcessor::parameterId(core);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(host.getAPVTS(), id, *combo);
        combo->bindParameterContext(host, id);
    }
    std::unique_ptr<PresentedCombo> combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};
}

template<class Check>
void testParameterComboContext(Check check)
{
    using namespace combo_context_detail;
    juce::TemporaryFile fixture("-cs80-combo-context");
    auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    host->setParamValue(P_parityMode, 0.0f);
    Bound bound(*host, P_fx1Type);
    auto& combo = *bound.combo;
    const auto id = Cs80AudioProcessor::parameterId(P_fx1Type);
    const auto otherId = Cs80AudioProcessor::parameterId(P_fx2Type);
    auto* parameter = host->getAPVTS().getParameter(id);
    const int target = parameter->getParameterIndex(), other = host->getAPVTS().getParameter(otherId)->getParameterIndex();
    const juce::KeyPress shortcut(juce::KeyPress::F10Key, juce::ModifierKeys::shiftModifier, 0);
    Probe probe; parameter->addListener(&probe);
    const float before = parameter->getValue();
    combo.mouseDown(mouse(combo, juce::ModifierKeys::rightButtonModifier));
    check(combo.contextPopups == 1 && !combo.isPopupActive() && parameter->getValue() == before && probe.events.empty(),
          "right press dispatches a context menu without opening choices or starting a parameter gesture");
    combo.mouseDrag(mouse(combo, juce::ModifierKeys::rightButtonModifier));
    combo.mouseUp(mouse(combo, 0));
    check(combo.contextPopups == 1 && !combo.isPopupActive() && probe.events.empty(),
          "popup down/drag/up is consumed exactly once");
    combo.mouseDown(mouse(combo, juce::ModifierKeys::leftButtonModifier));
    combo.mouseUp(mouse(combo, 0));
    combo.showContextMenu();
    check(!combo.keyPressed(shortcut) && !combo.keyPressed(juce::KeyPress(juce::KeyPress::downKey))
              && combo.contextPopups == 1 && !combo.isPopupActive()
              && !combo.bindParameterContext(*host, otherId),
          "active context presentation blocks duplicate context, ordinary input and rebinding");
    combo.setScrollWheelEnabled(true);
    juce::MouseWheelDetails wheel; wheel.deltaY = -1.0f;
    combo.mouseWheelMove(mouse(combo, 0), wheel);
    check(parameter->getValue() == before && probe.events.empty(), "active context presentation cannot wheel-edit the selection");
    combo.setScrollWheelEnabled(false);
    combo.dismissContext();
    check(combo.keyPressed(shortcut) && combo.contextPopups == 2 && probe.events.empty(),
          "exact Shift+F10 opens the same context after dismissal without a gesture");
    combo.dismissContext();
    const int modifierMasks[] {0, juce::ModifierKeys::ctrlModifier, juce::ModifierKeys::altModifier,
                         juce::ModifierKeys::commandModifier,
                         juce::ModifierKeys::shiftModifier | juce::ModifierKeys::ctrlModifier,
                         juce::ModifierKeys::shiftModifier | juce::ModifierKeys::altModifier,
                         juce::ModifierKeys::shiftModifier | juce::ModifierKeys::commandModifier};
    for (int modifiers : modifierMasks)
        check(!combo.keyPressed(juce::KeyPress(juce::KeyPress::F10Key, modifiers, 0)) && combo.contextPopups == 2,
              "plain and host-modified F10 retain their existing unhandled routing");
    check(!combo.keyPressed(juce::KeyPress(juce::KeyPress::F1Key)) && !action(combo.buildContextMenu(), "What is")
              && !action(combo.buildContextMenu(), "Type"),
          "discrete choices expose no generic help or continuous typed-value editor");

    parameter->setValueNotifyingHost(1.0f);
    check(pump([&] { return combo.getSelectedItemIndex() == 5; }), "popup guard fixture reaches a non-default choice");
    probe.events.clear();
    const float popupBefore = parameter->getValue();
    auto delayedReset = action(combo.buildContextMenu(), "Reset to default");
    auto delayedLearn = action(combo.buildContextMenu(), "MIDI learn");
    combo.mouseDown(mouse(combo, juce::ModifierKeys::leftButtonModifier));
    check(combo.isPopupActive() && !combo.keyPressed(shortcut) && !combo.bindParameterContext(*host, otherId),
          "normal left press retains JUCE's pending choice popup and blocks context/rebinding");
    check(pump([&] { return combo.choicePopups == 1; }), "ordinary left press reaches the queued JUCE showPopup callback");
    combo.mouseDown(mouse(combo, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::rightButtonModifier));
    combo.mouseUp(mouse(combo, juce::ModifierKeys::leftButtonModifier));
    check(combo.contextPopups == 2 && combo.isPopupActive(), "blocked right sequence cannot replace an ordinary popup");
    if (delayedReset) delayedReset();
    check(parameter->getValue() == popupBefore && probe.events.empty(), "delayed reset cannot edit under a held ordinary choice popup");
    combo.mouseUp(mouse(combo, 0));
    if (delayedReset) delayedReset(); if (delayedLearn) delayedLearn();
    check(parameter->getValue() == popupBefore && probe.events.empty() && host->getMidiLearn().learningParameter() == -1,
          "ordinary choice popup blocks delayed actions even after the pointer is released");
    combo.hidePopup();
    check(combo.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)) && combo.isPopupActive(), "Return still schedules the ordinary choice menu");
    check(pump([&] { return combo.choicePopups == 2; }), "Return reaches the ordinary queued popup presenter");
    combo.hidePopup();
    check(combo.keyPressed(shortcut), "context is usable after ordinary choice dismissal");
    combo.dismissContext();
    // A blocked popup release with no remaining left button must not strand the ordinary sequence.
    combo.mouseDown(mouse(combo, juce::ModifierKeys::leftButtonModifier));
    check(pump([&] { return combo.choicePopups == 3; }), "second ordinary click remains operational");
    combo.mouseDown(mouse(combo, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::rightButtonModifier));
    combo.mouseUp(mouse(combo, 0)); combo.hidePopup();
    check(combo.keyPressed(shortcut), "simultaneous pointer release cannot leave context permanently blocked");
    combo.dismissContext();
    parameter->removeListener(&probe);

    combo.setEnabled(false);
    const int menuCount = combo.contextPopups;
    check(!combo.keyPressed(shortcut) && !combo.keyPressed(juce::KeyPress(juce::KeyPress::downKey)), "disabled combo cannot open context or navigate");
    combo.mouseDown(mouse(combo, juce::ModifierKeys::rightButtonModifier)); combo.mouseUp(mouse(combo, 0));
    check(combo.contextPopups == menuCount && !action(combo.buildContextMenu(), "Reset"), "disabled pointer and menu actions are unavailable");
    combo.setEnabled(true);
    check(combo.getWantsKeyboardFocus() && combo.getDescription().contains("Shift+F10"), "bound choices are keyboard-focusable and describe context access");
    check(!combo.bindParameterContext(*host, "not-a-parameter") && combo.getContextHostParameterIndex() == target,
          "invalid binding cannot replace the valid ID");
    combo.setTooltip("Custom choice explanation.");
    check(combo.getComponentAt(10, 10) == &combo,
          "hovering the choice label reaches the dynamic tooltip client rather than a stale copied label tooltip");
    check(combo.getTooltip().contains("Custom choice explanation.") && combo.getTooltip().contains(parameter->getCurrentValueAsText()),
          "dynamic tooltip preserves custom explanation and reports the actual selected parameter value");

    auto learn = action(combo.buildContextMenu(), "MIDI learn");
    check((bool)learn, "bound choice exposes MIDI learning");
    if (learn) learn();
    check(host->getMidiLearn().learningParameter() == target && combo.getTooltip().contains("Waiting for a MIDI controller"),
          "learn arms the actual APVTS host index and updates the tooltip");
    auto cancel = action(combo.buildContextMenu(), "Cancel MIDI learn");
    host->getMidiLearn().armLearn(other);
    if (cancel) cancel();
    check(host->getMidiLearn().learningParameter() == other, "stale cancel cannot cancel a newer target");
    host->getMidiLearn().armLearn(target);
    if (cancel) cancel();
    check(host->getMidiLearn().learningParameter() == target, "stale cancel rejects same-target learn ABA");
    cancel = action(combo.buildContextMenu(), "Cancel MIDI learn");
    if (cancel) cancel();
    check(host->getMidiLearn().learningParameter() == -1, "fresh cancellation works");
    learn = action(combo.buildContextMenu(), "MIDI learn");
    host->getMidiLearn().armLearn(other);
    if (learn) learn();
    check(host->getMidiLearn().learningParameter() == other, "stale learn cannot overwrite a newer armed target");
    host->getMidiLearn().cancelLearn();
    learn = action(combo.buildContextMenu(), "MIDI learn");
    host->setParamValue(P_parityMode, 1.0f);
    if (learn) learn();
    check(host->getMidiLearn().learningParameter() == -1 && !action(combo.buildContextMenu(), "MIDI learn"),
          "PURE disables fresh and previously opened learn actions");
    host->setParamValue(P_parityMode, 0.0f);
    combo.setEnabled(false); if (learn) learn();
    check(host->getMidiLearn().learningParameter() == -1, "a disabled delayed learn is inert");
    combo.setEnabled(true);
    host->getMidiLearn().assign(42, target);
    host->getMidiLearn().assign(44, other);
    auto forget = action(combo.buildContextMenu(), "Forget MIDI CC 42");
    host->getMidiLearn().assign(43, target); if (forget) forget();
    check(host->getMidiLearn().controllerFor(target) == 43, "stale forget cannot erase a replacement CC");
    host->getMidiLearn().assign(42, target); if (forget) forget();
    check(host->getMidiLearn().controllerFor(target) == 42, "stale forget rejects same-CC mapping ABA");
    forget = action(combo.buildContextMenu(), "Forget MIDI CC 42");
    if (forget) forget();
    check(host->getMidiLearn().controllerFor(target) == -1 && host->getMidiLearn().controllerFor(other) == 44,
          "fresh forget removes only this control's mapping");
    host->getMidiLearn().clearAll();
    learn = action(combo.buildContextMenu(), "MIDI learn");
    auto reset = action(combo.buildContextMenu(), "Reset to default");
    bound.rebind(*host, P_fx2Type); bound.rebind(*host, P_fx1Type);
    parameter->setValueNotifyingHost(1.0f);
    if (learn) learn(); if (reset) reset();
    check(host->getMidiLearn().learningParameter() == -1 && parameter->getValue() == 1.0f,
          "binding generation rejects old learn/reset even after A-to-B-to-A rebinding");
    learn = action(combo.buildContextMenu(), "MIDI learn");
    reset = action(combo.buildContextMenu(), "Reset to default");
    {
        juce::Component parent; parent.addChildComponent(combo); parent.setEnabled(false);
        if (learn) learn(); if (reset) reset();
        check(!combo.keyPressed(shortcut) && host->getMidiLearn().learningParameter() == -1 && parameter->getValue() == 1.0f,
              "inherited disablement blocks dispatch and already-built actions");
        parent.removeChildComponent(&combo);
    }
    combo.clearParameterContext();
    if (learn) learn(); if (reset) reset();
    check(combo.getContextHostParameterIndex() == -1 && !combo.onContextMenu && combo.buildContextMenu().getNumItems() == 0
              && host->getMidiLearn().learningParameter() == -1 && parameter->getValue() == 1.0f,
          "clearing context invalidates actions without redirecting its live attachment");
    combo.setSelectedItemIndex(2, juce::sendNotificationSync);
    check(host->getParamValue(P_fx1Type) == 2.0f, "ordinary choice attachment still works after context clearing");
    bound.rebind(*host, P_fx1Type);
    combo.mouseDown(mouse(combo, juce::ModifierKeys::rightButtonModifier));
    auto clearedAction = action(combo.context, "MIDI learn");
    combo.clearParameterContext(); combo.dismissContext(); combo.mouseUp(mouse(combo, 0));
    if (clearedAction) clearedAction();
    check(!combo.isPopupActive() && host->getMidiLearn().learningParameter() == -1,
          "clear during a context sequence keeps its release swallowed and old action inert");
    {
        auto doomed = std::make_unique<Bound>(*host, P_fx1Type);
        doomed->combo->showContextMenu();
        auto deletedLearn = action(doomed->combo->context, "MIDI learn");
        auto deletedReset = action(doomed->combo->context, "Reset to default");
        auto dismissed = std::move(doomed->combo->dismissed);
        const float retained = parameter->getValue(); doomed.reset();
        if (deletedLearn) deletedLearn(); if (deletedReset) deletedReset(); if (dismissed) dismissed();
        check(host->getMidiLearn().learningParameter() == -1 && parameter->getValue() == retained,
              "destroyed combo actions and presentation completion are safe and inert");
    }
    ui::ParameterComboBox unbound;
    check(!unbound.keyPressed(shortcut) && unbound.buildContextMenu().getNumItems() == 0,
          "unbound combo retains ordinary routing without an empty context popup");

    // The four shipped choices currently default to zero. An independent
    // nonzero discrete default prevents a hard-coded first-item reset passing.
    {
        PresentedCombo nonzero;
        nonzero.addItemList({"MONO", "STEREO"}, 1);
        const auto outputId = Cs80AudioProcessor::parameterId(P_outputMode);
        juce::AudioProcessorValueTreeState::ComboBoxAttachment attachment(host->getAPVTS(), outputId, nonzero);
        nonzero.bindParameterContext(*host, outputId);
        auto* output = host->getAPVTS().getParameter(outputId);
        output->setValueNotifyingHost(0.0f);
        Probe events; output->addListener(&events);
        auto resetOutput = action(nonzero.buildContextMenu(), "Reset to default");
        if (resetOutput) resetOutput();
        check(output->getDefaultValue() == 1.0f && output->getValue() == 1.0f
                  && events.events == std::vector<int>({1, 0, -1}),
              "reset reads a nonzero discrete parameter default rather than choosing the first item");
        if (resetOutput) resetOutput();
        check(events.events == std::vector<int>({1, 0, -1}), "nonzero default reset is also an exact no-op when repeated");
        output->removeListener(&events);
    }

    // All four actual factory-built controls, with live ComboBoxAttachments.
    // No native peer or OS popup is constructed; keyboard/popup routing above
    // stops at presentation and menu actions below are invoked directly.
    auto editor = std::make_unique<Cs80AudioProcessorEditor>(*host);
    std::vector<ui::ParameterComboBox*> actual;
    std::function<void(juce::Component&)> collect = [&](juce::Component& parent) {
        for (auto* child : parent.getChildren())
        {
            if (auto* choice = dynamic_cast<ui::ParameterComboBox*>(child)) actual.push_back(choice);
            collect(*child);
        }
    };
    collect(*editor);
    check(actual.size() == 4, "actual Studio editor contains exactly three FX choices and one arp-mode choice");
    host->prepareToPlay(48000.0, 32);
    for (int core : {P_fx1Type, P_fx2Type, P_fx3Type, P_arpMode})
    {
        const auto expectedId = Cs80AudioProcessor::parameterId(core);
        auto it = std::find_if(actual.begin(), actual.end(), [&](auto* c) { return c->getContextParameterId() == expectedId; });
        check(it != actual.end(), "each expected Studio parameter ID has a real context-bound combo");
        if (it == actual.end()) continue;
        auto& choice = **it;
        auto* p = host->getAPVTS().getParameter(expectedId);
        check(choice.getContextHostParameterIndex() == p->getParameterIndex() && choice.getWantsKeyboardFocus()
                  && (bool)choice.onContextMenu, "actual choice factory binds its correct host index and keyboard context action");
        check(choice.getComponentAt(choice.getLocalBounds().getCentre()) == &choice,
              "actual choice label routes hover and pointer input to its dynamic context component");
        host->setParamValue(P_parityMode, 0.0f);
        host->getMidiLearn().clearAll();
        p->setValueNotifyingHost(0.0f);
        check(pump([&] { return choice.getSelectedItemIndex() == 0; }), "external parameter edit reaches the actual ComboBoxAttachment");
        Probe events; p->addListener(&events);
        check(choice.keyPressed(juce::KeyPress(juce::KeyPress::downKey)), "actual factory combo retains arrow-key navigation");
        check(pump([&] { return p->getValue() > 0.0f; }) && events.events == std::vector<int>({1, 0, -1}),
              "ordinary arrow choice sends one complete ComboBoxAttachment host gesture");
        events.events.clear();
        choice.setSelectedItemIndex(choice.getNumItems() - 1, juce::sendNotificationSync);
        check(p->getValue() == 1.0f && events.events == std::vector<int>({1, 0, -1}),
              "ordinary popup-selection notification remains attached with one complete host gesture");
        events.events.clear();
        host->getUndoableState().syncNow(); host->getUndoManager().clearUndoHistory();
        auto defaultAction = action(choice.buildContextMenu(), "Reset to default");
        check((bool)defaultAction, "each real choice exposes reset");
        if (defaultAction) defaultAction();
        const float defaultValue = p->convertTo0to1(p->convertFrom0to1(p->getDefaultValue()));
        check(p->getValue() == defaultValue && events.events == std::vector<int>({1, 0, -1}),
              "actual choice resets to its representable parameter default with a balanced host gesture");
        if (defaultAction) defaultAction();
        check(events.events == std::vector<int>({1, 0, -1}), "reset already at default adds no gesture");
        p->removeListener(&events);
        check(host->undo() && p->getValue() == 1.0f, "one undo restores the actual choice before reset");
        check(host->redo() && p->getValue() == defaultValue, "one redo restores the actual choice default");
        auto arm = action(choice.buildContextMenu(), "MIDI learn");
        if (arm) arm();
        check(host->getMidiLearn().learningParameter() == p->getParameterIndex(), "actual combo learn arms its exact APVTS host index");
        std::vector<float> original;
        for (auto* x : host->getParameters()) original.push_back(x->getValue());
        juce::AudioBuffer<float> audio(2, 32); juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, 127), 0);
        host->processBlock(audio, midi);
        bool isolated = true;
        for (int i = 0; i < host->getParameters().size(); ++i)
            if (i != p->getParameterIndex()) isolated &= original[(size_t)i] == host->getParameters()[i]->getValue();
        check(p->getValue() == 1.0f && isolated && host->getMidiLearn().parameterFor(74) == p->getParameterIndex(),
              "real learned MIDI event changes only the selected actual Studio parameter");
        check(choice.getTooltip().contains("MIDI CC 74") && choice.getTooltip().contains(p->getCurrentValueAsText()),
              "actual choice tooltip reports current mapped CC and current discrete value");
        host->setParamValue(P_parityMode, 1.0f);
        check(choice.getTooltip().contains("inactive in PURE CS-80"), "actual choice tooltip marks retained mapping inactive in PURE");
        midi.clear(); midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, 0), 0);
        host->processBlock(audio, midi);
        check(p->getValue() == 1.0f, "PURE processing keeps this actual choice's learned mapping inert");
    }
}
}
