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
// One caption style for control titles (knob, fader and dropdown headings), so neighbouring
// captions share size, colour and baseline. The text is centred vertically inside `area`; pass
// the same area height for every caption in a row. fontSize 0 derives the size from the area.
juce::Font controlCaptionFont(float areaHeight);
void drawControlCaption(juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                        juce::Justification = juce::Justification::centred, float fontSize = 0.0f);
// Readout pill (smoked glass, amber digits) sized to the text, centred in `area`; 10 to 12 px type.
void drawValuePill(juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                   float fontSize = 0.0f);
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
// Editor scale relative to the 1600 px design width (top-level width / 1600; 1 when detached).
float uiDesignScale(const juce::Component&);
// On-screen type height for a design-space size at the current editor scale, never below
// floorScreenPx (8 on screen by default). Use it for every legend so nothing falls under 8 px.
float designFontHeight(const juce::Component&, float designSize, float floorScreenPx = 8.0f);
// Returns `font` with its horizontal scale set so `text` fits `width`: first by shrinking toward
// floorHeight, then by squeezing, and never below minScale of squeeze unless there is no other way.
juce::Font fitFontToWidth(const juce::Font& font, const juce::String& text, float width,
                          float minScale = 0.85f, float floorHeight = 8.0f);
// Same, but never squeezes below minScale: `fitted` reports whether the text fits at all. When it
// does not, shorten the text (see fitLegendText) instead of crushing the type.
juce::Font fitFontToWidth(const juce::Font& font, const juce::String& text, float width,
                          float minScale, float floorHeight, bool& fitted);
// True when `text` fits `width` at floorHeight type with at most (1 - minScale) of squeeze.
bool legendFits(const juce::Font& font, const juce::String& text, float width,
                float minScale = 0.85f, float floorHeight = 8.0f);
// Printed short form of a legend word (FUNCTION -> FUNC, SUSTAIN -> SUS, ATTACK -> ATK...), or the
// word itself when it has none.
juce::String legendAbbreviation(const juce::String& text);
// The text as it should be printed in `width`: the word when it fits, else its abbreviation, else
// the longest leading part that fits. Never needs a squeeze below minScale at floorHeight type.
juce::String fitLegendText(const juce::Font& font, const juce::String& text, float width,
                           float minScale = 0.85f, float floorHeight = 8.0f);
// Readout text for a narrow column: the normal string when it fits, otherwise progressively
// compact forms ("0.38s", "+.12 oct", "+.12o") so that it never needs a squeeze below minScale.
juce::String formatParamValueFit(int parameterIndex, float value, const juce::Font& font, float width,
                                 float minScale = 0.85f, float floorHeight = 8.0f);
// Descriptive parameter name without the channel ("VCA Attack", "Initial Touch Level", "HPF Cutoff"),
// for tooltips, descriptions and drag bubbles. Accessibility titles do not change.
juce::String parameterLongName(int parameterIndex);
// Two-facet rocker paddle (lit facet, shaded facet, pivot ridge about 55 percent down) with an
// illuminated legend lens on the active facet and a small LED; no light spills outside bounds.
// `lens` is the lens colour when lit; the legend itself is drawn by the caller or LedButton.
void drawRockerSwitch(juce::Graphics&, juce::Rectangle<float> bounds, bool on, bool pressed,
                      bool hovered, juce::Colour lens, bool withLed);
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
// Keyboard focus ring for plates and tabs: gold outside with a dark inner ring, visible on lit plastic.
void drawFocusRing(juce::Graphics&, juce::Rectangle<float> bounds);
// Soft cached bloom (no blur filter) of `colour` around `bounds`, reaching `reach` px beyond it.
// Intended for lit tabs, lamps and rocker lenses; it is a few translucent layers in one image.
void drawLightBloom(juce::Graphics&, juce::Rectangle<float> bounds, float reach, juce::Colour colour,
                    float strength = 1.0f);
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

    // Round 4: a pointing hand over buttons, sliders and drop-downs unless the component set a
    // cursor of its own.
    juce::MouseCursor getMouseCursorFor(juce::Component&) override;
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
            drawFocusRing(g, getLocalBounds().toFloat());
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
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    juce::Slider& getSlider() noexcept { return slider; }
    int getParameterIndex() const noexcept { return parameterIndex; }
    void setHardwareSlider(juce::Colour capColour);
    // Moulded-cap fader, vertical (same as setHardwareSlider) or a compact horizontal strip with
    // the caption left, the readout right and the fader below: for envelope and level style
    // parameters on the software pages. A horizontal fader wants about 120 x 36 px or more.
    void setCapFader(juce::Colour capColour, bool horizontal);
    // Message-thread only. Keep an in-flight host gesture on its original parameter.
    bool bindParameter(int index);
    // Replaces the painted caption only (for example FUNC at small editor sizes). Titles, accessibility
    // names, tooltips and drag-bubble names are unchanged. An empty string restores the constructor name.
    void setCaption(const juce::String& painted);
    // Smallest painted type for the caption and the value chip, in screen pixels. 0 (the default) keeps
    // the automatic sizing; a value such as 12.5 also grows the caption and readout bands to fit. Names
    // and readouts are never squeezed below 85 percent: a word that does not fit is abbreviated.
    void setMinCaptionHeight(float px);
    float getMinCaptionHeight() const noexcept { return minCaption; }
    // Aligns a rotary with the faders beside it. Give the knob the same top and height as those faders and
    // pass the pixel width of one of them: the caption and readout are then painted in the same bands and
    // the same type as a fader column of that width, and the dial sits centred between them. 0 (default)
    // keeps the knob's own layout. Call again after a resize (the width is in screen pixels).
    void setLegendColumnWidth(float referenceFaderWidth);
    float getLegendColumnWidth() const noexcept { return legendColumnWidth; }
    // A rotary whose value ring and fill start at 12 o'clock (A4 tuning, bend ranges, macros) and run
    // left or right of it. Default false (the fill starts at the minimum).
    void setBipolar(bool isBipolar);
    bool isBipolar() const noexcept { return bipolar; }

private:
    Cs80AudioProcessor& processor;
    int parameterIndex;
    juce::String name;
    juce::String captionOverride;
    float minCaption = 0.0f;
    float legendColumnWidth = 0.0f;
    bool bipolar = false;
    bool hardwareSlider = false;
    bool horizontalFader = false;
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
            drawFocusRing(g, getLocalBounds().toFloat());
    }

    // Tooltips: the control's own sentence first, then its state on a last line ("PLUG: ON"). Call this
    // (not Button::setTooltip) to keep a long tip; the state is appended again on every toggle. A tip
    // of the plain form "<title>: ON|OFF" is treated as no custom tip.
    void setTooltip(const juce::String& customTip);
    const juce::String& getCustomTooltip() const noexcept { return customTip; }

    juce::Colour accent { amber };
    bool hardwareTab = false;
    // Draw as a compact two-facet rocker paddle with a lit legend lens and an LED instead of the
    // satin plate (for footswitch, assign and output style switches). Off by default.
    bool rockerStyle = false;

private:
    void paintRocker(juce::Graphics&, bool hovered, bool down, float physicalScale, bool deckTab);
    void refreshTooltip();
    juce::String customTip;
};

// Optional light layer: add it to the editor BEFORE the controls (so it sits behind them), give it
// the full editor bounds, and lit tone tabs, deck tabs and rocker switches under the same parent
// spill a soft warm bloom onto the panel around them. It finds the controls itself, takes no mouse
// or focus, is hidden from accessibility and repaints only when a lamp changes.
class LightSpill final : public juce::Component, private juce::Timer
{
public:
    LightSpill();
    ~LightSpill() override;
    void paint(juce::Graphics&) override;
    // Rescans now (a 10 Hz timer does it too); repaints the lamps that changed.
    void refresh();
    float strength = 1.0f;

private:
    struct Source { juce::Rectangle<int> bounds; juce::Colour colour; float reach = 6.0f; float gain = 1.0f; };
    void timerCallback() override;
    void collect(std::vector<Source>& out) const;
    void gather(const juce::Component& parent, std::vector<Source>& out) const;
    std::vector<Source> sources;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LightSpill)
};

// Roving focus for a row of radio-style buttons (the tone selector): the row is one Tab stop,
// Left/Right/Up/Down (and Home/End) move focus along it without changing the selection, and
// Space or Return selects as usual. Call add() for each button in order; keep the group alive as long
// as the buttons. The tab that takes Tab focus is the selected one, or the first.
class RovingFocusGroup final : private juce::KeyListener, private juce::Timer
{
public:
    RovingFocusGroup() = default;
    ~RovingFocusGroup() override;
    void add(juce::Button&);
    void clear();
    // Optionally split a long list into rows: Up/Down move by `columns` buttons.
    void setColumns(int columns) noexcept { cols = juce::jmax(1, columns); }

private:
    bool keyPressed(const juce::KeyPress&, juce::Component*) override;
    void timerCallback() override;
    void refreshStops();
    std::vector<juce::Component::SafePointer<juce::Button>> buttons;
    int cols = 1;
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
    // Keyboard: Left/Right bend while held (the ribbon springs back to centre on release), Home,
    // End, Space or Return centre it. The ribbon takes focus with Tab, not from a click.
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusGained(juce::Component::FocusChangeType) override { repaint(); }
    void focusLost(juce::Component::FocusChangeType) override { releaseKeyboardBend(); repaint(); }
    void paintOverChildren(juce::Graphics&) override;

    std::function<void(float)> onValueChange;

private:
    void setFromX(float x);
    void releaseKeyboardBend();
    bool keyboardBend = false;
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
