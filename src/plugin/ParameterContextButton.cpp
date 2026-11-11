#include "ParameterContextButton.h"
#include "PluginProcessor.h"

namespace cs80::ui
{
bool ParameterContextButton::bindParameterContext(Cs80AudioProcessor& owner, const juce::String& id)
{
    if (owner.getAPVTS().getParameter(id) == nullptr || ordinaryInputActive() || contextPointer || contextPopupActive)
        return false;
    if (parameterId.isEmpty()) originalDescription = getDescription();
    ++bindingGeneration;
    processor = &owner;
    parameterId = id;
    juce::Component::SafePointer<ParameterContextButton> safe(this);
    onContextMenu = [safe, generation = bindingGeneration] {
        if (safe != nullptr && safe->bindingGeneration == generation) safe->showContextMenu();
    };
    setWantsKeyboardFocus(true);
    setDescription(originalDescription + " Right-click or Shift+F10 opens MIDI learn and reset.");
    return true;
}

void ParameterContextButton::clearParameterContext()
{
    ++bindingGeneration;
    processor = nullptr;
    parameterId.clear();
    onContextMenu = {};
    // Preserve pointer/popup ownership until release/dismissal, including
    // when a context is cleared with its asynchronous menu still open.
    setDescription(originalDescription);
}

juce::RangedAudioParameter* ParameterContextButton::contextParameter() const
{
    return processor != nullptr ? processor->getAPVTS().getParameter(parameterId) : nullptr;
}

int ParameterContextButton::getContextHostParameterIndex() const
{
    if (const auto* target = contextParameter()) return target->getParameterIndex();
    return -1;
}

bool ParameterContextButton::canOpenContext() const
{
    return isEnabled() && !ordinaryInputActive() && !contextPopupActive && contextParameter() != nullptr;
}

bool ParameterContextButton::contextMatches(Cs80AudioProcessor* owner, const juce::String& id,
                                       int host, std::uint64_t generation) const
{
    // A selected context action may be dispatched before its dismissal callback.
    // Only ordinary activation blocks the action, not its own context menu.
    return isEnabled() && !ordinaryInputActive() && bindingGeneration == generation
        && processor == owner && parameterId == id && host >= 0 && getContextHostParameterIndex() == host;
}

juce::String ParameterContextButton::getTooltip()
{
    auto text = juce::Button::getTooltip();
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

bool ParameterContextButton::ordinaryInputActive() const
{
    // JUCE Return posts a click; registered shortcuts can hold buttonDown.
    // Custom selectors act immediately, so also respect physically held keys
    // while focused rather than opening a menu midway through their sequence.
    return ordinaryPointer || queuedKeyboardActions != 0 || isDown()
        || (hasKeyboardFocus(true)
            && (juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey)
                || juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::returnKey)
                || juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::leftKey)
                || juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::rightKey)));
}

void ParameterContextButton::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onContextMenu)
    {
        contextPointer = true;
        if (canOpenContext()) onContextMenu();
        return;
    }
    if (!isEnabled() || contextPopupActive || contextPointer || ordinaryPointer || queuedKeyboardActions != 0) return;
    ordinaryPointer = true;
    ordinaryMouseDown(e);
    juce::Button::mouseDown(e);
}

void ParameterContextButton::mouseDrag(const juce::MouseEvent& e)
{
    if (ordinaryPointer && !contextPointer && !contextPopupActive) juce::Button::mouseDrag(e);
}

void ParameterContextButton::mouseUp(const juce::MouseEvent& e)
{
    if (contextPointer)
    {
        contextPointer = false;
        if (!ordinaryPointer || e.mods.isLeftButtonDown()) return;
    }
    if (!ordinaryPointer) return;
    juce::Component::SafePointer<ParameterContextButton> safe(this);
    juce::Button::mouseUp(e);
    if (safe == nullptr) return;
    ordinaryPointer = false;
    ordinaryMouseUp(e);
}

bool ParameterContextButton::keyPressed(const juce::KeyPress& key)
{
    if (!isEnabled() || contextPopupActive || contextPointer) return false;
    const auto mods = key.getModifiers();
    if (key.getKeyCode() == juce::KeyPress::F10Key && mods.isShiftDown()
        && !mods.isCtrlDown() && !mods.isAltDown() && !mods.isCommandDown() && onContextMenu)
    {
        if (!canOpenContext()) return false;
        onContextMenu();
        return true;
    }
    // A selector's pressed half must never override a concurrent keyboard edit.
    if (ordinaryPointer) return false;
    return ordinaryKeyPressed(key);
}

bool ParameterContextButton::ordinaryKeyPressed(const juce::KeyPress& key)
{
    if (!key.isKeyCode(juce::KeyPress::returnKey)) return juce::Button::keyPressed(key);
    ++queuedKeyboardActions;
    const bool handled = juce::Button::keyPressed(key);
    juce::Component::SafePointer<ParameterContextButton> safe(this);
    // Queued after JUCE's click, including its disabled-before-delivery path.
    juce::MessageManager::callAsync([safe] {
        if (safe != nullptr) --safe->queuedKeyboardActions;
    });
    return handled;
}

juce::PopupMenu ParameterContextButton::buildContextMenu()
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
    juce::Component::SafePointer<ParameterContextButton> safe(this);
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

void ParameterContextButton::showContextMenu()
{
    if (!canOpenContext()) return;
    auto menu = buildContextMenu();
    contextPopupActive = true;
    juce::Component::SafePointer<ParameterContextButton> safe(this);
    presentContextMenu(std::move(menu), [safe] { if (safe != nullptr) safe->contextPopupActive = false; });
}

void ParameterContextButton::presentContextMenu(juce::PopupMenu menu, std::function<void()> dismissed)
{
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(240),
                       [dismissed = std::move(dismissed)](int) { dismissed(); });
}
}
