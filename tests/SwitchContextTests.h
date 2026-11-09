#pragma once

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UiComponents.h"
#include "plugin/MemoryLid.h"

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace cs80::tests
{
namespace switch_context_detail
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
struct Bound
{
    explicit Bound(Cs80AudioProcessor& host, const juce::String& id, int help = -1)
    {
        button = std::make_unique<ui::LedButton>("Test switch");
        button->setSize(60, 30); button->setVisible(true);
        rebind(host, id, help);
    }
    void rebind(Cs80AudioProcessor& host, const juce::String& id, int help = -1)
    {
        attachment.reset();
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(host.getAPVTS(), id, *button);
        button->bindParameterContext(host, id, help);
    }
    std::unique_ptr<ui::LedButton> button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
}

template<class Check>
void testSwitchContext(Check check)
{
    using namespace switch_context_detail;
    const juce::KeyPress shortcut(juce::KeyPress::F10Key, juce::ModifierKeys::shiftModifier, 0);
    juce::TemporaryFile fixture("-cs80-switch-context");
    auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    host->setParamValue(P_parityMode, 0.0f);
    const int core = chParam(0, CP_saw), otherCore = chParam(1, CP_pulse);
    const auto coreId = Cs80AudioProcessor::parameterId(core), otherId = Cs80AudioProcessor::parameterId(otherCore);
    auto* parameter = host->getAPVTS().getParameter(coreId);
    Bound bound(*host, coreId, core);
    auto& button = *bound.button;
    Probe probe; parameter->addListener(&probe);
    int menus = 0;
    button.onContextMenu = [&] { ++menus; };
    const float before = parameter->getValue();
    button.mouseDown(mouse(button, juce::ModifierKeys::rightButtonModifier));
    check(menus == 1 && !button.isDown() && parameter->getValue() == before && probe.events.empty(),
          "context press cannot begin a normal button activation before release");
    button.mouseDrag(mouse(button, juce::ModifierKeys::rightButtonModifier));
    button.mouseUp(mouse(button, 0));
    check(menus == 1 && parameter->getValue() == before && probe.events.empty() && !button.isDown(),
          "popup down/drag/up opens one switch menu without toggling or a host gesture");
    check(button.keyPressed(shortcut) && menus == 2 && parameter->getValue() == before && probe.events.empty(),
          "Shift+F10 opens the same switch menu without a parameter edit");
    const int modifiersToTest[] {0, juce::ModifierKeys::ctrlModifier, juce::ModifierKeys::altModifier,
                         juce::ModifierKeys::commandModifier,
                         juce::ModifierKeys::shiftModifier | juce::ModifierKeys::ctrlModifier,
                         juce::ModifierKeys::shiftModifier | juce::ModifierKeys::altModifier,
                         juce::ModifierKeys::shiftModifier | juce::ModifierKeys::commandModifier};
    for (int modifiers : modifiersToTest)
        check(!button.keyPressed(juce::KeyPress(juce::KeyPress::F10Key, modifiers, 0)) && menus == 2,
              "plain/host-modified F10 remains unhandled by switches");
    button.mouseDown(mouse(button, juce::ModifierKeys::leftButtonModifier));
    check(button.isDown() && !button.keyPressed(shortcut) && menus == 2,
          "a held ordinary switch click blocks the context shortcut");
    check(!button.bindParameterContext(*host, otherId), "context rebinding cannot interrupt a held switch activation");
    button.mouseDown(mouse(button, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::rightButtonModifier));
    button.mouseUp(mouse(button, juce::ModifierKeys::leftButtonModifier));
    check(button.isDown() && menus == 2 && parameter->getValue() == before && probe.events.empty(),
          "a blocked popup sequence cannot complete or toggle the still-held ordinary activation");
    button.mouseUp(mouse(button, 0));
    check(parameter->getValue() != before && probe.events == std::vector<int>({1, 0, -1}),
          "ordinary left activation retains one ButtonAttachment gesture");
    parameter->removeListener(&probe);
    button.setEnabled(false);
    check(!button.keyPressed(shortcut) && menus == 2, "disabled switch does not consume the menu shortcut");
    button.setEnabled(true);
    {
        juce::Component parent; parent.addChildComponent(button); parent.setEnabled(false);
        check(!button.keyPressed(shortcut) && menus == 2, "inherited disabled state prevents switch context dispatch");
        parent.removeChildComponent(&button);
    }
    check(button.getDescription().contains("Shift+F10") && button.getTooltip().contains("Shift+F10"),
          "switch description and tooltip advertise keyboard context access");
    ui::LedButton unbound("Unbound");
    check(!unbound.keyPressed(shortcut) && unbound.buildContextMenu().getNumItems() == 0,
          "unbound switches retain their existing input behavior without an empty popup");
    check(!button.bindParameterContext(*host, "not-a-parameter") && button.getContextHostParameterIndex() == parameter->getParameterIndex(),
          "an invalid context ID cannot replace the existing binding");

    button.setTooltip("Custom circuit explanation.");
    check(button.getTooltip().contains("Custom circuit explanation.")
              && button.getTooltip().contains(button.getToggleState() ? ": ON" : ": OFF"),
          "bound context preserves the custom explanation and live switch-state tooltip");

    auto learn = action(button.buildContextMenu(), "MIDI learn");
    check((bool)learn, "bound core switch exposes a learn action");
    if (learn) learn();
    check(host->getMidiLearn().learningParameter() == parameter->getParameterIndex()
              && button.getTooltip().contains("Waiting for a MIDI controller"), "learn arms the actual host index and reports waiting");
    auto cancel = action(button.buildContextMenu(), "Cancel MIDI learn");
    check((bool)cancel, "armed switch offers cancellation");
    host->getMidiLearn().armLearn(host->hostParameterIndex(otherCore));
    if (cancel) cancel();
    check(host->getMidiLearn().learningParameter() == host->hostParameterIndex(otherCore),
          "stale cancellation cannot cancel another control's newer learn");
    host->getMidiLearn().armLearn(parameter->getParameterIndex());
    if (cancel) cancel();
    check(host->getMidiLearn().learningParameter() == -1, "current target cancellation works");
    host->setParamValue(P_parityMode, 1.0f);
    if (learn) learn();
    check(host->getMidiLearn().learningParameter() == -1 && !action(button.buildContextMenu(), "MIDI learn"),
          "PURE blocks both fresh and previously opened learn actions");
    host->setParamValue(P_parityMode, 0.0f);
    button.setEnabled(false); if (learn) learn();
    check(host->getMidiLearn().learningParameter() == -1, "a disabled delayed learn action is inert");
    button.setEnabled(true);
    host->getMidiLearn().assign(42, parameter->getParameterIndex());
    auto forget = action(button.buildContextMenu(), "Forget MIDI CC 42");
    host->getMidiLearn().assign(43, parameter->getParameterIndex());
    if (forget) forget();
    check(host->getMidiLearn().controllerFor(parameter->getParameterIndex()) == 43,
          "stale forget cannot erase a newer mapping");
    forget = action(button.buildContextMenu(), "Forget MIDI CC 43");
    if (forget) forget();
    check(host->getMidiLearn().controllerFor(parameter->getParameterIndex()) == -1,
          "current forget removes only this switch's mapping");
    bound.rebind(*host, otherId, otherCore);
    bound.rebind(*host, coreId, core);
    if (learn) learn();
    check(host->getMidiLearn().learningParameter() == -1, "binding generation rejects old actions even after A-to-B-to-A rebinding");
    auto beforeUnbind = action(button.buildContextMenu(), "MIDI learn");
    button.clearParameterContext();
    if (beforeUnbind) beforeUnbind();
    check(button.getContextHostParameterIndex() == -1 && !button.onContextMenu
              && button.buildContextMenu().getNumItems() == 0 && host->getMidiLearn().learningParameter() == -1,
          "clearing a context invalidates old actions without touching the parameter attachment");
    bound.rebind(*host, coreId, core);
    auto delayedReset = action(button.buildContextMenu(), "Reset to default");
    const float disabledValue = parameter->getValue();
    {
        juce::Component parent; parent.addChildComponent(button); parent.setEnabled(false);
        if (delayedReset) delayedReset();
        check(parameter->getValue() == disabledValue, "inherited disablement blocks an already-built reset action");
        parent.removeChildComponent(&button);
    }
    auto doomed = std::make_unique<Bound>(*host, coreId, core);
    auto deletedLearn = action(doomed->button->buildContextMenu(), "MIDI learn");
    auto deletedReset = action(doomed->button->buildContextMenu(), "Reset to default");
    const auto retained = parameter->getValue(); doomed.reset();
    if (deletedLearn) deletedLearn(); if (deletedReset) deletedReset();
    check(host->getMidiLearn().learningParameter() == -1 && parameter->getValue() == retained,
          "actions copied from a destroyed switch cannot learn or edit");

    // Return is dispatched by JUCE asynchronously. Pump only until the click
    // and its ordinary visual flash finish; do not fabricate an onClick call.
    {
        Bound keyboard(*host, coreId, core);
        const float initial = parameter->getValue();
        Probe keyboardProbe; parameter->addListener(&keyboardProbe);
        check(keyboard.button->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)),
              "ordinary Return remains handled by JUCE's button implementation");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(750);
        while ((parameter->getValue() == initial || keyboard.button->isDown())
               && std::chrono::steady_clock::now() < deadline)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        check(parameter->getValue() != initial && keyboardProbe.events == std::vector<int>({1, 0, -1}),
              "the real Return callback still toggles with one complete attachment gesture");
        parameter->removeListener(&keyboardProbe);
    }
    {
        ui::LedButton momentary("Momentary", true); momentary.setSize(60, 30); momentary.setVisible(true);
        int clicks = 0; momentary.onClick = [&] { ++clicks; };
        momentary.mouseDown(mouse(momentary, juce::ModifierKeys::leftButtonModifier));
        momentary.mouseUp(mouse(momentary, 0));
        check(clicks == 1 && !momentary.getToggleState() && !momentary.keyPressed(shortcut),
              "unbound momentary controls retain their ordinary non-latching activation");
    }

    // One independent host per case keeps each reset's undo transaction isolated.
    for (int bank = -1; bank < 4; ++bank)
        for (int local : {CP_saw, CP_pulse})
    {
        auto processor = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        processor->setParamValue(P_parityMode, 0.0f);
        processor->prepareToPlay(48000.0, 32);
        const auto id = bank < 0 ? Cs80AudioProcessor::parameterId(chParam(0, local))
                                : Cs80AudioProcessor::memoryParameterId(bank, local);
        Bound current(*processor, id, bank < 0 ? chParam(0, local) : -1);
        auto* target = processor->getAPVTS().getParameter(id);
        const float opposite = target->getDefaultValue() < 0.5f ? 1.0f : 0.0f;
        target->setValueNotifyingHost(opposite);
        processor->getUndoableState().syncNow(); processor->getUndoManager().clearUndoHistory();
        Probe events; target->addListener(&events);
        auto reset = action(current.button->buildContextMenu(), "Reset to default");
        check((bool)reset, "core and memory switches expose their own default action");
        if (reset) reset();
        check(target->getValue() == target->getDefaultValue() && events.events == std::vector<int>({1, 0, -1}),
              "effective reset changes the actual bound parameter in exactly one complete host gesture");
        if (reset) reset();
        check(events.events == std::vector<int>({1, 0, -1}), "reset already at default emits no redundant gesture");
        target->removeListener(&events);
        check(processor->undo() && target->getValue() == opposite, "one undo restores the switch's pre-reset value");
        check(processor->redo() && target->getValue() == target->getDefaultValue(), "one redo reapplies the switch default");
        auto arm = action(current.button->buildContextMenu(), "MIDI learn");
        if (arm) arm();
        check(processor->getMidiLearn().learningParameter() == target->getParameterIndex(),
              "memory switch learn uses its distinct APVTS host index rather than a channel parameter alias");
        std::vector<float> original;
        for (auto* p : processor->getParameters()) original.push_back(p->getValue());
        juce::AudioBuffer<float> audio(2, 32); juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, opposite > 0.5f ? 127 : 0), 0);
        processor->processBlock(audio, midi);
        bool isolated = true;
        for (int i = 0; i < processor->getParameters().size(); ++i)
            if (i != target->getParameterIndex()) isolated &= original[(size_t)i] == processor->getParameters()[i]->getValue();
        check(target->getValue() == opposite && isolated
                  && processor->getMidiLearn().parameterFor(74) == target->getParameterIndex(),
              "a real learned MIDI event changes only the bound core/memory switch");
        check(current.button->getTooltip().contains("MIDI CC 74"), "tooltip reports the current switch mapping");
        processor->setParamValue(P_parityMode, 1.0f);
        check(current.button->getTooltip().contains("inactive in PURE CS-80"), "retained switch mapping is labelled inactive in PURE");
        midi.clear(); midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, opposite > 0.5f ? 0 : 127), 0);
        processor->processBlock(audio, midi);
        check(target->getValue() == opposite, "PURE processing leaves the stored switch mapping inert");
    }

    // Actual editor factories, not only hand-built fixtures. No native peer or
    // popup is created; native popup focus/capture remains a separate UI gate.
    auto editor = std::make_unique<Cs80AudioProcessorEditor>(*host);
    int switches = 0;
    std::function<void(juce::Component&)> inspect = [&](juce::Component& parent) {
        for (auto* child : parent.getChildren())
        {
            if (auto* led = dynamic_cast<ui::LedButton*>(child))
            {
                ++switches;
                check(led->getContextHostParameterIndex() >= 0 && (bool)led->onContextMenu,
                      "every real LedButton factory installs its parameter context");
            }
            inspect(*child);
        }
    };
    inspect(*editor);
    check(switches > 8, "the real editor fixture includes main and software switch controls");
    // IDs need not share a naming convention: inspect the lid's eight actual
    // IDs through its children and compare directly against APVTS identity.
    panel::MemoryLid lid(*host); int found = 0;
    for (auto* child : lid.getChildren())
        if (auto* led = dynamic_cast<ui::LedButton*>(child))
        {
            const auto* target = host->getAPVTS().getParameter(led->getComponentID());
            check(target != nullptr && led->getContextHostParameterIndex() == target->getParameterIndex(),
                  "each real memory SAW/PULSE switch keeps its distinct parameter ID");
            ++found;
        }
    check(found == 8, "all eight memory waveform switches are covered");
}
} // namespace cs80::tests
