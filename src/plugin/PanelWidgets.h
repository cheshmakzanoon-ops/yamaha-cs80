#pragma once

// Panel keys and the base class of the panel sections (channel rows, centre row, deck).

#include "ArtCommon.h"
#include "PanelLayout.h"
#include "PluginProcessor.h"
#include "UiComponents.h"

#include <functional>
#include <memory>
#include <vector>

namespace cs80::panel
{
namespace art = cs80::art;
namespace ui = cs80::ui;
using cs80::art::displayFont;
using cs80::art::spacedFont;
using cs80::art::textWidth;
using cs80::art::FloorScope;

// Toolbar and plate keys. A TextButton keeps the label, title, focus and
// accessibility semantics; only the drawing is replaced.
class KeyButton final : public ui::ContextTextButton
{
public:
    // key: toolbar key; previous/next: transport arrows; alert: red lens-cap pushbutton; plate: dark plate key;
    // accent: gold primary key (SAVE); quiet: low-key but enabled-looking key (INIT); satin: the software
    // keycap family with a lamp (memory lid, voice settings); selector: a gold-lit choice among several.
    enum class Style { key, previous, next, alert, plate, accent, quiet, satin, selector };
    enum class Glyph { none, gear };
    explicit KeyButton(const juce::String& text, Style s = Style::key) : ui::ContextTextButton(text), style(s) {}
    void setGlyph(Glyph value) { glyph = value; repaint(); }
    // An accent key is bright only while it has something to do (SAVE: the program has unsaved edits).
    void setEmphasis(bool value) { if (value != emphasis) { emphasis = value; repaint(); } }
    // Armed: the key has been pressed once and the next press will discard unsaved edits. It prints
    // DISCARD? (or ? on an arrow) in a warning tint until it is pressed again or the time runs out.
    void setArmed(bool value) { if (value != armed) { armed = value; repaint(); } }
    bool isArmed() const noexcept { return armed; }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        if (style == Style::satin) { paintSatin(g, hovered, down); return; }
        if (style == Style::alert) { paintLensCap(g, hovered, down); return; }
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        const float rad = juce::jmin(4.0f, r.getHeight() * 0.2f);
        const bool on = getToggleState();
        const bool accent = ! armed && (style == Style::selector && on);
        if (accent)
            ui::drawAccentPlate(g, getLocalBounds().toFloat(), down, hovered);
        else
        {
            juce::Colour top, bottom;
            switch (style)
            {
                case Style::plate:    top = juce::Colour(0xff3d3a35); bottom = juce::Colour(0xff1b1917); break;
                case Style::quiet:    top = juce::Colour(0xff2a2825); bottom = juce::Colour(0xff1b1a17); break;
                case Style::accent:   top = emphasis ? juce::Colour(0xff7b6330) : juce::Colour(0xff58544b);       // SAVE: a normal key with a gold rim,
                                      bottom = emphasis ? juce::Colour(0xff3d2e12) : juce::Colour(0xff2b2925); break;   // warmed only while there is something to save
                case Style::selector: top = juce::Colour(0xff4a4740); bottom = juce::Colour(0xff26241f); break;
                default:              top = juce::Colour(0xff625d54); bottom = juce::Colour(0xff2c2a26); break;
            }
            if (on) { top = top.interpolatedWith(juce::Colour(0xffa9772b), 0.46f); bottom = bottom.interpolatedWith(juce::Colour(0xff5a3a10), 0.5f); }
            if (armed) { top = juce::Colour(0xffb0452f); bottom = juce::Colour(0xff5e1e12); }
            if (hovered) { top = top.brighter(0.14f); bottom = bottom.brighter(0.10f); }
            if (down) { std::swap(top, bottom); r = r.translated(0.0f, 1.0f); }
            else ui::softShadow(g, r, rad, 0.0f, 1.8f, 2.4f, 0.7f);
            g.setGradientFill(art::vgrad(top, bottom, r));
            g.fillRoundedRectangle(r, rad);
            g.setColour(art::white(down ? 0.05f : 0.22f));
            g.drawLine(r.getX() + rad, r.getY() + 1.0f, r.getRight() - rad, r.getY() + 1.0f, 1.0f);
            g.setColour(art::black(0.45f));
            g.drawLine(r.getX() + rad, r.getBottom() - 1.0f, r.getRight() - rad, r.getBottom() - 1.0f, 1.0f);
            g.setColour(on ? juce::Colour(0xffe6b04a).withAlpha(0.9f) : art::black(0.85f));
            g.drawRoundedRectangle(r, rad, on ? 1.2f : 1.0f);
            if (style == Style::accent && ! armed)
            {
                g.setColour(juce::Colour(0xffd6bb68).withAlpha(emphasis ? 0.95f : 0.62f));
                g.drawRoundedRectangle(r.reduced(0.5f), rad, 1.2f);
            }
        }

        auto ink = armed ? juce::Colour(0xffffe6d6)
                 : accent ? juce::Colour(0xff2b1c06)
                 : style == Style::accent ? (emphasis ? juce::Colour(0xfff6dc92) : juce::Colour(0xffe2cf8e))
                 : style == Style::plate ? juce::Colour(0xffd9c88e)
                 : style == Style::quiet ? juce::Colour(0xffd8cfb2)
                 : style == Style::selector ? juce::Colour(0xffe6dcc0)
                 : on ? juce::Colour(0xffffe3a0) : juce::Colour(0xffeee6cc);
        const juce::String shown = armed ? juce::String("DISCARD?") : getButtonText();
        if ((style == Style::previous || style == Style::next) && armed)
        {
            auto font = displayFont(juce::jlimit(11.0f, 20.0f, r.getHeight() * 0.62f), juce::Font::bold);
            g.setFont(font);
            g.setColour(art::black(0.6f));
            g.drawText("?", r.translated(0.0f, 1.0f), juce::Justification::centred, false);
            g.setColour(ink);
            g.drawText("?", r, juce::Justification::centred, false);
        }
        else if (style == Style::previous || style == Style::next)
        {
            const float s = r.getHeight() * 0.20f;
            const auto c = r.getCentre().translated(style == Style::previous ? -s * 0.2f : s * 0.2f, down ? 0.0f : -0.3f);
            juce::Path tri;
            const float dir = style == Style::previous ? -1.0f : 1.0f;
            tri.addTriangle(c.x + dir * s, c.y, c.x - dir * s * 0.8f, c.y - s * 1.05f, c.x - dir * s * 0.8f, c.y + s * 1.05f);
            g.setColour(art::black(0.6f));
            g.fillPath(tri, juce::AffineTransform::translation(0.0f, 1.0f));
            g.setColour(ink);
            g.fillPath(tri);
        }
        else
        {
            const float size = juce::jlimit(art::kLegendFloorPx, 15.0f, r.getHeight() * (style == Style::plate ? 0.40f : style == Style::quiet ? 0.44f : 0.37f));
            auto font = displayFont(size, juce::Font::bold);
            font.setExtraKerningFactor(0.09f);
            auto area = r.reduced(4.0f, 1.0f).translated(0.0f, down ? 0.0f : -0.5f);
            if (glyph == Glyph::gear)
            {
                const float tw = textWidth(font, shown);
                const float cx = area.getCentreX() - tw * 0.5f - 3.0f;
                art::gearGlyph(g, { cx, area.getCentreY() + 0.3f }, 6.0f, ink);
                area.removeFromLeft(11.0f);
            }
            // Never squeeze the legend below 85 percent: shrink the type toward its floor first.
            font = ui::fitFontToWidth(font, shown, area.getWidth(), 0.85f, art::kLegendFloorPx);
            g.setFont(font);
            g.setColour(accent ? art::white(0.45f) : art::black(0.6f));
            g.drawFittedText(shown, area.translated(0.0f, 1.0f).toNearestInt(), juce::Justification::centred, 1, 0.85f);
            g.setColour(ink);
            g.drawFittedText(shown, area.toNearestInt(), juce::Justification::centred, 1, 0.85f);
        }
        if (on && style == Style::key)
        {
            g.setColour(juce::Colour(0xffffc65a));
            g.fillRoundedRectangle(r.getCentreX() - r.getWidth() * 0.22f, r.getBottom() - 4.0f, r.getWidth() * 0.44f, 1.8f, 0.9f);
        }
        paintFocus(g, rad + 1.0f);
    }

private:
    void paintFocus(juce::Graphics& g, float radius)
    {
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), radius, 1.5f);
        }
    }

    // Red translucent lens in a machined bezel, like an indicator cap: PANIC.
    void paintLensCap(juce::Graphics& g, bool hovered, bool down)
    {
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        const float rad = juce::jmin(7.0f, r.getHeight() * 0.3f);
        ui::softShadow(g, r, rad, 0.0f, down ? 1.0f : 2.6f, down ? 1.8f : 3.4f, 0.8f);
        // machined bezel ring
        g.setGradientFill(art::vgrad(juce::Colour(0xff6c6860), juce::Colour(0xff25231f), r));
        g.fillRoundedRectangle(r, rad);
        g.setColour(art::black(0.85f));
        g.drawRoundedRectangle(r, rad, 1.0f);
        auto lens = r.reduced(3.0f);
        if (down) lens = lens.translated(0.0f, 0.8f);
        // Muted, matte red (saturation about 0.55): the key is a deliberate act, not the loudest thing on the panel.
        const auto top = juce::Colour(hovered ? 0xffc2685a : 0xffb05a4e), bottom = juce::Colour(hovered ? 0xff82392f : 0xff74342b);
        g.setGradientFill(art::vgrad(down ? bottom : top, down ? top.darker(0.3f) : bottom, lens));
        g.fillRoundedRectangle(lens, rad - 2.0f);
        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path clip;
            clip.addRoundedRectangle(lens, rad - 2.0f);
            g.reduceClipRegion(clip);
            // moulded facets: a lit upper-left ramp, shaded lower-right, and a gloss band
            g.setGradientFill(juce::ColourGradient(art::white(0.10f), lens.getX(), lens.getY(),
                                                   juce::Colours::transparentWhite, lens.getCentreX(), lens.getCentreY(), false));
            g.fillRect(lens);
            g.setGradientFill(art::vgrad(art::white(down ? 0.03f : 0.09f), juce::Colours::transparentWhite, lens.withHeight(lens.getHeight() * 0.48f)));
            g.fillRect(lens.withHeight(lens.getHeight() * 0.48f));
            g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, lens.getCentreX(), lens.getCentreY(),
                                                   art::black(0.40f), lens.getRight(), lens.getBottom(), false));
            g.fillRect(lens);
        }
        g.setColour(art::black(0.55f));
        g.drawRoundedRectangle(lens, rad - 2.0f, 1.0f);
        auto font = displayFont(juce::jlimit(art::kLegendFloorPx, 15.0f, lens.getHeight() * 0.42f), juce::Font::bold);
        font.setExtraKerningFactor(0.10f);
        font = ui::fitFontToWidth(font, getButtonText(), lens.getWidth() - 8.0f, 0.85f, art::kLegendFloorPx);
        g.setFont(font);
        g.setColour(art::black(0.6f));
        g.drawText(getButtonText(), lens.translated(0.0f, 1.0f), juce::Justification::centred, false);
        g.setColour(juce::Colour(0xfff6e4dc));
        g.drawText(getButtonText(), lens, juce::Justification::centred, false);
        paintFocus(g, rad + 1.0f);
    }

    // The software keycap family the controls use (satin plate with a jewel lamp).
    void paintSatin(juce::Graphics& g, bool hovered, bool down)
    {
        const bool on = getToggleState();
        const auto bounds = getLocalBounds().toFloat();
        ui::drawSatinPlate(g, bounds, on, down, hovered, true);
        const auto face = bounds.reduced(1.8f, 1.2f).withTrimmedBottom(1.8f);
        const float lr = juce::jlimit(1.8f, 3.4f, face.getHeight() * 0.15f);
        auto textArea = face.withTrimmedLeft(7.0f + lr * 2.6f + 4.0f).withTrimmedRight(3.0f).translated(0.0f, down ? 1.0f : 0.0f);
        auto font = displayFont(juce::jlimit(art::kLegendFloorPx, 12.0f, face.getHeight() * 0.46f), juce::Font::bold);
        font.setExtraKerningFactor(0.03f);
        font = ui::fitFontToWidth(font, getButtonText(), textArea.getWidth(), 0.85f, art::kLegendFloorPx);
        ui::drawEngravedText(g, getButtonText(), textArea, font, juce::Justification::centredLeft,
                             on ? ui::amberBright : ui::ivory.withAlpha(0.92f));
        paintFocus(g, 4.0f);
    }

    Style style;
    Glyph glyph = Glyph::none;
    bool emphasis = false;
    bool armed = false;
};

// Engraved channel tag: a small brushed plate with the numeral cut into it. The tag of the channel the
// voice plate is showing carries a lit pilot.
class ChannelTag final : public juce::Button
{
public:
    explicit ChannelTag(bool second) : juce::Button(second ? "II" : "I"), isSecond(second)
    {
        setOpaque(false);
        setTitle(second ? "Channel II plate" : "Channel I plate");
        setTooltip(second ? "Show channel II in the signal-flow window" : "Show channel I in the signal-flow window");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
    }
    void setViewed(bool value) { if (value != viewed) { viewed = value; repaint(); } }
    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        const float sx = (float) getWidth() / 24.0f, sy = (float) getHeight() / 40.0f;
        FloorScope floor(sx);
        g.addTransform(juce::AffineTransform::scale(sx, sy));
        const juce::Rectangle<float> r(0.0f, 0.0f, 24.0f, 40.0f);
        ui::softShadow(g, r.reduced(1.0f), 2.5f, 0.0f, 1.6f, 3.0f, 0.7f);
        art::brushedMetal(g, r.reduced(1.0f), 2.5f, viewed ? 0.5f : -0.2f, isSecond ? 71 : 70);
        if (! viewed)
        {
            g.setColour(art::black(0.34f));
            g.fillRoundedRectangle(r.reduced(1.0f), 2.5f);
        }
        g.setColour(art::white(0.55f));
        g.drawRoundedRectangle(r.reduced(1.4f), 2.2f, 0.7f);
        g.setColour(art::black(0.85f));
        g.drawRoundedRectangle(r.reduced(0.5f), 2.8f, 1.0f);
        // The numeral is cut as a Roman capital with serifs, so a lone I does not read as a slot.
        const int count = isSecond ? 2 : 1;
        const float pitch = 9.0f, stem = 3.4f, top = 7.0f, bottom = 26.0f;
        const float startX = r.getCentreX() - (float) (count - 1) * pitch * 0.5f;
        for (int i = 0; i < count; ++i)
        {
            const float cx = startX + (float) i * pitch;
            juce::Path numeral;
            numeral.addRectangle(cx - stem * 0.5f, top, stem, bottom - top);
            numeral.addRectangle(cx - 4.2f, top, 8.4f, 2.4f);
            numeral.addRectangle(cx - 4.2f, bottom - 2.4f, 8.4f, 2.4f);
            g.setColour(art::white(0.65f));
            g.fillPath(numeral, juce::AffineTransform::translation(0.0f, 0.9f));
            g.setColour(juce::Colour(0xff1d1b17));
            g.fillPath(numeral);
        }
        // pilot lamp
        const juce::Point<float> c(r.getCentreX(), r.getBottom() - 5.0f);
        g.setColour(art::black(0.8f));
        g.fillEllipse(c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);
        if (viewed)
        {
            g.setColour(juce::Colour(0xffffa530).withAlpha(0.35f));
            g.fillEllipse(c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
        }
        g.setColour(viewed ? juce::Colour(0xffffcf70) : juce::Colour(0xff3a2412));
        g.fillEllipse(c.x - 2.1f, c.y - 2.1f, 4.2f, 4.2f);
        if (hovered || down)
        {
            g.setColour(art::white(down ? 0.05f : 0.14f));
            g.fillRoundedRectangle(r.reduced(1.0f), 2.5f);
        }
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawRoundedRectangle(r.reduced(0.7f), 2.8f, 1.4f);
        }
    }
private:
    bool isSecond, viewed = false;
};

// A component placed in absolute design coordinates (1600 x 1000 space).
struct Placement
{
    juce::Component* component = nullptr;
    juce::Rectangle<float> bounds;
};

// One block of the instrument panel. It owns its controls (and their APVTS attachments), keeps their design
// rectangles, and maps them to pixels with the editor's scale. Controls are placed in absolute design
// coordinates and rounded exactly as if they were children of the editor, so moving a control into a
// section never moves it by a pixel. A section paints nothing and passes clicks through to its controls;
// its static art (paintArt) is drawn by the editor's buffered chassis in design space. Group brackets are
// recorded while the controls are placed, so a title always spans exactly the columns it names.
class PanelSection : public juce::Component
{
public:
    explicit PanelSection(Cs80AudioProcessor& p);
    ~PanelSection() override;

    // Sets this section's bounds (its design area, mapped with sx/sy) and lays out every placed control.
    void layout(juce::Rectangle<float> designArea, float sx, float sy);
    const std::vector<Placement>& getPlacements() const noexcept { return placements; }
    // Every control bound to an engine parameter, with that parameter (for channel touch tracking).
    const std::vector<std::pair<juce::Component*, int>>& getBoundControls() const noexcept { return boundControls; }
    // Painted captions with a short form for small editors (titles and tooltips are untouched).
    virtual void applyCaptions(bool small);
    // Static art in design space (the chassis calls it): the recorded group brackets, plus any section art.
    virtual void paintArt(juce::Graphics& g) const;

protected:
    void place(juce::Component& component, float x, float y, float w, float h);
    ui::ParamKnob& addKnob(int parameter, float x, float y, float w, float h, const juce::String& name = {});
    ui::ParamKnob& addFader(int parameter, float x, float y, float w, float h,
                            const juce::String& label, juce::Colour colour);
    ui::LedButton& addLed(int parameter, const juce::String& label, float x, float y, float w, float h);
    void shortCaption(ui::ParamKnob& knob, const juce::String& full, const juce::String& shortForm);
    // Called at the end of layout(), when every control has its final pixel bounds.
    virtual void afterLayout() {}
    // An etched group bracket over [x, x + w] with its rule at ruleY (see art::bracket).
    void addBracket(const juce::String& title, float x, float ruleY, float w, float tick = 4.0f,
                    const juce::String& shortTitle = {});

    Cs80AudioProcessor& processor;

private:
    struct CaptionPair { ui::ParamKnob* knob; juce::String full, shortForm; };
    struct Bracket { juce::String title, shortTitle; float x, ruleY, w, tick; };
    std::vector<Bracket> brackets;
    std::vector<Placement> placements;
    std::vector<std::pair<juce::Component*, int>> boundControls;
    std::vector<CaptionPair> captionPairs;
    std::vector<std::unique_ptr<ui::ParamKnob>> knobs;
    std::vector<std::unique_ptr<ui::LedButton>> leds;
    // Declared after the buttons so the attachments are destroyed first.
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanelSection)
};

// The original slider-cap colours.
inline juce::Colour whiteCap() { return ui::capCream; }
inline juce::Colour greenCap() { return ui::capGreen; }
inline juce::Colour redCap() { return ui::capPink; }
inline juce::Colour yellowCap() { return ui::capYellow; }
inline juce::Colour blackCap() { return ui::capBlack; }
inline juce::Colour greyCap() { return ui::capGrey; }
} // namespace cs80::panel
