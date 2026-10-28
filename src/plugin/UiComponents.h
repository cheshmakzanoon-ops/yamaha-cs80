#pragma once

#include "core/Params.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

class Cs80AudioProcessor;

namespace cs80::ui
{

extern const juce::Colour ivory;
extern const juce::Colour ivoryDim;
extern const juce::Colour amber;
extern const juce::Colour amberBright;
extern const juce::Colour teal;
extern const juce::Colour charcoal;
extern const juce::Colour panelDark;
extern const juce::Colour panelMid;
extern const juce::Colour lineColour;

juce::String formatParamValue(int parameterIndex, float value);
juce::String shortParamName(int parameterIndex);

// Original vector rendering of the hardware's colored slider caps and rocker
// tabs. Rotary controls remain available for the software extension pages.
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
};

// Native slider semantics and accessibility are retained. Only the focus
// decoration is added; APVTS attachments continue to own value gestures.
class FocusSlider final : public juce::Slider
{
public:
    FocusSlider() { setWantsKeyboardFocus(true); }
    bool keyPressed(const juce::KeyPress& key) override
    {
        const bool arrow = key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey
            || key == juce::KeyPress::upKey || key == juce::KeyPress::downKey;
        if (isEnabled() && arrow && !key.getModifiers().isAnyModifierKeyDown() && !gestureActive)
        {
            // JUCE 8.0.6 arrow keys change values without drag notifications.
            // Attachments need these to bracket host automation, just as for
            // pointer, wheel and accessibility edits.
            juce::Slider::ScopedDragNotification gesture(*this);
            return juce::Slider::keyPressed(key);
        }
        return juce::Slider::keyPressed(key);
    }
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
    void focusGained(juce::Component::FocusChangeType cause) override
    {
        juce::Slider::focusGained(cause);
        repaint();
    }
    void focusLost(juce::Component::FocusChangeType cause) override
    {
        juce::Slider::focusLost(cause);
        repaint();
    }
    void paintOverChildren(juce::Graphics& g) override
    {
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 2.0f, 1.5f);
        }
    }
private:
    bool gestureActive = false;
};

// Parameter-bound rotary or hardware linear slider with a unit-aware readout.
class ParamKnob final : public juce::Component
{
public:
    ParamKnob(Cs80AudioProcessor& processor, int parameterIndex,
              juce::String displayName = {});
    ~ParamKnob() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    juce::Slider& getSlider() noexcept { return slider; }
    int getParameterIndex() const noexcept { return parameterIndex; }
    void setHardwareSlider(juce::Colour capColour);

private:
    Cs80AudioProcessor& processor;
    int parameterIndex;
    juce::String name;
    bool hardwareSlider = false;
    FocusSlider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamKnob)
};

// A parameter-backed illuminated switch. Used for waveforms, filters,
// effects, and performance modes.
class LedButton final : public juce::Button
{
public:
    explicit LedButton(const juce::String& text, bool momentary = false);

    void paintButton(juce::Graphics&, bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

    void paintOverChildren(juce::Graphics& g) override
    {
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 2.0f, 1.5f);
        }
    }

    juce::Colour accent { amber };
    bool hardwareTab = false;
};

// The CS-80 ribbon is a continuous position controller, not a mod wheel.
// Initial contact establishes zero; displacement changes pitch until release.
class RibbonStrip final : public juce::Component
{
public:
    RibbonStrip();

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    std::function<void(float)> onValueChange;

private:
    void setFromX(float x);
    float value = 0.0f;
    float anchorX = 0.0f;
    bool touching = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RibbonStrip)
};

// A narrow performance wheel. Pitch is spring-loaded; modulation latches.
class PerformanceWheel final : public juce::Component
{
public:
    PerformanceWheel(const juce::String& label, bool springLoaded);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    std::function<void(float)> onValueChange;
    float getValue() const noexcept { return value; }

private:
    void setFromY(float y);
    juce::String wheelLabel;
    bool spring;
    float value = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceWheel)
};

} // namespace cs80::ui
