#pragma once

#include "StudioUiTests.h"

namespace cs80::tests::ui_completion
{
using namespace cs80::tests::studio;

inline cs80::ui::ParameterContext* contextOf(juce::Component& component)
{
    if (auto* control = dynamic_cast<cs80::ui::ContextButton*>(&component)) return control->getParameterContext();
    if (auto* control = dynamic_cast<cs80::ui::ContextTextButton*>(&component)) return control->getParameterContext();
    if (auto* control = dynamic_cast<cs80::ui::ContextToggleButton*>(&component)) return control->getParameterContext();
    if (auto* control = dynamic_cast<cs80::ui::ContextComboBox*>(&component)) return control->getParameterContext();
    if (auto* control = dynamic_cast<cs80::ui::FocusSlider*>(&component)) return control->getParameterContext();
    return nullptr;
}

inline juce::Component* findContext(juce::Component& parent, const juce::String& id)
{
    if (auto* context = contextOf(parent); context != nullptr && context->getParameterId() == id) return &parent;
    for (auto* child : parent.getChildren())
        if (auto* found = findContext(*child, id)) return found;
    return nullptr;
}

// Acceptance registered in tolerances-v2/ui-completion.json before implementation.
inline void testParameterContexts(const Check& check, const Flush& flush)
{
    Cs80AudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);
    Cs80AudioProcessorEditor editor(processor);
    editor.setSize(1600, 1000);
    editor.setVisible(true);
    const auto saw = Cs80AudioProcessor::parameterId(cs80::chParam(0, cs80::CP_saw));
    const auto memory = Cs80AudioProcessor::memoryParameterId(2, cs80::CP_lpfCutoff);
    const std::array<juce::String, 7> ids {
        saw,
        Cs80AudioProcessor::parameterId(cs80::P_arpOn),
        Cs80AudioProcessor::parameterId(cs80::P_lfo1Wave),
        Cs80AudioProcessor::parameterId(cs80::P_voiceMode),
        Cs80AudioProcessor::hardwareSourceParameterId(1),
        memory,
        Cs80AudioProcessor::memoryParameterId(3, cs80::CP_pulse)
    };
    for (const auto& id : ids)
    {
        auto* component = findContext(editor, id);
        check(component != nullptr, "switch/combo/tone/memory control has a parameter context");
        if (component == nullptr) continue;
        auto* context = contextOf(*component);
        auto* parameter = processor.getAPVTS().getParameter(id);
        auto menu = context->buildContextMenu();
        for (const auto* label : { "MIDI learn", "Reset to default", "Type a value", "Copy value", "Paste value", "What is this control?" })
            check(menuItem(menu, label) != nullptr, "every parameter widget has the complete context menu");

        // Reset/copy/paste follow the host ID, not a visually similar main-panel control.
        parameter->setValueNotifyingHost(parameter->getDefaultValue() > 0.5f ? 0.0f : 1.0f);
        StudioGestureProbe probe;
        parameter->addListener(&probe);
        context->resetToDefault();
        check(parameter->getValue() == parameter->getDefaultValue() && probe.oneGesture(),
              "context reset uses one balanced gesture on its actual host parameter");
        parameter->removeListener(&probe);
        context->copyValue();
        parameter->setValueNotifyingHost(parameter->getDefaultValue() > 0.5f ? 0.0f : 1.0f);
        probe = {};
        parameter->addListener(&probe);
        check(context->pasteValue() && probe.oneGesture(), "context paste returns the copied value in one balanced gesture");
        parameter->removeListener(&probe);

        processor.getMidiLearn().clearAll();
        menu = context->buildContextMenu();
        const auto* learn = menuItem(menu, "MIDI learn");
        check(learn != nullptr && learn->isEnabled && (bool) learn->action, "parameter context can arm MIDI learn");
        if (learn != nullptr && learn->action) learn->action();
        check(processor.getMidiLearn().learningParameter() == parameter->getParameterIndex(), "learn targets the actual widget host parameter");
        sendController(processor, 74, 52);
        check(processor.getMidiLearn().controllerFor(parameter->getParameterIndex()) == 74,
              "switch/combo/tone/memory context learns the received MIDI CC");
        processor.setParamValue(cs80::P_parityMode, 1.0f);
        const float before = parameter->getValue();
        auto pureMenu = context->buildContextMenu();
        learn = menuItem(pureMenu, "MIDI learn");
        check(learn != nullptr && !learn->isEnabled, "every parameter context disables MIDI learn in PURE CS-80");
        sendController(processor, 74, 104);
        check(parameter->getValue() == before && processor.getMidiLearn().controllerFor(parameter->getParameterIndex()) == 74,
              "PURE CS-80 keeps mappings but does not apply the learned controller");
        processor.setParamValue(cs80::P_parityMode, 0.0f);
    }
    processor.getMidiLearn().clearAll();

    auto* miniature = findContext(editor, memory);
    if (miniature != nullptr)
    {
        auto* context = contextOf(*miniature);
        const float panel = processor.getParamValue(cs80::chParam(0, cs80::CP_lpfCutoff));
        check(context->applyTypedValue("850 Hz") && std::abs(processor.getMemoryValue(2, cs80::CP_lpfCutoff) - 850.0f) < 0.5f
                  && processor.getParamValue(cs80::chParam(0, cs80::CP_lpfCutoff)) == panel,
              "typed memory miniature edits its memory bank and leaves the main panel alone");
        check(!context->applyTypedValue("80 ms"), "memory miniature refuses incompatible units");
    }
    auto* combo = findContext(editor, Cs80AudioProcessor::parameterId(cs80::P_voiceMode));
    if (combo != nullptr)
        check(contextOf(*combo)->applyTypedValue("16 voices (two instruments)") && processor.getParamValue(cs80::P_voiceMode) == 1.0f,
              "combo context accepts the visible choice name");

    auto* button = findContext(editor, saw);
    if (button != nullptr)
    {
        auto* parameter = processor.getAPVTS().getParameter(saw);
        const float before = parameter->getValue();
        StudioGestureProbe probe;
        parameter->addListener(&probe);
        button->mouseDown(mouseAt(*button, { 4.0f, 4.0f }, juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier)));
        button->mouseUp(mouseAt(*button, { 4.0f, 4.0f }, {}));
        juce::PopupMenu::dismissAllActiveMenus();
        flush(30);
        check(parameter->getValue() == before && probe.begins == 0 && probe.changes == 0,
              "right-click menu does not also toggle the underlying switch or start a gesture");
        button->mouseDown(mouseAt(*button, { 4.0f, 4.0f }, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::commandModifier)));
        button->mouseUp(mouseAt(*button, { 4.0f, 4.0f }, {}));
        auto* entry = dynamic_cast<juce::TextEditor*>(findById(editor, "parameter_value_entry"));
        check(entry != nullptr && probe.begins == 0, "Ctrl/Cmd-click opens switch value entry without toggling it");
        if (entry != nullptr)
        {
            entry->setText(before >= 0.5f ? "OFF" : "ON", juce::dontSendNotification);
            if (entry->onReturnKey) entry->onReturnKey();
            flush(30);
            check(parameter->getValue() != before && probe.oneGesture(), "typed switch value commits in one balanced host gesture");
        }
        parameter->removeListener(&probe);
    }
}

} // namespace cs80::tests::ui_completion
