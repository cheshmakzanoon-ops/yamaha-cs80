#include "PluginEditor.h"

#include "AdvancedPanel.h"
#include "PresetBrowser.h"
#include "OutputMeterDisplay.h"
#include "UiComponents.h"
#include "HardwareKeyboard.h"
#include "core/FactoryPresets.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <functional>
#include <string_view>
#include <vector>

namespace ui = cs80::ui;

namespace
{
struct Placement
{
    juce::Component* component = nullptr;
    juce::Rectangle<float> bounds;
};

constexpr float kDesignWidth = 1600.0f;
constexpr float kDesignHeight = 1000.0f;

struct InitialEditorSize
{
    int width;
    int height;
};

constexpr InitialEditorSize initialEditorSizeForArea(int usableWidth, int usableHeight)
{
    // No display (e.g. headless rendering): preserve the established default.
    if (usableWidth <= 0 || usableHeight <= 0)
        return { 1600, 1000 };

    // Leave modest room for the host frame/title/toolbar where space permits.
    // Work in exact 8:5 steps, using JUCE's already-logical userArea pixels;
    // applying the monitor DPI scale again would shrink high-DPI windows twice.
    const int units = std::clamp(std::min((usableWidth - 32) / 8,
                                          (usableHeight - 64) / 5), 140, 200);
    // The current layout cannot go below 1120x700. Small displays may not fit
    // that content plus host chrome; do not silently scale below this minimum.
    return { units * 8, units * 5 };
}

static_assert(initialEditorSizeForArea(1920, 1200).width == 1600);
static_assert(initialEditorSizeForArea(1920, 1040).height == 975);
static_assert(initialEditorSizeForArea(1440, 860).width == 1272);
static_assert(initialEditorSizeForArea(1366, 728).height == 700);
static_assert(initialEditorSizeForArea(1024, 600).width == 1120);
static_assert(initialEditorSizeForArea(0, 0).height == 1000);

juce::Font displayFont(float height, int style = juce::Font::plain)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(),
                                        height, style));
}

juce::Font spacedFont(float height, float kerning, float hscale = 1.0f, int style = juce::Font::bold)
{
    auto f = displayFont(height, style);
    f.setExtraKerningFactor(kerning);
    if (hscale != 1.0f) f.setHorizontalScale(hscale);
    return f;
}

float textWidth(const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, text, 0.0f, 0.0f);
    return glyphs.getBoundingBox(0, -1, true).getWidth();
}

// UI-only classification of the requested APVTS selection. This immutable
// lookup must not read Engine diagnostics, which belong to the render thread.
constexpr std::string_view requestedSourceRouteTag(int rank, int source, cs80::RenderPolicy policy)
{
    switch (cs80::source_factory::route(rank, source, cs80::usesSourceModels(policy)))
    {
        case cs80::source_factory::Route::source_complete: return "SOURCE-COMPLETE";
        case cs80::source_factory::Route::source_inferred: return "SOURCE-INFERRED";
        case cs80::source_factory::Route::legacy_bridge: return "LEGACY STUDY";
        case cs80::source_factory::Route::panel_or_memory: return source == 13 ? "PANEL" : "MEMORY";
        case cs80::source_factory::Route::invalid: return "INVALID";
    }
    return "INVALID";
}
static_assert(requestedSourceRouteTag(0, 1, cs80::renderPolicy(1.0f, 0.0f)) == "SOURCE-COMPLETE");
static_assert(requestedSourceRouteTag(1, 0, cs80::renderPolicy(1.0f, 0.0f)) == "SOURCE-INFERRED");
static_assert(requestedSourceRouteTag(0, 1, cs80::renderPolicy(0.0f, 1.0f)) == "SOURCE-COMPLETE");
static_assert(requestedSourceRouteTag(1, 0, cs80::renderPolicy(0.0f, 1.0f)) == "SOURCE-INFERRED");
static_assert(cs80::renderPolicy(1.0f, 1.0f) == cs80::RenderPolicy::isolation);
static_assert(requestedSourceRouteTag(0, 1, cs80::renderPolicy(1.0f, 1.0f)) == "SOURCE-COMPLETE");
static_assert(requestedSourceRouteTag(0, 1, cs80::renderPolicy(0.0f, 0.0f)) == "LEGACY STUDY");
static_assert(requestedSourceRouteTag(1, 12, cs80::RenderPolicy::sourcePerformance) == "MEMORY");
static_assert(requestedSourceRouteTag(0, 13, cs80::RenderPolicy::legacy) == "PANEL");
static_assert(requestedSourceRouteTag(-1, 0, cs80::RenderPolicy::isolation) == "INVALID");

// Custom wheels are plain Components rather than TooltipClients. Reuse their
// accessibility description for hover help without intercepting pedal gestures.
class PanelTooltipWindow final : public juce::TooltipWindow
{
public:
    using juce::TooltipWindow::TooltipWindow;
    juce::String getTipFor(juce::Component& component) override
    {
        if (component.getComponentID() == "expression_pedal")
            return component.getDescription();
        return juce::TooltipWindow::getTipFor(component);
    }
};

// ---------------------------------------------------------------------------
// Chassis geometry in the 1600 x 1000 design space.
// ---------------------------------------------------------------------------
namespace geo
{
using Rf = juce::Rectangle<float>;

// Toolbar y 0-64
const Rf brandBadge   { 24, 8, 34, 34 };
const Rf prevBtn      { 252, 9, 34, 34 };
const Rf presetWell   { 292, 6, 414, 40 };
const Rf presetTag    { 306, 10, 280, 11 };
const Rf presetIndex  { 590, 10, 104, 11 };
const Rf presetName   { 304, 21, 394, 24 };
const Rf nextBtn      { 712, 9, 34, 34 };
const Rf cluster      { 762, 6, 350, 40 };
const Rf libraryBtn   { 768, 10, 90, 32 };
const Rf saveBtn      { 862, 10, 66, 32 };
const Rf initBtn      { 932, 10, 60, 32 };
const Rf advancedBtn  { 996, 10, 110, 32 };
const Rf meterWell    { 1130, 6, 352, 40 };
const Rf meterText    { 1138, 8, 154, 36 };
const Rf meterBar     { 1296, 13, 176, 14 };
const Rf panicBtn     { 1498, 8, 78, 36 };
const Rf ticker       { 22, 48, 1556, 14 };
const Rf rankI        { 36, 48, 228, 14 };
const Rf rankII       { 282, 48, 228, 14 };
const Rf subStatus    { 528, 48, 210, 14 };
const Rf modeStatus   { 752, 48, 820, 14 };

// Control surface y 64-660
const float faceTop = 64.0f, faceBottom = 660.0f, deckBottom = 772.0f;
const Rf plate        { 54, 98, 360, 378 };
const std::array<float, 2> rowY0 { 104.0f, 296.0f };
const Rf lowerPlate   { 48, 484, 1504, 166 };
const Rf toneBezel    { 669, 520, 528, 102 };

// Performance deck y 660-772
const Rf deckLeft     { 52, 662, 424, 108 };
const Rf deckRight    { 1124, 662, 424, 108 };
const Rf nameplate    { 640, 668, 320, 40 };
const Rf ribbonRect   { 504, 732, 592, 28 };
const Rf keybed       { 44, 772, 1512, 228 };
const Rf overlay      { 0, 64, 1600, 936 };
const Rf memoryLid    { 48, 100, 1504, 552 };
} // namespace geo

// ---------------------------------------------------------------------------
// Chassis drawing helpers (local; shared helpers live in UiComponents.h)
// ---------------------------------------------------------------------------
namespace art
{
using Rf = juce::Rectangle<float>;
using juce::Colour;

inline Colour black(float a) { return juce::Colours::black.withAlpha(a); }
inline Colour white(float a) { return juce::Colours::white.withAlpha(a); }
inline juce::ColourGradient vgrad(Colour top, Colour bottom, Rf r)
{
    return juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
}
inline juce::ColourGradient hgrad(Colour left, Colour right, Rf r)
{
    return juce::ColourGradient(left, r.getX(), r.getY(), right, r.getRight(), r.getY(), false);
}

// A seamless embossed micro-relief used for the crinkle/leatherette finish.
// Pixels hold white (raised facets) or black (shaded facets) with an alpha
// that encodes relief strength; callers modulate it with the fill opacity.
juce::Image makeRelief(int n, int seed, float gain)
{
    juce::Random rng(seed);
    auto layer = [&](int cell)
    {
        const int cells = n / cell;
        std::vector<float> lattice((size_t) (cells * cells));
        for (auto& v : lattice) v = rng.nextFloat();
        std::vector<float> out((size_t) (n * n));
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
            {
                const float fx = (float) x / (float) cell, fy = (float) y / (float) cell;
                const int x0 = (int) fx, y0 = (int) fy;
                float tx = fx - (float) x0, ty = fy - (float) y0;
                tx = tx * tx * (3.0f - 2.0f * tx);
                ty = ty * ty * (3.0f - 2.0f * ty);
                auto at = [&](int xx, int yy) { return lattice[(size_t) ((yy % cells) * cells + (xx % cells))]; };
                const float top = at(x0, y0) + (at(x0 + 1, y0) - at(x0, y0)) * tx;
                const float bot = at(x0, y0 + 1) + (at(x0 + 1, y0 + 1) - at(x0, y0 + 1)) * tx;
                out[(size_t) (y * n + x)] = top + (bot - top) * ty;
            }
        return out;
    };
    const auto coarse = layer(8), mid = layer(4), fine = layer(2);
    std::vector<float> height((size_t) (n * n));
    for (size_t i = 0; i < height.size(); ++i)
        height[i] = 0.40f * coarse[i] + 0.38f * mid[i] + 0.34f * fine[i] + 0.28f * rng.nextFloat();
    juce::Image image(juce::Image::ARGB, n, n, true);
    juce::Image::BitmapData data(image, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            const float here = height[(size_t) (y * n + x)];
            const float diag = height[(size_t) (((y + 1) % n) * n + ((x + 1) % n))];
            const float e = juce::jlimit(-1.0f, 1.0f, (here - diag) * gain);
            data.setPixelColour(x, y, e > 0.0f ? juce::Colours::white.withAlpha(e)
                                               : juce::Colours::black.withAlpha(-e));
        }
    return image;
}

// Tile a relief image at one texel per device pixel inside a design-space context.
void fillRelief(juce::Graphics& g, const juce::Image& tile, Rf area, float deviceScale,
                float opacity, float shiftX = 0.0f, float shiftY = 0.0f)
{
    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(area.toNearestIntEdges());
    juce::FillType fill(tile, juce::AffineTransform::scale(1.0f / deviceScale).translated(shiftX, shiftY));
    fill.setOpacity(opacity);
    g.setFillType(fill);
    g.fillRect(area);
}

void insetShadow(juce::Graphics& g, Rf r, float radius, float depth, float alpha)
{
    juce::Graphics::ScopedSaveState state(g);
    juce::Path clip;
    clip.addRoundedRectangle(r, radius);
    g.reduceClipRegion(clip);
    g.setGradientFill(juce::ColourGradient(black(alpha), r.getX(), r.getY(),
                                           juce::Colours::transparentBlack, r.getX(), r.getY() + depth, false));
    g.fillRect(r);
    g.setGradientFill(juce::ColourGradient(black(alpha * 0.85f), r.getX(), r.getY(),
                                           juce::Colours::transparentBlack, r.getX() + depth, r.getY(), false));
    g.fillRect(r);
    g.setGradientFill(juce::ColourGradient(black(alpha * 0.35f), r.getRight(), r.getBottom(),
                                           juce::Colours::transparentBlack, r.getRight() - depth * 0.6f, r.getBottom(), false));
    g.fillRect(r);
    g.setGradientFill(juce::ColourGradient(black(alpha * 0.30f), r.getRight(), r.getBottom(),
                                           juce::Colours::transparentBlack, r.getRight(), r.getBottom() - depth * 0.6f, false));
    g.fillRect(r);
}

// The edge of a recess: a dark line outside the top/left, a lit lip outside the bottom/right.
void recessEdge(juce::Graphics& g, Rf r, float radius, float strength = 1.0f)
{
    g.setColour(white(0.13f * strength));
    g.drawLine(r.getX() + radius, r.getBottom() + 0.7f, r.getRight() - radius, r.getBottom() + 0.7f, 1.0f);
    g.drawLine(r.getRight() + 0.7f, r.getY() + radius, r.getRight() + 0.7f, r.getBottom() - radius, 1.0f);
    g.setColour(black(0.55f * strength));
    g.drawLine(r.getX() + radius, r.getY() - 0.6f, r.getRight() - radius, r.getY() - 0.6f, 1.0f);
    g.drawLine(r.getX() - 0.6f, r.getY() + radius, r.getX() - 0.6f, r.getBottom() - radius, 1.0f);
    g.setColour(black(0.75f * strength));
    g.drawRoundedRectangle(r, radius, 0.9f);
}

void recessTray(juce::Graphics& g, Rf r, float radius, Colour top, Colour bottom, float depth = 10.0f, float shade = 0.65f)
{
    g.setGradientFill(vgrad(top, bottom, r));
    g.fillRoundedRectangle(r, radius);
    insetShadow(g, r, radius, depth, shade);
    recessEdge(g, r, radius);
}

void raisedPlate(juce::Graphics& g, Rf r, float radius, Colour top, Colour bottom, float lift = 1.0f)
{
    ui::softShadow(g, r, radius, 0.0f, 3.0f * lift, 5.0f * lift, 0.75f);
    g.setGradientFill(vgrad(top, bottom, r));
    g.fillRoundedRectangle(r, radius);
    g.setColour(white(0.16f));
    g.drawLine(r.getX() + radius, r.getY() + 1.0f, r.getRight() - radius, r.getY() + 1.0f, 1.0f);
    g.setColour(white(0.07f));
    g.drawLine(r.getX() + 1.0f, r.getY() + radius, r.getX() + 1.0f, r.getBottom() - radius, 1.0f);
    g.setColour(black(0.5f));
    g.drawLine(r.getX() + radius, r.getBottom() - 1.0f, r.getRight() - radius, r.getBottom() - 1.0f, 1.0f);
    g.setColour(black(0.7f));
    g.drawRoundedRectangle(r, radius, 1.0f);
}

void brushedMetal(juce::Graphics& g, Rf r, float radius, float brightness, int seed)
{
    juce::Graphics::ScopedSaveState state(g);
    juce::Path clip;
    clip.addRoundedRectangle(r, radius);
    g.reduceClipRegion(clip);
    juce::ColourGradient base(Colour(0xff77746d).interpolatedWith(juce::Colours::white, brightness * 0.1f),
                              r.getX(), r.getY(), Colour(0xff6c6962), r.getRight(), r.getY(), false);
    base.addColour(0.12, Colour(0xffcac7bf));
    base.addColour(0.38, Colour(0xff9c9992));
    base.addColour(0.62, Colour(0xffd6d3cb));
    base.addColour(0.88, Colour(0xffa4a199));
    g.setGradientFill(base);
    g.fillRect(r);
    juce::Random rng(seed);
    for (float y = r.getY(); y < r.getBottom(); y += 0.8f)
    {
        g.setColour((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha(0.015f + rng.nextFloat() * 0.07f));
        g.drawHorizontalLine((int) y, r.getX(), r.getRight());
    }
    g.setGradientFill(juce::ColourGradient(white(0.32f), r.getX(), r.getY(), black(0.38f), r.getX(), r.getBottom(), false));
    g.fillRect(r);
}

void slotScrew(juce::Graphics& g, juce::Point<float> c, float r, float turn)
{
    g.setColour(black(0.75f));
    g.fillEllipse(c.x - r - 0.9f, c.y - r - 0.7f, 2.0f * r + 1.8f, 2.0f * r + 1.8f);
    g.setColour(white(0.10f));
    g.drawEllipse(c.x - r - 0.9f, c.y - r - 0.7f, 2.0f * r + 1.8f, 2.0f * r + 1.8f, 0.7f);
    g.setGradientFill(juce::ColourGradient(Colour(0xfff0ece0), c.x - r * 0.6f, c.y - r * 0.8f,
                                           Colour(0xff4a463d), c.x + r * 0.7f, c.y + r * 0.9f, false));
    g.fillEllipse(c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    g.setColour(black(0.5f));
    g.drawEllipse(c.x - r, c.y - r, 2.0f * r, 2.0f * r, 0.6f);
    const float dx = std::cos(turn), dy = std::sin(turn);
    const float w = juce::jmax(1.0f, r * 0.34f);
    g.setColour(white(0.55f));
    g.drawLine(c.x - dx * r * 0.8f + 0.5f, c.y - dy * r * 0.8f + 0.8f, c.x + dx * r * 0.8f + 0.5f, c.y + dy * r * 0.8f + 0.8f, w * 0.6f);
    g.setColour(Colour(0xff15130f));
    g.drawLine(c.x - dx * r * 0.82f, c.y - dy * r * 0.82f, c.x + dx * r * 0.82f, c.y + dy * r * 0.82f, w);
}

void louvre(juce::Graphics& g, Rf r)
{
    recessTray(g, r, 2.5f, Colour(0xff0b0a09), Colour(0xff050505), 6.0f, 0.8f);
    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(r.reduced(1.5f).toNearestIntEdges());
    for (float y = r.getY() + 2.2f; y < r.getBottom() - 1.0f; y += 3.1f)
    {
        g.setColour(Colour(0xff000000));
        g.fillRect(r.getX() + 2.0f, y, r.getWidth() - 4.0f, 1.5f);
        g.setColour(Colour(0xff474239).withAlpha(0.9f));
        g.fillRect(r.getX() + 2.0f, y + 1.5f, r.getWidth() - 4.0f, 0.8f);
        g.setColour(white(0.05f));
        g.fillRect(r.getX() + 2.0f, y - 0.8f, r.getWidth() - 4.0f, 0.6f);
    }
}

// Printed legend with a drop shadow, in the cream silkscreen colour.
void legend(juce::Graphics& g, const juce::String& text, Rf area, const juce::Font& font,
            juce::Justification just = juce::Justification::centred,
            Colour ink = Colour(0xffe6dcbc), float shadow = 0.6f)
{
    g.setFont(font);
    const bool multi = text.containsChar('\n');
    g.setColour(black(shadow));
    if (multi) g.drawFittedText(text, area.translated(0.0f, 1.0f).toNearestInt(), just, 2, 1.0f);
    else       g.drawText(text, area.translated(0.0f, 1.0f), just, false);
    g.setColour(ink);
    if (multi) g.drawFittedText(text, area.toNearestInt(), just, 2, 1.0f);
    else       g.drawText(text, area, just, false);
}

// Text on bare metal: dark ink with a lit lower edge.
void stamped(juce::Graphics& g, const juce::String& text, Rf area, const juce::Font& font,
             juce::Justification just = juce::Justification::centred, Colour ink = Colour(0xff1d1b17))
{
    g.setFont(font);
    g.setColour(white(0.55f));
    g.drawText(text, area.translated(0.0f, 1.0f), just, false);
    g.setColour(ink);
    g.drawText(text, area, just, false);
}

// Gold etched grouping bracket: a rule with its title sitting in a gap and short end ticks.
void bracket(juce::Graphics& g, const juce::String& title, float x, float ruleY, float w,
             float tick = 6.0f, float fontSize = 9.5f)
{
    const auto font = spacedFont(fontSize, 0.10f);
    const float gap = textWidth(font, title) * 0.5f + 8.0f;
    const float mid = x + w * 0.5f;
    ui::etchLine(g, x, ruleY, mid - gap, ruleY, 0.8f);
    ui::etchLine(g, mid + gap, ruleY, x + w, ruleY, 0.8f);
    ui::etchLine(g, x, ruleY, x, ruleY + tick, 0.8f);
    ui::etchLine(g, x + w, ruleY, x + w, ruleY + tick, 0.8f);
    legend(g, title, { x, ruleY - 8.0f, w, 16.0f }, font);
}

// Rounded walnut cheek: bullnose shading, long grain, pores, varnish and a polished bevel.
void walnut(juce::Graphics& g, Rf r, bool left)
{
    juce::Path shape;
    shape.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), 12.0f, 12.0f, true, true, false, false);
    ui::softShadow(g, r, 8.0f, left ? 6.0f : -6.0f, 3.0f, 12.0f, 0.9f);
    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(shape);

    juce::ColourGradient base(Colour(left ? 0xff2e170c : 0xff1c0e07), r.getX(), 0, Colour(left ? 0xff1a0c06 : 0xff321a0e), r.getRight(), 0, false);
    if (left)
    {
        base.addColour(0.16, Colour(0xff55291a));
        base.addColour(0.42, Colour(0xff6f391f));
        base.addColour(0.72, Colour(0xff4a2314));
    }
    else
    {
        base.addColour(0.30, Colour(0xff4a2314));
        base.addColour(0.62, Colour(0xff6c371e));
        base.addColour(0.88, Colour(0xff4c2516));
    }
    g.setGradientFill(base);
    g.fillRect(r);

    juce::Random rng(left ? 31 : 47);
    // Figure: broad growth-ring bands.
    for (int i = 0; i < 9; ++i)
    {
        const float x = r.getX() + rng.nextFloat() * r.getWidth();
        const float w = 3.0f + rng.nextFloat() * 8.0f;
        g.setColour((rng.nextBool() ? Colour(0xff160804) : Colour(0xff9a5530)).withAlpha(0.07f + rng.nextFloat() * 0.07f));
        juce::Path band;
        const float ph = rng.nextFloat() * 6.0f, amp = 1.5f + rng.nextFloat() * 4.0f;
        band.startNewSubPath(x, r.getY());
        for (float y = r.getY(); y <= r.getBottom(); y += 30.0f)
            band.lineTo(x + std::sin(y * 0.006f + ph) * amp, y);
        g.strokePath(band, juce::PathStrokeType(w, juce::PathStrokeType::curved));
    }
    // Long grain lines.
    for (int i = 0; i < 110; ++i)
    {
        const float x = r.getX() + 1.0f + rng.nextFloat() * (r.getWidth() - 2.0f);
        const float ph = rng.nextFloat() * 6.28f, ph2 = rng.nextFloat() * 6.28f;
        const float amp = 0.8f + rng.nextFloat() * 3.2f;
        juce::Path p;
        p.startNewSubPath(x, r.getY());
        for (float y = r.getY(); y <= r.getBottom() + 30.0f; y += 28.0f)
            p.lineTo(x + std::sin(y * 0.011f + ph) * amp + std::sin(y * 0.034f + ph2) * amp * 0.35f, y);
        const bool dark = rng.nextFloat() < 0.62f;
        g.setColour((dark ? Colour(0xff150703) : Colour(0xffb8693a)).withAlpha(dark ? 0.06f + rng.nextFloat() * 0.18f
                                                                                      : 0.03f + rng.nextFloat() * 0.09f));
        g.strokePath(p, juce::PathStrokeType(0.35f + rng.nextFloat() * 1.3f, juce::PathStrokeType::curved));
    }
    // Pores.
    for (int i = 0; i < 260; ++i)
    {
        const float x = r.getX() + rng.nextFloat() * r.getWidth();
        const float y = r.getY() + rng.nextFloat() * r.getHeight();
        g.setColour(black(0.10f + rng.nextFloat() * 0.18f));
        g.fillRect(x, y, 0.6f, 3.0f + rng.nextFloat() * 10.0f);
    }
    // Varnish: a long soft highlight down the rounded face, brighter at the top.
    juce::ColourGradient sheen(juce::Colours::transparentWhite, r.getX(), 0, juce::Colours::transparentWhite, r.getRight(), 0, false);
    sheen.addColour(left ? 0.22 : 0.52, juce::Colours::transparentWhite);
    sheen.addColour(left ? 0.34 : 0.64, white(0.24f));
    sheen.addColour(left ? 0.46 : 0.76, juce::Colours::transparentWhite);
    g.setGradientFill(sheen);
    g.fillRect(r);
    g.setGradientFill(juce::ColourGradient(white(0.20f), 0, r.getY(), juce::Colours::transparentWhite, 0, r.getY() + 360.0f, false));
    g.fillRect(r);
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, 0, r.getBottom() - 260.0f, black(0.45f), 0, r.getBottom(), false));
    g.fillRect(r);
    // Bevel against the panel.
    const float innerX = left ? r.getRight() - 3.0f : r.getX();
    g.setGradientFill(hgrad(white(left ? 0.0f : 0.20f), white(left ? 0.20f : 0.0f), { innerX, r.getY(), 3.0f, r.getHeight() }));
    g.fillRect(innerX, r.getY(), 3.0f, r.getHeight());
    g.setColour(black(0.8f));
    g.fillRect(left ? r.getRight() - 1.0f : r.getX(), r.getY(), 1.0f, r.getHeight());
    // Outer edge falls away.
    g.setGradientFill(hgrad(black(left ? 0.55f : 0.0f), black(left ? 0.0f : 0.55f),
                            { left ? r.getX() : r.getRight() - 6.0f, r.getY(), 6.0f, r.getHeight() }));
    g.fillRect(left ? r.getX() : r.getRight() - 6.0f, r.getY(), 6.0f, r.getHeight());
    // Polished top edge.
    g.setColour(Colour(0xffe2a672).withAlpha(0.55f));
    g.fillRect(r.getX() + 10.0f, r.getY(), r.getWidth() - 20.0f, 1.2f);
    g.setColour(white(0.12f));
    g.fillRect(r.getX() + 10.0f, r.getY() + 1.2f, r.getWidth() - 20.0f, 2.0f);
    g.restoreState();
    g.saveState();
    g.setColour(black(0.7f));
    g.strokePath(shape, juce::PathStrokeType(1.0f));
}
} // namespace art

// Static chassis art. Rendering is expensive (wood grain, relief, layered
// shadows), so it is buffered once per size and only re-rendered on demand.
class Backdrop final : public juce::Component
{
public:
    Backdrop()
    {
        setOpaque(true);
        setInterceptsMouseClicks(false, false);
        setBufferedToImage(true);
    }
    void paint(juce::Graphics& g) override { if (draw) draw(g); }
    std::function<void(juce::Graphics&)> draw;
};

// A small timer that runs only while an overlay transition is in flight.
class FrameTimer final : private juce::Timer
{
public:
    std::function<void()> onFrame;
    void start() { startTimerHz(60); }
    void stop() { stopTimer(); }
    bool running() const { return isTimerRunning(); }
    ~FrameTimer() override { stopTimer(); }
private:
    void timerCallback() override { if (onFrame) onFrame(); }
};

// Dims the instrument behind an open overlay; clicking the dimmed margin closes it.
class OverlayScrim final : public juce::Component
{
public:
    OverlayScrim() { setOpaque(false); }
    void setAmount(float a) { if (std::abs(a - amount) > 0.002f) { amount = a; repaint(); } }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black.withAlpha(0.72f * amount)); }
    void mouseDown(const juce::MouseEvent&) override { if (onClick) onClick(); }
    std::function<void()> onClick;
private:
    float amount = 0.0f;
};

// The shadow the toolbar casts onto an open overlay sheet. Mouse-transparent.
class OverlayShade final : public juce::Component
{
public:
    OverlayShade() { setOpaque(false); setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics& g) override
    {
        const float s = (float) getHeight() / geo::overlay.getHeight();
        const float depth = 30.0f * s;
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.62f), 0, 0,
                                               juce::Colours::transparentBlack, 0, depth, false));
        g.fillRect(0.0f, 0.0f, (float) getWidth(), depth);
        g.setColour(juce::Colour(0xffcdb878).withAlpha(0.35f));
        g.fillRect(0.0f, 0.0f, (float) getWidth(), 1.0f);
    }
};

// Toolbar and plate keys. A TextButton keeps the label, title, focus and
// accessibility semantics; only the drawing is replaced.
class KeyButton final : public juce::TextButton
{
public:
    enum class Style { key, previous, next, alert, plate };
    explicit KeyButton(const juce::String& text, Style s = Style::key) : juce::TextButton(text), style(s) {}

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        const float rad = juce::jmin(4.0f, r.getHeight() * 0.2f);
        const bool on = getToggleState();
        juce::Colour top, bottom;
        switch (style)
        {
            case Style::alert: top = juce::Colour(0xff8a3226); bottom = juce::Colour(0xff3c110c); break;
            case Style::plate: top = juce::Colour(0xff3d3a35); bottom = juce::Colour(0xff1b1917); break;
            default:           top = juce::Colour(0xff5d5850); bottom = juce::Colour(0xff2a2825); break;
        }
        if (on) { top = top.interpolatedWith(juce::Colour(0xffa9772b), 0.42f); bottom = bottom.interpolatedWith(juce::Colour(0xff5a3a10), 0.5f); }
        if (hovered) { top = top.brighter(0.14f); bottom = bottom.brighter(0.10f); }
        if (down) { std::swap(top, bottom); r = r.translated(0.0f, 1.0f); }
        else ui::softShadow(g, r, rad, 0.0f, 1.8f, 2.4f, 0.7f);
        g.setGradientFill(art::vgrad(top, bottom, r));
        g.fillRoundedRectangle(r, rad);
        g.setColour(art::white(down ? 0.05f : 0.24f));
        g.drawLine(r.getX() + rad, r.getY() + 1.0f, r.getRight() - rad, r.getY() + 1.0f, 1.0f);
        g.setColour(art::black(0.45f));
        g.drawLine(r.getX() + rad, r.getBottom() - 1.0f, r.getRight() - rad, r.getBottom() - 1.0f, 1.0f);
        g.setColour(on ? juce::Colour(0xffe6b04a).withAlpha(0.85f) : art::black(0.85f));
        g.drawRoundedRectangle(r, rad, on ? 1.2f : 1.0f);

        const bool alert = style == Style::alert;
        const auto ink = alert ? juce::Colour(0xffffb3a0)
                               : style == Style::plate ? juce::Colour(0xffd9c88e)
                               : on ? juce::Colour(0xffffe3a0) : juce::Colour(0xffeee6cc);
        if (style == Style::previous || style == Style::next)
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
            const float size = juce::jlimit(8.0f, 15.0f, r.getHeight() * (style == Style::plate ? 0.40f : 0.37f));
            auto font = displayFont(size, juce::Font::bold);
            font.setExtraKerningFactor(0.09f);
            g.setFont(font);
            auto area = r.reduced(4.0f, 1.0f).translated(0.0f, down ? 0.0f : -0.5f);
            g.setColour(art::black(0.6f));
            g.drawFittedText(getButtonText(), area.translated(0.0f, 1.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
            g.setColour(ink);
            g.drawFittedText(getButtonText(), area.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
        if (on && (style == Style::key))
        {
            g.setColour(juce::Colour(0xffffc65a));
            g.fillRoundedRectangle(r.getCentreX() - r.getWidth() * 0.22f, r.getBottom() - 4.0f, r.getWidth() * 0.44f, 1.8f, 0.9f);
        }
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), rad + 1.0f, 1.5f);
        }
    }

private:
    Style style;
};

// A Label that honours its own font (the shared look-and-feel derives label fonts
// from the component height) and can print a soft drop shadow like silkscreen.
class PanelLabel final : public juce::Label
{
public:
    PanelLabel() { setMinimumHorizontalScale(0.78f); }
    void setMinimumPixels(float px) { minPixels = px; }
    void setLines(int n) { lines = n; }
    void setShadow(float alpha) { shadow = alpha; }
    void paint(juce::Graphics& g) override
    {
        auto f = getFont();
        if (minPixels > 0.0f && f.getHeight() < minPixels) f = f.withHeight(minPixels);
        g.setFont(f);
        const auto area = getLocalBounds();
        const auto text = getText();
        if (shadow > 0.0f)
        {
            g.setColour(juce::Colours::black.withAlpha(shadow));
            g.drawFittedText(text, area.translated(0, 1), getJustificationType(), lines, getMinimumHorizontalScale());
        }
        g.setColour(findColour(juce::Label::textColourId));
        g.drawFittedText(text, area, getJustificationType(), lines, getMinimumHorizontalScale());
    }
private:
    float minPixels = 0.0f;
    float shadow = 0.0f;
    int lines = 1;
};

// Lit segment bar for the final-output sample peak. Presentation only.
class MeterBar final : public juce::Component
{
public:
    MeterBar() { setInterceptsMouseClicks(false, false); }
    void setLevel(float dbfs, bool overRange, bool invalid)
    {
        const int lit = dbfs <= floorDb ? 0 : juce::jlimit(0, segments, (int) std::floor((dbfs - floorDb) / stepDb + 0.5f));
        if (lit != litSegments || overRange != over || invalid != nonFinite)
        {
            litSegments = lit; over = overRange; nonFinite = invalid;
            repaint();
        }
    }
    void paint(juce::Graphics& g) override
    {
        const float sx = (float) getWidth() / geo::meterBar.getWidth();
        const float sy = (float) getHeight() / geo::meterBar.getHeight();
        g.addTransform(juce::AffineTransform::scale(sx, sy));
        const float w = geo::meterBar.getWidth(), h = geo::meterBar.getHeight();
        const float ledW = 147.0f / (float) segments - 1.6f;
        for (int i = 0; i < segments; ++i)
        {
            const float db = floorDb + (float) (i + 1) * stepDb;
            const juce::Colour hue = db > -3.0f ? juce::Colour(0xffff5a3c) : db > -12.0f ? juce::Colour(0xffffc13d) : juce::Colour(0xff6fe08a);
            const auto seg = juce::Rectangle<float>((float) i * (ledW + 1.6f), 1.0f, ledW, h - 2.0f);
            const bool lit = i < litSegments;
            if (lit)
            {
                g.setColour(hue.withAlpha(0.22f));
                g.fillRoundedRectangle(seg.expanded(1.4f), 2.0f);
            }
            g.setGradientFill(art::vgrad(lit ? hue.brighter(0.35f) : hue.withMultipliedBrightness(0.20f).withAlpha(0.55f),
                                         lit ? hue.darker(0.2f) : hue.withMultipliedBrightness(0.09f).withAlpha(0.55f), seg));
            g.fillRoundedRectangle(seg, 1.0f);
            if (lit)
            {
                g.setColour(art::white(0.35f));
                g.fillRect(seg.getX() + 0.5f, seg.getY() + 0.5f, seg.getWidth() - 1.0f, 1.0f);
            }
        }
        const auto lamp = juce::Rectangle<float>(w - 19.0f, 0.0f, 19.0f, h);
        const bool alarm = over || nonFinite;
        const juce::Colour hue = nonFinite ? juce::Colour(0xffff3b30) : juce::Colour(0xffff7a2e);
        if (alarm)
        {
            g.setColour(hue.withAlpha(0.28f));
            g.fillRoundedRectangle(lamp.expanded(1.5f), 3.0f);
        }
        g.setGradientFill(art::vgrad(alarm ? hue.brighter(0.3f) : juce::Colour(0xff3a1a14), alarm ? hue.darker(0.2f) : juce::Colour(0xff1a0907), lamp));
        g.fillRoundedRectangle(lamp, 2.0f);
        g.setColour(alarm ? juce::Colour(0xff2a0a05) : juce::Colour(0xffa56a55).withAlpha(0.8f));
        g.setFont(displayFont(7.5f, juce::Font::bold));
        g.drawText("OVR", lamp, juce::Justification::centred, false);
    }
    static constexpr int segments = 24;
private:
    static constexpr float floorDb = -60.0f;
    static constexpr float stepDb = 2.5f;
    int litSegments = 0;
    bool over = false, nonFinite = false;
};

// ---------------------------------------------------------------------------
// Live voice-flow plate: signal path, envelopes and filter response for the
// selected channel, computed from the panel (or the sounding tone/memory).
// ---------------------------------------------------------------------------
class VoicePlate final : public juce::Component
{
public:
    struct Snapshot
    {
        std::array<float, cs80::CP_count> v {};
        int channel = 0;
        int source = 13;
        bool operator==(const Snapshot& o) const noexcept
        {
            return channel == o.channel && source == o.source && v == o.v;
        }
    };

    VoicePlate()
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(false);
        setTitle("Voice signal flow display");
        setDescription("Live curves for the selected channel: VCF envelope, VCA envelope and filter response.");
    }

    void setSnapshot(const Snapshot& s, const juce::String& sourceName)
    {
        if (s == snap && sourceName == sourceLabel) return;
        snap = s;
        sourceLabel = sourceName;
        repaint();
    }
    void setScopesVisible(bool shouldShow)
    {
        if (shouldShow == scopes) return;
        scopes = shouldShow;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / geo::plate.getWidth(),
                                                    (float) getHeight() / geo::plate.getHeight()));
        paintCaption(g);
        paintFlow(g);
        if (! scopes) return;
        paintVcfScope(g, { 14.0f, 116.0f, 160.0f, 106.0f });
        paintVcaScope(g, { 186.0f, 116.0f, 160.0f, 106.0f });
        paintFilterScope(g, { 14.0f, 230.0f, 332.0f, 72.0f });
    }

private:
    using Rf = juce::Rectangle<float>;
    static juce::Colour amber() { return juce::Colour(0xffe8ac3c); }
    static juce::Colour amberHot() { return juce::Colour(0xffffd98a); }

    float p(int local) const { return snap.v[(size_t) local]; }

    static juce::String time(float seconds)
    {
        if (seconds < 0.1f) return juce::String(seconds * 1000.0f, 0) + " ms";
        if (seconds < 1.0f) return juce::String(juce::roundToInt(seconds * 1000.0f)) + " ms";
        return juce::String(seconds, seconds < 10.0f ? 2 : 1) + " s";
    }
    static juce::String hertz(float hz)
    {
        return hz >= 1000.0f ? juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz" : juce::String(juce::roundToInt(hz)) + " Hz";
    }

    void paintCaption(juce::Graphics& g)
    {
        const auto text = juce::String(snap.channel == 0 ? "CH I" : "CH II") + "  /  " + sourceLabel;
        art::legend(g, text, { 150.0f, 40.0f, 196.0f, 12.0f }, spacedFont(7.5f, 0.08f), juce::Justification::centredRight,
                    juce::Colour(0xffcdb878));
    }

    // -- signal path ----------------------------------------------------------
    void paintFlow(juce::Graphics& g)
    {
        const char* names[] = { "VCO", "HPF", "LPF", "VCA" };
        const bool active[] = { true, p(cs80::CP_hpfOn) >= 0.5f, p(cs80::CP_lpfOn) >= 0.5f, true };
        const float y = 64.0f, h = 26.0f, w = 62.0f;
        for (int i = 0; i < 4; ++i)
        {
            const Rf box(14.0f + (float) i * 84.0f, y, w, h);
            // arrows between blocks and out of the last one
            const float ax0 = box.getRight() + 1.5f, ax1 = box.getRight() + 20.5f;
            const bool signalThrough = active[i] || i == 0 || i == 3;
            if (i < 3 || true)
                arrow(g, ax0, y + h * 0.5f, i < 3 ? ax1 : ax1 - 2.0f, y + h * 0.5f, signalThrough ? 0.95f : 0.55f);
            if (! active[i])
            {
                // bypass rail over a switched-out filter
                const float x0 = box.getX() - 10.0f, x1 = box.getRight() + 10.0f;
                juce::Path rail;
                rail.startNewSubPath(x0, y + h * 0.5f);
                rail.lineTo(x0, y - 5.0f);
                rail.lineTo(x1, y - 5.0f);
                rail.lineTo(x1, y + h * 0.5f);
                g.setColour(art::black(0.5f));
                g.strokePath(rail, juce::PathStrokeType(1.0f), juce::AffineTransform::translation(0.0f, 1.0f));
                g.setColour(juce::Colour(0xffe8ac3c).withAlpha(0.8f));
                g.strokePath(rail, juce::PathStrokeType(1.0f));
            }
            // block body
            art::recessTray(g, box, 2.5f, juce::Colour(0xff17140f), juce::Colour(0xff090807), 6.0f, 0.7f);
            if (active[i])
            {
                g.setGradientFill(juce::ColourGradient(amber().withAlpha(0.20f), box.getCentreX(), box.getCentreY(),
                                                       juce::Colours::transparentBlack, box.getX(), box.getCentreY(), true));
                g.fillRoundedRectangle(box.reduced(1.0f), 2.0f);
            }
            g.setColour(juce::Colour(0xffcdb878).withAlpha(active[i] ? 0.75f : 0.30f));
            g.drawRoundedRectangle(box.reduced(0.5f), 2.5f, 0.8f);
            art::legend(g, names[i], box.withTrimmedBottom(i == 0 ? 5.0f : 0.0f), spacedFont(11.0f, 0.10f),
                        juce::Justification::centred,
                        active[i] ? juce::Colour(0xfff1e6bd) : juce::Colour(0xff7b725d));
            if (i == 0)
            {
                const float levels[] = { p(cs80::CP_saw), p(cs80::CP_pulse), p(cs80::CP_noise) };
                for (int k = 0; k < 3; ++k)
                {
                    const juce::Point<float> c(box.getCentreX() + (float) (k - 1) * 11.0f, box.getBottom() - 4.5f);
                    const bool lit = levels[k] > 0.02f;
                    if (lit)
                    {
                        g.setColour(amberHot().withAlpha(0.35f));
                        g.fillEllipse(c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);
                    }
                    g.setColour(lit ? amberHot() : juce::Colour(0xff3a342a));
                    g.fillEllipse(c.x - 1.4f, c.y - 1.4f, 2.8f, 2.8f);
                }
            }
            if (i == 3)
            {
                // VCA level as a thin lit rule under the label
                const float lvl = juce::jlimit(0.0f, 1.0f, p(cs80::CP_vLevel));
                g.setColour(juce::Colour(0xff2a261e));
                g.fillRect(box.getX() + 8.0f, box.getBottom() - 5.0f, w - 16.0f, 1.8f);
                g.setColour(amberHot());
                g.fillRect(box.getX() + 8.0f, box.getBottom() - 5.0f, (w - 16.0f) * lvl, 1.8f);
            }
        }
        // sine path under the filters straight to the amplifier
        const float sine = juce::jlimit(0.0f, 1.0f, p(cs80::CP_sine));
        const float yy = y + h + 14.0f;
        const float a = 0.35f + 0.65f * sine;
        juce::Path route;
        route.startNewSubPath(45.0f, y + h);
        route.lineTo(45.0f, yy);
        route.lineTo(297.0f, yy);
        route.lineTo(297.0f, y + h + 3.0f);
        g.setColour(art::black(0.55f * a));
        g.strokePath(route, juce::PathStrokeType(1.0f), juce::AffineTransform::translation(0.0f, 1.0f));
        g.setColour(juce::Colour(0xffcdb878).withAlpha(a));
        g.strokePath(route, juce::PathStrokeType(1.0f));
        juce::Path head;
        head.addTriangle(297.0f, y + h + 0.5f, 293.5f, y + h + 6.5f, 300.5f, y + h + 6.5f);
        g.fillPath(head);
        g.setColour(juce::Colour(0xff0d0b08));
        g.fillRect(142.0f, yy - 5.5f, 76.0f, 11.0f);
        art::legend(g, "SINE  " + juce::String(juce::roundToInt(sine * 100.0f)) + "%", { 142.0f, yy - 6.0f, 76.0f, 12.0f },
                    spacedFont(7.5f, 0.10f), juce::Justification::centred, juce::Colour(0xffcdb878).withAlpha(a));
    }

    void arrow(juce::Graphics& g, float x0, float y0, float x1, float y1, float alpha)
    {
        g.setColour(art::black(0.55f * alpha));
        g.drawLine(x0, y0 + 1.0f, x1 - 4.0f, y1 + 1.0f, 1.0f);
        g.setColour(juce::Colour(0xffcdb878).withAlpha(alpha));
        g.drawLine(x0, y0, x1 - 4.0f, y1, 1.0f);
        juce::Path head;
        head.addTriangle(x1, y1, x1 - 6.0f, y1 - 3.4f, x1 - 6.0f, y1 + 3.4f);
        g.fillPath(head);
    }

    // -- scope framing -------------------------------------------------------
    void scopeFrame(juce::Graphics& g, Rf r, const juce::String& title, const juce::String& readout)
    {
        art::recessTray(g, r, 3.0f, juce::Colour(0xff1b1710), juce::Colour(0xff0a0805), 8.0f, 0.8f);
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(r.reduced(1.0f).toNearestIntEdges());
            // warm glow in the centre of the glass
            g.setGradientFill(juce::ColourGradient(amber().withAlpha(0.10f), r.getCentreX(), r.getCentreY(),
                                                   juce::Colours::transparentBlack, r.getX(), r.getY(), true));
            g.fillRect(r);
            g.setGradientFill(juce::ColourGradient(art::white(0.07f), r.getX(), r.getY(),
                                                   juce::Colours::transparentWhite, r.getRight(), r.getCentreY(), false));
            g.fillRect(r.withHeight(r.getHeight() * 0.5f));
        }
        art::legend(g, title, { r.getX() + 6.0f, r.getY() + 3.0f, r.getWidth() - 12.0f, 10.0f }, spacedFont(7.5f, 0.10f),
                    juce::Justification::centredLeft, juce::Colour(0xffcdb878));
        art::legend(g, readout, { r.getX() + 4.0f, r.getBottom() - 12.5f, r.getWidth() - 8.0f, 10.0f }, displayFont(7.5f, juce::Font::bold),
                    juce::Justification::centred, juce::Colour(0xffb5aa86), 0.8f);
    }

    Rf plotArea(Rf r) const { return { r.getX() + 8.0f, r.getY() + 16.0f, r.getWidth() - 16.0f, r.getHeight() - 31.0f }; }

    void grid(juce::Graphics& g, Rf plot, int cols, int rows)
    {
        g.setColour(amber().withAlpha(0.075f));
        for (int i = 0; i <= rows; ++i)
            g.drawHorizontalLine((int) (plot.getY() + plot.getHeight() * (float) i / (float) rows), plot.getX(), plot.getRight());
        for (int i = 0; i <= cols; ++i)
            g.drawVerticalLine((int) (plot.getX() + plot.getWidth() * (float) i / (float) cols), plot.getY(), plot.getBottom());
    }

    // Relative width of a stage on a compressed time axis (1 ms .. 20 s).
    static float stageWidth(float seconds)
    {
        const float t = juce::jlimit(0.0f, 1.0f, std::log10(juce::jmax(0.001f, seconds) * 1000.0f) / 4.3f);
        return 0.20f + 0.80f * t;
    }

    static float approach(float t, float k)
    {
        return (1.0f - std::exp(-k * t)) / (1.0f - std::exp(-k));
    }

    struct Stages { float xA, xD, xS, xR; float x0, x1; };
    Stages stages(Rf plot, float a, float d, float r) const
    {
        const float wa = stageWidth(a), wd = stageWidth(d), wh = 0.55f, wr = stageWidth(r);
        const float total = wa + wd + wh + wr;
        Stages s {};
        s.x0 = plot.getX();
        s.x1 = plot.getRight();
        const float u = plot.getWidth() / total;
        s.xA = s.x0 + wa * u;
        s.xD = s.xA + wd * u;
        s.xS = s.xD + wh * u;
        s.xR = s.x1;
        return s;
    }

    void strokeCurve(juce::Graphics& g, const juce::Path& line, const juce::Path& fill, Rf plot)
    {
        g.setGradientFill(art::vgrad(amber().withAlpha(0.34f), amber().withAlpha(0.03f), plot));
        g.fillPath(fill);
        g.setColour(amber().withAlpha(0.22f));
        g.strokePath(line, juce::PathStrokeType(3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(amberHot());
        g.strokePath(line, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void stageTicks(juce::Graphics& g, Rf plot, const Stages& s, const char* letters)
    {
        const float xs[] = { s.x0, s.xA, s.xD, s.xS, s.xR };
        g.setColour(juce::Colour(0xffcdb878).withAlpha(0.18f));
        const float dash[] = { 1.5f, 2.5f };
        for (int i = 1; i < 4; ++i)
            g.drawDashedLine(juce::Line<float>(xs[i], plot.getY(), xs[i], plot.getBottom()), dash, 2, 0.8f);
        g.setFont(displayFont(6.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xffcdb878).withAlpha(0.7f));
        for (int i = 0; i < 4; ++i)
            g.drawText(juce::String::charToString((juce::juce_wchar) letters[i]),
                       juce::Rectangle<float>(xs[i], plot.getBottom() - 8.0f, xs[i + 1] - xs[i], 8.0f), juce::Justification::centred, false);
    }

    void paintVcfScope(juce::Graphics& g, Rf r)
    {
        const float il = p(cs80::CP_fIl), al = p(cs80::CP_fAl);
        const float a = p(cs80::CP_fA), d = p(cs80::CP_fD), rel = p(cs80::CP_fR);
        scopeFrame(g, r, "VCF ENVELOPE", "A " + time(a) + "  D " + time(d) + "  R " + time(rel));
        const auto plot = plotArea(r);
        grid(g, plot, 8, 4);
        const auto s = stages(plot, a, d, rel);
        const float mid = plot.getCentreY(), span = plot.getHeight() * 0.46f;
        auto yOf = [&](float oct) { return mid - oct / 3.0f * span; };
        const float v0 = -3.0f * il, vA = 3.0f * al;
        // steady cutoff reference
        g.setColour(juce::Colour(0xffcdb878).withAlpha(0.35f));
        const float dash[] = { 3.0f, 3.0f };
        g.drawDashedLine(juce::Line<float>(plot.getX(), mid, plot.getRight(), mid), dash, 2, 0.8f);
        stageTicks(g, plot, s, "ADSR");
        juce::Path line;
        line.startNewSubPath(s.x0, yOf(v0));
        auto seg = [&](float xa, float xb, float va, float vb, float k)
        {
            for (int i = 1; i <= 14; ++i)
            {
                const float t = (float) i / 14.0f;
                line.lineTo(xa + (xb - xa) * t, yOf(va + (vb - va) * approach(t, k)));
            }
        };
        seg(s.x0, s.xA, v0, vA, 2.2f);
        seg(s.xA, s.xD, vA, 0.0f, 4.0f);
        line.lineTo(s.xS, yOf(0.0f));
        seg(s.xS, s.xR, 0.0f, v0, 4.0f);
        juce::Path fill(line);
        fill.lineTo(s.xR, mid);
        fill.lineTo(s.x0, mid);
        fill.closeSubPath();
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(plot.expanded(1.0f).toNearestIntEdges());
            strokeCurve(g, line, fill, plot);
        }
        marker(g, { s.x0, yOf(v0) }, "IL", true);
        marker(g, { s.xA, yOf(vA) }, "AL", false);
    }

    void marker(juce::Graphics& g, juce::Point<float> c, const char* text, bool left)
    {
        g.setColour(amberHot().withAlpha(0.4f));
        g.fillEllipse(c.x - 3.4f, c.y - 3.4f, 6.8f, 6.8f);
        g.setColour(amberHot());
        g.fillEllipse(c.x - 1.7f, c.y - 1.7f, 3.4f, 3.4f);
        art::legend(g, text, { left ? c.x + 3.0f : c.x - 22.0f, c.y - 13.0f, 19.0f, 9.0f }, displayFont(6.5f, juce::Font::bold),
                    left ? juce::Justification::centredLeft : juce::Justification::centredRight, juce::Colour(0xfff1e6bd));
    }

    void paintVcaScope(juce::Graphics& g, Rf r)
    {
        const float a = p(cs80::CP_vA), d = p(cs80::CP_vD), s = juce::jlimit(0.0f, 1.0f, p(cs80::CP_vS)), rel = p(cs80::CP_vR);
        scopeFrame(g, r, "VCA ENVELOPE",
                   "A " + time(a) + "  D " + time(d) + "  S " + juce::String(juce::roundToInt(s * 100.0f)) + "%  R " + time(rel));
        const auto plot = plotArea(r);
        grid(g, plot, 8, 4);
        const auto st = stages(plot, a, d, rel);
        const float bottom = plot.getBottom() - 2.0f, top = plot.getY() + 3.0f;
        auto yOf = [&](float v) { return bottom - v * (bottom - top); };
        stageTicks(g, plot, st, "ADSR");
        juce::Path line;
        line.startNewSubPath(st.x0, yOf(0.0f));
        auto seg = [&](float xa, float xb, float va, float vb, float k)
        {
            for (int i = 1; i <= 14; ++i)
            {
                const float t = (float) i / 14.0f;
                line.lineTo(xa + (xb - xa) * t, yOf(va + (vb - va) * approach(t, k)));
            }
        };
        seg(st.x0, st.xA, 0.0f, 1.0f, 2.2f);
        seg(st.xA, st.xD, 1.0f, s, 4.0f);
        line.lineTo(st.xS, yOf(s));
        seg(st.xS, st.xR, s, 0.0f, 4.0f);
        juce::Path fill(line);
        fill.lineTo(st.xR, bottom);
        fill.lineTo(st.x0, bottom);
        fill.closeSubPath();
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(plot.expanded(1.0f).toNearestIntEdges());
            strokeCurve(g, line, fill, plot);
        }
        marker(g, { st.xA, yOf(1.0f) }, "", false);
    }

    static float svfMagnitude(float f, float fc, float q, bool highPass)
    {
        const float w = f / juce::jmax(1.0f, fc);
        const float w2 = w * w;
        const float denom = std::sqrt((1.0f - w2) * (1.0f - w2) + (w / q) * (w / q));
        return (highPass ? w2 : 1.0f) / juce::jmax(1.0e-6f, denom);
    }

    void paintFilterScope(juce::Graphics& g, Rf r)
    {
        const bool hpOn = p(cs80::CP_hpfOn) >= 0.5f, lpOn = p(cs80::CP_lpfOn) >= 0.5f;
        const float hpf = p(cs80::CP_hpfCutoff), lpf = p(cs80::CP_lpfCutoff);
        const float hq = 1.0f / (2.0f - 1.8f * juce::jlimit(0.0f, 1.0f, p(cs80::CP_hpfRes)));
        const float lq = 1.0f / (2.0f - 1.8f * juce::jlimit(0.0f, 1.0f, p(cs80::CP_lpfRes)));
        scopeFrame(g, r, "FILTER RESPONSE",
                   juce::String(hpOn ? "HPF " + hertz(hpf) : "HPF OFF") + "    " + (lpOn ? "LPF " + hertz(lpf) : "LPF OFF"));
        auto plot = plotArea(r).withTrimmedBottom(-3.0f).withTrimmedTop(-3.0f).reduced(2.0f, 0.0f);
        plot.setHeight(plot.getHeight() - 3.0f);
        // grid: decades on a log axis, dB lines
        const float fLo = 20.0f, fHi = 20000.0f;
        auto xOf = [&](float f) { return plot.getX() + plot.getWidth() * std::log10(f / fLo) / std::log10(fHi / fLo); };
        const float dbTop = 18.0f, dbBottom = -54.0f;
        auto yOf = [&](float db) { return plot.getY() + plot.getHeight() * (dbTop - juce::jlimit(dbBottom, dbTop, db)) / (dbTop - dbBottom); };
        g.setColour(amber().withAlpha(0.075f));
        for (float f : { 100.0f, 1000.0f, 10000.0f })
            g.drawVerticalLine((int) xOf(f), plot.getY(), plot.getBottom());
        for (float db : { 12.0f, 0.0f, -12.0f, -24.0f, -36.0f })
            g.drawHorizontalLine((int) yOf(db), plot.getX(), plot.getRight());
        g.setColour(juce::Colour(0xffcdb878).withAlpha(0.28f));
        const float dash[] = { 3.0f, 3.0f };
        g.drawDashedLine(juce::Line<float>(plot.getX(), yOf(0.0f), plot.getRight(), yOf(0.0f)), dash, 2, 0.8f);
        g.setFont(displayFont(6.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xffcdb878).withAlpha(0.55f));
        const std::pair<float, const char*> decades[] = { { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } };
        for (const auto& decade : decades)
            g.drawText(decade.second, juce::Rectangle<float>(xOf(decade.first) + 1.5f, plot.getBottom() - 8.0f, 20.0f, 8.0f),
                       juce::Justification::centredLeft, false);

        juce::Path line;
        for (int i = 0; i <= 140; ++i)
        {
            const float f = fLo * std::pow(fHi / fLo, (float) i / 140.0f);
            float m = 1.0f;
            if (hpOn) m *= svfMagnitude(f, hpf, hq, true);
            if (lpOn) m *= svfMagnitude(f, lpf, lq, false);
            const float db = 20.0f * std::log10(juce::jmax(1.0e-5f, m));
            const float x = xOf(f), y = yOf(db);
            if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
        }
        juce::Path fill(line);
        fill.lineTo(plot.getRight(), plot.getBottom());
        fill.lineTo(plot.getX(), plot.getBottom());
        fill.closeSubPath();
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(plot.expanded(1.0f, 2.0f).toNearestIntEdges());
            strokeCurve(g, line, fill, plot);
        }
        auto cutoffMark = [&](float f, const char* tag, bool on)
        {
            if (! on) return;
            const float x = xOf(f), y = plot.getBottom() + 0.5f;
            juce::Path tri;
            tri.addTriangle(x, y - 4.5f, x - 3.2f, y + 1.0f, x + 3.2f, y + 1.0f);
            g.setColour(juce::Colour(0xfff1e6bd));
            g.fillPath(tri);
            art::legend(g, tag, { x - 5.0f, y - 14.0f, 10.0f, 8.0f }, displayFont(6.0f, juce::Font::bold),
                        juce::Justification::centred, juce::Colour(0xfff1e6bd).withAlpha(0.8f));
        };
        cutoffMark(hpf, "H", hpOn);
        cutoffMark(lpf, "L", lpOn);
    }

    Snapshot snap;
    juce::String sourceLabel { "PANEL" };
    bool scopes = true;
};

// Magnified view of the four independent analog-memory panels. This does not
// copy a live patch: moving each miniature control edits that bank directly.
class MemoryProgrammer final : public juce::Component
{
public:
    explicit MemoryProgrammer(Cs80AudioProcessor& p) : processor(p)
    {
        setOpaque(true);
        addAndMakeVisible(close);
        setTitle("Memory programmer");
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        close.setButtonText("CLOSE LID");
        close.setTitle("Close memory programmer");
        close.setWantsKeyboardFocus(true);
        close.onClick = [this] { if (onClose) onClose(); };
        for (int bank = 0; bank < 4; ++bank)
        {
            auto button = std::make_unique<KeyButton>("MEMORY " + juce::String(bank + 1), KeyButton::Style::plate);
            button->setClickingTogglesState(false);
            button->setWantsKeyboardFocus(true);
            button->setTitle("Select memory " + juce::String(bank + 1));
            button->onClick = [this, bank] { processor.selectHardwareSource(bank % 2, bank < 2 ? 11 : 12); refresh(); };
            button->setTooltip("Listen to this independent memory on channel " + juce::String(bank % 2 + 1));
            addAndMakeVisible(*button);
            bankButtons[(size_t) bank] = std::move(button);
            for (int col = 0; col < 26; ++col)
            {
                const int local = order[(size_t) col];
                const auto& def = cs80::kChannelDefs[local];
                Item item;
                item.bank = bank; item.local = local;
                const bool discrete = local == cs80::CP_saw || local == cs80::CP_pulse;
                if (discrete)
                {
                    item.toggle = std::make_unique<ui::LedButton>(local == cs80::CP_saw ? "SAW" : "PULSE");
                    auto* toggle = item.toggle.get();
                    toggle->hardwareTab = true;
                    toggle->accent = ui::capCream;
                    toggle->setTitle("Memory " + juce::String(bank + 1) + " " + def.name);
                    toggle->setComponentID(Cs80AudioProcessor::memoryParameterId(bank, local));
                    item.buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                        processor.getAPVTS(), Cs80AudioProcessor::memoryParameterId(bank, local), *toggle);
                    addAndMakeVisible(*toggle);
                }
                else
                {
                    item.slider = std::make_unique<ui::FocusSlider>();
                    auto* slider = item.slider.get();
                    slider->setSliderStyle(juce::Slider::LinearVertical);
                    slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                    slider->setRange(def.min, def.max);
                    slider->setSkewFactor(def.skew);
                    slider->setColour(juce::Slider::thumbColourId, ui::capCream);
                    slider->setDoubleClickReturnValue(true, def.def);
                    slider->setTitle("Memory " + juce::String(bank + 1) + " " + def.name);
                    slider->setTooltip(slider->getTitle());
                    slider->setDescription("Independent analog memory control. Use the arrow keys to adjust; select this memory to hear it.");
                    slider->setComponentID(Cs80AudioProcessor::memoryParameterId(bank, local));
                    item.sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                        processor.getAPVTS(), Cs80AudioProcessor::memoryParameterId(bank, local), *slider);
                    slider->onValueChange = [bank, local, slider] {
                        slider->setTooltip(slider->getTitle() + ": "
                            + ui::formatParamValue(cs80::chParam(bank % 2, local), (float) slider->getValue()));
                    };
                    addAndMakeVisible(*slider);
                }
                items.push_back(std::move(item));
            }
        }
        refresh();
    }

    void refresh()
    {
        for (auto& item : items)
        {
            if (item.slider && ! item.slider->isMouseButtonDown())
            {
                item.slider->setTooltip(item.slider->getTitle() + ": "
                    + ui::formatParamValue(cs80::chParam(item.bank % 2, item.local), (float) item.slider->getValue()));
            }
            if (item.toggle)
            {
                item.toggle->setTooltip(item.toggle->getTitle() + (item.toggle->getToggleState() ? ": ON" : ": OFF"));
            }
        }
        for (int bank = 0; bank < 4; ++bank)
            bankButtons[(size_t) bank]->setToggleState(
                processor.getHardwareSource(bank % 2) == (bank < 2 ? 11 : 12), juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override
    {
        g.saveState();
        const float sx = (float) getWidth() / 1472.0f, sy = (float) getHeight() / 554.0f;
        g.addTransform(juce::AffineTransform::scale(sx, sy));
        const juce::Rectangle<float> all(0, 0, 1472, 554);
        // Machined bezel, then the recessed black plate set into it.
        g.setGradientFill(art::vgrad(juce::Colour(0xff4d4943), juce::Colour(0xff17150f), all));
        g.fillRoundedRectangle(all, 9.0f);
        g.setColour(art::white(0.22f));
        g.drawLine(10, 1.5f, 1462, 1.5f, 1.0f);
        const juce::Rectangle<float> inner = all.reduced(9.0f);
        art::recessTray(g, inner, 5.0f, juce::Colour(0xff161411), juce::Colour(0xff070605), 16.0f, 0.9f);
        g.setGradientFill(juce::ColourGradient(art::white(0.045f), inner.getX(), inner.getY(),
                                               juce::Colours::transparentWhite, inner.getRight() * 0.6f, inner.getBottom(), false));
        g.fillRoundedRectangle(inner.reduced(1.0f), 5.0f);
        ui::etchRect(g, inner.reduced(7.0f), 3.0f, 0.55f);

        // Title block.
        art::legend(g, "MEMORY PROGRAMMER", { 34, 20, 520, 26 }, spacedFont(19.0f, 0.16f),
                    juce::Justification::centredLeft, juce::Colour(0xffe9d695));
        art::legend(g, "FOUR INDEPENDENT MINIATURE PANELS  /  MAGNIFIED VIEW", { 35, 48, 760, 14 },
                    spacedFont(8.5f, 0.16f), juce::Justification::centredLeft, juce::Colour(0xffa99f80));
        ui::etchLine(g, 34, 66, 480, 66, 0.7f);
        {
            // Gold signal-flow diagram: the programmer owns everything from VCO to VCA.
            const char* stages[] = { "VCO", "HPF", "LPF", "VCA" };
            for (int i = 0; i < 4; ++i)
            {
                const juce::Rectangle<float> box(620.0f + (float) i * 140.0f, 22.0f, 90.0f, 28.0f);
                art::recessTray(g, box, 3.0f, juce::Colour(0xff1b1813), juce::Colour(0xff0a0907), 6.0f, 0.8f);
                g.setColour(juce::Colour(0xffcdb878).withAlpha(0.7f));
                g.drawRoundedRectangle(box.reduced(0.5f), 3.0f, 0.8f);
                art::legend(g, stages[i], box, spacedFont(11.0f, 0.12f), juce::Justification::centred, juce::Colour(0xffe9d695));
                if (i < 3)
                {
                    const float x0 = box.getRight() + 3.0f, x1 = x0 + 44.0f, yy = box.getCentreY();
                    ui::etchLine(g, x0, yy, x1 - 5.0f, yy, 0.8f);
                    juce::Path head;
                    head.addTriangle(x1, yy, x1 - 6.0f, yy - 3.4f, x1 - 6.0f, yy + 3.4f);
                    g.setColour(juce::Colour(0xffcdb878).withAlpha(0.85f));
                    g.fillPath(head);
                }
            }
            art::legend(g, "26 CONTROLS PER MEMORY", { 620, 52, 380, 12 }, spacedFont(7.5f, 0.2f),
                        juce::Justification::centredLeft, juce::Colour(0xff9d9378));
        }
        for (int bank = 0; bank < 4; ++bank)
        {
            const float y = 75.0f + (float) bank * 111.0f;
            const juce::Rectangle<float> row(24.0f, y, 1424.0f, 103.0f);
            art::recessTray(g, row, 4.0f, juce::Colour(0xff211e1a), juce::Colour(0xff12100d), 9.0f, 0.75f);
            ui::etchRect(g, row.reduced(4.0f), 2.5f, 0.45f);
            // gold diagram: a signal rail running through the panel from the lid label
            g.setColour(juce::Colour(0xffcdb878).withAlpha(0.35f));
            g.fillRect(row.getX() + 8.0f, row.getY() + 30.0f, 118.0f, 1.0f);
            art::legend(g, bank % 2 == 0 ? "CHANNEL I" : "CHANNEL II", { 33.0f, y + 72.0f, 100.0f, 16.0f },
                        spacedFont(9.5f, 0.14f), juce::Justification::centred, juce::Colour(0xffe6dcbc));
            for (int col = 0; col < 26; ++col)
            {
                const float cx = 145.0f + (float) col * 50.0f;
                art::legend(g, labels[(size_t) col], { cx, y + 4.0f, 48.0f, 22.0f }, displayFont(8.0f, juce::Font::bold),
                            juce::Justification::centred, juce::Colour(0xffe6dcbc), 0.7f);
                if (col == 5 || col == 9 || col == 14 || col == 21)
                    ui::etchLine(g, cx + 49.0f, y + 12.0f, cx + 49.0f, y + 94.0f, 0.28f);
            }
        }
        art::legend(g, "Miniature controls are independent of the main panel. Select a memory to hear it; PANEL returns to the unchanged front-panel settings.",
                    { 34, 524, 1400, 16 }, displayFont(9.0f), juce::Justification::centredLeft, juce::Colour(0xffa39a7e));
        g.restoreState();
    }

    void resized() override
    {
        const float sx = (float) getWidth() / 1472.0f, sy = (float) getHeight() / 554.0f;
        auto r = [sx,sy](float x,float y,float w,float h) {
            return juce::Rectangle<int>(juce::roundToInt(x*sx),juce::roundToInt(y*sy),
                                       juce::roundToInt(w*sx),juce::roundToInt(h*sy));
        };
        close.setBounds(r(1318,20,126,30));
        for (int bank = 0; bank < 4; ++bank)
        {
            const float y = 75.0f + (float) bank * 111.0f;
            bankButtons[(size_t) bank]->setBounds(r(34,y+34,98,30));
            for (int col = 0; col < 26; ++col)
            {
                auto& item = items[(size_t)(bank*26+col)];
                if (item.slider) item.slider->setBounds(r(145+(float)col*50,y+28,48,72));
                if (item.toggle) item.toggle->setBounds(r(153+(float)col*50,y+38,32,54));
            }
        }
    }
    std::function<void()> onClose;

private:
    struct Item
    {
        int bank=0, local=0;
        std::unique_ptr<ui::FocusSlider> slider;
        std::unique_ptr<ui::LedButton> toggle;
        // Destroy attachments before their controls.
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
    };
    Cs80AudioProcessor& processor;
    KeyButton close { "CLOSE LID", KeyButton::Style::plate };
    std::array<std::unique_ptr<KeyButton>,4> bankButtons;
    std::vector<Item> items;
    const std::array<int,26> order {{
        cs80::CP_pwmSpeed, cs80::CP_pwmDepth, cs80::CP_pw, cs80::CP_pulse, cs80::CP_saw, cs80::CP_noise,
        cs80::CP_hpfCutoff, cs80::CP_hpfRes, cs80::CP_lpfCutoff, cs80::CP_lpfRes,
        cs80::CP_fIl, cs80::CP_fAl, cs80::CP_fA, cs80::CP_fD, cs80::CP_fR,
        cs80::CP_vcfLevel, cs80::CP_sine, cs80::CP_vA, cs80::CP_vD, cs80::CP_vS, cs80::CP_vR, cs80::CP_vLevel,
        cs80::CP_initBrill, cs80::CP_initLevel, cs80::CP_afterBrill, cs80::CP_afterLevel
    }};
    const std::array<const char*,26> labels {{
        "SPEED","PWM","PW","PULSE","SAW","NOISE","HPF","RES H","LPF","RES L",
        "IL","AL","A","D","R","VCF LEVEL","SINE","A","D","S","R","LEVEL",
        "INITIAL\nBRILL","INITIAL\nLEVEL","AFTER\nBRILL","AFTER\nLEVEL"
    }};
};

} // namespace

struct Cs80AudioProcessorEditor::Impl final : public juce::Component
{
    explicit Impl(Cs80AudioProcessor& p)
        : processor(p),
          keybed(p),
          pitchWheel("PITCH", true),
          modWheel("MOD", false),
          advanced(p),
          browser(p),
          memoryProgrammer(p)
    {
        // A new UI starts with fresh presentation history. This is the sole
        // consumer; audio published after this clear remains for normal polls.
        (void) processor.consumeOutputMeter();
        outputMeterDisplay.reset(juce::Time::getMillisecondCounterHiRes() * 0.001);
        setOpaque(true);
        setLookAndFeel(&lookAndFeel);
        setWantsKeyboardFocus(true);
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        setTitle("CS-80 instrument panel");

        addAndMakeVisible(backdrop);
        backdrop.draw = [this](juce::Graphics& g) { paintChassis(g); };
        addAndMakeVisible(voicePlate);

        buildTopBar();
        buildMasterSection();
        buildChannels();
        buildToneSection();
        buildPerformance();
        buildKeyboardControls();
        buildKeyboard();

        addChildComponent(scrim);
        scrim.onClick = [this] { if (browserVisible) showBrowser(false, true); else if (advancedVisible) showAdvanced(false, true); };
        addAndMakeVisible(advanced);
        advanced.setVisible(false);
        advanced.setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        addAndMakeVisible(browser);
        browser.setVisible(false);
        browser.setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        addChildComponent(shade);
        addAndMakeVisible(memoryProgrammer);
        memoryProgrammer.setVisible(false);
        memoryProgrammer.onClose = [this] {
            memoryProgrammer.setVisible(false);
            if (getPeer() != nullptr) memoryLid.grabKeyboardFocus();
        };

        advanced.onClose = [this] { showAdvanced(false, true); };
        browser.onClose = [this] { showBrowser(false, true); };
        browser.onPresetLoaded = [this]
        {
            syncPresetDisplay();
            syncToneMarks();
            repaint();
        };
        frames.onFrame = [this] { stepOverlayAnimation(); };

        // Host state/program changes may arrive on non-message threads.
        // The existing message-thread timer polls metadata and selectors;
        // never install a processor callback that captures this editor.
        syncPresetDisplay();
        syncToneMarks();
        syncSubMode();
        syncSourceStatus();
        syncVoicePlate();
    }

    ~Impl() override
    {
        frames.stop();
        setLookAndFeel(nullptr);
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key != juce::KeyPress::escapeKey) return false;
        if (memoryProgrammer.isVisible())
        {
            if (memoryProgrammer.onClose) memoryProgrammer.onClose();
            return true;
        }
        if (browserVisible) { showBrowser(false, true); return true; }
        if (advancedVisible) { showAdvanced(false, true); return true; }
        return false;
    }

    // -----------------------------------------------------------------------
    // Construction helpers
    // -----------------------------------------------------------------------
    void place(juce::Component& component, float x, float y, float w, float h)
    {
        placements.push_back({ &component, { x, y, w, h } });
    }

    ui::ParamKnob& addKnob(int parameter, float x, float y, float w, float h,
                           const juce::String& name = {})
    {
        auto knob = std::make_unique<ui::ParamKnob>(processor, parameter, name);
        auto* raw = knob.get();
        raw->getSlider().getProperties().set("hardwareRotary", true);
        addAndMakeVisible(*raw);
        place(*raw, x, y, w, h);
        knobs.push_back(std::move(knob));
        return *raw;
    }

    ui::LedButton& addLed(int parameter, const juce::String& label,
                          float x, float y, float w, float h)
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
        place(*raw, x, y, w, h);
        leds.push_back(std::move(button));
        return *raw;
    }

    void buildTopBar()
    {
        for (auto* button : { &prevButton, &nextButton, &browseButton, &saveButton,
                              &initButton, &advancedButton, &panicButton })
        {
            button->setWantsKeyboardFocus(true);
            addAndMakeVisible(*button);
        }

        presetLabel.setJustificationType(juce::Justification::centredLeft);
        presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffcf70));
        presetLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        presetLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(presetLabel);
        presetTagLabel.setJustificationType(juce::Justification::centredLeft);
        presetTagLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd09a3c));
        presetTagLabel.setInterceptsMouseClicks(false, false);
        presetTagLabel.setComponentID("preset_category_tag");
        addAndMakeVisible(presetTagLabel);
        presetIndexLabel.setJustificationType(juce::Justification::centredRight);
        presetIndexLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb88a3a));
        presetIndexLabel.setInterceptsMouseClicks(false, false);
        presetIndexLabel.setComponentID("preset_index");
        addAndMakeVisible(presetIndexLabel);
        subModeLabel.setComponentID("sub_mode_status");
        subModeLabel.setTitle("Sub oscillator compatibility and input status");
        subModeLabel.setJustificationType(juce::Justification::centredLeft);
        subModeLabel.setColour(juce::Label::textColourId, ui::ivoryDim);
        subModeLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(subModeLabel);
        modeStatusLabel.setComponentID("performance_mode_status");
        modeStatusLabel.setTitle("Requested performance mode and MIDI controller policy");
        modeStatusLabel.setJustificationType(juce::Justification::centredLeft);
        modeStatusLabel.setColour(juce::Label::textColourId, ui::ivoryDim);
        addAndMakeVisible(modeStatusLabel);
        outputMeterLabel.setComponentID("output_sample_peak");
        outputMeterLabel.setTitle("Output sample peak and over-range events");
        outputMeterLabel.setJustificationType(juce::Justification::centredLeft);
        outputMeterLabel.setTooltip("Read-only final-output sample peak in dBFS, not true peak or loudness. Numeric peak releases at 12 dB per second. OVER-RANGE holds for one second after a finite sample magnitude exceeds 1; it does not prove hard clipping. NON-FINITE holds for two seconds after NaN or infinity is observed and is excluded from the numeric peak. These independently collected events may appear in adjacent polls. This display never limits, normalizes or changes audio: leave monitoring headroom. Opening an editor clears old display history.");
        outputMeterLabel.setDescription(outputMeterLabel.getTooltip());
        addAndMakeVisible(outputMeterLabel);
        meterBar.setTitle("Output level bar");
        addAndMakeVisible(meterBar);
        updateOutputMeterLabel();
        for (int rank = 0; rank < 2; ++rank)
        {
            auto& label = sourceStatusLabels[(size_t) rank];
            label.setComponentID("source_route_status_" + juce::String(rank + 1));
            label.setTitle(juce::String(rank == 0 ? "Rank I" : "Rank II") + " requested source and model route");
            label.setJustificationType(juce::Justification::centredLeft);
            label.setColour(juce::Label::textColourId, ui::ivoryDim);
            addAndMakeVisible(label);
        }

        prevButton.setTitle("Previous preset");
        nextButton.setTitle("Next preset");
        browseButton.setTitle("Open preset library");
        saveButton.setTitle("Save a user preset");
        initButton.setTitle("Load the initial patch");
        advancedButton.setTitle("Show advanced controls");
        panicButton.setComponentID("panic_notes");
        panicButton.setTitle("Panic: stop existing notes on all MIDI channels");
        panicButton.setTooltip("Stop existing notes, sustain pedal latches and arpeggiator keys on all MIDI channels at the next audio block. "
                               "Clear queued on-screen keys; patch and controls stay unchanged. "
                               "FX tails and analog noise may remain; this is not output mute. New incoming notes can still play.");
        panicButton.setDescription(panicButton.getTooltip());
        panicButton.onClick = [this] { processor.requestPanicFromUI(); };
        prevButton.onClick = [this] { stepPreset(-1); };
        nextButton.onClick = [this] { stepPreset(1); };
        browseButton.onClick = [this]
        {
            showBrowser(true, true);
        };
        saveButton.onClick = [this]
        {
            showBrowser(true, true);
            browser.focusSaveField();
        };
        initButton.onClick = [this]
        {
            const auto& bank = processor.getFactoryPresets();
            int index = 0;
            for (int i = 0; i < (int) bank.size(); ++i)
                if (juce::String(bank[(size_t) i].name).equalsIgnoreCase("Init Patch"))
                {
                    index = i;
                    break;
                }
            processor.loadFactoryPreset(index);
            syncToneMarks();
        };
        advancedButton.setClickingTogglesState(true);
        advancedButton.onClick = [this] { showAdvanced(! advancedVisible, true); };
    }

    ui::ParamKnob& addFader(int parameter, float x, float y, float w, float h,
                            const juce::String& label, juce::Colour colour)
    {
        auto& control = addKnob(parameter, x, y, w, h, label);
        control.setHardwareSlider(colour);
        return control;
    }

    static juce::Colour whiteCap() { return ui::capCream; }
    static juce::Colour greenCap() { return ui::capGreen; }
    static juce::Colour redCap() { return ui::capPink; }
    static juce::Colour yellowCap() { return ui::capYellow; }
    static juce::Colour blackCap() { return ui::capBlack; }
    static juce::Colour greyCap() { return ui::capGrey; }

    void buildMasterSection()
    {
        // The lower strip follows Yamaha's PN2 panel: ring, sub oscillator,
        // feet, tone selectors, mix, brilliance and performance response.
        const float y = 510.0f, h = 126.0f;
        addKnob(cs80::P_masterTune, 49, y + 22, 62, 84, "PITCH");
        addFader(cs80::P_detune, 114, y, 39, h, "DETUNE", whiteCap());
        addFader(cs80::P_ringAttack, 161, y, 38, h, "ATTACK", blackCap());
        addFader(cs80::P_ringDecay, 201, y, 38, h, "DECAY", blackCap());
        addFader(cs80::P_ringDepth, 241, y, 38, h, "DEPTH", greyCap());
        addFader(cs80::P_ringSpeed, 281, y, 38, h, "SPEED", whiteCap());
        addFader(cs80::P_ringAmount, 321, y, 38, h, "MOD", greyCap());
        subFunction = &addFader(cs80::P_subWave, 373, y, 42, h, "FUNCTION", blackCap());
        subSpeed = &addFader(cs80::P_subSpeed, 417, y, 38, h, "SPEED", whiteCap());
        addFader(cs80::P_subVco, 457, y, 38, h, "VCO", whiteCap());
        addFader(cs80::P_subVcf, 497, y, 38, h, "VCF", greenCap());
        addFader(cs80::P_subVca, 537, y, 38, h, "VCA", greyCap());
        addFader(cs80::chParam(0, cs80::CP_feet), 582, y, 36, h, "FEET I", greyCap());
        addFader(cs80::chParam(1, cs80::CP_feet), 620, y, 36, h, "FEET II", greyCap());
        // Pitch follows the width of each engraved name so the legends never touch.
        addFader(cs80::P_mix, 1205, y, 38, h, "MIX", redCap());
        addFader(cs80::P_brilliance, 1243, y, 38, h, "BRILLIANCE", greenCap());
        addFader(cs80::P_resonance, 1289, y, 38, h, "RESONANCE", redCap());
        addFader(cs80::P_initBend, 1333, y, 38, h, "INIT BEND", whiteCap());
        addFader(cs80::P_subAtSpeed, 1373, y, 38, h, "AT SPEED", whiteCap());
        addFader(cs80::P_subAtVco, 1411, y, 38, h, "AT VCO", whiteCap());
        addFader(cs80::P_subAtVcf, 1449, y, 38, h, "AT VCF", greenCap());
        addKnob(cs80::P_volume, 1494, y + 24, 52, 84, "VOLUME");
    }

    void buildChannel(int channel, float y0)
    {
        auto p = [channel](int local) { return cs80::chParam(channel, local); };
        float x = 458.0f;
        const float fy = y0 + 24.0f, fh = 148.0f;
        auto fader = [&](int param, const char* name, juce::Colour colour)
        {
            addFader(p(param), x, fy, 38, fh, name, colour);
            x += 40.0f;
        };
        fader(cs80::CP_pwmSpeed, "SPEED", whiteCap());
        fader(cs80::CP_pwmDepth, "PWM", greenCap());
        fader(cs80::CP_pw, "PW", greenCap());
        auto& pulse = addLed(p(cs80::CP_pulse), "PULSE", x, fy + 52.0f, 36, 56);
        pulse.hardwareTab = true; pulse.accent = blackCap(); x += 40;
        auto& saw = addLed(p(cs80::CP_saw), "SAW", x, fy + 52.0f, 36, 56);
        saw.hardwareTab = true; saw.accent = blackCap(); x += 40;
        fader(cs80::CP_noise, "NOISE", greenCap());
        x += 9;
        fader(cs80::CP_hpfCutoff, "HPF", greenCap());
        fader(cs80::CP_hpfRes, "RES H", redCap());
        fader(cs80::CP_lpfCutoff, "LPF", greenCap());
        fader(cs80::CP_lpfRes, "RES L", redCap());
        fader(cs80::CP_fIl, "IL", blackCap());
        fader(cs80::CP_fAl, "AL", blackCap());
        fader(cs80::CP_fA, "A", blackCap());
        fader(cs80::CP_fD, "D", blackCap());
        fader(cs80::CP_fR, "R", yellowCap());
        x += 9;
        fader(cs80::CP_vcfLevel, "VCF LEVEL", greyCap());
        fader(cs80::CP_sine, "SINE", greyCap());
        fader(cs80::CP_vA, "A", blackCap());
        fader(cs80::CP_vD, "D", blackCap());
        fader(cs80::CP_vS, "S", blackCap());
        fader(cs80::CP_vR, "R", yellowCap());
        fader(cs80::CP_vLevel, "LEVEL", greyCap());
        x += 9;
        fader(cs80::CP_initBrill, "BRILLIANCE", greenCap());
        fader(cs80::CP_initLevel, "LEVEL", greyCap());
        fader(cs80::CP_afterBrill, "BRILLIANCE", greenCap());
        fader(cs80::CP_afterLevel, "LEVEL", greyCap());
    }

    void buildChannels()
    {
        buildChannel(0, geo::rowY0[0]);
        buildChannel(1, geo::rowY0[1]);
        // Explicit software-only voice settings stay under a separate lid.
        addAndMakeVisible(voiceOptions);
        voiceOptions.setButtonText("VOICE SETTINGS");
        voiceOptions.setTitle("Show software voice settings");
        voiceOptions.setWantsKeyboardFocus(true);
        voiceOptions.onClick = [this] {
            optionsVisible = ! optionsVisible;
            for (auto* component : optionComponents) component->setVisible(optionsVisible);
            voicePlate.setScopesVisible(! optionsVisible);
            backdrop.repaint();
            repaint();
        };
        place(voiceOptions, 72, 442, 324, 26);
        addAndMakeVisible(memoryLid);
        memoryLid.setButtonText("OPEN MEMORY LID");
        memoryLid.setTitle("Open memory programmer");
        memoryLid.setWantsKeyboardFocus(true);
        memoryLid.onClick = [this] {
            showAdvanced(false);
            showBrowser(false);
            memoryProgrammer.refresh();
            memoryProgrammer.setVisible(true);
            memoryProgrammer.toFront(true);
        };
        place(memoryLid, 72, 410, 324, 26);

        // Which channel the live curves describe.
        for (int channel = 0; channel < 2; ++channel)
        {
            auto& button = viewButtons[(size_t) channel];
            button.setClickingTogglesState(true);
            button.setRadioGroupId(0x300);
            button.setWantsKeyboardFocus(true);
            button.setTitle(channel == 0 ? "Show channel I voice curves" : "Show channel II voice curves");
            button.setTooltip(button.getTitle());
            button.setToggleState(channel == 0, juce::dontSendNotification);
            button.onClick = [this, channel] { viewChannel = channel; syncVoicePlate(); };
            addAndMakeVisible(button);
            place(button, 350.0f + (float) channel * 30.0f, 106.0f, channel == 0 ? 26.0f : 30.0f, 24.0f);
        }

        for (int channel = 0; channel < 2; ++channel)
        {
            const float y = 232.0f + (float) channel * 82.0f;
            auto p = [channel](int local) { return cs80::chParam(channel, local); };
            auto& pan = addKnob(p(cs80::CP_pan), 100, y, 66, 72, "PAN");
            auto& level = addKnob(p(cs80::CP_vLevel), 172, y, 66, 72, "LEVEL");
            auto& hp = addLed(p(cs80::CP_hpfOn), "HPF", 246, y + 10, 60, 24);
            auto& lp = addLed(p(cs80::CP_lpfOn), "LPF", 246, y + 40, 60, 24);
            optionComponents.insert(optionComponents.end(), { &pan, &level, &hp, &lp });
        }
        for (auto* component : optionComponents) component->setVisible(false);
    }

    void buildToneSection()
    {
        const std::array<juce::Colour, 14> colours = {
            yellowCap(), yellowCap(), redCap(), whiteCap(), yellowCap(),
            yellowCap(), yellowCap(), whiteCap(), greenCap(), greenCap(), greenCap(),
            whiteCap(), whiteCap(), whiteCap()
        };
        for (int channel = 0; channel < 2; ++channel)
        {
            for (int tone = 0; tone < 14; ++tone)
            {
                auto button = std::make_unique<juce::TextButton>(
                    (tone < 11 ? juce::String(cs80::hardwareToneName(tone, channel)).toUpperCase()
                     : tone == 13 ? juce::String("PANEL")
                     : juce::String("MEMORY ") + juce::String(channel + (tone == 11 ? 1 : 3))));
                const auto fullName = button->getButtonText();
                button->setTitle(fullName);
                button->setDescription((channel == 0 ? juce::String("Channel I") : juce::String("Channel II"))
                    + ": select this voice source with Space or Return. PANEL preserves the main-panel settings.");
                button->setTooltip(fullName + (channel == 0 ? " / CHANNEL I" : " / CHANNEL II"));
                button->setButtonText(fullName.replace("HARPSICHORD ", "HARPSI\nCHORD ")
                                              .replace("CLAVICHORD ", "CLAVI\nCHORD ")
                                              .replace("STRING ", "STRING\n")
                                              .replace("BRASS ", "BRASS\n")
                                              .replace("GUITAR ", "GUITAR\n")
                                              .replace("FUNKY ", "FUNKY\n")
                                              .replace("ORGAN ", "ORGAN\n")
                                              .replace("MEMORY ", "MEMORY\n"));
                button->setClickingTogglesState(true);
                button->setRadioGroupId(0x100 + channel);
                button->setWantsKeyboardFocus(true);
                button->getProperties().set("hardwareTab", true);
                button->setColour(juce::TextButton::buttonColourId, colours[(size_t) tone]);
                button->onClick = [this, channel, tone] { applyHardwareTone(channel, tone); };
                addAndMakeVisible(*button);
                place(*button, 677.0f + (float) tone * 37.0f,
                       527.0f + (float) channel * 45.0f, 34.0f, 41.0f);
                toneButtons[(size_t) channel][(size_t) tone] = std::move(button);
            }
        }
    }

    // Performance block: the left side of the deck.
    void buildPerformance()
    {
        const float y = 672.0f, h = 96.0f;
        addFader(cs80::P_masterSustain, 58, y, 42, h, "SUSTAIN", yellowCap());
        addFader(cs80::P_portamento, 102, y, 42, h, "PORTA", whiteCap());
        addFader(cs80::P_glideMode, 146, y, 42, h, "GLIDE", greyCap());
        addKnob(cs80::P_chorusSpeed, 194, y + 8, 56, 84, "SPEED");
        addKnob(cs80::P_chorusDepth, 252, y + 8, 56, 84, "DEPTH");
        auto& sus = addLed(cs80::P_sustainMode, "SUSTAIN II", 317, y + 26, 50, 62);
        sus.hardwareTab = true; sus.accent = whiteCap();
        auto& chorus = addLed(cs80::P_chorusOn, "CHORUS", 369, y + 26, 50, 62);
        chorus.hardwareTab = true; chorus.accent = yellowCap();
        auto& tremolo = addLed(cs80::P_chorusMode, "TREMOLO", 421, y + 26, 50, 62);
        tremolo.hardwareTab = true; tremolo.accent = whiteCap();
    }

    // Owner controls 9/10 (keyboard contour) plus the foot-controller group that
    // the original keeps on its left-hand panel: footswitch jack/contact, the two
    // assigners, EXP/WAH and the output switch. The right side of the deck.
    void buildKeyboardControls()
    {
        const float y = 672.0f, h = 96.0f;
        addFader(cs80::P_kbdBrillianceLow, 1132, y, 38, h, "LOW", greenCap());
        addFader(cs80::P_kbdBrillianceHigh, 1174, y, 38, h, "HIGH", greenCap());
        addFader(cs80::P_kbdLevelLow, 1226, y, 38, h, "LOW", greyCap());
        addFader(cs80::P_kbdLevelHigh, 1268, y, 38, h, "HIGH", greyCap());

        expressionPedal.setTitle("Foot controller position (expression)");
        expressionPedal.setComponentID("expression_pedal");
        expressionPedal.setDescription("On-screen foot controller: volume and the EXP-WAH sweep. MIDI CC11 follows this pedal with parity isolation OFF, in both legacy and source-performance modes. In isolation, CC11 is ignored; this on-screen pedal remains active.");
        expressionPedal.setValue(processor.getExpressionValue());
        expressionPedal.onValueChange = [this](float value) { processor.setExpressionFromUI(value); };
        addAndMakeVisible(expressionPedal);
        place(expressionPedal, 1318, 676, 38, 88);

        auto toggle = [this](int parameter, const char* label, const char* tip, float x, float y)
        {
            auto& led = addLed(parameter, label, x, y, 58, 22);
            led.setTooltip(tip);
        };
        toggle(cs80::P_footswitchPlugged, "PLUG",
               "Footswitch plugged into the jack. With no plug the switched jack acts as a pressed switch.", 1368, 692);
        toggle(cs80::P_footswitchPressed, "PRESS",
               "Footswitch contact. Latches; toggle it to play the switch.", 1430, 692);
        toggle(cs80::P_footMode, "WAH",
               "Foot controller selector: off = EXP (volume only), on = EXP-WAH.", 1492, 692);
        toggle(cs80::P_sustainAssign, "SUSTAIN",
               "Assign the footswitch to SUSTAIN. Plugged and open switch removes the sustain release.", 1368, 736);
        toggle(cs80::P_glideAssign, "PORTA",
               "Assign the footswitch to portamento/glissando. Plugged and open switch suppresses gliding.", 1430, 736);
        toggle(cs80::P_outputLevel, "HIGH",
               "Output level switch: lit = HIGH (unity), unlit = LOW.", 1492, 736);
    }

    void buildKeyboard()
    {
        processor.serviceKeyboardUI();
        keyboard.setTooltip("Drag down a held key for pressure; slide sideways for a new strike. Pointer pressure is disabled while the arpeggiator runs and when an active host voice shares the pitch (including other channels and release tails). After a mode change or PANIC, lift and strike again. MIDI aftertouch is unchanged.");
        keyboard.hostKeyDown = [this](int note) { return processor.isHostKeyDown(note); };
        keyboard.setAvailableRange(24, 84);
        keyboard.setLowestVisibleKey(24);
        keyboard.setScrollButtonsVisible(false);
        keyboard.setOctaveForMiddleC(4);
        keyboard.setVelocity(0.82f, true);
        keyboard.setMidiChannel(1);
        keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffd8d2c3));
        keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff0d1217));
        keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff3d392f));
        keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, ui::amber.withAlpha(0.22f));
        keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, ui::amber.withAlpha(0.46f));
        keyboard.setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha(0.55f));
        keyboard.setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, juce::Colour(0xff111a21));
        keyboard.setWantsKeyboardFocus(false);
        addAndMakeVisible(keybed);

        ribbon.onValueChange = [this](float value)
        {
            processor.setRibbonFromUI(value);
        };
        addAndMakeVisible(ribbon);

        pitchWheel.onValueChange = [this](float value)
        {
            processor.setPitchWheelFromUI(value);
        };
        addAndMakeVisible(pitchWheel);
        pitchWheel.setVisible(false);
        optionComponents.push_back(&pitchWheel);

        modWheel.onValueChange = [this](float value)
        {
            processor.setModWheelFromUI(value);
        };
        addAndMakeVisible(modWheel);
        modWheel.setVisible(false);
        optionComponents.push_back(&modWheel);
    }

    // -----------------------------------------------------------------------
    // Behaviour
    // -----------------------------------------------------------------------
    void stepPreset(int delta)
    {
        const auto& bank = processor.getFactoryPresets();
        if (bank.empty())
            return;
        const int count = (int) bank.size();
        int index = processor.getCurrentPresetIndex();
        // If the current state came from a user preset or an edited patch,
        // stepping starts from the closest factory program.
        index = juce::jlimit(0, count - 1, index);
        index = (index + delta + count) % count;
        processor.loadFactoryPreset(index);
        syncToneMarks();
    }

    void applyHardwareTone(int channel, int source)
    {
        processor.selectHardwareSource(channel, source);
        syncToneMarks();
    }

    void syncToneMarks()
    {
        for (int channel = 0; channel < 2; ++channel)
            for (int source = 0; source < 14; ++source)
                toneButtons[(size_t) channel][(size_t) source]->setToggleState(
                    processor.getHardwareSource(channel) == source, juce::dontSendNotification);
    }

    void syncPresetDisplay()
    {
        const auto metadata = processor.getPresetMetadata();
        if (presetLabel.getText() != metadata.name)
            presetLabel.setText(metadata.name, juce::dontSendNotification);
        const auto& bank = processor.getFactoryPresets();
        juce::String tag = "USER PATCH", index = "---";
        if (metadata.factoryIndex >= 0 && metadata.factoryIndex < (int) bank.size()
            && juce::String(bank[(size_t) metadata.factoryIndex].name) == metadata.name)
        {
            tag = juce::String(bank[(size_t) metadata.factoryIndex].category).toUpperCase();
            index = juce::String(metadata.factoryIndex + 1).paddedLeft('0', 3) + " / " + juce::String((int) bank.size());
        }
        if (presetTagLabel.getText() != tag) presetTagLabel.setText(tag, juce::dontSendNotification);
        if (presetIndexLabel.getText() != index) presetIndexLabel.setText(index, juce::dontSendNotification);
    }

    // -- overlays -----------------------------------------------------------
    // The animation is purely visual: bounds are final from the first frame,
    // and it only runs for a visible, interactive editor. Programmatic calls
    // (snapshot tools, tests, hosts) complete immediately.
    bool canAnimate() const { return getPeer() != nullptr && isShowing(); }

    void finishOverlayAnimation()
    {
        if (animTarget == nullptr && ! frames.running()) return;
        animStart = juce::Time::getMillisecondCounterHiRes() - 10000.0;
        stepOverlayAnimation();
    }

    void beginOverlayAnimation(juce::Component* target, bool opening)
    {
        finishOverlayAnimation();
        animTarget = target;
        animOpening = opening;
        animStart = juce::Time::getMillisecondCounterHiRes();
        target->setVisible(true);
        applyOverlayPhase(opening ? 0.0f : 1.0f);
        frames.start();
    }

    void applyOverlayPhase(float t)
    {
        if (animTarget == nullptr) return;
        const float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        const float slide = 22.0f * ((float) getHeight() / kDesignHeight);
        animTarget->setTransform(t >= 1.0f ? juce::AffineTransform()
                                           : juce::AffineTransform::translation(0.0f, (1.0f - e) * slide));
        animTarget->setAlpha(t >= 1.0f ? 1.0f : 0.15f + 0.85f * e);
        scrim.setAmount(e);
        shade.setVisible(true);
    }

    void stepOverlayAnimation()
    {
        if (animTarget == nullptr) { frames.stop(); return; }
        const double elapsed = juce::Time::getMillisecondCounterHiRes() - animStart;
        const float raw = juce::jlimit(0.0f, 1.0f, (float) (elapsed / animDurationMs));
        const float t = animOpening ? raw : 1.0f - raw;
        applyOverlayPhase(t);
        if (raw >= 1.0f)
        {
            frames.stop();
            auto* target = animTarget.getComponent();
            animTarget = nullptr;
            if (target != nullptr)
            {
                target->setTransform(juce::AffineTransform());
                target->setAlpha(1.0f);
                if (! animOpening) target->setVisible(false);
            }
            refreshOverlayChrome();
        }
    }

    // Scrim and toolbar shade follow whichever overlay is open (or closing).
    void refreshOverlayChrome()
    {
        const bool open = advancedVisible || browserVisible;
        scrim.setVisible(open || animTarget != nullptr);
        shade.setVisible(open || animTarget != nullptr);
        if (! open && animTarget == nullptr) scrim.setAmount(0.0f);
        scrim.toFront(false);
        if (advanced.isVisible()) advanced.toFront(false);
        if (browser.isVisible()) browser.toFront(false);
        shade.toFront(false);
        if (memoryProgrammer.isVisible()) memoryProgrammer.toFront(false);
    }

    void showAdvanced(bool shouldShow, bool animate = false)
    {
        finishOverlayAnimation();
        const bool wasVisible = advancedVisible;
        if (shouldShow) memoryProgrammer.setVisible(false);
        advancedVisible = shouldShow;
        advancedButton.setToggleState(shouldShow, juce::dontSendNotification);
        if (shouldShow)
        {
            browser.setVisible(false);
            browserVisible = false;
            advanced.toFront(true);
            if (animate && ! wasVisible && canAnimate())
                beginOverlayAnimation(&advanced, true);
            else
                advanced.setVisible(true);
        }
        else if (wasVisible)
        {
            if (animate && canAnimate())
                beginOverlayAnimation(&advanced, false);
            else
                advanced.setVisible(false);
            if (getPeer() != nullptr) advancedButton.grabKeyboardFocus();
        }
        else
            advanced.setVisible(false);
        refreshOverlayChrome();
        repaint();
    }

    void showBrowser(bool shouldShow, bool animate = false)
    {
        finishOverlayAnimation();
        const bool wasVisible = browserVisible;
        if (shouldShow) memoryProgrammer.setVisible(false);
        browserVisible = shouldShow;
        if (shouldShow)
        {
            browser.refresh();
            advanced.setVisible(false);
            advancedVisible = false;
            advancedButton.setToggleState(false, juce::dontSendNotification);
            browser.toFront(true);
            if (animate && ! wasVisible && canAnimate())
                beginOverlayAnimation(&browser, true);
            else
                browser.setVisible(true);
            browser.selectCurrentPreset();
        }
        else
        {
            if (animate && wasVisible && canAnimate())
                beginOverlayAnimation(&browser, false);
            else
                browser.setVisible(false);
            if (wasVisible && getPeer() != nullptr) browseButton.grabKeyboardFocus();
        }
        refreshOverlayChrome();
        repaint();
    }

    void syncSubMode()
    {
        if (subFunction == nullptr || subSpeed == nullptr) return;
        const bool isolated = processor.getParamValue(cs80::P_parityMode) >= 0.5f;
        const bool original = isolated || processor.getParamValue(cs80::P_subOriginalMode) >= 0.5f;
        const bool functionReady = subFunction->bindParameter(original ? (int) cs80::P_subOriginalFunction : (int) cs80::P_subWave);
        const bool speedReady = subSpeed->bindParameter(original ? (int) cs80::P_subOriginalSpeed : (int) cs80::P_subSpeed);
        juce::String status;
        if (!functionReady || !speedReady)
            status = "SUB MODE CHANGED / RELEASE CONTROL";
        else if (!original)
            status = "SUB: LEGACY / ORIGINAL IN ADVANCED";
        else if (std::lround(processor.getParamValue(cs80::P_subOriginalFunction)) == 5)
            status = processor.isExternalInputEnabled()
                ? "SUB: EXT BUS ENABLED" : "SUB: EXT BUS DISABLED / SILENT";
        else
            status = isolated ? "SUB: ORIGINAL / PARITY ISOLATION" : "SUB: ORIGINAL FUNCTIONS";
        if (subModeLabel.getText() != status)
            subModeLabel.setText(status, juce::dontSendNotification);
        subModeLabel.setTooltip((isolated
            ? juce::String("Parity isolation forces original SUB controls; the stored mode and legacy settings are retained. Not calibrated. ")
            : juce::String()) + (original
            ? "Original six functions; analog response laws remain provisional. EXT uses an optional mono input bus, disabled by default. Host routing is required; bus enabled does not prove a signal is present. At maximum LEVEL, a peak 1 passband sine gives nominal inverted modulation peak 1. Reconstruction delay: 63.5 host samples. This is software normalization, not physical voltage calibration."
            : "Legacy waveform and rate automation are preserved. Select Original mode in Advanced to use the hardware functions."));
    }

    void syncSourceStatus()
    {
        // These atomics are a UI snapshot, not a render-thread acceptance report.
        // Do not substitute getEngine().getSourceToneInfo() here.
        const float parityRequest = processor.getParamValue(cs80::P_parityMode);
        const float sourceRequest = processor.getParamValue(cs80::P_sourcePerformance);
        const auto policy = cs80::renderPolicy(parityRequest, sourceRequest);
        const juce::String mode = policy == cs80::RenderPolicy::isolation
            ? "COMPARISON ISOLATION / NOT CALIBRATED  |  MIDI OFF: BEND, CC1/11/64/74, MPE  |  PRESSURE, RIBBON + UI PEDAL LIVE  |  MODE CHANGE STOPS NOTES"
            : policy == cs80::RenderPolicy::sourcePerformance
                ? "SOURCE PERFORMANCE / NOT CALIBRATED  |  MIDI BEND + CC1/11/64/74 LIVE; MPE OPTIONAL  |  MODERN OVERLAYS LIVE  |  MODE CHANGE STOPS NOTES"
                : "LEGACY PERFORMANCE / SOURCE MODEL OFF  |  MIDI BEND + CC1/11/64/74 LIVE; MPE OPTIONAL  |  SOURCE MODE: ADVANCED > ABOUT";
        if (modeStatusLabel.getText() != mode)
            modeStatusLabel.setText(mode, juce::dontSendNotification);
        modeStatusLabel.setTooltip(juce::String("Requested mode from stored parameters; audio applies it on a control frame. Source performance uses current source models with MIDI, optional MPE and modern software overlays (macros, FX, pan and bypass). Neither source mode is calibrated or establishes a hardware match. Isolation wins when both switches are ON: it masks MIDI bend, CC1/11/64/74, MPE and modern features. Pressure, ribbon and the on-screen pedal remain live. An effective mode change resets sounding notes and controller/effect history. Changing the source switch while isolation stays ON is masked and does not stop notes. Library recall returns both ranks to PANEL and resets source performance OFF, while retaining isolation. Source output can exceed digital full scale: keep monitoring headroom.")
            + (policy == cs80::RenderPolicy::isolation && sourceRequest >= 0.5f
                ? " Source-performance request is ON but masked by isolation; turning isolation OFF applies that request."
                : ""));

        for (int rank = 0; rank < 2; ++rank)
        {
            const int source = processor.getHardwareSource(rank);
            const auto route = requestedSourceRouteTag(rank, source, policy);
            const juce::String identity = source < 11
                ? juce::String(cs80::hardwareToneName(source, rank)).toUpperCase()
                : source == 13 ? juce::String("MAIN PANEL")
                               : "BANK " + juce::String(rank + (source == 12 ? 3 : 1));
            const juce::String text = juce::String(rank == 0 ? "I REQUEST: " : "II REQUEST: ")
                + identity + " / " + juce::String(route.data());
            auto& label = sourceStatusLabels[(size_t) rank];
            if (label.getText() != text)
                label.setText(text, juce::dontSendNotification);
            label.setTooltip(text + ". Requested APVTS selection, not an audio-thread acknowledgment. "
                "SOURCE-COMPLETE means the source-matrix column has no unresolved cells, not that the instrument is calibrated; device laws remain inferred. "
                "SOURCE-INFERRED also uses explicit hypotheses for unresolved cells. LEGACY STUDY uses the older voicing table. "
                "Source performance and isolation share source classifications but differ in MIDI and modern-overlay policy. "
                "A selected tone or memory owns the 26 programmer controls; main-panel faders are retained for PANEL. Footage and global controls remain live. "
                "Library programs reset both selectors to PANEL and do not select source tabs.");
        }
    }

    void updateOutputMeterLabel()
    {
        const juce::String level = outputMeterDisplay.atFloor()
            ? "<= -120"
            : (outputMeterDisplay.peakDbfs() > 0.0f ? juce::String("+") : juce::String())
                + juce::String(outputMeterDisplay.peakDbfs(), 1);
        juce::String flags;
        if (outputMeterDisplay.overRange()) flags = "OVER-RANGE";
        if (outputMeterDisplay.nonFinite())
            flags += (flags.isEmpty() ? "" : " | ") + juce::String("NON-FINITE");
        if (flags.isEmpty()) flags = "READ-ONLY";
        const auto text = "SAMPLE PEAK " + level + " dBFS\n" + flags;
        if (outputMeterLabel.getText() != text)
            outputMeterLabel.setText(text, juce::dontSendNotification);
        const auto colour = outputMeterDisplay.nonFinite()
            ? juce::Colour(0xffff7166) : outputMeterDisplay.overRange() ? ui::amberBright
                                                                       : juce::Colour(0xffc9bd98);
        if (outputMeterLabel.findColour(juce::Label::textColourId) != colour)
            outputMeterLabel.setColour(juce::Label::textColourId, colour);
        meterBar.setLevel(outputMeterDisplay.atFloor() ? -120.0f : outputMeterDisplay.peakDbfs(),
                          outputMeterDisplay.overRange(), outputMeterDisplay.nonFinite());
    }

    // Curves describe what is sounding: the panel for PANEL, otherwise the
    // selected tone's voicing or the memory bank that owns the 26 controls.
    void syncVoicePlate()
    {
        VoicePlate::Snapshot snap;
        snap.channel = viewChannel;
        snap.source = processor.getHardwareSource(viewChannel);
        for (int local = 0; local < cs80::CP_count; ++local)
            snap.v[(size_t) local] = processor.getParamValue(cs80::chParam(viewChannel, local));
        juce::String name = "PANEL";
        if (snap.source < 11)
        {
            if (toneChannel != viewChannel || toneSource != snap.source)
            {
                toneChannel = viewChannel;
                toneSource = snap.source;
                toneCache = cs80::ParamValues();
                cs80::applyHardwareTone(toneCache, viewChannel, snap.source);
            }
            for (int local = 0; local < cs80::CP_count; ++local)
                if (local != cs80::CP_feet && local != cs80::CP_pan && local != cs80::CP_hpfOn && local != cs80::CP_lpfOn)
                    snap.v[(size_t) local] = toneCache[cs80::chParam(viewChannel, local)];
            name = juce::String(cs80::hardwareToneName(snap.source, viewChannel)).toUpperCase() + " TONE";
        }
        else if (snap.source < 13)
        {
            const int bank = viewChannel + (snap.source == 12 ? 2 : 0);
            for (int local = 0; local < cs80::CP_count; ++local)
                if (Cs80AudioProcessor::isMemoryControl(local))
                    snap.v[(size_t) local] = processor.getMemoryValue(bank, local);
            name = "MEMORY " + juce::String(bank + 1);
        }
        voicePlate.setSnapshot(snap, name);
    }

    void tick()
    {
        processor.serviceKeyboardUI();
        keyboard.repaint();
        syncSubMode();
        syncPresetDisplay();
        syncToneMarks();
        syncSourceStatus();
        outputMeterDisplay.update(processor.consumeOutputMeter(),
                                  juce::Time::getMillisecondCounterHiRes() * 0.001);
        updateOutputMeterLabel();
        syncVoicePlate();
        // Follow the pedal when MIDI CC11 or a restart moved it; never fight a drag.
        if (! expressionPedal.isMouseButtonDown()
            && std::abs(expressionPedal.getValue() - processor.getExpressionValue()) > 1.0e-4f)
            expressionPedal.setValue(processor.getExpressionValue());
        if (memoryProgrammer.isVisible()) memoryProgrammer.refresh();
        // Each live element repaints itself only when its content changes; the
        // chassis is a cached image, so no blanket repaint is needed here.
    }

    // -----------------------------------------------------------------------
    // Painting
    // -----------------------------------------------------------------------
    void panelText(juce::Graphics& g, const juce::String& text, float x, float y,
                   float w, float h, float font = 10.0f,
                   juce::Justification align = juce::Justification::centred,
                   juce::Colour colour = juce::Colour(0xffe6dcbc))
    {
        auto f = displayFont(font, juce::Font::bold);
        f.setExtraKerningFactor(0.03f);
        g.setFont(f);
        const auto area = juce::Rectangle<float>(x, y, w, h).toNearestInt();
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawFittedText(text, area.translated(0, 1), align, 2, 0.7f);
        g.setColour(colour);
        g.drawFittedText(text, area, align, 2, 0.7f);
    }

    // The voice-flow plate: raised machined frame, black glass, gold keylines.
    void paintVoicePlate(juce::Graphics& g)
    {
        const auto r = geo::plate;
        ui::softShadow(g, r, 5.0f, 2.0f, 5.0f, 10.0f, 0.95f);
        g.setGradientFill(art::vgrad(juce::Colour(0xff55514a), juce::Colour(0xff1c1a16), r));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(art::white(0.28f));
        g.drawLine(r.getX() + 5, r.getY() + 1, r.getRight() - 5, r.getY() + 1, 1.0f);
        g.setColour(art::black(0.8f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
        const auto glass = r.reduced(6.0f);
        art::recessTray(g, glass, 3.0f, juce::Colour(0xff191714), juce::Colour(0xff060504), 14.0f, 0.9f);
        g.setGradientFill(juce::ColourGradient(art::white(0.06f), glass.getX(), glass.getY(),
                                               juce::Colours::transparentWhite, glass.getRight() * 0.75f, glass.getBottom() * 0.45f, false));
        g.fillRoundedRectangle(glass.reduced(1.0f), 3.0f);
        ui::etchRect(g, glass.reduced(5.0f), 2.0f, 0.5f);

        // wordmark and caption row
        const float x0 = r.getX(), y0 = r.getY();
        {
            auto f = spacedFont(24.0f, 0.12f);
            g.setGradientFill(art::vgrad(juce::Colour(0xfff4e6a8), juce::Colour(0xffb08d3c), { x0 + 16, y0 + 12, 120, 26 }));
            g.setFont(f);
            g.drawText("CS-80", juce::Rectangle<float>(x0 + 16, y0 + 12, 120, 26), juce::Justification::centredLeft, false);
        }
        art::legend(g, "POLYPHONIC", { x0 + 116, y0 + 13, 100, 9 }, spacedFont(7.0f, 0.22f), juce::Justification::centredLeft, juce::Colour(0xffcdb878));
        art::legend(g, "SYNTHESIZER", { x0 + 116, y0 + 24, 100, 9 }, spacedFont(7.0f, 0.22f), juce::Justification::centredLeft, juce::Colour(0xffcdb878));
        art::legend(g, "CHANNEL VIEW", { x0 + 216, y0 + 15, 70, 9 }, spacedFont(6.5f, 0.2f), juce::Justification::centredRight, juce::Colour(0xff9d9378));
        ui::etchLine(g, x0 + 16, y0 + 51, x0 + 346, y0 + 51, 0.45f);
        art::legend(g, "VOICE SIGNAL FLOW", { x0 + 16, y0 + 40, 150, 11 }, spacedFont(8.0f, 0.14f), juce::Justification::centredLeft, juce::Colour(0xffe6dcbc));
        if (optionsVisible)
        {
            const juce::Rectangle<float> well(x0 + 12, y0 + 116, 336, 188);
            art::recessTray(g, well, 3.0f, juce::Colour(0xff201d18), juce::Colour(0xff0e0c0a), 10.0f, 0.8f);
            art::legend(g, "VOICE SETTINGS  /  SOFTWARE", { well.getX() + 8, well.getY() + 3, 200, 10 }, spacedFont(7.0f, 0.14f), juce::Justification::centredLeft, juce::Colour(0xffcdb878));
            panelText(g, "I", x0 + 17, 232 + 22, 20, 28, 12);
            panelText(g, "II", x0 + 17, 314 + 22, 20, 28, 12);
            ui::etchLine(g, x0 + 20, 306, x0 + 340, 306, 0.35f);
        }
    }

    void paintCheek(juce::Graphics& g, juce::Rectangle<float> r, bool left)
    {
        art::walnut(g, r, left);
    }

    // The raised back edge of the panel: bevelled lip with ventilation louvres.
    void paintTopLip(juce::Graphics& g)
    {
        const juce::Rectangle<float> lip(44, 64, 1512, 32);
        g.setGradientFill(art::vgrad(juce::Colour(0xff3d3a35), juce::Colour(0xff1e1c19), lip));
        g.fillRect(lip);
        g.setColour(art::white(0.20f));
        g.fillRect(44.0f, 64.0f, 1512.0f, 1.2f);
        g.setColour(art::white(0.06f));
        g.fillRect(44.0f, 65.2f, 1512.0f, 3.0f);
        g.setColour(art::black(0.65f));
        g.fillRect(44.0f, 93.0f, 1512.0f, 1.5f);
        g.setGradientFill(art::vgrad(art::black(0.5f), juce::Colours::transparentBlack, { 44, 94.5f, 1512, 9 }));
        g.fillRect(44.0f, 94.5f, 1512.0f, 9.0f);
        for (int i = 0; i < 3; ++i)
            art::louvre(g, { 76.0f + (float) i * 492.0f, 72.0f, 464.0f, 17.0f });
        for (float sx : { 60.0f, 554.0f, 1046.0f, 1540.0f })
            art::slotScrew(g, { sx, 80.5f }, 3.2f, sx * 0.07f);
    }

    // Faders sit in shallow trays so each bank reads as its own module.
    void paintChannelTray(juce::Graphics& g, int channel)
    {
        const float y0 = geo::rowY0[(size_t) channel];
        const juce::Rectangle<float> tray(424.0f, y0 - 6.0f, 1124.0f, 186.0f);
        art::recessTray(g, tray, 5.0f, juce::Colour(0xff1f1d1a), juce::Colour(0xff151311), 12.0f, 0.72f);
        g.setGradientFill(juce::ColourGradient(art::white(0.045f), tray.getX(), tray.getY(),
                                               juce::Colours::transparentWhite, tray.getX() + 500.0f, tray.getBottom(), false));
        g.fillRoundedRectangle(tray.reduced(1.0f), 5.0f);
        // channel numeral on a small engraved boss
        const juce::Rectangle<float> boss(430.0f, y0 + 66.0f, 24.0f, 40.0f);
        art::raisedPlate(g, boss, 3.0f, juce::Colour(0xff2f2c28), juce::Colour(0xff1c1a17), 0.5f);
        panelText(g, channel == 0 ? "I" : "II", boss.getX(), boss.getY() + 6.0f, boss.getWidth(), 28.0f, channel == 0 ? 22.0f : 17.0f);
        art::bracket(g, "VCO", 459, y0 + 10, 235);
        art::bracket(g, "VCF", 706, y0 + 10, 354);
        art::bracket(g, "VCA", 1075, y0 + 10, 272);
        art::bracket(g, "INITIAL", 1365, y0 + 10, 76);
        art::bracket(g, "AFTER", 1445, y0 + 10, 76);
        // faint column separators between functional groups
        for (float x : { 702.0f, 1071.0f, 1360.0f })
        {
            g.setColour(art::black(0.5f));
            g.fillRect(x, y0 + 22.0f, 1.0f, 148.0f);
            g.setColour(art::white(0.06f));
            g.fillRect(x + 1.0f, y0 + 22.0f, 1.0f, 148.0f);
        }
    }

    void paintToneBezel(juce::Graphics& g)
    {
        const auto r = geo::toneBezel;
        ui::softShadow(g, r, 5.0f, 0.0f, 3.0f, 5.0f, 0.8f);
        g.setGradientFill(art::vgrad(juce::Colour(0xff6a655d), juce::Colour(0xff211f1b), r));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(art::white(0.35f));
        g.drawLine(r.getX() + 5, r.getY() + 1, r.getRight() - 5, r.getY() + 1, 1.0f);
        const auto well = r.reduced(4.5f);
        art::recessTray(g, well, 3.0f, juce::Colour(0xff0c0b0a), juce::Colour(0xff030303), 16.0f, 1.0f);
        // dark glossy sheen across the top of the black bezel
        g.setGradientFill(art::vgrad(art::white(0.07f), juce::Colours::transparentWhite, well.withHeight(well.getHeight() * 0.45f)));
        g.fillRoundedRectangle(well.reduced(1.0f).withHeight(well.getHeight() * 0.45f), 3.0f);
        g.setColour(art::black(0.9f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
        panelText(g, "I", 656, 533, 14, 25, 11);
        panelText(g, "II", 654, 578, 18, 25, 11);
    }

    // The deck is a lower plane than the control surface: the surface's front
    // lip throws a shadow on it, and each block is a shallow tray.
    void paintDeck(juce::Graphics& g, const juce::Image& relief, float scale)
    {
        const juce::Rectangle<float> deck(44, geo::faceBottom, 1512, geo::deckBottom - geo::faceBottom);
        g.setGradientFill(art::vgrad(juce::Colour(0xff2b2825), juce::Colour(0xff1b1917), deck));
        g.fillRect(deck);
        art::fillRelief(g, relief, deck, scale, 0.55f, 37.0f, 11.0f);
        g.setGradientFill(art::vgrad(art::black(0.65f), juce::Colours::transparentBlack, { 44, geo::faceBottom, 1512, 18 }));
        g.fillRect(44.0f, geo::faceBottom, 1512.0f, 18.0f);
        // block trays
        for (const auto& tray : { geo::deckLeft, geo::deckRight })
        {
            art::recessTray(g, tray, 5.0f, juce::Colour(0xff1d1b18), juce::Colour(0xff131210), 11.0f, 0.7f);
            ui::etchRect(g, tray.reduced(3.0f), 3.0f, 0.22f);
        }
        art::bracket(g, "PERFORMANCE", 58, 669, 130, 4.0f);
        art::bracket(g, "CHORUS / TREMOLO", 194, 669, 280, 4.0f);
        art::bracket(g, "BRILLIANCE", 1132, 669, 80, 4.0f);
        art::bracket(g, "LEVEL", 1226, 669, 80, 4.0f);
        art::bracket(g, "FOOT", 1318, 669, 38, 4.0f, 8.5f);
        panelText(g, "FOOTSWITCH", 1368, 680, 120, 11, 7.5f);
        panelText(g, "FOOT", 1492, 680, 58, 11, 7.5f);
        panelText(g, "ASSIGN TO", 1368, 724, 120, 11, 7.5f);
        panelText(g, "OUTPUT", 1492, 724, 58, 11, 7.5f);

        // centre: nameplate, legends and the ribbon slot
        const auto np = geo::nameplate;
        art::raisedPlate(g, np, 3.0f, juce::Colour(0xff6a665e), juce::Colour(0xff3a3731), 0.7f);
        art::brushedMetal(g, np.reduced(1.5f), 2.5f, 0.0f, 9);
        g.setColour(art::white(0.55f));
        g.drawRoundedRectangle(np.reduced(1.5f), 2.5f, 0.8f);
        for (float sx : { np.getX() + 8.0f, np.getRight() - 8.0f })
            art::slotScrew(g, { sx, np.getCentreY() }, 3.0f, sx * 0.1f);
        art::stamped(g, "YAMAHA", np.reduced(18.0f, 3.0f), spacedFont(24.0f, 0.34f, 0.95f));

        {
            auto f = spacedFont(22.0f, 0.12f);
            const juce::Rectangle<float> area(500, 670, 130, 36);
            g.setFont(f);
            g.setColour(art::black(0.65f));
            g.drawText("CS-80", area.translated(0, 1.5f), juce::Justification::centred, false);
            g.setGradientFill(art::vgrad(juce::Colour(0xfff4e6a8), juce::Colour(0xffb08d3c), area));
            g.drawText("CS-80", area, juce::Justification::centred, false);
        }
        art::legend(g, "POLYPHONIC", { 970, 672, 130, 14 }, spacedFont(9.5f, 0.2f), juce::Justification::centred, juce::Colour(0xffe6dcbc));
        art::legend(g, "SYNTHESIZER", { 970, 688, 130, 14 }, spacedFont(9.5f, 0.2f), juce::Justification::centred, juce::Colour(0xffe6dcbc));
        const float ruleY = 718.0f;
        ui::etchLine(g, 504, ruleY, 568, ruleY, 0.7f);
        ui::etchLine(g, 1032, ruleY, 1096, ruleY, 0.7f);
        art::legend(g, "RIBBON CONTROLLER", { 568, ruleY - 7, 464, 14 }, spacedFont(8.5f, 0.2f));
        ui::etchLine(g, 504, ruleY, 504, ruleY + 5, 0.7f);
        ui::etchLine(g, 1096, ruleY, 1096, ruleY + 5, 0.7f);
        // printed scale along the ribbon
        for (int i = 0; i <= 24; ++i)
        {
            const float x = 508.0f + (float) i * (584.0f / 24.0f);
            const bool major = i % 6 == 0;
            g.setColour(juce::Colour(0xffd8cfb0).withAlpha(major ? 0.8f : 0.4f));
            g.fillRect(x - 0.4f, 724.5f, 0.9f, major ? 5.0f : 3.0f);
        }
        const auto slot = geo::ribbonRect.expanded(8.0f, 3.0f);
        art::recessTray(g, slot, 5.0f, juce::Colour(0xff101010), juce::Colour(0xff070707), 8.0f, 0.8f);
        for (float sx : { slot.getX() + 6.0f, slot.getRight() - 6.0f })
            art::slotScrew(g, { sx, slot.getCentreY() }, 3.0f, sx);
    }

    void paintToolbar(juce::Graphics& g, const juce::Image& relief, float scale)
    {
        const juce::Rectangle<float> bar(0, 0, 1600, 64);
        juce::ColourGradient base(juce::Colour(0xff3a3732), 0, 0, juce::Colour(0xff0c0b0a), 0, 64, false);
        base.addColour(0.10, juce::Colour(0xff2c2a26));
        base.addColour(0.60, juce::Colour(0xff161513));
        g.setGradientFill(base);
        g.fillRect(bar);
        juce::Random rng(3);
        for (int y = 0; y < 64; ++y)
        {
            g.setColour((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha(0.01f + rng.nextFloat() * 0.035f));
            g.drawHorizontalLine(y, 0.0f, 1600.0f);
        }
        art::fillRelief(g, relief, bar, scale, 0.35f);
        g.setColour(art::white(0.20f));
        g.fillRect(0.0f, 0.0f, 1600.0f, 1.0f);
        g.setColour(juce::Colour(0xffcdb878).withAlpha(0.5f));
        g.fillRect(0.0f, 62.0f, 1600.0f, 1.0f);
        g.setColour(art::black(0.85f));
        g.fillRect(0.0f, 63.0f, 1600.0f, 1.0f);

        // brand mark: a tiny fader bank on a gold-rimmed badge, then the wordmark
        const auto badge = geo::brandBadge;
        art::raisedPlate(g, badge, 5.0f, juce::Colour(0xff6b5a30), juce::Colour(0xff2b2312), 0.5f);
        const auto badgeFace = badge.reduced(2.5f);
        art::recessTray(g, badgeFace, 3.0f, juce::Colour(0xff15130e), juce::Colour(0xff060504), 6.0f, 0.9f);
        const juce::Colour caps[] = { juce::Colour(0xffdcd8c4), juce::Colour(0xff2b9a50), juce::Colour(0xffe0205f) };
        const float capY[] = { 0.62f, 0.30f, 0.50f };
        for (int i = 0; i < 3; ++i)
        {
            const float x = badgeFace.getX() + 5.5f + (float) i * 8.2f;
            g.setColour(art::black(0.9f));
            g.fillRect(x + 1.5f, badgeFace.getY() + 4.0f, 1.4f, badgeFace.getHeight() - 8.0f);
            const float cy = badgeFace.getY() + 4.0f + capY[i] * (badgeFace.getHeight() - 8.0f);
            g.setGradientFill(art::vgrad(caps[i].brighter(0.3f), caps[i].darker(0.25f), { x - 0.5f, cy - 2.5f, 6.0f, 5.0f }));
            g.fillRoundedRectangle(x - 0.5f, cy - 2.5f, 6.0f, 5.0f, 0.8f);
        }
        {
            const juce::Rectangle<float> word(68, 7, 170, 28);
            g.setFont(spacedFont(25.0f, 0.10f));
            g.setColour(art::black(0.8f));
            g.drawText("CS-80", word.translated(0, 1.5f), juce::Justification::centredLeft, false);
            g.setGradientFill(art::vgrad(juce::Colour(0xfff6e8ac), juce::Colour(0xffb08d3c), word));
            g.drawText("CS-80", word, juce::Justification::centredLeft, false);
            art::legend(g, "REFERENCE EDITION", { 69, 32, 172, 10 }, spacedFont(7.0f, 0.22f), juce::Justification::centredLeft, juce::Colour(0xffa99a6c));
        }

        // preset window: dark glass, warm backlight, polished rim
        {
            const auto w = geo::presetWell;
            art::raisedPlate(g, w.expanded(3.0f), 6.0f, juce::Colour(0xff6c675e), juce::Colour(0xff1f1d19), 0.4f);
            const auto glass = w;
            art::recessTray(g, glass, 4.0f, juce::Colour(0xff1a0f07), juce::Colour(0xff090502), 8.0f, 0.9f);
            {
                juce::Graphics::ScopedSaveState state(g);
                juce::Path clip; clip.addRoundedRectangle(glass.reduced(1.0f), 4.0f);
                g.reduceClipRegion(clip);
                g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe39a38).withAlpha(0.30f), glass.getCentreX() - 40.0f, glass.getCentreY() + 4.0f,
                                                       juce::Colours::transparentBlack, glass.getX(), glass.getCentreY(), true));
                g.fillRect(glass);
                g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe39a38).withAlpha(0.12f), glass.getRight() - 30.0f, glass.getCentreY(),
                                                       juce::Colours::transparentBlack, glass.getRight() - 130.0f, glass.getCentreY(), true));
                g.fillRect(glass);
                // faint segmented-display grain
                g.setColour(art::black(0.18f));
                for (float y = glass.getY() + 1.0f; y < glass.getBottom(); y += 2.0f)
                    g.fillRect(glass.getX(), y, glass.getWidth(), 0.7f);
                g.setGradientFill(juce::ColourGradient(art::white(0.14f), glass.getX(), glass.getY(),
                                                       juce::Colours::transparentWhite, glass.getX() + 120.0f, glass.getBottom(), false));
                g.fillRect(glass.withHeight(glass.getHeight() * 0.48f));
            }
        }
        // button cluster housing
        {
            const auto c = geo::cluster;
            art::recessTray(g, c, 5.0f, juce::Colour(0xff0f0e0c), juce::Colour(0xff060605), 8.0f, 0.9f);
            for (float x : { 860.0f, 930.0f, 994.0f })
            {
                g.setColour(art::black(0.8f));
                g.fillRect(x, 12.0f, 1.0f, 28.0f);
                g.setColour(art::white(0.07f));
                g.fillRect(x + 1.0f, 12.0f, 1.0f, 28.0f);
            }
        }
        // output meter window
        {
            const auto w = geo::meterWell;
            art::recessTray(g, w, 4.0f, juce::Colour(0xff0e0f0c), juce::Colour(0xff050504), 8.0f, 0.9f);
            g.setGradientFill(art::vgrad(art::white(0.08f), juce::Colours::transparentWhite, w.withHeight(w.getHeight() * 0.5f)));
            g.fillRoundedRectangle(w.reduced(1.0f).withHeight(w.getHeight() * 0.5f), 3.0f);
            ui::etchLine(g, 1291, 11, 1291, 41, 0.35f);
            // dB scale under the bar
            g.setFont(displayFont(6.5f, juce::Font::bold));
            const std::pair<float, const char*> marks[] = { { -60.0f, "-60" }, { -40.0f, "-40" }, { -20.0f, "-20" }, { -10.0f, "-10" }, { -3.0f, "-3" }, { 0.0f, "0" } };
            for (const auto& m : marks)
            {
                const float x = geo::meterBar.getX() + (m.first + 60.0f) / 60.0f * 147.0f;
                g.setColour(juce::Colour(0xffd8cfb0).withAlpha(0.55f));
                g.fillRect(x - 0.4f, 30.0f, 0.9f, 3.0f);
                g.drawText(m.second, juce::Rectangle<float>(x - 10.0f, 33.0f, 20.0f, 8.0f), juce::Justification::centred, false);
            }
            art::legend(g, "dBFS", { 1448, 33, 28, 8 }, displayFont(6.5f, juce::Font::bold), juce::Justification::centredRight, juce::Colour(0xffa99a6c), 0.8f);
        }
        // PANIC guard
        {
            const auto p = geo::panicBtn.expanded(4.0f);
            art::recessTray(g, p, 5.0f, juce::Colour(0xff160a08), juce::Colour(0xff070302), 7.0f, 0.9f);
            g.setColour(juce::Colour(0xffa8442f).withAlpha(0.45f));
            g.drawRoundedRectangle(p.reduced(1.2f), 4.0f, 0.8f);
        }
        // status ticker
        {
            const auto t = geo::ticker;
            art::recessTray(g, t, 3.0f, juce::Colour(0xff0d0c0a), juce::Colour(0xff060504), 5.0f, 0.9f);
            for (float x : { 268.0f, 514.0f, 744.0f })
            {
                g.setColour(juce::Colour(0xffcdb878).withAlpha(0.22f));
                g.fillRect(x, t.getY() + 3.0f, 1.0f, t.getHeight() - 6.0f);
            }
            for (float x : { 26.0f, 272.0f, 518.0f })
            {
                g.setColour(juce::Colour(0xff4aa068));
                g.fillEllipse(x, t.getCentreY() - 1.5f, 3.0f, 3.0f);
            }
        }
        for (float x : { 12.0f, 1588.0f })
            for (float y : { 12.0f, 52.0f })
                art::slotScrew(g, { x, y }, 3.4f, x + y);
    }

    void paintChassis(juce::Graphics& g)
    {
        const float sx = (float) getWidth() / kDesignWidth;
        const float sy = (float) getHeight() / kDesignHeight;
        const float scale = sx;
        g.saveState();
        g.addTransform(juce::AffineTransform::scale(sx, sy));
        g.fillAll(juce::Colour(0xff070605));

        static const juce::Image crinkle = art::makeRelief(192, 5, 1.3f);
        static const juce::Image fineGrain = art::makeRelief(128, 9, 0.85f);

        // ---- wooden cheeks ---------------------------------------------------
        paintCheek(g, { 0, 68, 44, 932 }, true);
        paintCheek(g, { 1556, 68, 44, 932 }, false);

        // ---- control surface: crinkle-finish charcoal -----------------------
        const juce::Rectangle<float> face(44, geo::faceTop, 1512, geo::faceBottom - geo::faceTop);
        {
            juce::ColourGradient base(juce::Colour(0xff34312d), 44, geo::faceTop, juce::Colour(0xff211f1c), 44, geo::faceBottom, false);
            base.addColour(0.35, juce::Colour(0xff2d2a27));
            g.setGradientFill(base);
            g.fillRect(face);
            art::fillRelief(g, crinkle, face, scale, 0.34f);
            art::fillRelief(g, fineGrain, face, scale, 0.14f, 61.0f, 23.0f);
            // soft overhead light pooling upper left, falling away to the corners
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffe9c0).withAlpha(0.085f), 620.0f, 260.0f,
                                                   juce::Colours::transparentBlack, 1300.0f, 700.0f, true));
            g.fillRect(face);
            g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, 800.0f, 380.0f,
                                                   art::black(0.34f), 1560.0f, 680.0f, true));
            g.fillRect(face);
            // shadow of each cheek on the panel
            g.setGradientFill(hgradShadow(44.0f, 80.0f, 0.55f));
            g.fillRect(44.0f, geo::faceTop, 36.0f, face.getHeight());
            g.setGradientFill(hgradShadow(1556.0f, 1520.0f, 0.55f));
            g.fillRect(1520.0f, geo::faceTop, 36.0f, face.getHeight());
        }
        paintTopLip(g);

        // ---- channel trays ---------------------------------------------------
        paintChannelTray(g, 0);
        paintChannelTray(g, 1);
        paintVoicePlate(g);

        // ---- lower plate: ring / sub / feet | tone selector | global --------
        {
            const auto lp = geo::lowerPlate;
            art::raisedPlate(g, lp, 5.0f, juce::Colour(0xff35322e), juce::Colour(0xff252320), 1.0f);
            art::fillRelief(g, crinkle, lp, scale, 0.20f, 17.0f, 5.0f);
            g.setGradientFill(juce::ColourGradient(art::white(0.05f), lp.getX(), lp.getY(),
                                                   juce::Colours::transparentWhite, lp.getX() + 700.0f, lp.getBottom(), false));
            g.fillRoundedRectangle(lp.reduced(1.0f), 5.0f);
            ui::etchRect(g, lp.reduced(4.0f), 3.0f, 0.5f);
            art::bracket(g, "RING MODULATOR", 158, 499, 200);
            art::bracket(g, "SUB OSCILLATOR", 370, 499, 205);
            art::bracket(g, "FEET", 580, 499, 80);
            art::bracket(g, "TONE SELECTOR", 675, 505, 514);
            art::bracket(g, "GLOBAL / TOUCH RESPONSE", 1208, 499, 340);
            paintToneBezel(g);
        }

        // front lip of the control surface over the deck
        g.setGradientFill(art::vgrad(juce::Colour(0xff4a4742), juce::Colour(0xff1c1a17), { 44, 650, 1512, 10 }));
        g.fillRect(44.0f, 652.0f, 1512.0f, 8.0f);
        g.setColour(art::white(0.22f));
        g.fillRect(44.0f, 652.0f, 1512.0f, 1.0f);
        g.setColour(art::black(0.9f));
        g.fillRect(44.0f, 659.0f, 1512.0f, 1.5f);

        paintDeck(g, fineGrain, scale);

        // ---- keybed base (the Keybed component paints over this) -----------
        g.setColour(juce::Colour(0xff0a0908));
        g.fillRect(geo::keybed);

        // ---- screws on the control face -----------------------------------
        for (float x : { 55.0f, 1545.0f })
            for (float y : { 645.0f })
                art::slotScrew(g, { x, y }, 4.2f, (x + y) * 0.7f);

        // ---- toolbar over everything, casting a shadow onto the panel ------
        g.setGradientFill(art::vgrad(art::black(0.6f), juce::Colours::transparentBlack, { 44, 64, 1512, 20 }));
        g.fillRect(44.0f, 64.0f, 1512.0f, 20.0f);
        paintToolbar(g, fineGrain, scale);
        g.restoreState();
    }

    static juce::ColourGradient hgradShadow(float from, float to, float alpha)
    {
        return juce::ColourGradient(juce::Colours::black.withAlpha(alpha), from, 0.0f,
                                    juce::Colours::transparentBlack, to, 0.0f, false);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0a0908));
    }

    void resized() override
    {
        const float sx = (float) getWidth() / kDesignWidth;
        const float sy = (float) getHeight() / kDesignHeight;
        auto r = [sx, sy](float x, float y, float w, float h) {
            return juce::Rectangle<int>(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                                        juce::roundToInt(w * sx), juce::roundToInt(h * sy));
        };
        auto rr = [&r](const juce::Rectangle<float>& b) { return r(b.getX(), b.getY(), b.getWidth(), b.getHeight()); };

        backdrop.setBounds(getLocalBounds());
        voicePlate.setBounds(rr(geo::plate));
        for (const auto& item : placements)
            item.component->setBounds(r(item.bounds.getX(), item.bounds.getY(),
                                        item.bounds.getWidth(), item.bounds.getHeight()));

        prevButton.setBounds(rr(geo::prevBtn));
        nextButton.setBounds(rr(geo::nextBtn));
        presetTagLabel.setBounds(rr(geo::presetTag));
        presetTagLabel.setFont(spacedFont(9.0f * sy, 0.16f));
        presetTagLabel.setMinimumPixels(7.5f);
        presetIndexLabel.setBounds(rr(geo::presetIndex));
        presetIndexLabel.setFont(spacedFont(9.0f * sy, 0.10f));
        presetIndexLabel.setMinimumPixels(7.5f);
        presetLabel.setBounds(rr(geo::presetName));
        presetLabel.setFont(spacedFont(21.0f * sy, 0.04f));
        presetLabel.setMinimumPixels(12.0f);
        presetLabel.setShadow(0.7f);
        browseButton.setBounds(rr(geo::libraryBtn));
        saveButton.setBounds(rr(geo::saveBtn));
        initButton.setBounds(rr(geo::initBtn));
        advancedButton.setBounds(rr(geo::advancedBtn));
        panicButton.setBounds(rr(geo::panicBtn));
        outputMeterLabel.setBounds(rr(geo::meterText));
        outputMeterLabel.setFont(displayFont(9.0f * sy, juce::Font::bold));
        outputMeterLabel.setMinimumPixels(7.5f);
        outputMeterLabel.setLines(2);
        meterBar.setBounds(rr(geo::meterBar));
        for (int rank = 0; rank < 2; ++rank)
        {
            auto& label = sourceStatusLabels[(size_t) rank];
            label.setBounds(rr(rank == 0 ? geo::rankI : geo::rankII));
            label.setFont(displayFont(9.5f * sy, juce::Font::bold));
            label.setMinimumPixels(9.0f);
        }
        subModeLabel.setBounds(rr(geo::subStatus));
        subModeLabel.setFont(displayFont(9.5f * sy, juce::Font::bold));
        subModeLabel.setMinimumPixels(9.0f);
        modeStatusLabel.setBounds(rr(geo::modeStatus));
        modeStatusLabel.setFont(displayFont(9.5f * sy, juce::Font::bold));
        modeStatusLabel.setMinimumPixels(9.0f);

        ribbon.setBounds(rr(geo::ribbonRect));
        pitchWheel.setBounds(r(318.0f, 230.0f, 30.0f, 150.0f));
        modWheel.setBounds(r(356.0f, 230.0f, 30.0f, 150.0f));
        keybed.setBounds(rr(geo::keybed));

        scrim.setBounds(rr(geo::overlay));
        shade.setBounds(rr(geo::overlay));
        advanced.setBounds(rr(geo::overlay));
        browser.setBounds(rr(geo::overlay));
        memoryProgrammer.setBounds(rr(geo::memoryLid));
    }

    Cs80AudioProcessor& processor;
    Backdrop backdrop;
    VoicePlate voicePlate;
    ui::LookAndFeel lookAndFeel;
    PanelTooltipWindow tooltips { this, 600 };

    KeyButton prevButton { "<", KeyButton::Style::previous };
    KeyButton nextButton { ">", KeyButton::Style::next };
    KeyButton browseButton { "LIBRARY" };
    KeyButton saveButton { "SAVE" };
    KeyButton initButton { "INIT" };
    KeyButton advancedButton { "ADVANCED" };
    KeyButton panicButton { "PANIC", KeyButton::Style::alert };
    PanelLabel presetLabel;
    PanelLabel presetTagLabel;
    PanelLabel presetIndexLabel;
    PanelLabel subModeLabel;
    PanelLabel modeStatusLabel;
    PanelLabel outputMeterLabel;
    MeterBar meterBar;
    ui::OutputMeterDisplay outputMeterDisplay;
    std::array<PanelLabel, 2> sourceStatusLabels;
    ui::ParamKnob* subFunction = nullptr;
    ui::ParamKnob* subSpeed = nullptr;

    std::vector<std::unique_ptr<ui::ParamKnob>> knobs;
    std::vector<std::unique_ptr<ui::LedButton>> leds;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::array<std::array<std::unique_ptr<juce::TextButton>, 14>, 2> toneButtons;
    std::vector<Placement> placements;
    KeyButton voiceOptions { "VOICE SETTINGS", KeyButton::Style::plate };
    KeyButton memoryLid { "OPEN MEMORY LID", KeyButton::Style::plate };
    std::array<KeyButton, 2> viewButtons { KeyButton("I", KeyButton::Style::plate), KeyButton("II", KeyButton::Style::plate) };
    std::vector<juce::Component*> optionComponents;
    bool optionsVisible = false;
    int viewChannel = 0;
    int toneChannel = -1, toneSource = -1;
    cs80::ParamValues toneCache;

    cs80::ui::Keybed keybed;
    ui::HardwareKeyboard& keyboard = keybed.keys;
    ui::RibbonStrip ribbon;
    ui::PerformanceWheel pitchWheel;
    ui::PerformanceWheel modWheel;
    ui::PerformanceWheel expressionPedal { "PEDAL", false };

    OverlayScrim scrim;
    ui::AdvancedPanel advanced;
    ui::PresetBrowser browser;
    OverlayShade shade;
    MemoryProgrammer memoryProgrammer;

    FrameTimer frames;
    juce::Component::SafePointer<juce::Component> animTarget;
    bool animOpening = true;
    double animStart = 0.0;
    static constexpr double animDurationMs = 140.0;

    bool advancedVisible = false;
    bool browserVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Impl)
};

Cs80AudioProcessorEditor::Cs80AudioProcessorEditor(Cs80AudioProcessor& p)
    : AudioProcessorEditor(&p)
{
    setOpaque(true);
    impl = std::make_unique<Impl>(p);
    addAndMakeVisible(*impl);

    setResizable(true, true);
    setResizeLimits(1120, 700, 2400, 1500);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(kDesignWidth / kDesignHeight);

    // The host has not attached/repositioned this editor yet. The mouse display
    // is the best available opening-display hint, with primary as a fallback.
    // This is an initial suggestion only: later host/manual setSize calls and
    // explicit screenshot dimensions remain authoritative, with no auto-resize.
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* display = displays.getDisplayForPoint(juce::Desktop::getMousePosition());
    if (display == nullptr)
        display = displays.getPrimaryDisplay();
    const auto initialSize = display != nullptr
        ? initialEditorSizeForArea(display->userArea.getWidth(), display->userArea.getHeight())
        : initialEditorSizeForArea(0, 0);
    setSize(initialSize.width, initialSize.height);

    startTimerHz(8);
}

Cs80AudioProcessorEditor::~Cs80AudioProcessorEditor()
{
    stopTimer();
    impl.reset();
}

void Cs80AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui::charcoal);
}

void Cs80AudioProcessorEditor::resized()
{
    if (impl != nullptr)
        impl->setBounds(getLocalBounds());
}

void Cs80AudioProcessorEditor::timerCallback()
{
    if (impl != nullptr)
        impl->tick();
}

void Cs80AudioProcessorEditor::showAdvancedPanel(bool shouldShow)
{
    if (impl != nullptr)
        impl->showAdvanced(shouldShow);
}

void Cs80AudioProcessorEditor::showAdvancedPage(int pageIndex)
{
    if (impl != nullptr)
        impl->advanced.showPage(pageIndex);
}

void Cs80AudioProcessorEditor::showPresetBrowser(bool shouldShow)
{
    if (impl != nullptr)
        impl->showBrowser(shouldShow);
}

bool Cs80AudioProcessorEditor::isAdvancedVisible() const
{
    return impl != nullptr && impl->advancedVisible;
}
