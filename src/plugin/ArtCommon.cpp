#include "ArtCommon.h"

#include "UiComponents.h"

#include <PanelFontData.h>

#include <cmath>
#include <vector>

namespace cs80::art
{
namespace
{
float floorPx = 0.0f;
bool smallEditor = false;
struct PanelResources
{
    juce::Typeface::Ptr medium, semiBold;
    juce::Image tile;
};
PanelResources& panelResources()
{
    static PanelResources resources;
    return resources;
}
} // namespace

FloorScope::FloorScope(float screenScale) : previous(floorPx), previousSmall(smallEditor)
{
    floorPx = screenScale > 0.0f ? kLegendFloorPx / screenScale : 0.0f;
    smallEditor = screenScale > 0.0f && screenScale < 0.85f;
}

FloorScope::~FloorScope()
{
    floorPx = previous;
    smallEditor = previousSmall;
}

float designFloor() noexcept { return floorPx; }
bool isSmallEditor() noexcept { return smallEditor; }

// The embedded panel typeface: Barlow Condensed Medium for plain text and SemiBold for bold, loaded once.
// If a face cannot be loaded the system sans serif is used, so the panel still draws.
const juce::Typeface::Ptr& panelTypeface(bool bold)
{
    auto& resources = panelResources();
    if (resources.medium == nullptr)
        resources.medium = juce::Typeface::createSystemTypefaceFor(
            cs80_panel_fonts::BarlowCondensedMedium_ttf, (size_t) cs80_panel_fonts::BarlowCondensedMedium_ttfSize);
    if (resources.semiBold == nullptr)
        resources.semiBold = juce::Typeface::createSystemTypefaceFor(
            cs80_panel_fonts::BarlowCondensedSemiBold_ttf, (size_t) cs80_panel_fonts::BarlowCondensedSemiBold_ttfSize);
    return bold ? resources.semiBold : resources.medium;
}

void releasePanelResources() { panelResources() = {}; }

juce::Font displayFont(float height, int style)
{
    const float h = juce::jmax(height, floorPx);
    const bool bold = (style & juce::Font::bold) != 0;
    if (const auto& face = panelTypeface(bold); face != nullptr)
    {
        auto font = juce::Font(juce::FontOptions(face).withHeight(h));
        if ((style & juce::Font::italic) != 0) font.setItalic(true);
        return font;
    }
    return juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), h, style));
}

juce::Font spacedFont(float height, float kerning, float hscale, int style)
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

namespace ui = cs80::ui;

void fillRelief(juce::Graphics& g, const juce::Image& tile, Rf area, float deviceScale,
                float opacity, float shiftX, float shiftY)
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
void recessEdge(juce::Graphics& g, Rf r, float radius, float strength)
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

void recessTray(juce::Graphics& g, Rf r, float radius, Colour top, Colour bottom, float depth, float shade)
{
    g.setGradientFill(vgrad(top, bottom, r));
    g.fillRoundedRectangle(r, radius);
    insetShadow(g, r, radius, depth, shade);
    recessEdge(g, r, radius);
}

void raisedPlate(juce::Graphics& g, Rf r, float radius, Colour top, Colour bottom, float lift)
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

// Printed legend with a drop shadow, in the cream silkscreen colour.
void legend(juce::Graphics& g, const juce::String& text, Rf area, const juce::Font& font,
            juce::Justification just, Colour ink, float shadow)
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

// Gold line art of the panel (frames, grouping rules): 1.25 px of gold at about 55 percent over a 1 px dark
// shadow offset down and to the right, so the line reads as etched into the enamel and holds up at 1x.
float etchAlpha(float a) { return a < 0.4f ? a : 0.40f + 0.28f * a; }

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
             float tick, float fontSize, const juce::String& shortTitle)
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
        etchLine(g, x, ruleY, x + w, ruleY, 0.8f);
    else
    {
        etchLine(g, x, ruleY, mid - gap, ruleY, 0.8f);
        etchLine(g, mid + gap, ruleY, x + w, ruleY, 0.8f);
    }
    etchLine(g, x, ruleY, x, ruleY + tick, 0.8f);
    etchLine(g, x + w, ruleY, x + w, ruleY + tick, 0.8f);
    if (text.isNotEmpty())
        legend(g, text, { x, ruleY - 8.0f, w, 16.0f }, font);
}

// Soft contact shadow all around an object that stands on the panel (no offset, so it also darkens
// the seam above it): tone tray, nameplate, knobs, row seams.
void ambient(juce::Graphics& g, Rf r, float radius, float spread, float alpha)
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
    auto& tile = panelResources().tile;
    if (tile.isNull()) tile = makeWrinkleTile(256, 5, Colour(0xff1d1c19), 0.46f);
    return tile;
}

void fillTile(juce::Graphics& g, const juce::Image& tile, Rf area, float deviceScale, float shiftX, float shiftY)
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

// YAMAHA-style engraved lettering on metal: the cut is darker, lit on its lower right lip,
// shadowed on its upper left wall.
void engraved(juce::Graphics& g, const juce::String& text, Rf area, const juce::Font& font,
              juce::Justification just)
{
    g.setFont(font);
    g.setColour(white(0.80f));
    g.drawText(text, area.translated(1.1f, 1.1f), just, false);
    g.setColour(black(0.55f));
    g.drawText(text, area.translated(-0.9f, -0.9f), just, false);
    g.setGradientFill(vgrad(Colour(0xff2a2823), Colour(0xff0e0d0b), area));
    g.drawText(text, area, just, false);
}
} // namespace cs80::art
