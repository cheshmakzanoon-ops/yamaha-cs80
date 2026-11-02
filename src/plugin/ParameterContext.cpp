#include "ParameterContext.h"

#include "ArtCommon.h"
#include "PluginProcessor.h"
#include "UiComponents.h"

#include <cmath>

namespace cs80::ui
{
ParameterContext::ParameterContext(Cs80AudioProcessor& p, juce::Component& c, const juce::String& id,
                                   int index, juce::StringArray labels)
    : processor(p), anchor(&c), parameter(p.getAPVTS().getParameter(id)), displayIndex(index), choices(std::move(labels))
{
    jassert(parameter != nullptr);
    if (choices.isEmpty())
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter)) choices = choice->choices;
    // This ID also makes the bound host parameter discoverable to accessibility/UI tests.
    c.getProperties().set("parameterContextId", id);
}

ParameterContext::~ParameterContext() = default;

float ParameterContext::value() const { return parameter != nullptr ? parameter->convertFrom0to1(parameter->getValue()) : 0.0f; }
int ParameterContext::getHostParameterIndex() const { return parameter != nullptr ? parameter->getParameterIndex() : -1; }
juce::String ParameterContext::getParameterId() const { return parameter != nullptr ? parameter->getParameterID() : juce::String(); }

juce::String ParameterContext::format(float physical) const
{
    if (parameter == nullptr) return {};
    if (!choices.isEmpty())
        return choices[juce::jlimit(0, choices.size() - 1, juce::roundToInt(physical - parameter->getNormalisableRange().start))];
    if (displayIndex >= 0) return formatParamValue(displayIndex, physical);
    return parameter->getText(parameter->convertTo0to1(physical), 80);
}

bool ParameterContext::parse(const juce::String& input, float& physical) const
{
    if (parameter == nullptr) return false;
    const auto text = input.trim();
    if (text.isEmpty() || text.length() > 80) return false;
    if (!choices.isEmpty())
    {
        int match = -1;
        for (int i = 0; i < choices.size(); ++i)
        {
            if (choices[i].equalsIgnoreCase(text)) { match = i; break; }
            if (choices[i].startsWithIgnoreCase(text))
            {
                if (match >= 0) return false;
                match = i;
            }
        }
        if (match >= 0)
        {
            physical = parameter->getNormalisableRange().start + (float) match;
            return true;
        }
    }
    if (displayIndex >= 0) return parseParamValue(displayIndex, text, value(), physical);
    if (!text.containsOnly("0123456789+-. ") || !text.containsAnyOf("0123456789")) return false;
    physical = parameter->getNormalisableRange().snapToLegalValue(text.getFloatValue());
    return std::isfinite(physical);
}

void ParameterContext::setNormalised(float normalised)
{
    if (parameter == nullptr || anchor == nullptr) return;
    normalised = juce::jlimit(0.0f, 1.0f, normalised);
    if (normalised == parameter->getValue()) return;
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(normalised);
    parameter->endChangeGesture();
    anchor->repaint();
}

bool ParameterContext::applyTypedValue(const juce::String& text)
{
    float physical = 0.0f;
    if (!parse(text, physical)) return false;
    setNormalised(parameter->convertTo0to1(physical));
    return true;
}
void ParameterContext::resetToDefault() { if (parameter != nullptr) setNormalised(parameter->getDefaultValue()); }
void ParameterContext::copyValue() { juce::SystemClipboard::copyTextToClipboard(format(value())); }
bool ParameterContext::pasteValue() { return applyTypedValue(juce::SystemClipboard::getTextFromClipboard().upToFirstOccurrenceOf("\n", false, false)); }

juce::PopupMenu ParameterContext::buildContextMenu()
{
    juce::PopupMenu menu;
    if (parameter == nullptr) return menu;
    menu.addSectionHeader(parameter->getName(80) + "   " + format(value()));
    const auto weak = weak_from_this();
    auto add = [&](const juce::String& text, bool enabled, auto fn)
    {
        juce::PopupMenu::Item item(text);
        item.isEnabled = enabled;
        item.action = [weak, fn] { if (auto self = weak.lock(); self != nullptr && self->anchor != nullptr) fn(*self); };
        menu.addItem(std::move(item));
    };
    auto& learn = processor.getMidiLearn();
    const int host = getHostParameterIndex(), cc = learn.controllerFor(host);
    const bool pure = processor.getParamValue(cs80::P_parityMode) >= 0.5f;
    if (pure)
        add("MIDI learn (off in PURE CS-80)", false, [](auto&) {});
    else if (learn.learningParameter() == host)
        add("Cancel MIDI learn (waiting for a controller)", true, [](auto& self) { self.processor.getMidiLearn().cancelLearn(); });
    else
        add(cc >= 0 ? "MIDI learn a different controller" : "MIDI learn: move a controller", host >= 0,
            [](auto& self) { if (self.processor.getParamValue(cs80::P_parityMode) < 0.5f) self.processor.getMidiLearn().armLearn(self.getHostParameterIndex()); });
    if (cc >= 0)
        add("Forget MIDI CC " + juce::String(cc), true, [](auto& self) { self.processor.getMidiLearn().clearParameter(self.getHostParameterIndex()); });
    menu.addSeparator();
    add("Reset to default  (" + format(parameter->convertFrom0to1(parameter->getDefaultValue())) + ")", true, [](auto& self) { self.resetToDefault(); });
    add("Type a value...", true, [](auto& self) { self.showValueEntry(); });
    add("Copy value  (" + format(value()) + ")", true, [](auto& self) { self.copyValue(); });
    float pasted = 0.0f;
    const bool pastable = parse(juce::SystemClipboard::getTextFromClipboard().upToFirstOccurrenceOf("\n", false, false), pasted);
    add(pastable ? "Paste value  (" + format(pasted) + ")" : juce::String("Paste value"), pastable, [](auto& self) { self.pasteValue(); });
    menu.addSeparator();
    add("What is this control?", true, [](auto& self) { self.showHelp(); });
    return menu;
}

void ParameterContext::showContextMenu()
{
    if (anchor != nullptr) buildContextMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(anchor).withMinimumWidth(240));
}

void ParameterContext::showValueEntry()
{
    if (anchor == nullptr || parameter == nullptr) return;
    auto* top = anchor->getTopLevelComponent();
    if (top == nullptr || top == anchor.getComponent()) return;
    auto editor = std::make_unique<juce::TextEditor>();
    auto* raw = editor.get();
    raw->setComponentID("parameter_value_entry");
    raw->setTitle("Type a value for " + parameter->getName(80));
    raw->setDescription("Press Return to apply; Escape cancels. Current value: " + format(value()));
    raw->setFont(art::displayFont(14.0f, juce::Font::bold));
    raw->setText(format(value()), juce::dontSendNotification);
    const auto area = top->getLocalArea(anchor, anchor->getLocalBounds());
    raw->setBounds(juce::Rectangle<int>(juce::jmax(144, area.getWidth()), 30).withCentre(area.getCentre()).constrainedWithin(top->getLocalBounds()));
    const auto weak = weak_from_this();
    const auto close = [weak]
    {
        juce::MessageManager::callAsync([weak] { if (auto self = weak.lock()) self->valueEditor.reset(); });
    };
    raw->onReturnKey = [weak, raw, close]
    {
        if (auto self = weak.lock())
        {
            if (self->applyTypedValue(raw->getText())) close();
            else raw->setColour(juce::TextEditor::outlineColourId, juce::Colour(0xffe0604a));
        }
    };
    raw->onEscapeKey = close;
    raw->onFocusLost = close;
    valueEditor = std::move(editor);
    top->addAndMakeVisible(*raw);
    raw->toFront(true);
    raw->grabKeyboardFocus();
    raw->selectAll();
}

void ParameterContext::showHelp()
{
    if (anchor == nullptr) return;
    if (displayIndex >= 0) { showControlHelp(*anchor, displayIndex); return; }
    auto text = std::make_unique<juce::TextEditor>();
    text->setReadOnly(true);
    text->setMultiLine(true);
    text->setFont(art::displayFont(14.0f));
    text->setText("VOICE SOURCE\n\nChoose a tone tab, one of this rank's memories, or PANEL. PANEL uses the controls on the main row; each memory keeps its own controls.\n\nRight-click a selector to learn a MIDI controller, reset, type, copy or paste the source choice.");
    text->setSize(320, 190);
    auto* top = anchor->getTopLevelComponent();
    juce::CallOutBox::launchAsynchronously(std::move(text), top->getLocalArea(anchor, anchor->getLocalBounds()), top).setComponentID("control_help");
}

bool ParameterContext::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showContextMenu(); return true; }
    if (e.mods.isCommandDown() && e.mods.isLeftButtonDown()) { showValueEntry(); return true; }
    return false;
}

bool ParameterContext::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::F1Key) { showHelp(); return true; }
    if (key.getKeyCode() == juce::KeyPress::F10Key && key.getModifiers().isShiftDown()) { showContextMenu(); return true; }
    return false;
}
} // namespace cs80::ui
