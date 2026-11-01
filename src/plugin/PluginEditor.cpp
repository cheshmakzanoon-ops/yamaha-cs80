#include "PluginEditor.h"

#include <map>
#include <tuple>

#include "AdvancedPanel.h"
#include "ArtCommon.h"
#include "PanelLayout.h"
#include "PanelWidgets.h"
#include "ChannelRow.h"
#include "CentreRow.h"
#include "PerformanceDeck.h"
#include "MemoryLid.h"
#include "ParameterWatch.h"
#include "EditorPreferences.h"
#include "StudioMount.h"   // [ws8-studio]
#include "KeyboardTuningPage.h"   // [integration] ws3 tuning page
#include "ModernModulationUi.h"   // [integration] ws5 pages
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

// Shared panel type, design-space floor and art live in ArtCommon.h.
using cs80::art::displayFont;
using cs80::art::spacedFont;
using cs80::art::textWidth;
using cs80::art::FloorScope;
// Panel keys, sections and displays (PanelWidgets.h, ChannelRow.h, CentreRow.h, PerformanceDeck.h, MemoryLid.h).
namespace panel = cs80::panel;
using cs80::panel::KeyButton;
using cs80::panel::Placement;

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

// Chassis geometry in the 1600 x 1000 design space (PanelLayout.h).
namespace geo = cs80::panel::geo;

// Chassis-only drawing helpers; the shared ones (ArtCommon.h) are reachable as art::*.
namespace art
{
using namespace cs80::art;
using Rf = juce::Rectangle<float>;
using juce::Colour;

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

// Eight voice lamps (two rows of eight in extension mode) and a MIDI-in lamp.
// Polls the lock-free, mode-consistent voice snapshot at 24 Hz.
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
            if (now.capacity != shown.capacity)
                setDescription(now.capacity == 16
                    ? "Sixteen voice lamps: the top row follows the primary eight voices and the bottom row the extension eight voices. The MIDI lamp flashes for host MIDI."
                    : "Eight lamps follow the sounding voices (up to eight at once). The MIDI lamp flashes for any MIDI the host sends: notes, controllers, pitch bend and pressure.");
            shown = now;
            repaint();
            if (onChange) onChange(shown.midi);
        }
    }

    void paint(juce::Graphics& g) override
    {
        // `refresh` owns snapshot publication and the accessibility callback;
        // painting must not consume a new mode before that callback sees it.
        const float sx = (float) getWidth() / geo::lampWell.getWidth();
        FloorScope floor(sx);
        g.addTransform(juce::AffineTransform::scale(sx, (float) getHeight() / geo::lampWell.getHeight()));
        art::legend(g, "VOICES", { 10.0f, 2.0f, 70.0f, 13.0f }, spacedFont(9.5f, 0.10f), juce::Justification::centredLeft,
                    juce::Colour(0xffd8c68a), 0.8f);
        art::legend(g, "MIDI", { 88.0f, 2.0f, 38.0f, 13.0f }, spacedFont(9.5f, 0.10f), juce::Justification::centred,
                    juce::Colour(0xffd8c68a), 0.8f);
        if (shown.capacity == 8)
        {
            // Preserve the default eight-voice drawing and screenshot exactly.
            for (int i = 0; i < 8; ++i)
                lamp(g, { 14.0f + (float) i * 10.5f, 27.0f }, 3.9f, ((shown.mask >> i) & 1u) != 0, juce::Colour(0xffffb53a));
        }
        else
        {
            // Both engines remain visible within the original 128x40 lamp well.
            for (int i = 0; i < 16; ++i)
                lamp(g, { 14.0f + (float) (i % 8) * 10.5f, i < 8 ? 22.5f : 32.5f }, 2.65f,
                     ((shown.mask >> i) & 1u) != 0, juce::Colour(0xffffb53a));
        }
        lamp(g, { 108.0f, 27.0f }, 4.4f, shown.midi, juce::Colour(0xff5cf08a));
    }

private:
    struct Sample
    {
        unsigned mask = 0;
        int capacity = 8;
        bool midi = false;
        bool operator==(const Sample& o) const noexcept { return mask == o.mask && capacity == o.capacity && midi == o.midi; }
        bool operator!=(const Sample& o) const noexcept { return ! (*this == o); }
    };

    Sample sample()
    {
        Sample s;
        const auto activity = processor.getVoiceActivitySnapshot();
        s.mask = activity.mask;
        s.capacity = activity.capacity;
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

// The CS-80 badge and wordmark double as the display-size menu (zoom presets). Only the hover glow and focus
// ring are drawn here; the mark itself is part of the buffered chassis.
class BrandButton final : public juce::Button
{
public:
    BrandButton() : juce::Button("Display size")
    {
        setTitle("Display size");
        setTooltip("Display size: 75, 100, 125, 150 or 200 percent of 1600 x 1000. New editor windows open at the chosen size when it fits the screen.");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
    }
    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced(1.0f);
        if (hovered || down)
        {
            g.setColour(juce::Colour(0xffffcf70).withAlpha(down ? 0.18f : 0.09f));
            g.fillRoundedRectangle(r, 4.0f);
        }
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawRoundedRectangle(r.expanded(0.5f), 4.0f, 1.5f);
        }
    }
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





} // namespace

struct Cs80AudioProcessorEditor::Impl final : public juce::Component
{
    explicit Impl(Cs80AudioProcessor& p)
        : processor(p),
          centreRow(p),
          deck(p),
          keybed(p),
          pitchWheel("PITCH", true),
          modWheel("MOD", false),
          advanced(p),
          browser(p),
          memoryLid(p)
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

        buildTopBar();
        addAndMakeVisible(centreRow);
        centreRow.onToneSelected = [this] { refreshRows(); };
        buildChannels();
        addAndMakeVisible(deck);
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
        addAndMakeVisible(memoryLid);
        memoryLid.setVisible(false);
        memoryLid.onClose = [this] {
            memoryLid.setVisible(false);
            if (getPeer() != nullptr) memoryLidKey.grabKeyboardFocus();
        };

        advanced.onClose = [this] { showAdvanced(false, true); };
        browser.onClose = [this] { showBrowser(false, true); };
        browser.isPatchEdited = [this] { return presetDirty; };
        browser.onPresetLoaded = [this]
        {
            syncPresetDisplay();
            centreRow.syncToneMarks();
            repaint();
        };
        frames.onFrame = [this] { stepOverlayAnimation(); };
        meterTicker.onTick = [this] { if (isShowing()) updateMeter(); };

        // Host state/program changes may arrive on non-message threads.
        // The message-thread timer polls metadata and selectors; parameter
        // changes only raise an atomic flag (ParameterChangeWatch). Never
        // install a processor callback that captures this editor.
        syncPresetDisplay();
        centreRow.syncToneMarks();
        syncSubMode();
        syncSourceStatus();
        refreshRows();

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
            // [integration] ws6's A/B compare: the inactive slot persists in the session state. Switching the
            // compared sound is one undo step.
            actions.selectCompare = [this](int slot)
            {
                if (juce::jlimit(0, 1, slot) != processor.getAbSlot())
                {
                    const Cs80AudioProcessor::ScopedUserEdit edit(processor.getUndoableState(), slot == 0 ? "Compare A" : "Compare B");
                    processor.abToggle();
                }
                centreRow.syncToneMarks();
            };
            actions.activeCompare = [this] { return processor.getAbSlot(); };
            actions.copyCompare = [this] { processor.abCopyCurrentTo(1 - processor.getAbSlot()); };
            actions.toggleStudio = [this] { showAdvanced(! advancedVisible, true); };
            actions.studioOpen = [this] { return advancedVisible; };
            actions.showEvidence = [this] { showAdvanced(true, true); advanced.showTab(ui::StudioTab::settings); };
            actions.savePreset = [this] { if (saveButton.onClick) saveButton.onClick(); };
            // [integration] a new preset starts like a fresh instance (ws2, plan 1.7).
            actions.initPatch = [this] { processor.loadInitPatch(); centreRow.syncToneMarks(); };
            // [integration] ws6's randomise: original panel controls inside their hardware ranges, one undo step.
            actions.randomise = [this]
            {
                const Cs80AudioProcessor::ScopedUserEdit edit(processor.getUndoableState(), "Randomise");
                processor.randomiseWithinOriginalRanges((juce::uint32) juce::Random::getSystemRandom().nextInt());
                centreRow.syncToneMarks();
            };
            actions.toggleScope = [this]
            {
                studio->scope.setVisible(! studio->scope.isVisible());
                if (studio->scope.isVisible()) studio->scope.toFront(true);
            };
            actions.scopeOpen = [this] { return studio->scope.isVisible(); };
            actions.toggleControllers = [this] { studio->setControllersShown(! studio->areControllersShown()); };
            actions.controllersShown = [this] { return studio->areControllersShown(); };
            actions.toggleQwerty = [this] { studio->setQwertyEnabled(! studio->qwerty.isEnabled()); };
            actions.qwertyEnabled = [this] { return studio->qwerty.isEnabled(); };
            actions.toggleCircuitNotes = [this] { studio->setCircuitNotesShown(! ui::circuitNotesInTooltips()); };
            actions.circuitNotesShown = [] { return ui::circuitNotesInTooltips(); };
            actions.midiLearnCount = [this] { return processor.getMidiLearn().count(); };
            actions.clearMidiLearn = [this] { processor.getMidiLearn().clearAll(); };
            actions.undo = [this] { processor.undo(); centreRow.syncToneMarks(); };
            actions.redo = [this] { processor.redo(); centreRow.syncToneMarks(); };
            actions.canUndo = [this] { return processor.getUndoManager().canUndo(); };
            actions.canRedo = [this] { return processor.getUndoManager().canRedo(); };
            actions.undoName = [this] { return processor.getUndoManager().getUndoDescription(); };
            actions.redoName = [this] { return processor.getUndoManager().getRedoDescription(); };
            actions.startTour = [this] { studio->tour.start(); };
            actions.zoomPresets.assign(panel::EditorPreferences::zoomPresets.begin(), panel::EditorPreferences::zoomPresets.end());
            actions.setZoom = [this](int percent) { if (onZoom) onZoom(percent); };
            actions.zoom = [this] { return currentZoom ? currentZoom() : 0; };
            studio->shortcuts.undo = actions.undo;
            studio->shortcuts.redo = actions.redo;
            studio->shortcuts.save = actions.savePreset;
            studio->toolbar.setActions(std::move(actions));
        }
        studio->scope.onClose = [this] { studio->scope.setVisible(false); };
        addKeyListener(&studio->qwerty);                // computer-keyboard notes through the on-screen keyboard's state
        addKeyListener(&studio->shortcuts);             // undo, redo and save
        addChildComponent(studio->tour);
        {
            // Tour steps: plain words over the area they describe (editor pixels at the current size).
            auto design = [this](float x, float y, float w, float h)
            {
                const float sx = (float) getWidth() / kDesignWidth, sy = (float) getHeight() / kDesignHeight;
                return juce::Rectangle<float>(x * sx, y * sy, w * sx, h * sy).toNearestInt();
            };
            auto keys = [this](std::initializer_list<const char*> ids)
            {
                juce::Rectangle<int> area;
                for (const char* id : ids)
                    for (auto* child : studio->toolbar.getChildren())
                        if (child->getComponentID() == id)
                            area = area.isEmpty() ? getLocalArea(child, child->getLocalBounds())
                                                  : area.getUnion(getLocalArea(child, child->getLocalBounds()));
                return area;
            };
            auto bounds = [this](juce::Component& c) { return getLocalArea(&c, c.getLocalBounds()); };
            studio->tour.setSteps({
                { "Welcome to the CS-80",
                  "This is the instrument's own panel, laid out as on the original: two channels of faders, the tone selector, the sub "
                  "oscillator and ring modulator, the ribbon and the keyboard. What the original does not have lives in the toolbar and "
                  "the Studio view.", {} },
                { "Programs",
                  "The display names the program. The arrows step through the library and LIBRARY opens it to browse, search, load and "
                  "save. EDITED means the sound has changed since it was loaded.",
                  [keys] { return keys({ "toolbar_previous", "toolbar_program", "toolbar_next", "toolbar_library" }); } },
                { "Faders work like the hardware",
                  "Drag a fader by its cap: it never jumps. Hold Shift for fine moves, Ctrl-click (Cmd-click on a Mac) to type a value, "
                  "double-click to reset. Right-click any control for MIDI learn, copy and paste, and a help card with its owner's-manual "
                  "page.", [design] { const auto f = geo::rowFrame(0); return design(f.getX(), f.getY(), f.getWidth(), f.getHeight()); } },
                { "Tone selector",
                  "Each channel has its own row of the original preset tabs. PANEL plays the faders; a tab or a MEMORY plays its own "
                  "settings for the 26 programmer controls.",
                  [this, bounds] { return bounds(centreRow.getToneButton(0, 0)).getUnion(bounds(centreRow.getToneButton(1, 13))); } },
                { "Ribbon and keys",
                  "Touch the ribbon anywhere and slide to bend the pitch; lift off to return. Drag down on a held key for pressure. The "
                  "computer keyboard plays too: A W S E D F T G Y H U J K, with Z and X for octaves.",
                  [this, bounds] { return bounds(deck.getRibbon()).getUnion(bounds(keybed)); } },
                { "Engine and PURE CS-80",
                  "ENGINE: CIRCUIT runs the circuit-informed models; LEGACY keeps old sessions sounding as they did. PURE CS-80 switches "
                  "off every modern addition. Neither is a calibrated match to a particular instrument.",
                  [keys] { return keys({ "toolbar_engine_circuit", "toolbar_engine_legacy", "toolbar_pure_cs80" }); } },
                { "Compare and undo",
                  "A and B hold two versions of a sound to compare. UNDO and REDO step through your edits (Ctrl/Cmd+Z); host automation "
                  "is never recorded, and loading a program starts a fresh history.",
                  [keys] { return keys({ "toolbar_compare_a", "toolbar_compare_b", "toolbar_undo", "toolbar_redo" }); } },
                { "The Studio view",
                  "STUDIO opens the modern layer: LFOs and wheels, MIDI learn and the computer keyboard, effects, the arpeggiator, and "
                  "Settings with the evidence register, which says what each part of the sound rests on.",
                  [keys] { return keys({ "toolbar_studio" }); } },
                { "Menu and help",
                  "The menu saves and initialises programs, opens the voice monitor and the controllers strip, and runs this tour again. "
                  "Hover a control for its tooltip and hold Shift for its circuit notes; press F1 on a focused control for its help card.",
                  [keys] { return keys({ "toolbar_menu" }); } },
            });
        }
        advanced.attachQwerty(&studio->qwerty);
        // [integration] ws5's modern pages and ws3's tuning page as further sub-pages of their tabs. The
        // built-in pages stay: none of these replaces all of a tab's controls.
        advanced.addPage(ui::StudioTab::modulation, std::make_unique<ui::modern_layer::ModernModulationPage>(processor), "MSEG + MATRIX");
        advanced.addPage(ui::StudioTab::keyboard, std::make_unique<ui::modern_layer::KeyboardTuningPage>(processor), "TUNING + VOICES");
        advanced.addPage(ui::StudioTab::keyboard, std::make_unique<ui::modern_layer::KeyboardModulationPage>(processor), "CURVES");
        advanced.addPage(ui::StudioTab::fx, std::make_unique<ui::modern_layer::ModernFxPage>(processor), "MODERN FX");
        deck.getRibbon().fullRangeFromTouch = [this] { return processor.getParamValue(cs80::P_parityMode) < 0.5f; };
        // [ws8-studio] end
    }

    ~Impl() override
    {
        if (studio != nullptr) { removeKeyListener(&studio->qwerty); removeKeyListener(&studio->shortcuts); }   // [ws8-studio]
        meterTicker.onTick = nullptr;
        frames.stop();
        setLookAndFeel(nullptr);
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key != juce::KeyPress::escapeKey) return false;
        if (memoryLid.isVisible())
        {
            if (memoryLid.onClose) memoryLid.onClose();
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
        ui::bindParameterContextMenu(*raw, processor, Cs80AudioProcessor::parameterId(parameter), parameter);
        place(*raw, x, y, w, h);
        leds.push_back(std::move(button));
        return *raw;
    }

    void buildTopBar()
    {
        addAndMakeVisible(brandButton);
        brandButton.onClick = [this] { showZoomMenu(); };
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
            centreRow.syncToneMarks();
        };
        advancedButton.setClickingTogglesState(true);
        advancedButton.onClick = [this] { showAdvanced(! advancedVisible, true); };
        syncLampStatus(false);
    }

    // The display-size menu: the zoom presets, with the current one ticked.
    void showZoomMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader("DISPLAY SIZE");
        const int current = currentZoom ? currentZoom() : 0;
        for (int percent : panel::EditorPreferences::zoomPresets)
            menu.addItem(percent, juce::String(percent) + " %   (" + juce::String(16 * percent) + " x " + juce::String(10 * percent) + ")",
                         true, percent == current);
        juce::Component::SafePointer<Impl> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&brandButton),
                           [safe](int chosen) { if (safe != nullptr && chosen > 0 && safe->onZoom) safe->onZoom(chosen); });
    }
    std::function<void(int)> onZoom;
    std::function<int()> currentZoom;

    // Printed status text on the panel: cream, bright enough to read against the crinkle at a glance.
    static juce::Colour stripInk() { return juce::Colour(0xffeee4c4); }

    // The sections that own panel controls, in panel order.
    std::array<panel::PanelSection*, 4> sections() { return { &centreRow, channelRows[0].get(), channelRows[1].get(), &deck }; }

    void applyCaptions()
    {
        const bool small = getWidth() < 1200;
        for (auto* section : sections()) section->applyCaptions(small);
    }

    void buildChannels()
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            channelRows[(size_t) channel] = std::make_unique<panel::ChannelRow>(processor, channel);
            addAndMakeVisible(*channelRows[(size_t) channel]);
        }
        // Software-only voice settings (pan, a second LEVEL, filter switches, wheels): none is on the original
        // panel, so they open over the lid's printed diagram on request.
        addAndMakeVisible(voiceOptions);
        voiceOptions.setButtonText("VOICE SETTINGS");
        voiceOptions.setTitle("Show software voice settings");
        voiceOptions.setWantsKeyboardFocus(true);
        voiceOptions.onClick = [this] {
            optionsVisible = ! optionsVisible;
            voiceOptions.setToggleState(optionsVisible, juce::dontSendNotification);
            for (auto* component : optionComponents) component->setVisible(optionsVisible);
            backdrop.repaint();
            repaint();
        };
        place(voiceOptions, geo::voiceKey.getX(), geo::voiceKey.getY(), geo::voiceKey.getWidth(), geo::voiceKey.getHeight());
        addAndMakeVisible(memoryLidKey);
        memoryLidKey.setButtonText("OPEN MEMORY LID");
        memoryLidKey.setTitle("Open memory programmer");
        memoryLidKey.setWantsKeyboardFocus(true);
        memoryLidKey.onClick = [this] {
            showAdvanced(false);
            showBrowser(false);
            memoryLid.refresh();
            memoryLid.setVisible(true);
            memoryLid.toFront(true);
            memoryLidKey.setToggleState(true, juce::dontSendNotification);
        };
        place(memoryLidKey, geo::memoryLidKey.getX(), geo::memoryLidKey.getY(), geo::memoryLidKey.getWidth(), geo::memoryLidKey.getHeight());
        for (int channel = 0; channel < 2; ++channel)
        {
            const float y = 176.0f + (float) channel * 82.0f;
            auto p = [channel](int local) { return cs80::chParam(channel, local); };
            auto& pan = addKnob(p(cs80::CP_pan), 80, y, 56, 72, "PAN");
            auto& level = addKnob(p(cs80::CP_vLevel), 138, y, 56, 72, "LEVEL");
            auto& hp = addLed(p(cs80::CP_hpfOn), "HPF", 198, y + 10, 52, 24);
            auto& lp = addLed(p(cs80::CP_lpfOn), "LPF", 198, y + 40, 52, 24);
            optionComponents.insert(optionComponents.end(), { &pan, &level, &hp, &lp });
        }
        for (auto* component : optionComponents) component->setVisible(false);
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
        // [integration] ws6: step through the library's current filter (scope, category, search), user presets
        // included, instead of the whole host list (Legacy Library first).
        browser.loadAdjacent(delta < 0 ? -1 : 1);
        centreRow.syncToneMarks();
    }

    void syncPresetDisplay()
    {
        const auto metadata = processor.getPresetMetadata();
        // Nothing below can change unless the program metadata or a parameter changed: skip the bank lookup
        // and the full-parameter comparison on the ordinary idle tick.
        const auto displayKey = metadata.name + "|" + juce::String(metadata.factoryIndex);
        if (! parameterWatch.consume() && displayKey == shownDisplayKey)
            return;
        shownDisplayKey = displayKey;
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
        const auto numberInk = isFactory ? juce::Colour(0xffffb347) : juce::Colour(0xffc28c42);   // USR: 4.9:1 on the lit well
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
        if (memoryLid.isVisible()) memoryLid.toFront(false);
    }

    void showAdvanced(bool shouldShow, bool animate = false)
    {
        finishOverlayAnimation();
        const bool wasVisible = advancedVisible;
        if (shouldShow) memoryLid.setVisible(false);
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
        if (shouldShow) memoryLid.setVisible(false);
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
        auto* subFunction = centreRow.getSubFunction();
        auto* subSpeed = centreRow.getSubSpeed();
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

    // Each row's numeral lights while that row is heard (its selector on PANEL).
    void refreshRows()
    {
        for (auto& row : channelRows)
            row->refresh();
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
            label->setMinimumPixels(art::kLegendFloorPx);
            label->setLeftPad(0);
            auto font = label->getFont();
            if (font.getHeight() < art::kLegendFloorPx) font = font.withHeight(art::kLegendFloorPx);
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
        const auto activity = processor.getVoiceActivitySnapshot();
        const int voices = activity.activeCount();
        // The toolbar lamps carry voices and MIDI; the strip keeps the (hidden) text for accessibility only.
        voiceStatusLabel.setDisplayText({});
        midiStatusLabel.setDisplayText({});
        voiceStatusLabel.setInterceptsMouseClicks(false, false);
        midiStatusLabel.setInterceptsMouseClicks(false, false);
        voiceStatusLabel.setText("Active voices: " + juce::String(voices) + " of " + juce::String(activity.capacity), juce::dontSendNotification);
        midiStatusLabel.setText(midi ? "MIDI input: notes arriving" : "MIDI input: idle", juce::dontSendNotification);
    }

    void tick()
    {
        processor.serviceKeyboardUI();
        keyboard.repaint();
        syncSubMode();
        syncPresetDisplay();
        centreRow.syncToneMarks();
        syncSourceStatus();
        layoutStrip(false);
        updateMeter();
        refreshRows();
        deck.refresh();
        if (armedKey != nullptr && juce::Time::getMillisecondCounterHiRes() >= armDeadlineMs) disarmKey();
        if (memoryLid.isVisible()) memoryLid.refresh();
        memoryLidKey.setToggleState(memoryLid.isVisible(), juce::dontSendNotification);
        lamps.refresh();
        // Each live element repaints itself only when its content changes; the
        // chassis is a cached image, so no blanket repaint is needed here.
    }

    // -----------------------------------------------------------------------
    // Painting
    // -----------------------------------------------------------------------
    // The closed memory lid at the upper left (owner index 46): a hinged plate in the panel's own finish, printed
    // with a block diagram of the voice and carrying its two keys on its front rail. The print is a stylised
    // diagram of this instrument's signal path, not a copy of the original print, which could not be read
    // (docs/PANEL_CONFORMANCE.md). With VOICE SETTINGS on, a software tray covers the diagram.
    void paintLid(juce::Graphics& g, float texel)
    {
        const auto r = geo::lidFace;
        art::ambient(g, r, 5.0f, 12.0f, 0.55f);
        ui::softShadow(g, r, 5.0f, 0.0f, 4.0f, 10.0f, 0.75f);
        g.setGradientFill(art::vgrad(juce::Colour(0xff4c4943), juce::Colour(0xff1a1916), r));
        g.fillRoundedRectangle(r, 5.0f);
        const auto plate = r.reduced(4.0f);
        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path clip;
            clip.addRoundedRectangle(plate, 3.0f);
            g.reduceClipRegion(clip);
            art::fillTile(g, art::panelTile(), plate, texel, 7.0f, 3.0f);
            g.setGradientFill(art::vgrad(art::white(0.05f), juce::Colours::transparentWhite, plate.withHeight(80.0f)));
            g.fillRect(plate);
        }
        g.setColour(art::white(0.22f));
        g.drawLine(r.getX() + 5.0f, r.getY() + 1.0f, r.getRight() - 5.0f, r.getY() + 1.0f, 1.0f);
        g.setColour(art::black(0.85f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
        // Hinge barrel along the back edge, with two knuckles.
        const juce::Rectangle<float> hinge(r.getX() + 12.0f, r.getY() - 2.0f, r.getWidth() - 24.0f, 7.0f);
        art::brushedMetal(g, hinge, 2.0f, 0.0f, 43);
        g.setColour(art::black(0.8f));
        g.drawRoundedRectangle(hinge, 2.0f, 1.0f);
        for (float kx : { hinge.getX() + 40.0f, hinge.getRight() - 40.0f })
        {
            const juce::Rectangle<float> knuckle(kx - 18.0f, hinge.getY() - 1.0f, 36.0f, hinge.getHeight() + 2.0f);
            art::brushedMetal(g, knuckle, 2.5f, 0.3f, (int) kx);
            g.setColour(art::black(0.85f));
            g.drawRoundedRectangle(knuckle, 2.5f, 1.0f);
        }
        art::legend(g, "MEMORY", { plate.getX() + 10.0f, plate.getY() + 8.0f, 120.0f, 14.0f }, spacedFont(12.0f, 0.18f),
                    juce::Justification::centredLeft, juce::Colour(0xffe9d695));
        art::legend(g, "LIFT TO OPEN", { plate.getRight() - 130.0f, plate.getY() + 8.0f, 120.0f, 14.0f }, spacedFont(9.0f, 0.12f),
                    juce::Justification::centredRight, juce::Colour(0xffb5a878), 0.5f);
        art::etchRect(g, plate.reduced(4.0f), 3.0f, 0.45f);
        art::etchLine(g, plate.getX() + 8.0f, geo::memoryLidKey.getY() - 8.0f, plate.getRight() - 8.0f, geo::memoryLidKey.getY() - 8.0f, 0.35f);
        if (optionsVisible)
            paintVoiceSettingsTray(g);
        else
            paintBlockDiagram(g, geo::lidDiagram);
    }

    // The printed diagram: per channel VCO -> HPF -> LPF -> VCA with the sine path around the filters; the sub
    // oscillator and ring modulator feeding the voices; both channels mixed into tremolo/chorus and the output.
    static void paintBlockDiagram(juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto ink = juce::Colour(0xffe6dcbc);
        const auto font = spacedFont(9.0f, 0.06f);
        auto box = [&](juce::Rectangle<float> b, const juce::String& text)
        {
            g.setColour(art::black(0.35f));
            g.fillRoundedRectangle(b, 2.0f);
            art::etchRect(g, b, 2.0f, 0.75f);
            art::legend(g, text, b, font, juce::Justification::centred, ink, 0.5f);
        };
        auto arrow = [&](float x0, float y, float x1)
        {
            art::etchLine(g, x0, y, x1 - 3.0f, y, 0.75f);
            juce::Path head;
            head.addTriangle(x1, y, x1 - 5.0f, y - 2.8f, x1 - 5.0f, y + 2.8f);
            g.setColour(juce::Colour(0xffd6bb68).withAlpha(0.68f));
            g.fillPath(head);
        };
        const float bw = 44.0f, bh = 19.0f, step = 60.0f;
        const float x0 = area.getX() + 26.0f;
        const char* stages[] = { "VCO", "HPF", "LPF", "VCA" };
        for (int channel = 0; channel < 2; ++channel)
        {
            const float y = area.getY() + 10.0f + (float) channel * 72.0f;
            art::legend(g, channel == 0 ? "I" : "II", { area.getX(), y, 22.0f, bh }, spacedFont(11.0f, 0.04f),
                        juce::Justification::centred, ink, 0.6f);
            for (int i = 0; i < 4; ++i)
            {
                const juce::Rectangle<float> b(x0 + (float) i * step, y, bw, bh);
                box(b, stages[i]);
                arrow(b.getRight() + 1.0f, b.getCentreY(), b.getRight() + step - bw - 1.0f);
            }
            // sine path from the oscillator around both filters into the amplifier
            const float sy = y + bh + 9.0f, sx0 = x0 + bw * 0.5f, sx1 = x0 + 3.0f * step + bw * 0.5f;
            art::etchLine(g, sx0, y + bh, sx0, sy, 0.6f);
            art::etchLine(g, sx0, sy, sx1, sy, 0.6f);
            art::etchLine(g, sx1, sy, sx1, y + bh + 1.0f, 0.6f);
            art::legend(g, "SINE", { sx0 + 40.0f, sy - 1.0f, 60.0f, 11.0f }, spacedFont(8.0f, 0.10f),
                        juce::Justification::centred, juce::Colour(0xffcfc4a2), 0.0f);
        }
        // modulation sources under the voices, and the common output chain
        const float my = area.getY() + 166.0f;
        box({ x0, my, 70.0f, bh }, "SUB OSC");
        box({ x0 + 82.0f, my, 70.0f, bh }, "RING MOD");
        const float busX = x0 + 3.0f * step + bw + 10.0f;
        art::etchLine(g, busX, area.getY() + 19.5f, busX, my + bh + 36.0f, 0.6f);
        const juce::Rectangle<float> out(x0 + 82.0f, my + 46.0f, busX - x0 - 82.0f - 6.0f, bh);
        box(out, "TREMOLO / CHORUS");
        arrow(busX, out.getCentreY(), out.getRight() + 1.0f);
        art::legend(g, "OUT", { out.getX() - 42.0f, out.getY(), 36.0f, bh }, font, juce::Justification::centredRight, ink, 0.5f);
        juce::Path tip;
        tip.addTriangle(out.getX() - 1.0f, out.getCentreY(), out.getX() + 4.0f, out.getCentreY() - 2.8f, out.getX() + 4.0f, out.getCentreY() + 2.8f);
        g.setColour(juce::Colour(0xffd6bb68).withAlpha(0.68f));
        g.fillPath(tip);
    }

    void paintVoiceSettingsTray(juce::Graphics& g)
    {
        const auto well = geo::lidOptions;
        art::recessTray(g, well, 3.0f, juce::Colour(0xff15140f), juce::Colour(0xff0b0a09), 8.0f, 0.8f);
        art::haloText(g, "VOICE SETTINGS  /  SOFTWARE", { well.getX() + 8, well.getY() + 4, 240, 12 }, spacedFont(8.5f, 0.14f),
                      juce::Justification::centredLeft, juce::Colour(0xffcdb878), juce::Colour(0xff13120e));
        for (int channel = 0; channel < 2; ++channel)
            art::legend(g, channel == 0 ? "I" : "II", { well.getX() + 2.0f, 176.0f + (float) channel * 82.0f + 22.0f, 16.0f, 28.0f },
                        spacedFont(12.0f, 0.04f), juce::Justification::centred, juce::Colour(0xffe6dcbc), 0.55f);
        art::etchLine(g, well.getX() + 8.0f, 254.0f, well.getRight() - 72.0f, 254.0f, 0.35f);
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
            if (! art::isSmallEditor())
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
                if (m.first == -10.0f || (art::isSmallEditor() && m.first == -60.0f)) continue;
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
        // Device pixels per design unit, including the display's own scale: textures are built and tiled at this
        // resolution, so the crinkle and the grain stay one texel per physical pixel on HiDPI screens.
        const float texel = g.getInternalContext().getPhysicalPixelScaleFactor();
        g.fillAll(juce::Colour(0xff070605));

        // ---- one fine crinkle finish for the toolbar, panel and deck ---------
        const juce::Rectangle<float> body(geo::bodyLeft, 0.0f, geo::bodyRight - geo::bodyLeft, geo::deckBottom);
        art::fillTile(g, art::panelTile(), body, texel);
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
        seamGroove(g, geo::rowY0[1] + geo::rowTop - 3.0f);
        seamGroove(g, geo::frontStrip.getY() - 4.0f);
        for (auto& row : channelRows)
            row->paintArt(g);
        paintLid(g, texel);

        // ---- front strip: PITCH, ring, sub, feet | tone selector | global, keyboard, touch, VOLUME
        centreRow.paintArt(g);
        // knobs stand on the panel: a soft contact shadow all around the cap
        auto knobShadows = [&g](const std::vector<Placement>& items)
        {
            for (const auto& item : items)
                if (auto* knob = dynamic_cast<ui::ParamKnob*>(item.component);
                    knob != nullptr && knob->isVisible() && knob->getSlider().getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag)
                {
                    const float d = juce::jmin(item.bounds.getWidth() - 10.0f, 46.0f);
                    const float cy = item.bounds.getHeight() > 100.0f ? item.bounds.getCentreY() : item.bounds.getY() + 7.0f + d * 0.5f;
                    art::ambient(g, juce::Rectangle<float>(d, d).withCentre({ item.bounds.getCentreX(), cy }),
                                 d * 0.5f, 5.0f, 0.42f);
                }
        };
        for (auto* section : sections())
            knobShadows(section->getPlacements());
        knobShadows(placements);

        // front edge of the control surface over the deck
        const float edge = geo::faceBottom - 8.0f;
        g.setColour(art::white(0.05f));
        g.fillRect(geo::bodyLeft, edge, geo::bodyRight - geo::bodyLeft, 8.0f);
        g.setColour(art::white(0.20f));
        g.fillRect(geo::bodyLeft, edge, geo::bodyRight - geo::bodyLeft, 1.0f);
        g.setColour(art::black(0.9f));
        g.fillRect(geo::bodyLeft, geo::faceBottom - 1.0f, geo::bodyRight - geo::bodyLeft, 1.5f);

        deck.paintArt(g);

        // ---- screws on the control face -----------------------------------
        for (float x : { 61.0f, 1539.0f })
            art::slotScrew(g, { x, edge - 7.0f }, 4.4f, (x + edge - 7.0f) * 0.7f);
        paintToolbar(g);

        // ---- keybed base (the Keybed component paints over this) -----------
        g.setColour(juce::Colour(0xff0a0908));
        g.fillRect(geo::keybed);

        // ---- walnut cheeks run the full height of the case, over everything --
        art::walnutCheek(g, { 0, 0, geo::bodyLeft, kDesignHeight }, true, geo::deckBottom, texel);
        art::walnutCheek(g, { geo::bodyRight, 0, kDesignWidth - geo::bodyRight, kDesignHeight }, false, geo::deckBottom, texel);
        g.restoreState();
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
        spill.setBounds(getLocalBounds());
        applyCaptions();
        for (const auto& item : placements)
            item.component->setBounds(r(item.bounds.getX(), item.bounds.getY(),
                                        item.bounds.getWidth(), item.bounds.getHeight()));
        centreRow.layout(centreRow.designArea(), sx, sy);
        for (auto& row : channelRows)
            row->layout(row->designArea(), sx, sy);
        deck.layout(deck.designArea(), sx, sy);

        brandButton.setBounds(rr(geo::brandBadge.getUnion(geo::brandText)));
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
        outputMeterLabel.setMinimumPixels(art::kLegendFloorPx);
        outputMeterLabel.setJustificationType(juce::Justification::centredLeft);
        outputMeterLabel.setLines(1);
        outputMeterLabel.setLeftPad(juce::roundToInt(2.0f * sx));
        meterBar.setBounds(rr(geo::meterBar));
        voiceStatusLabel.setBounds(rr(geo::voiceStatus));
        midiStatusLabel.setBounds(rr(geo::midiStatus));
        layoutStrip(true);

        pitchWheel.setBounds(r(258.0f, 176.0f, 30.0f, 150.0f));
        modWheel.setBounds(r(296.0f, 176.0f, 30.0f, 150.0f));
        keybed.setBounds(rr(geo::keybed));

        scrim.setBounds(rr(geo::overlayBody));
        shade.setBounds(rr(geo::overlayBody));
        advanced.setBounds(rr(geo::overlay));
        browser.setBounds(rr(geo::overlay));
        memoryLid.setBounds(rr(geo::memoryLid));
        // [ws8-studio] begin: toolbar over the program cluster (x 250-1114), drawers over the panel and keybed.
        if (studio != nullptr)
        {
            studio->toolbar.setBounds(r(250.0f, 2.0f, ui::StudioToolbar::kDesignW, ui::StudioToolbar::kDesignH));
            studio->scope.setBounds(r(56.0f, 70.0f, ui::ScopeDrawer::kDesignW, ui::ScopeDrawer::kDesignH));
            studio->controllers.setBounds(r(52.0f, 758.0f, ui::ControllersStrip::kDesignW, ui::ControllersStrip::kDesignH));
            studio->tour.setBounds(getLocalBounds());
        }
        // [ws8-studio] end
    }

    Cs80AudioProcessor& processor;
    Backdrop backdrop;
    ui::LightSpill spill;
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
    BrandButton brandButton;
    ActivityLamps lamps { processor };
    PanelLabel presetNumberLabel;
    PanelLabel voiceStatusLabel;
    PanelLabel midiStatusLabel;
    panel::ParameterChangeWatch parameterWatch { processor };
    juce::String shownDisplayKey;
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

    std::vector<std::unique_ptr<ui::ParamKnob>> knobs;
    std::vector<std::unique_ptr<ui::LedButton>> leds;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<Placement> placements;
    panel::CentreRow centreRow;
    std::array<std::unique_ptr<panel::ChannelRow>, 2> channelRows;
    panel::PerformanceDeck deck;
    KeyButton voiceOptions { "VOICE SETTINGS", KeyButton::Style::satin };
    KeyButton memoryLidKey { "OPEN MEMORY LID", KeyButton::Style::satin };
    juce::String stripKey;
    float stripEnd = 400.0f;
    std::vector<float> stripDividers;
    std::vector<juce::Component*> optionComponents;
    bool optionsVisible = false;

    cs80::ui::Keybed keybed;
    ui::HardwareKeyboard& keyboard = keybed.keys;
    ui::PerformanceWheel pitchWheel;
    ui::PerformanceWheel modWheel;

    OverlayScrim scrim;
    ui::AdvancedPanel advanced;
    ui::PresetBrowser browser;
    OverlayShade shade;
    panel::MemoryLid memoryLid;

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
    setResizeLimits(1120, 700, 3200, 2000);   // the 1120 x 700 minimum up to the 200 % display-size preset
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
    auto initialSize = display != nullptr
        ? initialEditorSizeForArea(display->userArea.getWidth(), display->userArea.getHeight())
        : initialEditorSizeForArea(0, 0);
    // A remembered display-size preset wins when it fits the same room the automatic size leaves for host chrome.
    if (const int zoom = cs80::panel::EditorPreferences::loadZoomPercent(); zoom > 0 && display != nullptr
        && 16 * zoom <= display->userArea.getWidth() - 32 && 10 * zoom <= display->userArea.getHeight() - 64)
        initialSize = { 16 * zoom, 10 * zoom };
    setSize(initialSize.width, initialSize.height);
    impl->onZoom = [this](int percent) { setZoomPercent(percent); };
    impl->currentZoom = [this] { return getZoomPercent(); };

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

void Cs80AudioProcessorEditor::setZoomPercent(int percent)
{
    if (! cs80::panel::EditorPreferences::isZoomPreset(percent))
        return;
    setSize(16 * percent, 10 * percent);
    cs80::panel::EditorPreferences::saveZoomPercent(percent);
}

int Cs80AudioProcessorEditor::getZoomPercent() const
{
    for (int percent : cs80::panel::EditorPreferences::zoomPresets)
        if (getWidth() == 16 * percent && getHeight() == 10 * percent)
            return percent;
    return 0;
}

bool Cs80AudioProcessorEditor::isAdvancedVisible() const
{
    return impl != nullptr && impl->advancedVisible;
}
