#include "PluginEditor.h"

#include <map>
#include <tuple>

#include "AdvancedPanel.h"
#include "StudioMount.h"   // [ws8-studio]
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

// Painters that draw in the 1600 x 1000 design space (chassis, plate, lid, meter, lamps) run under a
// floor, so printed type never falls under 9 screen pixels when the editor is scaled down. Code that
// sizes fonts in screen pixels (labels, buttons) is not inside such a scope and is unaffected.
float gDesignFloor = 0.0f;
bool gSmallEditor = false;
struct FloorScope
{
    explicit FloorScope(float screenScale) : previous(gDesignFloor), previousSmall(gSmallEditor)
    {
        gDesignFloor = screenScale > 0.0f ? 9.0f / screenScale : 0.0f;
        gSmallEditor = screenScale > 0.0f && screenScale < 0.85f;
    }
    ~FloorScope() { gDesignFloor = previous; gSmallEditor = previousSmall; }
    float previous;
    bool previousSmall;
};

juce::Font displayFont(float height, int style = juce::Font::plain)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(),
                                        juce::jmax(height, gDesignFloor), style));
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
        const auto id = component.getComponentID();
        if (id == "expression_pedal" || id == "ribbon_controller" || id == "activity_lamps")
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
const float bodyLeft = 48.0f, bodyRight = 1552.0f;

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
const Rf initBtn      { 944, 13, 58, 26 };
const Rf advancedBtn  { 1014, 10, 92, 32 };
const Rf lampWell     { 1122, 6, 128, 40 };
const Rf meterWell    { 1258, 6, 196, 40 };
const Rf meterText    { 1262, 17, 52, 20 };
const Rf meterBar     { 1318, 11, 128, 14 };
const Rf panicBtn     { 1466, 10, 74, 32 };   // >= 50 x 20 px at the 1120 x 700 minimum (PluginPanicTests)
// The status strip is a recessed window under the toolbar whose length follows what it prints (see layoutStrip).
const float stripX = 52.0f, stripY = 46.0f, stripH = 16.0f, stripPad = 12.0f, stripGap = 24.0f;
const Rf ticker       { 54, 47, 1492, 14 };
const Rf rankI        { 64, 47, 150, 14 };
const Rf rankII       { 238, 47, 150, 14 };
const Rf voiceStatus  { 1300, 47, 100, 14 };      // (blank: the lamps in the toolbar already show voices and MIDI)
const Rf midiStatus   { 1410, 47, 100, 14 };
const Rf subStatus    { 412, 47, 200, 14 };
const Rf modeStatus   { 636, 47, 300, 14 };
const std::array<float, 5> tickerRules { 283.0f, 509.0f, 673.0f, 797.0f, 1001.0f };

// Control surface y 64-660
// The raised back edge is an 8 px ledge (y64-72). Everything from the voice plate down was designed with a 32 px
// ledge, so it is laid out in those coordinates and moved up by midShift when it is painted and placed; the
// 24 px it frees go to the keybed (see shifted()).
const float faceTop = 64.0f, faceBottom = 660.0f, deckBottom = 772.0f;       // design coordinates of the shifted block
const float lipBottom = 72.0f, midShift = 24.0f, midTop = 96.0f;
const float deckBottomFinal = deckBottom - midShift;                          // 748: where the keybed starts
inline float shifted(float y) { return y >= midTop ? y - midShift : y; }
const Rf plate        { 48, 98, 360, 378 };
const Rf memoryLidKey { 62, 436, 160, 24 };
const Rf voiceKey     { 230, 436, 160, 24 };
const Rf viewKeyI     { 332, 112, 26, 22 };
const Rf viewKeyII    { 360, 112, 30, 22 };
const std::array<float, 2> rowY0 { 104.0f, 296.0f };
const Rf lowerPlate   { 48, 484, 1504, 166 };
const Rf toneBezel    { 660, 520, 537, 102 };
const float toneX0 = 683.0f, tonePitch = 36.4f, toneW = 34.0f;

// Performance deck y 660-772 (outer edges at x48 and x1552 like the lower plate and the key area). The two
// blocks are kept to 342 px so the ribbon can run 804 px between them.
const Rf deckLeft     { 48, 664, 342, 104 };
const Rf deckRight    { 1210, 664, 342, 104 };
const Rf nameplate    { 640, 667, 320, 38 };
const Rf ribbonRect   { 398, 724, 804, 36 };
const Rf keybed       { 48, 748, 1504, 252 };           // final coordinates (not shifted)
const Rf overlay      { 0, 64, 1600, 936 };
// Dimming and the toolbar's shadow stop at the walnut cheeks, which stay lit behind every overlay.
const Rf overlayBody  { 48, 64, 1504, 936 };
// The lid component also carries its hinge band above and its cast shadow below.
const Rf memoryLid    { 48, 66, 1504, 590 };            // final coordinates (not shifted)
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
// The smoked glass of the voice-flow window (also the halo under its type).
inline Colour glassColour() { return Colour(0xff1c1a17); }
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
    recessTray(g, r, 2.5f, Colour(0xff0b0a09), Colour(0xff050505), 6.0f, 0.9f);
    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(r.reduced(1.5f).toNearestIntEdges());
    for (float y = r.getY() + 2.0f; y < r.getBottom() - 1.5f; y += 3.6f)
    {
        g.setColour(Colour(0xff000000));
        g.fillRect(r.getX() + 2.0f, y, r.getWidth() - 4.0f, 1.7f);
        g.setColour(Colour(0xff9a917a));                         // lit chamfer of the slat
        g.fillRect(r.getX() + 2.0f, y + 1.7f, r.getWidth() - 4.0f, 1.0f);
        g.setColour(Colour(0xff3a362d));                         // its flank, falling into the next cut
        g.fillRect(r.getX() + 2.0f, y + 2.7f, r.getWidth() - 4.0f, 0.7f);
    }
    // The top of the opening shades what is behind it; the ends fall away into the frame.
    g.setGradientFill(vgrad(black(0.55f), juce::Colours::transparentBlack, { r.getX(), r.getY(), r.getWidth(), 4.0f }));
    g.fillRect(r.getX(), r.getY(), r.getWidth(), 4.0f);
    g.setGradientFill(hgrad(black(0.5f), juce::Colours::transparentBlack, { r.getX(), r.getY(), 14.0f, r.getHeight() }));
    g.fillRect(r.getX(), r.getY(), 14.0f, r.getHeight());
    g.setGradientFill(hgrad(juce::Colours::transparentBlack, black(0.5f), { r.getRight() - 14.0f, r.getY(), 14.0f, r.getHeight() }));
    g.fillRect(r.getRight() - 14.0f, r.getY(), 14.0f, r.getHeight());
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

// Gold line art of the panel (frames, grouping rules): 1.25 px of gold at about 55 percent over a 1 px dark
// shadow offset down and to the right, so the line reads as etched into the enamel and holds up at 1x.
inline float etchAlpha(float a) { return a < 0.4f ? a : 0.40f + 0.28f * a; }

void etchLine(juce::Graphics& g, float x1, float y1, float x2, float y2, float alpha)
{
    const float a = etchAlpha(alpha);
    g.setColour(black(0.70f * juce::jmin(1.0f, a * 1.6f)));
    g.drawLine(x1 + 0.9f, y1 + 1.0f, x2 + 0.9f, y2 + 1.0f, 1.0f);
    g.setColour(Colour(0xffd6bb68).withAlpha(a));
    g.drawLine(x1, y1, x2, y2, 1.25f);
}

void etchRect(juce::Graphics& g, Rf r, float radius, float alpha)
{
    const float a = etchAlpha(alpha);
    g.setColour(black(0.70f * juce::jmin(1.0f, a * 1.6f)));
    g.drawRoundedRectangle(r.translated(0.9f, 1.0f), radius, 1.0f);
    g.setColour(Colour(0xffd6bb68).withAlpha(a));
    g.drawRoundedRectangle(r, radius, 1.25f);
}

// Gold etched grouping bracket: a rule with its title sitting in a gap and short end ticks. When the
// title does not fit the span at the current type size, the short title is used, and if that does not fit
// either the bracket is printed without text rather than letting it run into its neighbours.
void bracket(juce::Graphics& g, const juce::String& title, float x, float ruleY, float w,
             float tick = 6.0f, float fontSize = 9.5f, const juce::String& shortTitle = {})
{
    const auto font = spacedFont(fontSize, 0.10f);
    juce::String text = title;
    if (shortTitle.isNotEmpty() && textWidth(font, text) > w - 4.0f)
        text = shortTitle;
    if (textWidth(font, text) > w - 4.0f)
        text = {};
    const float gap = text.isEmpty() ? 0.0f : textWidth(font, text) * 0.5f + 8.0f;
    const float mid = x + w * 0.5f;
    if (text.isEmpty())
        art::etchLine(g, x, ruleY, x + w, ruleY, 0.8f);
    else
    {
        art::etchLine(g, x, ruleY, mid - gap, ruleY, 0.8f);
        art::etchLine(g, mid + gap, ruleY, x + w, ruleY, 0.8f);
    }
    art::etchLine(g, x, ruleY, x, ruleY + tick, 0.8f);
    art::etchLine(g, x + w, ruleY, x + w, ruleY + tick, 0.8f);
    if (text.isNotEmpty())
        legend(g, text, { x, ruleY - 8.0f, w, 16.0f }, font);
}

// Soft contact shadow all around an object that stands on the panel (no offset, so it also darkens
// the seam above it): tone tray, nameplate, knobs, row seams.
void ambient(juce::Graphics& g, Rf r, float radius, float spread = 6.0f, float alpha = 0.40f)
{
    ui::softShadow(g, r, radius, 0.0f, 1.5f, spread, alpha);
}

// Wrinkle-paint finish, as on the real panel: near-black enamel broken into crisp cells of 2 to 3 px, every
// cell a small facet with its own random tilt, a groove between neighbouring cells, and sparse pin-point
// speculars where a facet happens to face a tight light from the upper left. Cells come from a toroidal
// jittered grid (Worley F1/F2), so the tile wraps, has no preferred direction and no blotches at larger
// scales. Opaque, built once and cached by the caller. `contrast` scales how strongly facets differ.
juce::Image makeWrinkleTile(int n, int seed, Colour albedo, float contrast)
{
    const int cells = 96;                                   // 256 px / 96 = cells of about 2.7 px
    const float cs = (float) n / (float) cells;
    juce::Random rng(seed);
    struct Cell { float px, py, tx, ty, tone; };
    std::vector<Cell> grid((size_t) (cells * cells));
    auto gauss = [&] { return (rng.nextFloat() + rng.nextFloat() + rng.nextFloat() + rng.nextFloat() - 2.0f) * 0.52f; };
    for (int cy = 0; cy < cells; ++cy)
        for (int cx = 0; cx < cells; ++cx)
        {
            auto& c = grid[(size_t) (cy * cells + cx)];
            c.px = ((float) cx + 0.12f + 0.76f * rng.nextFloat()) * cs;
            c.py = ((float) cy + 0.12f + 0.76f * rng.nextFloat()) * cs;
            c.tx = gauss(); c.ty = gauss();
            c.tone = 1.0f + 0.035f * (rng.nextFloat() - 0.5f) * 2.0f;     // faint pigment variation, per cell
        }
    // light from the upper left; the viewer looks straight down
    const float lx = -0.45f, ly = -0.55f, lz = 0.70f;
    const float ll = std::sqrt(lx * lx + ly * ly + lz * lz);
    const float Lx = lx / ll, Ly = ly / ll, Lz = lz / ll;
    float hx = Lx, hy = Ly, hz = Lz + 1.0f;
    const float hl = std::sqrt(hx * hx + hy * hy + hz * hz);
    hx /= hl; hy /= hl; hz /= hl;

    // A very faint directional grain: each row carries its own small gain and a slow wave along its length, so
    // the finish has a brushed direction at close range without ever reading as stripes. Periodic over the tile.
    std::vector<float> rowGain((size_t) n), rowAmp((size_t) n), rowPhase((size_t) n);
    std::vector<int> rowCycles((size_t) n);
    for (int y = 0; y < n; ++y)
    {
        rowGain[(size_t) y] = (rng.nextFloat() - 0.5f) * 2.0f;
        rowAmp[(size_t) y] = rng.nextFloat();
        rowPhase[(size_t) y] = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        rowCycles[(size_t) y] = 3 + rng.nextInt(6);
    }

    juce::Image image(juce::Image::RGB, n, n, false);
    juce::Image::BitmapData data(image, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            const float fx = (float) x + 0.5f, fy = (float) y + 0.5f;
            const int cx0 = (int) (fx / cs), cy0 = (int) (fy / cs);
            float d1 = 1.0e9f, d2 = 1.0e9f;
            const Cell* nearest = nullptr;
            float nearDx = 0.0f, nearDy = 0.0f;
            for (int oy = -1; oy <= 1; ++oy)
                for (int ox = -1; ox <= 1; ++ox)
                {
                    int cx = cx0 + ox, cy = cy0 + oy;
                    float wrapX = 0.0f, wrapY = 0.0f;
                    if (cx < 0) { cx += cells; wrapX = -(float) n; } else if (cx >= cells) { cx -= cells; wrapX = (float) n; }
                    if (cy < 0) { cy += cells; wrapY = -(float) n; } else if (cy >= cells) { cy -= cells; wrapY = (float) n; }
                    const auto& c = grid[(size_t) (cy * cells + cx)];
                    const float dx = fx - (c.px + wrapX), dy = fy - (c.py + wrapY);
                    const float d = std::sqrt(dx * dx + dy * dy);
                    if (d < d1) { d2 = d1; d1 = d; nearest = &c; nearDx = dx; nearDy = dy; }
                    else if (d < d2) d2 = d;
                }
            // facet normal: the cell's own tilt plus a little dome so a highlight is a point inside the cell
            const float sx = contrast * nearest->tx + 0.30f * nearDx / cs;
            const float sy = contrast * nearest->ty + 0.30f * nearDy / cs;
            const float inv = 1.0f / std::sqrt(sx * sx + sy * sy + 1.0f);
            const float nx = -sx * inv, ny = -sy * inv, nz = inv;
            const float diffuse = juce::jmax(0.0f, nx * Lx + ny * Ly + nz * Lz) / Lz;
            const float edge = juce::jlimit(0.0f, 1.0f, (d2 - d1) / (cs * 0.34f));
            const float groove = 0.50f + 0.50f * std::pow(edge, 0.8f);
            const float brushed = 1.0f + 0.016f * (0.55f * rowGain[(size_t) y]
                                                   + 0.45f * rowAmp[(size_t) y] * std::sin(juce::MathConstants<float>::twoPi * (float) rowCycles[(size_t) y] * (float) x / (float) n + rowPhase[(size_t) y]));
            float lum = (0.50f + 0.50f * juce::jlimit(0.0f, 1.4f, diffuse)) * groove * nearest->tone * brushed;
            const float ndh = juce::jmax(0.0f, nx * hx + ny * hy + nz * hz);
            const float spec = std::pow(ndh, 420.0f) * edge;               // very tight: only the odd facet ever flashes
            auto channel = [&](float albedoChannel, float specTint)
            {
                const float v = albedoChannel * lum * 255.0f + spec * specTint * 0.17f;
                return (juce::uint8) juce::jlimit(0, 255, juce::roundToInt(v));
            };
            data.setPixelColour(x, y, Colour(channel(albedo.getFloatRed(), 255.0f), channel(albedo.getFloatGreen(), 248.0f),
                                             channel(albedo.getFloatBlue(), 232.0f)));
        }
    return image;
}

// The one finish used by the toolbar, panel, deck and memory lid: near-black, neutral to very slightly warm.
const juce::Image& panelTile()
{
    static const juce::Image tile = makeWrinkleTile(256, 5, Colour(0xff1d1c19), 0.46f);
    return tile;
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

// Walnut cheek that runs the full height of the case, shaded as a convex bullnosed block lit from the upper
// left: a dark rim, a broad lit shoulder about 30 percent from the left edge, and a dark fall-off toward the
// right. The grain is long and nearly straight with the occasional cathedral where the rings are pushed
// sideways, a thin varnish streak rides the shoulder, and the top is the sloped end-grain face of the block.
// The left cheek turns its outer edge toward the light; the right cheek takes the same light more softly on
// the panel side and falls into shadow on its outer edge. Palette (design px, 8-bit):
//   grain (lightest .. darkest early/late wood)  0x5a2c16 .. 0x34170b
//   lit shoulder after shading                   about 0x5e2e16 (left) / 0x4c2412 (right)
//   rim and joint                                about 0x26110a
// The face is built per pixel at device resolution so the grain stays crisp at every window size.
inline float wHash(int x, int y, int seed)
{
    uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + (uint32_t) seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float) ((h ^ (h >> 16)) & 0xffffffu) / 16777216.0f;
}

inline float wNoise(float x, float y, int seed)
{
    const int xi = (int) std::floor(x), yi = (int) std::floor(y);
    float fx = x - (float) xi, fy = y - (float) yi;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    const float a = wHash(xi, yi, seed), b = wHash(xi + 1, yi, seed), c = wHash(xi, yi + 1, seed), d = wHash(xi + 1, yi + 1, seed);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

inline float wStep(float a, float b, float x)
{
    const float t = juce::jlimit(0.0f, 1.0f, (x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

juce::Image makeWalnutFace(Rf r, bool left, float scale)
{
    const int iw = juce::jmax(1, (int) std::ceil(r.getWidth() * scale)), ih = juce::jmax(1, (int) std::ceil(r.getHeight() * scale));
    juce::Image image(juce::Image::RGB, iw, ih, false);
    juce::Image::BitmapData data(image, juce::Image::BitmapData::writeOnly);
    const int s0 = left ? 11 : 83;
    const float w = r.getWidth(), h = r.getHeight();

    // Cathedral figures: centre across the block, along it, half length, half width, push in px.
    struct Figure { float cx, cy, half, wide, push; };
    juce::Random rng(left ? 3 : 29);
    std::array<Figure, 3> figures;
    for (auto& f : figures)
        f = { w * (0.30f + 0.40f * rng.nextFloat()), 120.0f + rng.nextFloat() * (h - 330.0f), 70.0f + rng.nextFloat() * 70.0f,
              w * (0.22f + 0.12f * rng.nextFloat()), (rng.nextBool() ? 1.0f : -1.0f) * (5.0f + 4.0f * rng.nextFloat()) };

    const Colour early(0xff6a3418), late(0xff3e1c0d);
    for (int py = 0; py < ih; ++py)
        for (int px = 0; px < iw; ++px)
        {
            const float u = ((float) px + 0.5f) / scale, v = ((float) py + 0.5f) / scale;
            const float xn = u / w;
            // Ring coordinate: nearly straight vertical grain that wanders slowly along the block.
            float ring = u + 3.2f * (wNoise(v / 190.0f, 0.5f, s0) - 0.5f) + 1.3f * (wNoise(v / 51.0f, 1.7f, s0 + 1) - 0.5f)
                         + 2.4f * (wNoise(u / 11.0f, v / 360.0f, s0 + 2) - 0.5f);
            for (const auto& f : figures)
            {
                const float dv = (v - f.cy) / f.half, du = (u - f.cx) / f.wide;
                ring += f.push * std::exp(-dv * dv * 1.6f) * juce::jmax(0.0f, 1.0f - std::abs(du)) * (1.0f - 0.35f * du * du);
            }
            // Tone from contours of long, stretched noise: broad early-wood runs, thin late-wood lines.
            const float broad = wNoise(ring / 6.5f, v / 520.0f, s0 + 3);
            const float mid = wNoise(ring / 2.6f, v / 140.0f, s0 + 4);
            float tone = 0.50f + 0.58f * (broad - 0.5f) + 0.30f * (mid - 0.5f);
            const float line = 1.0f - wStep(0.0f, 0.030f, std::abs(wNoise(ring * 0.24f, v / 430.0f, s0 + 5) - 0.5f));
            const float fine = 1.0f - wStep(0.0f, 0.045f, std::abs(wNoise(ring * 0.61f, v / 260.0f, s0 + 6) - 0.5f));
            tone = juce::jlimit(0.0f, 1.0f, tone - 0.55f * line - 0.25f * fine);
            float cr = late.getFloatRed() + (early.getFloatRed() - late.getFloatRed()) * tone;
            float cg = late.getFloatGreen() + (early.getFloatGreen() - late.getFloatGreen()) * tone;
            float cb = late.getFloatBlue() + (early.getFloatBlue() - late.getFloatBlue()) * tone;
            // Pores: short dark dashes along the grain.
            const float pore = wStep(0.80f, 0.94f, wNoise(u * 1.9f, v / 5.0f, s0 + 7));
            const float poreDark = 1.0f - 0.34f * pore;

            // Convex section lit from the upper left.
            float shade;
            if (left)
            {
                const float k = (xn - 0.28f) / 0.21f;
                shade = 0.60f + 0.72f * std::exp(-k * k) + 0.14f * std::exp(-(xn * xn) / 0.0064f);
                shade *= 1.0f - 0.30f * wStep(0.78f, 1.0f, xn);
            }
            else
            {
                const float k = (xn - 0.26f) / 0.21f;
                shade = 0.58f + 0.50f * std::exp(-k * k);
                shade *= 1.0f - 0.40f * wStep(0.66f, 1.0f, xn);
                shade *= 0.78f + 0.22f * wStep(0.0f, 0.10f, xn);
            }
            // The block darkens toward the foot, never below about 80 percent of its value at the top.
            const float along = 1.0f - 0.20f * wStep(0.30f, 1.0f, v / h);
            float m = shade * along * poreDark;
            // Thin varnish streak on the shoulder: the reflection of a tube light.
            const float sx = left ? 0.24f : 0.22f;
            const float streak = std::exp(-((xn - sx) * w) * ((xn - sx) * w) / 2.6f)
                                 * (0.45f + 0.55f * wNoise(v / 240.0f, 3.3f, s0 + 8)) * wStep(10.0f, 70.0f, v) * (1.0f - wStep(h - 90.0f, h - 20.0f, v));
            const float streakGain = (left ? 0.20f : 0.12f) * streak;
            cr = cr * m + (0.91f - cr * m) * streakGain;
            cg = cg * m + (0.69f - cg * m) * streakGain;
            cb = cb * m + (0.50f - cb * m) * streakGain;
            data.setPixelColour(px, py, Colour::fromFloatRGBA(juce::jlimit(0.0f, 1.0f, cr), juce::jlimit(0.0f, 1.0f, cg), juce::jlimit(0.0f, 1.0f, cb), 1.0f));
        }
    return image;
}

// The face is expensive to build and the chassis repaints whenever the status strip changes length, so keep the
// last few sizes. Message thread only (it is only called from painting).
juce::Image cachedWalnutFace(Rf r, bool left, float scale)
{
    static std::map<std::tuple<int, int, int, bool>, juce::Image> cache;
    const auto key = std::make_tuple(juce::roundToInt(r.getWidth() * 10.0f), juce::roundToInt(r.getHeight()), juce::roundToInt(scale * 1000.0f), left);
    if (const auto found = cache.find(key); found != cache.end()) return found->second;
    if (cache.size() > 8) cache.clear();
    return cache[key] = makeWalnutFace(r, left, scale);
}

void walnutCheek(juce::Graphics& g, Rf r, bool left, float shadowBottom, float scale)
{
    // Shadow thrown on the panel (and, below the panel, left to the keybed's own frame).
    {
        const float w = 14.0f;
        const float x0 = left ? r.getRight() : r.getX() - w;
        g.setGradientFill(juce::ColourGradient(black(left ? 0.58f : 0.46f), left ? r.getRight() : r.getX(), 0.0f,
                                               juce::Colours::transparentBlack, left ? r.getRight() + w : r.getX() - w, 0.0f, false));
        g.fillRect(x0, r.getY(), w, shadowBottom - r.getY());
    }
    // The top is cut on a slope that falls toward the outside of the case.
    const float drop = 14.0f;
    auto topAt = [&](float x) { const float t = (x - r.getX()) / r.getWidth(); return r.getY() + drop * (left ? 1.0f - t : t); };
    // Its outer top corner is rounded (a bullnosed block, not a cut slab).
    const float corner = 13.0f;
    juce::Path shape;
    if (left)
    {
        shape.startNewSubPath(r.getX(), topAt(r.getX()) + corner);
        shape.quadraticTo(r.getX(), topAt(r.getX()), r.getX() + corner, topAt(r.getX() + corner));
        shape.lineTo(r.getRight(), topAt(r.getRight()));
    }
    else
    {
        shape.startNewSubPath(r.getX(), topAt(r.getX()));
        shape.lineTo(r.getRight() - corner, topAt(r.getRight() - corner));
        shape.quadraticTo(r.getRight(), topAt(r.getRight()), r.getRight(), topAt(r.getRight()) + corner);
    }
    shape.lineTo(r.getRight(), r.getBottom());
    shape.lineTo(r.getX(), r.getBottom());
    shape.closeSubPath();
    {
    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(shape);
    const auto face = cachedWalnutFace(r, left, scale);
    g.drawImage(face, r.getX(), r.getY(), r.getWidth(), r.getHeight(), 0, 0, face.getWidth(), face.getHeight());

    // Edges: the left cheek's outer edge turns toward the light and catches a hairline; its inner edge is a
    // dark joint against the panel. The right cheek's panel side takes a faint lit chamfer, its outer edge none.
    if (left)
    {
        g.setColour(Colour(0xffe2a874).withAlpha(0.30f));
        g.fillRect(r.getX() + 1.0f, r.getY(), 1.0f, r.getHeight());
    }
    else
    {
        g.setColour(Colour(0xffc88a58).withAlpha(0.14f));
        g.fillRect(r.getX() + 1.0f, r.getY(), 1.0f, r.getHeight());
    }
    g.setColour(black(0.92f));
    g.fillRect(left ? r.getRight() - 1.0f : r.getX(), r.getY(), 1.0f, r.getHeight());

    // The face rolls over into the top: a soft lit gradient where the surface turns up, and a dark crease where
    // the end-grain cap begins.
    g.setGradientFill(vgrad(white(left ? 0.10f : 0.06f), juce::Colours::transparentWhite, { r.getX(), r.getY() + 28.0f, r.getWidth(), 70.0f }));
    g.fillRect(r.getX(), r.getY() + 28.0f, r.getWidth(), 70.0f);

    // End-grain top: the sloped face of the block, 18 px deep with tight rays and growth rings, a lit chamfer
    // along its upper edge and a dark joint below it.
    {
        juce::Random rng(left ? 5 : 17);
        juce::Path cap;
        cap.startNewSubPath(r.getX(), topAt(r.getX()));
        cap.lineTo(r.getRight(), topAt(r.getRight()));
        cap.lineTo(r.getRight(), topAt(r.getRight()) + 18.0f);
        cap.lineTo(r.getX(), topAt(r.getX()) + 18.0f);
        cap.closeSubPath();
        juce::Graphics::ScopedSaveState capState(g);
        g.reduceClipRegion(cap);
        const Rf box(r.getX(), r.getY(), r.getWidth(), drop + 18.0f);
        g.setGradientFill(vgrad(Colour(0xff5c3018), Colour(0xff2e170a), box));
        g.fillRect(box);
        const float fanX = r.getCentreX();
        for (float x = box.getX() - 6.0f; x < box.getRight() + 6.0f; x += 1.5f)
        {
            const float topX = fanX + (x - fanX) * 1.35f;
            g.setColour((rng.nextBool() ? Colour(0xff1c0c06) : Colour(0xff9a6038)).withAlpha(0.10f + rng.nextFloat() * 0.22f));
            g.drawLine(x, topAt(x) + 18.0f, topX, topAt(topX) - 2.0f, 0.7f);
        }
        for (float k = 3.0f; k < 18.0f; k += 5.0f)                                     // faint growth rings
        {
            juce::Path ring;
            ring.startNewSubPath(r.getX(), topAt(r.getX()) + k);
            ring.lineTo(r.getRight(), topAt(r.getRight()) + k);
            g.setColour(Colour(0xff1c0c06).withAlpha(0.18f));
            g.strokePath(ring, juce::PathStrokeType(0.8f));
        }
        juce::Path edge;                                                                  // lit chamfer along the top slope
        edge.startNewSubPath(r.getX(), topAt(r.getX()) + 1.0f);
        edge.lineTo(r.getRight(), topAt(r.getRight()) + 1.0f);
        g.setColour(Colour(0xffc08858).withAlpha(left ? 0.42f : 0.28f));
        g.strokePath(edge, juce::PathStrokeType(1.4f));
        juce::Path joint;
        joint.startNewSubPath(r.getX(), topAt(r.getX()) + 18.0f);
        joint.lineTo(r.getRight(), topAt(r.getRight()) + 18.0f);
        g.setColour(black(0.75f));
        g.strokePath(joint, juce::PathStrokeType(1.4f));
    }
    {
        // the rounded outer corner catches the light as a thin bevel that runs down the outer edge
        juce::Path bevel;
        if (left)
        {
            bevel.startNewSubPath(r.getX() + 1.4f, topAt(r.getX()) + corner + 40.0f);
            bevel.lineTo(r.getX() + 1.4f, topAt(r.getX()) + corner);
            bevel.quadraticTo(r.getX() + 1.4f, topAt(r.getX()) + 1.4f, r.getX() + corner, topAt(r.getX() + corner) + 1.4f);
        }
        else
        {
            bevel.startNewSubPath(r.getRight() - corner, topAt(r.getRight() - corner) + 1.4f);
            bevel.quadraticTo(r.getRight() - 1.4f, topAt(r.getRight()) + 1.4f, r.getRight() - 1.4f, topAt(r.getRight()) + corner);
            bevel.lineTo(r.getRight() - 1.4f, topAt(r.getRight()) + corner + 40.0f);
        }
        g.setColour(Colour(0xffe2aa78).withAlpha(left ? 0.50f : 0.22f));
        g.strokePath(bevel, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    }
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
        // Dark cuts in the walnut with a faint lit lip, so it is nearly invisible at rest and warms up on hover.
        const auto ink = isMouseDragging ? juce::Colour(0xffe0b068).withAlpha(0.9f)
                       : isMouseOver ? juce::Colour(0xffc89850).withAlpha(0.8f)
                                     : juce::Colour(0xff1a0c06).withAlpha(0.55f);
        for (float t : { 0.35f, 0.62f, 0.88f })
        {
            const juce::Line<float> line((float) w - s * t, (float) h, (float) w, (float) h - s * t);
            g.setColour(juce::Colour(0xffd09a68).withAlpha(0.14f));
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
// 24 Hz driver for the output meter and nothing else, so the level bar moves smoothly while the rest of the
// panel polls at 8 Hz.
class MeterTicker final : private juce::Timer
{
public:
    MeterTicker() { startTimerHz(24); }
    ~MeterTicker() override { stopTimer(); }
    std::function<void()> onTick;
private:
    void timerCallback() override { if (onTick) onTick(); }
};

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
    // key: toolbar key; previous/next: transport arrows; alert: red lens-cap pushbutton; plate: dark plate key;
    // accent: gold primary key (SAVE); quiet: low-key but enabled-looking key (INIT); satin: the software
    // keycap family with a lamp (memory lid, voice settings); selector: a gold-lit choice among several.
    enum class Style { key, previous, next, alert, plate, accent, quiet, satin, selector };
    enum class Glyph { none, gear };
    explicit KeyButton(const juce::String& text, Style s = Style::key) : juce::TextButton(text), style(s) {}
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
            const float size = juce::jlimit(8.0f, 15.0f, r.getHeight() * (style == Style::plate ? 0.40f : style == Style::quiet ? 0.44f : 0.37f));
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
            font = ui::fitFontToWidth(font, shown, area.getWidth(), 0.85f, 8.0f);
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
        auto font = displayFont(juce::jlimit(8.0f, 15.0f, lens.getHeight() * 0.42f), juce::Font::bold);
        font.setExtraKerningFactor(0.10f);
        font = ui::fitFontToWidth(font, getButtonText(), lens.getWidth() - 8.0f, 0.85f, 8.0f);
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
        auto font = displayFont(juce::jlimit(8.0f, 11.5f, face.getHeight() * 0.46f), juce::Font::bold);
        font.setExtraKerningFactor(0.03f);
        font = ui::fitFontToWidth(font, getButtonText(), textArea.getWidth(), 0.85f, 8.0f);
        ui::drawEngravedText(g, getButtonText(), textArea, font, juce::Justification::centredLeft,
                             on ? ui::amberBright : ui::ivory.withAlpha(0.92f));
        paintFocus(g, 4.0f);
    }

    Style style;
    Glyph glyph = Glyph::none;
    bool emphasis = false;
    bool armed = false;
};

// A Label that honours its own font (the shared look-and-feel derives label fonts
// from the component height). It can show a compact player-facing string while the
// full engineering text stays in getText() for accessibility and tooltips; the
// compact string may be deliberately blank.
class PanelLabel final : public juce::Label
{
public:
    PanelLabel() { setMinimumHorizontalScale(0.85f); }
    void setMinimumPixels(float px) { minPixels = px; }
    void setLines(int n) { lines = n; }
    void setLeftPad(int px) { leftPad = px; }
    void setShadow(float alpha) { shadow = alpha; }
    void setDisplayText(const juce::String& text)
    {
        if (! hasDisplay || text != display) { display = text; hasDisplay = true; repaint(); }
    }
    // A status dot before the text; pass a transparent colour to hide it.
    void setDot(juce::Colour c)
    {
        if (c != dot) { dot = c; repaint(); }
    }
    // A small circled "i" before the text: the tooltip carries the detail.
    void setInfoGlyph(bool value) { if (value != info) { info = value; repaint(); } }
    // What the label prints, and how much room the text, its dot and its glyph take at the given font height.
    juce::String shownText() const { return hasDisplay ? display : getText(); }
    float contentWidth(const juce::Font& font) const
    {
        const auto text = shownText();
        if (text.isEmpty()) return 0.0f;
        return textWidth(font, text) + (dot.isTransparent() ? 0.0f : 11.0f) + (info ? 14.0f : 0.0f) + (float) leftPad;
    }
    void paint(juce::Graphics& g) override
    {
        auto f = getFont();
        if (minPixels > 0.0f && f.getHeight() < minPixels) f = f.withHeight(minPixels);
        auto area = getLocalBounds().withTrimmedLeft(leftPad);
        const float cy = (float) area.getCentreY();
        if (! dot.isTransparent())
        {
            g.setColour(dot.withAlpha(0.30f));
            g.fillEllipse(0.5f, cy - 4.0f, 8.0f, 8.0f);
            g.setColour(dot);
            g.fillEllipse(2.0f, cy - 2.5f, 5.0f, 5.0f);
            area.removeFromLeft(11);
        }
        if (info)
        {
            g.setColour(juce::Colour(0xffb8aa7c));
            g.drawEllipse(0.8f, cy - 4.6f, 9.2f, 9.2f, 1.0f);
            g.setFont(displayFont(8.0f, juce::Font::bold));
            g.drawText("i", juce::Rectangle<float>(0.8f, cy - 4.6f, 9.2f, 9.2f), juce::Justification::centred, false);
            area.removeFromLeft(14);
        }
        const auto text = hasDisplay ? display : getText();
        if (text.isEmpty()) return;
        f = ui::fitFontToWidth(f, text, (float) area.getWidth(), getMinimumHorizontalScale(), minPixels > 0.0f ? minPixels : 8.0f);
        g.setFont(f);
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
    int leftPad = 0;
    int lines = 1;
    bool hasDisplay = false;
    bool info = false;
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
    // Width of the OVR lamp at the given scale (it grows when the type floor grows).
    static float lampWidthNow() { return juce::jmax(23.0f, textWidth(displayFont(8.0f, juce::Font::bold), "OVR") + 7.0f); }
    static float lampWidth(float scale)
    {
        FloorScope floor(scale);
        return lampWidthNow();
    }
    void paint(juce::Graphics& g) override
    {
        const float sx = (float) getWidth() / geo::meterBar.getWidth();
        const float sy = (float) getHeight() / geo::meterBar.getHeight();
        FloorScope floor(sx);
        g.addTransform(juce::AffineTransform::scale(sx, sy));
        const float w = geo::meterBar.getWidth(), h = geo::meterBar.getHeight();
        const float lampW = lampWidth(sx);
        const float pitch = (w - lampW - 2.0f) / (float) segments;
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
        const auto lamp = juce::Rectangle<float>(w - lampW, 0.0f, lampW, h);
        const bool alarm = over || nonFinite;
        const juce::Colour hue = nonFinite ? juce::Colour(0xffff3b30) : juce::Colour(0xffff8a2e);
        if (alarm)
        {
            g.setColour(hue.withAlpha(0.45f));
            g.fillRoundedRectangle(lamp.expanded(2.5f), 4.0f);
        }
        g.setGradientFill(art::vgrad(alarm ? hue.brighter(0.5f) : juce::Colour(0xff5a2a1e), alarm ? hue : juce::Colour(0xff2a140c), lamp));
        g.fillRoundedRectangle(lamp, 2.5f);
        g.setColour(art::black(0.8f));
        g.drawRoundedRectangle(lamp, 2.5f, 0.8f);
        g.setColour(alarm ? juce::Colour(0xff2a0a05) : juce::Colour(0xffe0a88c));
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
    explicit ActivityLamps(Cs80AudioProcessor& p) : processor(p), lastEventCount(p.getHostMidiEventCount())
    {
        setInterceptsMouseClicks(true, false);          // only so hovering shows the description
        setComponentID("activity_lamps");
        setTitle("Voice and MIDI activity");
        setDescription("Eight lamps follow the sounding voices (up to eight at once). The MIDI lamp flashes for any MIDI the host sends: notes, controllers, pitch bend and pressure.");
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
        const float sx = (float) getWidth() / geo::lampWell.getWidth();
        FloorScope floor(sx);
        g.addTransform(juce::AffineTransform::scale(sx, (float) getHeight() / geo::lampWell.getHeight()));
        art::legend(g, "VOICES", { 10.0f, 2.0f, 70.0f, 13.0f }, spacedFont(9.5f, 0.10f), juce::Justification::centredLeft,
                    juce::Colour(0xffd8c68a), 0.8f);
        art::legend(g, "MIDI", { 88.0f, 2.0f, 38.0f, 13.0f }, spacedFont(9.5f, 0.10f), juce::Justification::centred,
                    juce::Colour(0xffd8c68a), 0.8f);
        for (int i = 0; i < 8; ++i)
            lamp(g, { 14.0f + (float) i * 10.5f, 27.0f }, 3.9f, ((shown.mask >> i) & 1u) != 0, juce::Colour(0xffffb53a));
        lamp(g, { 108.0f, 27.0f }, 4.4f, shown.midi, juce::Colour(0xff5cf08a));
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
        // Any host MIDI event (note, CC, bend, pressure) bumps the processor's counter; a held note keeps it lit.
        bool held = false;
        for (int note = 0; note < 128 && ! held; ++note)
            held = processor.isHostKeyDown(note);
        const unsigned events = processor.getHostMidiEventCount();
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (held || events != lastEventCount) lastMidiMs = now;
        lastEventCount = events;
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
    unsigned lastEventCount = 0;
};

// The program display is also a button: hovering lights it, clicking opens the library. It draws only
// the hover glow, chevron and the EDITED tag; the glass is part of the buffered chassis and the text is
// carried by labels.
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
        const float sx = (float) getWidth() / geo::presetWell.getWidth();
        FloorScope floor(sx);
        g.addTransform(juce::AffineTransform::scale(sx, (float) getHeight() / geo::presetWell.getHeight()));
        const auto w = juce::Rectangle<float>(0.0f, 0.0f, geo::presetWell.getWidth(), geo::presetWell.getHeight());
        if (hovered || down)
        {
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffb347).withAlpha(down ? 0.30f : 0.20f), w.getCentreX(), w.getCentreY(),
                                                   juce::Colours::transparentBlack, w.getX(), w.getCentreY(), true));
            g.fillRoundedRectangle(w.reduced(1.0f), 4.0f);
            g.setColour(juce::Colour(0xffffcf70).withAlpha(0.55f));
            g.drawRoundedRectangle(w.reduced(0.8f), 4.0f, 1.0f);
        }
        if (dirty)
        {
            // A small "EDITED" tag at the right end of the name row. (The well itself is the control: it has no
            // chevron, and lights up under the pointer.)
            const auto font = spacedFont(9.0f, 0.10f);
            const float tw = textWidth(font, "EDITED") + 8.0f;
            const juce::Rectangle<float> tag(geo::presetWell.getWidth() - tw - 10.0f,
                                             geo::presetName.getBottom() - geo::presetWell.getY() - 14.0f, tw, 12.0f);
            g.setColour(juce::Colour(0xffffa530).withAlpha(0.16f));
            g.fillRoundedRectangle(tag, 2.5f);
            g.setColour(juce::Colour(0xffffb347).withAlpha(0.85f));
            g.drawRoundedRectangle(tag.reduced(0.5f), 2.5f, 0.9f);
            art::legend(g, "EDITED", tag, font, juce::Justification::centred, juce::Colour(0xffffcf70), 0.0f);
        }
    }
private:
    bool dirty = false;
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
        const float sx = (float) getWidth() / geo::plate.getWidth();
        FloorScope floor(sx);
        g.addTransform(juce::AffineTransform::scale(sx, (float) getHeight() / geo::plate.getHeight()));
        paintCaption(g);
        paintFlow(g);
        if (scopes)
        {
            paintVcfScope(g, { 14.0f, 112.0f, 160.0f, 116.0f });
            paintVcaScope(g, { 186.0f, 112.0f, 160.0f, 116.0f });
            paintFilterScope(g, { 14.0f, 236.0f, 332.0f, 90.0f });
        }
        // a matte window: no gloss, only a one-pixel inner shadow along the top and left lip
        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path window;
            window.addRoundedRectangle(juce::Rectangle<float>(7.0f, 7.0f, 346.0f, 364.0f), 3.0f);
            g.reduceClipRegion(window);
            // a one-pixel inner shadow along the top and left lip of the glass
            g.setColour(art::black(0.65f));
            g.fillRect(7.0f, 7.0f, 346.0f, 1.0f);
            g.fillRect(7.0f, 7.0f, 1.0f, 364.0f);
        }
    }

private:
    using Rf = juce::Rectangle<float>;
    static juce::Colour ink() { return juce::Colour(0xfff0e6c4); }
    static juce::Colour gold() { return juce::Colour(0xffcdb878); }
    static juce::Colour goldDim() { return juce::Colour(0xff9a8a5c); }
    static juce::Colour curve() { return juce::Colour(0xfff2dc98); }
    static juce::Colour plate() { return art::glassColour(); }
    static juce::Colour field() { return art::glassColour().darker(0.30f); }

    float p(int local) const { return snap.v[(size_t) local]; }

    // Compact readouts so eight-pixel type fits a 160 px scope.
    static juce::String time(float seconds)
    {
        const int ms = juce::roundToInt(seconds * 1000.0f);
        if (ms < 1000) return juce::String(ms) + "ms";            // never "1000ms": a second is "1.00s"
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
        const float y = 62.0f, h = 30.0f, w = 62.0f;
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
            // Name above, and under it what the stage is doing: the oscillator waves that sound, the cutoff, the level.
            // Small editors have no room for the second line and print the names alone.
            juce::String detail;
            if (i == 0)
            {
                juce::StringArray waves;
                if (p(cs80::CP_saw) > 0.02f) waves.add("SAW");
                if (p(cs80::CP_pulse) > 0.02f) waves.add("PLS");
                if (p(cs80::CP_noise) > 0.02f) waves.add("NSE");
                detail = waves.isEmpty() ? juce::String("---") : waves.joinIntoString("+");
            }
            else if (i == 1) detail = active[i] ? hertz(p(cs80::CP_hpfCutoff)) + "Hz" : juce::String("OFF");
            else if (i == 2) detail = active[i] ? hertz(p(cs80::CP_lpfCutoff)) + "Hz" : juce::String("OFF");
            else detail = juce::String(juce::roundToInt(juce::jlimit(0.0f, 1.0f, p(cs80::CP_vLevel)) * 100.0f)) + "%";
            const bool two = ! gSmallEditor;
            art::haloText(g, names[i], two ? box.withTrimmedBottom(h * 0.40f).translated(0.0f, 1.0f) : box, spacedFont(11.0f, 0.10f),
                          juce::Justification::centred, active[i] ? ink() : juce::Colour(0xff8a8168), juce::Colours::transparentBlack);
            if (two)
                art::haloText(g, detail, box.withTrimmedTop(h * 0.56f).withTrimmedBottom(1.0f), spacedFont(8.5f, 0.04f),
                              juce::Justification::centred, active[i] ? curve() : juce::Colour(0xff7c745e), juce::Colours::transparentBlack);
        }
        // sine path under the filters straight to the amplifier
        const float sine = juce::jlimit(0.0f, 1.0f, p(cs80::CP_sine));
        const float yy = y + h + 11.0f;
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
    // A scope field: a dark tint with graticule corner marks instead of a boxed outline. The readout is the first
    // candidate that fits at the current type size: two spaced rows (A, D / S, R), the same tighter, or one row.
    using Readout = std::pair<juce::String, juce::String>;
    static float rowHeight() { return juce::jmax(10.0f, gDesignFloor * 1.05f); }
    void scopeFrame(juce::Graphics& g, Rf r, const juce::String& title, const std::vector<Readout>& readouts, bool readoutOnTop)
    {
        g.setColour(field());
        g.fillRoundedRectangle(r, 2.0f);
        // graticule: hairline frame and heavier corner marks
        g.setColour(gold().withAlpha(0.16f));
        g.drawRect(r.reduced(0.5f), 0.8f);
        g.setColour(gold().withAlpha(0.60f));
        const float m = 6.0f;
        for (const auto& corner : { std::array<float, 4>{ r.getX(), r.getY(), 1.0f, 1.0f }, { r.getRight(), r.getY(), -1.0f, 1.0f },
                                    { r.getX(), r.getBottom(), 1.0f, -1.0f }, { r.getRight(), r.getBottom(), -1.0f, -1.0f } })
        {
            g.drawLine(corner[0], corner[1] + corner[3] * 0.5f, corner[0] + corner[2] * m, corner[1] + corner[3] * 0.5f, 1.0f);
            g.drawLine(corner[0] + corner[2] * 0.5f, corner[1], corner[0] + corner[2] * 0.5f, corner[1] + corner[3] * m, 1.0f);
        }
        const auto titleFont = spacedFont(8.0f, 0.12f);
        const float titleW = textWidth(titleFont, title);
        art::legend(g, title, { r.getX() + 6.0f, r.getY() + 3.0f, r.getWidth() - 12.0f, rowHeight() }, titleFont,
                    juce::Justification::centredLeft, gold(), 0.0f);
        const auto font = spacedFont(8.0f, 0.05f);
        const float avail = readoutOnTop ? r.getWidth() - 12.0f - titleW - 8.0f : r.getWidth() - 10.0f;
        for (const auto& candidate : readouts)
        {
            if (candidate.first.isEmpty()) continue;
            if (textWidth(font, candidate.first) > avail || textWidth(font, candidate.second) > avail) continue;
            if (readoutOnTop)
                art::legend(g, candidate.first, { r.getX() + 6.0f, r.getY() + 3.0f, r.getWidth() - 12.0f, rowHeight() }, font,
                            juce::Justification::centredRight, juce::Colour(0xffe2d8b4), 0.0f);
            else
            {
                const float rh = rowHeight();
                const bool two = candidate.second.isNotEmpty();
                art::legend(g, candidate.first, { r.getX() + 5.0f, r.getBottom() - rh * (two ? 2.0f : 1.0f) - 2.0f, r.getWidth() - 10.0f, rh }, font,
                            juce::Justification::centred, juce::Colour(0xffe2d8b4), 0.0f);
                if (two)
                    art::legend(g, candidate.second, { r.getX() + 5.0f, r.getBottom() - rh - 2.0f, r.getWidth() - 10.0f, rh }, font,
                                juce::Justification::centred, juce::Colour(0xffe2d8b4), 0.0f);
            }
            break;
        }
    }

    // The plot leaves a strip under it for the stage letters (they never sit on the curve) and the readout below that.
    Rf plotArea(Rf r, float gutter = 0.0f) const
    {
        const float below = 2.0f * rowHeight() + 3.0f + (gSmallEditor ? 0.0f : 10.0f);
        return { r.getX() + 8.0f + gutter, r.getY() + rowHeight() + 6.0f, r.getWidth() - 16.0f - gutter, r.getHeight() - rowHeight() - 6.0f - below };
    }

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
        // Etched gold, not phosphor: the trace is the panel's own line art at about 60 percent, with a dark
        // shadow offset so it reads as cut into the plate.
        g.setColour(curve().withAlpha(0.09f));
        g.fillPath(fill);
        g.setColour(art::black(0.55f));
        g.strokePath(line, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                     juce::AffineTransform::translation(0.7f, 1.0f));
        g.setColour(curve().withAlpha(0.62f));
        g.strokePath(line, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void stageTicks(juce::Graphics& g, Rf plot, const Stages& s, const char* letters)
    {
        const float xs[] = { s.x0, s.xA, s.xD, s.xS, s.xR };
        g.setColour(gold().withAlpha(0.22f));
        const float dash[] = { 1.5f, 2.5f };
        for (int i = 1; i < 4; ++i)
            g.drawDashedLine(juce::Line<float>(xs[i], plot.getY(), xs[i], plot.getBottom()), dash, 2, 0.8f);
        if (gSmallEditor) return;
        for (int i = 0; i < 4; ++i)
            art::legend(g, juce::String::charToString((juce::juce_wchar) letters[i]),
                        { xs[i], plot.getBottom() + 1.0f, xs[i + 1] - xs[i], 9.0f }, displayFont(8.0f, juce::Font::bold),
                        juce::Justification::centred, gold(), 0.0f);
    }

    void paintVcfScope(juce::Graphics& g, Rf r)
    {
        const float il = p(cs80::CP_fIl), al = p(cs80::CP_fAl);
        const float a = p(cs80::CP_fA), d = p(cs80::CP_fD), rel = p(cs80::CP_fR);
        scopeFrame(g, r, "VCF ENVELOPE",
                   { { "A " + time(a) + "   D " + time(d), "S 0%   R " + time(rel) },
                     { "A " + time(a) + " D " + time(d), "S 0% R " + time(rel) },
                     { "A " + time(a) + " D " + time(d) + " R " + time(rel), {} } }, false);
        const auto plot = plotArea(r, gSmallEditor ? 0.0f : 14.0f);       // IL gutter: the tag sits left of the start dot
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
        marker(g, { s.x0, yOf(v0) }, gSmallEditor ? "" : "IL", plot, true);
        marker(g, { s.xA, yOf(vA) }, gSmallEditor ? "" : "AL", plot, false);
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
        // IL lives in the gutter left of the plot; AL sits above-left of the peak, clamped inside the plot.
        Rf tag(start ? plot.getX() - 15.0f : c.x - 17.0f, start ? c.y - 4.5f : c.y - 12.0f, 14.0f, 9.0f);
        if (! start)
        {
            if (tag.getY() < plot.getY()) tag.setY(c.y + 3.0f);
            tag.setX(juce::jlimit(plot.getX() + 1.0f, plot.getRight() - tag.getWidth() - 1.0f, tag.getX()));
        }
        art::haloText(g, text, tag, displayFont(8.0f, juce::Font::bold),
                      start ? juce::Justification::centredRight : juce::Justification::centredRight, ink(), field());
    }

    void paintVcaScope(juce::Graphics& g, Rf r)
    {
        const float a = p(cs80::CP_vA), d = p(cs80::CP_vD), s = juce::jlimit(0.0f, 1.0f, p(cs80::CP_vS)), rel = p(cs80::CP_vR);
        const auto sus = juce::String(juce::roundToInt(s * 100.0f)) + "%";
        scopeFrame(g, r, "VCA ENVELOPE",
                   { { "A " + time(a) + "   D " + time(d), "S " + sus + "   R " + time(rel) },
                     { "A " + time(a) + " D " + time(d), "S " + sus + " R " + time(rel) },
                     { "A " + time(a) + " D " + time(d) + " R " + time(rel), {} } }, false);
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
        const auto hpText = hpOn ? "HPF " + hertz(hpf) + "Hz" : juce::String("HPF OFF");
        const auto lpText = lpOn ? "LPF " + hertz(lpf) + "Hz" : juce::String("LPF OFF");
        scopeFrame(g, r, "FILTER RESPONSE", { { hpText + "   " + lpText, {} }, { hpText + " " + lpText, {} },
                                              { (hpOn ? "HP " + hertz(hpf) : juce::String("HP OFF")) + " " + (lpOn ? "LP " + hertz(lpf) : juce::String("LP OFF")), {} } }, true);
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
            if (juce::String(tag).isNotEmpty())
                art::haloText(g, tag, { x - 5.0f, y - 15.0f, 10.0f, 9.0f }, displayFont(8.0f, juce::Font::bold),
                              juce::Justification::centred, ink(), field());
        };
        cutoffMark(hpf, gSmallEditor ? "" : "H", hpOn);
        cutoffMark(lpf, gSmallEditor ? "" : "L", lpOn);
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

// The lid's live readout: a small black window that names the control under the pointer (or being dragged)
// with its value, and otherwise says which source each channel is playing. It repaints on its own, so
// dragging a slider never repaints the whole lid.
class LidReadout final : public juce::Component
{
public:
    LidReadout() { setInterceptsMouseClicks(false, false); setOpaque(false); setTitle("Memory readout"); }
    void setLines(const juce::String& main, const juce::String& hint)
    {
        if (main != mainLine || hint != hintLine) { mainLine = main; hintLine = hint; repaint(); }
    }
    void paint(juce::Graphics& g) override
    {
        const float sx = (float) getWidth() / 730.0f, sy = (float) getHeight() / 44.0f;
        FloorScope floor(sx);
        g.addTransform(juce::AffineTransform::scale(sx, sy));
        const juce::Rectangle<float> w(0.0f, 0.0f, 730.0f, 44.0f);
        art::recessTray(g, w.reduced(1.0f), 4.0f, juce::Colour(0xff0e0d0b), juce::Colour(0xff050504), 7.0f, 0.9f);
        g.setGradientFill(art::vgrad(art::white(0.06f), juce::Colours::transparentWhite, w.withHeight(22.0f)));
        g.fillRoundedRectangle(w.reduced(2.0f).withHeight(21.0f), 3.0f);
        auto font = spacedFont(14.5f, 0.06f);
        font = ui::fitFontToWidth(font, mainLine, 702.0f, 0.8f, 9.0f);
        g.setFont(font);
        g.setColour(juce::Colour(0xffffc060));
        g.drawText(mainLine, juce::Rectangle<float>(14.0f, 3.0f, 702.0f, 22.0f), juce::Justification::centredLeft, false);
        auto hint = spacedFont(11.0f, 0.04f);
        hint = ui::fitFontToWidth(hint, hintLine, 702.0f, 0.8f, 9.0f);
        g.setFont(hint);
        g.setColour(juce::Colour(0xffb6aa86));
        g.drawText(hintLine, juce::Rectangle<float>(14.0f, 26.0f, 702.0f, 15.0f), juce::Justification::centredLeft, false);
    }
private:
    juce::String mainLine, hintLine;
};

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
        addAndMakeVisible(readout);
        addAndMakeVisible(close);
        setTitle("Memory programmer");
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        close.setButtonText("CLOSE LID");
        close.setTitle("Close memory programmer");
        close.setWantsKeyboardFocus(true);
        close.onClick = [this] { if (onClose) onClose(); };
        for (int bank = 0; bank < 4; ++bank)
        {
            auto button = std::make_unique<KeyButton>("MEMORY " + juce::String(bank + 1), KeyButton::Style::selector);
            button->setClickingTogglesState(false);
            button->setWantsKeyboardFocus(true);
            button->setTitle("Select memory " + juce::String(bank + 1));
            // A lit memory key is a toggle: pressing it again hands its channel back to the PANEL.
            button->onClick = [this, bank]
            {
                const int channel = bank % 2, source = bank < 2 ? 11 : 12;
                processor.selectHardwareSource(channel, processor.getHardwareSource(channel) == source ? 13 : source);
                refresh();
            };
            button->setTooltip("Listen to this independent memory on channel " + juce::String(bank % 2 + 1)
                               + ". Press it again while lit to return the channel to PANEL.");
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
                    toggle->addMouseListener(this, false);
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
                    slider->addMouseListener(this, false);
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
        bool changed = false;
        for (int bank = 0; bank < 4; ++bank)
        {
            const bool active = processor.getHardwareSource(bank % 2) == (bank < 2 ? 11 : 12);
            bankButtons[(size_t) bank]->setToggleState(active, juce::dontSendNotification);
            if (active != rowActive[(size_t) bank]) { rowActive[(size_t) bank] = active; changed = true; }
        }
        if (changed)
        {
            // Rows that are not the sounding memory sit back; the one that is heard stands out.
            for (auto& item : items)
            {
                const float alpha = rowActive[(size_t) item.bank] ? 1.0f : 0.62f;
                if (item.slider) item.slider->setAlpha(alpha);
                if (item.toggle) item.toggle->setAlpha(alpha);
            }
            repaint();
        }
        if (hovered != nullptr) describe(*hovered, false);
        else if (lastTouched != nullptr) describe(*lastTouched, true);
        else showIdle();
    }

    // -- live readout ----------------------------------------------------------------------
    void mouseEnter(const juce::MouseEvent& e) override { track(e); }
    void mouseMove(const juce::MouseEvent& e) override { track(e); }
    void mouseDrag(const juce::MouseEvent& e) override { track(e); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (auto* item = findItem(e.eventComponent)) { lastTouched = item; hovered = item; describe(*item, false); }
    }
    void mouseExit(const juce::MouseEvent& e) override
    {
        if (findItem(e.eventComponent) != nullptr)
        {
            hovered = nullptr;
            if (lastTouched != nullptr) describe(*lastTouched, true); else showIdle();
        }
    }

    void paint(juce::Graphics& g) override
    {
        const float scale = (float) getWidth() / kW;
        FloorScope floor(scale);
        g.saveState();
        g.addTransform(juce::AffineTransform::scale(scale, (float) getHeight() / kH));
        const juce::Rectangle<float> lid(0.0f, geo::lidTop, kW, geo::lidHeight);

        // Shadow of the raised lid on the deck below it, kept short so it does not smear over the keys.
        ui::softShadow(g, lid, 9.0f, 0.0f, 9.0f, 15.0f, 0.85f);
        g.setGradientFill(art::vgrad(art::black(0.32f), juce::Colours::transparentBlack, { 0.0f, lid.getBottom(), kW, kH - lid.getBottom() }));
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

        // Machined rim, then the plate set into it: the same crinkle finish as the instrument panel.
        g.setGradientFill(art::vgrad(juce::Colour(0xff55514a), juce::Colour(0xff1c1a16), lid));
        g.fillRoundedRectangle(lid, 9.0f);
        g.setColour(art::white(0.30f));
        g.drawLine(10.0f, lid.getY() + 1.5f, kW - 10.0f, lid.getY() + 1.5f, 1.0f);
        g.setColour(art::black(0.85f));
        g.drawRoundedRectangle(lid, 9.0f, 1.0f);
        const auto inner = lid.reduced(9.0f);
        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path clip;
            clip.addRoundedRectangle(inner, 5.0f);
            g.reduceClipRegion(clip);
            art::fillTile(g, art::panelTile(), inner, scale);
            g.setGradientFill(juce::ColourGradient(art::white(0.045f), inner.getX() + 160.0f, inner.getY(), juce::Colours::transparentWhite,
                                                   inner.getX() + 900.0f, inner.getBottom(), true));
            g.fillRect(inner);
            g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, inner.getX() + 500.0f, inner.getY() + 100.0f,
                                                   art::black(0.12f), inner.getRight(), inner.getBottom(), false));
            g.fillRect(inner);
        }
        art::insetShadow(g, inner, 5.0f, 12.0f, 0.5f);
        art::etchRect(g, inner.reduced(7.0f), 3.0f, 0.55f);
        for (float sx : { lid.getX() + 5.0f, lid.getRight() - 5.0f })
            for (float sy : { lid.getY() + 5.0f, lid.getBottom() - 5.0f })
                art::slotScrew(g, { sx, sy }, 3.0f, sx + sy);
        // Front rail: two latch catches that hold the lid down.
        for (float lx : { 330.0f, kW - 330.0f })
        {
            const juce::Rectangle<float> catchPlate(lx - 42.0f, lid.getBottom() - 9.0f, 84.0f, 13.0f);
            ui::softShadow(g, catchPlate, 3.0f, 0.0f, 2.0f, 4.0f, 0.7f);
            art::brushedMetal(g, catchPlate, 3.0f, 0.3f, (int) lx);
            g.setColour(art::black(0.85f));
            g.drawRoundedRectangle(catchPlate, 3.0f, 1.0f);
            art::slotScrew(g, { catchPlate.getX() + 11.0f, catchPlate.getCentreY() }, 2.5f, lx);
            art::slotScrew(g, { catchPlate.getRight() - 11.0f, catchPlate.getCentreY() }, 2.5f, lx + 3.0f);
            g.setColour(art::black(0.7f));
            g.fillRoundedRectangle(catchPlate.getCentreX() - 14.0f, catchPlate.getY() + 3.5f, 28.0f, 6.0f, 2.5f);
        }

        // Everything below is laid out in the programmer's own 1472 x 554 frame.
        g.addTransform(juce::AffineTransform::translation(kOffsetX, geo::lidTop));
        art::legend(g, "MEMORY PROGRAMMER", { 34, 14, 520, 26 }, spacedFont(19.0f, 0.16f),
                    juce::Justification::centredLeft, juce::Colour(0xffe9d695));
        art::legend(g, "FOUR INDEPENDENT MINIATURE PANELS", { 35, 41, 520, 14 },
                    spacedFont(10.5f, 0.12f), juce::Justification::centredLeft, juce::Colour(0xffcfc4a2));

        // One header strip for all four memories: group brackets and a caption over every column, so each
        // row carries only its sliders. The columns line up exactly with the rows below.
        {
            art::etchRect(g, { 24.0f, 62.0f, 1424.0f, 50.0f }, 4.0f, 0.6f);
            const struct { const char* title; int first, last; } groups[] = {
                { "VCO", 0, 5 }, { "VCF", 6, 14 }, { "VCA", 15, 21 }, { "INITIAL", 22, 23 }, { "AFTER", 24, 25 } };
            for (const auto& group : groups)
            {
                const float x0 = colX(group.first) + 1.0f, x1 = colX(group.last) + 47.0f;
                art::bracket(g, group.title, x0, 75.0f, x1 - x0, 4.0f, 11.0f);
            }
            for (int col = 0; col < 26; ++col)
                art::legend(g, labels[(size_t) col], { colX(col), 82.0f, 48.0f, 27.0f }, displayFont(11.5f, juce::Font::bold),
                            juce::Justification::centred, juce::Colour(0xffeee4c4), 0.7f);
        }
        for (int bank = 0; bank < 4; ++bank)
        {
            const float y = rowY(bank);
            const bool active = rowActive[(size_t) bank];
            const juce::Rectangle<float> row(24.0f, y, 1424.0f, 98.0f);
            if (active)
            {
                // The heard row: a lit ring, a soft glow and a mark at its left end.
                g.setColour(juce::Colour(0xffffc060).withAlpha(0.10f));
                g.fillRoundedRectangle(row.reduced(1.0f), 4.0f);
                g.setColour(juce::Colour(0xffffc060).withAlpha(0.28f));
                g.drawRoundedRectangle(row.expanded(2.0f), 6.0f, 3.0f);
                art::etchRect(g, row, 4.0f, 1.0f);
                g.setColour(juce::Colour(0xffffc060));
                g.fillRoundedRectangle(row.getX() + 3.0f, row.getY() + 12.0f, 3.0f, row.getHeight() - 24.0f, 1.5f);
            }
            else
                art::etchRect(g, row, 4.0f, 0.34f);
            art::legend(g, bank % 2 == 0 ? "CHANNEL I" : "CHANNEL II", { 33.0f, y + 10.0f, 100.0f, 16.0f },
                        spacedFont(11.0f, 0.12f), juce::Justification::centred, juce::Colour(active ? 0xfffff0cc : 0xffb8ae8e));
            art::legend(g, active ? "HEARD" : "NOT ACTIVE", { 33.0f, y + 72.0f, 100.0f, 14.0f },
                        spacedFont(10.0f, 0.10f), juce::Justification::centred, juce::Colour(active ? 0xffffc060 : 0xff9a9174), 0.6f);
        }
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
        readout.setBounds(r(570, 12, 730, 44));
        for (int bank = 0; bank < 4; ++bank)
        {
            const float y = rowY(bank);
            bankButtons[(size_t) bank]->setBounds(r(34, y + 36, 98, 32));
            for (int col = 0; col < 26; ++col)
            {
                auto& item = items[(size_t)(bank * 26 + col)];
                if (item.slider) item.slider->setBounds(r(colX(col), y + 6, 48, 86));
                if (item.toggle) item.toggle->setBounds(r(colX(col) + 8.0f, y + 20, 32, 58));
            }
        }
    }
    std::function<void()> onClose;

private:
    static constexpr float kW = 1504.0f, kH = 590.0f, kOffsetX = 16.0f;

    struct Item;
    Item* findItem(const juce::Component* c)
    {
        if (c == nullptr) return nullptr;
        for (auto& item : items)
            if (c == item.slider.get() || c == item.toggle.get()) return &item;
        return nullptr;
    }
    void track(const juce::MouseEvent& e)
    {
        if (auto* item = findItem(e.eventComponent)) { hovered = item; describe(*item, false); }
    }
    static juce::String sourceName(int source, int channel)
    {
        return source < 11 ? juce::String(cs80::hardwareToneName(source, channel)).toUpperCase()
             : source == 13 ? juce::String("PANEL") : "MEMORY " + juce::String(channel + (source == 12 ? 3 : 1));
    }
    void showIdle()
    {
        const bool anyActive = rowActive[0] || rowActive[1] || rowActive[2] || rowActive[3];
        readout.setLines("CH I: " + sourceName(processor.getHardwareSource(0), 0) + "      CH II: " + sourceName(processor.getHardwareSource(1), 1),
                         anyActive ? "HOVER OR DRAG A CONTROL TO READ ITS VALUE" : "BOTH CHANNELS ON PANEL  /  PRESS A MEMORY KEY TO HEAR ITS ROW");
    }
    void describe(const Item& item, bool lastTouch)
    {
        const juce::String value = item.slider != nullptr
            ? ui::formatParamValue(cs80::chParam(item.bank % 2, item.local), (float) item.slider->getValue())
            : (item.toggle->getToggleState() ? juce::String("ON") : juce::String("OFF"));
        const bool active = rowActive[(size_t) item.bank];
        const juce::String hint = ! active ? "NOT ACTIVE - PRESS MEMORY " + juce::String(item.bank + 1) + " TO HEAR THIS ROW"
                                : lastTouch ? juce::String("LAST TOUCHED  /  DRAG TO CHANGE  /  DOUBLE-CLICK FOR DEFAULT")
                                            : juce::String("DRAG TO CHANGE  /  DOUBLE-CLICK FOR DEFAULT");
        readout.setLines("MEMORY " + juce::String(item.bank + 1) + "  /  CH " + (item.bank % 2 == 0 ? "I" : "II") + "  /  "
                             + panelName(item.local) + "   " + value, hint);
    }
    // The front-panel name of a channel control (the lid prints the same names as the main panel).
    static juce::String panelName(int local)
    {
        if (local == cs80::CP_vcfLevel) return "VCF LV";
        return juce::String(cs80::kChannelDefs[local].name).toUpperCase();
    }
    LidReadout readout;
    Item* hovered = nullptr;
    Item* lastTouched = nullptr;
    std::array<bool, 4> rowActive { false, false, false, false };
    static float rowY(int bank) { return 118.0f + (float) bank * 104.0f; }
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
        "IL","AL","A","D","R","VCF LV","SINE","A","D","S","R","LEVEL",
        "BRILL","LEVEL","BRILL","LEVEL"
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
        addAndMakeVisible(spill);
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
        browser.isPatchEdited = [this] { return presetDirty; };
        browser.onPresetLoaded = [this]
        {
            syncPresetDisplay();
            syncToneMarks();
            repaint();
        };
        frames.onFrame = [this] { stepOverlayAnimation(); };
        for (int channel = 0; channel < 2; ++channel)
            touchViews[(size_t) channel].onTouch = [this, channel] { showChannel(channel); };
        meterTicker.onTick = [this] { if (isShowing()) updateMeter(); };

        // Host state/program changes may arrive on non-message threads.
        // The existing message-thread timer polls metadata and selectors;
        // never install a processor callback that captures this editor.
        syncPresetDisplay();
        syncToneMarks();
        syncSubMode();
        syncSourceStatus();
        syncVoicePlate();

        // [ws8-studio] begin: Studio toolbar, voice monitor drawer and controllers strip.
        studio = std::make_unique<ui::StudioMount>(processor);
        for (auto* c : std::initializer_list<juce::Component*> { &prevButton, &nextButton, &presetWindow, &browseButton, &saveButton,
                                                                 &initButton, &advancedButton, &presetLabel, &presetTagLabel,
                                                                 &presetIndexLabel, &presetNumberLabel })
            c->setVisible(false);                       // the Studio toolbar takes over this cluster
        for (auto* wheel : std::initializer_list<juce::Component*> { &pitchWheel, &modWheel })
        {
            optionComponents.erase(std::remove(optionComponents.begin(), optionComponents.end(), wheel), optionComponents.end());
            wheel->setVisible(false);                   // the wheels live in the controllers strip
        }
        addAndMakeVisible(studio->toolbar);
        addChildComponent(studio->scope);
        addChildComponent(studio->controllers);
        {
            ui::StudioToolbar::Actions actions;
            actions.presetDisplay = [this]
            {
                return ui::StudioToolbar::PresetDisplay { presetNumberLabel.getText(), presetLabel.getText(),
                                                          (presetTagLabel.shownText() + "   " + presetIndexLabel.shownText()).trim(), presetDirty };
            };
            actions.stepPreset = [this](int direction) { if (confirmDiscard(direction < 0 ? prevButton : nextButton)) stepPreset(direction); };
            actions.stepArmed = [this](int direction) { return (direction < 0 ? prevButton : nextButton).isArmed(); };
            actions.openLibrary = [this] { showBrowser(! browserVisible, true); };
            actions.libraryOpen = [this] { return browserVisible; };
            actions.selectCompare = [this](int slot) { studio->compare.select(slot); syncToneMarks(); };
            actions.activeCompare = [this] { return studio->compare.active(); };
            actions.copyCompare = [this] { studio->compare.copyToOther(); };
            actions.toggleStudio = [this] { showAdvanced(! advancedVisible, true); };
            actions.studioOpen = [this] { return advancedVisible; };
            actions.showEvidence = [this] { showAdvanced(true, true); advanced.showTab(ui::StudioTab::settings); };
            actions.savePreset = [this] { if (saveButton.onClick) saveButton.onClick(); };
            actions.initPatch = [this]
            {
                const auto& bank = processor.getFactoryPresets();
                for (int i = 0; i < (int) bank.size(); ++i)
                    if (juce::String(bank[(size_t) i].name).equalsIgnoreCase("Init Patch"))
                    {
                        processor.loadFactoryPreset(i);
                        break;
                    }
                syncToneMarks();
            };
            actions.toggleScope = [this]
            {
                studio->scope.setVisible(! studio->scope.isVisible());
                if (studio->scope.isVisible()) studio->scope.toFront(true);
            };
            actions.scopeOpen = [this] { return studio->scope.isVisible(); };
            actions.toggleControllers = [this] { studio->setControllersShown(! studio->areControllersShown()); };
            actions.controllersShown = [this] { return studio->areControllersShown(); };
            studio->toolbar.setActions(std::move(actions));
        }
        studio->scope.onClose = [this] { studio->scope.setVisible(false); };
        // [ws8-studio] end
    }

    ~Impl() override
    {
        meterTicker.onTick = nullptr;
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

    // Touching any control of a channel's row brings that channel up in the signal-flow window.
    struct TouchView final : juce::MouseListener
    {
        std::function<void()> onTouch;
        void mouseDown(const juce::MouseEvent&) override { if (onTouch) onTouch(); }
    };

    void listenForChannel(juce::Component& component, int parameter)
    {
        if (parameter >= cs80::P_numGlobals && parameter < cs80::P_legacyNumParams)
            component.addMouseListener(&touchViews[parameter < cs80::P_ch2Start ? 0u : 1u], true);
    }

    ui::ParamKnob& addKnob(int parameter, float x, float y, float w, float h,
                           const juce::String& name = {})
    {
        auto knob = std::make_unique<ui::ParamKnob>(processor, parameter, name);
        auto* raw = knob.get();
        raw->getSlider().getProperties().set("hardwareRotary", true);
        addAndMakeVisible(*raw);
        listenForChannel(*raw, parameter);
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
        listenForChannel(*raw, parameter);
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
            label->setColour(juce::Label::textColourId, stripInk());
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
        subModeLabel.setColour(juce::Label::textColourId, stripInk());
        subModeLabel.setInterceptsMouseClicks(true, false);
        addAndMakeVisible(subModeLabel);
        modeStatusLabel.setComponentID("performance_mode_status");
        modeStatusLabel.setTitle("Requested performance mode and MIDI controller policy");
        modeStatusLabel.setJustificationType(juce::Justification::centredLeft);
        modeStatusLabel.setColour(juce::Label::textColourId, stripInk());
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
            label.setColour(juce::Label::textColourId, stripInk());
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
        prevButton.onClick = [this] { if (confirmDiscard(prevButton)) stepPreset(-1); };
        nextButton.onClick = [this] { if (confirmDiscard(nextButton)) stepPreset(1); };
        prevButton.setTooltip("Previous program. If the current program has unsaved edits, press twice to discard them.");
        nextButton.setTooltip("Next program. If the current program has unsaved edits, press twice to discard them.");
        browseButton.setTooltip("Open the preset library: browse, search and load programs.");
        saveButton.setTooltip("Opens the library to save the current sound under a name.");
        initButton.setTooltip("Loads Init Patch and discards unsaved edits (press twice when the program is edited).");
        advancedButton.setTooltip("Advanced pages: engine mode, effects, routing, tuning and the other software-only settings.");
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
            if (! confirmDiscard(initButton)) return;
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

    // Printed status text on the panel: cream, bright enough to read against the crinkle at a glance.
    static juce::Colour stripInk() { return juce::Colour(0xffeee4c4); }

    ui::ParamKnob& addFader(int parameter, float x, float y, float w, float h,
                            const juce::String& label, juce::Colour colour)
    {
        auto& control = addKnob(parameter, x, y, w, h, label);
        control.setHardwareSlider(colour);
        return control;
    }

    // Painted captions that are too wide for their fader at small editor sizes get a short form there (titles,
    // tooltips and accessibility names are untouched). ParamKnob::setCaption is optional: no-op until it exists.
    struct CaptionPair { ui::ParamKnob* knob; juce::String full, shortForm; };
    std::vector<CaptionPair> captionPairs;
    template <typename Knob>
    static void setPaintedCaption(Knob& knob, const juce::String& text)
    {
        if constexpr (requires { knob.setCaption(text); }) knob.setCaption(text);
    }
    template <typename Knob>
    static void setLegendColumn(Knob& knob, float faderWidthPx)
    {
        if constexpr (requires { knob.setLegendColumnWidth(faderWidthPx); }) knob.setLegendColumnWidth(faderWidthPx);
    }
    void shortCaption(ui::ParamKnob& knob, const juce::String& full, const juce::String& shortForm)
    {
        captionPairs.push_back({ &knob, full, shortForm });
    }
    void applyCaptions()
    {
        const bool small = getWidth() < 1200;
        for (auto& pair : captionPairs) setPaintedCaption(*pair.knob, small ? pair.shortForm : pair.full);
        if (holdToggle != nullptr)
        {
            const juce::String text = small ? "HLD" : "HOLD";
            if (holdToggle->getButtonText() != text) holdToggle->setButtonText(text);
        }
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
        // The knobs share the faders' top and height; setLegendColumnWidth (applied in resized) sets their caption
        // and value on the faders' name and value lines.
        pitchKnob = &addKnob(cs80::P_masterTune, 49, y, 62, h, "PITCH");
        shortCaption(addFader(cs80::P_detune, 114, y, 39, h, "DETUNE", whiteCap()), "DETUNE", "DET");
        shortCaption(addFader(cs80::P_ringAttack, 161, y, 38, h, "ATTACK", blackCap()), "ATTACK", "ATK");
        legendReference = &addFader(cs80::P_ringDecay, 201, y, 38, h, "DECAY", blackCap());
        addFader(cs80::P_ringDepth, 241, y, 38, h, "DEPTH", greyCap());
        addFader(cs80::P_ringSpeed, 281, y, 38, h, "SPEED", whiteCap());
        addFader(cs80::P_ringAmount, 321, y, 38, h, "MOD", greyCap());
        // FUNCTION is a little wider than its neighbours, so its engraved legend would sit lower
        // than theirs. Lift the component by the difference to keep one baseline across the row.
        subFunction = &addFader(cs80::P_subWave, 373, y - 1.2f, 42, h - 0.5f, "FUNCTION", blackCap());
        shortCaption(*subFunction, "FUNCTION", "FUNC");
        subSpeed = &addFader(cs80::P_subSpeed, 417, y, 38, h, "SPEED", whiteCap());
        addFader(cs80::P_subVco, 457, y, 38, h, "VCO", whiteCap());
        addFader(cs80::P_subVcf, 497, y, 38, h, "VCF", greenCap());
        addFader(cs80::P_subVca, 537, y, 38, h, "VCA", greyCap());
        shortCaption(addFader(cs80::chParam(0, cs80::CP_feet), 582, y, 36, h, "FEET I", greyCap()), "FEET I", "FT I");
        shortCaption(addFader(cs80::chParam(1, cs80::CP_feet), 620, y, 36, h, "FEET II", greyCap()), "FEET II", "FT II");
        // Pitch follows the width of each engraved name so the legends never touch.
        addFader(cs80::P_mix, 1203, y, 38, h, "MIX", redCap());
        addFader(cs80::P_brilliance, 1241, y, 38, h, "BRILL", greenCap());
        addFader(cs80::P_resonance, 1285, y, 38, h, "RESO", redCap());
        addFader(cs80::P_initBend, 1330, y, 38, h, "BEND", whiteCap());
        addFader(cs80::P_subAtSpeed, 1374, y, 38, h, "AT SPD", whiteCap());
        addFader(cs80::P_subAtVco, 1417, y, 38, h, "AT VCO", whiteCap());
        addFader(cs80::P_subAtVcf, 1456, y, 38, h, "AT VCF", greenCap());
        volumeKnob = &addKnob(cs80::P_volume, 1497, y, 50, h, "VOLUME");
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
        fader(cs80::CP_vcfLevel, "VCF LV", greyCap());
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

        for (int channel = 0; channel < 2; ++channel)
        {
            auto& tag = channelTags[(size_t) channel];
            addAndMakeVisible(tag);
            tag.setViewed(channel == 0);
            tag.onClick = [this, channel] { showChannel(channel); };
            place(tag, 424.0f, geo::rowY0[(size_t) channel] + 66.0f, 24.0f, 40.0f);
        }
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
            button.onClick = [this, channel] { showChannel(channel); };
            addAndMakeVisible(button);
            const auto key = channel == 0 ? geo::viewKeyI : geo::viewKeyII;
            place(button, key.getX(), key.getY(), key.getWidth(), key.getHeight());
        }

        for (int channel = 0; channel < 2; ++channel)
        {
            const float y = 232.0f + (float) channel * 82.0f;
            auto p = [channel](int local) { return cs80::chParam(channel, local); };
            auto& pan = addKnob(p(cs80::CP_pan), 94, y, 66, 72, "PAN");
            auto& level = addKnob(p(cs80::CP_vLevel), 166, y, 66, 72, "LEVEL");
            auto& hp = addLed(p(cs80::CP_hpfOn), "HPF", 240, y + 10, 60, 24);
            auto& lp = addLed(p(cs80::CP_lpfOn), "LPF", 240, y + 40, 60, 24);
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

    // Performance block: the left side of the deck. Sustain, portamento, glide and the sustain-mode tab
    // form one group; chorus speed and depth sit beside the three-state chorus selector.
    void buildPerformance()
    {
        const float y = 679.0f, h = 89.0f;       // 4 px below the header rule so type stays clear of it even at the 9 px floor
        // Everything sits between x50 and x386, inside the 342 px block (x48..390) that leaves the ribbon its width.
        shortCaption(addFader(cs80::P_masterSustain, 50, y, 36, h, "SUSTAIN", yellowCap()), "SUSTAIN", "SUS");
        addFader(cs80::P_portamento, 86, y, 36, h, "PORTA", whiteCap());
        addFader(cs80::P_glideMode, 122, y, 36, h, "GLIDE", greyCap());
        susToggle = &addLed(cs80::P_sustainMode, "SUS I", 160, 698, 52, 62);
        susToggle->hardwareTab = true; susToggle->accent = whiteCap();
        susToggle->setTooltip("Sustain mode: I (lamp dark) or II (lamp lit).");
        // Knobs are sized so their engraved legends match the faders' (about 10 px).
        chorusSpeedKnob = &addKnob(cs80::P_chorusSpeed, 214, 690, 48, 66, "SPEED");
        chorusDepthKnob = &addKnob(cs80::P_chorusDepth, 262, 690, 48, 66, "DEPTH");

        // Chorus: one three-position selector (OFF / CHORUS / TREMOLO) over the two switches it stands for.
        const char* names[] = { "OFF", "CHORUS", "TREMOLO" };
        const char* tips[] = { "Chorus section off.", "Chorus: the ensemble delay.", "Tremolo: amplitude modulation instead of the chorus delay." };
        for (int i = 0; i < 3; ++i)
        {
            auto& key = chorusKeys[(size_t) i];
            key.setButtonText(names[i]);
            key.setTitle(juce::String("Chorus section: ") + names[i]);
            key.setTooltip(tips[i]);
            key.setClickingTogglesState(false);
            key.setWantsKeyboardFocus(true);
            key.onClick = [this, i] { setChorusState(i); };
            addAndMakeVisible(key);
            place(key, 314.0f, 687.0f + (float) i * 27.0f, 72.0f, 25.0f);
        }
        syncChorusState();
    }

    // Prev / next / INIT replace the sounding program. While it has unsaved edits the first press only arms
    // the key for three seconds; the second press goes through.
    bool confirmDiscard(KeyButton& button)
    {
        syncPresetDisplay();
        if (! presetDirty) return true;
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (armedKey == &button && now < armDeadlineMs)
        {
            disarmKey();
            return true;
        }
        disarmKey();
        armedKey = &button;
        armDeadlineMs = now + 3000.0;
        button.setArmed(true);
        return false;
    }

    void disarmKey()
    {
        if (armedKey != nullptr) armedKey->setArmed(false);
        armedKey = nullptr;
    }

    // 0 = off, 1 = chorus, 2 = tremolo, as the two stored switches say it.
    int readChorusState() const
    {
        auto& apvts = processor.getAPVTS();
        const auto* on = apvts.getRawParameterValue(Cs80AudioProcessor::parameterId(cs80::P_chorusOn));
        const auto* mode = apvts.getRawParameterValue(Cs80AudioProcessor::parameterId(cs80::P_chorusMode));
        if (on == nullptr || mode == nullptr || on->load() < 0.5f) return 0;
        return mode->load() >= 0.5f ? 2 : 1;
    }

    void setChorusState(int state)
    {
        auto set = [this](int index, float value)
        {
            if (auto* parameter = processor.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(index)))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
                parameter->endChangeGesture();
            }
        };
        if (state == 0) set(cs80::P_chorusOn, 0.0f);
        else { set(cs80::P_chorusMode, state == 2 ? 1.0f : 0.0f); set(cs80::P_chorusOn, 1.0f); }
        syncChorusState();
    }

    void syncChorusState()
    {
        const int state = readChorusState();
        for (int i = 0; i < 3; ++i)
            chorusKeys[(size_t) i].setToggleState(i == state, juce::dontSendNotification);
        // With the section off its two knobs do nothing: dim them (they stay usable so a patch can be prepared).
        for (auto* knob : { chorusSpeedKnob, chorusDepthKnob })
            if (knob != nullptr && knob->getAlpha() != (state == 0 ? 0.42f : 1.0f))
                knob->setAlpha(state == 0 ? 0.42f : 1.0f);
    }

    // Owner controls 9/10 (keyboard contour) plus the foot-controller group that
    // the original keeps on its left-hand panel: footswitch jack/contact, the two
    // assigners, EXP/WAH and the output switch. The right side of the deck.
    void buildKeyboardControls()
    {
        const float y = 679.0f, h = 89.0f;
        addFader(cs80::P_kbdBrillianceLow, 1212, y, 32, h, "LOW", greenCap());
        addFader(cs80::P_kbdBrillianceHigh, 1246, y, 32, h, "HIGH", greenCap());
        addFader(cs80::P_kbdLevelLow, 1284, y, 32, h, "LOW", greyCap());
        addFader(cs80::P_kbdLevelHigh, 1318, y, 32, h, "HIGH", greyCap());

        expressionPedal.setTitle("Foot controller position (expression)");
        expressionPedal.setComponentID("expression_pedal");
        expressionPedal.setDescription("On-screen foot controller: volume and the EXP-WAH sweep. MIDI CC11 follows this pedal with parity isolation OFF, in both legacy and source-performance modes. In isolation, CC11 is ignored; this on-screen pedal remains active.");
        expressionPedal.setValue(processor.getExpressionValue());
        expressionPedal.onValueChange = [this](float value) { processor.setExpressionFromUI(value); };
        addAndMakeVisible(expressionPedal);
        place(expressionPedal, 1358, 685, 32, 79);

        // Three columns of 54 px keep the block inside the key edge (x1538).
        // Rocker switches 54 x 26: a comfortable target, lit legend and LED.
        auto toggle = [this](int parameter, const char* label, const char* tip, float x, float y) -> ui::LedButton&
        {
            auto& led = addLed(parameter, label, x, y, 48, 26);
            led.rockerStyle = true;
            led.setTooltip(tip);
            return led;
        };
        toggle(cs80::P_footswitchPlugged, "PLUG",
               "Footswitch plugged into the jack. With no plug the switched jack acts as a pressed switch.", 1400, 684);
        holdToggle = &toggle(cs80::P_footswitchPressed, "HOLD",
               "Footswitch contact (PRESS). Latches; toggle it to play the switch.", 1450, 684);
        wahToggle = &toggle(cs80::P_footMode, "EXP",
               "Foot controller selector: EXP = volume only, WAH = EXP-WAH sweep.", 1500, 684);
        toggle(cs80::P_sustainAssign, "SUSTAIN",
               "Assign the footswitch to SUSTAIN. Plugged and open switch removes the sustain release.", 1400, 731);
        toggle(cs80::P_glideAssign, "PORTA",
               "Assign the footswitch to portamento/glissando. Plugged and open switch suppresses gliding.", 1450, 731);
        outputToggle = &toggle(cs80::P_outputLevel, "LOW",
               "Output level switch: HIGH = unity, LOW = reduced.", 1500, 731);
        syncDeckLegends();
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

        ribbon.setComponentID("ribbon_controller");
        ribbon.setTitle("Ribbon controller");
        ribbon.setDescription("Ribbon controller: touch it where you like to set the zero point, then slide left or right to bend the pitch. Lift off to return to centre.");
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
        // Provenance: the program is a factory program when its slot and name still agree (a fresh instance
        // adds a " / Original SUB" suffix), or when its name is a factory program's name. The library resolves
        // it the same way, so the toolbar and the library inspector always name the same slot.
        int slot = -1;
        bool exactName = false;
        if (metadata.factoryIndex >= 0 && metadata.factoryIndex < (int) bank.size())
        {
            const juce::String factoryName(bank[(size_t) metadata.factoryIndex].name);
            if (metadata.name == factoryName) { slot = metadata.factoryIndex; exactName = true; }
            else if (metadata.name.startsWith(factoryName + " / ")) slot = metadata.factoryIndex;
        }
        if (slot < 0)
            for (int i = 0; i < (int) bank.size(); ++i)
                if (juce::String(bank[(size_t) i].name).equalsIgnoreCase(metadata.name)) { slot = i; exactName = true; break; }
        const bool isFactory = slot >= 0;
        const juce::String category = isFactory ? juce::String(bank[(size_t) slot].category).toUpperCase() : juce::String();
        const juce::String tag = isFactory ? category : juce::String("USER PATCH");
        const juce::String index = isFactory ? juce::String(slot + 1).paddedLeft('0', 3) + " / " + juce::String((int) bank.size()) : juce::String("---");
        if (presetTagLabel.getText() != tag) presetTagLabel.setText(tag, juce::dontSendNotification);
        presetTagLabel.setDisplayText(isFactory ? "FACTORY  " + category : juce::String("USER PATCH"));
        if (presetIndexLabel.getText() != index) presetIndexLabel.setText(index, juce::dontSendNotification);
        presetIndexLabel.setDisplayText(isFactory ? "OF " + juce::String((int) bank.size()) : juce::String());
        const auto number = isFactory ? juce::String(slot + 1).paddedLeft('0', 3) : juce::String("USR");
        const auto numberInk = isFactory ? juce::Colour(0xffffb347) : juce::Colour(0xff9a6a2c);
        if (presetNumberLabel.findColour(juce::Label::textColourId) != numberInk)
            presetNumberLabel.setColour(juce::Label::textColourId, numberInk);
        if (presetNumberLabel.getText() != number) presetNumberLabel.setText(number, juce::dontSendNotification);
        const bool dirty = patchHasUnsavedEdits(metadata, isFactory && exactName, slot);
        presetDirty = dirty;
        if (! dirty) disarmKey();
        presetWindow.setDirty(dirty);
        saveButton.setEmphasis(dirty);   // SAVE lights only when there is something to save
    }

    // True when the sounding patch differs from the program that is named in the display. Factory programs
    // compare against the stored values; other programs (and variants such as "Init Patch / Original SUB")
    // against the state first seen under that name.
    bool patchHasUnsavedEdits(const Cs80AudioProcessor::PresetMetadata& metadata, bool compareToFactory, int slot)
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
        if (compareToFactory && slot >= 0)
            return differs(processor.getFactoryPresets()[(size_t) slot].values);
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
        // Printed ends of the six-position FUNCTION fader follow the function set it is bound to.
        {
            auto& props = subFunction->getSlider().getProperties();
            const juce::String top = original ? "EXT" : "RAND";
            if (props["faderLegendTop"].toString() != top || props["faderLegendBottom"].toString() != "SINE")
            {
                props.set("faderLegendTop", top);
                props.set("faderLegendBottom", "SINE");
                subFunction->getSlider().repaint();
            }
        }
        if (subModeLabel.getText() != status)
            subModeLabel.setText(status, juce::dontSendNotification);
        // Players read a short tag; the full engineering status stays in the text and tooltip.
        // The strip is quiet in the ordinary case: plain ORIGINAL / CLASSIC are not printed at all.
        const juce::String subShown = status.startsWith("SUB MODE CHANGED") ? "SUB  MOVE CONTROL TO RESUME"
                                    : status.contains("EXT BUS") ? (status.contains("DISABLED") ? "SUB  EXT INPUT OFF" : "SUB  EXT INPUT ON")
                                    : (isolated && original) ? "SUB  ORIGINAL (ISOLATED)" : juce::String();
        subModeLabel.setDisplayText(subShown);
        subModeLabel.setInterceptsMouseClicks(subShown.isNotEmpty(), false);
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
        const juce::String modeShown = policy == cs80::RenderPolicy::isolation ? "MODE  ISOLATED COMPARISON"
                                       : policy == cs80::RenderPolicy::sourcePerformance ? "MODE  SOURCE PERFORMANCE"
                                                                                         : juce::String();   // legacy is the default
        modeStatusLabel.setDisplayText(modeShown);
        modeStatusLabel.setInterceptsMouseClicks(modeShown.isNotEmpty(), false);
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
            // The strip says "MEMORY n" and "PANEL"; the verbose text keeps the engineering words.
            const juce::String plain = source < 11 ? identity : source == 13 ? juce::String("PANEL")
                                                                             : "MEMORY " + juce::String(rank + (source == 12 ? 3 : 1));
            label.setDisplayText(juce::String(rank == 0 ? "I   " : "II  ") + plain);
            label.setDot(source < 11 ? juce::Colour(0xffe8ac3c) : source == 13 ? juce::Colour(0xffd9cfae) : juce::Colour(0xff58d6c0));
            label.setTooltip(text + ". Requested APVTS selection, not an audio-thread acknowledgment. "
                "SOURCE-COMPLETE means the source-matrix column has no unresolved cells, not that the instrument is calibrated; device laws remain inferred. "
                "SOURCE-INFERRED also uses explicit hypotheses for unresolved cells. LEGACY STUDY uses the older voicing table. "
                "Source performance and isolation share source classifications but differ in MIDI and modern-overlay policy. "
                "A selected tone or memory owns the 26 programmer controls; main-panel faders are retained for PANEL. Footage and global controls remain live. "
                "Library programs reset both selectors to PANEL and do not select source tabs.");
        }
    }

    // The meter alone runs faster than the panel's 8 Hz poll (see MeterTicker); tick() also calls it so a host
    // or a test that drives tick() still sees fresh values.
    void updateMeter()
    {
        outputMeterDisplay.update(processor.consumeOutputMeter(), juce::Time::getMillisecondCounterHiRes() * 0.001);
        updateOutputMeterLabel();
    }

    void updateOutputMeterLabel()
    {
        const juce::String level = outputMeterDisplay.atFloor()
            ? "<= -120"
            : (outputMeterDisplay.peakDbfs() > 0.0f ? juce::String("+") : juce::String())
                + juce::String(outputMeterDisplay.peakDbfs(), 1);
        // On the panel the minus is a true minus sign, and small editors drop the decimal so the whole number fits.
        const juce::String minus = juce::String::charToString((juce::juce_wchar) 0x2212);
        const bool compactMeter = getWidth() < 1200;
        juce::String shownLevel = outputMeterDisplay.atFloor() ? minus + "INF"
            : (outputMeterDisplay.peakDbfs() > 0.0f ? juce::String("+") : juce::String())
                + (compactMeter ? juce::String(juce::roundToInt(outputMeterDisplay.peakDbfs())) : juce::String(outputMeterDisplay.peakDbfs(), 1));
        if (shownLevel.startsWithChar('-')) shownLevel = minus + shownLevel.substring(1);
        juce::String flags;
        if (outputMeterDisplay.overRange()) flags = "OVER-RANGE";
        if (outputMeterDisplay.nonFinite())
            flags += (flags.isEmpty() ? "" : " | ") + juce::String("NON-FINITE");
        if (flags.isEmpty()) flags = "READ-ONLY";
        const auto text = "SAMPLE PEAK " + level + " dBFS\n" + flags;
        if (outputMeterLabel.getText() != text)
            outputMeterLabel.setText(text, juce::dontSendNotification);
        // The toolbar shows only the peak-hold number; the full wording is the text and the tooltip.
        outputMeterLabel.setDisplayText(outputMeterDisplay.nonFinite() ? juce::String("NaN") : shownLevel);
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
    // Which channel the signal-flow window describes: from the tags, the I/II keys, or touching one of its faders.
    void showChannel(int channel)
    {
        if (channel == viewChannel) return;
        viewChannel = channel;
        syncVoicePlate();
    }

    void syncVoicePlate()
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            channelTags[(size_t) channel].setViewed(channel == viewChannel);
            viewButtons[(size_t) channel].setToggleState(channel == viewChannel, juce::dontSendNotification);
        }
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

    // The two-way deck switches print the position they are in.
    void syncDeckLegends()
    {
        if (wahToggle != nullptr)
        {
            const juce::String text = wahToggle->getToggleState() ? "WAH" : "EXP";
            if (wahToggle->getButtonText() != text) wahToggle->setButtonText(text);
        }
        if (susToggle != nullptr)
        {
            const juce::String text = susToggle->getToggleState() ? "SUS II" : "SUS I";
            if (susToggle->getButtonText() != text) susToggle->setButtonText(text);
        }
        if (outputToggle != nullptr)
        {
            const juce::String text = outputToggle->getToggleState() ? "HIGH" : "LOW";
            if (outputToggle->getButtonText() != text) outputToggle->setButtonText(text);
        }
    }

    // The status strip prints up to four short readouts (rank I, rank II, SUB, MODE). It is one recessed window
    // that is exactly as long as what it says: the labels are packed left to right at the size they print at,
    // thin dividers sit between them, and the window ends in a bezel just after the last one. An empty readout
    // takes no room, so the quiet case is a short window and the long mode text never leaves a dead bar behind.
    void layoutStrip(bool force)
    {
        if (getWidth() <= 0) return;
        const float sx = (float) getWidth() / kDesignWidth, sy = (float) getHeight() / kDesignHeight;
        PanelLabel* order[] = { &sourceStatusLabels[0], &sourceStatusLabels[1], &subModeLabel, &modeStatusLabel };
        juce::String key;
        for (auto* label : order) key << label->shownText() << '|';
        key << getWidth();
        if (! force && key == stripKey) return;
        stripKey = key;
        float x = geo::stripX + geo::stripPad, end = x;
        std::vector<float> dividers;
        bool any = false;
        for (auto* label : order)
        {
            label->setFont(displayFont(11.5f * sy, juce::Font::bold));
            label->setMinimumPixels(9.5f);
            label->setLeftPad(0);
            auto font = label->getFont();
            if (font.getHeight() < 9.5f) font = font.withHeight(9.5f);
            const float width = label->contentWidth(font);
            if (width <= 0.0f)
            {
                label->setBounds(juce::roundToInt(x * sx), juce::roundToInt(geo::ticker.getY() * sy), 1, juce::roundToInt(geo::ticker.getHeight() * sy));
                continue;
            }
            if (any) { dividers.push_back(x - geo::stripGap * 0.5f); }
            const float design = width / sx + 6.0f;
            label->setBounds(juce::roundToInt(x * sx), juce::roundToInt(geo::ticker.getY() * sy),
                             juce::roundToInt(design * sx), juce::roundToInt(geo::ticker.getHeight() * sy));
            end = x + design;
            x = end + geo::stripGap;
            any = true;
        }
        const float newEnd = end + geo::stripPad;
        if (std::abs(newEnd - stripEnd) > 0.4f || dividers != stripDividers)
        {
            stripEnd = newEnd;
            stripDividers = dividers;
            backdrop.repaint();
        }
    }

    void syncLampStatus(bool midi)
    {
        const int voices = juce::jlimit(0, 8, processor.getActiveVoiceCount());
        // The toolbar lamps carry voices and MIDI; the strip keeps the (hidden) text for accessibility only.
        voiceStatusLabel.setDisplayText({});
        midiStatusLabel.setDisplayText({});
        voiceStatusLabel.setInterceptsMouseClicks(false, false);
        midiStatusLabel.setInterceptsMouseClicks(false, false);
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
        layoutStrip(false);
        updateMeter();
        syncVoicePlate();
        syncChorusState();
        syncDeckLegends();
        if (armedKey != nullptr && juce::Time::getMillisecondCounterHiRes() >= armDeadlineMs) disarmKey();
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

    // The voice-flow plate: a machined bezel with four chamfer facets around a window of smoked glass.
    // The live curves are drawn on top of it by VoicePlate.
    void paintVoicePlate(juce::Graphics& g)
    {
        const auto r = geo::plate;
        const float x0 = r.getX(), y0 = r.getY();
        art::ambient(g, r, 5.0f, 14.0f, 0.62f);
        ui::softShadow(g, r, 5.0f, 0.0f, 5.0f, 12.0f, 0.8f);
        // bezel body: dark anodised metal, lit from the upper left
        g.setGradientFill(art::vgrad(juce::Colour(0xff2c2b28), juce::Colour(0xff121110), r));
        g.fillRoundedRectangle(r, 5.0f);
        // four chamfer facets between the outer edge and the glass
        const auto outer = r.reduced(1.0f), inner = r.reduced(7.0f);
        auto facet = [&](juce::Point<float> a, juce::Point<float> b, juce::Point<float> c, juce::Point<float> d, juce::Colour top, juce::Colour bottom, juce::Point<float> from, juce::Point<float> to)
        {
            juce::Path pth;
            pth.startNewSubPath(a); pth.lineTo(b); pth.lineTo(c); pth.lineTo(d); pth.closeSubPath();
            g.setGradientFill(juce::ColourGradient(top, from.x, from.y, bottom, to.x, to.y, false));
            g.fillPath(pth);
        };
        facet(outer.getTopLeft(), outer.getTopRight(), inner.getTopRight(), inner.getTopLeft(),
              juce::Colour(0xff4a4844), juce::Colour(0xff2c2b28), { x0, y0 }, { x0, y0 + 7.0f });                                // top: lit
        facet(outer.getTopLeft(), inner.getTopLeft(), inner.getBottomLeft(), outer.getBottomLeft(),
              juce::Colour(0xff3c3a37), juce::Colour(0xff252422), { x0, y0 }, { x0 + 7.0f, y0 });                                // left: lit
        facet(outer.getTopRight(), outer.getBottomRight(), inner.getBottomRight(), inner.getTopRight(),
              juce::Colour(0xff201f1d), juce::Colour(0xff141312), { r.getRight() - 7.0f, y0 }, { r.getRight(), y0 });            // right: shaded
        facet(outer.getBottomLeft(), inner.getBottomLeft(), inner.getBottomRight(), outer.getBottomRight(),
              juce::Colour(0xff161514), juce::Colour(0xff0b0b0a), { x0, r.getBottom() - 7.0f }, { x0, r.getBottom() });          // bottom: darkest
        // facet seams: a bright arris along the top/left, a dark one along the bottom/right
        g.setColour(art::white(0.15f));
        g.drawLine(outer.getX() + 4.0f, outer.getY(), outer.getRight() - 4.0f, outer.getY(), 1.0f);
        g.setColour(art::white(0.07f));
        g.drawLine(outer.getX(), outer.getY() + 4.0f, outer.getX(), outer.getBottom() - 4.0f, 1.0f);
        g.setColour(art::black(0.8f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
        g.setColour(art::black(0.65f));
        g.drawLine(inner.getX(), inner.getY(), inner.getRight(), inner.getY(), 1.0f);           // shadow under the lit lip
        g.setColour(art::white(0.10f));
        g.drawLine(inner.getX(), inner.getBottom(), inner.getRight(), inner.getBottom(), 1.0f); // the glass edge catching light

        // smoked glass: nearly flat so printed type keeps a clean halo; a warm glow from the lamps behind it
        const auto glass = inner;
        g.setGradientFill(art::vgrad(art::glassColour().brighter(0.08f), art::glassColour().darker(0.10f), glass));
        g.fillRect(glass);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe0c890).withAlpha(0.035f), glass.getCentreX(), glass.getCentreY() - 20.0f,
                                               juce::Colours::transparentBlack, glass.getX(), glass.getBottom(), true));
        g.fillRect(glass);
        art::insetShadow(g, glass, 0.5f, 9.0f, 0.55f);
        // hairline frame printed on the glass, and a rule under the title row
        g.setColour(juce::Colour(0xffcdb878).withAlpha(0.22f));
        g.drawRect(glass.reduced(2.5f), 0.8f);
        // screw bosses in the four corners of the bezel
        for (float sx : { x0 + 3.9f, r.getRight() - 3.9f })
            for (float sy : { y0 + 3.9f, r.getBottom() - 3.9f })
            {
                g.setColour(art::black(0.7f));
                g.fillEllipse(sx - 3.5f, sy - 3.0f, 7.0f, 7.0f);
                art::slotScrew(g, { sx, sy }, 2.7f, sx * 0.3f + sy);
            }

        art::haloText(g, "VOICE SIGNAL FLOW", { x0 + 14, y0 + 14, 220, 22 }, spacedFont(11.5f, 0.14f), juce::Justification::centredLeft,
                      juce::Colour(0xffeadfb8), art::glassColour());
        art::legend(g, "VIEW", { x0 + 222, y0 + 14, 58, 22 }, spacedFont(8.5f, 0.16f), juce::Justification::centredRight,
                    juce::Colour(0xffb5a878), 0.0f);
        art::etchLine(g, x0 + 14, y0 + 52, x0 + 346, y0 + 52, 0.45f);
        art::etchLine(g, x0 + 14, y0 + 330, x0 + 346, y0 + 330, 0.30f);
        if (optionsVisible)
        {
            const juce::Rectangle<float> well(x0 + 12, y0 + 116, 336, 188);
            art::etchRect(g, well, 3.0f, 0.5f);
            art::haloText(g, "VOICE SETTINGS  /  SOFTWARE", { well.getX() + 8, well.getY() + 4, 240, 11 }, spacedFont(8.0f, 0.14f),
                          juce::Justification::centredLeft, juce::Colour(0xffcdb878), art::glassColour());
            panelText(g, "I", x0 + 24, 232 + 22, 20, 28, 12);
            panelText(g, "II", x0 + 24, 314 + 22, 20, 28, 12);
            art::etchLine(g, x0 + 20, 306, x0 + 340, 306, 0.35f);
        }
    }

    // The raised back edge of the panel: an 8 px ledge in the panel's own finish with two narrow vent slots in it.
    // No printed legend: it is architecture, not information.
    void paintTopLip(juce::Graphics& g)
    {
        const juce::Rectangle<float> lip(geo::bodyLeft, 64.0f, geo::bodyRight - geo::bodyLeft, geo::lipBottom - 64.0f);
        g.setColour(art::white(0.035f));
        g.fillRect(lip);
        g.setColour(art::white(0.14f));
        g.fillRect(lip.getX(), 64.0f, lip.getWidth(), 1.2f);
        g.setColour(art::black(0.65f));
        g.fillRect(lip.getX(), geo::lipBottom - 1.5f, lip.getWidth(), 1.5f);
        g.setGradientFill(art::vgrad(art::black(0.42f), juce::Colours::transparentBlack, { lip.getX(), geo::lipBottom, lip.getWidth(), 9.0f }));
        g.fillRect(lip.getX(), geo::lipBottom, lip.getWidth(), 9.0f);
        for (const auto& slot : { juce::Rectangle<float>(300.0f, 67.2f, 466.0f, 2.6f), juce::Rectangle<float>(834.0f, 67.2f, 466.0f, 2.6f) })
        {
            g.setColour(art::white(0.10f));
            g.fillRoundedRectangle(slot.translated(0.0f, 0.9f), 1.2f);
            g.setColour(art::black(0.85f));
            g.fillRoundedRectangle(slot, 1.2f);
        }
    }

    // Each channel is outlined in etched gold, not boxed in a bevelled tray.
    void paintChannel(juce::Graphics& g, int channel)
    {
        const float y0 = geo::rowY0[(size_t) channel];
        const juce::Rectangle<float> frame(418.0f, y0 - 6.0f, 1134.0f, 186.0f);
        art::etchRect(g, frame, 4.0f, 0.62f);
        // (the channel numeral is an engraved ChannelTag component, lit while its curves are on the plate)
        art::bracket(g, "VCO", 459, y0 + 10, 235);
        art::bracket(g, "VCF", 706, y0 + 10, 354);
        art::bracket(g, "VCA", 1075, y0 + 10, 272);
        art::bracket(g, "INITIAL", 1365, y0 + 10, 76);
        art::bracket(g, "AFTER", 1445, y0 + 10, 76);
        // faint column separators between functional groups
        for (float x : { 702.0f, 1071.0f, 1360.0f })
            art::etchLine(g, x, y0 + 22.0f, x, y0 + 170.0f, 0.22f);
    }

    void paintToneBezel(juce::Graphics& g)
    {
        const auto r = geo::toneBezel;
        art::ambient(g, r, 6.0f, 14.0f, 0.55f);
        ui::softShadow(g, r, 5.0f, 0.0f, 3.0f, 8.0f, 0.85f);
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
            art::legend(g, row == 0 ? "I" : "II", { 662.0f, cy - 9.0f, 19.0f, 18.0f }, spacedFont(11.5f, 0.04f),
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
        {
            art::ambient(g, block, 5.0f, 8.0f, 0.42f);       // each block sits slightly proud of the deck
            art::etchRect(g, block, 5.0f, 0.5f);
        }
        // One bracket style for every deck header.
        const float ruleY = 672.0f;
        art::bracket(g, "PERFORMANCE", 50, ruleY, 162, 4.0f);
        art::bracket(g, "CHORUS / TREMOLO", 214, ruleY, 174, 4.0f, 9.5f, "CHORUS");
        // Small editors print these at the 9 px floor, which is wider in design units: the wide headers are pulled
        // in a little so neighbouring headers (LEVEL, FOOT, SWITCH, MODE) keep a gap between their rules, while
        // the narrow ones (FOOT, MODE) keep every pixel their text needs.
        const float pull = gSmallEditor ? 2.0f : 0.0f;
        auto header = [&](const char* title, float x, float y, float w, float inset, const char* shortTitle = "")
        {
            art::bracket(g, title, x + inset, y, w - 2.0f * inset, 4.0f, 9.5f, shortTitle);
        };
        // 8 px or more between neighbouring header rules (BRILL / LEVEL / FOOT / SWITCH / MODE)
        header("BRILLIANCE", 1212, ruleY, 64, pull, "BRILL");
        header("LEVEL", 1288, ruleY, 60, pull);
        header("FOOT", 1356, ruleY, 40, 0.0f);
        header("FOOTSWITCH", 1404, ruleY, 88, pull, "SWITCH");
        header("ASSIGN TO", 1404, 719.0f, 88, pull);
        header("OUTPUT", 1500, 719.0f, 48, 0.0f, "OUT");
        header("MODE", 1500, ruleY, 48, 0.0f);

        // centre: nameplate and the ribbon slot
        const auto np = geo::nameplate;
        ui::softShadow(g, np, 3.0f, 0.0f, 5.0f, 9.0f, 0.95f);
        art::brushedMetal(g, np, 3.0f, -0.35f, 9);
        // warm the metal toward brass-grey and pull its highlights down so it does not out-shout the controls
        g.setColour(juce::Colour(0xff7a5a30).withAlpha(0.30f));
        g.fillRoundedRectangle(np, 3.0f);
        g.setColour(art::black(0.16f));
        g.fillRoundedRectangle(np, 3.0f);
        g.setGradientFill(art::hgrad(art::black(0.30f), juce::Colours::transparentBlack, { np.getX(), np.getY(), 28.0f, 1.0f }));
        g.fillRoundedRectangle(np.withWidth(28.0f), 3.0f);
        g.setGradientFill(art::hgrad(juce::Colours::transparentBlack, art::black(0.30f), { np.getRight() - 28.0f, np.getY(), 28.0f, 1.0f }));
        g.fillRoundedRectangle(np.withX(np.getRight() - 28.0f).withWidth(28.0f), 3.0f);
        g.setColour(art::white(0.34f));
        g.drawRoundedRectangle(np.reduced(1.3f), 2.5f, 0.9f);
        g.setColour(art::black(0.8f));
        g.drawRoundedRectangle(np, 3.0f, 1.0f);
        for (float sx : { np.getX() + 13.0f, np.getRight() - 13.0f })
            art::slotScrew(g, { sx, np.getCentreY() }, 4.4f, sx * 0.1f);
        art::engraved(g, "YAMAHA", np.reduced(30.0f, 3.0f), spacedFont(25.0f, 0.34f, 0.95f));
        art::legend(g, "POLYPHONIC", { 490, 679, 136, 20 }, spacedFont(10.5f, 0.2f), juce::Justification::centredRight, juce::Colour(0xffe6dcbc));
        art::legend(g, "SYNTHESIZER", { 974, 679, 136, 20 }, spacedFont(10.5f, 0.2f), juce::Justification::centredLeft, juce::Colour(0xffe6dcbc));

        // The ribbon's own caption: one bracket over the whole slot. (The scale along the ribbon is printed by the
        // ribbon component itself.)
        art::bracket(g, "RIBBON CONTROLLER", geo::ribbonRect.getX() + 2.0f, 713.0f, geo::ribbonRect.getWidth() - 4.0f, 4.0f, 9.5f);
        const auto slot = geo::ribbonRect.expanded(5.0f, 3.0f);
        art::ambient(g, slot, 5.0f, 9.0f, 0.55f);
        art::recessTray(g, slot, 5.0f, juce::Colour(0xff101010), juce::Colour(0xff070707), 8.0f, 0.8f);
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
            if (! gSmallEditor)
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
        art::etchRect(g, geo::cluster, 5.0f, 0.5f);
        for (float x : { 860.0f, 934.0f, 1010.0f })
        {
            g.setColour(art::black(0.5f));
            g.fillRect(x, 12.0f, 1.0f, 28.0f);
            g.setColour(art::white(0.07f));
            g.fillRect(x + 1.0f, 12.0f, 1.0f, 28.0f);
        }
        art::etchRect(g, geo::lampWell, 5.0f, 0.5f);
        // output meter window: a flat bezel around dark glass, the scale printed under the bar
        {
            const auto w = geo::meterWell;
            art::raisedPlate(g, w.expanded(3.0f), 6.0f, juce::Colour(0xff6c675e), juce::Colour(0xff1f1d19), 0.4f);
            art::recessTray(g, w, 4.0f, juce::Colour(0xff0e0f0c), juce::Colour(0xff050504), 8.0f, 0.9f);
            g.setGradientFill(art::vgrad(art::white(0.07f), juce::Colours::transparentWhite, w.withHeight(w.getHeight() * 0.5f)));
            g.fillRoundedRectangle(w.reduced(1.0f).withHeight(w.getHeight() * 0.5f), 3.0f);
            art::legend(g, "OUT", { 1264, 6.5f, 46, 11 }, spacedFont(9.5f, 0.1f), juce::Justification::centredLeft, juce::Colour(0xffe0cf94), 0.8f);
            art::legend(g, "dBFS", { 1264, 34.5f, 46, 11 }, spacedFont(9.5f, 0.06f), juce::Justification::centredLeft, juce::Colour(0xffc4b583), 0.8f);
            art::etchLine(g, 1315, 11, 1315, 41, 0.35f);
            // dB scale under the bar, on the same 20-segment geometry the bar uses (its OVR lamp takes the right end)
            const float usable = geo::meterBar.getWidth() - MeterBar::lampWidthNow() - 2.0f;
            const std::pair<float, const char*> marks[] = { { -60.0f, "-60" }, { -40.0f, "-40" }, { -20.0f, "-20" }, { -10.0f, "-10" }, { 0.0f, "0" } };
            for (const auto& m : marks)
            {
                const float x = geo::meterBar.getX() + (m.first + 60.0f) / 60.0f * usable - (m.first >= 0.0f ? 0.6f : 0.0f);
                g.setColour(juce::Colour(0xffd8cfb0).withAlpha(0.7f));
                g.fillRect(x - 0.4f, 26.5f, 0.9f, 3.5f);
                // -10 keeps its tick but not its number: it would touch -20 and 0 (small editors also drop -60)
                if (m.first == -10.0f || (gSmallEditor && m.first == -60.0f)) continue;
                art::legend(g, m.second, { x - 12.0f, 29.5f, 24.0f, 12.0f }, displayFont(9.5f, juce::Font::bold),
                            juce::Justification::centred, juce::Colour(0xffe6dcbc), 0.0f);
            }
            const float x3 = geo::meterBar.getX() + (-3.0f + 60.0f) / 60.0f * usable;
            g.setColour(juce::Colour(0xffff6a4a).withAlpha(0.9f));
            g.fillRect(x3 - 0.4f, 26.5f, 0.9f, 3.5f);
        }
        // player status strip: a recessed black window with the status text lit inside it, as long as its text
        {
            const juce::Rectangle<float> t(geo::stripX, geo::stripY, juce::jmax(40.0f, stripEnd - geo::stripX), geo::stripH);
            art::recessTray(g, t, 4.0f, juce::Colour(0xff0b0b0a), juce::Colour(0xff040403), 5.0f, 0.9f);
            g.setGradientFill(art::vgrad(art::white(0.05f), juce::Colours::transparentWhite, t.withHeight(t.getHeight() * 0.5f)));
            g.fillRoundedRectangle(t.reduced(1.0f).withHeight(t.getHeight() * 0.5f), 3.0f);
            for (float x : stripDividers)
            {
                g.setColour(art::black(0.7f));
                g.fillRect(x, t.getY() + 3.0f, 1.0f, t.getHeight() - 6.0f);
                g.setColour(art::white(0.10f));
                g.fillRect(x + 1.0f, t.getY() + 3.0f, 1.0f, t.getHeight() - 6.0f);
            }
        }
    }

    // A groove across the whole panel between two functional rows: a dark cut, a lit lower lip, and a soft
    // fall of shadow beneath it.
    static void seamGroove(juce::Graphics& g, float y)
    {
        const float x = geo::bodyLeft, w = geo::bodyRight - geo::bodyLeft;
        g.setGradientFill(art::vgrad(art::black(0.30f), juce::Colours::transparentBlack, { x, y, w, 6.0f }));
        g.fillRect(x, y, w, 6.0f);
        g.setColour(art::black(0.78f));
        g.fillRect(x, y - 0.8f, w, 1.6f);
        g.setColour(art::white(0.11f));
        g.fillRect(x, y + 0.8f, w, 1.0f);
    }

    void paintChassis(juce::Graphics& g)
    {
        const float sx = (float) getWidth() / kDesignWidth;
        const float sy = (float) getHeight() / kDesignHeight;
        const float scale = sx;
        FloorScope floor(scale);
        g.saveState();
        g.addTransform(juce::AffineTransform::scale(sx, sy));
        g.fillAll(juce::Colour(0xff070605));

        // ---- one fine crinkle finish for the toolbar, panel and deck ---------
        const juce::Rectangle<float> body(geo::bodyLeft, 0.0f, geo::bodyRight - geo::bodyLeft, geo::deckBottomFinal);
        art::fillTile(g, art::panelTile(), body, scale);
        // One light over the whole panel: it comes from the upper left, so the left runs bright and the
        // lower right falls away. Nothing else lights the surface.
        g.setGradientFill(juce::ColourGradient(art::white(0.05f), 230.0f, 70.0f, juce::Colours::transparentWhite, 1050.0f, 520.0f, true));
        g.fillRect(body);
        g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, 680.0f, 260.0f, art::black(0.13f), 1560.0f, 775.0f, false));
        g.fillRect(body);
        // A large, faint diagonal specular wash from the upper-left light, baked once with the rest of the chassis:
        // a broad band running from the top left toward the lower right, soft on both sides.
        {
            juce::ColourGradient wash(juce::Colours::transparentWhite, 60.0f, -120.0f, juce::Colours::transparentWhite, 980.0f, 700.0f, false);
            wash.addColour(0.30, juce::Colours::transparentWhite);
            wash.addColour(0.48, art::white(0.050f));
            wash.addColour(0.66, juce::Colours::transparentWhite);
            g.setGradientFill(wash);
            g.fillRect(body);
        }

        paintTopLip(g);
        g.saveState();                                                  // the control surface and deck, moved up by midShift
        g.addTransform(juce::AffineTransform::translation(0.0f, -geo::midShift));
        seamGroove(g, 287.0f);
        seamGroove(g, 480.0f);
        paintChannel(g, 0);
        paintChannel(g, 1);
        paintVoicePlate(g);

        // ---- lower row: ring / sub / feet | tone selector | global ------------
        {
            const auto lp = geo::lowerPlate;
            art::etchRect(g, lp, 5.0f, 0.55f);
            art::bracket(g, "RING MODULATOR", 158, 499, 200);
            art::bracket(g, "SUB OSCILLATOR", 370, 499, 205, 6.0f, 9.5f, "SUB OSC");
            art::bracket(g, "FEET", 580, 499, 80);
            art::bracket(g, "TONE SELECTOR", 675, 505, 514);
            art::bracket(g, "GLOBAL / TOUCH RESPONSE", 1208, 499, 340, 6.0f, 9.5f, "GLOBAL");
            paintToneBezel(g);
        }
        // knobs stand on the panel: a soft contact shadow all around the cap
        for (const auto& item : placements)
            if (auto* knob = dynamic_cast<ui::ParamKnob*>(item.component);
                knob != nullptr && knob->isVisible() && knob->getSlider().getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag)
            {
                const float d = juce::jmin(item.bounds.getWidth() - 10.0f, 46.0f);
                const float cy = item.bounds.getHeight() > 100.0f ? item.bounds.getCentreY() : item.bounds.getY() + 7.0f + d * 0.5f;
                art::ambient(g, juce::Rectangle<float>(d, d).withCentre({ item.bounds.getCentreX(), cy }),
                             d * 0.5f, 5.0f, 0.42f);
            }

        // front edge of the control surface over the deck
        g.setColour(art::white(0.05f));
        g.fillRect(geo::bodyLeft, 652.0f, geo::bodyRight - geo::bodyLeft, 8.0f);
        g.setColour(art::white(0.20f));
        g.fillRect(geo::bodyLeft, 652.0f, geo::bodyRight - geo::bodyLeft, 1.0f);
        g.setColour(art::black(0.9f));
        g.fillRect(geo::bodyLeft, 659.0f, geo::bodyRight - geo::bodyLeft, 1.5f);

        paintDeck(g);

        // ---- screws on the control face -----------------------------------
        for (float x : { 61.0f, 1539.0f })
            art::slotScrew(g, { x, 645.0f }, 4.4f, (x + 645.0f) * 0.7f);
        g.restoreState();
        paintToolbar(g);

        // ---- keybed base (the Keybed component paints over this) -----------
        g.setColour(juce::Colour(0xff0a0908));
        g.fillRect(geo::keybed);

        // ---- walnut cheeks run the full height of the case, over everything --
        art::walnutCheek(g, { 0, 0, geo::bodyLeft, kDesignHeight }, true, geo::deckBottomFinal, scale);
        art::walnutCheek(g, { geo::bodyRight, 0, kDesignWidth - geo::bodyRight, kDesignHeight }, false, geo::deckBottomFinal, scale);
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
        // Rectangles of the control surface and deck are in the shifted block's design coordinates.
        auto rs = [&r](const juce::Rectangle<float>& b) { return r(b.getX(), geo::shifted(b.getY()), b.getWidth(), b.getHeight()); };

        backdrop.setBounds(getLocalBounds());
        spill.setBounds(getLocalBounds());
        applyCaptions();
        voicePlate.setBounds(rs(geo::plate));
        for (const auto& item : placements)
            item.component->setBounds(r(item.bounds.getX(), geo::shifted(item.bounds.getY()),
                                        item.bounds.getWidth(), item.bounds.getHeight()));
        // PITCH / VOLUME captions and values use the legend column of one 38 px fader.
        if (legendReference != nullptr)
            for (auto* knob : { pitchKnob, volumeKnob })
                if (knob != nullptr)
                    setLegendColumn(*knob, (float) legendReference->getWidth());

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
        outputMeterLabel.setMinimumPixels(9.0f);
        outputMeterLabel.setJustificationType(juce::Justification::centredLeft);
        outputMeterLabel.setLines(1);
        outputMeterLabel.setLeftPad(juce::roundToInt(2.0f * sx));
        meterBar.setBounds(rr(geo::meterBar));
        voiceStatusLabel.setBounds(rr(geo::voiceStatus));
        midiStatusLabel.setBounds(rr(geo::midiStatus));
        layoutStrip(true);

        ribbon.setBounds(rs(geo::ribbonRect));
        pitchWheel.setBounds(r(312.0f, geo::shifted(230.0f), 30.0f, 150.0f));
        modWheel.setBounds(r(350.0f, geo::shifted(230.0f), 30.0f, 150.0f));
        keybed.setBounds(rr(geo::keybed));

        scrim.setBounds(rr(geo::overlayBody));
        shade.setBounds(rr(geo::overlayBody));
        advanced.setBounds(rr(geo::overlay));
        browser.setBounds(rr(geo::overlay));
        memoryProgrammer.setBounds(rr(geo::memoryLid));
        // [ws8-studio] begin: toolbar over the program cluster (x 250-1114), drawers over the panel and keybed.
        if (studio != nullptr)
        {
            studio->toolbar.setBounds(r(250.0f, 2.0f, ui::StudioToolbar::kDesignW, ui::StudioToolbar::kDesignH));
            studio->scope.setBounds(r(56.0f, 70.0f, ui::ScopeDrawer::kDesignW, ui::ScopeDrawer::kDesignH));
            studio->controllers.setBounds(r(52.0f, 758.0f, ui::ControllersStrip::kDesignW, ui::ControllersStrip::kDesignH));
        }
        // [ws8-studio] end
    }

    Cs80AudioProcessor& processor;
    Backdrop backdrop;
    ui::LightSpill spill;
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
    bool presetDirty = false;
    KeyButton* armedKey = nullptr;
    double armDeadlineMs = 0.0;
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
    ui::ParamKnob* pitchKnob = nullptr;
    ui::ParamKnob* volumeKnob = nullptr;
    ui::ParamKnob* legendReference = nullptr;

    std::vector<std::unique_ptr<ui::ParamKnob>> knobs;
    std::vector<std::unique_ptr<ui::LedButton>> leds;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::array<std::array<std::unique_ptr<juce::TextButton>, 14>, 2> toneButtons;
    std::vector<Placement> placements;
    KeyButton voiceOptions { "VOICE SETTINGS", KeyButton::Style::satin };
    KeyButton memoryLid { "OPEN MEMORY LID", KeyButton::Style::satin };
    std::array<KeyButton, 2> viewButtons { KeyButton("I", KeyButton::Style::plate), KeyButton("II", KeyButton::Style::plate) };
    std::array<KeyButton, 3> chorusKeys { KeyButton("OFF", KeyButton::Style::selector), KeyButton("CHORUS", KeyButton::Style::selector),
                                          KeyButton("TREMOLO", KeyButton::Style::selector) };
    std::array<ChannelTag, 2> channelTags { ChannelTag(false), ChannelTag(true) };
    std::array<TouchView, 2> touchViews;
    ui::ParamKnob* chorusSpeedKnob = nullptr;
    ui::ParamKnob* chorusDepthKnob = nullptr;
    juce::String stripKey;
    float stripEnd = 400.0f;
    std::vector<float> stripDividers;
    ui::LedButton* susToggle = nullptr;
    ui::LedButton* holdToggle = nullptr;
    ui::LedButton* wahToggle = nullptr;
    ui::LedButton* outputToggle = nullptr;
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
    MeterTicker meterTicker;
    juce::Component::SafePointer<juce::Component> animTarget;
    bool animOpening = true;
    double animStart = 0.0;
    static constexpr double animDurationMs = 140.0;

    bool advancedVisible = false;
    bool browserVisible = false;

    std::unique_ptr<ui::StudioMount> studio;   // [ws8-studio] declared last: destroyed first

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
