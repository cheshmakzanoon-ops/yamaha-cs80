#include "AdvancedPanel.h"

#include "PluginProcessor.h"
#include "core/Dsp.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <functional>
#include <memory>
#include <vector>

namespace cs80::ui
{

namespace
{
using Rect = juce::Rectangle<float>;
using Pt = juce::Point<float>;
using Just = juce::Justification;

// Design space of the whole panel. The panel is placed at (0, 64) of the editor.
constexpr float kW = 1600.0f;
constexpr float kH = 936.0f;

// Same order as the engine: 1 pitch, 2 filter, 3 level, 4 pan, 5 pulse width.
const juce::StringArray kDestinations { "OFF", "PITCH", "VCF", "VCA", "PAN", "PWM" };
const juce::StringArray kFxTypes { "OFF", "CHORUS", "PHASER", "TAPE DELAY", "PLATE REVERB", "DRIVE" };
const juce::StringArray kSubWaves { "SINE", "TRIANGLE", "SAW", "SQUARE", "S&H", "RANDOM" };
const juce::StringArray kOriginalSubWaves { "SINE", "SAW DOWN", "SAW UP", "SQUARE", "WHITE NOISE", "EXT" };
const juce::StringArray kArpModes { "UP", "DOWN", "UP / DOWN", "RANDOM", "PLAYED" };
const juce::StringArray kGlideModes { "OFF", "PORTAMENTO", "GLISSANDO" };

const juce::Colour kInk { 0xff0a0907 };          // deep glass black
const juce::Colour kGlow { 0xfff3d98a };         // lit amber glass, top
const juce::Colour kGlowDeep { 0xffc98f2a };     // lit amber glass, bottom

// Colours are read at call time: the shared palette lives in another translation unit.
juce::Colour axisInk() { return ivoryDim.withAlpha(0.92f); }
juce::Colour lfoColour(int i)
{
    return i == 0 ? capGreen.brighter(0.3f) : (i == 1 ? capPink.brighter(0.2f) : capYellow);
}
juce::Colour pageColour(int i)
{
    return i == 0 ? capGreen : (i == 1 ? capPink : (i == 2 ? capYellow : capCream));
}
juce::Colour inkFor(juce::Colour plate)
{
    return plate.getPerceivedBrightness() > 0.55f ? juce::Colour(0xff17120a) : juce::Colours::white;
}

// ---------------------------------------------------------------------------
// Small drawing vocabulary
// ---------------------------------------------------------------------------

juce::Font fnt(float height, bool bold = false, float kern = 0.0f)
{
    auto f = juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), height,
                                          bold ? juce::Font::bold : juce::Font::plain));
    if (kern != 0.0f)
        f.setExtraKerningFactor(kern);
    return f;
}

void text(juce::Graphics& g, const juce::String& s, Rect r, float h, bool bold, juce::Colour c,
          Just j = Just::centredLeft, float kern = 0.0f)
{
    g.setFont(fnt(h, bold, kern));
    g.setColour(c);
    g.drawText(s, r, j, false);
}

// Axis numeral: 10 px, contrast above 3:1 on the glass.
void axisText(juce::Graphics& g, const juce::String& s, Rect r, Just j = Just::centred)
{
    text(g, s, r, 10.0f, false, axisInk(), j);
}

// Silkscreen legend pressed into the panel: dark undercut, bright ink.
void engrave(juce::Graphics& g, const juce::String& s, Rect r, float h, bool bold, juce::Colour c,
             Just j = Just::centredLeft, float kern = 0.0f)
{
    g.setFont(fnt(h, bold, kern));
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.drawText(s, r.translated(0.5f, 1.2f), j, false);
    g.setColour(c);
    g.drawText(s, r, j, false);
}

float textWidth(const juce::String& s, float h, bool bold, float kern)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText(fnt(h, bold, kern), s, 0.0f, 0.0f);
    return ga.getBoundingBox(0, -1, true).getWidth();
}

// ---- Crinkle finish: the same embossed micro-relief as the main control surface -------------

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

const juce::Image& crinkleTile() { static const juce::Image tile = makeRelief(192, 5, 1.3f); return tile; }
const juce::Image& grainTile() { static const juce::Image tile = makeRelief(128, 9, 0.85f); return tile; }

// Device pixels per design unit of the layer being painted; set by the cached layers.
float gArtScale = 1.0f;

void fillRelief(juce::Graphics& g, const juce::Image& tile, Rect area, float opacity,
                float shiftX = 0.0f, float shiftY = 0.0f)
{
    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(area.toNearestIntEdges());
    juce::FillType fill(tile, juce::AffineTransform::scale(1.0f / juce::jmax(0.25f, gArtScale)).translated(shiftX, shiftY));
    fill.setOpacity(opacity);
    g.setFillType(fill);
    g.fillRect(area);
}

// A small cap-coloured chiclet: the instrument's own colour coding, used as a page and card marker.
void capChip(juce::Graphics& g, Rect r, juce::Colour c)
{
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.fillRoundedRectangle(r.translated(0.0f, 1.2f), 2.5f);
    g.setGradientFill(juce::ColourGradient(c.brighter(0.25f), r.getX(), r.getY(), c.darker(0.35f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 2.5f);
    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.drawLine(r.getX() + 2.5f, r.getY() + 1.0f, r.getRight() - 2.5f, r.getY() + 1.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawRoundedRectangle(r, 2.5f, 0.9f);
}

// Gold etched bracket carrying a cream title, as on the main control surface. A tinted bracket
// carries a cap-coloured chiclet before the title. `endInset` leaves room for the info button.
void bracketTitle(juce::Graphics& g, float x0, float x1, float y, const juce::String& title,
                  float size = 13.0f, juce::Colour accent = juce::Colours::transparentBlack)
{
    const bool tinted = ! accent.isTransparent();
    const float tw = textWidth(title, size, true, 0.14f) + 26.0f;
    const float total = tw + (tinted ? 24.0f : 0.0f);
    const float mid = (x0 + x1) * 0.5f;
    const float left = mid - total * 0.5f, right = mid + total * 0.5f;
    auto line = [&](float ax, float ay, float bx, float by)
    {
        if (tinted)
        {
            g.setColour(accent.withAlpha(0.78f));
            g.drawLine(ax, ay, bx, by, 1.3f);
        }
        else
            etchLine(g, ax, ay, bx, by, 0.8f);
    };
    line(x0, y, left, y);
    line(right, y, x1, y);
    line(x0, y, x0, y + 7.0f);
    line(x1, y, x1, y + 7.0f);
    if (tinted)
        capChip(g, { left + 4.0f, y - 5.0f, 17.0f, 10.0f }, accent);
    engrave(g, title, { left + (tinted ? 24.0f : 0.0f), y - 10.0f, tw, 20.0f }, size, true, ivory, Just::centred, 0.14f);
}

// A raised charcoal sub-panel with a cast shadow, lit upper-left edge, dark lower-right edge,
// crinkle-finished and screwed down at the corners.
void drawModule(juce::Graphics& g, Rect r, const juce::String& title, bool primary,
                juce::Colour accent = juce::Colours::transparentBlack, bool hasInfo = false)
{
    constexpr float rad = 6.0f;
    softShadow(g, r, rad, 0.0f, 4.0f, 8.0f, 0.62f);
    juce::Path p;
    p.addRoundedRectangle(r, rad);
    g.saveState();
    g.reduceClipRegion(p);
    fillPanelFace(g, r);
    fillRelief(g, crinkleTile(), r, 0.24f, 11.0f, 7.0f);
    fillRelief(g, grainTile(), r, 0.09f, 61.0f, 23.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.07f), r.getX(), r.getY(),
                                           juce::Colours::transparentWhite, r.getX(), r.getY() + 46.0f, false));
    g.fillRect(r.withHeight(46.0f));
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, r.getX(), r.getBottom() - 40.0f,
                                           juce::Colours::black.withAlpha(0.22f), r.getX(), r.getBottom(), false));
    g.fillRect(r.withTrimmedTop(r.getHeight() - 40.0f));
    g.restoreState();

    g.setColour(juce::Colours::white.withAlpha(0.17f));
    g.drawLine(r.getX() + rad, r.getY() + 0.8f, r.getRight() - rad, r.getY() + 0.8f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawLine(r.getX() + 0.8f, r.getY() + rad, r.getX() + 0.8f, r.getBottom() - rad, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.drawLine(r.getX() + rad, r.getBottom() - 0.8f, r.getRight() - rad, r.getBottom() - 0.8f, 1.2f);
    g.drawLine(r.getRight() - 0.8f, r.getY() + rad, r.getRight() - 0.8f, r.getBottom() - rad, 1.2f);
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.drawRoundedRectangle(r, rad, 1.0f);
    etchRect(g, r.reduced(5.0f), 3.0f, primary ? 0.34f : 0.22f);

    drawScrew(g, { r.getX() + 14.0f, r.getY() + 14.0f }, 3.2f, 0.5f);
    drawScrew(g, { r.getRight() - 14.0f, r.getY() + 14.0f }, 3.2f, 1.1f);
    drawScrew(g, { r.getX() + 14.0f, r.getBottom() - 14.0f }, 3.2f, 2.0f);
    drawScrew(g, { r.getRight() - 14.0f, r.getBottom() - 14.0f }, 3.2f, 0.2f);
    bracketTitle(g, r.getX() + 32.0f, r.getRight() - (hasInfo ? 70.0f : 32.0f), r.getY() + 22.0f,
                 title.toUpperCase(), primary ? 13.5f : 12.5f, accent);
}

// Recessed dark glass: where a live display sits.
void glassWell(juce::Graphics& g, Rect r, float rad = 4.0f)
{
    drawWell(g, r, rad);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0x16ffe3a8), r.getX(), r.getY(),
                                           juce::Colours::transparentBlack, r.getX() + r.getWidth() * 0.55f,
                                           r.getY() + r.getHeight() * 0.8f, false));
    g.fillRoundedRectangle(r.reduced(1.5f), rad);
    juce::Path glare;
    glare.startNewSubPath(r.getX() + 3.0f, r.getY() + 3.0f);
    glare.lineTo(r.getX() + r.getWidth() * 0.34f, r.getY() + 3.0f);
    glare.lineTo(r.getX() + 3.0f, r.getY() + juce::jmin(r.getHeight() * 0.7f, 60.0f));
    glare.closeSubPath();
    g.setColour(juce::Colours::white.withAlpha(0.028f));
    g.fillPath(glare);
}

// Jewel lamp, same language as LedButton: glass bead with a halo when lit.
void jewel(juce::Graphics& g, Pt c, float r, juce::Colour colour, bool on)
{
    if (on)
    {
        g.setColour(colour.withAlpha(0.14f));
        g.fillEllipse(c.x - r * 2.4f, c.y - r * 2.4f, r * 4.8f, r * 4.8f);
        g.setColour(colour.withAlpha(0.26f));
        g.fillEllipse(c.x - r * 1.5f, c.y - r * 1.5f, r * 3.0f, r * 3.0f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.fillEllipse(c.x - r - 0.9f, c.y - r - 0.5f, r * 2.0f + 1.8f, r * 2.0f + 1.8f);
    g.setGradientFill(juce::ColourGradient(on ? colour.brighter(0.7f) : colour.darker(0.8f).withMultipliedSaturation(0.7f),
                                           c.x - r * 0.35f, c.y - r * 0.45f,
                                           on ? colour.darker(0.25f) : juce::Colour(0xff120a07),
                                           c.x + r * 0.5f, c.y + r * 0.6f, false));
    g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour(juce::Colours::white.withAlpha(on ? 0.65f : 0.14f));
    g.fillEllipse(c.x - r * 0.42f, c.y - r * 0.55f, r * 0.55f, r * 0.42f);
}

void hline(juce::Graphics& g, float x0, float x1, float y, float alpha, juce::Colour c = ivory)
{
    g.setColour(c.withAlpha(alpha));
    g.drawLine(x0, y, x1, y, 1.0f);
}

void vline(juce::Graphics& g, float x, float y0, float y1, float alpha, juce::Colour c = ivory)
{
    g.setColour(c.withAlpha(alpha));
    g.drawLine(x, y0, x, y1, 1.0f);
}

// Shown over a display whose controls the isolation fixture masks.
void inertOverlay(juce::Graphics& g, Rect r, const char* label = "ISOLATION  -  CONTROL INERT")
{
    g.saveState();
    g.reduceClipRegion(r.toNearestInt());
    g.setColour(kInk.withAlpha(0.5f));
    g.fillRect(r);
    g.setColour(amber.withAlpha(0.10f));
    for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 10.0f)
        g.drawLine(x, r.getBottom(), x + r.getHeight(), r.getY(), 1.3f);
    g.restoreState();
    const Rect badge = Rect(0.0f, 0.0f, 196.0f, 20.0f).withCentre(r.getCentre());
    g.setColour(kInk.withAlpha(0.9f));
    g.fillRoundedRectangle(badge, 3.0f);
    g.setColour(amber.withAlpha(0.7f));
    g.drawRoundedRectangle(badge, 3.0f, 1.0f);
    text(g, label, badge, 10.0f, true, amber, Just::centred, 0.08f);
}

juce::String periodString(float seconds)
{
    if (seconds < 1.0f)
        return juce::String(juce::roundToInt(seconds * 1000.0f)) + " ms";
    return juce::String(seconds, seconds < 10.0f ? 2 : 1) + " s";
}

juce::String signedString(float v, int decimals, const juce::String& unit)
{
    return (v >= 0.0f ? "+" : "-") + juce::String(std::abs(v), decimals) + unit;
}

juce::String plusMinus() { return juce::String(juce::CharPointer_UTF8("\xc2\xb1")); }

juce::String noteName(int midi)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String(names[((midi % 12) + 12) % 12]) + juce::String(midi / 12 - 1);
}

// One parameter edit with a balanced host gesture, for the widgets that edit directly.
void setParameterOnce(Cs80AudioProcessor& proc, int index, float physical)
{
    if (auto* p = proc.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(index)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(physical));
        p->endChangeGesture();
    }
}

// ---------------------------------------------------------------------------
// Live widgets. Each one reads a handful of parameters, remembers the last
// values it drew and repaints only when one of them changed. The glass well and
// graticule are static art in the page cache; draw() adds only the live trace.
// ---------------------------------------------------------------------------

using Sig = std::array<float, 12>;

class LiveWidget : public juce::Component
{
public:
    LiveWidget(Cs80AudioProcessor& p, float dw, float dh)
        : proc(p), designW(dw), designH(dh)
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(false);
    }

    void refresh()
    {
        Sig now {};
        sample(now);
        if (! haveLast || now != last)
        {
            last = now;
            haveLast = true;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / designW,
                                                    (float) getHeight() / designH));
        draw(g);
    }

    // Static art in widget design units: by default just the glass well.
    virtual void paintStatic(juce::Graphics& g) { glassWell(g, { 0.0f, 0.0f, designW, designH }); }
    float getDesignW() const noexcept { return designW; }
    float getDesignH() const noexcept { return designH; }

protected:
    virtual void sample(Sig& s) const = 0;
    virtual void draw(juce::Graphics& g) = 0;

    // Interactive widgets opt in to the mouse; positions are then converted to design units.
    void enableMouse(bool hand)
    {
        setInterceptsMouseClicks(true, false);
        setAccessible(false);
        if (hand)
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    Pt toDesign(juce::Point<int> p) const
    {
        return { (float) p.x * designW / (float) juce::jmax(1, getWidth()),
                 (float) p.y * designH / (float) juce::jmax(1, getHeight()) };
    }

    float v(int index) const { return proc.getParamValue(index); }
    bool isolated() const { return v(cs80::P_parityMode) >= 0.5f; }
    // 0 legacy, 1 source performance, 2 isolation (isolation wins).
    int policy() const
    {
        return v(cs80::P_parityMode) >= 0.5f ? 2 : (v(cs80::P_sourcePerformance) >= 0.5f ? 1 : 0);
    }
    juce::RangedAudioParameter* par(int index) const
    {
        return proc.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(index));
    }

    Cs80AudioProcessor& proc;
    float designW, designH;

private:
    Sig last {};
    bool haveLast = false;
};

// Scale factors the engine applies at full LFO depth.
const float kLfoScale[] = { 0.0f, 2.0f, 1.6f, 0.25f, 0.7f, 0.18f };
const char* const kLfoUnit[] = { "", " st", " oct", " lvl", " pan", " pw" };

// ---- LFO scope: the LFO at the set rate; the modulated output is drawn only when routed --------
class LfoScope final : public LiveWidget
{
public:
    LfoScope(Cs80AudioProcessor& p, int index)
        : LiveWidget(p, 440.0f, 140.0f), n(index), colour(lfoColour(index))
    {
        rateIdx = cs80::P_lfo1Rate + n * 3;
        depthIdx = rateIdx + 1;
        destIdx = rateIdx + 2;
    }

    void paintStatic(juce::Graphics& g) override
    {
        glassWell(g, { 0.0f, 0.0f, designW, designH });
        const auto plot = plotRect();
        for (int i = 0; i <= 4; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / 4.0f;
            vline(g, x, plot.getY(), plot.getBottom(), i == 0 || i == 4 ? 0.30f : 0.15f);
            axisText(g, i == 0 ? "0" : (i == 4 ? "4 s" : juce::String(i)), { x - 14.0f, plot.getBottom() + 2.0f, 28.0f, 13.0f });
        }
        hline(g, plot.getX(), plot.getRight(), plot.getCentreY(), 0.32f);
        hline(g, plot.getX(), plot.getRight(), plot.getY() + 4.0f, 0.12f);
        hline(g, plot.getX(), plot.getRight(), plot.getBottom() - 4.0f, 0.12f);
        axisText(g, "+1", { 4.0f, plot.getY() - 2.0f, 34.0f, 12.0f }, Just::centredRight);
        axisText(g, "0", { 4.0f, plot.getCentreY() - 6.0f, 34.0f, 12.0f }, Just::centredRight);
        axisText(g, "-1", { 4.0f, plot.getBottom() - 10.0f, 34.0f, 12.0f }, Just::centredRight);
    }

private:
    static Rect plotRect() { return { 44.0f, 22.0f, 382.0f, 98.0f }; }

    void sample(Sig& s) const override
    {
        s[0] = v(rateIdx); s[1] = v(depthIdx); s[2] = v(destIdx); s[3] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const float rate = v(rateIdx), depth = v(depthIdx);
        const int dest = juce::jlimit(0, 5, juce::roundToInt(v(destIdx)));
        const bool iso = isolated();
        const bool routed = dest > 0 && depth > 0.001f && ! iso;
        const auto plot = plotRect();
        const float cy = plot.getCentreY(), amp = plot.getHeight() * 0.5f - 6.0f;
        const float cycles = rate * 4.0f;
        const int count = juce::jlimit(160, 2000, (int) std::ceil(cycles * 14.0f) + 160);
        auto build = [&](float scale)
        {
            juce::Path path;
            for (int i = 0; i <= count; ++i)
            {
                const float t = (float) i / (float) count;
                const float x = plot.getX() + plot.getWidth() * t;
                const float y = cy - std::sin(juce::MathConstants<float>::twoPi * cycles * t) * amp * scale;
                if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
            }
            return path;
        };
        // Full-scale LFO: faint when its modulated output is drawn on top, the only trace otherwise.
        g.setColour(routed ? colour.withAlpha(0.30f) : axisInk().withAlpha(0.8f));
        g.strokePath(build(1.0f), juce::PathStrokeType(routed ? 1.0f : 1.6f));
        if (routed)
        {
            const auto trace = build(depth);
            auto area = trace;
            area.lineTo(plot.getRight(), cy);
            area.lineTo(plot.getX(), cy);
            area.closeSubPath();
            g.setColour(colour.withAlpha(0.16f));
            g.fillPath(area);
            g.setColour(colour.withAlpha(0.35f));
            g.strokePath(trace, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(colour.brighter(0.4f));
            g.strokePath(trace, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        text(g, juce::String(rate, rate < 10.0f ? 2 : 1) + " Hz    T " + periodString(1.0f / juce::jmax(0.05f, rate)),
             { 12.0f, 3.0f, 200.0f, 16.0f }, 11.0f, true, colour.brighter(0.3f), Just::centredLeft, 0.03f);
        juce::String right;
        if (iso) right = "MASKED BY ISOLATION";
        else if (dest == 0) right = "NOT ROUTED";
        else if (depth <= 0.001f) right = kDestinations[dest] + "   DEPTH 0";
        else right = kDestinations[dest] + "   " + plusMinus() + juce::String(depth * kLfoScale[dest], 2) + kLfoUnit[dest];
        text(g, right, { 190.0f, 3.0f, 238.0f, 16.0f }, 11.0f, true, routed ? ivory : axisInk(), Just::centredRight, 0.03f);
    }

    int n, rateIdx, depthIdx, destIdx;
    juce::Colour colour;
};

// ---- Modulation matrix: LFO x destination. A click routes; the lit cell shows the engine swing --
class LfoMatrix final : public LiveWidget
{
public:
    explicit LfoMatrix(Cs80AudioProcessor& p) : LiveWidget(p, 1440.0f, 208.0f) { enableMouse(true); }

    void paintStatic(juce::Graphics&) override {}

private:
    static constexpr float labelW = 132.0f, cellW = 250.0f, cellGap = 8.0f, rowH = 56.0f, rowGap = 6.0f, top = 30.0f;

    static Rect cellRect(int row, int col)
    {
        return { labelW + 8.0f + (float) col * (cellW + cellGap), top + (float) row * (rowH + rowGap), cellW, rowH };
    }

    void sample(Sig& s) const override
    {
        for (int i = 0; i < 3; ++i)
        {
            s[(size_t) i] = v(cs80::P_lfo1Dest + i * 3);
            s[(size_t) (3 + i)] = v(cs80::P_lfo1Depth + i * 3);
            s[(size_t) (6 + i)] = v(cs80::P_lfo1Rate + i * 3);
        }
        s[9] = v(cs80::P_parityMode); s[10] = (float) hover;
    }

    void mouseMove(const juce::MouseEvent& e) override { setHover(hit(toDesign(e.getPosition()))); }
    void mouseExit(const juce::MouseEvent&) override { setHover(-1); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int h = hit(toDesign(e.getPosition()));
        if (h < 0)
            return;
        const int row = h / 5, col = h % 5;
        const int current = juce::jlimit(0, 5, juce::roundToInt(v(cs80::P_lfo1Dest + row * 3)));
        setParameterOnce(proc, cs80::P_lfo1Dest + row * 3, current == col + 1 ? 0.0f : (float) (col + 1));
        refresh();
    }

    int hit(Pt p) const
    {
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 5; ++col)
                if (cellRect(row, col).contains(p))
                    return row * 5 + col;
        return -1;
    }
    void setHover(int h)
    {
        if (h != hover)
        {
            hover = h;
            repaint();
        }
    }

    void draw(juce::Graphics& g) override
    {
        const bool iso = isolated();
        static const char* const names[5] = { "PITCH", "VCF", "VCA", "PAN", "PWM" };
        static const char* const full[5] = { "2 st", "1.6 oct", "0.25 level", "0.7 pan", "0.18 pulse width" };
        for (int col = 0; col < 5; ++col)
        {
            const Rect c = cellRect(0, col);
            text(g, names[col], { c.getX(), 2.0f, c.getWidth(), 14.0f }, 11.5f, true, ivory, Just::centred, 0.1f);
            text(g, "FULL SCALE " + plusMinus() + juce::String(full[col]), { c.getX(), 15.0f, c.getWidth(), 13.0f }, 10.0f, false,
                 axisInk(), Just::centred);
        }
        text(g, iso ? "MASKED BY ISOLATION" : "CLICK A CELL TO ROUTE", { 0.0f, 8.0f, labelW + 4.0f, 14.0f }, 10.0f, true,
             iso ? amber : gold, Just::centredLeft, 0.06f);

        for (int row = 0; row < 3; ++row)
        {
            const auto colour = lfoColour(row);
            const float depth = v(cs80::P_lfo1Depth + row * 3);
            const int dest = juce::jlimit(0, 5, juce::roundToInt(v(cs80::P_lfo1Dest + row * 3)));
            const float rate = v(cs80::P_lfo1Rate + row * 3);
            const Rect chip(0.0f, top + (float) row * (rowH + rowGap), labelW, rowH);
            g.setColour(juce::Colours::black.withAlpha(0.65f));
            g.fillRoundedRectangle(chip.translated(0.0f, 1.5f), 5.0f);
            g.setGradientFill(juce::ColourGradient(colour.brighter(0.15f), chip.getX(), chip.getY(),
                                                   colour.darker(0.45f), chip.getX(), chip.getBottom(), false));
            g.fillRoundedRectangle(chip, 5.0f);
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.drawLine(chip.getX() + 5.0f, chip.getY() + 1.2f, chip.getRight() - 5.0f, chip.getY() + 1.2f, 1.0f);
            const auto ink = inkFor(colour);
            text(g, "LFO " + juce::String(row + 1), chip.withTrimmedLeft(14.0f).withHeight(30.0f).translated(0.0f, 4.0f), 15.0f, true, ink,
                 Just::centredLeft, 0.06f);
            text(g, juce::String(rate, rate < 10.0f ? 2 : 1) + " Hz", chip.withTrimmedLeft(14.0f).withTrimmedTop(32.0f), 11.0f, true,
                 ink.withAlpha(0.85f), Just::centredLeft);

            for (int col = 0; col < 5; ++col)
            {
                const Rect c = cellRect(row, col);
                const bool lit = dest == col + 1;
                const bool hot = hover == row * 5 + col;
                g.setColour(juce::Colours::black.withAlpha(0.6f));
                g.fillRoundedRectangle(c.translated(0.0f, 1.3f), 5.0f);
                if (lit)
                {
                    g.setColour(colour.withAlpha(iso ? 0.10f : 0.20f));
                    g.fillRoundedRectangle(c.expanded(3.0f), 8.0f);
                    g.setGradientFill(juce::ColourGradient(colour.withAlpha(iso ? 0.5f : 0.95f), c.getX(), c.getY(),
                                                           colour.darker(0.5f).withAlpha(iso ? 0.5f : 0.95f), c.getX(), c.getBottom(), false));
                }
                else
                    g.setGradientFill(juce::ColourGradient(juce::Colour(hot ? 0xff2f2c27 : 0xff24221e), c.getX(), c.getY(),
                                                           juce::Colour(0xff131210), c.getX(), c.getBottom(), false));
                g.fillRoundedRectangle(c, 5.0f);
                g.setColour(lit ? juce::Colours::white.withAlpha(0.35f) : juce::Colours::white.withAlpha(0.07f));
                g.drawLine(c.getX() + 5.0f, c.getY() + 1.1f, c.getRight() - 5.0f, c.getY() + 1.1f, 1.0f);
                g.setColour(hot ? colour.brighter(0.2f) : (lit ? colour.darker(0.6f) : juce::Colours::black.withAlpha(0.9f)));
                g.drawRoundedRectangle(c, 5.0f, hot ? 1.6f : 1.0f);
                if (lit)
                {
                    const auto ink = inkFor(colour);
                    text(g, plusMinus() + juce::String(depth * kLfoScale[col + 1], 2) + kLfoUnit[col + 1], c.withTrimmedLeft(14.0f).withHeight(34.0f),
                         16.0f, true, ink, Just::centredLeft, 0.02f);
                    text(g, juce::String(juce::roundToInt(depth * 100.0f)) + "%", c.withTrimmedRight(14.0f).withHeight(34.0f), 12.0f, true,
                         ink.withAlpha(0.85f), Just::centredRight);
                    g.setColour(ink.withAlpha(0.28f));
                    g.fillRoundedRectangle(c.getX() + 14.0f, c.getBottom() - 12.0f, c.getWidth() - 28.0f, 4.0f, 2.0f);
                    g.setColour(ink.withAlpha(0.85f));
                    g.fillRoundedRectangle(c.getX() + 14.0f, c.getBottom() - 12.0f, juce::jmax(3.0f, (c.getWidth() - 28.0f) * depth), 4.0f, 2.0f);
                }
                else
                    text(g, hot ? "ROUTE HERE" : plusMinus() + juce::String(depth * kLfoScale[col + 1], 2) + kLfoUnit[col + 1] + " at depth "
                                  + juce::String(juce::roundToInt(depth * 100.0f)) + "%",
                         c.withTrimmedLeft(14.0f), 11.0f, hot, hot ? ivory : axisInk().withAlpha(0.75f), Just::centredLeft);
            }
        }
        if (iso)
        {
            g.setColour(kInk.withAlpha(0.32f));
            g.fillRect(labelW + 4.0f, top, designW - labelW - 4.0f, 3.0f * rowH + 2.0f * rowGap);
        }
    }

    int hover = -1;
};

// ---- Mod wheel routing ------------------------------------------------------------------------
class WheelRouting final : public LiveWidget
{
public:
    explicit WheelRouting(Cs80AudioProcessor& p) : LiveWidget(p, 232.0f, 84.0f) {}

private:
    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_modWheelSub); s[1] = v(cs80::P_modWheelVcf); s[2] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const float sub = v(cs80::P_modWheelSub), vcf = v(cs80::P_modWheelVcf);
        auto row = [&](float y, const juce::String& label, float amount, const juce::String& result)
        {
            text(g, label, { 12.0f, y, 130.0f, 14.0f }, 10.0f, true, ivoryDim, Just::centredLeft, 0.05f);
            text(g, result, { 100.0f, y - 1.0f, 120.0f, 15.0f }, 11.5f, true, amberBright, Just::centredRight, 0.02f);
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.fillRoundedRectangle(12.0f, y + 17.0f, 208.0f, 6.0f, 3.0f);
            g.setColour(isolated() ? ivoryDim.withAlpha(0.4f) : amber);
            g.fillRoundedRectangle(12.0f, y + 17.0f, juce::jmax(4.0f, 208.0f * amount), 6.0f, 3.0f);
        };
        row(8.0f, "WHEEL > SUB PITCH", sub, "up to " + plusMinus() + juce::String(sub * 2.0f, 2) + " st");
        row(46.0f, "WHEEL > VCF", vcf, "up to +" + juce::String(vcf * 1.5f, 2) + " oct");
        if (isolated())
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }
};

// ---- Tuning readout --------------------------------------------------------------------------
class TuningDisplay final : public LiveWidget
{
public:
    explicit TuningDisplay(Cs80AudioProcessor& p) : LiveWidget(p, 372.0f, 52.0f) {}

private:
    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_tuningRef); s[1] = v(cs80::P_masterTune); s[2] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const bool iso = isolated();
        const float ref = iso ? 440.0f : v(cs80::P_tuningRef);
        const float mt = v(cs80::P_masterTune);
        const float a4 = ref * std::pow(2.0f, mt / 12.0f);
        text(g, "SOUNDING A4", { 12.0f, 4.0f, 130.0f, 13.0f }, 10.0f, true, ivoryDim, Just::centredLeft, 0.08f);
        text(g, juce::String(a4, 2) + " Hz", { 12.0f, 17.0f, 190.0f, 30.0f }, 23.0f, true, amberBright);
        text(g, iso ? "REF 440.0 Hz (ISOLATION)" : "REF " + juce::String(ref, 1) + " Hz", { 200.0f, 8.0f, 164.0f, 14.0f }, 11.0f, true,
             ivory, Just::centredRight, 0.03f);
        text(g, "MASTER " + signedString(mt, 2, " st") + " = " + signedString(mt * 100.0f, 0, " ct"), { 180.0f, 24.0f, 184.0f, 14.0f }, 11.0f,
             true, ivory, Just::centredRight, 0.02f);
        text(g, "C4  " + juce::String(a4 * std::pow(2.0f, -9.0f / 12.0f), 2) + " Hz", { 200.0f, 37.0f, 164.0f, 13.0f }, 10.0f, false,
             axisInk(), Just::centredRight);
    }
};

// ---- Bend ranges ------------------------------------------------------------------------------
class BendRanges final : public LiveWidget
{
public:
    explicit BendRanges(Cs80AudioProcessor& p) : LiveWidget(p, 372.0f, 56.0f) {}

private:
    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_pitchBendRange); s[1] = v(cs80::P_mpeBendRange); s[2] = v(cs80::P_mpeOn);
        s[3] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const bool iso = isolated();
        auto bar = [&](float y, const juce::String& label, float range, bool active)
        {
            text(g, label, { 12.0f, y, 230.0f, 13.0f }, 10.0f, true, ivoryDim, Just::centredLeft, 0.06f);
            text(g, plusMinus() + juce::String(juce::roundToInt(range)) + " st", { 262.0f, y - 1.0f, 98.0f, 14.0f }, 11.5f, true,
                 active ? amberBright : ivoryDim, Just::centredRight);
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.fillRoundedRectangle(12.0f, y + 14.0f, 348.0f, 5.0f, 2.5f);
            g.setColour(active ? amber : ivoryDim.withAlpha(0.45f));
            g.fillRoundedRectangle(12.0f, y + 14.0f, juce::jmax(4.0f, 348.0f * range / 96.0f), 5.0f, 2.5f);
        };
        bar(4.0f, "PITCH LEVER / MIDI BEND", v(cs80::P_pitchBendRange), ! iso);
        bar(30.0f, v(cs80::P_mpeOn) >= 0.5f ? "MPE MEMBER CHANNELS" : "MPE MEMBER CHANNELS (MPE OFF)",
            v(cs80::P_mpeBendRange), ! iso && v(cs80::P_mpeOn) >= 0.5f);
        if (iso)
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }
};

// ---- Sub oscillator scope: the waveform the engine would actually run ---------------------
class SubScope final : public LiveWidget
{
public:
    explicit SubScope(Cs80AudioProcessor& p) : LiveWidget(p, 712.0f, 120.0f)
    {
        cs80::Rng rng(0x1234abcdu);
        for (auto& x : noise)
            x = rng.nextBipolar();
    }

    void paintStatic(juce::Graphics& g) override
    {
        glassWell(g, { 0.0f, 0.0f, designW, designH });
        const auto plot = plotRect();
        for (int i = 0; i <= 4; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / 4.0f;
            vline(g, x, plot.getY(), plot.getBottom(), i == 0 || i == 4 ? 0.30f : 0.15f);
            axisText(g, i == 0 ? "0" : (i == 4 ? "1 s" : (i == 2 ? "0.5" : (i == 1 ? "0.25" : "0.75"))),
                     { x - 18.0f, plot.getBottom() + 2.0f, 36.0f, 13.0f });
        }
        hline(g, plot.getX(), plot.getRight(), plot.getCentreY(), 0.32f);
        axisText(g, "+1", { 4.0f, plot.getY() - 2.0f, 34.0f, 12.0f }, Just::centredRight);
        axisText(g, "0", { 4.0f, plot.getCentreY() - 6.0f, 34.0f, 12.0f }, Just::centredRight);
        axisText(g, "-1", { 4.0f, plot.getBottom() - 10.0f, 34.0f, 12.0f }, Just::centredRight);
    }

private:
    static Rect plotRect() { return { 44.0f, 22.0f, 656.0f, 76.0f }; }

    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_subOriginalMode); s[1] = v(cs80::P_parityMode); s[2] = v(cs80::P_subOriginalFunction);
        s[3] = v(cs80::P_subWave); s[4] = v(cs80::P_subOriginalSpeed); s[5] = v(cs80::P_subSpeed);
        s[6] = v(cs80::P_subExtLevel); s[7] = proc.isExternalInputEnabled() ? 1.0f : 0.0f;
    }

    void draw(juce::Graphics& g) override
    {
        const bool iso = isolated();
        const bool original = iso || v(cs80::P_subOriginalMode) >= 0.5f;
        const int fn = juce::jlimit(0, 5, juce::roundToInt(original ? v(cs80::P_subOriginalFunction) : v(cs80::P_subWave)));
        const float rate = original ? v(cs80::P_subOriginalSpeed) : v(cs80::P_subSpeed);
        const auto plot = plotRect();
        const float cy = plot.getCentreY(), amp = plot.getHeight() * 0.5f - 3.0f;
        const juce::String name = original ? kOriginalSubWaves[fn] : kSubWaves[fn];
        const bool ext = original && fn == 5;
        const bool noiseFn = original ? fn == 4 : fn == 5;

        juce::Path path;
        if (ext)
        {
            g.setColour(ivoryDim.withAlpha(0.7f));
            const float dash[] = { 5.0f, 4.0f };
            g.drawDashedLine(juce::Line<float>(plot.getX(), cy, plot.getRight(), cy), dash, 2, 1.4f);
        }
        else if (noiseFn)
        {
            for (int i = 0; i < (int) plot.getWidth(); i += 2)
            {
                const float a = noise[(size_t) (i / 2) % noise.size()];
                const float x = plot.getX() + (float) i;
                if (i == 0) path.startNewSubPath(x, cy - a * amp); else path.lineTo(x, cy - a * amp);
            }
        }
        else
        {
            const float cycles = rate * 1.0f;
            const int count = juce::jlimit(240, 2400, (int) std::ceil(cycles * 14.0f) + 240);
            for (int i = 0; i <= count; ++i)
            {
                const float t = (float) i / (float) count;
                const float ph = cycles * t - std::floor(cycles * t);
                float y = 0.0f;
                if (original)
                {
                    switch (fn)
                    {
                        case 0: y = std::sin(juce::MathConstants<float>::twoPi * ph); break;
                        case 1: y = (1.0f - 2.0f * ph) * (3.5f / 3.2f); break;
                        case 2: y = (2.0f * ph - 1.0f) * (3.5f / 3.2f); break;
                        default: y = ph < 0.5f ? 1.0f : -1.0f; break;
                    }
                    y *= 0.9f;
                }
                else
                {
                    switch (fn)
                    {
                        case 0: y = std::sin(juce::MathConstants<float>::twoPi * ph); break;
                        case 1: y = 1.0f - 4.0f * std::abs(ph - 0.5f); break;
                        case 2: y = 2.0f * ph - 1.0f; break;
                        case 3: y = ph < 0.5f ? 1.0f : -1.0f; break;
                        default:
                            y = noise[(size_t) (int) std::floor(cycles * t) % noise.size()];
                            break;
                    }
                }
                const float x = plot.getX() + plot.getWidth() * t;
                const float yy = cy - y * amp;
                if (i == 0) path.startNewSubPath(x, yy); else path.lineTo(x, yy);
            }
        }
        if (! ext)
        {
            g.setColour(amberBright.withAlpha(0.28f));
            g.strokePath(path, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(amberBright);
            g.strokePath(path, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const juce::String mode = iso ? "ORIGINAL  (FORCED BY ISOLATION)" : (original ? "ORIGINAL" : "LEGACY");
        text(g, mode + "  -  " + name, { 12.0f, 3.0f, 400.0f, 16.0f }, 11.0f, true, amber, Just::centredLeft, 0.08f);
        juce::String right;
        if (ext)
            right = "EXT LEVEL " + juce::String(juce::roundToInt(v(cs80::P_subExtLevel) * 100.0f)) + "%  -  BUS "
                    + (proc.isExternalInputEnabled() ? "ENABLED" : "DISABLED (HOST ROUTING REQUIRED)");
        else if (noiseFn)
            right = "SPEED IGNORED";
        else
            right = juce::String(rate, rate < 10.0f ? 2 : 1) + " Hz";
        text(g, right, { 330.0f, 3.0f, 370.0f, 16.0f }, 11.0f, true, ext ? ivory : amber, Just::centredRight, 0.05f);
    }

    std::array<float, 400> noise {};
};

// ---- Ring modulator: carrier frequency over a key-held envelope, with draggable A / D nodes ----
class RingCurve final : public LiveWidget
{
public:
    explicit RingCurve(Cs80AudioProcessor& p) : LiveWidget(p, 332.0f, 400.0f) { enableMouse(false); }

    void paintStatic(juce::Graphics& g) override { glassWell(g, { 0.0f, 0.0f, designW, designH }); }

private:
    static Rect plotRect() { return { 54.0f, 42.0f, 262.0f, 286.0f }; }

    struct Geo
    {
        float a, d, speed, depth, amount, ta, td, total, fmax;
    };

    Geo geo() const
    {
        Geo r {};
        r.a = juce::jmax(0.001f, v(cs80::P_ringAttack));
        r.d = juce::jmax(0.001f, v(cs80::P_ringDecay));
        r.speed = juce::jmax(0.05f, v(cs80::P_ringSpeed));
        r.depth = v(cs80::P_ringDepth);
        r.amount = v(cs80::P_ringAmount);
        // Engine Adsr: attack 1 - exp(-6 t / A) until ~0.999, then exp(-6 t / D) down to the zero sustain.
        r.ta = 1.151f * r.a;
        r.td = 1.267f * r.d;
        r.total = r.ta + r.td;
        r.fmax = r.speed * (1.0f + r.depth) * 1.1f;
        if (dragging != 0)
        {
            r.total = dragTotal;
            r.fmax = dragFmax;
        }
        return r;
    }

    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_ringAttack); s[1] = v(cs80::P_ringDecay); s[2] = v(cs80::P_ringSpeed);
        s[3] = v(cs80::P_ringDepth); s[4] = v(cs80::P_ringAmount); s[5] = (float) hover; s[6] = (float) dragging;
    }

    Pt nodePos(const Geo& gm, int which) const
    {
        const auto plot = plotRect();
        if (which == 1)
            return { plot.getX() + plot.getWidth() * gm.ta / gm.total,
                     plot.getBottom() - plot.getHeight() * gm.speed * (1.0f + gm.depth) / gm.fmax };
        return { plot.getX() + plot.getWidth() * (gm.ta + gm.td) / gm.total,
                 plot.getBottom() - plot.getHeight() * gm.speed / gm.fmax };
    }

    int hitNode(Pt p) const
    {
        const auto gm = geo();
        for (int which = 1; which <= 2; ++which)
            if (nodePos(gm, which).getDistanceFrom(p) < 13.0f)
                return which;
        return 0;
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const int h = hitNode(toDesign(e.getPosition()));
        if (h != hover)
        {
            hover = h;
            setMouseCursor(h != 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }
    void mouseExit(const juce::MouseEvent&) override
    {
        if (hover != 0 && dragging == 0)
        {
            hover = 0;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int h = hitNode(toDesign(e.getPosition()));
        if (h == 0)
            return;
        const auto gm = geo();
        dragging = h;
        dragTotal = gm.total;
        dragFmax = gm.fmax;
        params.clear();
        params.push_back(h == 1 ? cs80::P_ringAttack : cs80::P_ringDecay);
        if (h == 1)
            params.push_back(cs80::P_ringDepth);
        for (int index : params)
            if (auto* pr = par(index))
                pr->beginChangeGesture();
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragging == 0)
            return;
        const auto plot = plotRect();
        const auto p = toDesign(e.getPosition());
        const auto gm = geo();
        const float t = juce::jlimit(0.0f, 1.0f, (p.x - plot.getX()) / plot.getWidth()) * dragTotal;
        auto set = [&](int index, float value)
        {
            if (auto* pr = par(index))
                pr->setValueNotifyingHost(pr->convertTo0to1(value));
        };
        if (dragging == 1)
        {
            set(cs80::P_ringAttack, juce::jmax(0.001f, t / 1.151f));
            const float f = juce::jlimit(0.0f, 1.0f, (plot.getBottom() - p.y) / plot.getHeight()) * dragFmax;
            set(cs80::P_ringDepth, juce::jmax(0.0f, f / gm.speed - 1.0f));
        }
        else
            set(cs80::P_ringDecay, juce::jmax(0.001f, (t - gm.ta) / 1.267f));
        refresh();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragging == 0)
            return;
        for (int index : params)
            if (auto* pr = par(index))
                pr->endChangeGesture();
        params.clear();
        dragging = 0;
        repaint();
    }

    static juce::String timeText(float s)
    {
        return s < 1.0f ? juce::String(juce::roundToInt(s * 1000.0f)) + " ms" : juce::String(s, 2) + " s";
    }
    static juce::String hzText(float f) { return juce::String(f, f < 10.0f ? 2 : (f < 100.0f ? 1 : 0)); }

    void draw(juce::Graphics& g) override
    {
        const auto gm = geo();
        const auto plot = plotRect();
        const bool live = gm.amount > 0.001f;
        const juce::Colour curve = live ? amberBright : ivoryDim;

        // Frequency axis: 0, the unmodulated carrier and the peak, all in Hz.
        auto yOf = [&](float f) { return plot.getBottom() - plot.getHeight() * f / gm.fmax; };
        const float peak = gm.speed * (1.0f + gm.depth);
        hline(g, plot.getX(), plot.getRight(), plot.getBottom(), 0.34f);
        hline(g, plot.getX(), plot.getRight(), yOf(gm.speed), 0.26f);
        axisText(g, "0", { 4.0f, plot.getBottom() - 7.0f, 46.0f, 13.0f }, Just::centredRight);
        axisText(g, hzText(gm.speed) + " Hz", { 0.0f, yOf(gm.speed) - 6.5f, 50.0f, 13.0f }, Just::centredRight);
        if (std::abs(yOf(gm.speed) - yOf(peak)) > 13.0f)
        {
            hline(g, plot.getX(), plot.getRight(), yOf(peak), 0.16f);
            axisText(g, hzText(peak) + " Hz", { 0.0f, yOf(peak) - 6.5f, 50.0f, 13.0f }, Just::centredRight);
        }
        for (int i = 0; i <= 4; ++i)
            vline(g, plot.getX() + plot.getWidth() * (float) i / 4.0f, plot.getY(), plot.getBottom(), i == 0 ? 0.34f : 0.14f);

        auto env = [&](float t)
        {
            if (t <= gm.ta) return 1.0f - std::exp(-6.0f * t / gm.a);
            return std::exp(-6.0f * (t - gm.ta) / gm.d);
        };
        juce::Path carrier;
        constexpr int count = 320;
        const float span = gm.ta + gm.td;
        for (int i = 0; i <= count; ++i)
        {
            const float t = span * (float) i / (float) count;
            const float x = plot.getX() + plot.getWidth() * t / gm.total;
            const float y = yOf(gm.speed * (1.0f + gm.depth * env(t)));
            if (i == 0) carrier.startNewSubPath(x, y); else carrier.lineTo(x, y);
        }
        auto fill = carrier;
        fill.lineTo(plot.getX() + plot.getWidth() * span / gm.total, plot.getBottom());
        fill.lineTo(plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setColour(amber.withAlpha(live ? 0.20f : 0.08f));
        g.fillPath(fill);
        if (live)
        {
            g.setColour(amberBright.withAlpha(0.28f));
            g.strokePath(carrier, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour(curve);
        g.strokePath(carrier, juce::PathStrokeType(1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draggable nodes: A at the carrier peak, D where the decay reaches the carrier.
        for (int which = 1; which <= 2; ++which)
        {
            const Pt c = nodePos(gm, which);
            const bool hot = hover == which || dragging == which;
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.fillEllipse(c.x - 8.5f, c.y - 8.0f, 17.0f, 17.0f);
            g.setColour(hot ? juce::Colour(0xfff6df91) : (live ? amberBright : ivoryDim));
            g.fillEllipse(c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f);
            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.fillEllipse(c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
            text(g, which == 1 ? "A" : "D", { c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f }, 9.0f, true,
                 hot ? juce::Colour(0xfff6df91) : (live ? amberBright : ivoryDim), Just::centred);
        }

        // Time axis: each bracket and label sits under the segment it describes.
        const float xa = plot.getX() + plot.getWidth() * gm.ta / gm.total;
        const float xe = plot.getX() + plot.getWidth() * (gm.ta + gm.td) / gm.total;
        const float by = plot.getBottom() + 7.0f;
        auto bracket = [&](float x0, float x1, const juce::String& s, juce::Colour c)
        {
            g.setColour(c.withAlpha(0.8f));
            g.drawLine(x0 + 1.0f, by, x1 - 1.0f, by, 1.2f);
            g.drawLine(x0 + 1.0f, by - 3.0f, x0 + 1.0f, by + 3.0f, 1.2f);
            g.drawLine(x1 - 1.0f, by - 3.0f, x1 - 1.0f, by + 3.0f, 1.2f);
            const float tw = textWidth(s, 10.5f, true, 0.02f) + 6.0f;
            float cx = juce::jlimit(plot.getX() + tw * 0.5f, plot.getRight() - tw * 0.5f, (x0 + x1) * 0.5f);
            text(g, s, { cx - tw * 0.5f, by + 4.0f, tw, 14.0f }, 10.5f, true, c, Just::centred, 0.02f);
            return cx;
        };
        const float cxA = bracket(plot.getX(), xa, "A " + timeText(gm.a), amber);
        (void) cxA;
        bracket(xa, xe, "D " + timeText(gm.d), ivory);
        axisText(g, "0", { plot.getX() - 12.0f, plot.getBottom() + 22.0f, 24.0f, 12.0f });
        axisText(g, timeText(gm.total), { plot.getRight() - 50.0f, plot.getBottom() + 22.0f, 50.0f, 12.0f }, Just::centredRight);

        text(g, "CARRIER FREQUENCY, KEY HELD", { 12.0f, 5.0f, 230.0f, 14.0f }, 10.0f, true, axisInk(), Just::centredLeft, 0.05f);
        text(g, live ? "WET " + juce::String(juce::roundToInt(gm.amount * 100.0f)) + "%" : "BYPASSED", { 220.0f, 5.0f, 100.0f, 14.0f }, 10.5f,
             true, live ? amberBright : ivoryDim, Just::centredRight, 0.05f);
        text(g, hzText(gm.speed) + " > " + hzText(peak) + " Hz", { 12.0f, 20.0f, 308.0f, 16.0f }, 12.0f, true,
             live ? ivory : ivoryDim, Just::centredLeft, 0.03f);
        text(g, "DRAG A: ATTACK + DEPTH    DRAG D: DECAY", { 12.0f, 368.0f, 310.0f, 14.0f }, 10.0f, true, axisInk(), Just::centredLeft, 0.04f);
        text(g, "RELEASE 0.35 s FIXED  -  FIRST KEY OF A PHRASE TRIGGERS", { 12.0f, 384.0f, 310.0f, 14.0f }, 10.0f, false, axisInk(),
             Just::centredLeft);
    }

    int hover = 0, dragging = 0;
    float dragTotal = 1.0f, dragFmax = 1.0f;
    std::vector<int> params;
};

// ---- Chorus / tremolo sweep -----------------------------------------------------------------
class ChorusSweep final : public LiveWidget
{
public:
    explicit ChorusSweep(Cs80AudioProcessor& p) : LiveWidget(p, 416.0f, 236.0f) {}

private:
    static Rect plotRect() { return { 50.0f, 30.0f, 346.0f, 128.0f }; }

    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_chorusOn); s[1] = v(cs80::P_chorusMode); s[2] = v(cs80::P_chorusSpeed);
        s[3] = v(cs80::P_chorusDepth); s[4] = (float) policy();
    }

    void draw(juce::Graphics& g) override
    {
        const bool on = v(cs80::P_chorusOn) >= 0.5f, trem = v(cs80::P_chorusMode) >= 0.5f;
        const float speed = v(cs80::P_chorusSpeed), depth = v(cs80::P_chorusDepth);
        const int pol = policy();
        const bool bbd = pol >= 1 && ! trem;
        const auto plot = plotRect();
        constexpr float two = juce::MathConstants<float>::twoPi;

        float lo = 0.0f, hi = 20.0f;
        juce::String unit = " ms", title = "LEGACY  -  GENERIC DELAY LINE";
        if (trem) { hi = 1.0f; unit = ""; title = "TREMOLO  -  GENERIC AMPLITUDE LAW  -  GAIN"; }
        else if (bbd) { hi = 4.0f; title = "OE2 BBD DELAY  -  CLOCK RANGE INFERRED"; }

        for (int i = 0; i <= 2; ++i)
        {
            const float y = plot.getBottom() - plot.getHeight() * (float) i / 2.0f;
            hline(g, plot.getX(), plot.getRight(), y, i == 0 ? 0.34f : 0.16f);
            const float val = lo + (hi - lo) * (float) i / 2.0f;
            axisText(g, juce::String(val, hi <= 1.0f ? 1 : 0) + (i == 2 ? unit : juce::String()), { 0.0f, y - 6.5f, 46.0f, 13.0f },
                     Just::centredRight);
        }
        for (int i = 0; i <= 4; ++i)
            vline(g, plot.getX() + plot.getWidth() * (float) i / 4.0f, plot.getY(), plot.getBottom(), i % 2 == 0 ? 0.24f : 0.10f);
        axisText(g, "0", { plot.getX() - 12.0f, plot.getBottom() + 2.0f, 24.0f, 13.0f });
        axisText(g, periodString(2.0f / juce::jmax(0.05f, speed)), { plot.getRight() - 50.0f, plot.getBottom() + 2.0f, 50.0f, 13.0f },
                 Just::centredRight);

        auto trace = [&](float phaseOffset)
        {
            juce::Path path;
            constexpr int count = 240;
            for (int i = 0; i <= count; ++i)
            {
                const float t = (float) i / (float) count;
                const float mod = std::sin(two * 2.0f * t + phaseOffset);
                float value;
                if (trem)
                    value = 1.0f - depth * 0.45f * (0.5f + 0.5f * mod);
                else if (bbd)
                    value = 1000.0f * 256.0f / (106904.5f * std::exp2(depth * 0.5f * mod));
                else
                    value = 13.5f + 4.5f * depth * (0.5f + 0.5f * mod);
                const float x = plot.getX() + plot.getWidth() * t;
                const float y = plot.getBottom() - plot.getHeight() * juce::jlimit(0.0f, 1.0f, (value - lo) / (hi - lo));
                if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
            }
            return path;
        };
        const float k = on ? 1.0f : 0.35f;
        // Second channel: the generic chorus and tremolo offset the right LFO by 0.95 pi; BBD is one shared wet path.
        if (! bbd)
        {
            g.setColour(ivory.withAlpha(0.45f * k));
            g.strokePath(trace(juce::MathConstants<float>::pi * 0.95f), juce::PathStrokeType(1.3f));
        }
        const auto main = trace(0.0f);
        g.setColour(amberBright.withAlpha(0.28f * k));
        g.strokePath(main, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(on ? amberBright : ivoryDim);
        g.strokePath(main, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        text(g, title, { 12.0f, 5.0f, 396.0f, 16.0f }, 10.5f, true, amber, Just::centredLeft, 0.04f);
        text(g, "2 PERIODS   SPEED " + juce::String(speed, speed < 10.0f ? 2 : 1) + " Hz   T " + periodString(1.0f / juce::jmax(0.05f, speed)),
             { 12.0f, 188.0f, 250.0f, 15.0f }, 10.5f, true, ivoryDim, Just::centredLeft, 0.03f);
        juce::String range;
        if (trem) range = "GAIN 1.00 - " + juce::String(1.0f - depth * 0.45f, 2);
        else if (bbd) range = juce::String(1000.0f * 256.0f / (106904.5f * std::exp2(depth * 0.5f)), 2) + " - "
                              + juce::String(1000.0f * 256.0f / (106904.5f * std::exp2(-depth * 0.5f)), 2) + " ms";
        else range = "13.5 - " + juce::String(13.5f + 4.5f * depth, 1) + " ms";
        text(g, range, { 250.0f, 188.0f, 154.0f, 15.0f }, 11.5f, true, amber, Just::centredRight, 0.03f);
        text(g, on ? (bbd ? "ONE SHARED WET PATH  (LEFT = RIGHT)" : "FAINT TRACE = RIGHT CHANNEL, PHASE +0.95 PI")
                   : "CHORUS SWITCH OFF  -  SIGNAL BYPASSES THIS STAGE",
             { 12.0f, 208.0f, 396.0f, 15.0f }, 10.0f, false, axisInk(), Just::centredLeft, 0.03f);
    }
};

// ---- Analog voice numbers ----------------------------------------------------------------------
class VoiceLcd final : public LiveWidget
{
public:
    explicit VoiceLcd(Cs80AudioProcessor& p) : LiveWidget(p, 328.0f, 100.0f) {}

private:
    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_voiceDispersion); s[1] = v(cs80::P_drift); s[2] = v(cs80::P_analogNoise);
        s[3] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const float disp = v(cs80::P_voiceDispersion), drift = v(cs80::P_drift), noise = v(cs80::P_analogNoise);
        auto row = [&](float y, const juce::String& k, const juce::String& val)
        {
            text(g, k, { 14.0f, y, 120.0f, 20.0f }, 10.5f, true, ivoryDim, Just::centredLeft, 0.1f);
            text(g, val, { 130.0f, y, 186.0f, 20.0f }, 13.0f, true, amberBright, Just::centredRight, 0.02f);
        };
        row(5.0f, "TUNING", plusMinus() + juce::String(disp * cs80::kProvisionalVoiceTuneSpreadCents, 1) + " ct");
        row(28.0f, "PAN", plusMinus() + juce::String(disp * 0.8f, 2));
        row(51.0f, "DRIFT", juce::String(juce::roundToInt(drift * 100.0f)) + "%  /  32 s");
        row(74.0f, "NOISE FLOOR", noise <= 0.001f ? juce::String("OFF")
                                                   : juce::String(juce::roundToInt(20.0f * std::log10(noise * 0.0008f))) + " dB");
        if (isolated())
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }
};

// ---- Voice dispersion: pan against tuning for all sixteen ranks ---------------------------------
class VoiceScatter final : public LiveWidget
{
public:
    explicit VoiceScatter(Cs80AudioProcessor& p) : LiveWidget(p, 300.0f, 240.0f)
    {
        enableMouse(false);
        // Same generator and draw order as Engine::prepare: tune, cutoff, level, env, pan per rank.
        cs80::Rng rng(0xC5801234u);
        for (int voice = 0; voice < 8; ++voice)
            for (int rank = 0; rank < 2; ++rank)
            {
                tune[(size_t) (voice * 2 + rank)] = rng.nextBipolar();
                rng.nextBipolar();
                rng.nextBipolar();
                rng.nextBipolar();
                pan[(size_t) (voice * 2 + rank)] = rng.nextBipolar();
            }
    }

    void paintStatic(juce::Graphics& g) override
    {
        glassWell(g, { 0.0f, 0.0f, designW, designH });
        const auto plot = plotRect();
        hline(g, plot.getX(), plot.getRight(), plot.getCentreY(), 0.30f);
        vline(g, plot.getCentreX(), plot.getY(), plot.getBottom(), 0.30f);
        hline(g, plot.getX(), plot.getRight(), plot.getY(), 0.12f);
        hline(g, plot.getX(), plot.getRight(), plot.getBottom(), 0.12f);
        vline(g, plot.getX(), plot.getY(), plot.getBottom(), 0.12f);
        vline(g, plot.getRight(), plot.getY(), plot.getBottom(), 0.12f);
        const float spread = cs80::kProvisionalVoiceTuneSpreadCents;
        axisText(g, "+" + juce::String(spread, 0) + " ct", { 2.0f, plot.getY() - 6.0f, 42.0f, 12.0f }, Just::centredRight);
        axisText(g, "0", { 2.0f, plot.getCentreY() - 6.0f, 42.0f, 12.0f }, Just::centredRight);
        axisText(g, "-" + juce::String(spread, 0), { 2.0f, plot.getBottom() - 6.0f, 42.0f, 12.0f }, Just::centredRight);
        axisText(g, "L 0.8", { plot.getX(), plot.getBottom() + 2.0f, 44.0f, 12.0f }, Just::centredLeft);
        axisText(g, "PAN", { plot.getCentreX() - 20.0f, plot.getBottom() + 2.0f, 40.0f, 12.0f });
        axisText(g, "0.8 R", { plot.getRight() - 34.0f, plot.getBottom() + 2.0f, 44.0f, 12.0f }, Just::centredRight);
    }

private:
    static Rect plotRect() { return { 48.0f, 28.0f, 240.0f, 172.0f }; }

    Pt pointOf(int i, float disp) const
    {
        const auto plot = plotRect();
        return { plot.getCentreX() + pan[(size_t) i] * disp * plot.getWidth() * 0.5f,
                 plot.getCentreY() - tune[(size_t) i] * disp * plot.getHeight() * 0.5f };
    }
    float dispersion() const { return isolated() ? 0.0f : v(cs80::P_voiceDispersion); }

    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_voiceDispersion); s[1] = v(cs80::P_parityMode); s[2] = (float) hover;
        unsigned mask = 0;
        for (int i = 0; i < 8; ++i)
            if (proc.isVoiceActive(i))
                mask |= 1u << i;
        s[3] = (float) mask;
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const auto p = toDesign(e.getPosition());
        int best = -1;
        float bestD = 11.0f;
        for (int i = 0; i < 16; ++i)
        {
            const float d = pointOf(i, dispersion()).getDistanceFrom(p);
            if (d < bestD) { bestD = d; best = i; }
        }
        if (best != hover)
        {
            hover = best;
            repaint();
        }
    }
    void mouseExit(const juce::MouseEvent&) override
    {
        if (hover != -1)
        {
            hover = -1;
            repaint();
        }
    }

    void draw(juce::Graphics& g) override
    {
        const float disp = dispersion();
        text(g, "SIXTEEN RANKS:  PAN AGAINST TUNING", { 12.0f, 5.0f, 280.0f, 16.0f }, 10.5f, true, axisInk(), Just::centredLeft, 0.05f);
        for (int rank = 1; rank >= 0; --rank)
            for (int voice = 0; voice < 8; ++voice)
            {
                const int i = voice * 2 + rank;
                const Pt c = pointOf(i, disp);
                const bool sounding = proc.isVoiceActive(voice);
                const bool hot = hover == i;
                if (sounding)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.18f));
                    g.fillEllipse(c.x - 11.0f, c.y - 11.0f, 22.0f, 22.0f);
                    g.setColour(juce::Colours::white.withAlpha(0.85f));
                    g.drawEllipse(c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f, 1.3f);
                }
                g.setColour(juce::Colours::black.withAlpha(0.7f));
                g.fillEllipse(c.x - 7.4f, c.y - 7.0f, 14.8f, 14.8f);
                g.setColour(rank == 0 ? amberBright : ivory.withAlpha(0.92f));
                g.fillEllipse(c.x - 6.5f, c.y - 6.5f, 13.0f, 13.0f);
                if (hot)
                {
                    g.setColour(juce::Colour(0xfff6df91));
                    g.drawEllipse(c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f, 1.6f);
                }
                text(g, juce::String(voice + 1), { c.x - 6.5f, c.y - 6.5f, 13.0f, 13.0f }, 9.5f, true, juce::Colour(0xff17120a), Just::centred);
            }
        if (hover >= 0)
        {
            const int voice = hover / 2, rank = hover % 2;
            text(g, "CARD " + juce::String(voice + 1) + " / RANK " + (rank == 0 ? "I" : "II") + "   "
                        + signedString(tune[(size_t) hover] * disp * cs80::kProvisionalVoiceTuneSpreadCents, 1, " ct") + "   PAN "
                        + signedString(pan[(size_t) hover] * disp * 0.8f, 2, ""),
                 { 12.0f, 216.0f, 280.0f, 18.0f }, 11.0f, true, ivory, Just::centredLeft, 0.02f);
        }
        else
        {
            g.setColour(amberBright);
            g.fillEllipse(14.0f, 221.0f, 8.0f, 8.0f);
            text(g, "RANK I", { 26.0f, 216.0f, 56.0f, 18.0f }, 10.5f, true, axisInk(), Just::centredLeft);
            g.setColour(ivory.withAlpha(0.92f));
            g.fillEllipse(86.0f, 221.0f, 8.0f, 8.0f);
            text(g, "RANK II", { 98.0f, 216.0f, 60.0f, 18.0f }, 10.5f, true, axisInk(), Just::centredLeft);
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.drawEllipse(164.0f, 220.0f, 10.0f, 10.0f, 1.3f);
            text(g, "SOUNDING", { 178.0f, 216.0f, 70.0f, 18.0f }, 10.5f, true, axisInk(), Just::centredLeft);
        }
        if (isolated())
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }

    int hover = -1;
    std::array<float, 16> tune {}, pan {};
};

// ---- FX selection shared by the chain, the slot rows and the detail diagram ---------------------
struct FxSel
{
    int slot = 0;
    std::vector<LiveWidget*> peers;
    void set(int s)
    {
        if (s == slot)
            return;
        slot = s;
        for (auto* p : peers)
            p->refresh();
    }
};

// ---- Signal-order strip: clickable chips. The chorus lamp is its real on/off switch. -----------
class FxChain final : public LiveWidget
{
public:
    FxChain(Cs80AudioProcessor& p, std::shared_ptr<FxSel> selection) : LiveWidget(p, 1472.0f, 50.0f), sel(std::move(selection))
    {
        enableMouse(false);
    }

private:
    static constexpr float gap = 30.0f;
    static Rect nodeRect(int i)
    {
        const float nodeW = (1472.0f - 16.0f - gap * 5.0f) / 6.0f;
        return { 8.0f + (float) i * (nodeW + gap), 4.0f, nodeW, 42.0f };
    }

    void sample(Sig& s) const override
    {
        for (int i = 0; i < 3; ++i)
        {
            s[(size_t) i] = v(cs80::P_fx1Type + i * 2);
            s[(size_t) (3 + i)] = v(cs80::P_fx1Mix + i * 2);
        }
        s[6] = v(cs80::P_chorusOn); s[7] = v(cs80::P_chorusMode); s[8] = (float) policy(); s[9] = (float) sel->slot;
        s[10] = (float) hover;
    }

    // 1 chorus lamp, 2..4 FX chips, 0 nothing.
    int hit(Pt p) const
    {
        if (Rect(nodeRect(1).getX(), nodeRect(1).getY(), 38.0f, nodeRect(1).getHeight()).contains(p))
            return 1;
        for (int i = 0; i < 3; ++i)
            if (nodeRect(2 + i).contains(p))
                return 2 + i;
        return 0;
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const int h = hit(toDesign(e.getPosition()));
        if (h != hover)
        {
            hover = h;
            setMouseCursor(h != 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }
    void mouseExit(const juce::MouseEvent&) override
    {
        if (hover != 0)
        {
            hover = 0;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int h = hit(toDesign(e.getPosition()));
        if (h == 1)
        {
            setParameterOnce(proc, cs80::P_chorusOn, v(cs80::P_chorusOn) >= 0.5f ? 0.0f : 1.0f);
            refresh();
        }
        else if (h >= 2)
            sel->set(h - 2);
    }

    void draw(juce::Graphics& g) override
    {
        const int pol = policy();
        struct Node { juce::String name, detail; bool lit; bool masked; };
        std::vector<Node> nodes;
        nodes.push_back({ "VOICE MIXER", "8 VOICES x 2 RANKS", true, false });
        const bool chorusOn = v(cs80::P_chorusOn) >= 0.5f, trem = v(cs80::P_chorusMode) >= 0.5f;
        nodes.push_back({ "CS-80 CHORUS", ! chorusOn ? "OFF" : (trem ? "TREMOLO" : (pol >= 1 ? "OE2 BBD" : "GENERIC DELAY")),
                          chorusOn, false });
        for (int i = 0; i < 3; ++i)
        {
            const int type = juce::jlimit(0, 5, juce::roundToInt(v(cs80::P_fx1Type + i * 2)));
            const float mix = v(cs80::P_fx1Mix + i * 2);
            const bool active = type > 0 && mix > 0.001f;
            juce::String detail = type == 0 ? "EMPTY" : kFxTypes[type] + "  " + juce::String(juce::roundToInt(mix * 100.0f)) + "%";
            if (pol == 2)
                detail = "MASKED";
            nodes.push_back({ "FX " + juce::String(i + 1), detail, active && pol != 2, pol == 2 });
        }
        nodes.push_back({ "OUTPUT", pol == 0 ? "DC BLOCK + SOFT LIMIT" : "RAW SOURCE OUTPUT", true, false });

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const Rect r = nodeRect((int) i);
            const auto& node = nodes[i];
            const bool isFx = i >= 2 && i <= 4;
            const bool selected = isFx && sel->slot == (int) i - 2;
            const bool hot = (int) i == 1 ? hover == 1 : (isFx && hover == (int) i);
            if (node.lit)
            {
                g.setColour(amber.withAlpha(0.12f));
                g.fillRoundedRectangle(r.expanded(3.0f), 7.0f);
            }
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRoundedRectangle(r.translated(0.0f, 1.5f), 5.0f);
            g.setGradientFill(node.lit ? juce::ColourGradient(juce::Colour(0xff4a3a1c), r.getX(), r.getY(),
                                                              juce::Colour(0xff271d0e), r.getX(), r.getBottom(), false)
                                       : juce::ColourGradient(juce::Colour(hot ? 0xff34312c : 0xff2b2926), r.getX(), r.getY(),
                                                              juce::Colour(0xff151311), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle(r, 5.0f);
            g.setColour(selected ? juce::Colour(0xfff6df91) : (node.lit ? amber.withAlpha(0.75f) : juce::Colours::black.withAlpha(0.85f)));
            g.drawRoundedRectangle(r, 5.0f, selected ? 2.0f : 1.0f);
            g.setColour(juce::Colours::white.withAlpha(node.lit ? 0.16f : 0.07f));
            g.drawLine(r.getX() + 5.0f, r.getY() + 1.2f, r.getRight() - 5.0f, r.getY() + 1.2f, 1.0f);
            if (i == 1)
                jewel(g, { r.getX() + 17.0f, r.getCentreY() }, 5.0f, amber, node.lit);
            else
            {
                g.setColour(node.lit ? amberBright : ivoryDim.withAlpha(0.5f));
                g.fillRoundedRectangle(r.getX() + 12.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f, 1.5f);
            }
            text(g, node.name, { r.getX() + 30.0f, r.getY() + 4.0f, r.getWidth() - 36.0f, 16.0f }, 10.0f, true,
                 node.lit ? amberBright : ivoryDim, Just::centredLeft, 0.1f);
            text(g, node.detail, { r.getX() + 30.0f, r.getY() + 20.0f, r.getWidth() - 36.0f, 18.0f }, 11.0f, true,
                 node.lit ? ivory : ivoryDim.withAlpha(0.8f), Just::centredLeft, 0.03f);
            if (i + 1 < nodes.size())
            {
                const float ax = r.getRight() + 5.0f, bx = r.getRight() + gap - 6.0f, ay = r.getCentreY();
                etchLine(g, ax, ay, bx - 2.0f, ay, 0.85f);
                juce::Path head;
                head.addTriangle(bx, ay, bx - 6.0f, ay - 3.5f, bx - 6.0f, ay + 3.5f);
                g.setColour(gold);
                g.fillPath(head);
            }
        }
    }

    std::shared_ptr<FxSel> sel;
    int hover = 0;
};

// ---- One row of the insert list: badge, selection plate and summary. The type menu and mix knob ---
// are real controls placed over it.
class FxSlotRow final : public LiveWidget
{
public:
    FxSlotRow(Cs80AudioProcessor& p, int slot, std::shared_ptr<FxSel> selection)
        : LiveWidget(p, 572.0f, 92.0f), slotIndex(slot), sel(std::move(selection))
    {
        enableMouse(true);
    }

    void paintStatic(juce::Graphics&) override {}

private:
    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_fx1Type + slotIndex * 2); s[1] = v(cs80::P_fx1Mix + slotIndex * 2); s[2] = (float) sel->slot;
        s[3] = v(cs80::P_parityMode); s[4] = (float) policy();
    }

    void mouseDown(const juce::MouseEvent&) override { sel->set(slotIndex); }

    void draw(juce::Graphics& g) override
    {
        const int type = juce::jlimit(0, 5, juce::roundToInt(v(cs80::P_fx1Type + slotIndex * 2)));
        const float mix = v(cs80::P_fx1Mix + slotIndex * 2);
        static const float factor[] = { 0.0f, 0.62f, 0.72f, 0.58f, 0.62f, 1.0f };
        const bool selected = sel->slot == slotIndex;
        const Rect plate(0.0f, 0.0f, designW, designH);
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.fillRoundedRectangle(plate.reduced(2.0f), 5.0f);
        if (selected)
        {
            g.setColour(amber.withAlpha(0.10f));
            g.fillRoundedRectangle(plate.reduced(2.0f), 5.0f);
            g.setColour(juce::Colour(0xfff6df91).withAlpha(0.85f));
            g.drawRoundedRectangle(plate.reduced(2.0f), 5.0f, 1.6f);
        }
        else
        {
            g.setColour(juce::Colours::white.withAlpha(0.07f));
            g.drawRoundedRectangle(plate.reduced(2.0f), 5.0f, 1.0f);
        }
        // Badge in the same plastic as the signal-order chips.
        const Rect badge(10.0f, 28.0f, 46.0f, 34.0f);
        capChip(g, badge, type > 0 ? capYellow : juce::Colour(0xff4a463f));
        text(g, "FX " + juce::String(slotIndex + 1), badge, 12.5f, true, type > 0 ? juce::Colour(0xff17120a) : ivoryDim, Just::centred, 0.05f);
        drawControlCaption(g, "EFFECT", { 70.0f, 4.0f, 170.0f, 17.0f }, Just::centred, 11.5f);

        const bool masked = policy() == 2;
        if (type == 0)
        {
            text(g, "EMPTY", { 372.0f, 22.0f, 190.0f, 20.0f }, 14.0f, true, amber, Just::centredLeft, 0.1f);
            text(g, "choose an effect", { 372.0f, 44.0f, 190.0f, 16.0f }, 11.0f, false, axisInk(), Just::centredLeft);
        }
        else
        {
            text(g, kFxTypes[type], { 372.0f, 18.0f, 196.0f, 20.0f }, 13.5f, true, masked ? ivoryDim : ivory, Just::centredLeft, 0.06f);
            text(g, masked ? "MASKED BY ISOLATION"
                           : "MIX " + juce::String(juce::roundToInt(mix * 100.0f)) + "%   WET "
                                 + juce::String(juce::roundToInt(juce::jlimit(0.0f, 1.0f, mix * factor[type]) * 100.0f)) + "%",
                 { 372.0f, 40.0f, 196.0f, 16.0f }, 11.0f, true, masked ? amber : amberBright, Just::centredLeft, 0.03f);
            text(g, selected ? "SHOWN AT RIGHT" : "CLICK TO SHOW", { 372.0f, 60.0f, 196.0f, 14.0f }, 10.0f, false, axisInk(), Just::centredLeft);
        }
    }

    int slotIndex;
    std::shared_ptr<FxSel> sel;
};

// ---- Detail diagram of the selected FX slot -------------------------------------------------------
class FxDiagram final : public LiveWidget
{
public:
    FxDiagram(Cs80AudioProcessor& p, std::shared_ptr<FxSel> selection)
        : LiveWidget(p, 820.0f, 300.0f), sel(std::move(selection))
    {
        enableMouse(false);
        cs80::Rng rng(0x7a11u);
        for (auto& x : scatter)
            x = rng.nextUnipolar();
    }

private:
    static Rect plotRect() { return { 64.0f, 40.0f, 730.0f, 190.0f }; }
    // An empty slot offers the five effects as tiles; a click assigns one.
    static Rect tileRect(int i) { return { 64.0f + (float) i * 148.5f, 62.0f, 136.0f, 168.0f }; }

    bool slotEmpty() const { return juce::roundToInt(v(cs80::P_fx1Type + sel->slot * 2)) == 0; }
    int tileAt(Pt p) const
    {
        if (! slotEmpty())
            return -1;
        for (int i = 0; i < 5; ++i)
            if (tileRect(i).contains(p))
                return i;
        return -1;
    }
    void mouseMove(const juce::MouseEvent& e) override
    {
        const int t = tileAt(toDesign(e.getPosition()));
        if (t != hoverTile)
        {
            hoverTile = t;
            setMouseCursor(t >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }
    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoverTile >= 0)
        {
            hoverTile = -1;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int t = tileAt(toDesign(e.getPosition()));
        if (t >= 0)
        {
            setParameterOnce(proc, cs80::P_fx1Type + sel->slot * 2, (float) (t + 1));
            hoverTile = -1;
            refresh();
        }
    }

    void drawTile(juce::Graphics& g, int i, bool hot)
    {
        static const char* const names[5] = { "CHORUS", "PHASER", "TAPE DELAY", "PLATE REVERB", "DRIVE" };
        static const char* const notes[5] = { "13.5 - 16.3 ms sweep", "6 all-pass stages", "340 ms taps", "dense decaying tail", "x3.2 soft clip" };
        const Rect r = tileRect(i);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(r.translated(0.0f, 1.5f), 6.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(hot ? 0xff353029 : 0xff262320), r.getX(), r.getY(), juce::Colour(0xff12100e), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(hot ? juce::Colour(0xfff6df91) : juce::Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle(r, 6.0f, hot ? 1.8f : 1.0f);
        const Rect gl(r.getX() + 16.0f, r.getY() + 16.0f, r.getWidth() - 32.0f, 70.0f);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(gl, 3.0f);
        juce::Path p;
        const juce::Colour ink = hot ? amberBright : amber;
        constexpr float two = juce::MathConstants<float>::twoPi;
        switch (i)
        {
            case 0:
                for (int ch = 0; ch < 2; ++ch)
                {
                    juce::Path q;
                    for (int k = 0; k <= 60; ++k)
                    {
                        const float t = (float) k / 60.0f;
                        const float x = gl.getX() + 6.0f + (gl.getWidth() - 12.0f) * t;
                        const float y = gl.getCentreY() - std::sin(two * 1.5f * t + (float) ch * 3.0f) * gl.getHeight() * 0.28f;
                        if (k == 0) q.startNewSubPath(x, y); else q.lineTo(x, y);
                    }
                    g.setColour(ch == 0 ? ink : ivory.withAlpha(0.5f));
                    g.strokePath(q, juce::PathStrokeType(1.6f));
                }
                break;
            case 1:
                for (int k = 0; k <= 60; ++k)
                {
                    const float t = (float) k / 60.0f;
                    const float notch = 1.0f - 0.75f * std::exp(-std::pow((t - 0.3f) / 0.07f, 2.0f)) - 0.75f * std::exp(-std::pow((t - 0.62f) / 0.09f, 2.0f));
                    const float x = gl.getX() + 6.0f + (gl.getWidth() - 12.0f) * t;
                    const float y = gl.getBottom() - 8.0f - (gl.getHeight() - 20.0f) * notch;
                    if (k == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
                }
                g.setColour(ink);
                g.strokePath(p, juce::PathStrokeType(1.8f));
                break;
            case 2:
            {
                float amp = 1.0f;
                for (int k = 0; k < 5; ++k)
                {
                    const float h = (gl.getHeight() - 14.0f) * amp;
                    g.setColour(k == 0 ? ivory.withAlpha(0.85f) : ink.withAlpha(0.9f));
                    g.fillRoundedRectangle(gl.getX() + 14.0f + (float) k * 20.0f, gl.getBottom() - 6.0f - h, 7.0f, h, 1.5f);
                    amp *= 0.55f;
                }
                break;
            }
            case 3:
                g.setColour(ivory.withAlpha(0.85f));
                g.fillRect(gl.getX() + 10.0f, gl.getY() + 10.0f, 5.0f, gl.getHeight() - 16.0f);
                for (size_t k = 0; k < 30; ++k)
                {
                    const float t = (float) k / 30.0f;
                    const float h = (gl.getHeight() - 22.0f) * std::exp(-3.0f * t) * (0.45f + 0.55f * scatter[k]);
                    g.setColour(ink.withAlpha(0.9f));
                    g.fillRect(gl.getX() + 22.0f + (gl.getWidth() - 32.0f) * t, gl.getBottom() - 6.0f - h, 2.0f, h);
                }
                break;
            default:
                for (int k = 0; k <= 60; ++k)
                {
                    const float x = -1.0f + 2.0f * (float) k / 60.0f;
                    const float y = cs80::softClip(x * 3.2f) * 0.68f;
                    const float px = gl.getCentreX() + x * (gl.getHeight() * 0.42f);
                    const float py = gl.getCentreY() - y * (gl.getHeight() * 0.42f);
                    if (k == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
                }
                g.setColour(ink);
                g.strokePath(p, juce::PathStrokeType(1.8f));
                break;
        }
        text(g, names[i], { r.getX(), r.getY() + 96.0f, r.getWidth(), 20.0f }, 12.5f, true, hot ? ivory : ivory.withAlpha(0.92f), Just::centred, 0.06f);
        text(g, notes[i], { r.getX(), r.getY() + 118.0f, r.getWidth(), 16.0f }, 10.5f, false, axisInk(), Just::centred);
        text(g, hot ? "CLICK TO USE" : "ADD", { r.getX(), r.getY() + 140.0f, r.getWidth(), 16.0f }, 10.0f, true, hot ? amberBright : gold, Just::centred, 0.14f);
    }

    void sample(Sig& s) const override
    {
        const int slot = sel->slot;
        s[0] = v(cs80::P_fx1Type + slot * 2); s[1] = v(cs80::P_fx1Mix + slot * 2); s[2] = v(cs80::P_parityMode);
        s[3] = (float) slot; s[4] = (float) hoverTile;
    }

    void draw(juce::Graphics& g) override
    {
        const int slot = sel->slot;
        const int type = juce::jlimit(0, 5, juce::roundToInt(v(cs80::P_fx1Type + slot * 2)));
        const float mix = v(cs80::P_fx1Mix + slot * 2);
        const bool iso = isolated();
        static const float factor[] = { 0.0f, 0.62f, 0.72f, 0.58f, 0.62f, 1.0f };
        const float wet = juce::jlimit(0.0f, 1.0f, mix * factor[type]);
        const auto plot = plotRect();
        constexpr float two = juce::MathConstants<float>::twoPi;
        const bool active = type > 0 && mix > 0.001f && ! iso;
        const juce::Colour trace = active ? amberBright : ivoryDim.withAlpha(0.8f);
        juce::String title, foot;
        auto frame = [&](int divisionsX, int divisionsY)
        {
            for (int i = 0; i <= divisionsX; ++i)
                vline(g, plot.getX() + plot.getWidth() * (float) i / (float) divisionsX, plot.getY(), plot.getBottom(),
                      i == 0 || i == divisionsX ? 0.30f : 0.13f);
            for (int i = 0; i <= divisionsY; ++i)
                hline(g, plot.getX(), plot.getRight(), plot.getBottom() - plot.getHeight() * (float) i / (float) divisionsY,
                      i == 0 ? 0.34f : 0.13f);
        };

        if (type == 0)
        {
            title = "FX " + juce::String(slot + 1) + "  -  EMPTY.  CHOOSE AN EFFECT";
            for (int i = 0; i < 5; ++i)
                drawTile(g, i, hoverTile == i);
            foot = "AN EMPTY SLOT PASSES THE SIGNAL THROUGH UNCHANGED.  CLICK A TILE OR USE THE EFFECT MENU IN THE LIST";
        }
        else
        {
            title = "FX " + juce::String(slot + 1) + "  -  " + kFxTypes[type];
            switch (type)
            {
                case 1: // modern stereo chorus: 0.62 Hz, depth 0.62, generic delay line
                {
                    frame(4, 4);
                    for (int i = 0; i <= 4; i += 2)
                        axisText(g, juce::String(i * 5) + (i == 4 ? " ms" : ""), { 4.0f, plot.getBottom() - plot.getHeight() * (float) i / 4.0f - 6.5f, 56.0f, 13.0f },
                                 Just::centredRight);
                    axisText(g, "0", { plot.getX() - 12.0f, plot.getBottom() + 3.0f, 24.0f, 13.0f });
                    axisText(g, "3.2 s  (2 periods)", { plot.getRight() - 110.0f, plot.getBottom() + 3.0f, 110.0f, 13.0f }, Just::centredRight);
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        juce::Path path;
                        for (int i = 0; i <= 240; ++i)
                        {
                            const float t = (float) i / 240.0f;
                            const float mod = std::sin(two * 2.0f * t + (ch == 0 ? 0.0f : juce::MathConstants<float>::pi * 0.95f));
                            const float ms = 13.5f + 4.5f * 0.62f * (0.5f + 0.5f * mod);
                            const float x = plot.getX() + plot.getWidth() * t;
                            const float y = plot.getBottom() - plot.getHeight() * ms / 20.0f;
                            if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
                        }
                        g.setColour(ch == 0 ? trace : ivory.withAlpha(active ? 0.5f : 0.25f));
                        g.strokePath(path, juce::PathStrokeType(ch == 0 ? 2.0f : 1.3f));
                    }
                    foot = "DELAY SWEEP 0.62 Hz,  13.5 - 16.3 ms.  FAINT TRACE = RIGHT CHANNEL";
                    break;
                }
                case 2: // six-stage all-pass phaser, bilinear response at 96 kHz
                {
                    frame(4, 3);
                    constexpr float sr = 96000.0f;
                    for (int pass = 0; pass < 3; ++pass)
                    {
                        const float mod = pass == 0 ? -0.72f : (pass == 1 ? 0.0f : 0.72f);
                        const float cutoff = juce::jlimit(90.0f, sr * 0.42f, 680.0f * std::pow(2.0f, 2.9f * mod));
                        const float tn = std::tan(juce::MathConstants<float>::pi * cutoff / sr);
                        // Engine stage: H = (-a + z^-1) / (1 - a z^-1) with a = (1 - t) / (1 + t).
                        const float a = (1.0f - tn) / (1.0f + tn);
                        juce::Path path;
                        for (int i = 0; i <= 200; ++i)
                        {
                            const float t = (float) i / 200.0f;
                            const float f = 20.0f * std::pow(40000.0f / 20.0f, t);
                            const auto zinv = std::polar(1.0f, -two * f / sr);
                            const std::complex<float> h1 = (std::complex<float>(-a, 0.0f) + zinv) / (1.0f - a * zinv);
                            auto h = std::complex<float>(1.0f, 0.0f);
                            for (int s = 0; s < 6; ++s) h *= h1;
                            const float mag = std::abs((1.0f - wet) + wet * h);
                            const float db = juce::jlimit(-30.0f, 3.0f, 20.0f * std::log10(juce::jmax(mag, 1.0e-4f)));
                            const float x = plot.getX() + plot.getWidth() * t;
                            const float y = plot.getY() + plot.getHeight() * (3.0f - db) / 33.0f;
                            if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
                        }
                        g.setColour(pass == 1 ? trace : ivory.withAlpha(active ? 0.4f : 0.22f));
                        g.strokePath(path, juce::PathStrokeType(pass == 1 ? 2.0f : 1.2f));
                    }
                    const char* const dbText[4] = { "+3 dB", "-8", "-19", "-30" };
                    for (int i = 0; i < 4; ++i)
                        axisText(g, dbText[i], { 4.0f, plot.getY() + plot.getHeight() * (float) i / 3.0f - 6.5f, 56.0f, 13.0f }, Just::centredRight);
                    axisText(g, "20 Hz", { plot.getX() - 12.0f, plot.getBottom() + 3.0f, 50.0f, 13.0f }, Just::centredLeft);
                    axisText(g, "40 kHz", { plot.getRight() - 50.0f, plot.getBottom() + 3.0f, 50.0f, 13.0f }, Just::centredRight);
                    const float x1k = plot.getX() + plot.getWidth() * std::log(1000.0f / 20.0f) / std::log(40000.0f / 20.0f);
                    axisText(g, "1 kHz", { x1k - 25.0f, plot.getBottom() + 3.0f, 50.0f, 13.0f });
                    foot = "6 ALL-PASS STAGES, 0.32 Hz.  LOW / MID / HIGH SWEEP POSITIONS AT A 96 kHz ENGINE RATE";
                    break;
                }
                case 3: // tape delay: 340 ms taps, cross-feedback 0.36
                {
                    frame(5, 4);
                    const float step = plot.getWidth() / 6.0f;
                    auto stem = [&](float x, float height, juce::Colour c)
                    {
                        const float h = juce::jmax(2.0f, plot.getHeight() * height);
                        g.setColour(c);
                        g.fillRoundedRectangle(x - 5.0f, plot.getBottom() - h, 10.0f, h, 2.0f);
                    };
                    stem(plot.getX() + step * 0.5f, 1.0f - wet, ivory.withAlpha(active ? 0.9f : 0.4f));
                    axisText(g, "DRY", { plot.getX() + step * 0.5f - 24.0f, plot.getBottom() + 3.0f, 48.0f, 13.0f });
                    float amp = wet;
                    for (int i = 1; i <= 5; ++i)
                    {
                        const float x = plot.getX() + step * (0.5f + (float) i);
                        stem(x, amp, i % 2 == 1 ? trace : trace.withAlpha(0.6f));
                        axisText(g, juce::String(340 * i) + " ms", { x - 30.0f, plot.getBottom() + 3.0f, 60.0f, 13.0f });
                        amp *= 0.36f;
                    }
                    axisText(g, "1", { 4.0f, plot.getY() - 6.0f, 56.0f, 13.0f }, Just::centredRight);
                    axisText(g, "0", { 4.0f, plot.getBottom() - 6.0f, 56.0f, 13.0f }, Just::centredRight);
                    foot = "340 ms TAPS, EACH x0.36 OF THE LAST, SIDE TO SIDE";
                    break;
                }
                case 4: // plate reverb: dry stem plus a dense decaying tail
                {
                    frame(4, 4);
                    g.setColour(ivory.withAlpha(active ? 0.9f : 0.4f));
                    g.fillRoundedRectangle(plot.getX() + 8.0f, plot.getBottom() - plot.getHeight() * (1.0f - wet), 10.0f,
                                           plot.getHeight() * (1.0f - wet), 2.0f);
                    for (size_t i = 0; i < scatter.size(); ++i)
                    {
                        const float t = (float) i / (float) scatter.size();
                        const float x = plot.getX() + 40.0f + (plot.getWidth() - 50.0f) * t;
                        const float env = std::exp(-3.6f * t) * (0.45f + 0.55f * scatter[i]);
                        const float h = plot.getHeight() * wet * 0.95f * env;
                        g.setColour(i % 2 ? trace : trace.withAlpha(0.6f));
                        g.fillRect(x, plot.getBottom() - juce::jmax(1.0f, h), 3.0f, juce::jmax(1.0f, h));
                    }
                    axisText(g, "1", { 4.0f, plot.getY() - 6.0f, 56.0f, 13.0f }, Just::centredRight);
                    axisText(g, "0", { 4.0f, plot.getBottom() - 6.0f, 56.0f, 13.0f }, Just::centredRight);
                    axisText(g, "DRY", { plot.getX() - 6.0f, plot.getBottom() + 3.0f, 36.0f, 13.0f });
                    axisText(g, "TAIL  (SHAPE, NOT A MEASURED TIME)", { plot.getCentreX() - 130.0f, plot.getBottom() + 3.0f, 260.0f, 13.0f });
                    foot = "WET LEVEL SCALES THE TAIL";
                    break;
                }
                default: // drive
                {
                    const float side = plot.getHeight() - 8.0f;
                    const Rect box(plot.getCentreX() - side * 0.5f, plot.getY() + 4.0f, side, side);
                    g.setColour(kInk.withAlpha(0.6f));
                    g.fillRect(box);
                    hline(g, box.getX(), box.getRight(), box.getCentreY(), 0.28f);
                    vline(g, box.getCentreX(), box.getY(), box.getBottom(), 0.28f);
                    g.setColour(ivory.withAlpha(0.35f));
                    g.drawLine(box.getX(), box.getBottom(), box.getRight(), box.getY(), 1.0f);
                    juce::Path path;
                    for (int i = 0; i <= 100; ++i)
                    {
                        const float x = -1.0f + 2.0f * (float) i / 100.0f;
                        const float y = x + (cs80::softClip(x * 3.2f) * 0.68f - x) * mix;
                        const float px = box.getX() + (x * 0.5f + 0.5f) * box.getWidth();
                        const float py = box.getBottom() - (juce::jlimit(-1.0f, 1.0f, y) * 0.5f + 0.5f) * box.getHeight();
                        if (i == 0) path.startNewSubPath(px, py); else path.lineTo(px, py);
                    }
                    g.setColour(trace);
                    g.strokePath(path, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    axisText(g, "+1", { box.getX() - 40.0f, box.getY() - 6.0f, 36.0f, 13.0f }, Just::centredRight);
                    axisText(g, "-1", { box.getX() - 40.0f, box.getBottom() - 6.0f, 36.0f, 13.0f }, Just::centredRight);
                    axisText(g, "OUT", { box.getX() - 40.0f, box.getCentreY() - 6.0f, 36.0f, 13.0f }, Just::centredRight);
                    axisText(g, "-1", { box.getX() - 12.0f, box.getBottom() + 3.0f, 24.0f, 13.0f });
                    axisText(g, "INPUT", { box.getCentreX() - 25.0f, box.getBottom() + 3.0f, 50.0f, 13.0f });
                    axisText(g, "+1", { box.getRight() - 12.0f, box.getBottom() + 3.0f, 24.0f, 13.0f });
                    foot = "MIX 0 = DIAGONAL, MIX 1 = x3.2 SOFT CLIP WITH OUTPUT x0.68";
                    break;
                }
            }
        }
        text(g, title, { 14.0f, 8.0f, 460.0f, 22.0f }, 14.0f, true, amber, Just::centredLeft, 0.08f);
        text(g, foot, { 14.0f, 272.0f, 792.0f, 18.0f }, 11.0f, false, axisInk(), Just::centredLeft, 0.02f);
        if (type > 0 && ! iso)
            text(g, "WET " + juce::String(juce::roundToInt(wet * 100.0f)) + "%", { 640.0f, 8.0f, 166.0f, 22.0f }, 14.0f, true, ivory,
                 Just::centredRight, 0.04f);
        if (iso)
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }

    std::shared_ptr<FxSel> sel;
    std::array<float, 110> scatter {};
    int hoverTile = -1;
};

// ---- Macro destinations: a flat bipolar bar and three equal destination rows ---------------------
class MacroBars final : public LiveWidget
{
public:
    MacroBars(Cs80AudioProcessor& p, int index) : LiveWidget(p, 168.0f, 98.0f), m(index) {}
    void paintStatic(juce::Graphics&) override {}

private:
    void sample(Sig& s) const override { s[0] = v(cs80::P_macro1 + m); s[1] = v(cs80::P_parityMode); }

    void draw(juce::Graphics& g) override
    {
        const bool iso = isolated();
        const float x = iso ? 0.0f : (v(cs80::P_macro1 + m) - 0.5f) * 2.0f;
        const bool lit = std::abs(x) > 0.01f;
        struct Row { const char* name; juce::String value; };
        std::vector<Row> rows;
        auto pct = [&](float k) { return signedString(x * k * 100.0f, 0, "%"); };
        switch (m)
        {
            case 0: rows = { { "BRILLIANCE", signedString(x * 0.75f, 2, " oct") } }; break;
            case 1: rows = { { "ENVELOPE TIMES", "x" + juce::String(std::pow(2.0f, -x * 1.1f), 2) },
                             { "LFO DEPTH", signedString(juce::jmax(0.0f, x) * 18.0f, 0, "%") } }; break;
            case 2: rows = { { "DETUNE II", signedString(x * 12.0f, 1, " ct") },
                             { "DISPERSION", pct(0.22f) }, { "CHORUS DEPTH", pct(0.25f) } }; break;
            default: rows = { { "RING AMOUNT", pct(0.22f) }, { "SUB > VCF", pct(0.20f) }, { "SUB > VCA", pct(0.12f) } }; break;
        }
        // Flat bipolar bar: centre is neutral.
        const Rect track(4.0f, 6.0f, designW - 8.0f, 9.0f);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillRoundedRectangle(track, 4.0f);
        const float mid = track.getCentreX();
        const float half = track.getWidth() * 0.5f - 2.0f;
        g.setColour(lit ? amber : ivoryDim.withAlpha(0.4f));
        g.fillRect(x >= 0.0f ? mid : mid + x * half, track.getY() + 1.5f, juce::jmax(1.5f, std::abs(x) * half), track.getHeight() - 3.0f);
        g.setColour(ivory.withAlpha(0.7f));
        g.fillRect(mid - 0.75f, track.getY() - 2.0f, 1.5f, track.getHeight() + 4.0f);
        for (int i = 0; i < 3; ++i)
        {
            const float y = 22.0f + (float) i * 25.0f;
            if (i >= (int) rows.size())
            {
                g.setColour(ivory.withAlpha(0.06f));
                g.fillRect(4.0f, y + 12.0f, designW - 8.0f, 1.0f);
                continue;
            }
            text(g, rows[(size_t) i].name, { 4.0f, y, 96.0f, 22.0f }, 10.0f, true, lit ? ivory : ivoryDim, Just::centredLeft, 0.04f);
            text(g, rows[(size_t) i].value, { 80.0f, y, 84.0f, 22.0f }, 11.5f, true, lit ? amberBright : ivoryDim.withAlpha(0.9f),
                 Just::centredRight, 0.02f);
            g.setColour(ivory.withAlpha(0.10f));
            g.fillRect(4.0f, y + 23.0f, designW - 8.0f, 1.0f);
        }
    }

    int m;
};

// ---- Channel mix readout, under the MIX I/II fader ------------------------------------------------
class MixReadout final : public LiveWidget
{
public:
    explicit MixReadout(Cs80AudioProcessor& p) : LiveWidget(p, 476.0f, 26.0f) {}
    void paintStatic(juce::Graphics&) override {}

private:
    void sample(Sig& s) const override { s[0] = v(cs80::P_mix); }

    void draw(juce::Graphics& g) override
    {
        const float mix = v(cs80::P_mix);
        text(g, "CHANNEL I  " + juce::String(juce::roundToInt((1.0f - mix) * 100.0f)) + "%", { 4.0f, 0.0f, 170.0f, designH }, 11.5f, true,
             ivory, Just::centredLeft, 0.04f);
        text(g, "I = 1 - MIX     II = MIX", { 150.0f, 0.0f, 176.0f, designH }, 10.0f, false, axisInk(), Just::centred);
        text(g, "CHANNEL II  " + juce::String(juce::roundToInt(mix * 100.0f)) + "%", { 302.0f, 0.0f, 170.0f, designH }, 11.5f, true, ivory,
             Just::centredRight, 0.04f);
    }
};

// ---- Output sample-peak meter. The editor owns the one telemetry consumer, so this reads the -----
// text of its read-only peak label instead of draining the processor's meter a second time.
class OutputMeter final : public LiveWidget
{
public:
    explicit OutputMeter(Cs80AudioProcessor& p) : LiveWidget(p, 628.0f, 104.0f) {}

    void paintStatic(juce::Graphics& g) override
    {
        glassWell(g, { 0.0f, 0.0f, designW, designH });
        const auto bar = barRect();
        for (int db = -60; db <= 0; db += 10)
        {
            const float x = bar.getX() + bar.getWidth() * (float) (db + 60) / 60.0f;
            vline(g, x, bar.getY() - 4.0f, bar.getBottom() + 4.0f, db == 0 ? 0.5f : 0.22f);
            axisText(g, db == 0 ? "0 dBFS" : juce::String(db), { x - 24.0f, bar.getBottom() + 5.0f, 48.0f, 13.0f });
        }
    }

private:
    static Rect barRect() { return { 22.0f, 40.0f, 584.0f, 26.0f }; }

    static juce::Label* findLabel(juce::Component* c)
    {
        if (c->getComponentID() == "output_sample_peak")
            return dynamic_cast<juce::Label*>(c);
        for (auto* child : c->getChildren())
            if (auto* r = findLabel(child))
                return r;
        return nullptr;
    }

    void sample(Sig& s) const override
    {
        if (meterLabel == nullptr)
            if (auto* top = getTopLevelComponent())
                meterLabel = findLabel(top);
        float db = -120.0f;
        bool over = false, bad = false;
        bool have = false;
        if (meterLabel != nullptr)
        {
            const auto t = meterLabel->getText();
            have = true;
            over = t.contains("OVER-RANGE");
            bad = t.contains("NON-FINITE");
            const auto first = t.upToFirstOccurrenceOf("\n", false, false);
            if (first.startsWith("SAMPLE PEAK") && ! first.contains("<="))
                db = first.fromFirstOccurrenceOf("SAMPLE PEAK", false, false).upToFirstOccurrenceOf("dBFS", false, false).trim().getFloatValue();
        }
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (db >= hold)
        {
            hold = db;
            holdAt = now;
        }
        else if (now - holdAt > 1.2)
            hold = juce::jmax(db, hold - 24.0f * (float) (now - holdAt - 1.2));
        if (now - holdAt > 1.2)
            holdAt = now - 1.2;
        s[0] = db; s[1] = over ? 1.0f : 0.0f; s[2] = bad ? 1.0f : 0.0f; s[3] = std::round(hold * 2.0f) * 0.5f; s[4] = have ? 1.0f : 0.0f;
    }

    void draw(juce::Graphics& g) override
    {
        Sig s {};
        sample(s);
        const float db = s[0];
        const bool over = s[1] > 0.5f, bad = s[2] > 0.5f, have = s[4] > 0.5f;
        const auto bar = barRect();
        auto xOf = [&](float d) { return bar.getX() + bar.getWidth() * juce::jlimit(0.0f, 1.0f, (d + 60.0f) / 60.0f); };
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.fillRoundedRectangle(bar, 3.0f);
        if (db > -60.0f)
        {
            const float x1 = xOf(db);
            g.setGradientFill(juce::ColourGradient(amber.darker(0.3f), bar.getX(), 0.0f, amberBright, xOf(-12.0f), 0.0f, false));
            g.fillRoundedRectangle(bar.getX(), bar.getY() + 2.0f, juce::jmin(x1, xOf(-3.0f)) - bar.getX(), bar.getHeight() - 4.0f, 2.0f);
            if (db > -3.0f)
            {
                g.setColour(juce::Colour(0xffff7166));
                g.fillRect(xOf(-3.0f), bar.getY() + 2.0f, x1 - xOf(-3.0f), bar.getHeight() - 4.0f);
            }
        }
        if (s[3] > -60.0f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.fillRect(xOf(s[3]) - 1.0f, bar.getY() - 2.0f, 2.0f, bar.getHeight() + 4.0f);
        }
        text(g, "OUTPUT SAMPLE PEAK", { 14.0f, 4.0f, 240.0f, 16.0f }, 10.5f, true, axisInk(), Just::centredLeft, 0.08f);
        text(g, ! have ? "NO METER" : (db <= -119.5f ? "<= -120 dBFS" : juce::String(db, 1) + " dBFS"), { 330.0f, 2.0f, 290.0f, 30.0f }, 20.0f, true,
             bad ? juce::Colour(0xffff7166) : (over ? amberBright : ivory), Just::centredRight, 0.02f);
        text(g, bad ? "NON-FINITE" : (over ? "OVER-RANGE" : "READ-ONLY  -  MONO SAMPLE PEAK, NOT TRUE PEAK"), { 14.0f, 84.0f, 600.0f, 16.0f }, 10.0f,
             true, bad ? juce::Colour(0xffff7166) : (over ? amberBright : axisInk()), Just::centredLeft, 0.04f);
    }

    mutable juce::Component::SafePointer<juce::Label> meterLabel;
    mutable float hold = -120.0f;
    mutable double holdAt = 0.0;
};

// ---- Arpeggiator clock ----------------------------------------------------------------------------
class ArpClock final : public LiveWidget
{
public:
    explicit ArpClock(Cs80AudioProcessor& p) : LiveWidget(p, 124.0f, 92.0f) {}

private:
    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_arpRate); s[1] = v(cs80::P_arpMode); s[2] = v(cs80::P_arpOn); s[3] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const float rate = juce::jmax(0.25f, v(cs80::P_arpRate));
        const bool random = juce::roundToInt(v(cs80::P_arpMode)) == 3;
        const bool on = v(cs80::P_arpOn) >= 0.5f && ! isolated();
        auto row = [&](float y, const juce::String& k, const juce::String& val)
        {
            text(g, k, { 10.0f, y, 50.0f, 16.0f }, 10.0f, true, ivoryDim, Just::centredLeft, 0.1f);
            text(g, val, { 44.0f, y, 74.0f, 16.0f }, 11.5f, true, on ? amberBright : ivory.withAlpha(0.75f), Just::centredRight);
        };
        row(8.0f, "STEP", periodString(1.0f / rate));
        row(36.0f, "CLOCK", juce::String(rate, rate < 10.0f ? 2 : 1) + "/s");
        row(64.0f, "GATE", juce::String(random ? 45 : 58) + "%");
    }
};

// ---- Arpeggio pattern: pitch gutter, step numbers and gate bars ------------------------------------
class ArpPattern final : public LiveWidget
{
public:
    explicit ArpPattern(Cs80AudioProcessor& p) : LiveWidget(p, 676.0f, 180.0f) {}

private:
    static Rect plotRect() { return { 58.0f, 24.0f, 604.0f, 130.0f }; }

    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_arpMode); s[1] = v(cs80::P_arpOctaves); s[2] = v(cs80::P_arpOn); s[3] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const int mode = juce::jlimit(0, 4, juce::roundToInt(v(cs80::P_arpMode)));
        const int octaves = juce::jlimit(1, 4, juce::roundToInt(v(cs80::P_arpOctaves)));
        const bool on = v(cs80::P_arpOn) >= 0.5f && ! isolated();
        // The same ordering rules as Engine::rebuildArpSequence, on a demonstration chord pressed E, G, C.
        std::vector<int> base { 64, 67, 60 };
        if (mode != 4)
            std::sort(base.begin(), base.end());
        std::vector<int> seq;
        for (int o = 0; o < octaves; ++o)
            for (int n : base)
                seq.push_back(n + 12 * o);
        if (mode == 1)
            std::reverse(seq.begin(), seq.end());
        else if (mode == 2 && seq.size() > 2)
        {
            const int up = (int) seq.size();
            for (int i = up - 2; i > 0; --i)
                seq.push_back(seq[(size_t) i]);
        }
        const auto plot = plotRect();
        const int lowest = 60, highest = 67 + 12 * (octaves - 1);
        const float rowH = plot.getHeight() / (float) (highest - lowest + 1);
        const float stepW = plot.getWidth() / (float) seq.size();
        const float lamp = on ? 1.0f : 0.45f;

        for (int n = lowest; n <= highest; ++n)
        {
            const float y = plot.getBottom() - (float) (n - lowest + 1) * rowH;
            if (n % 12 == 0 || n % 12 == 4 || n % 12 == 7)
            {
                g.setColour(ivory.withAlpha(0.06f));
                g.fillRect(plot.getX(), y, plot.getWidth(), rowH);
            }
        }
        for (size_t i = 0; i <= seq.size(); ++i)
            vline(g, plot.getX() + stepW * (float) i, plot.getY(), plot.getBottom(), i == 0 ? 0.30f : 0.12f);
        hline(g, plot.getX(), plot.getRight(), plot.getBottom(), 0.34f);
        vline(g, plot.getX(), plot.getY(), plot.getBottom(), 0.34f);

        // Pitch gutter: only the notes the pattern uses, so the labels never crowd.
        std::vector<int> used(base.begin(), base.end());
        for (int o = 1; o < octaves; ++o)
            for (int n : base)
                used.push_back(n + 12 * o);
        for (int n : used)
        {
            const float y = plot.getBottom() - ((float) (n - lowest) + 0.5f) * rowH;
            axisText(g, noteName(n), { 8.0f, y - 6.5f, 42.0f, 13.0f }, Just::centredRight);
            g.setColour(axisInk().withAlpha(0.5f));
            g.fillRect(plot.getX() - 4.0f, y - 0.5f, 4.0f, 1.0f);
        }

        const bool random = mode == 3;
        const float gate = random ? 0.45f : 0.58f;
        for (size_t i = 0; i < seq.size(); ++i)
        {
            const int note = seq[i];
            const float y = plot.getBottom() - (float) (note - lowest + 1) * rowH;
            const Rect bar(plot.getX() + stepW * (float) i + stepW * 0.06f, y + 0.5f, stepW * gate, juce::jmax(3.0f, rowH - 1.0f));
            if (random)
            {
                g.setColour(on ? amber.withAlpha(0.28f) : ivoryDim.withAlpha(0.2f));
                g.fillRoundedRectangle(bar.withWidth(stepW * 0.86f), 1.5f);
            }
            else
            {
                if (on)
                {
                    g.setColour(amberBright.withAlpha(0.22f));
                    g.fillRoundedRectangle(bar.expanded(1.8f), 2.5f);
                }
                g.setGradientFill(juce::ColourGradient((on ? amberBright : ivory.withAlpha(0.7f)).withMultipliedAlpha(lamp), bar.getX(), bar.getY(),
                                                       (on ? amber.darker(0.2f) : ivoryDim.withAlpha(0.6f)).withMultipliedAlpha(lamp), bar.getX(),
                                                       bar.getBottom(), false));
                g.fillRoundedRectangle(bar, 1.8f);
            }
            // Step number: one per step, centred under its gate bar.
            if (stepW > 15.0f)
                axisText(g, juce::String((int) i + 1), { plot.getX() + stepW * (float) i, plot.getBottom() + 3.0f, stepW, 13.0f });
        }
        if (random)
            text(g, "RANDOM STEP FROM THE HELD NOTES, ALL OCTAVES", plot, 12.0f, true, amberBright.withAlpha(on ? 1.0f : 0.5f), Just::centred, 0.1f);
        text(g, "DEMO CHORD C4 E4 G4" + juce::String(mode == 4 ? "  PRESSED E4 G4 C4" : "") + "  -  "
                    + juce::String((int) seq.size()) + " STEPS PER CYCLE",
             { 12.0f, 3.0f, 440.0f, 16.0f }, 10.5f, true, amber.withMultipliedAlpha(lamp), Just::centredLeft, 0.06f);
        text(g, kArpModes[mode] + "  -  " + juce::String(octaves) + (octaves == 1 ? " OCTAVE" : " OCTAVES"),
             { 440.0f, 3.0f, 224.0f, 16.0f }, 10.5f, true, ivory.withMultipliedAlpha(lamp), Just::centredRight, 0.06f);
        text(g, "STEP", { 4.0f, plot.getBottom() + 3.0f, 50.0f, 13.0f }, 10.0f, true, axisInk(), Just::centredRight);
        if (isolated())
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }
};

// ---- Glide curve ------------------------------------------------------------------------------------
class GlideCurve final : public LiveWidget
{
public:
    explicit GlideCurve(Cs80AudioProcessor& p) : LiveWidget(p, 688.0f, 180.0f) {}

    void paintStatic(juce::Graphics& g) override
    {
        glassWell(g, { 0.0f, 0.0f, designW, designH });
        const auto plot = plotRect();
        for (int i = 0; i <= 4; ++i)
        {
            const float y = plot.getBottom() - plot.getHeight() * (float) i / 4.0f;
            hline(g, plot.getX(), plot.getRight(), y, i == 0 ? 0.34f : 0.15f);
            axisText(g, juce::String(i * 3) + (i == 4 ? " st" : ""), { 0.0f, y - 6.5f, 48.0f, 13.0f }, Just::centredRight);
        }
        vline(g, plot.getX(), plot.getY(), plot.getBottom(), 0.34f);
    }

private:
    static Rect plotRect() { return { 56.0f, 24.0f, 612.0f, 110.0f }; }

    void sample(Sig& s) const override { s[0] = v(cs80::P_portamento); s[1] = v(cs80::P_glideMode); }

    void draw(juce::Graphics& g) override
    {
        const float tau = v(cs80::P_portamento);
        const int mode = juce::jlimit(0, 2, juce::roundToInt(v(cs80::P_glideMode)));
        const auto plot = plotRect();
        const bool gliding = tau > 0.0001f && mode != 0;
        const float span = tau > 0.0001f ? tau * 5.0f : 1.0f;
        auto xOfT = [&](float t01) { return plot.getX() + plot.getWidth() * t01; };

        // Time axis in time-constant steps: 1 tau reaches 63%, 3 tau 95%.
        for (int i = 0; i <= 5; ++i)
        {
            vline(g, xOfT((float) i / 5.0f), plot.getY(), plot.getBottom(), i == 0 ? 0.34f : 0.13f);
            if (tau > 0.0001f)
            {
                axisText(g, i == 0 ? "0" : periodString(tau * (float) i), { xOfT((float) i / 5.0f) - 32.0f, plot.getBottom() + 3.0f, 64.0f, 13.0f });
                if (i > 0)
                    axisText(g, juce::String(i) + juce::String(juce::CharPointer_UTF8("\xcf\x84")),
                             { xOfT((float) i / 5.0f) - 20.0f, plot.getBottom() + 17.0f, 40.0f, 13.0f });
            }
        }
        // 63 / 86 / 95 percent reference lines.
        for (float level : { 0.632f, 0.865f, 0.95f })
            hline(g, plot.getX(), plot.getRight(), plot.getBottom() - plot.getHeight() * level, 0.07f, amber);

        auto expCurve = [&](bool stepped)
        {
            juce::Path path;
            constexpr int count = 400;
            for (int i = 0; i <= count; ++i)
            {
                const float t01 = (float) i / (float) count;
                float semis = tau > 0.0001f ? 12.0f * (1.0f - std::exp(-5.0f * t01)) : 12.0f;
                if (stepped)
                    semis = std::round(semis);
                const float x = xOfT(t01);
                const float y = plot.getBottom() - plot.getHeight() * semis / 12.0f;
                if (i == 0) path.startNewSubPath(x, tau > 0.0001f ? y : plot.getBottom());
                if (tau <= 0.0001f && i == 0)
                    path.lineTo(x, y);
                else if (i > 0)
                    path.lineTo(x, y);
            }
            return path;
        };
        // Dashed reference: the smooth exponential (glissando steps lie on it) or, with glide off, the jump.
        {
            juce::Path dashed;
            const float dash[] = { 5.0f, 4.0f };
            juce::PathStrokeType(1.4f).createDashedStroke(dashed, expCurve(false), dash, 2);
            g.setColour(ivory.withAlpha(gliding && mode == 2 ? 0.6f : 0.0f));
            g.fillPath(dashed);
        }
        const auto path = expCurve(mode == 2);
        g.setColour(amberBright.withAlpha(gliding ? 0.28f : 0.12f));
        g.strokePath(path, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(gliding ? amberBright : ivoryDim);
        g.strokePath(path, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (tau > 0.0001f)
        {
            const float x63 = xOfT(1.0f / 5.0f);
            g.setColour(ivory.withAlpha(0.4f));
            const float dash[] = { 3.0f, 3.0f };
            g.drawDashedLine(juce::Line<float>(x63, plot.getBottom(), x63, plot.getBottom() - plot.getHeight() * 0.632f), dash, 2, 1.0f);
        }
        text(g, "ONE OCTAVE UP FROM THE PREVIOUS NOTE", { 12.0f, 3.0f, 360.0f, 16.0f }, 10.5f, true, axisInk(), Just::centredLeft, 0.05f);
        text(g, mode == 0 ? "GLIDE OFF  -  PITCH JUMPS" : (tau <= 0.0001f ? "GLIDE TIME 0  -  NO GLIDE" : (mode == 2 ? "GLISSANDO  -  SEMITONE STEPS, DASHED = SMOOTH" : "PORTAMENTO  -  EXPONENTIAL, TIME CONSTANT = GLIDE TIME")),
             { 300.0f, 3.0f, 376.0f, 16.0f }, 10.5f, true, gliding ? amber : axisInk(), Just::centredRight, 0.05f);
        if (tau <= 0.0001f)
            axisText(g, "GLIDE TIME IS 0  -  THE PITCH ARRIVES AT ONCE", { plot.getX(), plot.getBottom() + 4.0f, plot.getWidth(), 14.0f });
        else
            axisText(g, "TIME", { 4.0f, plot.getBottom() + 17.0f, 46.0f, 13.0f }, Just::centredRight);
    }
};

// ---- What each render policy changes: a table with the effective column lit ---------------------------
class PolicyTable final : public LiveWidget
{
public:
    explicit PolicyTable(Cs80AudioProcessor& p) : LiveWidget(p, 1440.0f, 220.0f) {}
    void paintStatic(juce::Graphics&) override {}

private:
    static constexpr float labelW = 384.0f, rowH = 22.0f;

    void sample(Sig& s) const override
    {
        s[0] = (float) policy(); s[1] = v(cs80::P_sourcePerformance); s[2] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const int pol = policy();
        const float colW = (designW - labelW) / 3.0f;
        struct Row { const char* label; const char* cell[3]; int kind[3]; };   // kind: 0 text, 1 live, 2 off, 3 masked
        static const Row rows[] = {
            { "VOICE MODEL", { "STUDY TABLE", "SOURCE MODELS", "SOURCE MODELS, NOMINAL" }, { 0, 0, 0 } },
            { "MIDI BEND, CC1 / 11 / 64 / 74", { "LIVE", "LIVE", "OFF" }, { 1, 1, 2 } },
            { "MPE  (WHEN SWITCHED ON)", { "LIVE", "LIVE", "OFF" }, { 1, 1, 2 } },
            { "MACROS, FX, PAN, BYPASS", { "LIVE", "LIVE", "MASKED" }, { 1, 1, 3 } },
            { "PRESSURE, RIBBON, ON-SCREEN PEDAL", { "LIVE", "LIVE", "LIVE" }, { 1, 1, 1 } },
            { "SUB OSCILLATOR", { "STORED MODE", "STORED MODE", "ORIGINAL, FORCED" }, { 0, 0, 0 } },
            { "CHORUS", { "GENERIC DELAY", "OE2 BBD", "OE2 BBD" }, { 0, 0, 0 } },
            { "OUTPUT", { "DC BLOCK + SOFT LIMIT", "RAW, KEEP HEADROOM", "RAW, KEEP HEADROOM" }, { 0, 0, 0 } },
            { "CALIBRATION", { "STUDY, NOT A MATCH", "NOT CALIBRATED", "NOT CALIBRATED" }, { 0, 0, 0 } },
        };
        static const char* const heads[3] = { "LEGACY", "SOURCE PERFORMANCE", "PARITY ISOLATION" };
        // Effective column band.
        const Rect band(labelW + colW * (float) pol, 0.0f, colW, designH);
        g.setGradientFill(juce::ColourGradient(amber.withAlpha(0.16f), band.getX(), 0.0f, amber.withAlpha(0.06f), band.getX(), designH, false));
        g.fillRoundedRectangle(band.reduced(2.0f, 0.0f), 4.0f);
        g.setColour(amber.withAlpha(0.55f));
        g.drawRoundedRectangle(band.reduced(2.0f, 0.5f), 4.0f, 1.0f);
        for (int c = 0; c < 3; ++c)
            text(g, heads[c], { labelW + colW * (float) c, 0.0f, colW, rowH }, 11.0f, true, c == pol ? amberBright : gold, Just::centred, 0.12f);
        hline(g, 0.0f, designW, rowH, 0.3f, gold);
        for (size_t r = 0; r < std::size(rows); ++r)
        {
            const float y = rowH + 2.0f + (float) r * rowH;
            if (r % 2 == 1)
            {
                g.setColour(juce::Colours::white.withAlpha(0.03f));
                g.fillRect(0.0f, y, designW, rowH);
            }
            text(g, rows[r].label, { 8.0f, y, labelW - 12.0f, rowH }, 11.0f, true, ivory, Just::centredLeft, 0.05f);
            for (int c = 0; c < 3; ++c)
            {
                const int kind = rows[r].kind[c];
                const juce::Colour ink = kind == 1 ? amberBright : (kind == 2 ? ivoryDim.withAlpha(0.8f) : (kind == 3 ? amber : (c == pol ? ivory : axisInk())));
                text(g, rows[r].cell[c], { labelW + colW * (float) c, y, colW, rowH }, 11.0f, kind != 0, ink, Just::centred, 0.04f);
            }
        }
    }
};

// ---- Footer policy badge: one lamp for the effective policy (pages 1 to 3) --------------------------------
class PolicyBadge final : public LiveWidget
{
public:
    explicit PolicyBadge(Cs80AudioProcessor& p) : LiveWidget(p, 360.0f, 22.0f) {}
    void paintStatic(juce::Graphics&) override {}

private:
    void sample(Sig& s) const override
    {
        s[0] = (float) policy(); s[1] = v(cs80::P_sourcePerformance); s[2] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const int pol = policy();
        static const char* const names[] = { "LEGACY", "SOURCE PERFORMANCE", "PARITY ISOLATION" };
        jewel(g, { 12.0f, designH * 0.5f }, 4.6f, amber, true);
        const bool masked = pol == 2 && v(cs80::P_sourcePerformance) >= 0.5f;
        text(g, "RENDER POLICY", { 26.0f, 0.0f, 110.0f, designH }, 10.0f, true, gold, Just::centredLeft, 0.14f);
        text(g, juce::String(names[pol]) + (masked ? "  (SOURCE MASKED)" : ""), { 140.0f, 0.0f, 220.0f, designH }, 10.5f, true, amberBright,
             Just::centredLeft, 0.08f);
    }
};

} // namespace

// ---------------------------------------------------------------------------
// Policy switch cell: one of three segments. The two stored requests are real parameter-bound
// buttons; LEGACY clears both.
// ---------------------------------------------------------------------------

class AdvPolicyCell final : public juce::Button
{
public:
    AdvPolicyCell(Cs80AudioProcessor& p, int idx, const juce::String& name, const juce::String& caption)
        : juce::Button(name), proc(p), index(idx), sub(caption)
    {
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setClickingTogglesState(idx != 0);
    }

    void refreshState()
    {
        const int now = effective();
        if (now != lastEffective)
        {
            lastEffective = now;
            repaint();
        }
    }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        constexpr float dw = 472.0f, dh = 64.0f;
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / dw, (float) getHeight() / dh));
        const int pol = effective();
        const bool lit = pol == index;
        const bool masked = index == 1 && pol == 2 && proc.getParamValue(cs80::P_sourcePerformance) >= 0.5f;
        const Rect r = Rect(2.0f, 2.0f, dw - 4.0f, dh - 6.0f).translated(0.0f, down ? 1.5f : 0.0f);
        if (lit)
        {
            g.setColour(amber.withAlpha(0.13f));
            g.fillRoundedRectangle(r.expanded(4.0f), 9.0f);
        }
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRoundedRectangle(r.translated(0.0f, 2.2f), 5.0f);
        g.setGradientFill(lit ? juce::ColourGradient(kGlow, r.getX(), r.getY(), kGlowDeep, r.getX(), r.getBottom(), false)
                              : juce::ColourGradient(juce::Colour(hovered ? 0xff3a3733 : 0xff2b2926), r.getX(), r.getY(),
                                                     juce::Colour(0xff151311), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(juce::Colours::white.withAlpha(lit ? 0.5f : 0.1f));
        g.drawLine(r.getX() + 5.0f, r.getY() + 1.1f, r.getRight() - 5.0f, r.getY() + 1.1f, 1.0f);
        g.setColour(lit ? juce::Colour(0xff6b4510) : juce::Colours::black.withAlpha(0.9f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
        const juce::Colour ink = lit ? juce::Colour(0xff2a1c07) : ivory;
        jewel(g, { r.getX() + 24.0f, r.getCentreY() }, 5.5f, lit ? juce::Colour(0xff7a1d0a) : amber, lit);
        if (masked)
        {
            g.setColour(amber);
            g.drawEllipse(r.getX() + 24.0f - 8.0f, r.getCentreY() - 8.0f, 16.0f, 16.0f, 1.3f);
        }
        ui::text(g, getButtonText() + (masked ? "  (MASKED)" : ""), { r.getX() + 46.0f, r.getY() + 8.0f, r.getWidth() - 56.0f, 20.0f }, 13.5f, true,
                 masked ? amberBright : ink, Just::centredLeft, 0.1f);
        ui::text(g, sub, { r.getX() + 46.0f, r.getY() + 29.0f, r.getWidth() - 56.0f, 18.0f }, 11.0f, false,
                 lit ? juce::Colour(0xff3c260a) : axisInk(), Just::centredLeft, 0.02f);
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawRoundedRectangle(r.expanded(1.5f), 6.0f, 1.6f);
        }
    }

private:
    int effective() const
    {
        return proc.getParamValue(cs80::P_parityMode) >= 0.5f ? 2 : (proc.getParamValue(cs80::P_sourcePerformance) >= 0.5f ? 1 : 0);
    }

    Cs80AudioProcessor& proc;
    int index;
    juce::String sub;
    int lastEffective = -1;
};

// ---------------------------------------------------------------------------
// Info button and the card it opens: explanatory prose lives here and in tooltips.
// ---------------------------------------------------------------------------

class AdvInfoButton final : public juce::Button
{
public:
    AdvInfoButton(const juce::String& subject, const juce::String& text) : juce::Button("Info"), body(text), heading(subject)
    {
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setTitle("About " + subject);
        setDescription(text);
        setTooltip(text);
    }

    juce::String body, heading;

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        const Rect r = getLocalBounds().toFloat().reduced(1.5f);
        const float d = juce::jmin(r.getWidth(), r.getHeight());
        const Rect c = Rect(0.0f, 0.0f, d, d).withCentre(r.getCentre()).translated(0.0f, down ? 1.0f : 0.0f);
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillEllipse(c.translated(0.0f, 1.5f));
        g.setGradientFill(juce::ColourGradient(juce::Colour(hovered ? 0xff5c5850 : 0xff46433d), c.getX(), c.getY(),
                                               juce::Colour(0xff1e1c19), c.getX(), c.getBottom(), false));
        g.fillEllipse(c);
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawEllipse(c.reduced(0.7f), 0.9f);
        g.setColour(juce::Colours::black.withAlpha(0.9f));
        g.drawEllipse(c, 1.0f);
        ui::text(g, "i", c.translated(0.0f, -0.5f), c.getHeight() * 0.72f, true, hovered ? amberBright : amber, Just::centred);
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawEllipse(c.expanded(2.0f), 1.5f);
        }
    }
};

class AdvInfoLayer final : public juce::Component
{
public:
    AdvInfoLayer()
    {
        setVisible(false);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setTitle("Information");
    }

    void open(const juce::String& title, const juce::String& body, juce::Component* from)
    {
        heading = title;
        message = body;
        opener = from;
        setVisible(true);
        toFront(false);
        grabKeyboardFocus();
        repaint();
    }

    void close()
    {
        if (! isVisible())
            return;
        setVisible(false);
        if (opener != nullptr)
            opener->grabKeyboardFocus();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
        {
            close();
            return true;
        }
        return true; // modal while open
    }

    void mouseDown(const juce::MouseEvent&) override { close(); }

    void paint(juce::Graphics& g) override
    {
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / kW, (float) getHeight() / kH));
        g.setColour(juce::Colours::black.withAlpha(0.58f));
        g.fillRect(0.0f, 0.0f, kW, kH);
        constexpr float cardW = 620.0f;
        juce::AttributedString a;
        a.setJustification(Just::topLeft);
        a.setWordWrap(juce::AttributedString::byWord);
        a.setLineSpacing(2.2f);
        a.append(message, fnt(13.0f), ivory);
        juce::TextLayout layout;
        layout.createLayout(a, cardW - 64.0f);
        const float cardH = layout.getHeight() + 112.0f;
        const Rect card = Rect(0.0f, 0.0f, cardW, cardH).withCentre({ kW * 0.5f, kH * 0.5f });
        softShadow(g, card, 8.0f, 0.0f, 8.0f, 18.0f, 0.8f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2b2926), card.getX(), card.getY(), juce::Colour(0xff161513), card.getX(), card.getBottom(), false));
        g.fillRoundedRectangle(card, 8.0f);
        g.setColour(gold.withAlpha(0.7f));
        g.drawRoundedRectangle(card, 8.0f, 1.4f);
        etchRect(g, card.reduced(6.0f), 5.0f, 0.4f);
        engrave(g, heading.toUpperCase(), { card.getX() + 32.0f, card.getY() + 18.0f, cardW - 64.0f, 24.0f }, 15.0f, true, ivory, Just::centredLeft, 0.12f);
        etchLine(g, card.getX() + 32.0f, card.getY() + 50.0f, card.getRight() - 32.0f, card.getY() + 50.0f, 0.6f);
        layout.draw(g, { card.getX() + 32.0f, card.getY() + 62.0f, cardW - 64.0f, layout.getHeight() + 4.0f });
        text(g, "Click or press Esc to close", { card.getX() + 32.0f, card.getBottom() - 30.0f, cardW - 64.0f, 18.0f }, 10.0f, false, axisInk(),
             Just::centredRight);
    }

private:
    juce::String heading, message;
    juce::Component::SafePointer<juce::Component> opener;
};

// ---------------------------------------------------------------------------
// Page: records static art (modules, labels, wells) in design space and hosts the live
// controls and widgets.
// ---------------------------------------------------------------------------

namespace
{
class ArtLayer final : public juce::Component
{
public:
    ArtLayer()
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(false);
        setBufferedToImage(true);
    }

    void paint(juce::Graphics& g) override
    {
        if (! draw)
            return;
        gArtScale = (float) getWidth() / kW;
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / kW, (float) getHeight() / kH));
        draw(g);
    }

    std::function<void(juce::Graphics&)> draw;
};

struct Placed
{
    juce::Component* component = nullptr;
    Rect bounds;
};
} // namespace

class AdvancedPage final : public juce::Component
{
public:
    explicit AdvancedPage(Cs80AudioProcessor& p) : proc(p)
    {
        setInterceptsMouseClicks(false, true);
        art = std::make_unique<ArtLayer>();
        art->draw = [this](juce::Graphics& g) { for (auto& op : artOps) op(g); };
        addAndMakeVisible(*art);
    }

    std::function<void(const juce::String&, const juce::String&, juce::Component*)> onInfo;

    void resized() override
    {
        art->setBounds(getLocalBounds());
        const float sx = (float) getWidth() / kW;
        const float sy = (float) getHeight() / kH;
        for (auto& item : placed)
            item.component->setBounds(juce::Rectangle<int>(
                juce::roundToInt(item.bounds.getX() * sx), juce::roundToInt(item.bounds.getY() * sy),
                juce::roundToInt(item.bounds.getWidth() * sx), juce::roundToInt(item.bounds.getHeight() * sy)));
    }

    void refresh()
    {
        for (auto& t : tickers)
            t();
        for (auto* w : widgets)
            w->refresh();
    }

    // ---- static art (call module() before the things inside it) ----------------------------
    // `info` becomes an info button on the module's title bracket: the explanatory prose lives
    // there and in its tooltip instead of on the panel face.
    void module(Rect r, const juce::String& title, bool primary = true,
                juce::Colour accent = juce::Colours::transparentBlack, const juce::String& info = {})
    {
        const bool hasInfo = info.isNotEmpty();
        artOps.push_back([r, title, primary, accent, hasInfo](juce::Graphics& g) { drawModule(g, r, title, primary, accent, hasInfo); });
        if (hasInfo)
            infoButton({ r.getRight() - 62.0f, r.getY() + 12.0f, 24.0f, 22.0f }, title, info);
    }

    AdvInfoButton* infoButton(Rect r, const juce::String& subject, const juce::String& body)
    {
        auto control = std::make_unique<AdvInfoButton>(subject, body);
        auto* raw = control.get();
        raw->setExplicitFocusOrder(1000 + (++infoCounter));
        raw->onClick = [this, raw] { if (onInfo) onInfo(raw->heading, raw->body, raw); };
        addAndMakeVisible(*raw);
        owned.push_back(std::move(control));
        placed.push_back({ raw, r });
        return raw;
    }

    void label(const juce::String& s, Rect r, float size = 10.5f, juce::Colour c = ivoryDim,
               Just j = Just::centredLeft, float kern = 0.09f)
    {
        artOps.push_back([=](juce::Graphics& g) { engrave(g, s, r, size, true, c, j, kern); });
    }

    // The one caption style for control headings: same area height as a knob title keeps baselines level.
    void caption(const juce::String& s, Rect area, Just j = Just::centred, float size = 11.5f)
    {
        artOps.push_back([=](juce::Graphics& g) { drawControlCaption(g, s, area, j, size); });
    }

    void bracket(float x0, float x1, float y, const juce::String& title, float size = 11.0f)
    {
        artOps.push_back([=](juce::Graphics& g) { bracketTitle(g, x0, x1, y, title, size); });
    }

    void custom(std::function<void(juce::Graphics&)> op) { artOps.push_back(std::move(op)); }

    void tick(std::function<void()> fn) { tickers.push_back(std::move(fn)); }

    // ---- live widgets ---------------------------------------------------------------------------
    template <class W, class... Args>
    W* widget(Rect r, Args&&... args)
    {
        auto w = std::make_unique<W>(proc, std::forward<Args>(args)...);
        W* raw = w.get();
        addAndMakeVisible(*raw);
        owned.push_back(std::move(w));
        widgets.push_back(raw);
        placed.push_back({ raw, r });
        artOps.push_back([raw, r](juce::Graphics& g)
        {
            g.saveState();
            g.addTransform(juce::AffineTransform::scale(r.getWidth() / raw->getDesignW(), r.getHeight() / raw->getDesignH())
                               .translated(r.getX(), r.getY()));
            raw->paintStatic(g);
            g.restoreState();
        });
        return raw;
    }

    // ---- parameter controls (same classes and parameters as before) --------------------------
    ParamKnob* knob(Rect r, int parameter, const juce::String& label = {})
    {
        auto control = std::make_unique<ParamKnob>(proc, parameter, label);
        auto* raw = control.get();
        raw->setExplicitFocusOrder(++focusCounter);
        addAndMakeVisible(*raw);
        owned.push_back(std::move(control));
        placed.push_back({ raw, r });
        return raw;
    }

    // The same parameter on a hardware-style vertical fader with a coloured cap.
    ParamKnob* fader(Rect r, int parameter, const juce::String& label, juce::Colour cap)
    {
        auto* raw = knob(r, parameter, label);
        raw->setHardwareSlider(cap);
        return raw;
    }

    // A compact horizontal cap fader (caption left, readout right, fader below).
    ParamKnob* hfader(Rect r, int parameter, const juce::String& label, juce::Colour cap)
    {
        auto* raw = knob(r, parameter, label);
        raw->setCapFader(cap, true);
        return raw;
    }

    LedButton* toggle(Rect r, int parameter, const juce::String& label)
    {
        auto control = std::make_unique<LedButton>(label);
        auto* raw = control.get();
        raw->setExplicitFocusOrder(++focusCounter);
        addAndMakeVisible(*raw);
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.getAPVTS(), Cs80AudioProcessor::parameterId(parameter), *raw));
        owned.push_back(std::move(control));
        placed.push_back({ raw, r });
        return raw;
    }

    juce::ComboBox* choice(Rect r, int parameter, const juce::StringArray& items)
    {
        auto control = std::make_unique<juce::ComboBox>();
        control->addItemList(items, 1);
        control->setJustificationType(juce::Justification::centredLeft);
        control->setWantsKeyboardFocus(false);
        control->setTitle(cs80::paramDef(parameter).name);
        control->setExplicitFocusOrder(++focusCounter);
        auto* raw = control.get();
        addAndMakeVisible(*raw);
        comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            proc.getAPVTS(), Cs80AudioProcessor::parameterId(parameter), *raw));
        owned.push_back(std::move(control));
        placed.push_back({ raw, r });
        artOps.push_back([r](juce::Graphics& g) { drawWell(g, r.expanded(2.5f), 4.0f); });
        return raw;
    }

    // One segment of the three-way render-policy switch.
    AdvPolicyCell* policyCell(Rect r, int index, const juce::String& name, const juce::String& caption)
    {
        auto control = std::make_unique<AdvPolicyCell>(proc, index, name, caption);
        auto* raw = control.get();
        raw->setExplicitFocusOrder(++focusCounter);
        addAndMakeVisible(*raw);
        if (index == 1 || index == 2)
            buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                proc.getAPVTS(), Cs80AudioProcessor::parameterId(index == 1 ? cs80::P_sourcePerformance : cs80::P_parityMode), *raw));
        owned.push_back(std::move(control));
        placed.push_back({ raw, r });
        tickers.push_back([raw] { raw->refreshState(); });
        return raw;
    }

    Cs80AudioProcessor& processor() noexcept { return proc; }

private:
    Cs80AudioProcessor& proc;
    std::unique_ptr<ArtLayer> art;
    std::vector<std::function<void(juce::Graphics&)>> artOps;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<Placed> placed;
    std::vector<LiveWidget*> widgets;
    std::vector<std::function<void()>> tickers;
    int focusCounter = 0;
    int infoCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedPage)
};

// ---------------------------------------------------------------------------
// Chassis: walnut cheeks, crinkle-finished charcoal body, header and footer rails. Cached once.
// ---------------------------------------------------------------------------

class AdvChassis final : public juce::Component
{
public:
    AdvChassis()
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(true);
        setBufferedToImage(true);
    }

    void paint(juce::Graphics& g) override
    {
        gArtScale = (float) getWidth() / kW;
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / kW, (float) getHeight() / kH));
        g.fillAll(juce::Colour(0xff0c0b0a));

        const Rect body(44.0f, 0.0f, kW - 88.0f, kH);
        fillPanelFace(g, body);
        fillRelief(g, crinkleTile(), body, 0.34f);
        fillRelief(g, grainTile(), body, 0.14f, 61.0f, 23.0f);

        // Header rail: a darker band that carries the title and the page tabs.
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.66f), 0.0f, 0.0f,
                                               juce::Colours::black.withAlpha(0.34f), 0.0f, 96.0f, false));
        g.fillRect(body.withHeight(96.0f));
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.42f), 0.0f, 96.0f,
                                               juce::Colours::transparentBlack, 0.0f, 114.0f, false));
        g.fillRect(body.getX(), 96.0f, body.getWidth(), 18.0f);
        etchLine(g, body.getX() + 14.0f, 96.0f, body.getRight() - 14.0f, 96.0f, 0.75f);

        // Footer rail.
        g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, 0.0f, 892.0f,
                                               juce::Colours::black.withAlpha(0.55f), 0.0f, kH, false));
        g.fillRect(body.getX(), 892.0f, body.getWidth(), kH - 892.0f);
        etchLine(g, body.getX() + 14.0f, 904.0f, body.getRight() - 14.0f, 904.0f, 0.6f);

        // Tab strip: a recessed black glass slot the illuminated tabs sit in, centred on the panel.
        glassWell(g, { 357.0f, 14.0f, 886.0f, 68.0f }, 7.0f);

        // Corner screws holding the body to the cheeks.
        for (float x : { 60.0f, kW - 60.0f })
            for (float y : { 14.0f, kH - 14.0f })
                drawScrew(g, { x, y }, 4.0f, x < 800.0f ? 0.7f : 1.9f);

        // Walnut cheeks: the shared figured-grain helper, a different figure on each side.
        drawWalnutGrain(g, { 0.0f, 0.0f, 44.0f, kH }, true, 11, true);
        drawWalnutGrain(g, { kW - 44.0f, 0.0f, 44.0f, kH }, true, 23, true);
        g.setColour(gold.withAlpha(0.5f));
        g.drawLine(44.5f, 0.0f, 44.5f, kH, 1.0f);
        g.drawLine(kW - 44.5f, 0.0f, kW - 44.5f, kH, 1.0f);
    }
};

// ---------------------------------------------------------------------------
// Page tabs: back-lit tone plates in the instrument's cap colours, numbered, with an underline on the
// active page.
// ---------------------------------------------------------------------------

class AdvPageTab final : public juce::Button
{
public:
    AdvPageTab(int index, const juce::String& name, const juce::String& caption)
        : juce::Button(name), page(index)
    {
        setClickingTogglesState(true);
        setRadioGroupId(0x80 + 7);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setExplicitFocusOrder(index + 1);
        setTitle(name);
        setDescription("Advanced page " + juce::String(index + 1) + " of 4: " + caption + ". Shortcut Control+" + juce::String(index + 1)
                       + ". Left and Right arrows move between pages.");
        setTooltip(name + "  (Ctrl+" + juce::String(index + 1) + ")  -  " + caption);
    }

    std::function<void(int)> onStep;

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::leftKey || key == juce::KeyPress::upKey) { if (onStep) onStep(-1); return true; }
        if (key == juce::KeyPress::rightKey || key == juce::KeyPress::downKey) { if (onStep) onStep(1); return true; }
        return juce::Button::keyPressed(key);
    }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        constexpr float dw = 212.0f, dh = 48.0f;
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / dw, (float) getHeight() / dh));
        const bool on = getToggleState();
        const auto colour = pageColour(page);
        const Rect face(2.0f, 2.0f, dw - 4.0f, dh - 10.0f);
        drawTonePlate(g, face, colour, on, down, hovered);

        const juce::Colour darkInk(0xff17120a);
        // Lit plates are pale: dark ink. Unlit green and pink are deep enough for cream; yellow and grey are not.
        const juce::Colour ink = on ? darkInk : (page >= 2 ? darkInk : ivory.withAlpha(hovered ? 1.0f : 0.92f));
        drawIcon(g, page, { face.getX() + 10.0f, face.getY() + 6.0f, 26.0f, face.getHeight() - 12.0f }, ink, on);
        // Number badge.
        const Pt bc(face.getX() + 52.0f, face.getCentreY());
        g.setColour(on ? ink.withAlpha(0.88f) : colour.withAlpha(0.9f));
        g.fillEllipse(bc.x - 9.0f, bc.y - 9.0f, 18.0f, 18.0f);
        ui::text(g, juce::String(page + 1), { bc.x - 9.0f, bc.y - 9.0f, 18.0f, 18.0f }, 11.0f, true,
                 on ? colour.brighter(0.5f) : inkFor(colour), Just::centred);
        // Name: 11.5 px bold on one line.
        g.setFont(fnt(11.5f, true, 0.05f));
        if (on || page < 2)
        {
            g.setColour(on ? juce::Colours::white.withAlpha(0.3f) : juce::Colours::black.withAlpha(0.7f));
            g.drawText(getButtonText(), Rect(face.getX() + 68.0f, face.getY() + 0.8f, face.getWidth() - 72.0f, face.getHeight()), Just::centredLeft, false);
        }
        g.setColour(ink);
        g.drawText(getButtonText(), Rect(face.getX() + 68.0f, face.getY(), face.getWidth() - 72.0f, face.getHeight()), Just::centredLeft, false);
        // Active page: a lit underline in the page colour.
        if (on)
        {
            const Rect bar(face.getX() + 6.0f, face.getBottom() + 3.0f, face.getWidth() - 12.0f, 3.0f);
            g.setColour(colour.withAlpha(0.3f));
            g.fillRoundedRectangle(bar.expanded(2.0f, 2.0f), 3.0f);
            g.setColour(colour.brighter(0.4f));
            g.fillRoundedRectangle(bar, 1.5f);
        }
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawRoundedRectangle(face.expanded(1.5f), 6.0f, 1.6f);
        }
    }

private:
    static void drawIcon(juce::Graphics& g, int which, Rect b, juce::Colour c, bool on)
    {
        g.setColour(c);
        const juce::PathStrokeType stroke(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::Path p;
        switch (which)
        {
            case 0: // sine LFO with its zero line
            {
                for (int i = 0; i <= 40; ++i)
                {
                    const float t = (float) i / 40.0f;
                    const float x = b.getX() + b.getWidth() * t;
                    const float y = b.getCentreY() - std::sin(t * juce::MathConstants<float>::twoPi * 1.5f) * b.getHeight() * 0.34f;
                    if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
                }
                g.strokePath(p, stroke);
                g.setColour(c.withAlpha(0.45f));
                g.drawLine(b.getX(), b.getCentreY(), b.getRight(), b.getCentreY(), 1.0f);
                break;
            }
            case 1: // two interlocked rings: ring modulation
            {
                const float rr = b.getHeight() * 0.33f;
                g.drawEllipse(b.getCentreX() - rr * 1.15f, b.getCentreY() - rr, rr * 2.0f, rr * 2.0f, 1.8f);
                g.drawEllipse(b.getCentreX() - rr * 0.85f, b.getCentreY() - rr, rr * 2.0f, rr * 2.0f, 1.8f);
                g.fillEllipse(b.getCentreX() - 1.5f, b.getCentreY() - 1.5f, 3.0f, 3.0f);
                break;
            }
            case 2: // three faders: macros and mix
            {
                for (int i = 0; i < 3; ++i)
                {
                    const float x = b.getX() + b.getWidth() * (0.2f + 0.3f * (float) i);
                    g.setColour(c.withAlpha(0.55f));
                    g.drawLine(x, b.getY() + 3.0f, x, b.getBottom() - 3.0f, 1.4f);
                    const float cy = b.getY() + b.getHeight() * (i == 0 ? 0.62f : (i == 1 ? 0.32f : 0.5f));
                    g.setColour(c);
                    g.fillRoundedRectangle(x - 4.0f, cy - 2.6f, 8.0f, 5.2f, 1.2f);
                }
                break;
            }
            default: // rising step bars: arpeggio
            {
                for (int i = 0; i < 4; ++i)
                {
                    const float h = b.getHeight() * (0.3f + 0.2f * (float) i);
                    g.setColour(on ? c : c.withAlpha(0.55f + 0.15f * (float) i));
                    g.fillRoundedRectangle(b.getX() + 2.0f + (float) i * 6.4f, b.getBottom() - 2.0f - h, 4.5f, h, 1.2f);
                }
                break;
            }
        }
    }

    int page;
};

// A raised close plate in the same plastic as the toolbar keys.
class AdvCloseButton final : public juce::Button
{
public:
    explicit AdvCloseButton(const juce::String& name) : juce::Button(name)
    {
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setTitle("Close advanced controls");
        setTooltip("Close advanced controls (Esc)");
    }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        constexpr float dw = 116.0f, dh = 38.0f;
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / dw, (float) getHeight() / dh));
        const Rect r = Rect(2.0f, 2.0f, dw - 4.0f, dh - 7.0f).translated(0.0f, down ? 1.5f : 0.0f);
        if (! down)
            softShadow(g, r, 3.0f, 0.0f, 2.2f, 2.5f, 0.65f);
        auto top = juce::Colour(0xff55514a), bottom = juce::Colour(0xff2a2723);
        if (down) std::swap(top, bottom);
        if (hovered) { top = top.brighter(0.14f); bottom = bottom.brighter(0.1f); }
        g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(down ? 0.05f : 0.2f));
        g.drawLine(r.getX() + 3.0f, r.getY() + 1.2f, r.getRight() - 3.0f, r.getY() + 1.2f, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
        const Pt c(r.getX() + 22.0f, r.getCentreY());
        g.setColour(amber);
        g.drawLine(c.x - 4.0f, c.y - 4.0f, c.x + 4.0f, c.y + 4.0f, 1.8f);
        g.drawLine(c.x - 4.0f, c.y + 4.0f, c.x + 4.0f, c.y - 4.0f, 1.8f);
        ui::text(g, getButtonText(), r.withTrimmedLeft(36.0f), 12.0f, true, ivory, Just::centredLeft, 0.12f);
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawRoundedRectangle(r.expanded(1.5f), 3.5f, 1.5f);
        }
    }
};

class AdvTitleBlock final : public juce::Component
{
public:
    AdvTitleBlock() { setInterceptsMouseClicks(false, false); setOpaque(false); }

    void setPage(int index)
    {
        page = index;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / 270.0f, (float) getHeight() / 78.0f));
        static const char* const titles[] = { "MODULATION", "SUB OSC / RING", "FX / MACROS", "ARP / PERFORMANCE" };
        static const char* const sub[] = {
            "Three LFOs, wheels, pitch range and tuning",
            "Sub osc, ring, chorus and analog voice",
            "Three insert effects, macros and output",
            "Arpeggiator, glide and render policy" };
        text(g, "ADVANCED CONTROL", { 0.0f, 2.0f, 270.0f, 13.0f }, 10.0f, true, gold, Just::centredLeft, 0.2f);
        engrave(g, titles[page], { 0.0f, 16.0f, 270.0f, 34.0f }, 22.0f, true, ivory, Just::centredLeft, 0.06f);
        text(g, sub[page], { 1.0f, 54.0f, 270.0f, 18.0f }, 11.0f, false, ivoryDim, Just::centredLeft, 0.01f);
    }

private:
    int page = 0;
};

class AdvFooterStatus final : public juce::Component
{
public:
    explicit AdvFooterStatus(Cs80AudioProcessor& p)
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(false);
        badge = std::make_unique<PolicyBadge>(p);
        addAndMakeVisible(*badge);
    }

    // The render-policy switch lives on page 4; the other pages carry its status here.
    void setPage(int index)
    {
        badge->setVisible(index != 3);
        repaint();
    }

    void refresh()
    {
        if (badge->isVisible())
            badge->refresh();
    }

    void paint(juce::Graphics& g) override
    {
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / 1472.0f, (float) getHeight() / 26.0f));
        text(g, "Ctrl+1 to 4 switch pages   -   Esc closes   -   double-click any knob to reset it", { 22.0f, 0.0f, 520.0f, 26.0f }, 10.0f, false,
             axisInk(), Just::centredLeft, 0.02f);
        text(g, "ALL PARAMETERS HOST-AUTOMATABLE", { 1130.0f, 0.0f, 320.0f, 26.0f }, 10.0f, true, amber.withAlpha(0.95f), Just::centredRight, 0.12f);
    }

    void resized() override
    {
        const float sx = (float) getWidth() / 1472.0f, sy = (float) getHeight() / 26.0f;
        badge->setBounds(juce::roundToInt(560.0f * sx), juce::roundToInt(2.0f * sy), juce::roundToInt(360.0f * sx), juce::roundToInt(22.0f * sy));
    }

private:
    std::unique_ptr<PolicyBadge> badge;
};

// ---------------------------------------------------------------------------
// Page layouts (design space 1600 x 936; content x 64..1536, y 108..896)
// ---------------------------------------------------------------------------

namespace
{
void buildModulation(AdvancedPage& page)
{
    const float xs[3] = { 64.0f, 560.0f, 1056.0f };
    const int rateParams[3] = { cs80::P_lfo1Rate, cs80::P_lfo2Rate, cs80::P_lfo3Rate };
    const int depthParams[3] = { cs80::P_lfo1Depth, cs80::P_lfo2Depth, cs80::P_lfo3Depth };
    const int destParams[3] = { cs80::P_lfo1Dest, cs80::P_lfo2Dest, cs80::P_lfo3Dest };
    const char* const flavour[3] = {
        "Use it for vibrato and tremolo on the whole instrument.",
        "Independent rate and destination. Use it for slow filter drift.",
        "A fast setting gives the CS-80 its characteristic shimmer." };
    for (int i = 0; i < 3; ++i)
    {
        const float x = xs[i], y = 108.0f;
        page.module({ x, y, 480.0f, 310.0f }, "LFO " + juce::String(i + 1), true, lfoColour(i),
                    juce::String("A free-running global sine LFO shared by all voices. ") + flavour[i]
                        + " Full-depth swing per destination: PITCH 2 st, VCF 1.6 oct, VCA 0.25 level, PAN 0.7, PWM 0.18 pulse width. "
                          "In parity isolation the modern LFO routing is masked.");
        page.widget<LfoScope>({ x + 20.0f, y + 40.0f, 440.0f, 140.0f }, i);
        page.knob({ x + 28.0f, y + 194.0f, 112.0f, 108.0f }, rateParams[i], "RATE");
        page.knob({ x + 148.0f, y + 194.0f, 112.0f, 108.0f }, depthParams[i], "DEPTH");
        page.caption("DESTINATION", { x + 288.0f, y + 194.0f, 176.0f, 17.0f });
        page.choice({ x + 288.0f, y + 216.0f, 176.0f, 32.0f }, destParams[i], kDestinations);
    }

    page.module({ 64.0f, 434.0f, 1472.0f, 266.0f }, "Modulation matrix", true, juce::Colours::transparentBlack,
                "Rows are the three LFOs, columns the engine's five destinations. Click a cell to route that LFO there; click the lit cell to "
                "switch the LFO off. Each LFO drives one destination. The lit cell shows the swing the engine applies: depth times the full-scale "
                "amount. The destination list follows the engine order: PITCH, VCF, VCA, PAN, PWM.");
    page.widget<LfoMatrix>({ 80.0f, 474.0f, 1440.0f, 208.0f });

    page.module({ 64.0f, 716.0f, 520.0f, 180.0f }, "Wheels / MPE", true, juce::Colours::transparentBlack,
                "The CS-80's pressure and velocity are separate performance dimensions. Initial touch sets the attack brightness; aftertouch bends the "
                "filter while held. The mod wheel adds sub-oscillator pitch and filter movement. MPE maps per-note bend and pressure when enabled; "
                "isolation disables MPE.");
    page.knob({ 88.0f, 756.0f, 112.0f, 112.0f }, cs80::P_modWheelSub, "WHEEL > SUB");
    page.knob({ 208.0f, 756.0f, 112.0f, 112.0f }, cs80::P_modWheelVcf, "WHEEL > VCF");
    page.widget<WheelRouting>({ 340.0f, 756.0f, 232.0f, 84.0f });
    page.toggle({ 340.0f, 846.0f, 232.0f, 30.0f }, cs80::P_mpeOn, "MPE");

    page.module({ 600.0f, 716.0f, 936.0f, 180.0f }, "Pitch / tuning", true, juce::Colours::transparentBlack,
                "PLAY RANGE: 61-key panel, MIDI notes 0-127. A4 sets the reference frequency for twelve-tone equal temperament; MIDI Tuning Standard "
                "and tuning-table import are not implemented. The pitch lever is spring-loaded; MPE member channels use their own bend range. The "
                "ribbon is a separate continuous controller and returns to centre.");
    page.knob({ 624.0f, 756.0f, 120.0f, 112.0f }, cs80::P_pitchBendRange, "WHEEL BEND");
    page.knob({ 752.0f, 756.0f, 120.0f, 112.0f }, cs80::P_mpeBendRange, "MPE BEND");
    page.knob({ 880.0f, 756.0f, 120.0f, 112.0f }, cs80::P_tuningRef, "A4 TUNING");
    page.knob({ 1008.0f, 756.0f, 120.0f, 112.0f }, cs80::P_masterTune, "MASTER TUNE");
    page.widget<TuningDisplay>({ 1152.0f, 758.0f, 372.0f, 52.0f });
    page.widget<BendRanges>({ 1152.0f, 816.0f, 372.0f, 56.0f });
}

void buildSubRing(AdvancedPage& page)
{
    // --- Sub oscillator -------------------------------------------------------------------------
    page.module({ 64.0f, 108.0f, 760.0f, 472.0f }, "Sub oscillator", true, pageColour(1),
                "PARITY ISOLATION forces ORIGINAL; the stored mode and the legacy settings are retained but inert. Noise and EXT ignore SPEED. "
                "EXT: optional mono input, disabled by default; host routing required. Six original functions; analog waveform, depth and touch laws "
                "remain provisional. Full depth moves VCO 1.5 st, VCF 1.5 oct, VCA 0.45 level.");
    auto* originalMode = page.toggle({ 88.0f, 148.0f, 214.0f, 28.0f }, cs80::P_subOriginalMode, "STORED ORIGINAL MODE");
    originalMode->setComponentID("sub_original_mode");
    originalMode->setTooltip("Stored preference: on selects original six functions and 0.7-60 Hz; off selects legacy. "
                             "PARITY ISOLATION forces original regardless of this retained setting; legacy controls are inert while isolated.");
    page.caption("ORIGINAL FUNCTION", { 88.0f, 188.0f, 190.0f, 17.0f });
    page.choice({ 88.0f, 212.0f, 190.0f, 32.0f }, cs80::P_subOriginalFunction, kOriginalSubWaves);
    page.knob({ 292.0f, 188.0f, 96.0f, 100.0f }, cs80::P_subOriginalSpeed, "ORIGINAL Hz");
    auto* extLevel = page.knob({ 396.0f, 188.0f, 96.0f, 100.0f }, cs80::P_subExtLevel, "EXT LEVEL");
    extLevel->getSlider().setDescription("External modulation input LEVEL. The optional mono bus is disabled by default and requires host routing. "
                                         "At maximum LEVEL, a peak 1 passband sine gives nominal inverted modulation peak 1, with 63.5 host samples "
                                         "of causal reconstruction delay. Software normalization, not physical voltage calibration.");
    page.custom([](juce::Graphics& g) { etchLine(g, 514.0f, 194.0f, 514.0f, 286.0f, 0.55f); });
    page.caption("LEGACY FUNCTION", { 534.0f, 188.0f, 160.0f, 17.0f });
    page.choice({ 534.0f, 212.0f, 160.0f, 32.0f }, cs80::P_subWave, kSubWaves);
    page.knob({ 704.0f, 188.0f, 92.0f, 100.0f }, cs80::P_subSpeed, "LEGACY Hz");
    page.widget<SubScope>({ 88.0f, 300.0f, 712.0f, 120.0f });
    page.bracket(88.0f, 420.0f, 442.0f, "DEPTH");
    page.bracket(460.0f, 792.0f, 442.0f, "PRESSURE");
    const char* const routeNames[6] = { "VCO", "VCF", "VCA", "AT SPEED", "AT VCO", "AT VCF" };
    const int routeParams[6] = { cs80::P_subVco, cs80::P_subVcf, cs80::P_subVca, cs80::P_subAtSpeed, cs80::P_subAtVco,
                                 cs80::P_subAtVcf };
    const float routeX[6] = { 88.0f, 202.0f, 316.0f, 460.0f, 574.0f, 688.0f };
    for (int i = 0; i < 6; ++i)
        page.knob({ routeX[i], 456.0f, 104.0f, 96.0f }, routeParams[i], routeNames[i]);

    // --- Ring modulator ---------------------------------------------------------------------------------
    page.module({ 840.0f, 108.0f, 696.0f, 472.0f }, "Ring modulator", true, pageColour(1),
                "A global ring modulator feeds the output alongside the voices. The envelope is retriggered by the first key of a phrase. DEPTH sweeps "
                "oscillator frequency; AMOUNT sets the dry/wet mix. The curve shows the carrier frequency in Hz; drag the A node to set ATTACK and "
                "DEPTH, the D node to set DECAY. The analog response remains provisional.");
    page.fader({ 864.0f, 150.0f, 52.0f, 400.0f }, cs80::P_ringSpeed, "SPEED", capCream);
    page.fader({ 924.0f, 150.0f, 52.0f, 400.0f }, cs80::P_ringDepth, "DEPTH", capGrey);
    page.fader({ 984.0f, 150.0f, 52.0f, 400.0f }, cs80::P_ringAttack, "ATTACK", capBlack);
    page.fader({ 1044.0f, 150.0f, 52.0f, 400.0f }, cs80::P_ringDecay, "DECAY", capBlack);
    page.fader({ 1104.0f, 150.0f, 52.0f, 400.0f }, cs80::P_ringAmount, "AMOUNT", capGrey);
    page.widget<RingCurve>({ 1180.0f, 150.0f, 332.0f, 400.0f });

    // --- Chorus / tremolo ---------------------------------------------------------------------------------
    page.module({ 64.0f, 596.0f, 760.0f, 300.0f }, "Chorus / tremolo", true, pageColour(1),
                "Legacy: provisional generic delay line. Source Performance and Isolation: OE2 BBD delay path (256 clock periods, printed filters); "
                "clock range, wet level and polarity are inferred, not validated against original hardware.");
    page.knob({ 88.0f, 640.0f, 136.0f, 140.0f }, cs80::P_chorusSpeed, "SPEED");
    page.knob({ 232.0f, 640.0f, 136.0f, 140.0f }, cs80::P_chorusDepth, "DEPTH");
    page.toggle({ 88.0f, 800.0f, 136.0f, 32.0f }, cs80::P_chorusOn, "CHORUS");
    page.toggle({ 232.0f, 800.0f, 136.0f, 32.0f }, cs80::P_chorusMode, "TREMOLO");
    page.widget<ChorusSweep>({ 384.0f, 636.0f, 416.0f, 236.0f });

    // --- Analog voice ----------------------------------------------------------------------------------------
    page.module({ 840.0f, 596.0f, 696.0f, 300.0f }, "Analog voice", true, pageColour(1),
                "Dispersion spreads the eight voice cards in tuning and pan; drift slowly moves each card; noise adds a provisional background floor. "
                "Set them to zero for a repeatable render; this is not hardware calibration. The scatter plots the sixteen ranks (eight cards, two "
                "ranks each); a ring marks a card that is sounding.");
    page.knob({ 864.0f, 640.0f, 104.0f, 108.0f }, cs80::P_voiceDispersion, "DISPERSION");
    page.knob({ 976.0f, 640.0f, 104.0f, 108.0f }, cs80::P_drift, "DRIFT");
    page.knob({ 1088.0f, 640.0f, 104.0f, 108.0f }, cs80::P_analogNoise, "NOISE");
    page.widget<VoiceLcd>({ 864.0f, 762.0f, 328.0f, 100.0f });
    page.widget<VoiceScatter>({ 1212.0f, 636.0f, 300.0f, 240.0f });
}

void buildFxMacros(AdvancedPage& page, Cs80AudioProcessor& proc)
{
    auto sel = std::make_shared<FxSel>();

    page.label("SIGNAL ORDER", { 64.0f, 104.0f, 260.0f, 16.0f }, 11.0f, gold, Just::centredLeft, 0.16f);
    auto* chain = page.widget<FxChain>({ 64.0f, 122.0f, 1472.0f, 50.0f }, sel);
    sel->peers.push_back(chain);

    const int typeParams[3] = { cs80::P_fx1Type, cs80::P_fx2Type, cs80::P_fx3Type };
    const int mixParams[3] = { cs80::P_fx1Mix, cs80::P_fx2Mix, cs80::P_fx3Mix };
    page.module({ 64.0f, 188.0f, 596.0f, 360.0f }, "Insert effects", true, pageColour(2),
                "Three insert slots, processed in series after the voice mixer and chorus. Tape Delay includes modulation and a filtered feedback "
                "path. Plate Reverb is a dense, bright algorithmic plate. Effects are bypassed at 0% mix and an empty slot passes the signal through; "
                "isolation masks them. Pick a slot here or in the signal-order strip to see its response at right.");
    for (int i = 0; i < 3; ++i)
    {
        const float y0 = 232.0f + 96.0f * (float) i;
        auto* row = page.widget<FxSlotRow>({ 76.0f, y0, 572.0f, 92.0f }, i, sel);
        sel->peers.push_back(row);
        page.choice({ 76.0f + 70.0f, y0 + 28.0f, 170.0f, 32.0f }, typeParams[i], kFxTypes);
        page.knob({ 76.0f + 258.0f, y0 + 2.0f, 96.0f, 88.0f }, mixParams[i], "MIX");
    }

    page.module({ 676.0f, 188.0f, 860.0f, 360.0f }, "Effect detail", true, pageColour(2));
    auto* detail = page.widget<FxDiagram>({ 696.0f, 232.0f, 820.0f, 300.0f }, sel);
    sel->peers.push_back(detail);

    // A slot that gains an effect becomes the selected one.
    auto last = std::make_shared<std::array<int, 3>>(std::array<int, 3> { -1, -1, -1 });
    page.tick([&proc, sel, last]
    {
        for (int i = 0; i < 3; ++i)
        {
            const int t = juce::roundToInt(proc.getParamValue(cs80::P_fx1Type + i * 2));
            if ((*last)[(size_t) i] >= 0 && t != (*last)[(size_t) i] && t != 0)
                sel->set(i);
            (*last)[(size_t) i] = t;
        }
    });

    page.module({ 64.0f, 564.0f, 780.0f, 332.0f }, "Performance macros", true, pageColour(2),
                "Macros are patch-level offsets. At centre they are neutral; turning them up pushes brilliance, envelope motion, detune/chorus width "
                "and the sub/ring depth respectively. They never replace the underlying panel settings. Each bar shows the macro's position "
                "(centre = neutral); the rows below show what it does to each destination.");
    const char* const macroNames[4] = { "BRILLIANCE", "MOTION", "WIDTH", "SPACE" };
    for (int i = 0; i < 4; ++i)
    {
        const float x = 80.0f + (float) i * 186.0f;
        page.knob({ x + 6.0f, 604.0f, 156.0f, 142.0f }, cs80::P_macro1 + i, macroNames[i]);
        page.widget<MacroBars>({ x, 758.0f, 168.0f, 98.0f }, i);
    }

    page.module({ 860.0f, 564.0f, 676.0f, 332.0f }, "Output stage", true, pageColour(2),
                "Stereo retains the left and right outputs. Mono averages them after effects and limiting; phase cancellation can change the result. "
                "MIX I/II crossfades the two channels: channel I plays at 1 - MIX, channel II at MIX. The engine runs at 2x. Legacy output is "
                "DC-blocked and soft-limited. Source modes keep raw source output, which can exceed digital full scale: leave monitoring headroom. "
                "The meter shows the editor's read-only sample peak.");
    page.knob({ 884.0f, 606.0f, 124.0f, 130.0f }, cs80::P_volume, "VOLUME");
    page.hfader({ 1036.0f, 606.0f, 476.0f, 58.0f }, cs80::P_mix, "MIX I/II", capGrey);
    page.widget<MixReadout>({ 1036.0f, 668.0f, 476.0f, 26.0f });
    page.toggle({ 1036.0f, 706.0f, 160.0f, 30.0f }, cs80::P_outputMode, "STEREO");
    page.widget<OutputMeter>({ 884.0f, 760.0f, 628.0f, 104.0f });
}

void buildArpPerformance(AdvancedPage& page, Cs80AudioProcessor& proc)
{
    page.module({ 64.0f, 108.0f, 724.0f, 352.0f }, "Arpeggiator", true, pageColour(3),
                "The arpeggiator holds the chord and retriggers the CS-80 voice envelopes at the selected clock. Up/Down, random and played-order "
                "modes cover the patterns used on the classic records. The pattern shows a demonstration chord, not a live readout of the notes "
                "you hold.");
    page.toggle({ 88.0f, 184.0f, 112.0f, 32.0f }, cs80::P_arpOn, "ARP");
    page.caption("MODE", { 216.0f, 144.0f, 176.0f, 17.0f });
    page.choice({ 216.0f, 168.0f, 176.0f, 32.0f }, cs80::P_arpMode, kArpModes);
    page.knob({ 408.0f, 144.0f, 112.0f, 112.0f }, cs80::P_arpRate, "RATE");
    page.knob({ 528.0f, 144.0f, 112.0f, 112.0f }, cs80::P_arpOctaves, "OCTAVES");
    page.widget<ArpClock>({ 652.0f, 152.0f, 124.0f, 92.0f });
    page.widget<ArpPattern>({ 88.0f, 268.0f, 676.0f, 180.0f });

    page.module({ 804.0f, 108.0f, 732.0f, 352.0f }, "Performance", true, pageColour(3),
                "Portamento glides; Glissando steps in semitones. Both sustain modes release notes. Sustain II also cuts previous release tails at "
                "a new key press. MIDI CC64 hold is a separate extension, disabled by parity isolation. RELEASE sets how long a released key rings; "
                "INIT BEND is the initial-touch pitch bend.");
    page.knob({ 828.0f, 144.0f, 112.0f, 112.0f }, cs80::P_portamento, "GLIDE");
    page.knob({ 948.0f, 144.0f, 112.0f, 112.0f }, cs80::P_keyRelease, "RELEASE");
    page.knob({ 1068.0f, 144.0f, 112.0f, 112.0f }, cs80::P_initBend, "INIT BEND");
    page.caption("GLIDE MODE", { 1204.0f, 144.0f, 200.0f, 17.0f });
    page.choice({ 1204.0f, 168.0f, 200.0f, 32.0f }, cs80::P_glideMode, kGlideModes);
    page.toggle({ 1204.0f, 214.0f, 200.0f, 32.0f }, cs80::P_sustainMode, "SUSTAIN II");
    page.widget<GlideCurve>({ 828.0f, 268.0f, 688.0f, 180.0f });

    // --- Render policy: one three-way switch, the table of what it changes, and About -------------------
    page.module({ 64.0f, 476.0f, 1472.0f, 420.0f }, "Render policy", true, pageColour(3),
                "NOT CALIBRATED - no hardware match established. SOURCE: current source models with MIDI, optional MPE and the modern overlays. "
                "ISOLATION wins when both are on: bend, CC1/11/64/74 and MPE are off and the modern effects are masked; pressure, ribbon and the "
                "on-screen pedal stay active. An effective mode change stops notes; keep headroom. The stored source request is retained but masked "
                "while isolation is on. Library programs reset the source request off and return both rank selectors to PANEL.");
    auto* legacy = page.policyCell({ 80.0f, 516.0f, 472.0f, 64.0f }, 0, "LEGACY", "Study voicing. MIDI and modern overlays live.");
    legacy->setComponentID("render_policy_legacy");
    legacy->setTitle("Legacy render policy");
    legacy->setTooltip("Legacy: the older voicing table with ordinary MIDI and the modern overlays. Selecting it clears both stored requests, "
                       "SOURCE PERFORMANCE and PARITY ISOLATION.");
    legacy->setDescription(legacy->getTooltip());
    legacy->onClick = [&proc]
    {
        setParameterOnce(proc, cs80::P_sourcePerformance, 0.0f);
        setParameterOnce(proc, cs80::P_parityMode, 0.0f);
    };

    auto* sourcePerformance = page.policyCell({ 564.0f, 516.0f, 472.0f, 64.0f }, 1, "SOURCE PERFORMANCE", "Source models. MIDI and modern overlays live.");
    sourcePerformance->setComponentID("source_performance");
    sourcePerformance->setTitle("Source performance (uncalibrated)");
    sourcePerformance->setTooltip("Opt-in source-performance policy: current source models with ordinary MIDI pitch bend, CC1/11/64/74, optional MPE and "
                                  "modern software overlays (macros, FX, pan and bypass). Not calibrated; no hardware match established. Keep output "
                                  "headroom. An effective mode change stops notes and clears controller/effect history. PARITY ISOLATION wins if both "
                                  "switches are ON; changing this source request while isolation stays ON is masked and does not stop notes. The source "
                                  "request is saved with sound presets; library programs reset it OFF and return both selectors to PANEL. Selected tones "
                                  "and memories still own only the 26 programmer controls; footage and global controls remain live.");
    sourcePerformance->setDescription(sourcePerformance->getTooltip());

    auto* parity = page.policyCell({ 1048.0f, 516.0f, 472.0f, 64.0f }, 2, "PARITY ISOLATION", "Nominal source models. MIDI and overlays masked.");
    parity->setComponentID("parity_mode");
    parity->setTooltip("Comparison fixture, not calibrated. No hardware match established. Enable before playing: toggling stops notes and clears "
                       "controller/effect history. MIDI pitch bend, CC1/11/64/74 and MPE are ignored; pressure, ribbon and the on-screen pedal "
                       "remain active. Hardware factory tabs use nominal source models; library programs still return to PANEL. Modern settings are "
                       "retained but inert. Isolation takes priority over SOURCE PERFORMANCE; its stored source request is masked, not cleared. "
                       "Output may exceed digital full scale.");
    parity->setDescription(parity->getTooltip());

    page.widget<PolicyTable>({ 80.0f, 596.0f, 1440.0f, 220.0f });

    // About: one slim strip; the long text is behind its info button.
    page.custom([](juce::Graphics& g)
    {
        const Rect strip(80.0f, 832.0f, 1440.0f, 48.0f);
        glassWell(g, strip, 5.0f);
        etchRect(g, strip.reduced(4.0f), 3.0f, 0.4f);
        engrave(g, "CS-80 REFERENCE", { 100.0f, 836.0f, 240.0f, 40.0f }, 16.0f, true, ivory, Just::centredLeft, 0.1f);
        text(g, "ENGINE  2x oversampled, 8 voices, 16 ranks", { 380.0f, 836.0f, 420.0f, 40.0f }, 11.0f, true, amber, Just::centredLeft, 0.03f);
        text(g, "FORMAT  VST3, AU (macOS), standalone", { 800.0f, 836.0f, 340.0f, 40.0f }, 11.0f, true, amber, Just::centredLeft, 0.03f);
        text(g, "BUILD  Reference Instruments - 1977-CS80", { 1130.0f, 836.0f, 330.0f, 40.0f }, 11.0f, true, amber, Just::centredLeft, 0.03f);
    });
    page.infoButton({ 1478.0f, 843.0f, 26.0f, 26.0f }, "About this instrument",
                    "A circuit-informed recreation of the Yamaha CS-80 (1977): eight voices, each with two independent ranks, for sixteen "
                    "oscillator/filter/VCA chains. The signal path follows the original's high-pass into low-pass topology, linear VCA and global "
                    "sub-oscillator.\n\nThe primary panel exposes the controls a player touches; these pages expose the modern layer (LFOs, MPE, "
                    "effects, dispersion) without cluttering the instrument. All controls are host-automatable and saved in plugin state, except "
                    "transient performance gestures. The library has 22 legacy tone studies and 200 performance patches; loading one returns both "
                    "ranks to PANEL and resets SOURCE PERFORMANCE off. Current source models are available with SOURCE PERFORMANCE or comparison "
                    "isolation.\n\nTouch the ribbon above the keyboard to bend pitch. Drag the pitch lever (PITCH) or modulation wheel (MOD) at the "
                    "left of the keyboard. Double-click any knob to reset it.\n\nNOT CALIBRATED: no hardware match is established for the source "
                    "models, the BBD chorus or the analog laws.");
}
} // namespace

// ---------------------------------------------------------------------------
// The panel
// ---------------------------------------------------------------------------

AdvancedPanel::AdvancedPanel(Cs80AudioProcessor& p)
    : processor(p)
{
    setVisible(false);

    chassis = std::make_unique<AdvChassis>();
    addAndMakeVisible(*chassis);

    const char* const names[] = { "MODULATION", "SUB OSC / RING", "FX / MACROS", "ARP / PERFORMANCE" };
    const char* const captions[] = { "LFOs, wheels, tuning", "sub oscillator, ring, chorus, voice",
                                     "insert effects, macros, output", "arpeggiator, glide, render policy" };
    for (int i = 0; i < 4; ++i)
    {
        auto tab = std::make_unique<AdvPageTab>(i, names[i], captions[i]);
        tab->onClick = [this, i] { showPage(i); };
        tab->onStep = [this](int direction) { stepPage(direction); };
        addAndMakeVisible(*tab);
        tabs[(size_t) i] = std::move(tab);
    }

    for (int i = 0; i < 4; ++i)
    {
        pages[(size_t) i] = std::make_unique<AdvancedPage>(processor);
        addAndMakeVisible(*pages[(size_t) i]);
    }
    buildModulation(*pages[0]);
    buildSubRing(*pages[1]);
    buildFxMacros(*pages[2], processor);
    buildArpPerformance(*pages[3], processor);

    titleBlock = std::make_unique<AdvTitleBlock>();
    addAndMakeVisible(*titleBlock);
    footer = std::make_unique<AdvFooterStatus>(processor);
    addAndMakeVisible(*footer);

    closeButton = std::make_unique<AdvCloseButton>("CLOSE");
    closeButton->onClick = [this]
    {
        if (onClose)
            onClose();
    };
    addAndMakeVisible(*closeButton);

    info = std::make_unique<AdvInfoLayer>();
    addChildComponent(*info);
    for (auto& page : pages)
        page->onInfo = [this](const juce::String& title, const juce::String& body, juce::Component* from)
        {
            info->open(title, body, from);
        };

    showPage(0);
}

AdvancedPanel::~AdvancedPanel()
{
    stopTimer();
}

void AdvancedPanel::showPage(int index)
{
    if (info != nullptr)
        info->close();
    activePage = juce::jlimit(0, 3, index);
    for (int i = 0; i < 4; ++i)
    {
        pages[(size_t) i]->setVisible(i == activePage);
        tabs[(size_t) i]->setToggleState(i == activePage, juce::dontSendNotification);
    }
    titleBlock->setPage(activePage);
    footer->setPage(activePage);
    pages[(size_t) activePage]->refresh();
    repaint();
}

void AdvancedPanel::stepPage(int direction)
{
    const int next = (activePage + direction + 4) % 4;
    showPage(next);
    tabs[(size_t) next]->grabKeyboardFocus();
}

bool AdvancedPanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && info->isVisible())
    {
        info->close();
        return true;
    }
    const int code = key.getKeyCode();
    if (key.getModifiers().isCommandDown() && code >= '1' && code <= '4')
    {
        showPage(code - '1');
        return true;
    }
    return false;
}

void AdvancedPanel::visibilityChanged()
{
    if (isVisible())
    {
        pages[(size_t) activePage]->refresh();
        startTimerHz(15);
    }
    else
    {
        if (info != nullptr)
            info->close();
        stopTimer();
    }
}

void AdvancedPanel::timerCallback()
{
    pages[(size_t) activePage]->refresh();
    footer->refresh();
}

void AdvancedPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0c0b0a));
}

void AdvancedPanel::resized()
{
    const float sx = (float) getWidth() / kW;
    const float sy = (float) getHeight() / kH;
    auto r = [sx, sy](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                                    juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };
    chassis->setBounds(getLocalBounds());
    for (int i = 0; i < 4; ++i)
    {
        tabs[(size_t) i]->setBounds(r(367.0f + (float) i * 218.0f, 22.0f, 212.0f, 48.0f));
        pages[(size_t) i]->setBounds(getLocalBounds());
    }
    titleBlock->setBounds(r(80.0f, 14.0f, 270.0f, 78.0f));
    footer->setBounds(r(64.0f, 908.0f, 1472.0f, 26.0f));
    closeButton->setBounds(r(1420.0f, 26.0f, 116.0f, 38.0f));
    info->setBounds(getLocalBounds());
}

} // namespace cs80::ui
