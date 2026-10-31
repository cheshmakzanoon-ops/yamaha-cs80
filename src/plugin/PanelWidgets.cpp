#include "PanelWidgets.h"

namespace cs80::panel
{
PanelSection::PanelSection(Cs80AudioProcessor& p) : processor(p)
{
    setOpaque(false);
    setInterceptsMouseClicks(false, true);
}

PanelSection::~PanelSection() = default;

void PanelSection::layout(juce::Rectangle<float> designArea, float sx, float sy)
{
    auto r = [sx, sy](float x, float y, float w, float h) {
        return juce::Rectangle<int>(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                                    juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };
    const auto area = r(designArea.getX(), designArea.getY(), designArea.getWidth(), designArea.getHeight());
    setBounds(area);
    // Round each control in editor space first, then make it local: identical pixels to an editor child.
    for (const auto& item : placements)
        item.component->setBounds(r(item.bounds.getX(), item.bounds.getY(),
                                    item.bounds.getWidth(), item.bounds.getHeight()) - area.getPosition());
    afterLayout();
}

void PanelSection::applyCaptions(bool small)
{
    for (auto& pair : captionPairs)
        pair.knob->setCaption(small ? pair.shortForm : pair.full);
}

void PanelSection::paintArt(juce::Graphics& g) const
{
    for (const auto& b : brackets)
        art::bracket(g, b.title, b.x, b.ruleY, b.w, b.tick, 9.5f, b.shortTitle);
}

void PanelSection::addBracket(const juce::String& title, float x, float ruleY, float w, float tick,
                              const juce::String& shortTitle)
{
    brackets.push_back({ title, shortTitle, x, ruleY, w, tick });
}

void PanelSection::place(juce::Component& component, float x, float y, float w, float h)
{
    placements.push_back({ &component, { x, y, w, h } });
}

ui::ParamKnob& PanelSection::addKnob(int parameter, float x, float y, float w, float h, const juce::String& name)
{
    auto knob = std::make_unique<ui::ParamKnob>(processor, parameter, name);
    auto* raw = knob.get();
    raw->getSlider().getProperties().set("hardwareRotary", true);
    addAndMakeVisible(*raw);
    boundControls.emplace_back(raw, parameter);
    place(*raw, x, y, w, h);
    knobs.push_back(std::move(knob));
    return *raw;
}

ui::ParamKnob& PanelSection::addFader(int parameter, float x, float y, float w, float h,
                                      const juce::String& label, juce::Colour colour)
{
    auto& control = addKnob(parameter, x, y, w, h, label);
    control.setHardwareSlider(colour);
    return control;
}

ui::LedButton& PanelSection::addLed(int parameter, const juce::String& label, float x, float y, float w, float h)
{
    auto button = std::make_unique<ui::LedButton>(label);
    auto* raw = button.get();
    addAndMakeVisible(*raw);
    buttonAttachments.push_back(
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.getAPVTS(), Cs80AudioProcessor::parameterId(parameter), *raw));
    const auto channelName = parameter >= cs80::P_numGlobals && parameter < cs80::P_legacyNumParams
        ? juce::String(parameter < cs80::P_ch2Start ? "Channel I " : "Channel II ") : juce::String();
    raw->setTitle(channelName + juce::String(cs80::paramDef(parameter).name));
    raw->setTooltip(raw->getTitle() + (raw->getToggleState() ? ": ON" : ": OFF"));
    boundControls.emplace_back(raw, parameter);
    place(*raw, x, y, w, h);
    leds.push_back(std::move(button));
    return *raw;
}

void PanelSection::shortCaption(ui::ParamKnob& knob, const juce::String& full, const juce::String& shortForm)
{
    captionPairs.push_back({ &knob, full, shortForm });
}
} // namespace cs80::panel
