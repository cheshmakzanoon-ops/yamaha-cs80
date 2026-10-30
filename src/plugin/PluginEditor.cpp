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

// The instrument body (toolbar, panel, deck) sits between the wooden cheeks.
const float bodyLeft = 44.0f, bodyRight = 1556.0f;

// Toolbar y 0-64
const Rf brandBadge   { 54, 8, 34, 34 };
const Rf brandText    { 96, 10, 150, 30 };
const Rf prevBtn      { 252, 9, 34, 34 };
const Rf presetWell   { 292, 6, 414, 40 };
const Rf presetNumber { 302, 10, 52, 30 };
const Rf presetTag    { 372, 9, 228, 12 };
const Rf presetIndex  { 600, 9, 72, 12 };
const Rf presetName   { 372, 21, 300, 24 };
const Rf presetChevron{ 678, 18, 18, 12 };
const Rf nextBtn      { 712, 9, 34, 34 };
const Rf cluster      { 762, 6, 350, 40 };
const Rf libraryBtn   { 768, 10, 90, 32 };
const Rf saveBtn      { 862, 10, 66, 32 };
const Rf initBtn      { 932, 10, 60, 32 };
const Rf advancedBtn  { 996, 10, 110, 32 };
const Rf lampWell     { 1122, 6, 128, 40 };
const Rf meterWell    { 1258, 6, 196, 40 };
const Rf meterText    { 1262, 17, 52, 20 };
const Rf meterBar     { 1318, 11, 128, 14 };
const Rf panicBtn     { 1466, 8, 76, 36 };
const Rf ticker       { 54, 48, 1492, 14 };
const Rf rankI        { 64, 48, 212, 14 };
const Rf rankII       { 290, 48, 212, 14 };
const Rf voiceStatus  { 516, 48, 150, 14 };
const Rf midiStatus   { 680, 48, 110, 14 };
const Rf subStatus    { 804, 48, 190, 14 };
const Rf modeStatus   { 1008, 48, 530, 14 };
const std::array<float, 5> tickerRules { 283.0f, 509.0f, 673.0f, 797.0f, 1001.0f };

// Control surface y 64-660
const float faceTop = 64.0f, faceBottom = 660.0f, deckBottom = 772.0f;
const Rf plate        { 54, 98, 360, 378 };
const Rf memoryLidKey { 64, 444, 166, 24 };
const Rf voiceKey     { 238, 444, 166, 24 };
const Rf viewKeyI     { 340, 106, 26, 24 };
const Rf viewKeyII    { 368, 106, 30, 24 };
const std::array<float, 2> rowY0 { 104.0f, 296.0f };
const Rf lowerPlate   { 48, 484, 1504, 166 };
const Rf toneBezel    { 660, 520, 537, 102 };
const float toneX0 = 688.0f, tonePitch = 36.0f, toneW = 33.0f;

// Performance deck y 660-772 (outer edges line up with the key area at x62..1538)
const Rf deckLeft     { 62, 664, 414, 104 };
const Rf deckRight    { 1130, 664, 408, 104 };
const Rf nameplate    { 640, 668, 320, 40 };
const Rf ribbonRect   { 504, 732, 592, 28 };
const Rf keybed       { 44, 772, 1512, 228 };
const Rf overlay      { 0, 64, 1600, 936 };
// The lid component also carries its hinge band above and its cast shadow below.
const Rf memoryLid    { 48, 90, 1504, 590 };
const float lidTop = 10.0f, lidHeight = 552.0f;
} // namespace geo

// ---------------------------------------------------------------------------
// Chassis drawing helpers (local; shared helpers live in UiComponents.h)
// ---------------------------------------------------------------------------
namespace art
{
using Rf = juce::Rectangle<float>;
using juce::Colour;

inline Colour black(float a) { return juce::Colours::black.withAlpha(a); }
// Matte instrument plate colour (voice plate, memory lid, status strip): also the halo under its type.
inline Colour plateColour() { return Colour(0xff262421); }
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
        g.setColour(Colour(0xff6b6453).withAlpha(0.95f));
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

// One fine crinkle finish for the whole instrument body. The tile is opaque
// (base colour modulated per pixel), so lightness and grain are set directly:
// lum ~44 with a ~8 level standard deviation at one texel per device pixel.
juce::Image makeCrinkleTile(int n, int seed, Colour base, float relativeStd)
{
    juce::Random rng(seed);
    const size_t count = (size_t) (n * n);
    std::vector<float> a(count), b(count), z(count);
    for (auto& v : a) v = rng.nextFloat() - 0.5f;
    for (auto& v : b) v = rng.nextFloat() - 0.5f;
    auto at = [n](const std::vector<float>& v, int x, int y)
    { return v[(size_t) (((y % n + n) % n) * n + ((x % n + n) % n))]; };
    double sum = 0.0, sumSq = 0.0;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            // Raised facets: a short embossed noise plus a softer 2 x 2 undulation.
            const float soft = 0.25f * (at(b, x, y) + at(b, x + 1, y) + at(b, x, y + 1) + at(b, x + 1, y + 1));
            const float emboss = at(a, x, y) - 0.7f * at(a, x + 1, y + 1) - 0.3f * at(a, x - 1, y);
            const float v = 0.5f * emboss + 1.5f * soft;
            z[(size_t) (y * n + x)] = v;
            sum += v;
            sumSq += (double) v * v;
        }
    const double mean = sum / (double) count;
    const double sd = std::sqrt(juce::jmax(1.0e-9, sumSq / (double) count - mean * mean));
    juce::Image image(juce::Image::RGB, n, n, false);
    juce::Image::BitmapData data(image, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            float m = 1.0f + relativeStd * (float) ((z[(size_t) (y * n + x)] - mean) / sd);
            const float speck = rng.nextFloat();
            if (speck < 0.02f) m *= 0.78f;        // fissure between facets
            else if (speck > 0.992f) m *= 1.22f;  // catch-light on a ridge
            m = juce::jlimit(0.45f, 1.7f, m);
            data.setPixelColour(x, y, Colour::fromFloatRGBA(base.getFloatRed() * m, base.getFloatGreen() * m,
                                                            base.getFloatBlue() * m, 1.0f));
        }
    return image;
}

void fillTile(juce::Graphics& g, const juce::Image& tile, Rf area, float deviceScale, float shiftX = 0.0f, float shiftY = 0.0f)
{
    fillRelief(g, tile, area, deviceScale, 1.0f, shiftX, shiftY);
}

// Printed text with a plate-coloured halo so lines and dots never cut through it.
void haloText(juce::Graphics& g, const juce::String& text, Rf area, const juce::Font& font,
              juce::Justification just, Colour ink, Colour halo)
{
    g.setFont(font);
    g.setColour(halo);
    for (auto d : { juce::Point<float>(-1.0f, 0.0f), juce::Point<float>(1.0f, 0.0f), juce::Point<float>(0.0f, -1.0f),
                    juce::Point<float>(0.0f, 1.0f), juce::Point<float>(-0.8f, -0.8f), juce::Point<float>(0.8f, 0.8f) })
        g.drawText(text, area.translated(d.x, d.y), just, false);
    g.setColour(ink);
    g.drawText(text, area, just, false);
}

// A small toothed wheel for the ADVANCED key.
void gearGlyph(juce::Graphics& g, juce::Point<float> c, float radius, Colour ink)
{
    juce::Path gear;
    const int teeth = 8;
    for (int i = 0; i < teeth; ++i)
    {
        const float a0 = juce::MathConstants<float>::twoPi * (float) i / (float) teeth;
        const float step = juce::MathConstants<float>::twoPi / (float) teeth;
        const float inner = radius * 0.74f;
        const float pts[][2] = { { a0 - step * 0.22f, inner }, { a0 - step * 0.14f, radius }, { a0 + step * 0.14f, radius },
                                 { a0 + step * 0.22f, inner } };
        for (int k = 0; k < 4; ++k)
        {
            const float x = c.x + std::cos(pts[k][0]) * pts[k][1], y = c.y + std::sin(pts[k][0]) * pts[k][1];
            if (i == 0 && k == 0) gear.startNewSubPath(x, y); else gear.lineTo(x, y);
        }
    }
    gear.closeSubPath();
    gear.addEllipse(c.x - radius * 0.32f, c.y - radius * 0.32f, radius * 0.64f, radius * 0.64f);
    gear.setUsingNonZeroWinding(false);
    g.setColour(black(0.55f));
    g.fillPath(gear, juce::AffineTransform::translation(0.0f, 0.9f));
    g.setColour(ink);
    g.fillPath(gear);
}

// Dark walnut cheek that runs the full height of the case: figure from the shared
// grain helper pulled down to a deep brown, a broad diagonal varnish sheen, a lit
// bevel on the inner edge, a rounded end-grain cap and a cast shadow on the panel.
void walnutCheek(juce::Graphics& g, Rf r, bool left, float shadowBottom)
{
    // Shadow thrown on the panel (and, below the panel, left to the keybed's own frame).
    {
        const float w = 14.0f;
        const float x0 = left ? r.getRight() : r.getX() - w;
        g.setGradientFill(juce::ColourGradient(black(0.62f), left ? r.getRight() : r.getX(), 0.0f,
                                               juce::Colours::transparentBlack, left ? r.getRight() + w : r.getX() - w, 0.0f, false));
        g.fillRect(x0, r.getY(), w, shadowBottom - r.getY());
    }
    juce::Path shape;
    shape.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), 15.0f, 15.0f, true, true, false, false);
    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(shape);

    g.setColour(Colour(0xff3a2014));
    g.fillRect(r);
    ui::drawWalnutGrain(g, r, true, left ? 31 : 47, false);
    // Pull the helper's figure down to deep walnut without losing its grain.
    g.setColour(Colour(0xff1f0e07).withAlpha(0.40f));
    g.fillRect(r);
    // Bullnose: the face turns away at both long edges.
    juce::ColourGradient round(black(0.62f), r.getX(), 0.0f, black(0.62f), r.getRight(), 0.0f, false);
    round.addColour(0.22, juce::Colours::transparentBlack);
    round.addColour(0.62, juce::Colours::transparentBlack);
    g.setGradientFill(round);
    g.fillRect(r);

    juce::Random rng(left ? 7 : 19);
    // Pores: short dark dashes with a faint highlight beside them.
    for (int i = 0; i < 360; ++i)
    {
        const float x = r.getX() + 3.0f + rng.nextFloat() * (r.getWidth() - 6.0f);
        const float y = r.getY() + 16.0f + rng.nextFloat() * (r.getHeight() - 20.0f);
        const float len = 2.0f + rng.nextFloat() * 9.0f;
        g.setColour(black(0.16f + rng.nextFloat() * 0.22f));
        g.fillRect(x, y, 0.7f, len);
        g.setColour(Colour(0xffd09a6a).withAlpha(0.05f));
        g.fillRect(x + 0.8f, y, 0.6f, len * 0.8f);
    }
    // Long grain lines that wander slightly, dark and pale, with the odd heavier figure line.
    for (int i = 0; i < 70; ++i)
    {
        const float x = r.getX() + 2.0f + rng.nextFloat() * (r.getWidth() - 4.0f);
        float y = r.getY() + 18.0f + rng.nextFloat() * (r.getHeight() * 0.8f);
        const float len = 60.0f + rng.nextFloat() * 380.0f;
        const float phase = rng.nextFloat() * 6.28f, amp = 0.4f + rng.nextFloat() * 1.6f, freq = 0.012f + rng.nextFloat() * 0.02f;
        juce::Path streak;
        streak.startNewSubPath(x, y);
        for (float d = 4.0f; d <= len; d += 4.0f)
            streak.lineTo(x + std::sin(phase + d * freq) * amp, y + d);
        const bool dark = rng.nextFloat() < 0.7f;
        g.setColour((dark ? Colour(0xff0e0503) : Colour(0xff9a6a42)).withAlpha(dark ? 0.16f + rng.nextFloat() * 0.26f : 0.05f + rng.nextFloat() * 0.07f));
        g.strokePath(streak, juce::PathStrokeType(dark && rng.nextFloat() < 0.15f ? 1.6f : 0.7f));
    }
    // Broad diagonal varnish sheen with a second, fainter reflection.
    {
        const float h = r.getHeight();
        juce::ColourGradient sheen(juce::Colours::transparentWhite, r.getX(), r.getY() + h * 0.10f,
                                   juce::Colours::transparentWhite, r.getRight(), r.getY() + h * 0.38f, false);
        sheen.addColour(0.5, white(0.20f));
        g.setGradientFill(sheen);
        g.fillRect(r);
        juce::ColourGradient sheen2(juce::Colours::transparentWhite, r.getX(), r.getY() + h * 0.50f,
                                    juce::Colours::transparentWhite, r.getRight(), r.getY() + h * 0.78f, false);
        sheen2.addColour(0.5, white(0.10f));
        g.setGradientFill(sheen2);
        g.fillRect(r);
    }
    // Inner bevel: lit edge facing the light, then a dark joint against the panel.
    const float innerX = left ? r.getRight() - 4.0f : r.getX();
    g.setGradientFill(hgrad(white(left ? 0.0f : 0.24f), white(left ? 0.24f : 0.0f), { innerX, r.getY(), 4.0f, r.getHeight() }));
    g.fillRect(innerX, r.getY(), 4.0f, r.getHeight());
    g.setColour(black(0.9f));
    g.fillRect(left ? r.getRight() - 1.0f : r.getX(), r.getY(), 1.0f, r.getHeight());
    // Outer edge falls away into shadow.
    const float outerX = left ? r.getX() : r.getRight() - 5.0f;
    g.setGradientFill(hgrad(black(left ? 0.6f : 0.0f), black(left ? 0.0f : 0.6f), { outerX, r.getY(), 5.0f, r.getHeight() }));
    g.fillRect(outerX, r.getY(), 5.0f, r.getHeight());
    // Lower end sinks into darkness.
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, 0.0f, r.getBottom() - 220.0f,
                                           black(0.5f), 0.0f, r.getBottom(), false));
    g.fillRect(r);

    // End-grain cap: tight, pale rings across the top 18 px, rounded like the case end.
    {
        const Rf cap(r.getX(), r.getY(), r.getWidth(), 18.0f);
        g.setGradientFill(vgrad(Colour(0xff8a5a36), Colour(0xff4a2916), cap));
        g.fillRect(cap);
        for (float y = cap.getY() + 1.0f; y < cap.getBottom(); y += 1.6f)
        {
            g.setColour((rng.nextBool() ? Colour(0xff2a140a) : Colour(0xffb88458)).withAlpha(0.10f + rng.nextFloat() * 0.25f));
            g.drawHorizontalLine((int) y, cap.getX(), cap.getRight());
        }
        g.setGradientFill(vgrad(white(0.30f), juce::Colours::transparentWhite, cap.withHeight(8.0f)));
        g.fillRect(cap.withHeight(8.0f));
        g.setColour(black(0.75f));
        g.fillRect(cap.getX(), cap.getBottom(), cap.getWidth(), 1.2f);
        g.setGradientFill(vgrad(juce::Colours::transparentBlack, black(0.35f), { cap.getX(), cap.getBottom() + 1.0f, cap.getWidth(), 6.0f }));
        g.fillRect(cap.getX(), cap.getBottom() + 1.0f, cap.getWidth(), 6.0f);
    }
    g.restoreState();
    g.saveState();
    g.setColour(black(0.8f));
    g.strokePath(shape, juce::PathStrokeType(1.0f));
}

// Matte or brushed instrument plate: flat colour with a faint grain and a hairline bevel,
// no gradient glow. Ink drawn on it stays flat.
void matteSheet(juce::Graphics& g, Rf r, float radius, Colour base, int seed)
{
    g.setColour(base);
    g.fillRoundedRectangle(r, radius);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(r, radius);
        g.reduceClipRegion(clip);
        juce::Random rng(seed);
        for (float y = r.getY() + 0.5f; y < r.getBottom(); y += 1.0f)
        {
            const bool lit = rng.nextBool();
            g.setColour((lit ? juce::Colours::white : juce::Colours::black).withAlpha(0.008f + rng.nextFloat() * 0.022f));
            g.drawHorizontalLine((int) y, r.getX(), r.getRight());
        }
        g.setGradientFill(vgrad(white(0.05f), juce::Colours::transparentWhite, r.withHeight(r.getHeight() * 0.35f)));
        g.fillRect(r);
    }
    g.setColour(white(0.20f));
    g.drawLine(r.getX() + radius, r.getY() + 0.8f, r.getRight() - radius, r.getY() + 0.8f, 1.0f);
    g.setColour(white(0.07f));
    g.drawLine(r.getX() + 0.8f, r.getY() + radius, r.getX() + 0.8f, r.getBottom() - radius, 1.0f);
    g.setColour(black(0.55f));
    g.drawLine(r.getX() + radius, r.getBottom() - 0.8f, r.getRight() - radius, r.getBottom() - 0.8f, 1.0f);
    g.setColour(black(0.85f));
    g.drawRoundedRectangle(r, radius, 1.0f);
}

// YAMAHA-style engraved lettering on metal: the cut is darker, lit on its lower right lip,
// shadowed on its upper left wall.
void engraved(juce::Graphics& g, const juce::String& text, Rf area, const juce::Font& font,
              juce::Justification just = juce::Justification::centred)
{
    g.setFont(font);
    g.setColour(white(0.80f));
    g.drawText(text, area.translated(1.1f, 1.1f), just, false);
    g.setColour(black(0.55f));
    g.drawText(text, area.translated(-0.9f, -0.9f), just, false);
    g.setGradientFill(vgrad(Colour(0xff2a2823), Colour(0xff0e0d0b), area));
    g.drawText(text, area, just, false);
}
} // namespace art

// The editor's resize grip, tinted to sit on walnut instead of drawing white diagonals.
class GripLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawCornerResizer(juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging) override
    {
        const float s = (float) juce::jmin(w, h);
        const float thickness = juce::jmax(1.0f, s * 0.09f);
        const auto ink = juce::Colour(0xffd9a85a).withAlpha(isMouseDragging ? 0.95f : isMouseOver ? 0.8f : 0.5f);
        for (float t : { 0.35f, 0.62f, 0.88f })
        {
            const juce::Line<float> line((float) w - s * t, (float) h, (float) w, (float) h - s * t);
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.drawLine(line.getStartX(), line.getStartY() + 1.0f, line.getEndX(), line.getEndY() + 1.0f, thickness);
            g.setColour(ink);
            g.drawLine(line, thickness);
        }
    }
};

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
    enum class Style { key, previous, next, alert, plate, accent, quiet, rocker };
    enum class Glyph { none, gear };
    explicit KeyButton(const juce::String& text, Style s = Style::key) : juce::TextButton(text), style(s) {}
    void setGlyph(Glyph value) { glyph = value; repaint(); }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        if (style == Style::rocker) { paintRocker(g, hovered, down); return; }
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        const float rad = juce::jmin(4.0f, r.getHeight() * 0.2f);
        const bool on = getToggleState();
        const bool accent = style == Style::accent;
        if (accent)
            ui::drawAccentPlate(g, getLocalBounds().toFloat(), down, hovered);
        else
        {
            juce::Colour top, bottom;
            switch (style)
            {
                case Style::alert: top = juce::Colour(0xff9a3a2c); bottom = juce::Colour(0xff4a140e); break;
                case Style::plate: top = juce::Colour(0xff3d3a35); bottom = juce::Colour(0xff1b1917); break;
                case Style::quiet: top = juce::Colour(0xff34322e); bottom = juce::Colour(0xff201e1b); break;
                default:           top = juce::Colour(0xff625d54); bottom = juce::Colour(0xff2c2a26); break;
            }
            if (on) { top = top.interpolatedWith(juce::Colour(0xffa9772b), 0.46f); bottom = bottom.interpolatedWith(juce::Colour(0xff5a3a10), 0.5f); }
            if (hovered) { top = top.brighter(0.14f); bottom = bottom.brighter(0.10f); }
            if (down) { std::swap(top, bottom); r = r.translated(0.0f, 1.0f); }
            else if (style != Style::quiet) ui::softShadow(g, r, rad, 0.0f, 1.8f, 2.4f, 0.7f);
            g.setGradientFill(art::vgrad(top, bottom, r));
            g.fillRoundedRectangle(r, rad);
            g.setColour(art::white(down ? 0.05f : (style == Style::quiet ? 0.10f : 0.24f)));
            g.drawLine(r.getX() + rad, r.getY() + 1.0f, r.getRight() - rad, r.getY() + 1.0f, 1.0f);
            g.setColour(art::black(0.45f));
            g.drawLine(r.getX() + rad, r.getBottom() - 1.0f, r.getRight() - rad, r.getBottom() - 1.0f, 1.0f);
            g.setColour(on ? juce::Colour(0xffe6b04a).withAlpha(0.9f) : art::black(0.85f));
            g.drawRoundedRectangle(r, rad, on ? 1.2f : 1.0f);
        }

        const auto ink = accent ? juce::Colour(0xff2b1c06)
                        : style == Style::alert ? juce::Colour(0xffffc8b8)
                        : style == Style::plate ? juce::Colour(0xffd9c88e)
                        : style == Style::quiet ? juce::Colour(0xffb5ab92)
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
            const float size = juce::jlimit(8.0f, 15.0f, r.getHeight() * (style == Style::plate ? 0.40f : style == Style::quiet ? 0.31f : 0.37f));
            auto font = displayFont(size, juce::Font::bold);
            font.setExtraKerningFactor(0.09f);
            g.setFont(font);
            auto area = r.reduced(4.0f, 1.0f).translated(0.0f, down ? 0.0f : -0.5f);
            if (glyph == Glyph::gear)
            {
                const float tw = textWidth(font, getButtonText());
                const float cx = area.getCentreX() - tw * 0.5f - 3.0f;
                art::gearGlyph(g, { cx, area.getCentreY() + 0.3f }, 6.0f, ink);
                area.removeFromLeft(11.0f);
            }
            if (! accent)
            {
                g.setColour(art::black(0.6f));
                g.drawFittedText(getButtonText(), area.translated(0.0f, 1.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
            }
            else
            {
                g.setColour(art::white(0.45f));
                g.drawFittedText(getButtonText(), area.translated(0.0f, 1.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
            }
            g.setColour(ink);
            g.drawFittedText(getButtonText(), area.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
        if (on && (style == Style::key))
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

    // Small black rocker with a pilot lamp: the top half stands proud when released,
    // the lower half when latched on.
    void paintRocker(juce::Graphics& g, bool hovered, bool down)
    {
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        const bool on = getToggleState();
        const float rad = 2.6f;
        ui::softShadow(g, r, rad, 0.0f, down ? 0.8f : 1.8f, down ? 1.4f : 2.6f, 0.75f);
        juce::Path body;
        body.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), rad);
        g.setColour(juce::Colour(0xff0a0a09));
        g.fillPath(body);
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(body);
            const float split = r.getY() + r.getHeight() * (on ? 0.42f : 0.58f);
            const auto upper = juce::Rectangle<float>(r.getX(), r.getY(), r.getWidth(), split - r.getY());
            const auto lower = juce::Rectangle<float>(r.getX(), split, r.getWidth(), r.getBottom() - split);
            g.setGradientFill(art::vgrad(juce::Colour(on ? 0xff1f1e1c : 0xff3a3936).brighter(hovered ? 0.12f : 0.0f),
                                         juce::Colour(on ? 0xff161513 : 0xff23221f), upper));
            g.fillRect(upper);
            g.setGradientFill(art::vgrad(juce::Colour(on ? 0xff343330 : 0xff191816).brighter(hovered ? 0.12f : 0.0f),
                                         juce::Colour(on ? 0xff1e1d1a : 0xff0d0d0c), lower));
            g.fillRect(lower);
            g.setColour(art::white(0.20f));
            g.fillRect(r.getX(), r.getY() + 0.5f, r.getWidth(), 1.0f);
            g.setColour(art::black(0.9f));
            g.fillRect(r.getX(), split - 0.4f, r.getWidth(), 1.0f);
            g.setColour(art::white(0.10f));
            g.fillRect(r.getX(), split + 0.6f, r.getWidth(), 1.0f);
        }
        g.setColour(art::black(0.9f));
        g.drawRoundedRectangle(r, rad, 1.0f);

        const juce::Point<float> lamp(r.getX() + 9.0f, r.getCentreY());
        if (on)
        {
            g.setColour(juce::Colour(0xffffa530).withAlpha(0.28f));
            g.fillEllipse(lamp.x - 5.5f, lamp.y - 5.5f, 11.0f, 11.0f);
        }
        g.setColour(on ? juce::Colour(0xffffc65a) : juce::Colour(0xff3a1c10));
        g.fillEllipse(lamp.x - 2.6f, lamp.y - 2.6f, 5.2f, 5.2f);
        g.setColour(art::black(0.75f));
        g.drawEllipse(lamp.x - 2.6f, lamp.y - 2.6f, 5.2f, 5.2f, 0.7f);
        if (on)
        {
            g.setColour(art::white(0.7f));
            g.fillEllipse(lamp.x - 1.3f, lamp.y - 1.6f, 1.5f, 1.2f);
        }
        auto font = displayFont(juce::jlimit(8.0f, 11.0f, r.getHeight() * 0.38f), juce::Font::bold);
        font.setExtraKerningFactor(0.08f);
        g.setFont(font);
        auto text = r.withTrimmedLeft(18.0f).withTrimmedRight(4.0f).toNearestInt();
        g.setColour(art::black(0.7f));
        g.drawFittedText(getButtonText(), text.translated(0, 1), juce::Justification::centredLeft, 1, 0.75f);
        g.setColour(on ? juce::Colour(0xffffe3a0) : juce::Colour(0xffd8ceac));
        g.drawFittedText(getButtonText(), text, juce::Justification::centredLeft, 1, 0.75f);
        paintFocus(g, rad + 1.0f);
    }

    Style style;
    Glyph glyph = Glyph::none;
};

// A Label that honours its own font (the shared look-and-feel derives label fonts
// from the component height). It can show a compact player-facing string while the
// full engineering text stays in getText() for accessibility and tooltips.
class PanelLabel final : public juce::Label
{
public:
    PanelLabel() { setMinimumHorizontalScale(0.78f); }
    void setMinimumPixels(float px) { minPixels = px; }
    void setLines(int n) { lines = n; }
    void setShadow(float alpha) { shadow = alpha; }
    void setDisplayText(const juce::String& text)
    {
        if (text != display) { display = text; repaint(); }
    }
    // A status dot before the text; pass a transparent colour to hide it.
    void setDot(juce::Colour c)
    {
        if (c != dot) { dot = c; repaint(); }
    }
    void paint(juce::Graphics& g) override
    {
        auto f = getFont();
        if (minPixels > 0.0f && f.getHeight() < minPixels) f = f.withHeight(minPixels);
        g.setFont(f);
        auto area = getLocalBounds();
        if (! dot.isTransparent())
        {
            const float cy = (float) area.getCentreY();
            g.setColour(dot.withAlpha(0.30f));
            g.fillEllipse(0.5f, cy - 4.0f, 8.0f, 8.0f);
            g.setColour(dot);
            g.fillEllipse(2.0f, cy - 2.5f, 5.0f, 5.0f);
            area.removeFromLeft(11);
        }
        const auto text = display.isNotEmpty() ? display : getText();
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
    juce::String display;
    juce::Colour dot = juce::Colours::transparentBlack;
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
        const float pitch = (w - 25.0f) / (float) segments;
        for (int i = 0; i < segments; ++i)
        {
            const float db = floorDb + (float) (i + 1) * stepDb;
            const juce::Colour hue = db > -3.0f ? juce::Colour(0xffff4a30) : db > -12.0f ? juce::Colour(0xffffc22e) : juce::Colour(0xff55e27a);
            const auto seg = juce::Rectangle<float>((float) i * pitch, 1.0f, pitch - 1.2f, h - 2.0f);
            const bool lit = i < litSegments;
            if (lit)
            {
                g.setColour(hue.withAlpha(0.25f));
                g.fillRoundedRectangle(seg.expanded(1.3f), 2.0f);
            }
            g.setGradientFill(art::vgrad(lit ? hue.brighter(0.4f) : hue.withMultipliedBrightness(0.46f).withAlpha(0.75f),
                                         lit ? hue : hue.withMultipliedBrightness(0.22f).withAlpha(0.75f), seg));
            g.fillRoundedRectangle(seg, 1.0f);
            if (lit)
            {
                g.setColour(art::white(0.45f));
                g.fillRect(seg.getX() + 0.5f, seg.getY() + 0.5f, seg.getWidth() - 1.0f, 1.0f);
            }
        }
        const auto lamp = juce::Rectangle<float>(w - 23.0f, 0.0f, 23.0f, h);
        const bool alarm = over || nonFinite;
        const juce::Colour hue = nonFinite ? juce::Colour(0xffff3b30) : juce::Colour(0xffff8a2e);
        if (alarm)
        {
            g.setColour(hue.withAlpha(0.45f));
            g.fillRoundedRectangle(lamp.expanded(2.5f), 4.0f);
        }
        g.setGradientFill(art::vgrad(alarm ? hue.brighter(0.5f) : juce::Colour(0xff4a2218), alarm ? hue : juce::Colour(0xff22100a), lamp));
        g.fillRoundedRectangle(lamp, 2.5f);
        g.setColour(art::black(0.8f));
        g.drawRoundedRectangle(lamp, 2.5f, 0.8f);
        g.setColour(alarm ? juce::Colour(0xff2a0a05) : juce::Colour(0xffc88a6e));
        g.setFont(displayFont(8.0f, juce::Font::bold));
        g.drawText("OVR", lamp, juce::Justification::centred, false);
    }
    static constexpr int segments = 20;
    static constexpr float floorDb = -60.0f;
    static constexpr float stepDb = 3.0f;
private:
    int litSegments = 0;
    bool over = false, nonFinite = false;
};

// Eight voice lamps and a MIDI-in lamp. It polls the lock-free voice mask at
// 24 Hz and repaints its own small rectangle only when a lamp changes.
class ActivityLamps final : public juce::Component, private juce::Timer
{
public:
    explicit ActivityLamps(Cs80AudioProcessor& p) : processor(p)
    {
        setInterceptsMouseClicks(false, false);
        setTitle("Voice and MIDI activity");
        setDescription("Eight lamps follow the sounding voices; the MIDI lamp lights while notes arrive from the host.");
        startTimerHz(24);
    }
    ~ActivityLamps() override { stopTimer(); }

    // Called on the message thread whenever a lamp changes; the argument is the MIDI lamp state.
    std::function<void(bool)> onChange;

    void refresh()
    {
        const auto now = sample();
        if (now != shown)
        {
            shown = now;
            repaint();
            if (onChange) onChange(shown.midi);
        }
    }

    void paint(juce::Graphics& g) override
    {
        shown = sample();
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / geo::lampWell.getWidth(),
                                                    (float) getHeight() / geo::lampWell.getHeight()));
        art::legend(g, "VOICES", { 10.0f, 4.0f, 56.0f, 10.0f }, spacedFont(8.0f, 0.12f), juce::Justification::centredLeft,
                    juce::Colour(0xffcdb878), 0.8f);
        art::legend(g, "MIDI", { 94.0f, 4.0f, 28.0f, 10.0f }, spacedFont(8.0f, 0.12f), juce::Justification::centred,
                    juce::Colour(0xffcdb878), 0.8f);
        for (int i = 0; i < 8; ++i)
            lamp(g, { 14.0f + (float) i * 10.5f, 25.0f }, 3.9f, ((shown.mask >> i) & 1u) != 0, juce::Colour(0xffffb53a));
        lamp(g, { 108.0f, 25.0f }, 4.4f, shown.midi, juce::Colour(0xff5cf08a));
    }

private:
    struct Sample
    {
        unsigned mask = 0;
        bool midi = false;
        bool operator==(const Sample& o) const noexcept { return mask == o.mask && midi == o.midi; }
        bool operator!=(const Sample& o) const noexcept { return ! (*this == o); }
    };

    Sample sample()
    {
        Sample s;
        for (int i = 0; i < 8; ++i)
            if (processor.isVoiceActive(i)) s.mask |= 1u << i;
        bool held = false;
        for (int note = 0; note < 128 && ! held; ++note)
            held = processor.isHostKeyDown(note);
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (held) lastMidiMs = now;
        s.midi = now - lastMidiMs < 160.0;
        return s;
    }

    static void lamp(juce::Graphics& g, juce::Point<float> c, float r, bool lit, juce::Colour hue)
    {
        g.setColour(art::black(0.85f));
        g.fillEllipse(c.x - r - 1.0f, c.y - r - 0.6f, 2.0f * r + 2.0f, 2.0f * r + 2.0f);
        if (lit)
        {
            g.setColour(hue.withAlpha(0.30f));
            g.fillEllipse(c.x - r * 2.0f, c.y - r * 2.0f, r * 4.0f, r * 4.0f);
        }
        g.setGradientFill(juce::ColourGradient(lit ? hue.brighter(0.55f) : hue.withMultipliedBrightness(0.30f), c.x - r * 0.4f, c.y - r * 0.5f,
                                               lit ? hue.darker(0.25f) : hue.withMultipliedBrightness(0.10f), c.x + r, c.y + r, true));
        g.fillEllipse(c.x - r, c.y - r, 2.0f * r, 2.0f * r);
        g.setColour(art::white(lit ? 0.7f : 0.12f));
        g.fillEllipse(c.x - r * 0.55f, c.y - r * 0.75f, r * 0.8f, r * 0.5f);
    }

    void timerCallback() override { refresh(); }

    Cs80AudioProcessor& processor;
    Sample shown;
    double lastMidiMs = -1.0e9;
};

// The program display is also a button: hovering lights it, clicking opens the
// library. It draws only the hover glow, chevron and unsaved-edit dot; the glass
// itself is part of the buffered chassis and the text is carried by labels.
class PresetWindow final : public juce::Button
{
public:
    PresetWindow() : juce::Button({})
    {
        setTitle("Program display");
        setWantsKeyboardFocus(false);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Click to open the preset library.");
    }
    void setDirty(bool value)
    {
        if (value == dirty) return;
        dirty = value;
        setTooltip(dirty ? "Click to open the preset library. This program has unsaved edits."
                         : "Click to open the preset library.");
        repaint();
    }
    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / geo::presetWell.getWidth(),
                                                    (float) getHeight() / geo::presetWell.getHeight()));
        const auto w = juce::Rectangle<float>(0.0f, 0.0f, geo::presetWell.getWidth(), geo::presetWell.getHeight());
        if (hovered || down)
        {
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffb347).withAlpha(down ? 0.30f : 0.20f), w.getCentreX(), w.getCentreY(),
                                                   juce::Colours::transparentBlack, w.getX(), w.getCentreY(), true));
            g.fillRoundedRectangle(w.reduced(1.0f), 4.0f);
            g.setColour(juce::Colour(0xffffcf70).withAlpha(0.55f));
            g.drawRoundedRectangle(w.reduced(0.8f), 4.0f, 1.0f);
        }
        const auto c = juce::Point<float>(geo::presetChevron.getCentreX() - geo::presetWell.getX(),
                                          geo::presetChevron.getCentreY() - geo::presetWell.getY());
        juce::Path chevron;
        chevron.startNewSubPath(c.x - 5.0f, c.y - 2.5f);
        chevron.lineTo(c.x, c.y + 2.5f);
        chevron.lineTo(c.x + 5.0f, c.y - 2.5f);
        g.setColour(art::black(0.7f));
        g.strokePath(chevron, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                     juce::AffineTransform::translation(0.0f, 1.0f));
        g.setColour(hovered ? juce::Colour(0xffffe3a0) : juce::Colour(0xffd09a3c));
        g.strokePath(chevron, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (dirty)
        {
            const juce::Point<float> d(geo::presetName.getX() - geo::presetWell.getX() - 8.0f, geo::presetName.getCentreY() - geo::presetWell.getY() + 1.0f);
            g.setColour(juce::Colour(0xffffa530).withAlpha(0.35f));
            g.fillEllipse(d.x - 5.0f, d.y - 5.0f, 10.0f, 10.0f);
            g.setColour(juce::Colour(0xffffc65a));
            g.fillEllipse(d.x - 2.8f, d.y - 2.8f, 5.6f, 5.6f);
            g.setColour(art::white(0.7f));
            g.fillEllipse(d.x - 1.3f, d.y - 1.7f, 1.6f, 1.2f);
        }
    }
private:
    bool dirty = false;
};

// ---------------------------------------------------------------------------
// Live voice-flow plate: signal path, envelopes and filter response for the
// selected channel, computed from the panel (or the sounding tone/memory).
// Printed like the rest of the panel: flat cream and gold ink on a matte plate,
// no glow, with a plate-coloured halo under every label so lines never cut text.
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
        paintFilterScope(g, { 14.0f, 230.0f, 332.0f, 100.0f });
    }

private:
    using Rf = juce::Rectangle<float>;
    static juce::Colour ink() { return juce::Colour(0xfff0e6c4); }
    static juce::Colour gold() { return juce::Colour(0xffcdb878); }
    static juce::Colour goldDim() { return juce::Colour(0xff9a8a5c); }
    static juce::Colour curve() { return juce::Colour(0xfff2dc98); }
    static juce::Colour plate() { return art::plateColour(); }
    static juce::Colour field() { return art::plateColour().darker(0.22f); }

    float p(int local) const { return snap.v[(size_t) local]; }

    // Compact readouts so eight-pixel type fits a 160 px scope.
    static juce::String time(float seconds)
    {
        if (seconds < 1.0f) return juce::String(juce::roundToInt(seconds * 1000.0f)) + "ms";
        return juce::String(seconds, seconds < 10.0f ? 2 : 1) + "s";
    }
    static juce::String hertz(float hz)
    {
        return hz >= 1000.0f ? juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + "k" : juce::String(juce::roundToInt(hz));
    }

    void label(juce::Graphics& g, const juce::String& text, Rf area, float size, juce::Justification just,
               juce::Colour colour, float kerning = 0.08f)
    {
        art::haloText(g, text, area, spacedFont(size, kerning), just, colour, field());
    }

    void paintCaption(juce::Graphics& g)
    {
        const auto text = juce::String(snap.channel == 0 ? "CH I" : "CH II") + "  /  " + sourceLabel;
        art::haloText(g, text, { 14.0f, 38.0f, 332.0f, 12.0f }, spacedFont(8.5f, 0.12f), juce::Justification::centredLeft,
                      gold(), plate());
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
            const float ax0 = box.getRight() + 1.5f, ax1 = box.getRight() + 20.5f;
            arrow(g, ax0, y + h * 0.5f, i < 3 ? ax1 : ax1 - 2.0f, y + h * 0.5f, (active[i] || i == 0 || i == 3) ? 0.95f : 0.55f);
            if (! active[i])
            {
                // bypass rail over a switched-out filter
                const float x0 = box.getX() - 10.0f, x1 = box.getRight() + 10.0f;
                juce::Path rail;
                rail.startNewSubPath(x0, y + h * 0.5f);
                rail.lineTo(x0, y - 5.0f);
                rail.lineTo(x1, y - 5.0f);
                rail.lineTo(x1, y + h * 0.5f);
                g.setColour(gold().withAlpha(0.8f));
                g.strokePath(rail, juce::PathStrokeType(1.0f));
            }
            g.setColour(active[i] ? juce::Colour(0xff2f2c27) : field());
            g.fillRoundedRectangle(box, 2.5f);
            g.setColour(gold().withAlpha(active[i] ? 0.85f : 0.35f));
            g.drawRoundedRectangle(box.reduced(0.5f), 2.5f, active[i] ? 1.0f : 0.8f);
            art::haloText(g, names[i], box.withTrimmedBottom(i == 0 ? 5.0f : 0.0f), spacedFont(11.0f, 0.10f),
                          juce::Justification::centred, active[i] ? ink() : juce::Colour(0xff8a8168), juce::Colours::transparentBlack);
            if (i == 0)
            {
                const float levels[] = { p(cs80::CP_saw), p(cs80::CP_pulse), p(cs80::CP_noise) };
                for (int k = 0; k < 3; ++k)
                {
                    const juce::Point<float> c(box.getCentreX() + (float) (k - 1) * 11.0f, box.getBottom() - 4.5f);
                    const bool lit = levels[k] > 0.02f;
                    g.setColour(lit ? curve() : juce::Colour(0xff4a4437));
                    g.fillEllipse(c.x - 1.6f, c.y - 1.6f, 3.2f, 3.2f);
                }
            }
            if (i == 3)
            {
                // VCA level as a thin rule under the label
                const float lvl = juce::jlimit(0.0f, 1.0f, p(cs80::CP_vLevel));
                g.setColour(juce::Colour(0xff4a4437));
                g.fillRect(box.getX() + 8.0f, box.getBottom() - 5.0f, w - 16.0f, 1.8f);
                g.setColour(curve());
                g.fillRect(box.getX() + 8.0f, box.getBottom() - 5.0f, (w - 16.0f) * lvl, 1.8f);
            }
        }
        // sine path under the filters straight to the amplifier
        const float sine = juce::jlimit(0.0f, 1.0f, p(cs80::CP_sine));
        const float yy = y + h + 14.0f;
        juce::Path route;
        route.startNewSubPath(45.0f, y + h);
        route.lineTo(45.0f, yy);
        route.lineTo(297.0f, yy);
        route.lineTo(297.0f, y + h + 3.0f);
        g.setColour(gold().withAlpha(0.35f + 0.65f * sine));
        g.strokePath(route, juce::PathStrokeType(1.0f));
        juce::Path head;
        head.addTriangle(297.0f, y + h + 0.5f, 293.5f, y + h + 6.5f, 300.5f, y + h + 6.5f);
        g.fillPath(head);
        // the label always prints at full strength on a plate-coloured gap in the route
        g.setColour(plate());
        g.fillRect(150.0f, yy - 6.0f, 66.0f, 12.0f);
        art::legend(g, "SINE  " + juce::String(juce::roundToInt(sine * 100.0f)) + "%", { 150.0f, yy - 6.0f, 66.0f, 12.0f },
                    spacedFont(8.5f, 0.08f), juce::Justification::centred, ink(), 0.0f);
    }

    void arrow(juce::Graphics& g, float x0, float y0, float x1, float y1, float alpha)
    {
        g.setColour(gold().withAlpha(alpha));
        g.drawLine(x0, y0, x1 - 4.0f, y1, 1.0f);
        juce::Path head;
        head.addTriangle(x1, y1, x1 - 6.0f, y1 - 3.4f, x1 - 6.0f, y1 + 3.4f);
        g.fillPath(head);
    }

    // -- scope framing -------------------------------------------------------
    // A printed field: a slightly darker flat tint inside a hairline gold frame.
    void scopeFrame(juce::Graphics& g, Rf r, const juce::String& title, const juce::String& readout, bool readoutOnTop)
    {
        g.setColour(field());
        g.fillRoundedRectangle(r, 2.5f);
        g.setColour(gold().withAlpha(0.55f));
        g.drawRoundedRectangle(r.reduced(0.5f), 2.5f, 0.9f);
        art::legend(g, title, { r.getX() + 6.0f, r.getY() + 3.0f, r.getWidth() - 12.0f, 10.0f }, spacedFont(8.0f, 0.12f),
                    juce::Justification::centredLeft, gold(), 0.0f);
        if (readoutOnTop)
            art::legend(g, readout, { r.getX() + 90.0f, r.getY() + 3.0f, r.getWidth() - 96.0f, 10.0f }, displayFont(8.0f, juce::Font::bold),
                        juce::Justification::centredRight, juce::Colour(0xffd6cba6), 0.0f);
        else
            art::legend(g, readout, { r.getX() + 4.0f, r.getBottom() - 12.0f, r.getWidth() - 8.0f, 10.0f }, displayFont(8.0f, juce::Font::bold),
                        juce::Justification::centred, juce::Colour(0xffd6cba6), 0.0f);
    }

    Rf plotArea(Rf r) const { return { r.getX() + 8.0f, r.getY() + 16.0f, r.getWidth() - 16.0f, r.getHeight() - 31.0f }; }

    void grid(juce::Graphics& g, Rf plot, int cols, int rows)
    {
        g.setColour(gold().withAlpha(0.10f));
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

    // Flat cream line over a faint flat wash of the same ink.
    void strokeCurve(juce::Graphics& g, const juce::Path& line, const juce::Path& fill)
    {
        g.setColour(curve().withAlpha(0.13f));
        g.fillPath(fill);
        g.setColour(curve());
        g.strokePath(line, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void stageTicks(juce::Graphics& g, Rf plot, const Stages& s, const char* letters)
    {
        const float xs[] = { s.x0, s.xA, s.xD, s.xS, s.xR };
        g.setColour(gold().withAlpha(0.22f));
        const float dash[] = { 1.5f, 2.5f };
        for (int i = 1; i < 4; ++i)
            g.drawDashedLine(juce::Line<float>(xs[i], plot.getY(), xs[i], plot.getBottom()), dash, 2, 0.8f);
        for (int i = 0; i < 4; ++i)
            art::haloText(g, juce::String::charToString((juce::juce_wchar) letters[i]),
                          { xs[i], plot.getBottom() - 9.0f, xs[i + 1] - xs[i], 9.0f }, displayFont(8.0f, juce::Font::bold),
                          juce::Justification::centred, gold(), field());
    }

    void paintVcfScope(juce::Graphics& g, Rf r)
    {
        const float il = p(cs80::CP_fIl), al = p(cs80::CP_fAl);
        const float a = p(cs80::CP_fA), d = p(cs80::CP_fD), rel = p(cs80::CP_fR);
        scopeFrame(g, r, "VCF ENVELOPE", "A" + time(a) + " D" + time(d) + " R" + time(rel), false);
        const auto plot = plotArea(r);
        grid(g, plot, 8, 4);
        const auto s = stages(plot, a, d, rel);
        const float mid = plot.getCentreY(), span = plot.getHeight() * 0.46f;
        auto yOf = [&](float oct) { return mid - oct / 3.0f * span; };
        const float v0 = -3.0f * il, vA = 3.0f * al;
        // steady cutoff reference
        g.setColour(gold().withAlpha(0.40f));
        const float dash[] = { 3.0f, 3.0f };
        g.drawDashedLine(juce::Line<float>(plot.getX(), mid, plot.getRight(), mid), dash, 2, 0.8f);
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
            strokeCurve(g, line, fill);
        }
        stageTicks(g, plot, s, "ADSR");
        marker(g, { s.x0, yOf(v0) }, "IL", plot, true);
        marker(g, { s.xA, yOf(vA) }, "AL", plot, false);
    }

    // A dot on the curve with a haloed tag. The tag sits below-right of the start point and
    // above-left of the peak, then is clamped inside the plot so it never leaves the scope.
    void marker(juce::Graphics& g, juce::Point<float> c, const char* text, Rf plot, bool start)
    {
        g.setColour(curve());
        g.fillEllipse(c.x - 2.2f, c.y - 2.2f, 4.4f, 4.4f);
        g.setColour(field());
        g.drawEllipse(c.x - 2.2f, c.y - 2.2f, 4.4f, 4.4f, 0.8f);
        if (juce::String(text).isEmpty()) return;
        Rf tag(start ? c.x + 4.0f : c.x - 17.0f, start ? c.y + 3.0f : c.y - 12.0f, 14.0f, 9.0f);
        if (tag.getBottom() > plot.getBottom() - 9.0f) tag.setY(c.y - 12.0f);
        if (tag.getY() < plot.getY()) tag.setY(c.y + 3.0f);
        tag.setX(juce::jlimit(plot.getX() + 1.0f, plot.getRight() - tag.getWidth() - 1.0f, tag.getX()));
        art::haloText(g, text, tag, displayFont(8.0f, juce::Font::bold),
                      start ? juce::Justification::centredLeft : juce::Justification::centredRight, ink(), field());
    }

    void paintVcaScope(juce::Graphics& g, Rf r)
    {
        const float a = p(cs80::CP_vA), d = p(cs80::CP_vD), s = juce::jlimit(0.0f, 1.0f, p(cs80::CP_vS)), rel = p(cs80::CP_vR);
        scopeFrame(g, r, "VCA ENVELOPE",
                   "A" + time(a) + " D" + time(d) + " S" + juce::String(juce::roundToInt(s * 100.0f)) + "% R" + time(rel), false);
        const auto plot = plotArea(r);
        grid(g, plot, 8, 4);
        const auto st = stages(plot, a, d, rel);
        const float bottom = plot.getBottom() - 2.0f, top = plot.getY() + 3.0f;
        auto yOf = [&](float v) { return bottom - v * (bottom - top); };
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
            strokeCurve(g, line, fill);
        }
        stageTicks(g, plot, st, "ADSR");
        marker(g, { st.xA, yOf(1.0f) }, "", plot, false);
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
                   juce::String(hpOn ? "HPF " + hertz(hpf) + "Hz" : "HPF OFF") + "   " + (lpOn ? "LPF " + hertz(lpf) + "Hz" : "LPF OFF"), true);
        // The frequency axis has its own strip under the plot, so its numbers never sit on the curve.
        const Rf plot(r.getX() + 10.0f, r.getY() + 17.0f, r.getWidth() - 20.0f, r.getHeight() - 17.0f - 15.0f);
        const float fLo = 20.0f, fHi = 20000.0f;
        auto xOf = [&](float f) { return plot.getX() + plot.getWidth() * std::log10(f / fLo) / std::log10(fHi / fLo); };
        const float dbTop = 18.0f, dbBottom = -54.0f;
        auto yOf = [&](float db) { return plot.getY() + plot.getHeight() * (dbTop - juce::jlimit(dbBottom, dbTop, db)) / (dbTop - dbBottom); };
        g.setColour(gold().withAlpha(0.10f));
        for (float f : { 100.0f, 1000.0f, 10000.0f })
            g.drawVerticalLine((int) xOf(f), plot.getY(), plot.getBottom());
        for (float db : { 12.0f, 0.0f, -12.0f, -24.0f, -36.0f })
            g.drawHorizontalLine((int) yOf(db), plot.getX(), plot.getRight());
        g.setColour(gold().withAlpha(0.35f));
        const float dash[] = { 3.0f, 3.0f };
        g.drawDashedLine(juce::Line<float>(plot.getX(), yOf(0.0f), plot.getRight(), yOf(0.0f)), dash, 2, 0.8f);
        const std::pair<float, const char*> decades[] = { { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } };
        for (const auto& decade : decades)
            art::legend(g, decade.second, { xOf(decade.first) - 14.0f, plot.getBottom() + 2.0f, 28.0f, 9.0f },
                        displayFont(8.0f, juce::Font::bold), juce::Justification::centred, goldDim(), 0.0f);

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
            strokeCurve(g, line, fill);
        }
        auto cutoffMark = [&](float f, const char* tag, bool on)
        {
            if (! on) return;
            const float x = xOf(f), y = plot.getBottom();
            juce::Path tri;
            tri.addTriangle(x, y - 5.0f, x - 3.4f, y + 0.5f, x + 3.4f, y + 0.5f);
            g.setColour(ink());
            g.fillPath(tri);
            art::haloText(g, tag, { x - 5.0f, y - 15.0f, 10.0f, 9.0f }, displayFont(8.0f, juce::Font::bold),
                          juce::Justification::centred, ink(), field());
        };
        cutoffMark(hpf, "H", hpOn);
        cutoffMark(lpf, "L", lpOn);
    }

    Snapshot snap;
    juce::String sourceLabel { "PANEL" };
    bool scopes = true;
};


// Slider cap colours as printed on the main panel, so a memory row reads like a miniature of it.
juce::Colour memoryCapColour(int local)
{
    switch (local)
    {
        case cs80::CP_pwmSpeed: return ui::capCream;
        case cs80::CP_pwmDepth: case cs80::CP_pw: case cs80::CP_noise:
        case cs80::CP_hpfCutoff: case cs80::CP_lpfCutoff:
        case cs80::CP_initBrill: case cs80::CP_afterBrill: return ui::capGreen;
        case cs80::CP_hpfRes: case cs80::CP_lpfRes: return ui::capPink;
        case cs80::CP_fIl: case cs80::CP_fAl: case cs80::CP_fA: case cs80::CP_fD:
        case cs80::CP_vA: case cs80::CP_vD: case cs80::CP_vS: return ui::capBlack;
        case cs80::CP_fR: case cs80::CP_vR: return ui::capYellow;
        default: return ui::capGrey;
    }
}

// Magnified view of the four independent analog-memory panels. This does not
// copy a live patch: moving each miniature control edits that bank directly.
// The component is the whole lid: a hinge band above it, the rimmed lid itself and the
// shadow the raised lid throws on the deck below, so it is laid out in 1504 x 590 design units.
class MemoryProgrammer final : public juce::Component
{
public:
    explicit MemoryProgrammer(Cs80AudioProcessor& p) : processor(p)
    {
        setOpaque(false);
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
                    toggle->accent = ui::capBlack;
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
                    slider->setColour(juce::Slider::thumbColourId, memoryCapColour(local));
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
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / kW, (float) getHeight() / kH));
        const juce::Rectangle<float> lid(0.0f, geo::lidTop, kW, geo::lidHeight);

        // Shadow of the raised lid on the deck below it, and a little on either side.
        ui::softShadow(g, lid, 9.0f, 0.0f, 16.0f, 26.0f, 0.9f);
        g.setGradientFill(art::vgrad(art::black(0.42f), juce::Colours::transparentBlack, { 0.0f, lid.getBottom(), kW, kH - lid.getBottom() }));
        g.fillRect(8.0f, lid.getBottom(), kW - 16.0f, kH - lid.getBottom());

        // Hinge: a brushed barrel strip along the back edge with three knuckles.
        {
            const juce::Rectangle<float> strip(48.0f, 0.0f, kW - 96.0f, geo::lidTop + 6.0f);
            art::brushedMetal(g, strip, 2.0f, 0.0f, 41);
            g.setColour(art::black(0.8f));
            g.drawRoundedRectangle(strip, 2.0f, 1.0f);
            for (int i = 0; i < 4; ++i)
            {
                const float kx = strip.getX() + 120.0f + (float) i * (strip.getWidth() - 240.0f) / 3.0f;
                const juce::Rectangle<float> knuckle(kx - 34.0f, strip.getY() - 1.0f, 68.0f, strip.getHeight() + 2.0f);
                art::brushedMetal(g, knuckle, 3.0f, 0.3f, 50 + i);
                g.setColour(art::black(0.85f));
                g.drawRoundedRectangle(knuckle, 3.0f, 1.0f);
                art::slotScrew(g, { kx - 20.0f, knuckle.getCentreY() }, 2.4f, kx);
                art::slotScrew(g, { kx + 20.0f, knuckle.getCentreY() }, 2.4f, kx + 1.0f);
            }
        }

        // Machined rim, then the matte plate set into it.
        g.setGradientFill(art::vgrad(juce::Colour(0xff55514a), juce::Colour(0xff1c1a16), lid));
        g.fillRoundedRectangle(lid, 9.0f);
        g.setColour(art::white(0.30f));
        g.drawLine(10.0f, lid.getY() + 1.5f, kW - 10.0f, lid.getY() + 1.5f, 1.0f);
        g.setColour(art::black(0.85f));
        g.drawRoundedRectangle(lid, 9.0f, 1.0f);
        const auto inner = lid.reduced(9.0f);
        art::matteSheet(g, inner, 5.0f, juce::Colour(0xff1f1d1a), 77);
        ui::etchRect(g, inner.reduced(7.0f), 3.0f, 0.55f);
        for (float sx : { lid.getX() + 5.0f, lid.getRight() - 5.0f })
            for (float sy : { lid.getY() + 5.0f, lid.getBottom() - 5.0f })
                art::slotScrew(g, { sx, sy }, 3.0f, sx + sy);

        // Everything below is laid out in the programmer's own 1472 x 554 frame.
        g.addTransform(juce::AffineTransform::translation(kOffsetX, geo::lidTop));
        art::legend(g, "MEMORY PROGRAMMER", { 34, 20, 520, 26 }, spacedFont(19.0f, 0.16f),
                    juce::Justification::centredLeft, juce::Colour(0xffe9d695));
        art::legend(g, "FOUR INDEPENDENT MINIATURE PANELS  /  MAGNIFIED VIEW", { 35, 48, 760, 14 },
                    spacedFont(9.0f, 0.16f), juce::Justification::centredLeft, juce::Colour(0xffb5ab8c));
        ui::etchLine(g, 34, 66, 480, 66, 0.7f);
        {
            // Gold signal-flow diagram: the programmer owns everything from VCO to VCA.
            const char* stages[] = { "VCO", "HPF", "LPF", "VCA" };
            for (int i = 0; i < 4; ++i)
            {
                const juce::Rectangle<float> box(620.0f + (float) i * 140.0f, 22.0f, 90.0f, 28.0f);
                g.setColour(juce::Colour(0xff2a2723));
                g.fillRoundedRectangle(box, 3.0f);
                g.setColour(juce::Colour(0xffcdb878).withAlpha(0.7f));
                g.drawRoundedRectangle(box.reduced(0.5f), 3.0f, 0.9f);
                art::legend(g, stages[i], box, spacedFont(11.0f, 0.12f), juce::Justification::centred, juce::Colour(0xffe9d695), 0.0f);
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
            art::legend(g, "26 CONTROLS PER MEMORY", { 620, 52, 380, 12 }, spacedFont(8.0f, 0.2f),
                        juce::Justification::centredLeft, juce::Colour(0xffa99f80));
        }
        for (int bank = 0; bank < 4; ++bank)
        {
            const float y = rowY(bank);
            const juce::Rectangle<float> row(24.0f, y, 1424.0f, 103.0f);
            ui::etchRect(g, row, 4.0f, 0.6f);
            art::legend(g, bank % 2 == 0 ? "CHANNEL I" : "CHANNEL II", { 33.0f, y + 12.0f, 100.0f, 16.0f },
                        spacedFont(10.0f, 0.14f), juce::Justification::centred, juce::Colour(0xffe6dcbc));
            // Group brackets as on the main panel, then the legend of each control.
            const struct { const char* title; int first, last; } groups[] = {
                { "VCO", 0, 5 }, { "VCF", 6, 14 }, { "VCA", 15, 21 }, { "INITIAL", 22, 23 }, { "AFTER", 24, 25 } };
            for (const auto& group : groups)
            {
                const float x0 = colX(group.first) + 1.0f, x1 = colX(group.last) + 47.0f;
                art::bracket(g, group.title, x0, y + 9.0f, x1 - x0, 4.0f, 8.5f);
            }
            for (int col = 0; col < 26; ++col)
                art::legend(g, labels[(size_t) col], { colX(col), y + 17.0f, 48.0f, 20.0f }, displayFont(8.0f, juce::Font::bold),
                            juce::Justification::centred, juce::Colour(0xffe6dcbc), 0.7f);
        }
        art::legend(g, "Miniature controls are independent of the main panel. Select a memory to hear it; PANEL returns to the unchanged front-panel settings.",
                    { 34, 524, 1400, 16 }, displayFont(9.0f), juce::Justification::centredLeft, juce::Colour(0xffb0a688));
        g.restoreState();
    }

    void resized() override
    {
        const float sx = (float) getWidth() / kW, sy = (float) getHeight() / kH;
        auto r = [sx, sy](float x, float y, float w, float h) {
            return juce::Rectangle<int>(juce::roundToInt((x + kOffsetX) * sx), juce::roundToInt((y + geo::lidTop) * sy),
                                        juce::roundToInt(w * sx), juce::roundToInt(h * sy));
        };
        close.setBounds(r(1318, 20, 126, 30));
        for (int bank = 0; bank < 4; ++bank)
        {
            const float y = rowY(bank);
            bankButtons[(size_t) bank]->setBounds(r(34, y + 42, 98, 30));
            for (int col = 0; col < 26; ++col)
            {
                auto& item = items[(size_t)(bank * 26 + col)];
                if (item.slider) item.slider->setBounds(r(colX(col), y + 38, 48, 62));
                if (item.toggle) item.toggle->setBounds(r(colX(col) + 8.0f, y + 41, 32, 52));
            }
        }
    }
    std::function<void()> onClose;

private:
    static constexpr float kW = 1504.0f, kH = 590.0f, kOffsetX = 16.0f;
    static float rowY(int bank) { return 75.0f + (float) bank * 111.0f; }
    static float colX(int col) { return 145.0f + (float) col * 50.0f; }

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
        "IL","AL","A","D","R","VCF\nLEVEL","SINE","A","D","S","R","LEVEL",
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
        // The program display is a button under its own labels; labels never take the click.
        addAndMakeVisible(presetWindow);
        presetWindow.onClick = [this] { showBrowser(true, true); };
        for (auto* button : { &prevButton, &nextButton, &browseButton, &saveButton,
                              &initButton, &advancedButton, &panicButton })
        {
            button->setWantsKeyboardFocus(true);
            addAndMakeVisible(*button);
        }
        advancedButton.setGlyph(KeyButton::Glyph::gear);
        saveButton.getProperties().set("cs80Accent", true);
        addAndMakeVisible(lamps);
        lamps.onChange = [this](bool midi) { syncLampStatus(midi); };

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
        presetNumberLabel.setJustificationType(juce::Justification::centred);
        presetNumberLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb347));
        presetNumberLabel.setInterceptsMouseClicks(false, false);
        presetNumberLabel.setComponentID("preset_number");
        addAndMakeVisible(presetNumberLabel);
        for (auto* label : { &voiceStatusLabel, &midiStatusLabel })
        {
            label->setJustificationType(juce::Justification::centredLeft);
            label->setColour(juce::Label::textColourId, juce::Colour(0xffd9cfae));
            label->setInterceptsMouseClicks(true, false);
            addAndMakeVisible(*label);
        }
        voiceStatusLabel.setComponentID("voice_status");
        voiceStatusLabel.setTitle("Active voices");
        voiceStatusLabel.setTooltip("Number of sounding voices of the eight-voice engine. The lamps in the toolbar show which voices are active.");
        midiStatusLabel.setComponentID("midi_status");
        midiStatusLabel.setTitle("MIDI input activity");
        midiStatusLabel.setTooltip("Lights while notes arrive from the host or a MIDI controller.");
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
        syncLampStatus(false);
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
        addKnob(cs80::P_masterTune, 49, y + 28, 62, 72, "PITCH");
        addFader(cs80::P_detune, 114, y, 39, h, "DETUNE", whiteCap());
        addFader(cs80::P_ringAttack, 161, y, 38, h, "ATTACK", blackCap());
        addFader(cs80::P_ringDecay, 201, y, 38, h, "DECAY", blackCap());
        addFader(cs80::P_ringDepth, 241, y, 38, h, "DEPTH", greyCap());
        addFader(cs80::P_ringSpeed, 281, y, 38, h, "SPEED", whiteCap());
        addFader(cs80::P_ringAmount, 321, y, 38, h, "MOD", greyCap());
        // FUNCTION is a little wider than its neighbours, so its engraved legend would sit lower
        // than theirs. Lift the component by the difference to keep one baseline across the row.
        subFunction = &addFader(cs80::P_subWave, 373, y - 1.2f, 42, h - 0.5f, "FUNCTION", blackCap());
        subSpeed = &addFader(cs80::P_subSpeed, 417, y, 38, h, "SPEED", whiteCap());
        addFader(cs80::P_subVco, 457, y, 38, h, "VCO", whiteCap());
        addFader(cs80::P_subVcf, 497, y, 38, h, "VCF", greenCap());
        addFader(cs80::P_subVca, 537, y, 38, h, "VCA", greyCap());
        addFader(cs80::chParam(0, cs80::CP_feet), 582, y, 36, h, "FEET I", greyCap());
        addFader(cs80::chParam(1, cs80::CP_feet), 620, y, 36, h, "FEET II", greyCap());
        // Pitch follows the width of each engraved name so the legends never touch.
        addFader(cs80::P_mix, 1203, y, 38, h, "MIX", redCap());
        addFader(cs80::P_brilliance, 1241, y, 38, h, "BRILL", greenCap());
        addFader(cs80::P_resonance, 1285, y, 38, h, "RESO", redCap());
        addFader(cs80::P_initBend, 1330, y, 38, h, "INIT BEND", whiteCap());
        addFader(cs80::P_subAtSpeed, 1374, y, 38, h, "AT SPEED", whiteCap());
        addFader(cs80::P_subAtVco, 1417, y, 38, h, "AT VCO", whiteCap());
        addFader(cs80::P_subAtVcf, 1456, y, 38, h, "AT VCF", greenCap());
        addKnob(cs80::P_volume, 1497, y + 28, 50, 72, "VOLUME");
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
        fader(cs80::CP_initBrill, "BRILL", greenCap());
        fader(cs80::CP_initLevel, "LEVEL", greyCap());
        fader(cs80::CP_afterBrill, "BRILL", greenCap());
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
            voiceOptions.setToggleState(optionsVisible, juce::dontSendNotification);
            for (auto* component : optionComponents) component->setVisible(optionsVisible);
            voicePlate.setScopesVisible(! optionsVisible);
            backdrop.repaint();
            repaint();
        };
        place(voiceOptions, geo::voiceKey.getX(), geo::voiceKey.getY(), geo::voiceKey.getWidth(), geo::voiceKey.getHeight());
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
            memoryLid.setToggleState(true, juce::dontSendNotification);
        };
        place(memoryLid, geo::memoryLidKey.getX(), geo::memoryLidKey.getY(), geo::memoryLidKey.getWidth(), geo::memoryLidKey.getHeight());

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
            const auto key = channel == 0 ? geo::viewKeyI : geo::viewKeyII;
            place(button, key.getX(), key.getY(), key.getWidth(), key.getHeight());
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
                place(*button, geo::toneX0 + (float) tone * geo::tonePitch,
                       527.0f + (float) channel * 45.0f, geo::toneW, 41.0f);
                toneButtons[(size_t) channel][(size_t) tone] = std::move(button);
            }
        }
    }

    // Performance block: the left side of the deck.
    void buildPerformance()
    {
        const float y = 672.0f, h = 96.0f;
        // Everything sits between x66 and x470 so the block lines up with the key area (x62..1538).
        addFader(cs80::P_masterSustain, 66, y, 40, h, "SUSTAIN", yellowCap());
        addFader(cs80::P_portamento, 108, y, 40, h, "PORTA", whiteCap());
        addFader(cs80::P_glideMode, 150, y, 40, h, "GLIDE", greyCap());
        // Knobs are sized so their engraved legends match the faders' (about 10 px).
        addKnob(cs80::P_chorusSpeed, 196, y + 18, 54, 66, "SPEED");
        addKnob(cs80::P_chorusDepth, 252, y + 18, 54, 66, "DEPTH");
        auto& sus = addLed(cs80::P_sustainMode, "SUS II", 314, y + 26, 51, 62);
        sus.hardwareTab = true; sus.accent = whiteCap();
        auto& chorus = addLed(cs80::P_chorusOn, "CHORUS", 367, y + 26, 51, 62);
        chorus.hardwareTab = true; chorus.accent = yellowCap();
        auto& tremolo = addLed(cs80::P_chorusMode, "TREMOLO", 420, y + 26, 51, 62);
        tremolo.hardwareTab = true; tremolo.accent = whiteCap();
    }

    // Owner controls 9/10 (keyboard contour) plus the foot-controller group that
    // the original keeps on its left-hand panel: footswitch jack/contact, the two
    // assigners, EXP/WAH and the output switch. The right side of the deck.
    void buildKeyboardControls()
    {
        const float y = 672.0f, h = 96.0f;
        addFader(cs80::P_kbdBrillianceLow, 1134, y, 38, h, "LOW", greenCap());
        addFader(cs80::P_kbdBrillianceHigh, 1174, y, 38, h, "HIGH", greenCap());
        addFader(cs80::P_kbdLevelLow, 1224, y, 38, h, "LOW", greyCap());
        addFader(cs80::P_kbdLevelHigh, 1264, y, 38, h, "HIGH", greyCap());

        expressionPedal.setTitle("Foot controller position (expression)");
        expressionPedal.setComponentID("expression_pedal");
        expressionPedal.setDescription("On-screen foot controller: volume and the EXP-WAH sweep. MIDI CC11 follows this pedal with parity isolation OFF, in both legacy and source-performance modes. In isolation, CC11 is ignored; this on-screen pedal remains active.");
        expressionPedal.setValue(processor.getExpressionValue());
        expressionPedal.onValueChange = [this](float value) { processor.setExpressionFromUI(value); };
        addAndMakeVisible(expressionPedal);
        place(expressionPedal, 1312, 680, 40, 84);

        // Three columns of 54 px keep the block inside the key edge (x1538).
        auto toggle = [this](int parameter, const char* label, const char* tip, float x, float y)
        {
            auto& led = addLed(parameter, label, x, y, 54, 22);
            led.setTooltip(tip);
        };
        toggle(cs80::P_footswitchPlugged, "PLUG",
               "Footswitch plugged into the jack. With no plug the switched jack acts as a pressed switch.", 1358, 686);
        toggle(cs80::P_footswitchPressed, "PRESS",
               "Footswitch contact. Latches; toggle it to play the switch.", 1416, 686);
        toggle(cs80::P_footMode, "WAH",
               "Foot controller selector: off = EXP (volume only), on = EXP-WAH.", 1474, 686);
        toggle(cs80::P_sustainAssign, "SUSTAIN",
               "Assign the footswitch to SUSTAIN. Plugged and open switch removes the sustain release.", 1358, 730);
        toggle(cs80::P_glideAssign, "PORTA",
               "Assign the footswitch to portamento/glissando. Plugged and open switch suppresses gliding.", 1416, 730);
        toggle(cs80::P_outputLevel, "HIGH",
               "Output level switch: lit = HIGH (unity), unlit = LOW.", 1474, 730);
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
        const bool isFactory = tag != "USER PATCH";
        presetIndexLabel.setDisplayText(isFactory ? "OF " + juce::String((int) bank.size()) : juce::String());
        const auto number = isFactory ? juce::String(metadata.factoryIndex + 1).paddedLeft('0', 3) : juce::String("USR");
        if (presetNumberLabel.getText() != number) presetNumberLabel.setText(number, juce::dontSendNotification);
        presetWindow.setDirty(patchHasUnsavedEdits(metadata, isFactory));
    }

    // True when the sounding patch differs from the program that is named in the display. Factory programs
    // compare against the stored values; other programs against the state first seen under that name.
    bool patchHasUnsavedEdits(const Cs80AudioProcessor::PresetMetadata& metadata, bool isFactory)
    {
        const auto current = processor.currentValues();
        auto differs = [&](const cs80::ParamValues& reference)
        {
            for (int i = 0; i < cs80::P_numParams; ++i)
            {
                if (Cs80AudioProcessor::isPhysicalSetupParameter(i) || i == cs80::P_parityMode || i == cs80::P_sourcePerformance)
                    continue;
                const auto& def = cs80::paramDef(i);
                if (std::abs(current[i] - reference[i]) > 0.003f * (def.max - def.min) + 1.0e-4f)
                    return true;
            }
            return false;
        };
        if (isFactory)
            return differs(processor.getFactoryPresets()[(size_t) metadata.factoryIndex].values);
        const auto key = metadata.name + "|" + juce::String(metadata.factoryIndex);
        if (key != baselineKey)
        {
            baselineKey = key;
            baseline = current;
            return false;
        }
        return differs(baseline);
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

    // phase 0 = sheet fully away, 1 = sheet settled in place.
    void applyOverlayPhase(float phase)
    {
        if (animTarget == nullptr) return;
        const float slide = 22.0f * ((float) getHeight() / kDesignHeight);
        animTarget->setTransform(phase >= 1.0f ? juce::AffineTransform()
                                               : juce::AffineTransform::translation(0.0f, (1.0f - phase) * slide));
        animTarget->setAlpha(phase >= 1.0f ? 1.0f : 0.15f + 0.85f * phase);
        scrim.setAmount(phase);
        shade.setVisible(true);
    }

    void stepOverlayAnimation()
    {
        if (animTarget == nullptr) { frames.stop(); return; }
        const double elapsed = juce::Time::getMillisecondCounterHiRes() - animStart;
        const float raw = juce::jlimit(0.0f, 1.0f, (float) (elapsed / animDurationMs));
        const float eased = 1.0f - (1.0f - raw) * (1.0f - raw) * (1.0f - raw);
        applyOverlayPhase(animOpening ? eased : 1.0f - eased);
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
        browseButton.setToggleState(shouldShow, juce::dontSendNotification);
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
        // Players read a short tag; the full engineering status stays in the text and tooltip.
        subModeLabel.setDisplayText(status.startsWith("SUB MODE CHANGED") ? "SUB  RELEASE CONTROL"
                                    : status.contains("EXT BUS") ? (status.contains("DISABLED") ? "SUB  EXT BUS OFF" : "SUB  EXT BUS ON")
                                    : status.contains("LEGACY") ? "SUB  LEGACY" : "SUB  ORIGINAL");
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
        modeStatusLabel.setDisplayText(policy == cs80::RenderPolicy::isolation ? "MODE  ISOLATION  (HOVER FOR DETAILS)"
                                       : policy == cs80::RenderPolicy::sourcePerformance ? "MODE  SOURCE PERFORMANCE  (HOVER FOR DETAILS)"
                                                                                         : "MODE  LEGACY PERFORMANCE  (HOVER FOR DETAILS)");
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
            label.setDisplayText(juce::String(rank == 0 ? "I   " : "II  ") + identity);
            label.setDot(source < 11 ? juce::Colour(0xffe8ac3c) : source == 13 ? juce::Colour(0xffd9cfae) : juce::Colour(0xff58d6c0));
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
        // The toolbar shows only the peak-hold number; the full wording is the text and the tooltip.
        outputMeterLabel.setDisplayText(outputMeterDisplay.nonFinite() ? juce::String("NaN")
                                        : outputMeterDisplay.atFloor() ? juce::String("-INF") : level);
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

    void syncLampStatus(bool midi)
    {
        const int voices = juce::jlimit(0, 8, processor.getActiveVoiceCount());
        voiceStatusLabel.setDisplayText("VOICES  " + juce::String(voices) + " / 8");
        voiceStatusLabel.setDot(voices > 0 ? juce::Colour(0xffffb53a) : juce::Colour(0xff4a4437));
        midiStatusLabel.setDisplayText("MIDI IN");
        midiStatusLabel.setDot(midi ? juce::Colour(0xff5cf08a) : juce::Colour(0xff2f4a38));
        voiceStatusLabel.setText("Active voices: " + juce::String(voices) + " of 8", juce::dontSendNotification);
        midiStatusLabel.setText(midi ? "MIDI input: notes arriving" : "MIDI input: idle", juce::dontSendNotification);
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
        memoryLid.setToggleState(memoryProgrammer.isVisible(), juce::dontSendNotification);
        lamps.refresh();
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

    // The voice-flow plate: a matte instrument plate printed in flat gold and cream.
    // The live curves are drawn on top of it by VoicePlate.
    void paintVoicePlate(juce::Graphics& g)
    {
        const auto r = geo::plate;
        ui::softShadow(g, r, 4.0f, 0.0f, 4.0f, 9.0f, 0.8f);
        art::matteSheet(g, r, 4.0f, art::plateColour(), 21);
        for (float sx : { r.getX() + 5.5f, r.getRight() - 5.5f })
            for (float sy : { r.getY() + 5.5f, r.getBottom() - 5.5f })
                art::slotScrew(g, { sx, sy }, 2.7f, sx * 0.3f + sy);
        const float x0 = r.getX(), y0 = r.getY();
        art::haloText(g, "VOICE SIGNAL FLOW", { x0 + 14, y0 + 8, 220, 18 }, spacedFont(11.5f, 0.14f), juce::Justification::centredLeft,
                      juce::Colour(0xffeadfb8), art::plateColour());
        art::legend(g, "VIEW", { x0 + 232, y0 + 8, 52, 24 }, spacedFont(8.5f, 0.16f), juce::Justification::centredRight,
                    juce::Colour(0xffb5a878), 0.0f);
        ui::etchLine(g, x0 + 14, y0 + 52, x0 + 346, y0 + 52, 0.45f);
        if (optionsVisible)
        {
            const juce::Rectangle<float> well(x0 + 12, y0 + 116, 336, 188);
            ui::etchRect(g, well, 3.0f, 0.5f);
            art::haloText(g, "VOICE SETTINGS  /  SOFTWARE", { well.getX() + 8, well.getY() + 4, 240, 11 }, spacedFont(8.0f, 0.14f),
                          juce::Justification::centredLeft, juce::Colour(0xffcdb878), art::plateColour());
            panelText(g, "I", x0 + 24, 232 + 22, 20, 28, 12);
            panelText(g, "II", x0 + 24, 314 + 22, 20, 28, 12);
            ui::etchLine(g, x0 + 20, 306, x0 + 340, 306, 0.35f);
        }
    }

    // The raised back edge of the panel: a vent strip in the panel's own finish with louvres either
    // side of a silkscreen legend.
    void paintTopLip(juce::Graphics& g)
    {
        const juce::Rectangle<float> lip(geo::bodyLeft, 64, geo::bodyRight - geo::bodyLeft, 32);
        g.setColour(art::white(0.035f));
        g.fillRect(lip);
        g.setColour(art::white(0.14f));
        g.fillRect(lip.getX(), 64.0f, lip.getWidth(), 1.2f);
        g.setColour(art::black(0.65f));
        g.fillRect(lip.getX(), 94.5f, lip.getWidth(), 1.5f);
        g.setGradientFill(art::vgrad(art::black(0.42f), juce::Colours::transparentBlack, { lip.getX(), 96.0f, lip.getWidth(), 9 }));
        g.fillRect(lip.getX(), 96.0f, lip.getWidth(), 9.0f);
        art::louvre(g, { 76.0f, 72.0f, 472.0f, 17.0f });
        art::louvre(g, { 1052.0f, 72.0f, 472.0f, 17.0f });
        art::bracket(g, "8 VOICES  /  2 CHANNELS  /  4 MEMORIES", 566.0f, 80.5f, 468.0f, 4.0f, 9.0f);
        for (float sx : { 60.0f, 557.0f, 1043.0f, 1540.0f })
            art::slotScrew(g, { sx, 80.5f }, 3.6f, sx * 0.07f);
    }

    // Each channel is outlined in etched gold, not boxed in a bevelled tray.
    void paintChannel(juce::Graphics& g, int channel)
    {
        const float y0 = geo::rowY0[(size_t) channel];
        const juce::Rectangle<float> frame(424.0f, y0 - 6.0f, 1124.0f, 186.0f);
        ui::etchRect(g, frame, 4.0f, 0.62f);
        // channel numeral in a small etched box
        const juce::Rectangle<float> boss(430.0f, y0 + 66.0f, 24.0f, 40.0f);
        ui::etchRect(g, boss, 3.0f, 0.5f);
        panelText(g, channel == 0 ? "I" : "II", boss.getX(), boss.getY() + 6.0f, boss.getWidth(), 28.0f, channel == 0 ? 22.0f : 17.0f);
        art::bracket(g, "VCO", 459, y0 + 10, 235);
        art::bracket(g, "VCF", 706, y0 + 10, 354);
        art::bracket(g, "VCA", 1075, y0 + 10, 272);
        art::bracket(g, "INITIAL", 1365, y0 + 10, 76);
        art::bracket(g, "AFTER", 1445, y0 + 10, 76);
        // faint column separators between functional groups
        for (float x : { 702.0f, 1071.0f, 1360.0f })
            ui::etchLine(g, x, y0 + 22.0f, x, y0 + 170.0f, 0.22f);
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
        g.setColour(art::black(0.9f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
        // Row markers: large enough to read at a glance and clear of the first tab.
        for (int row = 0; row < 2; ++row)
        {
            const float cy = 527.0f + (float) row * 45.0f + 20.5f;
            art::legend(g, row == 0 ? "I" : "II", { 665.0f, cy - 9.0f, 21.0f, 18.0f }, spacedFont(11.5f, 0.06f),
                        juce::Justification::centred, juce::Colour(0xfff0e6c4), 0.8f);
        }
    }

    // The deck is a lower step of the same panel. Its blocks are outlined in etched gold; the
    // centre carries the nameplate and the ribbon.
    void paintDeck(juce::Graphics& g)
    {
        const juce::Rectangle<float> deck(geo::bodyLeft, geo::faceBottom, geo::bodyRight - geo::bodyLeft, geo::deckBottom - geo::faceBottom);
        g.setGradientFill(art::vgrad(art::black(0.55f), juce::Colours::transparentBlack, { deck.getX(), deck.getY(), deck.getWidth(), 18.0f }));
        g.fillRect(deck.getX(), deck.getY(), deck.getWidth(), 18.0f);
        for (const auto& block : { geo::deckLeft, geo::deckRight })
            ui::etchRect(g, block, 5.0f, 0.5f);
        // One bracket style for every deck header.
        const float ruleY = 673.0f;
        art::bracket(g, "PERFORMANCE", 66, ruleY, 124, 4.0f);
        art::bracket(g, "CHORUS / TREMOLO", 196, ruleY, 270, 4.0f);
        art::bracket(g, "BRILLIANCE", 1134, ruleY, 78, 4.0f);
        art::bracket(g, "LEVEL", 1222, ruleY, 80, 4.0f);
        art::bracket(g, "FOOT", 1312, ruleY, 40, 4.0f);
        art::bracket(g, "FOOTSWITCH", 1358, ruleY, 112, 4.0f);
        art::bracket(g, "EXP / WAH", 1474, ruleY, 58, 4.0f);
        art::bracket(g, "ASSIGN TO", 1358, 719.0f, 112, 4.0f);
        art::bracket(g, "OUTPUT", 1474, 719.0f, 58, 4.0f);

        // centre: nameplate and the ribbon slot
        const auto np = geo::nameplate;
        ui::softShadow(g, np, 3.0f, 0.0f, 5.0f, 9.0f, 0.95f);
        art::brushedMetal(g, np, 3.0f, 0.25f, 9);
        g.setGradientFill(art::hgrad(art::black(0.30f), juce::Colours::transparentBlack, { np.getX(), np.getY(), 28.0f, 1.0f }));
        g.fillRoundedRectangle(np.withWidth(28.0f), 3.0f);
        g.setGradientFill(art::hgrad(juce::Colours::transparentBlack, art::black(0.30f), { np.getRight() - 28.0f, np.getY(), 28.0f, 1.0f }));
        g.fillRoundedRectangle(np.withX(np.getRight() - 28.0f).withWidth(28.0f), 3.0f);
        g.setColour(art::white(0.60f));
        g.drawRoundedRectangle(np.reduced(1.3f), 2.5f, 0.9f);
        g.setColour(art::black(0.8f));
        g.drawRoundedRectangle(np, 3.0f, 1.0f);
        for (float sx : { np.getX() + 13.0f, np.getRight() - 13.0f })
            art::slotScrew(g, { sx, np.getCentreY() }, 4.4f, sx * 0.1f);
        art::engraved(g, "YAMAHA", np.reduced(30.0f, 3.0f), spacedFont(25.0f, 0.34f, 0.95f));
        art::legend(g, "POLYPHONIC", { 490, 679, 136, 20 }, spacedFont(10.5f, 0.2f), juce::Justification::centredRight, juce::Colour(0xffe6dcbc));
        art::legend(g, "SYNTHESIZER", { 974, 679, 136, 20 }, spacedFont(10.5f, 0.2f), juce::Justification::centredLeft, juce::Colour(0xffe6dcbc));

        const float rule = 718.0f;
        ui::etchLine(g, 504, rule, 568, rule, 0.7f);
        ui::etchLine(g, 1032, rule, 1096, rule, 0.7f);
        art::legend(g, "RIBBON CONTROLLER", { 568, rule - 7, 464, 14 }, spacedFont(9.0f, 0.2f));
        ui::etchLine(g, 504, rule, 504, rule + 5, 0.7f);
        ui::etchLine(g, 1096, rule, 1096, rule + 5, 0.7f);
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

    // Toolbar furniture only: the finish itself is the panel's. Controls are laid on the panel, with the
    // preset display and the output meter as the only recessed wells.
    void paintToolbar(juce::Graphics& g)
    {
        const float left = geo::bodyLeft, width = geo::bodyRight - geo::bodyLeft;
        g.setColour(art::white(0.16f));
        g.fillRect(left, 0.0f, width, 1.0f);
        g.setColour(juce::Colour(0xffcdb878).withAlpha(0.5f));
        g.fillRect(left, 62.0f, width, 1.0f);
        g.setColour(art::black(0.85f));
        g.fillRect(left, 63.0f, width, 1.0f);
        g.setGradientFill(art::vgrad(art::black(0.45f), juce::Colours::transparentBlack, { left, 64.0f, width, 12.0f }));
        g.fillRect(left, 64.0f, width, 12.0f);

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
            const juce::Rectangle<float> word(96, 6, 150, 28);
            g.setFont(spacedFont(25.0f, 0.10f));
            g.setColour(art::black(0.8f));
            g.drawText("CS-80", word.translated(0, 1.5f), juce::Justification::centredLeft, false);
            g.setGradientFill(art::vgrad(juce::Colour(0xfff6e8ac), juce::Colour(0xffb08d3c), word));
            g.drawText("CS-80", word, juce::Justification::centredLeft, false);
            art::legend(g, "REFERENCE EDITION", { 97, 33, 150, 11 }, spacedFont(8.0f, 0.2f), juce::Justification::centredLeft, juce::Colour(0xffb5a678));
        }

        // preset window: dark glass with a warm backlight, a printed divider after the program number
        {
            const auto w = geo::presetWell;
            art::raisedPlate(g, w.expanded(3.0f), 6.0f, juce::Colour(0xff6c675e), juce::Colour(0xff1f1d19), 0.4f);
            art::recessTray(g, w, 4.0f, juce::Colour(0xff1a0f07), juce::Colour(0xff090502), 8.0f, 0.9f);
            juce::Graphics::ScopedSaveState state(g);
            juce::Path clip; clip.addRoundedRectangle(w.reduced(1.0f), 4.0f);
            g.reduceClipRegion(clip);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe39a38).withAlpha(0.26f), w.getX() + 70.0f, w.getCentreY() + 4.0f,
                                                   juce::Colours::transparentBlack, w.getX(), w.getCentreY(), true));
            g.fillRect(w);
            g.setColour(art::black(0.16f));
            for (float y = w.getY() + 1.0f; y < w.getBottom(); y += 2.0f)
                g.fillRect(w.getX(), y, w.getWidth(), 0.7f);
            g.setGradientFill(juce::ColourGradient(art::white(0.10f), w.getX(), w.getY(),
                                                   juce::Colours::transparentWhite, w.getX() + 120.0f, w.getBottom(), false));
            g.fillRect(w.withHeight(w.getHeight() * 0.48f));
            const float dx = geo::presetNumber.getRight() + 3.0f;
            g.setColour(juce::Colour(0xffd09a3c).withAlpha(0.35f));
            g.fillRect(dx, w.getY() + 7.0f, 1.0f, w.getHeight() - 14.0f);
        }
        // button cluster: one etched outline, the keys sit on the panel
        ui::etchRect(g, geo::cluster, 5.0f, 0.5f);
        for (float x : { 860.0f, 930.0f, 994.0f })
        {
            g.setColour(art::black(0.5f));
            g.fillRect(x, 12.0f, 1.0f, 28.0f);
            g.setColour(art::white(0.07f));
            g.fillRect(x + 1.0f, 12.0f, 1.0f, 28.0f);
        }
        ui::etchRect(g, geo::lampWell, 5.0f, 0.5f);
        // output meter window
        {
            const auto w = geo::meterWell;
            art::recessTray(g, w, 4.0f, juce::Colour(0xff0e0f0c), juce::Colour(0xff050504), 8.0f, 0.9f);
            g.setGradientFill(art::vgrad(art::white(0.06f), juce::Colours::transparentWhite, w.withHeight(w.getHeight() * 0.5f)));
            g.fillRoundedRectangle(w.reduced(1.0f).withHeight(w.getHeight() * 0.5f), 3.0f);
            art::legend(g, "OUT", { 1264, 8, 32, 10 }, spacedFont(8.0f, 0.2f), juce::Justification::centredLeft, juce::Colour(0xffcdb878), 0.8f);
            art::legend(g, "dBFS", { 1264, 36, 42, 8 }, spacedFont(8.0f, 0.08f), juce::Justification::centredLeft, juce::Colour(0xffa99a6c), 0.8f);
            ui::etchLine(g, 1315, 11, 1315, 41, 0.35f);
            // dB scale under the bar: every printed number is at least 8 px and clear of its neighbours
            const float usable = geo::meterBar.getWidth() - 25.0f;
            const std::pair<float, const char*> marks[] = { { -60.0f, "-60" }, { -40.0f, "-40" }, { -20.0f, "-20" }, { -10.0f, "-10" }, { 0.0f, "0" } };
            for (const auto& m : marks)
            {
                const float x = geo::meterBar.getX() + (m.first + 60.0f) / 60.0f * usable;
                g.setColour(juce::Colour(0xffd8cfb0).withAlpha(0.6f));
                g.fillRect(x - 0.4f, 27.0f, 0.9f, 3.0f);
                art::legend(g, m.second, { x - 11.0f, 30.0f, 22.0f, 10.0f }, displayFont(8.0f, juce::Font::bold),
                            juce::Justification::centred, juce::Colour(0xffd8cfb0), 0.0f);
            }
            const float x3 = geo::meterBar.getX() + (-3.0f + 60.0f) / 60.0f * usable;
            g.setColour(juce::Colour(0xffff6a4a).withAlpha(0.8f));
            g.fillRect(x3 - 0.4f, 27.0f, 0.9f, 3.0f);
        }
        // PANIC guard: a red etched outline
        {
            const auto p = geo::panicBtn.expanded(4.0f);
            g.setColour(juce::Colour(0xffa8442f).withAlpha(0.55f));
            g.drawRoundedRectangle(p.reduced(0.5f), 5.0f, 1.0f);
            g.setColour(art::black(0.6f));
            g.drawRoundedRectangle(p.expanded(1.0f), 6.0f, 0.8f);
        }
        // player status strip: printed directly on the panel between hairlines
        {
            const auto t = geo::ticker;
            g.setColour(juce::Colour(0xffcdb878).withAlpha(0.28f));
            g.fillRect(t.getX(), t.getY() - 1.0f, t.getWidth(), 1.0f);
            g.setColour(art::black(0.5f));
            g.fillRect(t.getX(), t.getBottom() + 0.5f, t.getWidth(), 1.0f);
            for (float x : geo::tickerRules)
            {
                g.setColour(juce::Colour(0xffcdb878).withAlpha(0.28f));
                g.fillRect(x, t.getY() + 2.0f, 1.0f, t.getHeight() - 4.0f);
            }
        }
    }

    void paintChassis(juce::Graphics& g)
    {
        const float sx = (float) getWidth() / kDesignWidth;
        const float sy = (float) getHeight() / kDesignHeight;
        const float scale = sx;
        g.saveState();
        g.addTransform(juce::AffineTransform::scale(sx, sy));
        g.fillAll(juce::Colour(0xff070605));

        // ---- one fine crinkle finish for the toolbar, panel and deck ---------
        static const juce::Image crinkle = art::makeCrinkleTile(256, 5, juce::Colour(0xff302e2b), 0.17f);
        const juce::Rectangle<float> body(geo::bodyLeft, 0.0f, geo::bodyRight - geo::bodyLeft, geo::deckBottom);
        art::fillTile(g, crinkle, body, scale);
        // soft light from the upper left, falling away toward the far corner
        g.setGradientFill(juce::ColourGradient(art::white(0.06f), 300.0f, 90.0f, juce::Colours::transparentWhite, 1000.0f, 600.0f, true));
        g.fillRect(body);
        g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, 800.0f, 330.0f, art::black(0.14f), 1560.0f, 770.0f, true));
        g.fillRect(body);

        paintTopLip(g);
        paintChannel(g, 0);
        paintChannel(g, 1);
        paintVoicePlate(g);

        // ---- lower row: ring / sub / feet | tone selector | global ------------
        {
            const auto lp = geo::lowerPlate;
            ui::etchRect(g, lp, 5.0f, 0.55f);
            art::bracket(g, "RING MODULATOR", 158, 499, 200);
            art::bracket(g, "SUB OSCILLATOR", 370, 499, 205);
            art::bracket(g, "FEET", 580, 499, 80);
            art::bracket(g, "TONE SELECTOR", 675, 505, 514);
            art::bracket(g, "GLOBAL / TOUCH RESPONSE", 1208, 499, 340);
            paintToneBezel(g);
        }

        // front edge of the control surface over the deck
        g.setColour(art::white(0.05f));
        g.fillRect(geo::bodyLeft, 652.0f, geo::bodyRight - geo::bodyLeft, 8.0f);
        g.setColour(art::white(0.20f));
        g.fillRect(geo::bodyLeft, 652.0f, geo::bodyRight - geo::bodyLeft, 1.0f);
        g.setColour(art::black(0.9f));
        g.fillRect(geo::bodyLeft, 659.0f, geo::bodyRight - geo::bodyLeft, 1.5f);

        paintDeck(g);
        paintToolbar(g);

        // ---- keybed base (the Keybed component paints over this) -----------
        g.setColour(juce::Colour(0xff0a0908));
        g.fillRect(geo::keybed);

        // ---- screws on the control face -----------------------------------
        for (float x : { 61.0f, 1539.0f })
            art::slotScrew(g, { x, 645.0f }, 4.4f, (x + 645.0f) * 0.7f);

        // ---- walnut cheeks run the full height of the case, over everything --
        art::walnutCheek(g, { 0, 0, geo::bodyLeft, kDesignHeight }, true, geo::deckBottom);
        art::walnutCheek(g, { geo::bodyRight, 0, kDesignWidth - geo::bodyRight, kDesignHeight }, false, geo::deckBottom);
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
        presetWindow.setBounds(rr(geo::presetWell));
        lamps.setBounds(rr(geo::lampWell));
        presetNumberLabel.setBounds(rr(geo::presetNumber));
        presetNumberLabel.setFont(spacedFont(21.0f * sy, 0.06f));
        presetNumberLabel.setMinimumPixels(12.0f);
        presetNumberLabel.setShadow(0.7f);
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
        outputMeterLabel.setFont(displayFont(11.5f * sy, juce::Font::bold));
        outputMeterLabel.setMinimumPixels(8.0f);
        outputMeterLabel.setJustificationType(juce::Justification::centredLeft);
        outputMeterLabel.setLines(1);
        meterBar.setBounds(rr(geo::meterBar));
        for (int rank = 0; rank < 2; ++rank)
        {
            auto& label = sourceStatusLabels[(size_t) rank];
            label.setBounds(rr(rank == 0 ? geo::rankI : geo::rankII));
            label.setFont(displayFont(9.5f * sy, juce::Font::bold));
            label.setMinimumPixels(8.5f);
        }
        voiceStatusLabel.setBounds(rr(geo::voiceStatus));
        voiceStatusLabel.setFont(displayFont(9.5f * sy, juce::Font::bold));
        voiceStatusLabel.setMinimumPixels(8.5f);
        midiStatusLabel.setBounds(rr(geo::midiStatus));
        midiStatusLabel.setFont(displayFont(9.5f * sy, juce::Font::bold));
        midiStatusLabel.setMinimumPixels(8.5f);
        subModeLabel.setBounds(rr(geo::subStatus));
        subModeLabel.setFont(displayFont(9.5f * sy, juce::Font::bold));
        subModeLabel.setMinimumPixels(8.5f);
        modeStatusLabel.setBounds(rr(geo::modeStatus));
        modeStatusLabel.setFont(displayFont(9.5f * sy, juce::Font::bold));
        modeStatusLabel.setMinimumPixels(8.5f);

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
    KeyButton saveButton { "SAVE", KeyButton::Style::accent };
    KeyButton initButton { "INIT", KeyButton::Style::quiet };
    KeyButton advancedButton { "ADVANCED" };
    KeyButton panicButton { "PANIC", KeyButton::Style::alert };
    PresetWindow presetWindow;
    ActivityLamps lamps { processor };
    PanelLabel presetNumberLabel;
    PanelLabel voiceStatusLabel;
    PanelLabel midiStatusLabel;
    juce::String baselineKey;
    cs80::ParamValues baseline;
    GripLookAndFeel gripLookAndFeel;
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
    KeyButton voiceOptions { "VOICE SETTINGS", KeyButton::Style::rocker };
    KeyButton memoryLid { "OPEN MEMORY LID", KeyButton::Style::rocker };
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
    setLookAndFeel(&impl->gripLookAndFeel);
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
    setLookAndFeel(nullptr);
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
