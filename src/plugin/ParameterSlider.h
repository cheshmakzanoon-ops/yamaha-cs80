#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cs80::ui::modern_layer
{
// Keep ordinary JUCE pointer/text behavior; add only balanced, guarded
// keyboard edits for attached host parameters.
class ParameterSlider final : public juce::Slider
{
public:
    using juce::Slider::Slider;
    bool keyPressed(const juce::KeyPress& key) override
    {
        const bool arrow = key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey
            || key == juce::KeyPress::upKey || key == juce::KeyPress::downKey;
        if (arrow && !key.getModifiers().isAnyModifierKeyDown())
        {
            if (!isEnabled() || gestureActive) return false;
            juce::Slider::ScopedDragNotification gesture(*this);
            return juce::Slider::keyPressed(key);
        }
        return juce::Slider::keyPressed(key);
    }
private:
    void startedDragging() override
    {
        gestureActive = true;
        juce::Slider::startedDragging();
    }
    void stoppedDragging() override
    {
        gestureActive = false;
        juce::Slider::stoppedDragging();
    }
    bool gestureActive = false;
};

} // namespace cs80::ui::modern_layer
