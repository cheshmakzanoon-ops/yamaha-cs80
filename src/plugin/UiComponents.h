#pragma once

#include "core/Params.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
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
extern const juce::Colour gold;        // engraved gold silkscreen lines
extern const juce::Colour capCream;    // original slider-cap / tab colours
extern const juce::Colour capGreen;
extern const juce::Colour capPink;
extern const juce::Colour capYellow;
extern const juce::Colour capBlack;
extern const juce::Colour capGrey;

juce::String formatParamValue(int parameterIndex, float value);
juce::String shortParamName(int parameterIndex);

// 2.5D drawing helpers shared by the chassis, controls and overlay sheets.
// Light comes from the upper left; depth is shown by bevels and layered
// shadows (no per-frame blur, so repainting 140 controls stays cheap).
void softShadow(juce::Graphics&, juce::Rectangle<float>, float radius,
                float dx, float dy, float spread, float alpha);
void drawChiclet(juce::Graphics&, juce::Rectangle<float> face, juce::Colour colour, float depth);
void drawRocker(juce::Graphics&, juce::Rectangle<float> area, bool on, bool hovered);
void drawKnob(juce::Graphics&, juce::Point<float> centre, float radius, float angle);
void drawScrew(juce::Graphics&, juce::Point<float> centre, float radius, float turn);
void drawWell(juce::Graphics&, juce::Rectangle<float>, float radius);
void etchLine(juce::Graphics&, float x1, float y1, float x2, float y2, float alpha = 0.85f);
void etchRect(juce::Graphics&, juce::Rectangle<float>, float radius, float alpha = 0.85f);
juce::Colour panelFaceAt(float y, float top, float bottom);
void fillPanelFace(juce::Graphics&, juce::Rectangle<float>);
void paintSheet(juce::Graphics&, juce::Rectangle<float>, float radius);

// ---- Shared drawing helpers (round 2). Light comes from the upper left; none uses a blur
// filter. Helpers named draw* paint immediately; call them from a cached juce::Image when the
// same artwork repaints often (the control look-and-feel does exactly that).

// Bevelled plate: lit top-left edge and dark bottom-right edge (raised), or the reverse (recessed).
void drawBevelRect(juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour base,
                   float bevel, bool raised);
// Fine-grained brushed steel/brass rectangle; grain runs along its long axis unless forced.
void drawBrushedMetal(juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour tint,
                      bool horizontalGrain);
// Smoked-glass window (display, readout, bezel): tinted dark fill, inner shadow, diagonal sheen.
void drawGlassPanel(juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour tint,
                    float sheen = 1.0f);
// Printed/engraved legend: dark drop shadow under light ink, or a lit lower edge under dark ink.
void drawEngravedText(juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                      const juce::Font&, juce::Justification,
                      juce::Colour ink = juce::Colour(0xffe6dcbc));
// Soft radial light bloom (one gradient fill) for lamps, lit plastic and contact points.
void drawLampGlow(juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour colour,
                  float intensity);
// Round indicator lamp in a brass bezel: dark glass when off, glowing jewel when on.
void drawJewelLamp(juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour colour,
                   bool on);

enum class CapState { idle, hover, dragging };
// Moulded plastic fader cap (lit top face, darker front/right faces, grip grooves, cast shadow).
// faceWidth is the width of the lit top face; 24.5 gives the nominal size, other widths scale it.
void drawFaderCap(juce::Graphics&, juce::Point<float> faceCentre, float faceWidth,
                  juce::Colour colour, CapState state);
// Recessed fader slot with a metal rail between two y positions, centred on x.
void drawFaderSlot(juce::Graphics&, float centreX, float top, float bottom, float scale);
// Backlit translucent tone-selector tab inside its bounds; lit tabs bleed light onto the bezel.
void drawTonePlate(juce::Graphics&, juce::Rectangle<float> bounds, juce::Colour colour,
                   bool lit, bool pressed, bool hovered);
// Raised satin-black key plate with optional amber jewel lamp (software pages, toolbar).
void drawSatinPlate(juce::Graphics&, juce::Rectangle<float> bounds, bool on, bool pressed,
                    bool hovered, bool withLamp);

// Dark figured walnut with pores, grain, a broad varnish sheen and an optional lit bevel. The
// grain runs along the long axis when vertical is true (side cheeks) or horizontally otherwise
// (end blocks). The seed picks the figure, so neighbouring parts do not repeat. The result is
// cached as an image keyed by size, seed and orientation, so calling it every paint is cheap.
void drawWalnutGrain(juce::Graphics&, juce::Rectangle<float>, bool vertical, int seed,
                     bool bevel = true);
// Gold-accent primary button plate (what a Button with the "cs80Accent" property draws).
void drawAccentPlate(juce::Graphics&, juce::Rectangle<float> bounds, bool pressed, bool hovered);

// Original vector rendering of the hardware's colored slider caps and rocker
// tabs. Rotary controls remain available for the software extension pages.
//
// Component properties understood by the look and feel (set with getProperties().set):
//   "hardwareTab"  (bool, Button)  backlit translucent tone-selector tab; the button colour is the plastic.
//   "cs80Accent"   (bool, Button)  gold-accent primary button (for example SAVE in the toolbar).
//   "hardwareRotary" (bool, Slider) machined knob with engraved tick dots instead of a lit value ring.
//   "faderLegendTop"/"faderLegendBottom" (String, Slider) end legends engraved beside a fader.
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    ~LookAndFeel() override;

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

    // Round 2 additions.
    int getSliderThumbRadius(juce::Slider&) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;
    void drawBubble(juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip,
                    const juce::Rectangle<float>& body) override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator,
                           bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader(juce::Graphics&, const juce::Rectangle<int>& area,
                                    const juce::String& sectionName) override;
    juce::Font getPopupMenuFont() override;
    int getPopupMenuBorderSize() override;
    void fillTextEditorBackground(juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override;
};

// Native slider semantics and accessibility are retained. Only the focus
// decoration is added; APVTS attachments continue to own value gestures.
class FocusSlider final : public juce::Slider
{
public:
    FocusSlider()
    {
        setWantsKeyboardFocus(true);
        setRepaintsOnMouseActivity(true);
    }
    ~FocusSlider() override;
    // Optional drag-bubble text. Without them the bubble shows the title and the slider's own
    // value text. Only shown while the pointer drags; never changes bounds or accessibility.
    std::function<juce::String()> bubbleTitleText;
    std::function<juce::String(double)> bubbleValueText;
    void valueChanged() override { juce::Slider::valueChanged(); updateDragBubble(); }
    bool isGestureInProgress() const noexcept { return gestureActive; }
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
        showDragBubble();
    }
    void stoppedDragging() override
    {
        gestureActive = false;
        hideDragBubble();
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
    void showDragBubble();
    void hideDragBubble();
    void updateDragBubble();
    bool gestureActive = false;
    std::unique_ptr<juce::Component> dragBubble;
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
    // Message-thread only. Keep an in-flight host gesture on its original parameter.
    bool bindParameter(int index);

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

// Pointer-to-pressure mapping for the on-screen keys. After the strike, pushing
// the pointer further down the key presses harder (full pressure after 40% of the
// key height); drawing back up eases off. This stands in for the original per-key
// pressure sensor and does not model its force law.
class KeyPressureGesture
{
public:
    static constexpr float kTravelFraction = 0.4f;

    void begin(int note, float y) noexcept { activeNote = note; anchorY = y; }
    void end() noexcept { activeNote = -1; }
    int getNote() const noexcept { return activeNote; }

    // Pressure 0..1 for the key under the pointer. Sliding onto a different key is
    // a new strike, not a pressure change, and returns -1.
    float drag(int note, float y, float keyHeight) noexcept
    {
        if (note != activeNote)
        {
            begin(note, y);
            return -1.0f;
        }
        const float travel = kTravelFraction * (keyHeight > 1.0f ? keyHeight : 1.0f);
        const float p = (y - anchorY) / travel;
        return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
    }

private:
    int activeNote = -1;
    float anchorY = 0.0f;
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
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    std::function<void(float)> onValueChange;

private:
    void setFromX(float x);
    float value = 0.0f;
    float anchorX = 0.0f;
    float hoverX = -1.0f;
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
    // Moves the control without notifying onValueChange (initial or restored position).
    void setValue(float newValue);

private:
    void setFromY(float y);
    juce::Rectangle<float> trackArea() const;
    juce::String wheelLabel;
    bool spring;
    float value = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceWheel)
};

} // namespace cs80::ui
