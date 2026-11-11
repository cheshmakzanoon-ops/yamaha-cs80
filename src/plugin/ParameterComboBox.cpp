#include "ParameterComboBox.h"
#include "PluginProcessor.h"

namespace cs80::ui
{
ParameterComboBox::ParameterComboBox()
{
    routeLabelInput();
}

void ParameterComboBox::lookAndFeelChanged()
{
    juce::ComboBox::lookAndFeelChanged();
    routeLabelInput();
}

void ParameterComboBox::routeLabelInput()
{
    // These are non-editable choices. Keep their label from becoming the
    // TooltipWindow's hovered client with a stale, copied tooltip. All input
    // reaches the same ComboBox handlers (as a non-editable label already did).
    for (auto* child : getChildren())
        if (auto* label = dynamic_cast<juce::Label*>(child)) label->setInterceptsMouseClicks(false, false);
}

bool ParameterComboBox::bindParameterContext(Cs80AudioProcessor& owner, const juce::String& id)
{
    if (owner.getAPVTS().getParameter(id) == nullptr || ordinaryPointer || isPopupActive() || contextPopupActive)
        return false;
    ++bindingGeneration;
    processor = &owner;
    parameterId = id;
    juce::Component::SafePointer<ParameterComboBox> safe(this);
    onContextMenu = [safe, generation = bindingGeneration] {
        if (safe != nullptr && safe->bindingGeneration == generation) safe->showContextMenu();
    };
    setWantsKeyboardFocus(true);
    setDescription("Use arrow keys to change the choice, or Return to open the choices. Right-click or Shift+F10 opens MIDI learn and reset.");
    return true;
}

void ParameterComboBox::clearParameterContext()
{
    ++bindingGeneration;
    processor = nullptr;
    parameterId.clear();
    onContextMenu = {};
    // Preserve pointer/popup ownership until release/dismissal, including
    // when a context is cleared with its asynchronous menu still open.
    setDescription("Use arrow keys to change the choice, or Return to open the choices.");
}

juce::RangedAudioParameter* ParameterComboBox::contextParameter() const
{
    return processor != nullptr ? processor->getAPVTS().getParameter(parameterId) : nullptr;
}

int ParameterComboBox::getContextHostParameterIndex() const
{
    if (const auto* target = contextParameter()) return target->getParameterIndex();
    return -1;
}

bool ParameterComboBox::canOpenContext() const
{
    return isEnabled() && !ordinaryPointer && !isPopupActive() && !contextPopupActive && contextParameter() != nullptr;
}

bool ParameterComboBox::contextMatches(Cs80AudioProcessor* owner, const juce::String& id,
                                       int host, std::uint64_t generation) const
{
    // A selected context action may be dispatched before its dismissal callback.
    // Only the ordinary choice popup blocks the action, not its own context menu.
    return isEnabled() && !ordinaryPointer && !isPopupActive() && bindingGeneration == generation
        && processor == owner && parameterId == id && host >= 0 && getContextHostParameterIndex() == host;
}

juce::String ParameterComboBox::getTooltip()
{
    auto text = juce::ComboBox::getTooltip();
    if (const auto* target = contextParameter())
    {
        if (text.isNotEmpty()) text << "\n";
        text << target->getName(64) << ": " << target->getCurrentValueAsText()
             << "\nRight-click or Shift+F10: MIDI learn, reset.";
        const auto& learn = processor->getMidiLearn();
        const int host = target->getParameterIndex();
        const bool pure = processor->getParamValue(P_parityMode) >= 0.5f;
        if (!pure && learn.learningParameter() == host)
            text << "\nWaiting for a MIDI controller. Shift+F10 to cancel.";
        const int cc = learn.controllerFor(host);
        if (cc >= 0) text << "\nMIDI CC " << cc << (pure ? " (inactive in PURE CS-80)" : "");
    }
    return text;
}

void ParameterComboBox::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onContextMenu)
    {
        contextPointer = true;
        if (canOpenContext()) onContextMenu();
        return;
    }
    if (!isEnabled() || contextPopupActive || contextPointer) return;
    ordinaryPointer = !e.mods.isPopupMenu();
    juce::ComboBox::mouseDown(e);
}

void ParameterComboBox::mouseDrag(const juce::MouseEvent& e)
{
    if (!contextPointer && !contextPopupActive) juce::ComboBox::mouseDrag(e);
}

void ParameterComboBox::mouseUp(const juce::MouseEvent& e)
{
    if (contextPointer)
    {
        contextPointer = false;
        // A blocked right press must not strand an earlier left sequence if
        // both buttons have been released together.
        if (!ordinaryPointer || e.mods.isLeftButtonDown()) return;
    }
    if (contextPopupActive) return;
    ordinaryPointer = false;
    juce::ComboBox::mouseUp(e);
}

void ParameterComboBox::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (isEnabled() && !contextPointer && !contextPopupActive) juce::ComboBox::mouseWheelMove(e, wheel);
}

void ParameterComboBox::showPopup()
{
    if (!contextPopupActive) juce::ComboBox::showPopup();
}

bool ParameterComboBox::keyPressed(const juce::KeyPress& key)
{
    if (!isEnabled() || contextPopupActive) return false;
    const auto mods = key.getModifiers();
    if (key.getKeyCode() == juce::KeyPress::F10Key && mods.isShiftDown()
        && !mods.isCtrlDown() && !mods.isAltDown() && !mods.isCommandDown() && onContextMenu)
    {
        if (!canOpenContext()) return false;
        onContextMenu();
        return true;
    }
    return juce::ComboBox::keyPressed(key);
}

juce::PopupMenu ParameterComboBox::buildContextMenu()
{
    juce::PopupMenu menu;
    const auto* target = contextParameter();
    if (target == nullptr) return menu;
    menu.addSectionHeader(target->getName(64) + "   " + target->getCurrentValueAsText());
    const int host = target->getParameterIndex();
    const bool enabled = canOpenContext();
    const bool pure = processor->getParamValue(P_parityMode) >= 0.5f;
    const auto& learn = processor->getMidiLearn();
    const auto mapGeneration = learn.generation();
    const int cc = learn.controllerFor(host);
    juce::Component::SafePointer<ParameterComboBox> safe(this);
    auto current = [safe, owner = processor, id = parameterId, host, generation = bindingGeneration] {
        return safe != nullptr && safe->contextMatches(owner, id, host, generation);
    };
    // A changed map invalidates MIDI actions, including A-to-B-to-A learn or
    // mapping replacements. This intentionally errs on the side of reopening
    // a menu after any concurrent map edit rather than overwriting newer intent.
    auto currentMap = [safe, current, generation = mapGeneration] {
        return current() && safe->processor->getMidiLearn().generation() == generation;
    };
    auto add = [&menu](const juce::String& title, bool available, std::function<void()> action) {
        juce::PopupMenu::Item item(title); item.isEnabled = available; item.action = std::move(action); menu.addItem(item);
    };
    if (pure)
        add("MIDI learn (off in PURE CS-80)", false, nullptr);
    else if (learn.learningParameter() == host)
        add("Cancel MIDI learn (waiting for a controller)", enabled, [safe, currentMap, host] {
            if (currentMap() && safe->processor->getMidiLearn().learningParameter() == host)
                safe->processor->getMidiLearn().cancelLearn();
        });
    else
        add(cc >= 0 ? "MIDI learn a different controller" : "MIDI learn: move a controller", enabled, [safe, currentMap, host] {
            if (currentMap() && safe->processor->getParamValue(P_parityMode) < 0.5f)
                safe->processor->getMidiLearn().armLearn(host);
        });
    if (cc >= 0)
        add("Forget MIDI CC " + juce::String(cc), enabled, [safe, currentMap, host, cc] {
            if (currentMap() && safe->processor->getMidiLearn().controllerFor(host) == cc)
                safe->processor->getMidiLearn().clearParameter(host);
        });
    menu.addSeparator();
    add("Reset to default  (" + target->getText(target->getDefaultValue(), 64) + ")", enabled, [safe, current] {
        if (!current()) return;
        if (auto* parameter = safe->contextParameter())
        {
            const float value = parameter->convertTo0to1(parameter->convertFrom0to1(parameter->getDefaultValue()));
            if (parameter->getValue() == value) return;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(value);
            parameter->endChangeGesture();
        }
    });
    return menu;
}

void ParameterComboBox::showContextMenu()
{
    if (!canOpenContext()) return;
    auto menu = buildContextMenu();
    contextPopupActive = true;
    juce::Component::SafePointer<ParameterComboBox> safe(this);
    presentContextMenu(std::move(menu), [safe] { if (safe != nullptr) safe->contextPopupActive = false; });
}

void ParameterComboBox::presentContextMenu(juce::PopupMenu menu, std::function<void()> dismissed)
{
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(240),
                       [dismissed = std::move(dismissed)](int) { dismissed(); });
}
}
