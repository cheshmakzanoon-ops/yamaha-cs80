#include "AdvancedPanel.h"

#include "ArtCommon.h"
#include "EvidenceRegister.h"
#include "HardwareKeyboard.h"
#include "PluginProcessor.h"
#include "core/Dsp.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <functional>
#include <map>
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
// Quiet text on the satin panel face (lighter than the black glass): half way from the dim ink to ivory, so it
// still clears 4.5:1 where axisInk does not.
juce::Colour panelInk() { return ivoryDim.interpolatedWith(ivory, 0.5f); }
// The three LFOs use cool scope colours so they never read as one of the page cap colours.
juce::Colour lfoColour(int i)
{
    return i == 0 ? juce::Colour(0xff46c8dc) : (i == 1 ? juce::Colour(0xff8aa4f7) : juce::Colour(0xffc994f2));
}
// The shortcut modifier as the platform names it.
juce::String shortcutName()
{
#if JUCE_MAC
    return "Cmd";
#else
    return "Ctrl";
#endif
}
juce::Colour pageColour(int i)
{
    return i == 0 ? capGreen : (i == 1 ? capPink : (i == 2 ? capYellow : (i == 3 ? capCream : capGrey)));
}
// Whichever of dark or white ink reads better on the plate.
juce::Colour inkFor(juce::Colour plate)
{
    auto lin = [](float c) { return c <= 0.03928f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };
    const float lum = 0.2126f * lin(plate.getFloatRed()) + 0.7152f * lin(plate.getFloatGreen()) + 0.0722f * lin(plate.getFloatBlue());
    const juce::Colour dark(0xff17120a);
    const float darkLum = 0.0070f;
    return (lum + 0.05f) / (darkLum + 0.05f) >= 1.05f / (lum + 0.05f) ? dark : juce::Colours::white;
}

// ---------------------------------------------------------------------------
// Small drawing vocabulary
// ---------------------------------------------------------------------------

// No readable text is set below 12.5 design px (about 8.75 px on screen in the smallest window).
constexpr float kMinFont = 12.5f;

// The panel typeface (ArtCommon.h), so the Studio view prints like the instrument on every OS.
juce::Font fnt(float height, bool bold = false, float kern = 0.0f)
{
    auto f = art::displayFont(juce::jmax(height, kMinFont), bold ? juce::Font::bold : juce::Font::plain);
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

// Axis numeral at the type floor, contrast above 4:1 on the glass.
void axisText(juce::Graphics& g, const juce::String& s, Rect r, Just j = Just::centred)
{
    text(g, s, r, kMinFont, false, axisInk(), j);
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

// The wrinkle-paint finish and fillRelief are the shared ones in ArtCommon.h (one copy for every panel).
// Device pixels per design unit of the layer being painted; set by the cached layers.
float gArtScale = 1.0f;

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

// Where the title chiclet sits on a tinted bracket (shared by the bracket and the lamp widget).
Rect bracketChipRect(float x0, float x1, float y, const juce::String& title, float size)
{
    const float tw = textWidth(title, size, true, 0.14f) + 26.0f;
    const float total = tw + 24.0f;
    const float mid = (x0 + x1) * 0.5f;
    return { mid - total * 0.5f + 4.0f, y - 5.0f, 17.0f, 10.0f };
}

// Gold etched bracket carrying a cream title, as on the main control surface. A tinted bracket
// carries a cap-coloured chiclet before the title; with `chip` false the space is kept and a live
// lamp widget sits there instead.
void bracketTitle(juce::Graphics& g, float x0, float x1, float y, const juce::String& title,
                  float size = 13.0f, juce::Colour accent = juce::Colours::transparentBlack, bool chip = true)
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
    if (tinted && chip)
        capChip(g, { left + 4.0f, y - 5.0f, 17.0f, 10.0f }, accent);
    engrave(g, title, { left + (tinted ? 24.0f : 0.0f), y - 10.0f, tw, 20.0f }, size, true, ivory, Just::centred, 0.14f);
}

// A raised charcoal sub-panel with a cast shadow, lit upper-left edge, dark lower-right edge,
// crinkle-finished and screwed down at the corners.
constexpr float kInfoReserve = 88.0f;   // title bracket stops this far from the module's right edge when it has an info button

void drawModule(juce::Graphics& g, Rect r, const juce::String& title, bool primary,
                juce::Colour accent = juce::Colours::transparentBlack, bool hasInfo = false, bool chip = true)
{
    constexpr float rad = 6.0f;
    softShadow(g, r, rad, 0.0f, 4.0f, 8.0f, 0.62f);
    juce::Path p;
    p.addRoundedRectangle(r, rad);
    g.saveState();
    g.reduceClipRegion(p);
    fillPanelFace(g, r);
    art::fillRelief(g, art::panelTile(), r, gArtScale, 0.80f, 11.0f, 7.0f);
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
    bracketTitle(g, r.getX() + 32.0f, r.getRight() - (hasInfo ? kInfoReserve : 32.0f), r.getY() + 22.0f,
                 title.toUpperCase(), primary ? 13.5f : 12.5f, accent, chip);
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
    const Rect badge = Rect(0.0f, 0.0f, textWidth(label, 12.5f, true, 0.08f) + 28.0f, 22.0f).withCentre(r.getCentre());
    g.setColour(kInk.withAlpha(0.9f));
    g.fillRoundedRectangle(badge, 3.0f);
    g.setColour(amber.withAlpha(0.7f));
    g.drawRoundedRectangle(badge, 3.0f, 1.0f);
    text(g, label, badge, 12.5f, true, amber, Just::centred, 0.08f);
}

// One rule for every time on these pages: below 1000 ms print milliseconds, from 1000 ms print seconds.
juce::String periodString(float seconds)
{
    const int ms = juce::roundToInt(seconds * 1000.0f);
    if (ms < 1000)
        return juce::String(ms) + " ms";
    return juce::String(seconds, seconds < 10.0f ? 2 : 1) + " s";
}

juce::String signedString(float v, int decimals, const juce::String& unit)
{
    return (v >= 0.0f ? "+" : "-") + juce::String(std::abs(v), decimals) + unit;
}

juce::String plusMinus() { return juce::String(juce::CharPointer_UTF8("\xc2\xb1")); }

// The plate reverb's low-band decay, estimated from the engine's own constants: four comb loops per
// channel with feedback 0.78 and delays (0.0297 + 0.011 i) * kReverbTimeScale * (1 + 0.031 i) seconds.
// RT60 of a loop is 60 dB divided by the 2.16 dB it loses on each trip. An estimate, never a measurement.
struct PlateTail
{
    std::array<float, 4> rt60 {}, delay {};
    float lo = 0.0f, hi = 0.0f, mean = 0.0f;
    // Linear envelope of the summed loops, 1 at the onset.
    float envelope(float t) const
    {
        float sum = 0.0f;
        for (float r : rt60)
            sum += std::pow(10.0f, -3.0f * t / r);
        return sum * 0.25f;
    }
};
PlateTail plateTail()
{
    PlateTail t;
    for (int i = 0; i < 4; ++i)
    {
        const float d = (0.0297f + 0.011f * (float) i) * cs80::kReverbTimeScale * (1.0f + 0.031f * (float) i);
        t.delay[(size_t) i] = d;
        t.rt60[(size_t) i] = 3.0f * d / -std::log10(0.78f);
        t.mean += t.rt60[(size_t) i] * 0.25f;
    }
    t.lo = *std::min_element(t.rt60.begin(), t.rt60.end());
    t.hi = *std::max_element(t.rt60.begin(), t.rt60.end());
    return t;
}

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

// ---- Module lamp: the cap-coloured chiclet on a title bracket, lit only while the module is doing something ------
class ModuleLamp final : public LiveWidget
{
public:
    ModuleLamp(Cs80AudioProcessor& p, juce::Colour c, std::function<bool(Cs80AudioProcessor&)> st)
        : LiveWidget(p, 17.0f, 10.0f), colour(c), state(std::move(st)) {}

    void paintStatic(juce::Graphics&) override {}

private:
    void sample(Sig& s) const override { s[0] = state(proc) ? 1.0f : 0.0f; }

    void draw(juce::Graphics& g) override
    {
        const bool on = state(proc);
        const Rect r(0.0f, 0.0f, designW, designH);
        if (on)
        {
            g.setColour(colour.withAlpha(0.22f));
            g.fillRoundedRectangle(r.expanded(3.5f, 3.0f), 4.0f);
            capChip(g, r, colour);
        }
        else
            capChip(g, r, juce::Colour(0xff35302a).interpolatedWith(colour, 0.10f));
    }

    juce::Colour colour;
    std::function<bool(Cs80AudioProcessor&)> state;
};

// ---- LFO scope: the LFO at the set rate; the modulated output is drawn only when routed --------
class LfoScope final : public LiveWidget
{
public:
    LfoScope(Cs80AudioProcessor& p, int index)
        : LiveWidget(p, 440.0f, 196.0f), n(index), colour(lfoColour(index))
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
            axisText(g, i == 0 ? "0" : (i == 4 ? "4 s" : juce::String(i)), { x - 16.0f, plot.getBottom() + 3.0f, 32.0f, 14.0f });
        }
        hline(g, plot.getX(), plot.getRight(), plot.getCentreY(), 0.32f);
        hline(g, plot.getX(), plot.getRight(), plot.getY() + 4.0f, 0.12f);
        hline(g, plot.getX(), plot.getRight(), plot.getBottom() - 4.0f, 0.12f);
    }

private:
    static Rect plotRect() { return { 70.0f, 32.0f, 356.0f, 132.0f }; }

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
        // The vertical axis is the swing the engine applies; before a route exists it is the unit LFO.
        const float swing = routed ? depth * kLfoScale[dest] : 1.0f;
        const juce::String top = routed ? "+" + juce::String(swing, 2) + kLfoUnit[dest] : juce::String("+1");
        const juce::String bottom = routed ? "-" + juce::String(swing, 2) : juce::String("-1");
        const Rect axisBox(2.0f, 0.0f, plot.getX() - 8.0f, 14.0f);
        axisText(g, top, axisBox.withY(plot.getY() - 3.0f), Just::centredRight);
        axisText(g, "0", axisBox.withY(plot.getCentreY() - 7.0f), Just::centredRight);
        axisText(g, bottom, axisBox.withY(plot.getBottom() - 11.0f), Just::centredRight);

        if (routed)
        {
            // Full-scale LFO faint underneath; the modulated output is the lit trace.
            g.setColour(colour.withAlpha(0.30f));
            g.strokePath(build(1.0f), juce::PathStrokeType(1.0f));
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
        else
        {
            // Not routed: the unit LFO, dimmed, so the card reads as idle.
            g.setColour(axisInk().withAlpha(0.40f));
            g.strokePath(build(1.0f), juce::PathStrokeType(1.3f));
        }

        text(g, juce::String(rate, rate < 10.0f ? 2 : 1) + " Hz    T " + periodString(1.0f / juce::jmax(0.05f, rate)),
             { 14.0f, 5.0f, 230.0f, 18.0f }, 12.0f, true, routed ? colour.brighter(0.3f) : axisInk(), Just::centredLeft, 0.03f);
        juce::String right;
        if (iso) right = "MASKED BY ISOLATION";
        else if (dest == 0) right = "NOT ROUTED";
        else if (depth <= 0.001f) right = kDestinations[dest] + "   DEPTH 0";
        else right = "TO " + kDestinations[dest];
        text(g, right, { 220.0f, 5.0f, 206.0f, 18.0f }, 12.0f, true, routed ? ivory : (iso ? amber : axisInk()), Just::centredRight, 0.06f);
    }

    int n, rateIdx, depthIdx, destIdx;
    juce::Colour colour;
};

// ---- Mod wheel routing: what each wheel amount means in the engine's own units ------------------
class WheelRouting final : public LiveWidget
{
public:
    explicit WheelRouting(Cs80AudioProcessor& p) : LiveWidget(p, 448.0f, 112.0f) {}

private:
    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_modWheelSub); s[1] = v(cs80::P_modWheelVcf); s[2] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const float sub = v(cs80::P_modWheelSub), vcf = v(cs80::P_modWheelVcf);
        const bool iso = isolated();
        auto row = [&](float y, const juce::String& label, float amount, const juce::String& result)
        {
            const float bar = designW - 32.0f;
            text(g, label, { 16.0f, y, 220.0f, 20.0f }, 12.5f, true, ivory, Just::centredLeft, 0.05f);
            text(g, result, { 220.0f, y, designW - 236.0f, 20.0f }, 14.0f, true, amount > 0.001f && ! iso ? amberBright : axisInk(), Just::centredRight, 0.02f);
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.fillRoundedRectangle(16.0f, y + 26.0f, bar, 8.0f, 4.0f);
            g.setColour(iso ? ivoryDim.withAlpha(0.4f) : amber);
            if (amount > 0.001f)
                g.fillRoundedRectangle(16.0f, y + 26.0f, juce::jmax(5.0f, bar * amount), 8.0f, 4.0f);
        };
        row(8.0f, "WHEEL > SUB PITCH", sub, "up to " + plusMinus() + juce::String(sub * 2.0f, 2) + " st");
        row(58.0f, "WHEEL > VCF", vcf, "up to +" + juce::String(vcf * 1.5f, 2) + " oct");
        if (iso)
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }
};

// ---- Tuning readout: the sounding A4 and where it comes from ---------------------------------------
class TuningDisplay final : public LiveWidget
{
public:
    explicit TuningDisplay(Cs80AudioProcessor& p, float width = 944.0f) : LiveWidget(p, width, 112.0f) {}

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
        const bool off = std::abs(a4 - 440.0f) > 0.005f;
        text(g, "SOUNDING A4", { 20.0f, 10.0f, 220.0f, 16.0f }, 11.0f, true, ivoryDim, Just::centredLeft, 0.1f);
        text(g, juce::String(a4, 2) + " Hz", { 20.0f, 30.0f, 300.0f, 52.0f }, 38.0f, true, off ? amberBright : ivory, Just::centredLeft, 0.01f);
        text(g, off ? signedString(1200.0f * std::log2(a4 / 440.0f), 1, " ct from 440 Hz") : juce::String("CONCERT PITCH"),
             { 20.0f, 84.0f, 300.0f, 18.0f }, 11.0f, true, off ? amber : axisInk(), Just::centredLeft, 0.04f);

        vline(g, 336.0f, 14.0f, designH - 14.0f, 0.22f, gold);
        const float split2 = designW - 156.0f;
        auto row = [&](float x, float y, const juce::String& k, const juce::String& val, bool hot)
        {
            text(g, k, { x, y, 180.0f, 20.0f }, 11.0f, true, ivoryDim, Just::centredLeft, 0.08f);
            text(g, val, { x + 150.0f, y, split2 - 24.0f - (x + 150.0f), 20.0f }, 13.0f, true, hot ? amberBright : ivory, Just::centredRight, 0.02f);
        };
        row(356.0f, 10.0f, "A4 REFERENCE", iso ? "440.0 Hz (ISOLATION)" : juce::String(ref, 1) + " Hz", ref != 440.0f && ! iso);
        row(356.0f, 38.0f, "PITCH KNOB", signedString(mt, 2, " st") + " = " + signedString(mt * 100.0f, 0, " ct"), std::abs(mt) > 0.001f);
        row(356.0f, 66.0f, "TEMPERAMENT", "12-TONE EQUAL", false);
        vline(g, split2, 14.0f, designH - 14.0f, 0.22f, gold);
        for (int i = 0; i < 3; ++i)
        {
            const int note = i == 0 ? 60 : (i == 1 ? 57 : 81);
            const float hz = a4 * std::pow(2.0f, (float) (note - 69) / 12.0f);
            const float y = 10.0f + (float) i * 28.0f;
            text(g, noteName(note), { split2 + 22.0f, y, 50.0f, 20.0f }, 13.0f, true, ivory, Just::centredLeft);
            text(g, juce::String(hz, 2) + " Hz", { split2 + 66.0f, y, 80.0f, 20.0f }, 13.0f, true, axisInk(), Just::centredRight);
        }
    }
};

// ---- Sub oscillator scope: the waveform the engine would actually run ---------------------
class SubScope final : public LiveWidget
{
public:
    explicit SubScope(Cs80AudioProcessor& p) : LiveWidget(p, 712.0f, 150.0f)
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
                     { x - 18.0f, plot.getBottom() + 3.0f, 36.0f, 14.0f });
        }
        hline(g, plot.getX(), plot.getRight(), plot.getCentreY(), 0.32f);
        axisText(g, "+1", { 4.0f, plot.getY() - 3.0f, 40.0f, 14.0f }, Just::centredRight);
        axisText(g, "0", { 4.0f, plot.getCentreY() - 7.0f, 40.0f, 14.0f }, Just::centredRight);
        axisText(g, "-1", { 4.0f, plot.getBottom() - 11.0f, 40.0f, 14.0f }, Just::centredRight);
    }

private:
    static Rect plotRect() { return { 50.0f, 30.0f, 650.0f, 90.0f }; }

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
        const bool bus = proc.isExternalInputEnabled();
        if (ext)
        {
            // No input meter exists: a disabled bus is a faint line, an enabled one a firmer line.
            g.setColour(ivoryDim.withAlpha(bus ? 0.8f : 0.22f));
            const float dash[] = { 5.0f, 4.0f };
            g.drawDashedLine(juce::Line<float>(plot.getX(), cy, plot.getRight(), cy), dash, 2, 1.4f);
            text(g, bus ? "EXTERNAL INPUT ROUTED FROM THE HOST  -  LEVEL IS NOT METERED" : "EXT BUS DISABLED  -  NO SIGNAL",
                 { plot.getX() + 40.0f, cy - 30.0f, plot.getWidth() - 80.0f, 22.0f }, 12.5f, true, bus ? ivory : amber, Just::centred, 0.06f);
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
        text(g, mode + "  -  " + name, { 14.0f, 5.0f, 420.0f, 18.0f }, 12.0f, true, amber, Just::centredLeft, 0.08f);
        juce::String right;
        if (ext)
            right = "EXT LEVEL " + juce::String(juce::roundToInt(v(cs80::P_subExtLevel) * 100.0f)) + "%  -  BUS "
                    + (proc.isExternalInputEnabled() ? "ENABLED" : "DISABLED");
        else if (noiseFn)
            right = "SPEED IGNORED";
        else
            right = juce::String(rate, rate < 10.0f ? 2 : 1) + " Hz";
        text(g, right, { 330.0f, 5.0f, 370.0f, 18.0f }, 12.0f, true, ext ? ivory : amber, Just::centredRight, 0.05f);
    }

    std::array<float, 400> noise {};
};

// ---- Sub oscillator routing: the engine's own units for every depth and pressure amount ------------
class SubRouting final : public LiveWidget
{
public:
    explicit SubRouting(Cs80AudioProcessor& p) : LiveWidget(p, 712.0f, 100.0f) {}

private:
    void sample(Sig& s) const override
    {
        const int idx[6] = { cs80::P_subVco, cs80::P_subVcf, cs80::P_subVca, cs80::P_subAtSpeed, cs80::P_subAtVco, cs80::P_subAtVcf };
        for (int i = 0; i < 6; ++i)
            s[(size_t) i] = v(idx[i]);
        s[6] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const bool iso = isolated();
        struct Row { const char* name; int param; float full; const char* unit; };
        static const Row left[3] = { { "VCO", cs80::P_subVco, 1.5f, " st" }, { "VCF", cs80::P_subVcf, 1.5f, " oct" },
                                     { "VCA", cs80::P_subVca, 0.45f, " lvl" } };
        static const Row right[3] = { { "AT SPEED", cs80::P_subAtSpeed, 0.0f, "" }, { "AT VCO", cs80::P_subAtVco, 1.5f, " st" },
                                      { "AT VCF", cs80::P_subAtVcf, 1.5f, " oct" } };
        text(g, "DEPTH NOW  (MAX)", { 14.0f, 0.0f, 330.0f, 20.0f }, 12.5f, true, gold, Just::centredLeft, 0.1f);
        text(g, "PRESSURE AT FULL TOUCH  (MAX)", { 370.0f, 0.0f, 330.0f, 20.0f }, 12.5f, true, gold, Just::centredLeft, 0.1f);
        vline(g, 356.0f, 4.0f, designH - 4.0f, 0.18f, gold);
        auto cell = [&](float x, float y, const Row& r)
        {
            const float amount = v(r.param);
            const bool zero = amount <= 0.001f;
            const bool lit = ! zero && ! iso;
            text(g, r.name, { x, y, 80.0f, 22.0f }, 12.5f, true, lit ? ivory : ivoryDim, Just::centredLeft, 0.03f);
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.fillRoundedRectangle(x + 84.0f, y + 8.0f, 72.0f, 7.0f, 3.5f);
            if (! zero)
            {
                g.setColour(iso ? ivoryDim.withAlpha(0.4f) : amber);
                g.fillRoundedRectangle(x + 84.0f, y + 8.0f, juce::jmax(4.0f, 72.0f * amount), 7.0f, 3.5f);
            }
            // The bright figure is what the setting gives now; the dim one is what full amount would give.
            // AT SPEED multiplies the rate by 1 + 2 x amount at full key pressure.
            const juce::String now = r.full > 0.0f ? (zero ? juce::String("0") + r.unit : "+" + juce::String(amount * r.full, 2) + r.unit)
                                                   : (zero ? juce::String("0") : "x" + juce::String(1.0f + 2.0f * amount, 2));
            const juce::String top = r.full > 0.0f ? "max " + juce::String(r.full, 2) : juce::String("max x3.00");
            text(g, now, { x + 158.0f, y, 90.0f, 22.0f }, 12.5f, lit, lit ? amberBright : ivoryDim, Just::centredRight, 0.02f);
            text(g, top, { x + 254.0f, y, 74.0f, 22.0f }, 12.5f, false, axisInk(), Just::centredRight);
        };
        for (int i = 0; i < 3; ++i)
        {
            cell(14.0f, 24.0f + (float) i * 25.0f, left[i]);
            cell(370.0f, 24.0f + (float) i * 25.0f, right[i]);
        }
    }
};

// ---- Ring modulator: carrier frequency over a key-held envelope, with draggable A / D nodes ----
// The five ring parameters have their front-panel home; this plot is where their combined effect is
// read and where the envelope is edited by hand. At zero depth a dashed preview at full depth shows
// the shape the controls will draw.
class RingCurve final : public LiveWidget
{
public:
    explicit RingCurve(Cs80AudioProcessor& p) : LiveWidget(p, 668.0f, 376.0f) { enableMouse(false); }

    void paintStatic(juce::Graphics& g) override { glassWell(g, { 0.0f, 0.0f, designW, designH }); }

private:
    static Rect plotRect() { return { 84.0f, 58.0f, 562.0f, 226.0f }; }

    struct Geo
    {
        float a, d, speed, depth, ed, amount, ta, td, total, fmax;
        bool ghost;
    };

    Geo geo() const
    {
        Geo r {};
        r.a = juce::jmax(0.001f, v(cs80::P_ringAttack));
        r.d = juce::jmax(0.001f, v(cs80::P_ringDecay));
        r.speed = juce::jmax(0.05f, v(cs80::P_ringSpeed));
        r.depth = v(cs80::P_ringDepth);
        r.amount = v(cs80::P_ringAmount);
        r.ghost = r.depth <= 0.001f;
        r.ed = r.ghost ? 1.0f : r.depth;
        // Engine Adsr: attack 1 - exp(-6 t / A) until ~0.999, then exp(-6 t / D) down to the zero sustain.
        r.ta = 1.151f * r.a;
        r.td = 1.267f * r.d;
        r.total = r.ta + r.td;
        r.fmax = r.speed * (1.0f + r.ed) * 1.1f;
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
                     plot.getBottom() - plot.getHeight() * gm.speed * (1.0f + gm.ed) / gm.fmax };
        return { plot.getX() + plot.getWidth() * (gm.ta + gm.td) / gm.total,
                 plot.getBottom() - plot.getHeight() * gm.speed / gm.fmax };
    }

    int hitNode(Pt p) const
    {
        const auto gm = geo();
        for (int which = 1; which <= 2; ++which)
            if (nodePos(gm, which).getDistanceFrom(p) < 15.0f)
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

    static juce::String timeText(float s) { return periodString(s); }
    static juce::String hzText(float f) { return juce::String(f, f < 10.0f ? 2 : (f < 100.0f ? 1 : 0)); }

    void draw(juce::Graphics& g) override
    {
        const auto gm = geo();
        const auto plot = plotRect();
        const bool wet = gm.amount > 0.001f;
        const bool live = wet && ! gm.ghost;
        const juce::Colour curve = live ? amberBright : ivoryDim;

        // Frequency axis: 0, the unmodulated carrier and the peak, all in Hz.
        auto yOf = [&](float f) { return plot.getBottom() - plot.getHeight() * f / gm.fmax; };
        const float peak = gm.speed * (1.0f + gm.ed);
        hline(g, plot.getX(), plot.getRight(), plot.getBottom(), 0.34f);
        hline(g, plot.getX(), plot.getRight(), yOf(gm.speed), 0.26f);
        axisText(g, "0", { 4.0f, plot.getBottom() - 8.0f, 72.0f, 16.0f }, Just::centredRight);
        axisText(g, hzText(gm.speed) + " Hz", { 0.0f, yOf(gm.speed) - 8.0f, 78.0f, 16.0f }, Just::centredRight);
        if (std::abs(yOf(gm.speed) - yOf(peak)) > 17.0f)
        {
            hline(g, plot.getX(), plot.getRight(), yOf(peak), 0.16f);
            axisText(g, hzText(peak) + " Hz", { 0.0f, yOf(peak) - 8.0f, 78.0f, 16.0f }, Just::centredRight);
        }
        for (int i = 0; i <= 4; ++i)
            vline(g, plot.getX() + plot.getWidth() * (float) i / 4.0f, plot.getY(), plot.getBottom(), i == 0 ? 0.34f : 0.14f);

        auto env = [&](float t)
        {
            if (t <= gm.ta) return 1.0f - std::exp(-6.0f * t / gm.a);
            return std::exp(-6.0f * (t - gm.ta) / gm.d);
        };
        juce::Path carrier;
        constexpr int count = 360;
        const float span = gm.ta + gm.td;
        for (int i = 0; i <= count; ++i)
        {
            const float t = span * (float) i / (float) count;
            const float x = plot.getX() + plot.getWidth() * t / gm.total;
            const float y = yOf(gm.speed * (1.0f + gm.ed * env(t)));
            if (i == 0) carrier.startNewSubPath(x, y); else carrier.lineTo(x, y);
        }
        auto fill = carrier;
        fill.lineTo(plot.getX() + plot.getWidth() * span / gm.total, plot.getBottom());
        fill.lineTo(plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setColour(amber.withAlpha(live ? 0.20f : 0.06f));
        g.fillPath(fill);
        if (live)
        {
            g.setColour(amberBright.withAlpha(0.28f));
            g.strokePath(carrier, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(curve);
            g.strokePath(carrier, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            // Idle or bypassed: the same curve as a dashed preview.
            juce::Path dashed;
            const float dash[] = { 6.0f, 5.0f };
            juce::PathStrokeType(1.8f).createDashedStroke(dashed, carrier, dash, 2);
            g.setColour(ivory.withAlpha(0.62f));
            g.fillPath(dashed);
        }

        // Draggable nodes: A at the carrier peak, D where the decay reaches the carrier.
        for (int which = 1; which <= 2; ++which)
        {
            const Pt c = nodePos(gm, which);
            const bool hot = hover == which || dragging == which;
            const juce::Colour ink = hot ? juce::Colour(0xfff6df91) : (live ? amberBright : ivory.withAlpha(0.85f));
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.fillEllipse(c.x - 10.5f, c.y - 10.0f, 21.0f, 21.0f);
            g.setColour(ink);
            g.fillEllipse(c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f);
            g.setColour(juce::Colours::black.withAlpha(0.88f));
            g.fillEllipse(c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f);
            text(g, which == 1 ? "A" : "D", { c.x - 8.0f, c.y - 8.0f, 16.0f, 16.0f }, 12.5f, true, ink, Just::centred);
        }

        // Time axis: each bracket and label sits under the segment it describes.
        const float xa = plot.getX() + plot.getWidth() * gm.ta / gm.total;
        const float xe = plot.getX() + plot.getWidth() * (gm.ta + gm.td) / gm.total;
        const float by = plot.getBottom() + 9.0f;
        auto bracket = [&](float x0, float x1, const juce::String& s, juce::Colour c)
        {
            g.setColour(c.withAlpha(0.8f));
            g.drawLine(x0 + 1.0f, by, x1 - 1.0f, by, 1.2f);
            g.drawLine(x0 + 1.0f, by - 3.0f, x0 + 1.0f, by + 3.0f, 1.2f);
            g.drawLine(x1 - 1.0f, by - 3.0f, x1 - 1.0f, by + 3.0f, 1.2f);
            const float tw = textWidth(s, 12.5f, true, 0.02f) + 8.0f;
            const float cx = juce::jlimit(plot.getX() + tw * 0.5f, plot.getRight() - tw * 0.5f, (x0 + x1) * 0.5f);
            text(g, s, { cx - tw * 0.5f, by + 4.0f, tw, 17.0f }, 12.5f, true, c, Just::centred, 0.02f);
        };
        bracket(plot.getX(), xa, "A " + timeText(gm.a), amber);
        bracket(xa, xe, "D " + timeText(gm.d), ivory);
        axisText(g, "0", { plot.getX() - 12.0f, plot.getBottom() + 29.0f, 24.0f, 16.0f });
        axisText(g, timeText(gm.total), { plot.getRight() - 70.0f, plot.getBottom() + 29.0f, 70.0f, 16.0f }, Just::centredRight);

        text(g, "CARRIER FREQUENCY, KEY HELD", { 14.0f, 6.0f, 300.0f, 20.0f }, 12.5f, true, axisInk(), Just::centredLeft, 0.04f);
        // A bypassed ring modulator reads as idle: neutral ink, never the amber of a working stage.
        text(g, wet ? "WET " + juce::String(juce::roundToInt(gm.amount * 100.0f)) + "%" : juce::String("BYPASSED  -  MOD 0%"),
             { 330.0f, 6.0f, 324.0f, 20.0f }, 12.5f, true, wet ? amberBright : axisInk(), Just::centredRight, 0.05f);
        text(g, hzText(gm.speed) + " > " + hzText(peak) + " Hz", { 14.0f, 26.0f, 300.0f, 22.0f }, 15.0f, true,
             live ? ivory : ivoryDim, Just::centredLeft, 0.03f);
        text(g, gm.ghost ? juce::String("DEPTH 0%  -  PREVIEW AT 100%") : "DEPTH " + juce::String(juce::roundToInt(gm.depth * 100.0f)) + "%",
             { 330.0f, 28.0f, 324.0f, 20.0f }, 12.5f, true, gm.ghost ? axisInk() : ivory, Just::centredRight, 0.04f);
        text(g, "DRAG A: ATTACK AND DEPTH      DRAG D: DECAY", { 14.0f, 336.0f, 640.0f, 18.0f }, 12.5f, true, axisInk(), Just::centredLeft, 0.03f);
        text(g, "RELEASE 0.35 s, FIXED   -   THE FIRST KEY OF A PHRASE TRIGGERS IT", { 14.0f, 354.0f, 640.0f, 18.0f }, 12.5f, false, axisInk(),
             Just::centredLeft);
    }

    int hover = 0, dragging = 0;
    float dragTotal = 1.0f, dragFmax = 1.0f;
    std::vector<int> params;
};

// ---- Chorus / tremolo sweep -----------------------------------------------------------------
// Chorus and tremolo are set on the main panel; this plot shows what those settings do to the
// delay line (or the gain), channel by channel.
class ChorusSweep final : public LiveWidget
{
public:
    explicit ChorusSweep(Cs80AudioProcessor& p, float width = 732.0f) : LiveWidget(p, width, 276.0f) {}

private:
    Rect plotRect() const { return { 70.0f, 40.0f, designW - 90.0f, 120.0f }; }

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

        // The extremes the trace reaches, so the vertical axis can close in on them.
        const float bbdBase = 1000.0f * 256.0f / 106904.5f;
        float vmin, vmax, step, minSpan;
        juce::String unit = " ms", title = "LEGACY  -  GENERIC DELAY LINE";
        if (trem) { vmin = 1.0f - depth * 0.45f; vmax = 1.0f; step = 0.05f; minSpan = 0.1f; unit = ""; title = "TREMOLO  -  GENERIC AMPLITUDE LAW  -  GAIN"; }
        else if (bbd) { vmin = bbdBase / std::exp2(depth * 0.5f); vmax = bbdBase * std::exp2(depth * 0.5f); step = 0.5f; minSpan = 1.0f;
                        title = "OE2 BBD DELAY  -  CLOCK RANGE INFERRED"; }
        else { vmin = 13.5f; vmax = 13.5f + 4.5f * depth; step = 1.0f; minSpan = 4.0f; }
        const float centre = std::round(0.5f * (vmin + vmax) / step) * step;
        const float half = std::ceil((juce::jmax((vmax - vmin) * 1.5f, minSpan) + step) * 0.5f / step) * step;
        float lo = centre - half, hi = centre + half;
        if (lo < 0.0f) { hi -= lo; lo = 0.0f; }
        if (trem && hi > 1.0f) { lo -= hi - 1.0f; hi = 1.0f; lo = juce::jmax(0.0f, lo); }

        for (int i = 0; i <= 2; ++i)
        {
            const float y = plot.getBottom() - plot.getHeight() * (float) i / 2.0f;
            hline(g, plot.getX(), plot.getRight(), y, i == 0 ? 0.34f : 0.16f);
            const float val = lo + (hi - lo) * (float) i / 2.0f;
            axisText(g, juce::String(val, trem ? 2 : (step < 1.0f ? 1 : 0)) + (i == 2 ? unit : juce::String()), { 0.0f, y - 8.0f, 66.0f, 16.0f },
                     Just::centredRight);
        }
        for (int i = 0; i <= 4; ++i)
            vline(g, plot.getX() + plot.getWidth() * (float) i / 4.0f, plot.getY(), plot.getBottom(), i % 2 == 0 ? 0.24f : 0.10f);
        axisText(g, "0", { plot.getX() - 12.0f, plot.getBottom() + 3.0f, 24.0f, 16.0f });
        axisText(g, "2 PERIODS  =  " + periodString(2.0f / juce::jmax(0.05f, speed)), { plot.getRight() - 190.0f, plot.getBottom() + 3.0f, 190.0f, 16.0f },
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
                    value = bbdBase / std::exp2(depth * 0.5f * mod);
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

        text(g, title, { 14.0f, 6.0f, 410.0f, 22.0f }, 12.5f, true, on ? amber : axisInk(), Just::centredLeft, 0.03f);
        text(g, on ? "ON" : "OFF  -  BYPASSED", { designW - 152.0f, 6.0f, 138.0f, 22.0f }, 12.5f, true, on ? amberBright : axisInk(), Just::centredRight, 0.06f);

        // Readouts. SPEED and DEPTH are main-panel settings shown here for reference, so they are flat
        // figures between hairlines in ivory; the delay or gain range is derived from them and lit.
        juce::String range;
        if (trem) range = "1.00 - " + juce::String(1.0f - depth * 0.45f, 2);
        else if (bbd) range = juce::String(bbdBase / std::exp2(depth * 0.5f), 2) + " - " + juce::String(bbdBase * std::exp2(depth * 0.5f), 2) + " ms";
        else range = "13.5 - " + juce::String(13.5f + 4.5f * depth, 1) + " ms";
        struct Cell { const char* k; juce::String val; bool derived; };
        const Cell cells[3] = { { "SPEED", juce::String(speed, speed < 10.0f ? 2 : 1) + " Hz", false },
                                { "DEPTH", juce::String(juce::roundToInt(depth * 100.0f)) + "%", false },
                                { trem ? "GAIN RANGE" : "DELAY RANGE", range, true } };
        const float cellW[3] = { 200.0f, 170.0f, 300.0f };
        float cx = 14.0f;
        const float cy = 186.0f;
        hline(g, 14.0f, designW - 14.0f, cy, 0.20f);
        hline(g, 14.0f, designW - 14.0f, cy + 50.0f, 0.20f);
        for (int i = 0; i < 3; ++i)
        {
            if (i > 0)
                vline(g, cx - 8.0f, cy + 6.0f, cy + 44.0f, 0.22f);
            text(g, cells[i].k, { cx, cy + 3.0f, cellW[i] - 10.0f, 20.0f }, 12.5f, true, ivoryDim, Just::centredLeft, 0.08f);
            text(g, cells[i].val, { cx, cy + 22.0f, cellW[i] - 10.0f, 26.0f }, 18.0f, true,
                 ! on ? ivoryDim : (cells[i].derived ? amberBright : ivory), Just::centredLeft, 0.02f);
            cx += cellW[i] + 12.0f;
        }
        text(g, on ? (bbd ? "ONE SHARED WET PATH  (LEFT = RIGHT)" : "FAINT TRACE = RIGHT CHANNEL, PHASE +0.95 PI")
                   : "SWITCH CHORUS ON AT THE MAIN PANEL TO HEAR THIS STAGE",
             { 14.0f, 244.0f, designW - 28.0f, 18.0f }, 12.5f, false, axisInk(), Just::centredLeft, 0.01f);
    }
};

// ---- Analog voice numbers ----------------------------------------------------------------------
class VoiceLcd final : public LiveWidget
{
public:
    explicit VoiceLcd(Cs80AudioProcessor& p, float width = 348.0f) : LiveWidget(p, width, 124.0f) {}

private:
    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_voiceDispersion); s[1] = v(cs80::P_drift); s[2] = v(cs80::P_analogNoise);
        s[3] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const float disp = v(cs80::P_voiceDispersion), drift = v(cs80::P_drift), noise = v(cs80::P_analogNoise);
        const bool iso = isolated();
        struct Cell { const char* k; juce::String val; bool lit; };
        const Cell cells[4] = {
            { "TUNING SPREAD", plusMinus() + juce::String(disp * cs80::kProvisionalVoiceTuneSpreadCents, 1) + " ct", disp > 0.001f },
            { "PAN SPREAD", plusMinus() + juce::String(disp * 0.8f, 2), disp > 0.001f },
            { "DRIFT", juce::String(juce::roundToInt(drift * 100.0f)) + "%  /  32 s", drift > 0.001f },
            { "NOISE FLOOR", noise <= 0.001f ? juce::String("OFF") : juce::String(juce::roundToInt(20.0f * std::log10(noise * 0.0008f))) + " dB", noise > 0.001f } };
        for (int i = 0; i < 4; ++i)
        {
            const float cw = (designW - 28.0f) * 0.5f;
            const Rect c(8.0f + (float) (i % 2) * (cw + 12.0f), 4.0f + (float) (i / 2) * 60.0f, cw, 54.0f);
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.fillRoundedRectangle(c, 4.0f);
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.drawRoundedRectangle(c, 4.0f, 1.0f);
            text(g, cells[i].k, c.withTrimmedLeft(12.0f).withHeight(24.0f), 11.0f, true, ivoryDim, Just::centredLeft, 0.1f);
            text(g, cells[i].val, c.withTrimmedLeft(12.0f).withTrimmedTop(20.0f), 19.0f, true, cells[i].lit && ! iso ? amberBright : ivoryDim,
                 Just::centredLeft, 0.02f);
        }
        if (iso)
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }
};

// ---- Voice dispersion: pan against tuning for all sixteen ranks ---------------------------------
// The axes follow the dispersion that is actually set, so a few cents of spread fills the plot
// instead of hugging the centre. At zero dispersion a dimmed preview at full dispersion is shown.
class VoiceScatter final : public LiveWidget
{
public:
    explicit VoiceScatter(Cs80AudioProcessor& p, float width = 308.0f) : LiveWidget(p, width, 276.0f)
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
        axisText(g, "PAN", { plot.getCentreX() - 20.0f, plot.getBottom() + 3.0f, 40.0f, 14.0f });
    }

private:
    Rect plotRect() const { return { 62.0f, 32.0f, designW - 78.0f, 178.0f }; }

    float dispersion() const { return isolated() ? 0.0f : v(cs80::P_voiceDispersion); }
    // Effective dispersion for drawing: a preview at 100 % when the control is at zero.
    float shown() const { const float d = dispersion(); return d > 0.001f ? d : 1.0f; }

    // Half-ranges of the two axes: pan in engine units, tuning in cents.
    void extents(float disp, float& panExt, float& centExt) const
    {
        float mp = 0.0f, mc = 0.0f;
        for (int i = 0; i < 16; ++i)
        {
            mp = juce::jmax(mp, std::abs(pan[(size_t) i]) * disp * 0.8f);
            mc = juce::jmax(mc, std::abs(tune[(size_t) i]) * disp * cs80::kProvisionalVoiceTuneSpreadCents);
        }
        auto nice = [](float x, float floorV)
        {
            x = juce::jmax(x * 1.15f, floorV);
            const float mag = std::pow(10.0f, std::floor(std::log10(x)));
            for (float m : { 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 10.0f })
                if (x <= m * mag)
                    return m * mag;
            return 10.0f * mag;
        };
        panExt = nice(mp, 0.05f);
        centExt = nice(mc, 0.5f);
    }

    Pt pointOf(int i, float disp, float panExt, float centExt) const
    {
        const auto plot = plotRect();
        return { plot.getCentreX() + (pan[(size_t) i] * disp * 0.8f / panExt) * plot.getWidth() * 0.5f,
                 plot.getCentreY() - (tune[(size_t) i] * disp * cs80::kProvisionalVoiceTuneSpreadCents / centExt) * plot.getHeight() * 0.5f };
    }

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
        const float d = shown();
        float px, cx;
        extents(d, px, cx);
        int best = -1;
        float bestD = 12.0f;
        for (int i = 0; i < 16; ++i)
        {
            const float dist = pointOf(i, d, px, cx).getDistanceFrom(p);
            if (dist < bestD) { bestD = dist; best = i; }
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
        const bool ghost = dispersion() <= 0.001f && ! isolated();
        const float disp = shown();
        float panExt, centExt;
        extents(disp, panExt, centExt);
        const auto plot = plotRect();
        auto num = [](float x) { return juce::String(x, x < 1.0f ? 2 : (x < 10.0f ? 1 : 0)); };
        axisText(g, "+" + num(centExt) + " ct", { 0.0f, plot.getY() - 7.0f, 58.0f, 14.0f }, Just::centredRight);
        axisText(g, "0", { 0.0f, plot.getCentreY() - 7.0f, 58.0f, 14.0f }, Just::centredRight);
        axisText(g, "-" + num(centExt), { 0.0f, plot.getBottom() - 7.0f, 58.0f, 14.0f }, Just::centredRight);
        axisText(g, "L " + num(panExt), { plot.getX() - 4.0f, plot.getBottom() + 3.0f, 56.0f, 14.0f }, Just::centredLeft);
        axisText(g, num(panExt) + " R", { plot.getRight() - 52.0f, plot.getBottom() + 3.0f, 56.0f, 14.0f }, Just::centredRight);

        text(g, "16 RANKS:  PAN AGAINST TUNING", { 14.0f, 6.0f, designW - 20.0f, 18.0f }, 12.5f, true, axisInk(), Just::centredLeft, 0.03f);
        const float fade = ghost ? 0.38f : 1.0f;
        for (int rank = 1; rank >= 0; --rank)
            for (int voice = 0; voice < 8; ++voice)
            {
                const int i = voice * 2 + rank;
                const Pt c = pointOf(i, disp, panExt, centExt);
                const bool sounding = proc.isVoiceActive(voice) && ! ghost;
                const bool hot = hover == i;
                if (sounding)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.18f));
                    g.fillEllipse(c.x - 11.0f, c.y - 11.0f, 22.0f, 22.0f);
                    g.setColour(juce::Colours::white.withAlpha(0.85f));
                    g.drawEllipse(c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f, 1.3f);
                }
                g.setColour(juce::Colours::black.withAlpha(0.7f * fade));
                g.fillEllipse(c.x - 6.8f, c.y - 6.4f, 13.6f, 13.6f);
                g.setColour((rank == 0 ? amberBright : ivory.withAlpha(0.92f)).withMultipliedAlpha(fade));
                g.fillEllipse(c.x - 5.8f, c.y - 5.8f, 11.6f, 11.6f);
                if (hot)
                {
                    g.setColour(juce::Colour(0xfff6df91));
                    g.drawEllipse(c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f, 1.6f);
                }
            }
        if (hover >= 0)
        {
            const int voice = hover / 2, rank = hover % 2;
            text(g, "CARD " + juce::String(voice + 1) + " / " + (rank == 0 ? "I" : "II") + "   "
                        + signedString(tune[(size_t) hover] * disp * cs80::kProvisionalVoiceTuneSpreadCents, 1, " ct") + "   PAN "
                        + signedString(pan[(size_t) hover] * disp * 0.8f, 2, ""),
                 { 14.0f, 244.0f, designW - 20.0f, 20.0f }, 12.5f, true, ivory, Just::centredLeft, 0.0f);
        }
        else if (ghost)
            text(g, "DISPERSION 0%  -  PREVIEW AT 100%", { 14.0f, 244.0f, designW - 20.0f, 20.0f }, 12.5f, true, axisInk(), Just::centredLeft, 0.02f);
        else
        {
            g.setColour(amberBright);
            g.fillEllipse(14.0f, 249.0f, 10.0f, 10.0f);
            text(g, "RANK I", { 28.0f, 244.0f, 62.0f, 20.0f }, 12.5f, true, axisInk(), Just::centredLeft);
            g.setColour(ivory.withAlpha(0.92f));
            g.fillEllipse(94.0f, 249.0f, 10.0f, 10.0f);
            text(g, "RANK II", { 108.0f, 244.0f, 68.0f, 20.0f }, 12.5f, true, axisInk(), Just::centredLeft);
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.drawEllipse(180.0f, 248.0f, 12.0f, 12.0f, 1.3f);
            text(g, "SOUNDING", { 196.0f, 244.0f, 90.0f, 20.0f }, 12.5f, true, axisInk(), Just::centredLeft);
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
    FxChain(Cs80AudioProcessor& p, std::shared_ptr<FxSel> selection) : LiveWidget(p, 1472.0f, 54.0f), sel(std::move(selection))
    {
        enableMouse(false);
    }

private:
    static constexpr float gap = 30.0f;
    static Rect nodeRect(int i)
    {
        const float nodeW = (1472.0f - 16.0f - gap * 5.0f) / 6.0f;
        return { 8.0f + (float) i * (nodeW + gap), 4.0f, nodeW, 46.0f };
    }

    void sample(Sig& s) const override
    {
        for (int i = 0; i < 3; ++i)
        {
            s[(size_t) i] = v(cs80::P_fx1Type + i * 2);
            s[(size_t) (3 + i)] = v(cs80::P_fx1Mix + i * 2);
        }
        s[6] = v(cs80::P_chorusOn) + 2.0f * v(cs80::P_chorusMode); s[8] = (float) policy() + 4.0f * v(cs80::P_outputMode); s[9] = (float) sel->slot;
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
        struct Node { juce::String name, detail; bool lit; bool masked; bool empty = false; };
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
            juce::String detail = type == 0 ? "NONE" : kFxTypes[type] + "  " + juce::String(juce::roundToInt(mix * 100.0f)) + "%";
            if (pol == 2)
                detail = "MASKED";
            nodes.push_back({ "FX " + juce::String(i + 1), detail, active && pol != 2, pol == 2, type == 0 && pol != 2 });
        }
        nodes.push_back({ "OUTPUT", juce::String(v(cs80::P_outputMode) >= 0.5f ? "STEREO" : "MONO") + (pol == 0 ? "  SOFT LIMIT" : "  RAW"),
                          true, false });

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
            text(g, node.name, { r.getX() + 30.0f, r.getY() + 4.0f, r.getWidth() - 36.0f, 18.0f }, 11.0f, true,
                 node.lit ? amberBright : ivoryDim, Just::centredLeft, 0.1f);
            text(g, node.detail, { r.getX() + 30.0f, r.getY() + 23.0f, r.getWidth() - 36.0f, 20.0f }, 12.0f, node.empty ? false : true,
                 node.lit ? ivory : (node.empty ? ivoryDim.withAlpha(0.55f) : ivoryDim.withAlpha(0.8f)), Just::centredLeft, 0.03f);
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
        : LiveWidget(p, 544.0f, 102.0f), slotIndex(slot), sel(std::move(selection))
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
        g.setColour(juce::Colours::black.withAlpha(selected ? 0.34f : 0.26f));
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
        // Badge in the same plastic as the signal-order chips: lit in cap yellow only while the slot works.
        const bool masked = policy() == 2;
        const bool working = type > 0 && mix > 0.001f && ! masked;
        const Rect badge(10.0f, 36.0f, 46.0f, 34.0f);
        capChip(g, badge, working ? capYellow : juce::Colour(0xff3a3731));
        text(g, "FX " + juce::String(slotIndex + 1), badge, 12.5f, true, working ? juce::Colour(0xff17120a) : ivoryDim, Just::centred, 0.05f);
        drawControlCaption(g, "EFFECT", { 70.0f, 8.0f, 170.0f, 17.0f }, Just::centred, 12.5f);

        // The summary column: type, mix and wet, then one fact about the effect. An empty slot says where it
        // sits in the series chain instead of repeating that it is off.
        if (type != 0)
        {
            static const char* const facts[] = { "", "0.62 Hz SWEEP", "6 STAGES, 0.32 Hz", "340 ms TAPS", "TAIL ABOUT 3 s (EST.)", "x3.2 SOFT CLIP" };
            text(g, kFxTypes[type], { 368.0f, 28.0f, 172.0f, 22.0f }, 14.0f, true, masked ? ivoryDim : ivory, Just::centredLeft, 0.06f);
            text(g, masked ? "MASKED BY ISOLATION"
                           : "MIX " + juce::String(juce::roundToInt(mix * 100.0f)) + "%   WET "
                                 + juce::String(juce::roundToInt(juce::jlimit(0.0f, 1.0f, mix * factor[type]) * 100.0f)) + "%",
                 { 368.0f, 52.0f, 172.0f, 20.0f }, 12.5f, true, masked ? amber : (mix > 0.001f ? amberBright : axisInk()), Just::centredLeft, 0.02f);
            text(g, mix <= 0.001f && ! masked ? juce::String("MIX 0: BYPASSED") : juce::String(facts[type]),
                 { 368.0f, 74.0f, 172.0f, 20.0f }, 12.5f, false, axisInk(), Just::centredLeft);
        }
        else
        {
            static const char* const after[] = { "AFTER THE CHORUS", "AFTER FX 1", "AFTER FX 2" };
            text(g, "EMPTY SLOT", { 368.0f, 36.0f, 172.0f, 22.0f }, 14.0f, true, axisInk(), Just::centredLeft, 0.06f);
            text(g, after[slotIndex], { 368.0f, 60.0f, 172.0f, 20.0f }, 12.5f, false, axisInk(), Just::centredLeft);
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
        : LiveWidget(p, 820.0f, 316.0f), sel(std::move(selection))
    {
        cs80::Rng rng(0x7a11u);
        for (auto& x : scatter)
            x = rng.nextUnipolar();
        enableMouse(false);
    }

private:
    static Rect plotRect() { return { 70.0f, 46.0f, 728.0f, 212.0f }; }
    // An empty slot lists the five effects as passive thumbnails; the type is chosen in the list at left.
    static Rect tileRect(int i) { return { 14.0f + (float) i * 158.0f, 78.0f, 148.0f, 200.0f }; }

    void drawTile(juce::Graphics& g, int i)
    {
        static const char* const names[5] = { "CHORUS", "PHASER", "TAPE DELAY", "PLATE REVERB", "DRIVE" };
        static const char* const notes[5] = { "13.5 - 16.3 ms sweep", "6 all-pass stages", "340 ms taps", "dense decaying tail", "x3.2 soft clip" };
        const Rect r = tileRect(i);
        const bool hot = hover == i;
        if (hot)
        {
            g.setColour(amber.withAlpha(0.12f));
            g.fillRoundedRectangle(r.expanded(4.0f), 7.0f);
        }
        g.setColour(juce::Colours::black.withAlpha(hot ? 0.28f : 0.45f));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(hot ? amber.withAlpha(0.85f) : juce::Colours::white.withAlpha(0.10f));
        g.drawRoundedRectangle(r, 5.0f, hot ? 1.6f : 1.0f);
        const Rect gl(r.getX(), r.getY(), r.getWidth(), 110.0f);
        juce::Path p;
        const juce::Colour ink = amber.withAlpha(0.7f);
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
                        const float x = gl.getX() + 10.0f + (gl.getWidth() - 20.0f) * t;
                        const float y = gl.getCentreY() - std::sin(two * 1.5f * t + (float) ch * 3.0f) * gl.getHeight() * 0.28f;
                        if (k == 0) q.startNewSubPath(x, y); else q.lineTo(x, y);
                    }
                    g.setColour(ch == 0 ? ink : ivory.withAlpha(0.4f));
                    g.strokePath(q, juce::PathStrokeType(1.6f));
                }
                break;
            case 1:
                for (int k = 0; k <= 60; ++k)
                {
                    const float t = (float) k / 60.0f;
                    const float notch = 1.0f - 0.75f * std::exp(-std::pow((t - 0.3f) / 0.07f, 2.0f)) - 0.75f * std::exp(-std::pow((t - 0.62f) / 0.09f, 2.0f));
                    const float x = gl.getX() + 10.0f + (gl.getWidth() - 20.0f) * t;
                    const float y = gl.getBottom() - 14.0f - (gl.getHeight() - 30.0f) * notch;
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
                    const float h = (gl.getHeight() - 24.0f) * amp;
                    g.setColour(k == 0 ? ivory.withAlpha(0.6f) : ink);
                    g.fillRoundedRectangle(gl.getX() + 18.0f + (float) k * 25.0f, gl.getBottom() - 12.0f - h, 8.0f, h, 1.5f);
                    amp *= 0.55f;
                }
                break;
            }
            case 3:
                g.setColour(ivory.withAlpha(0.6f));
                g.fillRect(gl.getX() + 14.0f, gl.getY() + 14.0f, 5.0f, gl.getHeight() - 26.0f);
                for (size_t k = 0; k < 30; ++k)
                {
                    const float t = (float) k / 30.0f;
                    const float h = (gl.getHeight() - 36.0f) * std::exp(-3.0f * t) * (0.45f + 0.55f * scatter[k]);
                    g.setColour(ink);
                    g.fillRect(gl.getX() + 28.0f + (gl.getWidth() - 44.0f) * t, gl.getBottom() - 12.0f - h, 2.0f, h);
                }
                break;
            default:
                for (int k = 0; k <= 60; ++k)
                {
                    const float x = -1.0f + 2.0f * (float) k / 60.0f;
                    const float y = cs80::softClip(x * 3.2f) * 0.68f;
                    const float px = gl.getCentreX() + x * (gl.getHeight() * 0.40f);
                    const float py = gl.getCentreY() - y * (gl.getHeight() * 0.40f);
                    if (k == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
                }
                g.setColour(ink);
                g.strokePath(p, juce::PathStrokeType(1.8f));
                break;
        }
        text(g, names[i], { r.getX(), r.getY() + 118.0f, r.getWidth(), 22.0f }, 12.5f, true, hot ? amberBright : ivory.withAlpha(0.9f), Just::centred, 0.04f);
        text(g, notes[i], { r.getX() + 2.0f, r.getY() + 142.0f, r.getWidth() - 4.0f, 20.0f }, 12.5f, false, axisInk(), Just::centred);
        text(g, hot ? "CLICK TO LOAD" : "", { r.getX(), r.getY() + 168.0f, r.getWidth(), 20.0f }, 12.5f, true, amber, Just::centred, 0.06f);
    }

    void sample(Sig& s) const override
    {
        const int slot = sel->slot;
        s[0] = v(cs80::P_fx1Type + slot * 2); s[1] = v(cs80::P_fx1Mix + slot * 2); s[2] = v(cs80::P_parityMode);
        s[3] = (float) slot; s[4] = (float) hover;
    }

    // The gallery of an empty slot is a row of buttons: a click loads that effect through the same
    // parameter the type list uses. Nothing else on this plot takes the mouse.
    int tileAt(Pt p) const
    {
        if (isolated() || juce::roundToInt(v(cs80::P_fx1Type + sel->slot * 2)) != 0)
            return -1;
        for (int i = 0; i < 5; ++i)
            if (tileRect(i).contains(p))
                return i;
        return -1;
    }
    void mouseMove(const juce::MouseEvent& e) override
    {
        const int h = tileAt(toDesign(e.getPosition()));
        if (h != hover)
        {
            hover = h;
            setMouseCursor(h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }
    void mouseExit(const juce::MouseEvent&) override
    {
        if (hover != -1)
        {
            hover = -1;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int h = tileAt(toDesign(e.getPosition()));
        if (h < 0)
            return;
        setParameterOnce(proc, cs80::P_fx1Type + sel->slot * 2, (float) (h + 1));
        hover = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        refresh();
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
            title = "FX " + juce::String(slot + 1);
            text(g, "CLICK AN EFFECT TO LOAD IT INTO FX " + juce::String(slot + 1) + ", OR PICK ONE IN THE LIST AT LEFT",
                 { 14.0f, 42.0f, 792.0f, 20.0f }, 12.5f, true, gold, Just::centredLeft, 0.1f);
            for (int i = 0; i < 5; ++i)
                drawTile(g, i);
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
                        axisText(g, juce::String(i * 5) + (i == 4 ? " ms" : ""), { 4.0f, plot.getBottom() - plot.getHeight() * (float) i / 4.0f - 6.5f, 56.0f, 14.0f },
                                 Just::centredRight);
                    axisText(g, "0", { plot.getX() - 12.0f, plot.getBottom() + 3.0f, 24.0f, 14.0f });
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
                        axisText(g, dbText[i], { 4.0f, plot.getY() + plot.getHeight() * (float) i / 3.0f - 6.5f, 56.0f, 14.0f }, Just::centredRight);
                    axisText(g, "20 Hz", { plot.getX() - 12.0f, plot.getBottom() + 3.0f, 50.0f, 14.0f }, Just::centredLeft);
                    axisText(g, "40 kHz", { plot.getRight() - 50.0f, plot.getBottom() + 3.0f, 50.0f, 14.0f }, Just::centredRight);
                    const float x1k = plot.getX() + plot.getWidth() * std::log(1000.0f / 20.0f) / std::log(40000.0f / 20.0f);
                    axisText(g, "1 kHz", { x1k - 25.0f, plot.getBottom() + 3.0f, 50.0f, 14.0f });
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
                    axisText(g, "DRY", { plot.getX() + step * 0.5f - 24.0f, plot.getBottom() + 3.0f, 48.0f, 14.0f });
                    float amp = wet;
                    for (int i = 1; i <= 5; ++i)
                    {
                        const float x = plot.getX() + step * (0.5f + (float) i);
                        stem(x, amp, i % 2 == 1 ? trace : trace.withAlpha(0.6f));
                        axisText(g, periodString(0.34f * (float) i), { x - 30.0f, plot.getBottom() + 3.0f, 60.0f, 14.0f });
                        amp *= 0.36f;
                    }
                    axisText(g, "1", { 4.0f, plot.getY() - 6.0f, 56.0f, 14.0f }, Just::centredRight);
                    axisText(g, "0", { 4.0f, plot.getBottom() - 6.0f, 56.0f, 14.0f }, Just::centredRight);
                    foot = "340 ms TAPS, EACH x0.36 OF THE LAST, SIDE TO SIDE";
                    break;
                }
                case 4: // plate reverb: the dry stem and the tail on a decibel axis, 0 to -60 dB over 5 s
                {
                    frame(5, 3);
                    const auto tail = plateTail();
                    constexpr float span = 5.0f, floorDb = -60.0f;
                    const float shownWet = wet > 0.001f ? wet : 0.3f;
                    auto yOfDb = [&](float db) { return plot.getY() + plot.getHeight() * juce::jlimit(0.0f, 1.0f, db / floorDb); };
                    // The dry signal at the onset, then the tail: loop envelope, a little density texture.
                    const float dryDb = 20.0f * std::log10(juce::jmax(0.001f, 1.0f - shownWet));
                    g.setColour(ivory.withAlpha(active ? 0.9f : 0.4f));
                    g.fillRoundedRectangle(plot.getX() + 6.0f, yOfDb(dryDb), 10.0f, plot.getBottom() - yOfDb(dryDb), 2.0f);
                    const float x0 = plot.getX() + 24.0f, x1 = plot.getRight() - 6.0f;
                    for (size_t i = 0; i < scatter.size(); ++i)
                    {
                        const float u = (float) i / (float) scatter.size();
                        const float t = span * (x0 - plot.getX() + (x1 - x0) * u) / plot.getWidth();
                        const float db = 20.0f * std::log10(juce::jmax(1.0e-4f, shownWet * tail.envelope(t))) - (1.0f - scatter[i]) * 5.0f;
                        const float y = yOfDb(db);
                        g.setColour(i % 2 ? trace : trace.withAlpha(0.6f));
                        g.fillRect(x0 + (x1 - x0) * u, y, 3.0f, juce::jmax(1.0f, plot.getBottom() - y));
                    }
                    // Where the loops' mean low-band decay reaches -60 dB below the onset.
                    const float xr = plot.getX() + plot.getWidth() * tail.mean / span;
                    g.setColour(ivory.withAlpha(0.5f));
                    const float dash[] = { 3.0f, 3.0f };
                    g.drawDashedLine(juce::Line<float>(xr, plot.getY(), xr, plot.getBottom()), dash, 2, 1.0f);
                    axisText(g, "RT60 ABOUT " + juce::String(tail.mean, 1) + " s (EST.)", { xr + 6.0f, plot.getY() + 2.0f, 200.0f, 16.0f }, Just::centredLeft);
                    for (int i = 0; i <= 3; ++i)
                        axisText(g, i == 0 ? juce::String("0 dB") : juce::String(-20 * i),
                                 { 4.0f, plot.getY() + plot.getHeight() * (float) i / 3.0f - 6.5f, 56.0f, 14.0f }, Just::centredRight);
                    for (int i = 0; i <= 5; ++i)
                        axisText(g, i == 0 ? juce::String("0") : juce::String(i) + (i == 5 ? " s" : ""),
                                 { plot.getX() + plot.getWidth() * (float) i / 5.0f - 20.0f - (i == 5 ? 12.0f : 0.0f), plot.getBottom() + 3.0f, 40.0f, 14.0f });
                    foot = "ESTIMATED FROM THE FOUR COMB LOOPS (" + juce::String(juce::roundToInt(1000.0f * tail.delay[0])) + " - "
                           + juce::String(juce::roundToInt(1000.0f * tail.delay[3])) + " ms, FEEDBACK 0.78): RT60 " + juce::String(tail.lo, 1) + " - "
                           + juce::String(tail.hi, 1) + " s IN THE LOW BAND";
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
                    axisText(g, "+1", { box.getX() - 40.0f, box.getY() - 6.0f, 36.0f, 14.0f }, Just::centredRight);
                    axisText(g, "-1", { box.getX() - 40.0f, box.getBottom() - 6.0f, 36.0f, 14.0f }, Just::centredRight);
                    axisText(g, "OUT", { box.getX() - 40.0f, box.getCentreY() - 6.0f, 36.0f, 14.0f }, Just::centredRight);
                    axisText(g, "-1", { box.getX() - 12.0f, box.getBottom() + 3.0f, 24.0f, 14.0f });
                    axisText(g, "INPUT", { box.getCentreX() - 25.0f, box.getBottom() + 3.0f, 50.0f, 14.0f });
                    axisText(g, "+1", { box.getRight() - 12.0f, box.getBottom() + 3.0f, 24.0f, 14.0f });
                    foot = "MIX 0 = DIAGONAL, MIX 1 = x3.2 SOFT CLIP WITH OUTPUT x0.68";
                    break;
                }
            }
        }
        text(g, title, { 14.0f, 8.0f, 360.0f, 24.0f }, 15.0f, true, type == 0 ? ivoryDim : amber, Just::centredLeft, 0.08f);
        text(g, foot, { 14.0f, 292.0f, 792.0f, 20.0f }, 12.5f, false, axisInk(), Just::centredLeft, 0.0f);
        if (type > 0)
        {
            // Only the type and the mix are engine parameters; every curve here is drawn from the fixed law.
            const juce::String tag = "ILLUSTRATION: SHAPE ONLY";
            const float tw = textWidth(tag, 12.5f, true, 0.06f) + 22.0f;
            const Rect chip = Rect(0.0f, 0.0f, tw, 22.0f).withCentre({ 520.0f, 20.0f });
            g.setColour(ivory.withAlpha(0.07f));
            g.fillRoundedRectangle(chip, 11.0f);
            g.setColour(ivory.withAlpha(0.38f));
            g.drawRoundedRectangle(chip, 11.0f, 1.0f);
            text(g, tag, chip, 12.5f, true, axisInk(), Just::centred, 0.06f);
        }
        if (type > 0 && ! iso)
            text(g, "WET " + juce::String(juce::roundToInt(wet * 100.0f)) + "%", { 640.0f, 8.0f, 166.0f, 24.0f }, 15.0f, true, ivory,
                 Just::centredRight, 0.04f);
        if (iso)
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }

    std::shared_ptr<FxSel> sel;
    std::array<float, 110> scatter {};
    int hover = -1;
};

// ---- Macro destinations: a flat bipolar bar and three equal destination rows ---------------------
// An idle macro shows the scale it can reach; a moved one shows what it is doing right now.
class MacroBars final : public LiveWidget
{
public:
    MacroBars(Cs80AudioProcessor& p, int index) : LiveWidget(p, 268.0f, 112.0f), m(index) {}
    void paintStatic(juce::Graphics&) override {}

private:
    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_macro1 + m);
        s[1] = v(cs80::P_parityMode);
        s[2] = m == 0 ? v(cs80::P_brilliance) : 0.0f;
    }

    void draw(juce::Graphics& g) override
    {
        const bool iso = isolated();
        const float x = iso ? 0.0f : (v(cs80::P_macro1 + m) - 0.5f) * 2.0f;
        const bool lit = std::abs(x) > 0.01f;
        // `fact` rows are not a destination of the macro: they carry something true about the column that is worth
        // showing beside it, so every column has three rows. A fact always shows its value; a `note` spans the row.
        struct Row { const char* name; juce::String value; juce::String scale; bool fact = false; bool note = false; };
        std::vector<Row> rows;
        auto pct = [&](float k) { return signedString(x * k * 100.0f, 0, "%"); };
        auto range = [&](float k) { return plusMinus() + juce::String(juce::roundToInt(k * 100.0f)) + "%"; };
        switch (m)
        {
            case 0:
            {
                // The macro only adds to the BRILL slider on the main panel; the total is clamped at 3 octaves.
                const float panel = v(cs80::P_brilliance);
                const float total = juce::jlimit(-3.0f, 3.0f, panel + x * 0.75f);
                rows = { { "BRILLIANCE", signedString(x * 0.75f, 2, " oct"), plusMinus() + "0.75 oct" },
                         { "PANEL BRILL", signedString(panel, 2, " oct"), {}, true },
                         { "SOUNDING", signedString(total, 2, " oct"), {}, true } };
                break;
            }
            case 1: rows = { { "ENVELOPE TIMES", "x" + juce::String(std::pow(2.0f, -x * 1.1f), 2), "x0.47 - x2.14" },
                             { "LFO DEPTH", signedString(juce::jmax(0.0f, x) * 18.0f, 0, "%"), "0 - +18%" },
                             { "LFO DEPTH: ABOVE CENTRE ONLY", {}, {}, false, true } }; break;
            case 2: rows = { { "DETUNE II", signedString(x * 12.0f, 1, " ct"), plusMinus() + "12.0 ct" },
                             { "DISPERSION", pct(0.22f), range(0.22f) }, { "CHORUS DEPTH", pct(0.25f), range(0.25f) } }; break;
            default: rows = { { "RING MOD", pct(0.22f), range(0.22f) }, { "SUB > VCF", pct(0.20f), range(0.20f) },
                              { "SUB > VCA", pct(0.12f), range(0.12f) } }; break;
        }
        // Flat bipolar bar: centre is neutral.
        const Rect track(4.0f, 6.0f, designW - 8.0f, 11.0f);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillRoundedRectangle(track, 5.0f);
        const float mid = track.getCentreX();
        const float half = track.getWidth() * 0.5f - 2.0f;
        g.setColour(lit ? amber : ivoryDim.withAlpha(0.4f));
        g.fillRect(x >= 0.0f ? mid : mid + x * half, track.getY() + 2.0f, juce::jmax(1.5f, std::abs(x) * half), track.getHeight() - 4.0f);
        g.setColour(ivory.withAlpha(0.7f));
        g.fillRect(mid - 0.75f, track.getY() - 2.0f, 1.5f, track.getHeight() + 4.0f);
        for (int i = 0; i < 3; ++i)
        {
            const float y = 24.0f + (float) i * 29.0f;
            if (i >= (int) rows.size())
            {
                g.setColour(ivory.withAlpha(0.06f));
                g.fillRect(4.0f, y + 14.0f, designW - 8.0f, 1.0f);
                continue;
            }
            const auto& row = rows[(size_t) i];
            if (row.note)
            {
                text(g, row.name, { 4.0f, y, designW - 8.0f, 24.0f }, 12.5f, true, panelInk(), Just::centredLeft, 0.02f);
                g.setColour(ivory.withAlpha(0.10f));
                g.fillRect(4.0f, y + 27.0f, designW - 8.0f, 1.0f);
                continue;
            }
            text(g, row.name, { 4.0f, y, 150.0f, 24.0f }, 12.5f, true, lit || row.fact ? ivory : panelInk(), Just::centredLeft, 0.04f);
            text(g, lit || row.fact ? row.value : row.scale, { 120.0f, y, 144.0f, 24.0f }, 12.5f, lit || row.fact,
                 lit ? amberBright : (row.fact ? ivory : axisInk()), Just::centredRight, 0.02f);
            g.setColour(ivory.withAlpha(0.10f));
            g.fillRect(4.0f, y + 27.0f, designW - 8.0f, 1.0f);
        }
    }

    int m;
};

// ---- A caption that can be dimmed together with the control under it --------------------------------
class CaptionWidget final : public LiveWidget
{
public:
    CaptionWidget(Cs80AudioProcessor& p, float w, float h, juce::String t, Just j, float size)
        : LiveWidget(p, w, h), label(std::move(t)), just(j), fontSize(size) {}
    void paintStatic(juce::Graphics&) override {}

private:
    void sample(Sig&) const override {}
    void draw(juce::Graphics& g) override { drawControlCaption(g, label, { 0.0f, 0.0f, designW, designH }, just, fontSize); }

    juce::String label;
    Just just;
    float fontSize;
};

// ---- Output channels: stereo or mono, and what the output stage does to the signal ------------------
class OutputPanel final : public LiveWidget
{
public:
    explicit OutputPanel(Cs80AudioProcessor& p) : LiveWidget(p, 256.0f, 200.0f) {}
    void paintStatic(juce::Graphics&) override {}

private:
    void sample(Sig& s) const override { s[0] = v(cs80::P_outputMode); s[1] = (float) policy(); }

    void draw(juce::Graphics& g) override
    {
        const bool stereo = v(cs80::P_outputMode) >= 0.5f;
        const int pol = policy();
        // Two channel lamps run to the output socket; in mono they join first.
        const Rect well(0.0f, 0.0f, designW, 82.0f);
        glassWell(g, well, 5.0f);
        const Pt l(40.0f, 22.0f), r(40.0f, 60.0f), socket(206.0f, 41.0f);
        g.setColour(amberBright.withAlpha(0.85f));
        juce::Path p;
        if (stereo)
        {
            p.startNewSubPath(l.x + 12.0f, l.y); p.lineTo(socket.x - 12.0f, l.y); p.lineTo(socket.x - 6.0f, socket.y - 5.0f);
            p.startNewSubPath(r.x + 12.0f, r.y); p.lineTo(socket.x - 12.0f, r.y); p.lineTo(socket.x - 6.0f, socket.y + 5.0f);
        }
        else
        {
            p.startNewSubPath(l.x + 12.0f, l.y); p.lineTo(110.0f, l.y); p.lineTo(130.0f, socket.y);
            p.startNewSubPath(r.x + 12.0f, r.y); p.lineTo(110.0f, r.y); p.lineTo(130.0f, socket.y);
            p.startNewSubPath(130.0f, socket.y); p.lineTo(socket.x - 12.0f, socket.y);
        }
        g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        jewel(g, l, 7.0f, amber, true);
        jewel(g, r, 7.0f, amber, true);
        text(g, "L", { 8.0f, l.y - 9.0f, 18.0f, 18.0f }, 12.0f, true, ivory, Just::centred);
        text(g, "R", { 8.0f, r.y - 9.0f, 18.0f, 18.0f }, 12.0f, true, ivory, Just::centred);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillRoundedRectangle(socket.x - 14.0f, socket.y - 14.0f, 40.0f, 28.0f, 4.0f);
        g.setColour(amber.withAlpha(0.7f));
        g.drawRoundedRectangle(socket.x - 14.0f, socket.y - 14.0f, 40.0f, 28.0f, 4.0f, 1.2f);
        text(g, stereo ? "L R" : "MONO", { socket.x - 14.0f, socket.y - 14.0f, 40.0f, 28.0f }, 11.0f, true, amberBright, Just::centred);

        text(g, "OUTPUT STAGE", { 2.0f, 92.0f, 250.0f, 18.0f }, 11.0f, true, gold, Just::centredLeft, 0.14f);
        text(g, pol == 0 ? "DC BLOCK + SOFT LIMIT" : "RAW SOURCE OUTPUT", { 2.0f, 112.0f, 250.0f, 22.0f }, 14.0f, true, ivory, Just::centredLeft, 0.03f);
        text(g, stereo ? "LEFT AND RIGHT KEPT SEPARATE" : "L AND R AVERAGED AFTER THE EFFECTS", { 2.0f, 136.0f, 250.0f, 18.0f }, 11.0f, false,
             axisInk(), Just::centredLeft);
        text(g, "ENGINE RUNS AT 2x OVERSAMPLING", { 2.0f, 154.0f, 250.0f, 18.0f }, 11.0f, false, axisInk(), Just::centredLeft);
        text(g, pol == 0 ? "OVERLOAD SHOWS ON THE OVR LAMP" : "CAN CLIP: KEEP HEADROOM", { 2.0f, 174.0f, 250.0f, 20.0f }, 11.0f, true,
             pol == 0 ? axisInk() : amberBright, Just::centredLeft, 0.02f);
    }
};

// ---- Arpeggiator clock ----------------------------------------------------------------------------
class ArpClock final : public LiveWidget
{
public:
    explicit ArpClock(Cs80AudioProcessor& p) : LiveWidget(p, 152.0f, 112.0f) {}

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
        auto row = [&](float y, const juce::String& k, const juce::String& val, juce::Colour ink)
        {
            text(g, k, { 12.0f, y, 60.0f, 20.0f }, 12.5f, true, ivoryDim, Just::centredLeft, 0.1f);
            text(g, val, { 60.0f, y, 80.0f, 20.0f }, 13.0f, true, ink, Just::centredRight);
        };
        const juce::Colour lit = on ? amberBright : ivory.withAlpha(0.7f);
        row(8.0f, "STEP", periodString(1.0f / rate), lit);
        row(36.0f, "CLOCK", juce::String(rate, rate < 10.0f ? 2 : 1) + "/s", lit);
        // The gate length is set by the mode, not by a control: it is shown in neutral ink and says so.
        row(64.0f, "GATE", juce::String(random ? 45 : 58) + "%", ivory.withAlpha(0.7f));
        text(g, "(FIXED)", { 60.0f, 84.0f, 80.0f, 20.0f }, 12.5f, true, axisInk(), Just::centredRight, 0.04f);
    }
};

// ---- Arpeggio pattern: pitch gutter, step numbers and gate bars ------------------------------------
// With the arpeggiator off the same bars are drawn as a dashed preview, so the panel always shows what
// the mode and octave settings will play.
class ArpPattern final : public LiveWidget
{
public:
    explicit ArpPattern(Cs80AudioProcessor& p) : LiveWidget(p, 712.0f, 250.0f) {}

private:
    static Rect plotRect() { return { 64.0f, 32.0f, 634.0f, 184.0f }; }

    // The keys held right now: on-screen keys and host MIDI, lowest first. `more` counts the ones beyond eight.
    std::vector<int> heldNotes(int& more) const
    {
        std::vector<int> notes;
        more = 0;
        for (int n = 0; n < 128; ++n)
            if (proc.isHostKeyDown(n) || proc.getKeyboardState().isNoteOnForChannels(0xffff, n))
            {
                if (notes.size() < 8) notes.push_back(n); else ++more;
            }
        return notes;
    }

    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_arpMode); s[1] = v(cs80::P_arpOctaves); s[2] = v(cs80::P_arpOn); s[3] = v(cs80::P_parityMode);
        // The held keys as eight 16-bit words, exact in a float.
        std::array<unsigned, 8> words {};
        for (int n = 0; n < 128; ++n)
            if (proc.isHostKeyDown(n) || proc.getKeyboardState().isNoteOnForChannels(0xffff, n))
                words[(size_t) (n / 16)] |= 1u << (n % 16);
        for (size_t i = 0; i < 8; ++i)
            s[4 + i] = (float) words[i];
    }

    void draw(juce::Graphics& g) override
    {
        const int mode = juce::jlimit(0, 4, juce::roundToInt(v(cs80::P_arpMode)));
        const int octaves = juce::jlimit(1, 4, juce::roundToInt(v(cs80::P_arpOctaves)));
        const bool armed = v(cs80::P_arpOn) >= 0.5f && ! isolated();
        // The chord: the keys held now, else a demonstration chord pressed E, G, C.
        int more = 0;
        std::vector<int> base = heldNotes(more);
        const bool liveKeys = ! base.empty();
        if (! liveKeys)
            base = { 64, 67, 60 };
        // Same ordering rules as Engine::rebuildArpSequence. Press order is not tracked, so PLAYED shows ascending.
        if (mode != 4 || liveKeys)
            std::sort(base.begin(), base.end());
        const bool on = armed && liveKeys;
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
        const int lowest = *std::min_element(seq.begin(), seq.end()), highest = *std::max_element(seq.begin(), seq.end());
        const float rowH = plot.getHeight() / (float) (highest - lowest + 1);
        const float stepW = plot.getWidth() / (float) seq.size();

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

        // Pitch gutter: the notes the pattern uses, thinned so the labels never touch.
        std::vector<int> used;
        for (int o = 0; o < octaves; ++o)
            for (int n : base)
                used.push_back(n + 12 * o);
        std::sort(used.begin(), used.end());
        used.erase(std::unique(used.begin(), used.end()), used.end());
        float lastY = 1.0e9f;
        for (int n : used)
        {
            const float y = plot.getBottom() - ((float) (n - lowest) + 0.5f) * rowH;
            if (lastY - y < 15.0f)
                continue;
            lastY = y;
            axisText(g, noteName(n), { 6.0f, y - 8.0f, 50.0f, 16.0f }, Just::centredRight);
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
                g.setColour(on ? amber.withAlpha(0.28f) : ivoryDim.withAlpha(0.12f));
                g.fillRoundedRectangle(bar.withWidth(stepW * 0.86f), 1.5f);
                if (! on)
                {
                    juce::Path outline, dashed;
                    outline.addRoundedRectangle(bar.withWidth(stepW * 0.86f), 1.5f);
                    const float dash[] = { 4.0f, 3.0f };
                    juce::PathStrokeType(1.1f).createDashedStroke(dashed, outline, dash, 2);
                    g.setColour(ivory.withAlpha(0.5f));
                    g.fillPath(dashed);
                }
            }
            else if (on)
            {
                g.setColour(amberBright.withAlpha(0.22f));
                g.fillRoundedRectangle(bar.expanded(1.8f), 2.5f);
                g.setGradientFill(juce::ColourGradient(amberBright, bar.getX(), bar.getY(), amber.darker(0.2f), bar.getX(), bar.getBottom(), false));
                g.fillRoundedRectangle(bar, 1.8f);
            }
            else
            {
                // Preview: dashed outline over a faint fill.
                g.setColour(ivory.withAlpha(0.08f));
                g.fillRoundedRectangle(bar, 1.8f);
                juce::Path outline, dashed;
                outline.addRoundedRectangle(bar.reduced(0.5f), 1.8f);
                const float dash[] = { 4.0f, 3.0f };
                juce::PathStrokeType(1.2f).createDashedStroke(dashed, outline, dash, 2);
                g.setColour(ivory.withAlpha(0.6f));
                g.fillPath(dashed);
            }
            // Step number: one per step, centred under its gate bar.
            if (stepW > 15.0f)
                axisText(g, juce::String((int) i + 1), { plot.getX() + stepW * (float) i, plot.getBottom() + 3.0f, stepW, 16.0f });
        }
        if (random)
            text(g, "RANDOM STEP FROM THE HELD NOTES, ALL OCTAVES", plot, 12.5f, true, on ? amberBright : axisInk(), Just::centred, 0.08f);

        juce::String names;
        for (int n : base)
            names += (names.isEmpty() ? "" : " ") + noteName(n);
        juce::String head = liveKeys ? "HELD KEYS  " + names + (more > 0 ? " +" + juce::String(more) : juce::String())
                                     : "DEMO CHORD  " + names + "  -  HOLD KEYS FOR YOURS";
        if (! armed)
            head = "ARP OFF  -  PREVIEW  -  " + head;
        text(g, head, { 14.0f, 5.0f, 476.0f, 22.0f }, 12.5f, true, on ? amber : axisInk(), Just::centredLeft, 0.02f);
        text(g, kArpModes[mode] + "  -  " + juce::String(octaves) + " OCT  -  " + juce::String((int) seq.size()) + " STEPS",
             { 400.0f, 5.0f, 298.0f, 22.0f }, 12.5f, true, on ? ivory : axisInk(), Just::centredRight, 0.03f);
        text(g, "STEP", { 4.0f, plot.getBottom() + 3.0f, 56.0f, 16.0f }, 12.5f, true, axisInk(), Just::centredRight);
        if (mode == 4 && liveKeys)
            text(g, "PLAYED ORDER IS NOT TRACKED  -  SHOWN ASCENDING", { plot.getX() + 10.0f, plot.getY() + 4.0f, 360.0f, 20.0f }, 12.5f, true,
                 axisInk(), Just::centredLeft, 0.02f);
        if (isolated())
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }
};

// ---- Glide curve ------------------------------------------------------------------------------------
// Portamento and glissando are set on the main panel (PORTA and GLIDE); this plot shows the pitch path
// they give for a one-octave leap. With glide off the real jump is drawn solid and the glide the last
// non-zero time would give is drawn as a dashed preview.
class GlideCurve final : public LiveWidget
{
public:
    explicit GlideCurve(Cs80AudioProcessor& p) : LiveWidget(p, 648.0f, 208.0f) {}

    void paintStatic(juce::Graphics& g) override
    {
        glassWell(g, { 0.0f, 0.0f, designW, designH });
        const auto plot = plotRect();
        for (int i = 0; i <= 4; ++i)
        {
            const float y = plot.getBottom() - plot.getHeight() * (float) i / 4.0f;
            hline(g, plot.getX(), plot.getRight(), y, i == 0 ? 0.34f : 0.15f);
            axisText(g, juce::String(i * 3) + (i == 4 ? " st" : ""), { 0.0f, y - 8.0f, 54.0f, 16.0f }, Just::centredRight);
        }
        vline(g, plot.getX(), plot.getY(), plot.getBottom(), 0.34f);
    }

private:
    static Rect plotRect() { return { 60.0f, 32.0f, 572.0f, 134.0f }; }

    void sample(Sig& s) const override { s[0] = v(cs80::P_portamento); s[1] = v(cs80::P_glideMode); }

    void draw(juce::Graphics& g) override
    {
        const float tau = v(cs80::P_portamento);
        const int mode = juce::jlimit(0, 2, juce::roundToInt(v(cs80::P_glideMode)));
        if (tau > 0.0001f)
            lastTau = tau;
        const auto plot = plotRect();
        const bool gliding = tau > 0.0001f && mode != 0;
        // The curve and axis always describe a real time: the set one, else the last one used, else 300 ms.
        const float showTau = tau > 0.0001f ? tau : (lastTau > 0.0001f ? lastTau : 0.3f);
        auto xOfT = [&](float t01) { return plot.getX() + plot.getWidth() * t01; };

        // Time axis in time-constant steps: 1 tau reaches 63%, 3 tau 95%.
        const juce::Colour axisCol = gliding ? axisInk() : axisInk().withAlpha(0.75f);
        for (int i = 0; i <= 5; ++i)
        {
            vline(g, xOfT((float) i / 5.0f), plot.getY(), plot.getBottom(), i == 0 ? 0.34f : 0.13f);
            // The last label hangs from the plot's right edge so it stays inside the glass.
            const float shift = i == 5 ? -22.0f : 0.0f;
            text(g, i == 0 ? "0" : periodString(showTau * (float) i), { xOfT((float) i / 5.0f) - 36.0f + shift, plot.getBottom() + 3.0f, 72.0f, 16.0f },
                 12.5f, false, axisCol, Just::centred);
            if (i > 0)
                text(g, juce::String(i) + juce::String(juce::CharPointer_UTF8("\xcf\x84")),
                     { xOfT((float) i / 5.0f) - 20.0f + shift, plot.getBottom() + 19.0f, 40.0f, 16.0f }, 12.5f, false, axisCol, Just::centred);
        }
        text(g, "TIME", { 0.0f, plot.getBottom() + 3.0f, 44.0f, 16.0f }, 12.5f, true, axisInk(), Just::centredRight);
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
                float semis = 12.0f * (1.0f - std::exp(-5.0f * t01));
                if (stepped)
                    semis = std::round(semis);
                const float x = xOfT(t01);
                const float y = plot.getBottom() - plot.getHeight() * semis / 12.0f;
                if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
            }
            return path;
        };
        // Dashed reference: the smooth exponential (glissando steps lie on it), or the glide that is switched off.
        {
            juce::Path dashed;
            const float dash[] = { 5.0f, 4.0f };
            juce::PathStrokeType(1.5f).createDashedStroke(dashed, expCurve(false), dash, 2);
            g.setColour(ivory.withAlpha(gliding && mode == 2 ? 0.6f : (gliding ? 0.0f : 0.55f)));
            g.fillPath(dashed);
        }
        if (gliding)
        {
            const auto path = expCurve(mode == 2);
            g.setColour(amberBright.withAlpha(0.28f));
            g.strokePath(path, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(amberBright);
            g.strokePath(path, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const float x63 = xOfT(1.0f / 5.0f);
            g.setColour(ivory.withAlpha(0.4f));
            const float dash[] = { 3.0f, 3.0f };
            g.drawDashedLine(juce::Line<float>(x63, plot.getBottom(), x63, plot.getBottom() - plot.getHeight() * 0.632f), dash, 2, 1.0f);
        }
        else
        {
            // The real behaviour: the pitch jumps at once.
            juce::Path jump;
            jump.startNewSubPath(plot.getX(), plot.getBottom());
            jump.lineTo(plot.getX() + 1.0f, plot.getY());
            jump.lineTo(plot.getRight(), plot.getY());
            g.setColour(ivoryDim);
            g.strokePath(jump, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        text(g, "ONE-OCTAVE LEAP", { 14.0f, 5.0f, 150.0f, 22.0f }, 12.5f, true, axisInk(), Just::centredLeft, 0.03f);
        juce::String right;
        if (mode == 0) right = "GLIDE OFF  -  PITCH JUMPS  -  DASHED: " + periodString(showTau);
        else if (tau <= 0.0001f) right = "PORTA 0  -  PITCH JUMPS  -  DASHED: " + periodString(showTau);
        else right = mode == 2 ? "GLISSANDO  -  SEMITONE STEPS  -  DASHED: SMOOTH" : "PORTAMENTO  -  TIME CONSTANT = PORTA";
        text(g, right, { 160.0f, 5.0f, 478.0f, 22.0f }, 12.5f, true, gliding ? amber : axisInk(), Just::centredRight, 0.02f);
    }

    float lastTau = 0.0f;
};

// ---- Key release: the VCA level after a key is let go, for both ranks -----------------------------------
// The engine uses the longer of a rank's own release and the KEY RELEASE floor, scaled by the MOTION macro.
class ReleaseCurve final : public LiveWidget
{
public:
    explicit ReleaseCurve(Cs80AudioProcessor& p) : LiveWidget(p, 490.0f, 168.0f) {}

private:
    static Rect plotRect() { return { 56.0f, 34.0f, 416.0f, 82.0f }; }

    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_keyRelease); s[1] = v(cs80::chParam(0, cs80::CP_vR)); s[2] = v(cs80::chParam(1, cs80::CP_vR));
        s[3] = v(cs80::P_macro2); s[4] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const float floorT = v(cs80::P_keyRelease);
        const float motion = isolated() ? 0.0f : (v(cs80::P_macro2) - 0.5f) * 2.0f;
        const float scale = std::pow(2.0f, -motion * 1.1f);
        const float r1 = juce::jmax(v(cs80::chParam(0, cs80::CP_vR)), floorT) * scale;
        const float r2 = juce::jmax(v(cs80::chParam(1, cs80::CP_vR)), floorT) * scale;
        const float longest = juce::jmax(0.05f, juce::jmax(r1, r2));
        const float span = longest * 1.15f;
        const auto plot = plotRect();
        auto xOf = [&](float t) { return plot.getX() + plot.getWidth() * t / span; };

        hline(g, plot.getX(), plot.getRight(), plot.getBottom(), 0.34f);
        hline(g, plot.getX(), plot.getRight(), plot.getY(), 0.15f);
        vline(g, plot.getX(), plot.getY(), plot.getBottom(), 0.34f);
        axisText(g, "full", { 0.0f, plot.getY() - 8.0f, 52.0f, 16.0f }, Just::centredRight);
        axisText(g, "0", { 0.0f, plot.getBottom() - 8.0f, 52.0f, 16.0f }, Just::centredRight);
        axisText(g, "KEY UP", { plot.getX() - 12.0f, plot.getBottom() + 3.0f, 64.0f, 16.0f }, Just::centredLeft);
        axisText(g, periodString(span), { plot.getRight() - 70.0f, plot.getBottom() + 3.0f, 70.0f, 16.0f }, Just::centredRight);
        if (floorT > 0.001f)
        {
            const float fx = xOf(floorT * scale);
            g.setColour(ivory.withAlpha(0.45f));
            const float dash[] = { 3.0f, 3.0f };
            g.drawDashedLine(juce::Line<float>(fx, plot.getY(), fx, plot.getBottom()), dash, 2, 1.0f);
        }
        auto trace = [&](float r)
        {
            juce::Path path;
            for (int i = 0; i <= 200; ++i)
            {
                const float t = span * (float) i / 200.0f;
                const float x = xOf(t);
                const float y = plot.getBottom() - plot.getHeight() * std::exp(-6.0f * t / juce::jmax(0.001f, r));
                if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
            }
            return path;
        };
        g.setColour(ivory.withAlpha(0.55f));
        g.strokePath(trace(r2), juce::PathStrokeType(1.4f));
        const auto main = trace(r1);
        g.setColour(amberBright.withAlpha(0.28f));
        g.strokePath(main, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(amberBright);
        g.strokePath(main, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        text(g, "VCA LEVEL AFTER KEY UP", { 14.0f, 5.0f, 230.0f, 22.0f }, 12.5f, true, axisInk(), Just::centredLeft, 0.03f);
        text(g, "RANK I " + periodString(r1) + "    RANK II " + periodString(r2), { 240.0f, 5.0f, 242.0f, 22.0f }, 12.5f, true, amberBright,
             Just::centredRight, 0.02f);
        text(g, floorT > 0.001f ? "DASHED: KEY RELEASE FLOOR " + periodString(floorT * scale) + "  -  THE LONGER WINS"
                                : juce::String("KEY RELEASE OFF  -  EACH RANK USES ITS OWN RELEASE"),
             { 14.0f, 142.0f, 474.0f, 20.0f }, 12.5f, false, axisInk(), Just::centredLeft, 0.01f);
    }
};

// ---- What each render policy changes: a table with the effective column lit ---------------------------
class PolicyTable final : public LiveWidget
{
public:
    explicit PolicyTable(Cs80AudioProcessor& p) : LiveWidget(p, 1440.0f, 184.0f) {}
    void paintStatic(juce::Graphics&) override {}

private:
    static constexpr float labelW = 312.0f, rowH = 20.0f, pad = 20.0f;

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
            { "PRESSURE, RIBBON, SCREEN PEDAL", { "LIVE", "LIVE", "LIVE" }, { 1, 1, 1 } },
            { "SUB OSCILLATOR", { "STORED MODE", "STORED MODE", "ORIGINAL, FORCED" }, { 0, 0, 0 } },
            { "CHORUS", { "GENERIC DELAY", "OE2 BBD", "OE2 BBD" }, { 0, 0, 0 } },
            { "OUTPUT", { "DC BLOCK + SOFT LIMIT", "RAW, KEEP HEADROOM", "RAW, KEEP HEADROOM" }, { 0, 0, 0 } },
            { "CALIBRATION", { "STUDY, NOT A MATCH", "NOT CALIBRATED", "NOT CALIBRATED" }, { 0, 0, 0 } },
        };
        // The column of the policy in force is banded; the switch above it is the column head.
        const Rect band(labelW + colW * (float) pol, 0.0f, colW, designH);
        g.setGradientFill(juce::ColourGradient(amber.withAlpha(0.16f), band.getX(), 0.0f, amber.withAlpha(0.06f), band.getX(), designH, false));
        g.fillRoundedRectangle(band.reduced(2.0f, 0.0f), 4.0f);
        g.setColour(amber.withAlpha(0.55f));
        g.drawRoundedRectangle(band.reduced(2.0f, 0.5f), 4.0f, 1.0f);
        for (size_t r = 0; r < std::size(rows); ++r)
        {
            const float y = 2.0f + (float) r * rowH;
            if (r % 2 == 1)
            {
                g.setColour(juce::Colours::white.withAlpha(0.03f));
                g.fillRect(0.0f, y, designW, rowH);
            }
            text(g, rows[r].label, { 8.0f, y, labelW - 12.0f, rowH }, 12.5f, true, ivory, Just::centredLeft, 0.02f);
            for (int c = 0; c < 3; ++c)
            {
                const int kind = rows[r].kind[c];
                const juce::Colour ink = kind == 1 ? amberBright : (kind == 2 ? panelInk() : (kind == 3 ? amberBright.withMultipliedBrightness(0.85f) : (c == pol ? ivory : panelInk())));
                text(g, rows[r].cell[c], { labelW + colW * (float) c + pad, y, colW - pad - 6.0f, rowH }, 12.5f, kind != 0, ink, Just::centredLeft, 0.02f);
            }
        }
    }
};

} // namespace

// ---------------------------------------------------------------------------
// Policy switch cell: one of three segments. The two stored requests are real parameter-bound
// buttons; LEGACY clears both.
// ---------------------------------------------------------------------------

class AdvPolicyCell final : public ContextButton
{
public:
    AdvPolicyCell(Cs80AudioProcessor& p, int idx, const juce::String& name, const juce::String& caption)
        : ContextButton(name), proc(p), index(idx), sub(caption)
    {
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setClickingTogglesState(idx != 0);
        if (idx != 0)
        {
            const int parameter = idx == 1 ? cs80::P_sourcePerformance : cs80::P_parityMode;
            bindParameterContext(p, Cs80AudioProcessor::parameterId(parameter), parameter);
        }
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
        constexpr float dw = 368.0f, dh = 54.0f;
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
        jewel(g, { r.getX() + 22.0f, r.getCentreY() }, 5.5f, lit ? juce::Colour(0xff7a1d0a) : amber, lit);
        if (masked)
        {
            g.setColour(amber);
            g.drawEllipse(r.getX() + 22.0f - 8.0f, r.getCentreY() - 8.0f, 16.0f, 16.0f, 1.3f);
        }
        ui::text(g, getButtonText() + (masked ? "  (MASKED)" : ""), { r.getX() + 44.0f, r.getY() + 6.0f, r.getWidth() - 52.0f, 20.0f }, 13.5f, true,
                 masked ? amberBright : ink, Just::centredLeft, 0.06f);
        ui::text(g, sub, { r.getX() + 44.0f, r.getY() + 27.0f, r.getWidth() - 52.0f, 18.0f }, 12.5f, false,
                 lit ? juce::Colour(0xff3c260a) : panelInk(), Just::centredLeft, 0.0f);
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
        // The hit area is 36 design px; the visible bead is 26.
        const Rect r = getLocalBounds().toFloat();
        const float d = juce::jmin(r.getWidth(), r.getHeight()) * (26.0f / 36.0f);
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

// ---------------------------------------------------------------------------
// LFO destination chip: one of the six states of an LFO's single destination. The lit chip shows the
// swing the engine applies; clicking it again switches the LFO off.
// ---------------------------------------------------------------------------

class AdvLfoChip final : public ContextButton
{
public:
    AdvLfoChip(Cs80AudioProcessor& p, int lfo, int dest)
        : ContextButton(kDestinations[dest]), proc(p), lfoIndex(lfo), destination(dest)
    {
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        const juce::String lfoName = "LFO " + juce::String(lfo + 1);
        setTitle(lfoName + " destination " + kDestinations[dest]);
        const int parameter = cs80::P_lfo1Dest + lfo * 3;
        bindParameterContext(p, Cs80AudioProcessor::parameterId(parameter), parameter);
        const juce::String tip = dest == 0 ? "Switch " + lfoName + " off: no destination."
                                           : "Send " + lfoName + " to " + kDestinations[dest] + ". Click the lit chip again to switch the LFO off.";
        setTooltip(tip);
        setDescription(tip);
        onClick = [this]
        {
            const int param = cs80::P_lfo1Dest + lfoIndex * 3;
            const int current = juce::jlimit(0, 5, juce::roundToInt(proc.getParamValue(param)));
            setParameterOnce(proc, param, (destination != 0 && current == destination) ? 0.0f : (float) destination);
            refreshState();
        };
    }

    void refreshState()
    {
        const int dest = juce::jlimit(0, 5, juce::roundToInt(proc.getParamValue(cs80::P_lfo1Dest + lfoIndex * 3)));
        const float depth = proc.getParamValue(cs80::P_lfo1Depth + lfoIndex * 3);
        const int sig = dest * 100000 + juce::roundToInt(depth * 1000.0f) * 2 + (proc.getParamValue(cs80::P_parityMode) >= 0.5f ? 1 : 0);
        if (sig != lastSig)
        {
            lastSig = sig;
            setToggleState(dest == destination, juce::dontSendNotification);
            repaint();
        }
    }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        constexpr float dw = 80.0f, dh = 44.0f;
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / dw, (float) getHeight() / dh));
        const int dest = juce::jlimit(0, 5, juce::roundToInt(proc.getParamValue(cs80::P_lfo1Dest + lfoIndex * 3)));
        const float depth = proc.getParamValue(cs80::P_lfo1Depth + lfoIndex * 3);
        const bool iso = proc.getParamValue(cs80::P_parityMode) >= 0.5f;
        const bool lit = dest == destination;
        const auto colour = lfoColour(lfoIndex);
        // OFF is not a route: lit, it is a pressed gunmetal key with ivory legend and no cap colour at all.
        const bool offKey = destination == 0;
        const Rect r = Rect(1.0f, 1.0f, dw - 2.0f, dh - 4.0f).translated(0.0f, down ? 1.0f : 0.0f);
        if (lit && ! offKey)
        {
            g.setColour(colour.withAlpha(iso ? 0.10f : 0.24f));
            g.fillRoundedRectangle(r.expanded(3.0f), 7.0f);
        }
        if (! (lit && offKey))
        {
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRoundedRectangle(r.translated(0.0f, 1.5f), 5.0f);
        }
        if (lit && offKey)
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1a1918), r.getX(), r.getY(), juce::Colour(0xff4a4843), r.getX(), r.getBottom(), false));
        else if (lit)
            g.setGradientFill(juce::ColourGradient(colour.brighter(0.18f).withAlpha(iso ? 0.55f : 1.0f), r.getX(), r.getY(),
                                                   colour.darker(0.42f).withAlpha(iso ? 0.55f : 1.0f), r.getX(), r.getBottom(), false));
        else
            g.setGradientFill(juce::ColourGradient(juce::Colour(hovered ? 0xff2f2c27 : 0xff24221e), r.getX(), r.getY(),
                                                   juce::Colour(0xff131210), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 5.0f);
        if (lit && offKey)
        {
            // Pressed: the cap's top edge is in shadow and a hairline of light runs along the bottom.
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.drawLine(r.getX() + 5.0f, r.getY() + 1.4f, r.getRight() - 5.0f, r.getY() + 1.4f, 1.6f);
            g.setColour(juce::Colours::white.withAlpha(0.10f));
            g.drawLine(r.getX() + 5.0f, r.getBottom() - 1.0f, r.getRight() - 5.0f, r.getBottom() - 1.0f, 1.0f);
        }
        else
        {
            g.setColour(juce::Colours::white.withAlpha(lit ? 0.38f : 0.08f));
            g.drawLine(r.getX() + 5.0f, r.getY() + 1.1f, r.getRight() - 5.0f, r.getY() + 1.1f, 1.0f);
        }
        const juce::Colour edge = offKey ? ivory.withAlpha(0.55f) : colour.brighter(0.2f);
        g.setColour(hovered ? edge : (lit && ! offKey ? colour.darker(0.6f) : juce::Colours::black.withAlpha(0.9f)));
        g.drawRoundedRectangle(r, 5.0f, hovered ? 1.6f : 1.0f);

        const juce::Colour ink = lit ? (offKey ? ivory : inkFor(colour)) : (hovered ? ivory : ivory.withAlpha(0.9f));
        if (offKey)
        {
            ui::text(g, "OFF", r, 12.5f, true, lit ? ink : ivoryDim, Just::centred, 0.1f);
        }
        else
        {
            ui::text(g, kDestinations[destination], { r.getX() + 4.0f, r.getY() + 3.0f, r.getWidth() - 8.0f, 18.0f }, 12.5f, true, ink,
                     Just::centred, 0.04f);
            const bool have = depth > 0.001f;
            const float amount = have ? depth * kLfoScale[destination] : kLfoScale[destination];
            ui::text(g, plusMinus() + juce::String(amount, amount < 1.0f ? 2 : (have ? 2 : 1)) + kLfoUnit[destination],
                     { r.getX() + 2.0f, r.getY() + 21.0f, r.getWidth() - 4.0f, 16.0f }, 12.5f, lit,
                     lit ? ink : axisInk(), Just::centred);
        }
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawRoundedRectangle(r.expanded(1.8f), 6.0f, 1.6f);
        }
    }

private:
    Cs80AudioProcessor& proc;
    int lfoIndex, destination;
    int lastSig = -1;
};

// ---------------------------------------------------------------------------
// A parameter-bound switch whose face carries a plain-words legend. The button text stays the stable
// stored name that the automation and self-test layers look for.
// ---------------------------------------------------------------------------

class AdvSwitch final : public ContextButton
{
public:
    AdvSwitch(const juce::String& storedName, const juce::String& legendText) : ContextButton(storedName), legend(legendText)
    {
        setClickingTogglesState(true);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setTitle(legendText + " (" + storedName.toLowerCase() + ")");
    }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        constexpr float dw = 236.0f, dh = 40.0f;
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / dw, (float) getHeight() / dh));
        const bool on = getToggleState();
        const Rect r(1.0f, 1.0f, dw - 2.0f, dh - 3.0f);
        drawSatinPlate(g, r, on, down, hovered, true);
        ui::text(g, legend, { r.getX() + 30.0f, r.getY(), r.getWidth() - 36.0f, r.getHeight() - 2.0f }, 12.0f, true, on ? amberBright : ivory,
                 Just::centredLeft, 0.1f);
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawRoundedRectangle(r.expanded(0.5f), 3.5f, 1.6f);
        }
    }

private:
    juce::String legend;
};

// ---------------------------------------------------------------------------
// Two-way selector: STEREO and MONO side by side, the one in force lit. One boolean parameter (on = stereo).
// ---------------------------------------------------------------------------

class AdvOutputSwitch final : public ContextButton
{
public:
    AdvOutputSwitch() : ContextButton("Output mode")
    {
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setTitle("Output mode: stereo or mono");
        setTooltip("STEREO keeps the left and right outputs separate. MONO averages them after effects and limiting; phase cancellation "
                   "can change the result.");
        setDescription(getTooltip());
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        pressed = e.x < getWidth() / 2 ? 0 : 1;
        ContextButton::mouseDown(e);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        ContextButton::mouseUp(e);
        pressed = -1;
    }

    // The arrows pick a side, Space and Return flip it.
    bool keyPressed(const juce::KeyPress& key) override
    {
        const bool flip = key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey;
        const bool left = key == juce::KeyPress::leftKey, right = key == juce::KeyPress::rightKey;
        if (! (flip || left || right))
            return ContextButton::keyPressed(key);
        const bool toStereo = flip ? ! getToggleState() : left;
        if (toStereo != getToggleState())
            setToggleState(toStereo, juce::sendNotificationSync);
        return true;
    }

    // A click chooses the half it landed on. The parameter attachment also reaches this through
    // setToggleState, with no mouse press behind it: that notification must not flip anything.
    void clicked() override
    {
        if (pressed >= 0)
            setToggleState(pressed == 0, juce::dontSendNotification);
    }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        constexpr float dw = 236.0f, dh = 40.0f;
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / dw, (float) getHeight() / dh));
        const bool stereo = getToggleState();
        const Rect track(1.0f, 1.0f, dw - 2.0f, dh - 3.0f);
        drawWell(g, track, 5.0f);
        const float half = (track.getWidth() - 9.0f) * 0.5f;
        for (int seg = 0; seg < 2; ++seg)
        {
            const bool lit = (seg == 0) == stereo;
            const Rect r(track.getX() + 3.0f + (float) seg * (half + 3.0f), track.getY() + 3.0f, half, track.getHeight() - 6.0f);
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRoundedRectangle(r.translated(0.0f, 1.5f), 4.0f);
            g.setGradientFill(lit ? juce::ColourGradient(kGlow, r.getX(), r.getY(), kGlowDeep, r.getX(), r.getBottom(), false)
                                  : juce::ColourGradient(juce::Colour(hovered ? 0xff3a3733 : 0xff2b2926), r.getX(), r.getY(),
                                                         juce::Colour(0xff151311), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle(r.translated(0.0f, down && lit ? 1.0f : 0.0f), 4.0f);
            g.setColour(juce::Colours::white.withAlpha(lit ? 0.45f : 0.10f));
            g.drawLine(r.getX() + 4.0f, r.getY() + 1.1f, r.getRight() - 4.0f, r.getY() + 1.1f, 1.0f);
            g.setColour(lit ? juce::Colour(0xff6b4510) : juce::Colours::black.withAlpha(0.9f));
            g.drawRoundedRectangle(r, 4.0f, 1.0f);
            ui::text(g, seg == 0 ? "STEREO" : "MONO", r, 12.5f, true, lit ? juce::Colour(0xff2a1c07) : ivory, Just::centred, 0.1f);
        }
        if (hasKeyboardFocus(true))
        {
            g.setColour(juce::Colour(0xfff6df91));
            g.drawRoundedRectangle(track.expanded(0.5f), 5.5f, 1.6f);
        }
    }

private:
    int pressed = -1;
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
        setAccessible(false);   // decorative: what it says is also given as text (AccessibleText)
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

// Painted content (the model notes and the evidence register) for screen readers: an invisible static-text element
// over the painting, with the same rows as its description. It takes no clicks and no Tab stop.
class AccessibleText final : public juce::Component
{
public:
    AccessibleText(const juce::String& title, const juce::String& text)
    {
        setTitle(title);
        setDescription(text);
        setInterceptsMouseClicks(false, false);
        setWantsKeyboardFocus(false);
    }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::staticText);
    }
};

juce::String registerRowText(const evidence::ModelEntry& row)
{
    return juce::String(row.subsystem) + ": circuit engine " + row.circuit + "; legacy " + row.legacy + "; reference " + row.reference
         + "; evidence " + evidence::basisName(row.basis) + ", coverage " + row.coverage + ".";
}

// Evidence chip colours: green for source-derived laws, amber for a source route with an inferred law,
// warm grey for a declared inference, cool blue for the modern layer (never one of the cap colours alone).
juce::Colour basisColour(evidence::Basis b)
{
    switch (b)
    {
        case evidence::Basis::sourceDerived: return juce::Colour(0xff4fbf6e);
        case evidence::Basis::sourceTopologyInferredLaw: return juce::Colour(0xffe2b04a);
        case evidence::Basis::engineeringInferred: return juce::Colour(0xffc9bfae);
        case evidence::Basis::modernExtension: return juce::Colour(0xff6cc6e6);
    }
    return ivory;
}

juce::String basisShort(evidence::Basis b)
{
    switch (b)
    {
        case evidence::Basis::sourceDerived: return "SOURCE";
        case evidence::Basis::sourceTopologyInferredLaw: return "SOURCE ROUTE / INFERRED LAW";
        case evidence::Basis::engineeringInferred: return "INFERRED";
        case evidence::Basis::modernExtension: return "MODERN";
    }
    return {};
}

void fitted(juce::Graphics& g, const juce::String& s, Rect r, float h, bool bold, juce::Colour c, Just j = Just::centredLeft)
{
    g.setFont(fnt(h, bold, 0.01f));
    g.setColour(c);
    g.drawFittedText(s, r.toNearestInt(), j, 1, 0.86f);
}

// A small evidence chip: the basis in words on a tint of its colour, the coverage codes after it. A chip too narrow
// for the full words uses the compact form rather than squeezing the type.
void basisChip(juce::Graphics& g, Rect r, evidence::Basis b, const juce::String& codes)
{
    const auto c = basisColour(b);
    g.setColour(c.withAlpha(0.20f));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(c.withAlpha(0.85f));
    g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
    auto label = basisShort(b) + (codes.isNotEmpty() ? "  " + codes : juce::String());
    if (textWidth(label, 12.5f, true, 0.01f) > r.getWidth() - 16.0f && b == evidence::Basis::sourceTopologyInferredLaw)
        label = "ROUTE + INFERRED" + (codes.isNotEmpty() ? "  " + codes : juce::String());
    fitted(g, label, r.reduced(8.0f, 0.0f), 12.5f, true, c.brighter(0.25f), Just::centred);
}

// The circuit-engine column of the register: "same as legacy" rows say so instead of a bare "same".
juce::String circuitText(const evidence::ModelEntry& row)
{
    const juce::String circuit(row.circuit);
    if (circuit == "same")
        return "same as legacy";
    if (circuit.startsWith("same "))
        return "same as legacy " + circuit.substring(5);
    if (circuit.startsWith("same;"))
        return "same as legacy;" + circuit.substring(5);
    return circuit;
}

const evidence::ModelEntry* modelEntry(const char* subsystem)
{
    for (const auto& row : evidence::kModelRegister)
        if (juce::String(row.subsystem) == subsystem)
            return &row;
    return nullptr;
}
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

    // Moves everything recorded inside its scope by (dx, dy) design units, so a module written for one
    // place on the former Advanced pages can be placed elsewhere on a Studio tab unchanged.
    struct Origin
    {
        Origin(AdvancedPage& p, float dx, float dy) : page(p), previous(p.off) { p.off = { previous.x + dx, previous.y + dy }; }
        ~Origin() { page.off = previous; }
        AdvancedPage& page;
        Pt previous;
    };

    void resized() override
    {
        art->setBounds(getLocalBounds());
        const float sx = (float) getWidth() / kW;
        const float sy = (float) getHeight() / kH;
        // The same type floor as the rest of the page: 12.5 design px, whatever the window size.
        for (auto* k : knobs)
            k->setMinCaptionHeight(kMinFont * sx);
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
    // there and in its tooltip instead of on the panel face. `lamp` makes the title chiclet a live
    // lamp that is lit only while the module is doing something.
    using LampFn = std::function<bool(Cs80AudioProcessor&)>;

    void module(Rect r, const juce::String& title, bool primary = true,
                juce::Colour accent = juce::Colours::transparentBlack, const juce::String& info = {}, LampFn lamp = nullptr)
    {
        const bool hasInfo = info.isNotEmpty();
        const bool lamped = lamp != nullptr && ! accent.isTransparent();
        artOps.push_back([a = at(r), title, primary, accent, hasInfo, lamped](juce::Graphics& g)
                         { drawModule(g, a, title, primary, accent, hasInfo, ! lamped); });
        if (hasInfo)
            infoButton({ r.getRight() - 76.0f, r.getY() + 4.0f, 36.0f, 36.0f }, title, info);
        if (lamped)
        {
            const float x1 = r.getRight() - (hasInfo ? kInfoReserve : 32.0f);
            widget<ModuleLamp>(bracketChipRect(r.getX() + 32.0f, x1, r.getY() + 22.0f, title.toUpperCase(), primary ? 13.5f : 12.5f), accent,
                               std::move(lamp));
        }
    }

    AdvInfoButton* infoButton(Rect r, const juce::String& subject, const juce::String& body)
    {
        auto control = std::make_unique<AdvInfoButton>(subject, body);
        auto* raw = control.get();
        raw->setExplicitFocusOrder(1000 + (++infoCounter));
        raw->onClick = [this, raw] { if (onInfo) onInfo(raw->heading, raw->body, raw); };
        addAndMakeVisible(*raw);
        owned.push_back(std::move(control));
        placed.push_back({ raw, at(r) });
        return raw;
    }

    void label(const juce::String& s, Rect r, float size = 12.5f, juce::Colour c = ivoryDim,
               Just j = Just::centredLeft, float kern = 0.09f)
    {
        artOps.push_back([=, a = at(r)](juce::Graphics& g) { engrave(g, s, a, size, true, c, j, kern); });
    }

    // Quiet running text for a plot or a group: 11 px or more, never an engraved legend.
    void note(const juce::String& s, Rect r, float size = 12.5f, Just j = Just::centredLeft, bool bold = false)
    {
        artOps.push_back([=, a = at(r)](juce::Graphics& g) { text(g, s, a, size, bold, axisInk(), j, 0.02f); });
    }

    // Dim controls that do nothing in the present state. They stay editable: a value set now is used later.
    void dimWhen(std::function<bool()> inert, std::vector<juce::Component*> parts, float alpha = 0.68f)
    {
        auto last = std::make_shared<int>(-1);
        tickers.push_back([inert = std::move(inert), parts = std::move(parts), alpha, last]
        {
            const int now = inert() ? 1 : 0;
            if (now != *last)
            {
                *last = now;
                for (auto* c : parts)
                    c->setAlpha(now != 0 ? alpha : 1.0f);
            }
        });
    }

    // The one caption style for control headings: same area height as a knob title keeps baselines level.
    void caption(const juce::String& s, Rect area, Just j = Just::centred, float size = 12.5f)
    {
        artOps.push_back([=, a = at(area)](juce::Graphics& g) { drawControlCaption(g, s, a, j, size); });
    }

    // The same caption as a live component, for a heading that must dim together with its control.
    CaptionWidget* captionLive(const juce::String& s, Rect area, Just j = Just::centred, float size = 12.5f)
    {
        return widget<CaptionWidget>(area, area.getWidth(), area.getHeight(), s, j, size);
    }

    void bracket(float x0, float x1, float y, const juce::String& title, float size = 12.5f)
    {
        artOps.push_back([=, o = off](juce::Graphics& g) { bracketTitle(g, x0 + o.x, x1 + o.x, y + o.y, title, size); });
    }

    void custom(std::function<void(juce::Graphics&)> op)
    {
        if (off.x == 0.0f && off.y == 0.0f)
        {
            artOps.push_back(std::move(op));
            return;
        }
        artOps.push_back([o = off, op = std::move(op)](juce::Graphics& g)
        {
            juce::Graphics::ScopedSaveState state(g);
            g.addTransform(juce::AffineTransform::translation(o.x, o.y));
            op(g);
        });
    }

    void tick(std::function<void()> fn) { tickers.push_back(std::move(fn)); }

    // ---- evidence -------------------------------------------------------------------------------
    // A module listing the model-register rows of the named subsystems: what the circuit engine runs and
    // what that law rests on. The rows come from EvidenceRegister.h, the one compiled copy of the register.
    void modelNotes(Rect r, const juce::String& title, std::vector<const char*> subsystems)
    {
        module(r, title, true, juce::Colours::transparentBlack,
               "Which model makes each part of the sound, and what it rests on. SOURCE: the law is taken from the Yamaha "
               "schematics or manuals. SOURCE ROUTE / INFERRED LAW: the circuit path is from the sources, how strongly it acts "
               "is a declared estimate. INFERRED: a declared engineering model where the public sources are silent. MODERN: a "
               "software addition, switched off by PURE CS-80. The letters are the coverage ledger's status codes (IB "
               "implemented behaviour, A approximate law, M missing, MR measurement required, PC physical controller "
               "required). Nothing here is a hardware measurement. Settings lists every subsystem.");
        juce::StringArray spoken;
        for (const char* name : subsystems)
            if (const auto* row = modelEntry(name))
                spoken.add(registerRowText(*row));
        adoptText(std::make_unique<AccessibleText>(title, spoken.joinIntoString(" ")), r.reduced(8.0f, 40.0f).translated(0.0f, 16.0f));
        custom([r, subsystems = std::move(subsystems)](juce::Graphics& g)
        {
            const float x = r.getX() + 24.0f, w = r.getWidth() - 48.0f;
            const float top = r.getY() + 46.0f;
            // A narrow card stacks each row: the name and its chip, then the circuit and legacy lines at full width.
            const bool stacked = w < 640.0f;
            const float chipW = stacked ? juce::jmin(190.0f, w * 0.44f) : juce::jmin(300.0f, w * 0.34f), nameW = juce::jmin(220.0f, w * 0.26f);
            const float rowH = juce::jmin(stacked ? 92.0f : 54.0f, (r.getBottom() - 18.0f - top) / (float) juce::jmax(1, (int) subsystems.size()));
            float y = top;
            for (const char* name : subsystems)
            {
                const auto* row = modelEntry(name);
                if (row == nullptr)
                    continue;
                if (y > top)
                    etchLine(g, x, y, x + w, y, 0.35f);
                if (stacked)
                {
                    const float line = rowH / 3.2f;
                    fitted(g, juce::String(row->subsystem).toUpperCase(), { x, y + 4.0f, w - chipW - 10.0f, line }, 13.0f, true, ivory);
                    basisChip(g, { x + w - chipW, y + 4.0f + (line - 24.0f) * 0.5f, chipW, 24.0f }, row->basis, row->coverage);
                    fitted(g, "CIRCUIT  " + circuitText(*row), { x, y + 4.0f + line, w, line }, 12.5f, false, ivory.withAlpha(0.92f));
                    fitted(g, "LEGACY  " + juce::String(row->legacy), { x, y + 4.0f + line * 2.0f, w, line }, 12.5f, false, panelInk());
                    y += rowH;
                    continue;
                }
                const float textX = x + nameW, textW = w - nameW - chipW - 16.0f;
                fitted(g, juce::String(row->subsystem).toUpperCase(), { x, y, nameW - 8.0f, rowH }, 13.0f, true, ivory);
                fitted(g, "CIRCUIT  " + circuitText(*row), { textX, y + rowH * 0.06f, textW, rowH * 0.44f }, 12.5f, false, ivory.withAlpha(0.92f));
                fitted(g, "LEGACY  " + juce::String(row->legacy) + "      SEE  " + juce::String(row->doc),
                       { textX, y + rowH * 0.5f, textW, rowH * 0.44f }, 12.5f, false, panelInk());
                basisChip(g, { x + w - chipW, y + (rowH - 26.0f) * 0.5f, chipW, 26.0f }, row->basis, row->coverage);
                y += rowH;
            }
        });
    }

    // The whole model register: every subsystem, what each engine runs, the offline reference and the evidence.
    void evidenceRegister(Rect r)
    {
        module(r, "Evidence register", true, pageColour(4),
               "The model register of docs/STATUS.md, compiled into the instrument. LEGACY is the compatibility engine kept for "
               "old sessions; CIRCUIT is the circuit engine (ENGINE: CIRCUIT and PURE CS-80). REFERENCE names the offline truth "
               "or experimental model that holds the realtime model to account; it never makes sound. EVIDENCE says what the "
               "circuit engine's law rests on and gives the coverage ledger's status codes (IB implemented behaviour, A "
               "approximate law, M missing, MR measurement required, PC physical controller required). No row is a hardware "
               "measurement or a claim of verified parity.");
        juce::StringArray spoken;
        for (const auto& row : evidence::kModelRegister)
            spoken.add(registerRowText(row));
        adoptText(std::make_unique<AccessibleText>("Evidence register", spoken.joinIntoString(" ")), r.reduced(8.0f, 40.0f).translated(0.0f, 16.0f));
        custom([r](juce::Graphics& g)
        {
            const float x = r.getX() + 24.0f, w = r.getWidth() - 48.0f;
            const float cols[] = { 0.0f, 0.15f, 0.36f, 0.66f, 0.83f, 1.0f };
            const char* const heads[] = { "SUBSYSTEM", "LEGACY", "CIRCUIT ENGINE", "REFERENCE (NO SOUND)", "EVIDENCE" };
            const float headY = r.getY() + 42.0f;
            for (int c = 0; c < 5; ++c)
                text(g, heads[c], { x + w * cols[c], headY, w * (cols[c + 1] - cols[c]) - 10.0f, 20.0f }, 12.5f, true, gold, Just::centredLeft, 0.08f);
            etchLine(g, x, headY + 22.0f, x + w, headY + 22.0f, 0.55f);
            const int n = evidence::kModelRegisterSize;
            const float top = headY + 26.0f;
            const float rowH = (r.getBottom() - 14.0f - top) / (float) n;
            for (int i = 0; i < n; ++i)
            {
                const auto& row = evidence::kModelRegister[i];
                const float y = top + rowH * (float) i;
                if ((i & 1) == 1)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.025f));
                    g.fillRect(x - 6.0f, y, w + 12.0f, rowH);
                }
                auto cell = [&](int c) { return Rect(x + w * cols[c], y, w * (cols[c + 1] - cols[c]) - 10.0f, rowH); };
                fitted(g, row.subsystem, cell(0), 12.5f, true, ivory);
                fitted(g, row.legacy, cell(1), 12.5f, false, panelInk());
                fitted(g, circuitText(row), cell(2), 12.5f, false, ivory.withAlpha(0.92f));
                fitted(g, row.reference, cell(3), 12.5f, false, panelInk());
                basisChip(g, cell(4).withSizeKeepingCentre(cell(4).getWidth(), juce::jmin(rowH - 3.0f, 22.0f)), row.basis, row.coverage);
            }
        });
    }

    // ---- live widgets ---------------------------------------------------------------------------
    template <class W, class... Args>
    W* widget(Rect r, Args&&... args)
    {
        auto w = std::make_unique<W>(proc, std::forward<Args>(args)...);
        W* raw = w.get();
        addAndMakeVisible(*raw);
        owned.push_back(std::move(w));
        widgets.push_back(raw);
        const Rect a = at(r);
        placed.push_back({ raw, a });
        artOps.push_back([raw, a](juce::Graphics& g)
        {
            g.saveState();
            g.addTransform(juce::AffineTransform::scale(a.getWidth() / raw->getDesignW(), a.getHeight() / raw->getDesignH())
                               .translated(a.getX(), a.getY()));
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
        raw->setMinCaptionHeight(kMinFont);
        addAndMakeVisible(*raw);
        owned.push_back(std::move(control));
        placed.push_back({ raw, at(r) });
        knobs.push_back(raw);
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
        raw->bindParameterContext(proc, Cs80AudioProcessor::parameterId(parameter), parameter);
        raw->setExplicitFocusOrder(++focusCounter);
        addAndMakeVisible(*raw);
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.getAPVTS(), Cs80AudioProcessor::parameterId(parameter), *raw));
        owned.push_back(std::move(control));
        placed.push_back({ raw, at(r) });
        return raw;
    }

    // One state of an LFO's destination.
    AdvLfoChip* lfoChip(Rect r, int lfo, int dest)
    {
        auto control = std::make_unique<AdvLfoChip>(proc, lfo, dest);
        auto* raw = control.get();
        raw->setExplicitFocusOrder(++focusCounter);
        addAndMakeVisible(*raw);
        owned.push_back(std::move(control));
        placed.push_back({ raw, at(r) });
        tickers.push_back([raw] { raw->refreshState(); });
        return raw;
    }

    // A parameter-bound switch with a plain-words legend on its face.
    AdvSwitch* switchButton(Rect r, int parameter, const juce::String& storedName, const juce::String& legend)
    {
        auto control = std::make_unique<AdvSwitch>(storedName, legend);
        auto* raw = control.get();
        raw->bindParameterContext(proc, Cs80AudioProcessor::parameterId(parameter), parameter);
        raw->setExplicitFocusOrder(++focusCounter);
        addAndMakeVisible(*raw);
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.getAPVTS(), Cs80AudioProcessor::parameterId(parameter), *raw));
        owned.push_back(std::move(control));
        placed.push_back({ raw, at(r) });
        return raw;
    }

    // A two-way STEREO / MONO selector on one boolean parameter.
    AdvOutputSwitch* outputSwitch(Rect r, int parameter)
    {
        auto control = std::make_unique<AdvOutputSwitch>();
        auto* raw = control.get();
        raw->bindParameterContext(proc, Cs80AudioProcessor::parameterId(parameter), parameter);
        raw->setExplicitFocusOrder(++focusCounter);
        addAndMakeVisible(*raw);
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.getAPVTS(), Cs80AudioProcessor::parameterId(parameter), *raw));
        owned.push_back(std::move(control));
        placed.push_back({ raw, at(r) });
        return raw;
    }

    juce::ComboBox* choice(Rect r, int parameter, const juce::StringArray& items)
    {
        auto control = std::make_unique<ContextComboBox>();
        control->bindParameterContext(proc, Cs80AudioProcessor::parameterId(parameter), parameter, items);
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
        const Rect a = at(r);
        placed.push_back({ raw, a });
        artOps.push_back([a](juce::Graphics& g) { drawWell(g, a.expanded(2.5f), 4.0f); });
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
        placed.push_back({ raw, at(r) });
        tickers.push_back([raw] { raw->refreshState(); });
        return raw;
    }

    // Static text for assistive technology over painted content; never a Tab stop.
    void adoptText(std::unique_ptr<AccessibleText> component, Rect r)
    {
        auto* raw = component.get();
        addAndMakeVisible(*raw);
        raw->toBack();
        owned.push_back(std::move(component));
        placed.push_back({ raw, at(r) });
    }

    // A component made elsewhere (for example a plain button), placed in design space and owned here.
    template <class C>
    C* adopt(std::unique_ptr<C> component, Rect r)
    {
        auto* raw = component.get();
        raw->setExplicitFocusOrder(++focusCounter);
        addAndMakeVisible(*raw);
        owned.push_back(std::move(component));
        placed.push_back({ raw, at(r) });
        return raw;
    }

    Cs80AudioProcessor& processor() noexcept { return proc; }

private:
    Rect at(Rect r) const noexcept { return r.translated(off.x, off.y); }

    Cs80AudioProcessor& proc;
    std::unique_ptr<ArtLayer> art;
    std::vector<std::function<void(juce::Graphics&)>> artOps;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<Placed> placed;
    std::vector<ParamKnob*> knobs;
    std::vector<LiveWidget*> widgets;
    std::vector<std::function<void()>> tickers;
    Pt off { 0.0f, 0.0f };
    int focusCounter = 0;
    int infoCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedPage)
};

// ---------------------------------------------------------------------------
// Chassis: crinkle-finished charcoal body between the editor's cheeks, header and footer rails. Cached once.
// ---------------------------------------------------------------------------

// The tab strip: a recessed glass slot holding five back-lit plates.
constexpr float kTabGlassX = 357.0f, kTabGlassW = 886.0f;
constexpr float kTabPlateW = 166.0f, kTabPlateH = 48.0f, kTabPitch = 174.0f, kTabX0 = 367.0f, kTabY = 22.0f;

class AdvChassis final : public juce::Component
{
public:
    AdvChassis()
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(false);
        setBufferedToImage(true);
    }

    // Only the body between the cheek columns (x 44 to 1556) is painted. The cheeks belong to the editor's
    // own backdrop and stay transparent here, as they do under the memory lid.
    void paint(juce::Graphics& g) override
    {
        gArtScale = (float) getWidth() / kW;
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / kW, (float) getHeight() / kH));

        const Rect body(44.0f, 0.0f, kW - 88.0f, kH);
        fillPanelFace(g, body);
        art::fillRelief(g, art::panelTile(), body, gArtScale, 0.90f);

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
        glassWell(g, { kTabGlassX, 14.0f, kTabGlassW, 68.0f }, 7.0f);

        // Corner screws holding the body to the cheeks.
        for (float x : { 54.0f, kW - 54.0f })
            for (float y : { 14.0f, kH - 14.0f })
                drawScrew(g, { x, y }, 4.0f, x < 800.0f ? 0.7f : 1.9f);

        // The body seats against the cheeks: a short inner shadow along each edge, no drawn line.
        for (int side = 0; side < 2; ++side)
        {
            const float x = side == 0 ? body.getX() : body.getRight();
            const float dir = side == 0 ? 1.0f : -1.0f;
            g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.45f), x, 0.0f,
                                                   juce::Colours::transparentBlack, x + dir * 7.0f, 0.0f, false));
            g.fillRect(side == 0 ? x : x - 7.0f, 0.0f, 7.0f, kH);
        }
    }
};

// ---------------------------------------------------------------------------
// Tabs: back-lit tone plates in the instrument's cap colours, numbered, with an underline on the
// active tab.
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
        setTitle(name + " tab");
        setDescription("Studio tab " + juce::String(index + 1) + " of 5: " + caption + ". Shortcut " + shortcutName() + "+" + juce::String(index + 1)
                       + ". Left and Right arrows move between tabs.");
        setTooltip(name + "  (" + shortcutName() + "+" + juce::String(index + 1) + ")  -  " + caption);
    }

    std::function<void(int)> onStep;

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::leftKey || key == juce::KeyPress::upKey) { if (onStep) onStep(-1); return true; }
        if (key == juce::KeyPress::rightKey || key == juce::KeyPress::downKey) { if (onStep) onStep(1); return true; }
        return juce::Button::keyPressed(key);
    }

    // The plate is kTabPlateW x kTabPlateH design px; the component reaches kPad further on every side so the lit
    // bloom can spill onto the glass slot.
    static constexpr float kPad = 8.0f;

    bool hitTest(int x, int y) override
    {
        const float sx = (float) getWidth() / (kTabPlateW + 2.0f * kPad), sy = (float) getHeight() / (kTabPlateH + 2.0f * kPad);
        const float px = (float) x / sx - kPad, py = (float) y / sy - kPad;
        return px >= 0.0f && px < kTabPlateW && py >= 0.0f && py < kTabPlateH;
    }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / (kTabPlateW + 2.0f * kPad), (float) getHeight() / (kTabPlateH + 2.0f * kPad)));
        g.addTransform(juce::AffineTransform::translation(kPad, kPad));
        const bool on = getToggleState();
        const auto colour = pageColour(page);
        const Rect plate(0.0f, 0.0f, kTabPlateW, kTabPlateH);

        // The main panel's own tone-selector tab: an unlit page is a dim smoked lens (about 55 percent of its lit
        // brightness, less saturated, cream legend); the open page is lit from behind with an inner glow and a
        // small bloom on the glass slot.
        if (on)
            drawLightBloom(g, plate.reduced(3.2f).withTrimmedRight(3.2f).withTrimmedBottom(3.2f), kPad - 1.0f, colour.brighter(0.15f), 0.55f);
        // Lit plastic is run a little deeper than the cap colour, so the lamp lifts it without bleaching the whole plate.
        drawTonePlate(g, plate, on ? colour.withMultipliedBrightness(0.80f).withMultipliedSaturation(1.15f) : colour, on, down, hovered);

        // The plate travels down and right when lit or pressed; the legend moves with it.
        const float travel = down ? 2.56f : (on ? 1.44f : 0.0f);
        const Rect face(3.2f + travel, 3.2f + travel, kTabPlateW - 9.6f, kTabPlateH - 9.6f);
        const juce::Colour ink = on ? juce::Colour(0xff15120c) : juce::Colour(0xfff6f0dc);
        drawIcon(g, page, { face.getX() + 7.0f, face.getY() + 7.0f, 22.0f, face.getHeight() - 14.0f }, ink, on);
        // Number badge: a dark disc with a cream numeral, readable on every plate.
        const Pt bc(face.getX() + 43.0f, face.getCentreY());
        g.setColour(juce::Colour(0xff17120a));
        g.fillEllipse(bc.x - 9.5f, bc.y - 9.5f, 19.0f, 19.0f);
        g.setColour(on ? colour.brighter(0.2f) : colour.withMultipliedBrightness(0.8f));
        g.drawEllipse(bc.x - 9.5f, bc.y - 9.5f, 19.0f, 19.0f, 1.2f);
        ui::text(g, juce::String(page + 1), { bc.x - 10.0f, bc.y - 10.0f, 20.0f, 20.0f }, 12.0f, true, juce::Colours::white, Just::centred);
        // Name: 12.5 px bold on one line.
        g.setFont(fnt(12.5f, true, 0.0f));
        g.setColour(ink);
        g.drawFittedText(getButtonText(), Rect(face.getX() + 57.0f, face.getY(), face.getWidth() - 59.0f, face.getHeight() - 3.0f).toNearestInt(),
                         Just::centredLeft, 1, 0.9f);
        if (hasKeyboardFocus(true))
            drawFocusRing(g, plate);
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
            case 1: // three white keys and two black keys: the keyboard
            {
                const float kw = b.getWidth() / 3.0f, top = b.getY() + 2.0f, h = b.getHeight() - 4.0f;
                for (int i = 0; i < 3; ++i)
                    g.drawRect(Rect(b.getX() + kw * (float) i, top, kw, h), 1.2f);
                for (int i = 1; i < 3; ++i)
                    g.fillRect(Rect(b.getX() + kw * (float) i - kw * 0.28f, top, kw * 0.56f, h * 0.58f));
                break;
            }
            case 2: // three faders: effects and macros
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
            case 3: // rising step bars: arpeggio
            {
                for (int i = 0; i < 4; ++i)
                {
                    const float h = b.getHeight() * (0.3f + 0.2f * (float) i);
                    g.setColour(on ? c : c.withAlpha(0.55f + 0.15f * (float) i));
                    g.fillRoundedRectangle(b.getX() + 2.0f + (float) i * 6.4f, b.getBottom() - 2.0f - h, 4.5f, h, 1.2f);
                }
                break;
            }
            default: // a toothed wheel: settings
            {
                const auto ctr = b.getCentre();
                const float ro = juce::jmin(b.getWidth(), b.getHeight()) * 0.46f, ri = ro * 0.72f;
                for (int i = 0; i < 16; ++i)
                {
                    const float a = juce::MathConstants<float>::twoPi * (float) i / 16.0f;
                    const float r = (i / 2) % 2 == 0 ? ro : ri;
                    const Pt pt(ctr.x + std::sin(a) * r, ctr.y - std::cos(a) * r);
                    if (i == 0) p.startNewSubPath(pt); else p.lineTo(pt);
                }
                p.closeSubPath();
                g.strokePath(p, juce::PathStrokeType(1.5f));
                g.drawEllipse(ctr.x - ro * 0.3f, ctr.y - ro * 0.3f, ro * 0.6f, ro * 0.6f, 1.4f);
                break;
            }
        }
    }

    int page;
};

// A sub-page key in the header: a satin plate whose lamp is lit on the open sub-page.
class StudioSubTab final : public juce::Button
{
public:
    StudioSubTab() : juce::Button("SUB-PAGE")
    {
        setClickingTogglesState(false);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
    }

    std::function<void(int)> onStep;

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::leftKey) { if (onStep) onStep(-1); return true; }
        if (key == juce::KeyPress::rightKey) { if (onStep) onStep(1); return true; }
        return juce::Button::keyPressed(key);
    }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced(1.0f);
        drawSatinPlate(g, r, getToggleState(), down, hovered, true);
        const float s = (float) getHeight() / 36.0f;
        g.setFont(fnt(12.5f * s, true, 0.06f));
        g.setColour(getToggleState() ? amberBright : ivory);
        g.drawFittedText(getButtonText(), r.withTrimmedLeft(22.0f * s).reduced(4.0f * s, 0.0f).toNearestInt(), Just::centred, 1, 0.8f);
        if (hasKeyboardFocus(true))
            drawFocusRing(g, r);
    }
};

// A raised close plate in the same plastic as the toolbar keys.
class AdvCloseButton final : public juce::Button
{
public:
    explicit AdvCloseButton(const juce::String& name) : juce::Button(name)
    {
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setTitle("Close the Studio view");
        setTooltip("Close the Studio view (Esc)");
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

    // `subtitle` false leaves the bottom line to the sub-page keys.
    void setPage(int index, bool subtitle)
    {
        page = index;
        showSubtitle = subtitle;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / 286.0f, (float) getHeight() / 78.0f));
        static const char* const titles[] = { "MODULATION", "KEYBOARD / MPE", "FX / MACROS", "ARP / PERFORMANCE", "SETTINGS" };
        static const char* const sub[] = {
            "LFOs, wheels, sub oscillator and ring",
            "Computer keys, MIDI learn, bend, tuning",
            "Three insert effects, macros and output",
            "Arpeggiator, glide and key release",
            "Engine mode and the evidence register" };
        const int i = juce::jlimit(0, 4, page);
        text(g, "STUDIO", { 0.0f, 2.0f, 286.0f, 13.0f }, 10.0f, true, gold, Just::centredLeft, 0.2f);
        engrave(g, titles[i], { 0.0f, 16.0f, 286.0f, 34.0f }, 22.0f, true, ivory, Just::centredLeft, 0.06f);
        if (showSubtitle)
            text(g, sub[i], { 1.0f, 54.0f, 286.0f, 18.0f }, 11.0f, false, ivoryDim, Just::centredLeft, 0.01f);
    }

private:
    int page = 0;
    bool showSubtitle = true;
};

// The footer: the page's one footnote on the left (what lives on the main panel), the key hints on the right.
class AdvFooterStatus final : public juce::Component
{
public:
    AdvFooterStatus()
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(false);
    }

    void setNote(const juce::String& text)
    {
        if (note == text)
            return;
        note = text;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / 1472.0f, (float) getHeight() / 26.0f));
        text(g, note, { 0.0f, 0.0f, 860.0f, 26.0f }, 12.5f, true, gold, Just::centredLeft, 0.03f);
        // The key hints sit on the dark footer rail: bold cream at 13 px, well clear of the 12.5 floor.
        text(g, shortcutName() + "+1 to 5 tabs   -   Esc closes   -   right-click a control for more", { 760.0f, 0.0f, 696.0f, 26.0f }, 13.0f,
             true, ivory.withAlpha(0.88f), Just::centredRight, 0.02f);
    }

private:
    juce::String note;
};

// ---------------------------------------------------------------------------
// Keyboard tab: the computer keyboard and the MIDI learn map
// ---------------------------------------------------------------------------
namespace
{
// A satin key with a jewel lamp that is not bound to a parameter (editor state, actions).
class StudioKey final : public juce::Button
{
public:
    StudioKey(const juce::String& text, const juce::String& title) : juce::Button(text)
    {
        setTitle(title);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
    }
    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced(1.0f);
        drawSatinPlate(g, r, getToggleState(), down, hovered, true);
        const float s = (float) getHeight() / 36.0f;
        g.setFont(fnt(12.5f * s, true, 0.06f));
        g.setColour(! isEnabled() ? ivoryDim : getToggleState() ? amberBright : ivory);
        g.drawFittedText(getButtonText(), r.withTrimmedLeft(24.0f * s).reduced(6.0f * s, 0.0f).toNearestInt(), Just::centredLeft, 1, 0.85f);
        if (hasKeyboardFocus(true))
            drawFocusRing(g, r);
    }
};

juce::String midiNoteName(int note)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String(names[note % 12]) + juce::String(note / 12 - 1);
}

// The computer-keyboard panel: the switch, the key map with the notes it plays now, octave and velocity.
class QwertyPanel final : public juce::Component, private juce::Timer
{
public:
    static constexpr float kDesignW = 512.0f, kDesignH = 384.0f;

    QwertyPanel(Cs80AudioProcessor& p, std::function<QwertyKeyboard*()> source) : proc(p), keyboard(std::move(source))
    {
        setTitle("Computer keyboard");
        toggle.setComponentID("qwerty_enable");
        toggle.setTooltip("Play notes from the computer keyboard: A W S E D F T G Y H U J K O L P ; are the keys from C to E; Z and X move "
                          "an octave, C and V change the velocity. Text fields keep their keys, and Ctrl/Cmd chords stay shortcuts.");
        toggle.onClick = [this]
        {
            const bool on = ! proc.getEditorState().qwertyEnabled.load();
            proc.getEditorState().qwertyEnabled = on;
            if (auto* q = keyboard ? keyboard() : nullptr)
                q->setEnabled(on);
            refresh();
        };
        addAndMakeVisible(toggle);
        startTimerHz(15);
        refresh();
    }

    void resized() override
    {
        const float s = (float) getWidth() / kDesignW;
        toggle.setBounds(juce::Rectangle<float>(0.0f, 0.0f, 372.0f, 38.0f).transformedBy(juce::AffineTransform::scale(s)).toNearestInt());
    }

    void paint(juce::Graphics& g) override
    {
        const float s = (float) getWidth() / kDesignW;
        g.addTransform(juce::AffineTransform::scale(s));
        auto* q = keyboard ? keyboard() : nullptr;
        const bool on = proc.getEditorState().qwertyEnabled.load();
        const int base = q != nullptr ? q->getBaseNote() : 60;
        const float velocity = q != nullptr ? q->getVelocity() : 0.8f;
        // Two rows of keycaps: the black-key row above (W E T Y U O P), the white-key row below (A S D F G H J K L ;).
        const float cap = 44.0f, gap = 6.0f, x0 = 4.0f, yWhite = 140.0f, yBlack = 82.0f;
        const char* const white = "asdfghjkl;";
        const int whiteOffsets[] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16 };
        const char* const black = "we tyu op";
        const int blackOffsets[] = { 1, 3, -1, 6, 8, 10, -1, 13, 15 };
        auto keycap = [&](Rect r, juce::juce_wchar letter, int offset, bool sharp)
        {
            const int note = base + offset;
            const bool lit = on && note >= 0 && note <= 127 && proc.getKeyboardState().isNoteOn(1, note);
            glassWell(g, r, 4.0f);
            if (lit)
            {
                g.setColour(amber.withAlpha(0.35f));
                g.fillRoundedRectangle(r.reduced(2.0f), 3.0f);
            }
            g.setColour(sharp ? juce::Colour(0xff0a0a0c).withAlpha(0.6f) : juce::Colours::white.withAlpha(0.04f));
            g.fillRoundedRectangle(r.reduced(3.0f), 3.0f);
            text(g, juce::String::charToString(letter).toUpperCase(), r.withTrimmedBottom(r.getHeight() * 0.45f), 15.0f, true,
                 on ? ivory : ivoryDim, Just::centred);
            text(g, midiNoteName(juce::jlimit(0, 127, note)), r.withTrimmedTop(r.getHeight() * 0.52f), 12.5f, false,
                 lit ? amberBright : panelInk(), Just::centred);
        };
        for (int i = 0; i < 10; ++i)
            keycap({ x0 + (float) i * (cap + gap), yWhite, cap, cap + 8.0f }, (juce::juce_wchar) white[i], whiteOffsets[i], false);
        for (int i = 0; i < 9; ++i)
            if (blackOffsets[i] >= 0)
                keycap({ x0 + (cap + gap) * 0.5f + (float) i * (cap + gap), yBlack, cap, cap + 8.0f }, (juce::juce_wchar) black[i], blackOffsets[i], true);
        etchLine(g, 0.0f, 216.0f, kDesignW, 216.0f, 0.4f);
        auto row = [&](float y, const juce::String& keys, const juce::String& what, const juce::String& value)
        {
            text(g, keys, { 0.0f, y, 90.0f, 22.0f }, 13.0f, true, ivory, Just::centredLeft, 0.06f);
            text(g, what, { 96.0f, y, 230.0f, 22.0f }, 12.5f, false, panelInk(), Just::centredLeft);
            text(g, value, { 300.0f, y, 212.0f, 22.0f }, 13.0f, true, amberBright, Just::centredRight);
        };
        row(228.0f, "Z  /  X", "OCTAVE DOWN / UP", "A KEY = " + midiNoteName(base));
        row(256.0f, "C  /  V", "VELOCITY DOWN / UP", juce::String(juce::roundToInt(velocity * 100.0f)) + " %");
        row(284.0f, "CHANNEL", "MIDI CHANNEL OF THE NOTES", "1");
        text(g, "Text fields keep their keys; Ctrl and Cmd chords stay shortcuts.", { 0.0f, 318.0f, kDesignW, 20.0f }, 12.5f, false,
             panelInk(), Just::centredLeft);
        text(g, "Notes go through the on-screen keyboard, so they light its keys.", { 0.0f, 340.0f, kDesignW, 20.0f }, 12.5f, false,
             panelInk(), Just::centredLeft);
    }

private:
    void timerCallback() override { refresh(); }
    void refresh()
    {
        auto* q = keyboard ? keyboard() : nullptr;
        const bool on = proc.getEditorState().qwertyEnabled.load();
        toggle.setToggleState(on, juce::dontSendNotification);
        toggle.setButtonText(on ? "COMPUTER KEYBOARD PLAYS NOTES" : "COMPUTER KEYBOARD OFF");
        std::uint64_t sig = (std::uint64_t) (on ? 1 : 0) | ((std::uint64_t) (q != nullptr ? q->getBaseNote() : 60) << 1)
                          | ((std::uint64_t) juce::roundToInt((q != nullptr ? q->getVelocity() : 0.8f) * 100.0f) << 8);
        const int base = q != nullptr ? q->getBaseNote() : 60;
        for (int i = 0; i < 17; ++i)
            if (base + i <= 127 && proc.getKeyboardState().isNoteOn(1, base + i))
                sig ^= (std::uint64_t) 1 << (20 + i);
        if (sig != lastSig)
        {
            lastSig = sig;
            repaint();
        }
    }

    Cs80AudioProcessor& proc;
    std::function<QwertyKeyboard*()> keyboard;
    StudioKey toggle { "COMPUTER KEYBOARD PLAYS NOTES", "Computer keyboard plays notes" };
    std::uint64_t lastSig = ~std::uint64_t(0);
};

// The MIDI learn map of this session: one row per learned controller, with FORGET; the learning status.
class MidiLearnPanel final : public juce::Component, private juce::Timer, private juce::ListBoxModel
{
public:
    static constexpr float kDesignW = 848.0f, kDesignH = 384.0f;

    explicit MidiLearnPanel(Cs80AudioProcessor& p) : proc(p)
    {
        setTitle("MIDI learn map");
        list.setModel(this);
        list.setComponentID("midi_learn_list");
        list.setTitle("Learned MIDI controllers");
        list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        list.setRowSelectedOnMouseDown(false);
        addAndMakeVisible(list);
        cancel.setComponentID("midi_learn_cancel");
        cancel.setTooltip("Stop waiting for a controller.");
        cancel.onClick = [this] { proc.getMidiLearn().cancelLearn(); refresh(); };
        clearAll.setComponentID("midi_learn_clear_all");
        clearAll.setTooltip("Forget every learned controller of this session.");
        clearAll.onClick = [this] { proc.getMidiLearn().clearAll(); refresh(); };
        addAndMakeVisible(cancel);
        addAndMakeVisible(clearAll);
        // Host index -> the panel's own parameter names.
        const auto& parameters = proc.getParameters();
        for (int i = 0; i < cs80::P_numParams; ++i)
        {
            const int host = proc.hostParameterIndex(i);
            if (host >= 0 && host < parameters.size())
                coreOf[host] = i;
        }
        startTimerHz(10);
        refresh();
    }

    ~MidiLearnPanel() override { list.setModel(nullptr); }

    juce::String nameOf(int host) const
    {
        if (auto it = coreOf.find(host); it != coreOf.end())
        {
            const int core = it->second;
            const auto channel = core >= cs80::P_numGlobals && core < cs80::P_legacyNumParams
                                     ? juce::String(core < cs80::P_ch2Start ? "I " : "II ") : juce::String();
            return (channel + parameterLongName(core)).toUpperCase();
        }
        const auto& parameters = proc.getParameters();
        return host >= 0 && host < parameters.size() ? parameters.getUnchecked(host)->getName(48).toUpperCase() : juce::String("?");
    }

    int getNumRows() override { return (int) rows.size(); }
    juce::String getNameForRow(int row) override
    {
        return row >= 0 && row < (int) rows.size() ? "MIDI CC " + juce::String(rows[(size_t) row].first) + " drives " + nameOf(rows[(size_t) row].second)
                                                   : juce::String();
    }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool) override
    {
        if (row < 0 || row >= (int) rows.size())
            return;
        const float s = (float) getWidth() / kDesignW;
        if ((row & 1) == 1)
        {
            g.setColour(juce::Colours::white.withAlpha(0.03f));
            g.fillRect(0, 0, width, height);
        }
        const auto [cc, host] = rows[(size_t) row];
        g.setFont(fnt(13.0f * s, true, 0.04f));
        g.setColour(amberBright);
        g.drawText("CC " + juce::String(cc), juce::Rectangle<float>(8.0f * s, 0.0f, 90.0f * s, (float) height), Just::centredLeft, false);
        g.setColour(ivory);
        g.drawFittedText(nameOf(host), juce::Rectangle<float>(104.0f * s, 0.0f, (float) width - 240.0f * s, (float) height).toNearestInt(),
                         Just::centredLeft, 1, 0.85f);
        const auto* parameter = host < proc.getParameters().size() ? proc.getParameters()[host] : nullptr;
        if (parameter != nullptr)
        {
            const auto core = coreOf.find(host);
            g.setFont(fnt(12.5f * s, false));
            g.setColour(panelInk());
            g.drawText(core != coreOf.end() ? formatParamValue(core->second, proc.getParamValue(core->second)) : parameter->getCurrentValueAsText(),
                       juce::Rectangle<float>((float) width - 230.0f * s, 0.0f, 110.0f * s, (float) height), Just::centredRight, false);
        }
    }
    juce::Component* refreshComponentForRow(int row, bool, juce::Component* existing) override
    {
        if (row < 0 || row >= (int) rows.size())
        {
            delete existing;
            return nullptr;
        }
        auto* holder = dynamic_cast<RowForget*>(existing);
        if (holder == nullptr)
        {
            delete existing;
            holder = new RowForget(*this);
        }
        holder->set(rows[(size_t) row].first);
        return holder;
    }

    void resized() override
    {
        const float s = (float) getWidth() / kDesignW;
        auto r = [s](float x, float y, float w, float h) { return juce::Rectangle<float>(x, y, w, h).transformedBy(juce::AffineTransform::scale(s)).toNearestInt(); };
        list.setRowHeight(juce::jmax(24, juce::roundToInt(34.0f * s)));
        list.setBounds(r(0.0f, 70.0f, kDesignW, 270.0f));
        cancel.setBounds(r(kDesignW - 330.0f, 4.0f, 160.0f, 36.0f));
        clearAll.setBounds(r(kDesignW - 160.0f, 4.0f, 160.0f, 36.0f));
    }

    void paint(juce::Graphics& g) override
    {
        const float s = (float) getWidth() / kDesignW;
        g.addTransform(juce::AffineTransform::scale(s));
        auto& learn = proc.getMidiLearn();
        const bool pure = proc.getParamValue(cs80::P_parityMode) >= 0.5f;
        const int armed = learn.learningParameter();
        juce::String status = pure ? "PURE CS-80: MIDI LEARN IS OFF  -  MAPPINGS ARE KEPT"
                            : armed >= 0 ? "LEARNING  " + nameOf(armed) + "  -  MOVE A CONTROLLER"
                                         : juce::String(rows.size()) + (rows.size() == 1 ? " CONTROLLER LEARNED" : " CONTROLLERS LEARNED");
        text(g, status, { 0.0f, 4.0f, kDesignW - 340.0f, 36.0f }, 13.5f, true, armed >= 0 && ! pure ? amberBright : ivory, Just::centredLeft, 0.05f);
        text(g, "CC", { 8.0f, 46.0f, 90.0f, 20.0f }, 12.5f, true, gold, Just::centredLeft, 0.08f);
        text(g, "CONTROL", { 104.0f, 46.0f, 300.0f, 20.0f }, 12.5f, true, gold, Just::centredLeft, 0.08f);
        text(g, "VALUE", { kDesignW - 230.0f, 46.0f, 110.0f, 20.0f }, 12.5f, true, gold, Just::centredRight, 0.08f);
        etchLine(g, 0.0f, 68.0f, kDesignW, 68.0f, 0.5f);
        if (rows.empty())
            text(g, "Right-click a control and choose MIDI learn, then move a knob or slider on your controller.",
                 { 8.0f, 90.0f, kDesignW - 16.0f, 24.0f }, 12.5f, false, panelInk(), Just::centredLeft);
        etchLine(g, 0.0f, 346.0f, kDesignW, 346.0f, 0.4f);
        text(g, "Saved with the session, not with presets. Bank select, data entry, RPN/NRPN and channel-mode controllers are never learned.",
             { 0.0f, 352.0f, kDesignW, 22.0f }, 12.5f, false, panelInk(), Just::centredLeft);
    }

private:
    // The FORGET key of a row; the row text is painted by the list.
    class RowForget final : public juce::Component
    {
    public:
        explicit RowForget(MidiLearnPanel& o) : owner(o)
        {
            setInterceptsMouseClicks(false, true);
            forget.setTooltip("Forget this controller.");
            forget.onClick = [this] { owner.proc.getMidiLearn().clearController(cc); owner.refresh(); };
            addAndMakeVisible(forget);
        }
        void set(int controller)
        {
            cc = controller;
            forget.setTitle("Forget MIDI CC " + juce::String(cc));
            forget.setComponentID("midi_learn_forget_" + juce::String(cc));
        }
        void resized() override
        {
            const float s = (float) owner.getWidth() / kDesignW;
            forget.setBounds(juce::Rectangle<int>(juce::roundToInt(104.0f * s), juce::jmax(24, getHeight() - 4))
                                 .withCentre({ getWidth() - juce::roundToInt(60.0f * s), getHeight() / 2 }));
        }
    private:
        MidiLearnPanel& owner;
        StudioKey forget { "FORGET", "Forget" };
        int cc = -1;
    };

    void timerCallback() override { refresh(); }
    void refresh()
    {
        auto& learn = proc.getMidiLearn();
        const auto generation = learn.generation();
        const bool pure = proc.getParamValue(cs80::P_parityMode) >= 0.5f;
        if (generation == lastGeneration && pure == lastPure)
        {
            list.repaint();   // the VALUE column follows the controllers
            return;
        }
        lastGeneration = generation;
        lastPure = pure;
        rows.clear();
        for (int cc = 0; cc < cs80::MidiLearnMap::kNumControllers; ++cc)
            if (const int host = learn.parameterFor(cc); host >= 0)
                rows.emplace_back(cc, host);
        cancel.setVisible(learn.learningParameter() >= 0);
        clearAll.setEnabled(! rows.empty());
        list.updateContent();
        repaint();
    }

    Cs80AudioProcessor& proc;
    juce::ListBox list;
    StudioKey cancel { "CANCEL LEARN", "Cancel MIDI learn" };
    StudioKey clearAll { "CLEAR ALL", "Forget every learned controller" };
    std::vector<std::pair<int, int>> rows;
    std::map<int, int> coreOf;
    std::uint32_t lastGeneration = ~0u;
    bool lastPure = false;
};
} // namespace

// ---------------------------------------------------------------------------
// Page layouts (design space 1600 x 936; content x 64..1536, y 108..896). Each module is written at the
// place it had on the former Advanced pages; an AdvancedPage::Origin moves it on its Studio tab.
// ---------------------------------------------------------------------------

namespace
{
bool lfoRouted(Cs80AudioProcessor& p, int lfo)
{
    return p.getParamValue(cs80::P_parityMode) < 0.5f && juce::roundToInt(p.getParamValue(cs80::P_lfo1Dest + lfo * 3)) > 0
        && p.getParamValue(cs80::P_lfo1Depth + lfo * 3) > 0.001f;
}

void buildLfos(AdvancedPage& page)
{
    const float xs[3] = { 64.0f, 560.0f, 1056.0f };
    const int rateParams[3] = { cs80::P_lfo1Rate, cs80::P_lfo2Rate, cs80::P_lfo3Rate };
    const int depthParams[3] = { cs80::P_lfo1Depth, cs80::P_lfo2Depth, cs80::P_lfo3Depth };
    const char* const flavour[3] = {
        "Use it for vibrato and tremolo on the whole instrument.",
        "Independent rate and destination. Use it for slow filter drift.",
        "A fast setting gives the CS-80 its characteristic shimmer." };
    // Destination chips in the engine's order, two columns by three rows; OFF closes the grid.
    const int chipOrder[6] = { 1, 2, 3, 4, 5, 0 };
    for (int i = 0; i < 3; ++i)
    {
        const float x = xs[i], y = 108.0f;
        page.module({ x, y, 480.0f, 440.0f }, "LFO " + juce::String(i + 1), true, lfoColour(i),
                    juce::String("A free-running global sine LFO shared by all voices. ") + flavour[i]
                        + " Each LFO drives one destination: click a chip to route it there, or click the lit chip to switch the LFO off. "
                          "The vertical axis of the scope is the swing the engine applies at full depth: PITCH 2 st, VCF 1.6 oct, VCA 0.25 level, "
                          "PAN 0.7, PWM 0.18 pulse width. In PURE CS-80 the modern LFO routing is masked.",
                    [i](Cs80AudioProcessor& p) { return lfoRouted(p, i); });
        page.widget<LfoScope>({ x + 20.0f, y + 42.0f, 440.0f, 196.0f }, i);
        auto* rate = page.knob({ x + 14.0f, y + 250.0f, 130.0f, 134.0f }, rateParams[i], "RATE");
        auto* depth = page.knob({ x + 148.0f, y + 250.0f, 130.0f, 134.0f }, depthParams[i], "DEPTH");
        page.caption("DESTINATION", { x + 288.0f, y + 252.0f, 166.0f, 17.0f });
        for (int k = 0; k < 6; ++k)
            page.lfoChip({ x + 288.0f + (float) (k % 2) * 84.0f, y + 276.0f + (float) (k / 2) * 50.0f, 80.0f, 44.0f }, i, chipOrder[k]);
        page.dimWhen([&proc = page.processor(), i] { return juce::roundToInt(proc.getParamValue(cs80::P_lfo1Dest + i * 3)) == 0; },
                     { rate, depth });
    }
}

void buildWheels(AdvancedPage& page)
{
    page.module({ 64.0f, 564.0f, 480.0f, 332.0f }, "Wheels", true, juce::Colours::transparentBlack,
                "The CS-80's pressure and velocity are separate performance dimensions. Initial touch sets the attack brightness; aftertouch bends the "
                "filter while held. The mod wheel adds sub-oscillator pitch and filter movement; the readouts below give each amount in the engine's "
                "own units. PURE CS-80 masks the modern wheel routing.");
    page.knob({ 139.0f, 604.0f, 150.0f, 140.0f }, cs80::P_modWheelSub, "WHEEL > SUB");
    page.knob({ 319.0f, 604.0f, 150.0f, 140.0f }, cs80::P_modWheelVcf, "WHEEL > VCF");
    page.widget<WheelRouting>({ 80.0f, 764.0f, 448.0f, 112.0f });
}

void buildPitchTuning(AdvancedPage& page)
{
    page.module({ 560.0f, 564.0f, 976.0f, 332.0f }, "Pitch, bend and tuning", true, juce::Colours::transparentBlack,
                "PLAY RANGE: 61-key panel, MIDI notes 0-127. WHEEL BEND is the range of the spring-loaded pitch lever and MIDI pitch bend. With MPE on, "
                "each note's own channel bends by MPE BEND instead; PURE CS-80 disables MPE. A4 sets the reference frequency for twelve-tone equal "
                "temperament; MIDI Tuning Standard and tuning-table import are not implemented here. The ribbon is a separate continuous controller and "
                "returns to centre. The main-panel PITCH knob adds a fine offset, included in the readout.");
    page.knob({ 620.0f, 604.0f, 160.0f, 140.0f }, cs80::P_pitchBendRange, "WHEEL BEND")->setBipolar(true);
    page.custom([](juce::Graphics& g) { etchRect(g, { 878.0f, 598.0f, 340.0f, 152.0f }, 6.0f, 0.5f); });
    page.toggle({ 892.0f, 652.0f, 114.0f, 40.0f }, cs80::P_mpeOn, "MPE");
    auto* mpeBend = page.knob({ 1018.0f, 604.0f, 160.0f, 140.0f }, cs80::P_mpeBendRange, "MPE BEND");
    mpeBend->setBipolar(true);
    page.dimWhen([&proc = page.processor()]
                 { return proc.getParamValue(cs80::P_mpeOn) < 0.5f || proc.getParamValue(cs80::P_parityMode) >= 0.5f; },
                 { mpeBend });
    page.knob({ 1316.0f, 604.0f, 160.0f, 140.0f }, cs80::P_tuningRef, "A4 TUNING")->setBipolar(true);
    page.widget<TuningDisplay>({ 576.0f, 764.0f, 944.0f, 112.0f });
}

void buildSubOsc(AdvancedPage& page)
{
    page.module({ 64.0f, 108.0f, 760.0f, 440.0f }, "Sub oscillator", true, pageColour(1),
                "FUNCTION, SPEED, DEPTH and PRESSURE are set on the main panel; the panel's function and speed follow whichever waveform set is in use. "
                "This page holds what the main panel does not: the switch between the original and legacy waveform sets, and the external level. "
                "PURE CS-80 forces ORIGINAL; the stored switch and the legacy settings are retained but inert. Noise and EXT ignore SPEED. "
                "EXT: optional mono input, disabled by default; host routing required. Six original functions; analog waveform, depth and touch laws "
                "remain provisional. Full depth moves VCO 1.5 st, VCF 1.5 oct, VCA 0.45 level.",
                [](Cs80AudioProcessor& p)
                {
                    const int depths[] = { cs80::P_subVco, cs80::P_subVcf, cs80::P_subVca, cs80::P_subAtVco, cs80::P_subAtVcf };
                    for (int index : depths)
                        if (p.getParamValue(index) > 0.001f)
                            return true;
                    const bool original = p.getParamValue(cs80::P_parityMode) >= 0.5f || p.getParamValue(cs80::P_subOriginalMode) >= 0.5f;
                    return original && juce::roundToInt(p.getParamValue(cs80::P_subOriginalFunction)) == 5 && p.isExternalInputEnabled()
                        && p.getParamValue(cs80::P_subExtLevel) > 0.001f;
                });
    auto* originalMode = page.switchButton({ 88.0f, 192.0f, 236.0f, 40.0f }, cs80::P_subOriginalMode, "STORED ORIGINAL MODE", "ORIGINAL WAVEFORMS");
    originalMode->setComponentID("sub_original_mode");
    originalMode->setTooltip("Original waveforms: on selects the six original functions and 0.7-60 Hz; off selects the legacy set. The choice is stored "
                             "with the patch. PARITY ISOLATION forces original regardless of this stored setting; legacy controls are inert while isolated.");
    page.note("ON    THE SIX ORIGINAL FUNCTIONS, 0.7 - 60 Hz", { 344.0f, 182.0f, 300.0f, 18.0f });
    page.note("OFF   THE LEGACY FUNCTION SET", { 344.0f, 204.0f, 300.0f, 18.0f });
    page.note("PURE CS-80 FORCES THE ORIGINAL SET", { 344.0f, 226.0f, 300.0f, 18.0f });
    auto* extLevel = page.knob({ 656.0f, 146.0f, 140.0f, 134.0f }, cs80::P_subExtLevel, "EXT LEVEL");
    extLevel->getSlider().setDescription("External modulation input LEVEL. The optional mono bus is disabled by default and requires host routing. "
                                         "At maximum LEVEL, a peak 1 passband sine gives nominal inverted modulation peak 1, with 63.5 host samples "
                                         "of causal reconstruction delay. Software normalization, not physical voltage calibration.");
    page.dimWhen([&proc = page.processor()]
                 {
                     const bool original = proc.getParamValue(cs80::P_parityMode) >= 0.5f || proc.getParamValue(cs80::P_subOriginalMode) >= 0.5f;
                     return ! (original && juce::roundToInt(proc.getParamValue(cs80::P_subOriginalFunction)) == 5);
                 },
                 { extLevel });
    page.widget<SubScope>({ 88.0f, 284.0f, 712.0f, 150.0f });
    page.widget<SubRouting>({ 88.0f, 440.0f, 712.0f, 100.0f });
}

void buildRing(AdvancedPage& page)
{
    page.module({ 840.0f, 108.0f, 696.0f, 440.0f }, "Ring modulator", true, pageColour(1),
                "A global ring modulator feeds the output alongside the voices. SPEED, DEPTH, ATTACK, DECAY and MOD are on the main panel; this "
                "plot is where their combined effect is read. The envelope is retriggered by the first key of a phrase. DEPTH sweeps oscillator "
                "frequency; MOD sets the dry/wet mix. The curve shows the carrier frequency in Hz; drag the A node to set ATTACK and DEPTH, the "
                "D node to set DECAY. At zero depth a dashed preview shows the curve at full depth. The analog response remains provisional.",
                [](Cs80AudioProcessor& p) { return p.getParamValue(cs80::P_ringAmount) > 0.001f; });
    page.widget<RingCurve>({ 854.0f, 148.0f, 668.0f, 376.0f });
}

void buildChorus(AdvancedPage& page)
{
    page.module({ 64.0f, 564.0f, 760.0f, 332.0f }, "Chorus / tremolo", true, pageColour(1),
                "SPEED, DEPTH and the CHORUS and TREMOLO switches are on the main panel. Legacy: provisional generic delay line. ENGINE CIRCUIT "
                "and PURE CS-80: OE2 BBD delay path (256 clock periods, printed filters); clock range, wet level and polarity are inferred, not "
                "validated against original hardware.",
                [](Cs80AudioProcessor& p) { return p.getParamValue(cs80::P_chorusOn) >= 0.5f; });
    page.widget<ChorusSweep>({ 78.0f, 604.0f, 732.0f, 276.0f }, 732.0f);
}

void buildAnalogVoice(AdvancedPage& page)
{
    page.module({ 840.0f, 564.0f, 696.0f, 332.0f }, "Analog voice", true, pageColour(1),
                "Dispersion spreads the eight voice cards in tuning and pan; drift slowly moves each card; noise adds a provisional background floor. "
                "Set them to zero for a repeatable render; this is not hardware calibration. The scatter plots the sixteen ranks (eight cards, two "
                "ranks each) on axes that follow the dispersion you set; a ring marks a card that is sounding. At zero dispersion a dashed preview "
                "shows full dispersion.",
                [](Cs80AudioProcessor& p)
                {
                    return p.getParamValue(cs80::P_parityMode) < 0.5f
                        && (p.getParamValue(cs80::P_voiceDispersion) > 0.001f || p.getParamValue(cs80::P_drift) > 0.001f
                            || p.getParamValue(cs80::P_analogNoise) > 0.001f);
                });
    page.knob({ 852.0f, 602.0f, 116.0f, 136.0f }, cs80::P_voiceDispersion, "DISPERSION");
    page.knob({ 968.0f, 602.0f, 116.0f, 136.0f }, cs80::P_drift, "DRIFT");
    page.knob({ 1084.0f, 602.0f, 116.0f, 136.0f }, cs80::P_analogNoise, "NOISE");
    page.widget<VoiceLcd>({ 852.0f, 750.0f, 348.0f, 124.0f }, 348.0f);
    page.widget<VoiceScatter>({ 1212.0f, 602.0f, 308.0f, 276.0f }, 308.0f);
}

void buildFxMacros(AdvancedPage& page, Cs80AudioProcessor& proc)
{
    auto sel = std::make_shared<FxSel>();

    page.label("SIGNAL ORDER", { 64.0f, 102.0f, 260.0f, 16.0f }, 11.0f, gold, Just::centredLeft, 0.16f);
    auto* chain = page.widget<FxChain>({ 64.0f, 120.0f, 1472.0f, 54.0f }, sel);
    sel->peers.push_back(chain);

    const int typeParams[3] = { cs80::P_fx1Type, cs80::P_fx2Type, cs80::P_fx3Type };
    const int mixParams[3] = { cs80::P_fx1Mix, cs80::P_fx2Mix, cs80::P_fx3Mix };
    page.module({ 64.0f, 190.0f, 596.0f, 370.0f }, "Insert effects", true, pageColour(2),
                "Three insert slots, processed in series after the voice mixer and chorus. Tape Delay includes modulation and a filtered feedback "
                "path. Plate Reverb is a dense, bright algorithmic plate. Effects are bypassed at 0% mix and an empty slot passes the signal through; "
                "PURE CS-80 masks them. Pick a slot here or in the signal-order strip to see its response at right.",
                [](Cs80AudioProcessor& p)
                {
                    if (p.getParamValue(cs80::P_parityMode) >= 0.5f)
                        return false;
                    for (int i = 0; i < 3; ++i)
                        if (juce::roundToInt(p.getParamValue(cs80::P_fx1Type + i * 2)) > 0 && p.getParamValue(cs80::P_fx1Mix + i * 2) > 0.001f)
                            return true;
                    return false;
                });
    for (int i = 0; i < 3; ++i)
    {
        const float y0 = 234.0f + 108.0f * (float) i;
        auto* row = page.widget<FxSlotRow>({ 90.0f, y0, 544.0f, 102.0f }, i, sel);
        sel->peers.push_back(row);
        page.choice({ 90.0f + 70.0f, y0 + 30.0f, 170.0f, 34.0f }, typeParams[i], kFxTypes);
        auto* mix = page.knob({ 90.0f + 258.0f, y0 + 8.0f, 100.0f, 87.0f }, mixParams[i], "MIX");
        // The mix does nothing while the slot is empty or PURE CS-80 masks the effects.
        page.dimWhen([&proc, i]
                     { return juce::roundToInt(proc.getParamValue(cs80::P_fx1Type + i * 2)) == 0 || proc.getParamValue(cs80::P_parityMode) >= 0.5f; },
                     { mix });
    }

    page.module({ 676.0f, 190.0f, 860.0f, 370.0f }, "Effect detail", true, pageColour(2), {},
                [sel](Cs80AudioProcessor& p)
                {
                    const int slot = sel->slot;
                    return p.getParamValue(cs80::P_parityMode) < 0.5f && juce::roundToInt(p.getParamValue(cs80::P_fx1Type + slot * 2)) > 0
                        && p.getParamValue(cs80::P_fx1Mix + slot * 2) > 0.001f;
                });
    auto* detail = page.widget<FxDiagram>({ 696.0f, 234.0f, 820.0f, 316.0f }, sel);
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

    page.module({ 64.0f, 576.0f, 1168.0f, 320.0f }, "Performance macros", true, pageColour(2),
                "Macros are patch-level offsets. At centre they are neutral; turning them up pushes brilliance, envelope motion, detune/chorus width "
                "and the sub/ring depth respectively. They never replace the underlying panel settings. Each bar shows the macro's position "
                "(centre = neutral); the rows below give the range each destination can reach, and the amount it has now once the macro is moved.",
                [](Cs80AudioProcessor& p)
                {
                    if (p.getParamValue(cs80::P_parityMode) >= 0.5f)
                        return false;
                    for (int i = 0; i < 4; ++i)
                        if (std::abs(p.getParamValue(cs80::P_macro1 + i) - 0.5f) > 0.005f)
                            return true;
                    return false;
                });
    const char* const macroNames[4] = { "BRILLIANCE", "MOTION", "WIDTH", "SPACE" };
    for (int i = 0; i < 4; ++i)
    {
        const float x = 80.0f + (float) i * 284.0f;
        page.knob({ x + 54.0f, 610.0f, 164.0f, 144.0f }, cs80::P_macro1 + i, macroNames[i])->setBipolar(true);
        page.widget<MacroBars>({ x + 2.0f, 764.0f, 268.0f, 112.0f }, i);
        if (i > 0)
            page.custom([x](juce::Graphics& g) { etchLine(g, x - 8.0f, 612.0f, x - 8.0f, 876.0f, 0.45f); });
    }

    page.module({ 1248.0f, 576.0f, 288.0f, 320.0f }, "Output", true, juce::Colours::transparentBlack,
                "Stereo retains the left and right outputs. Mono averages them after effects and limiting; phase cancellation can change the result. "
                "VOLUME and the MIX I/II crossfade are on the main panel, and the toolbar meter shows the output level. The engine runs at 2x. "
                "Legacy output is DC-blocked and soft-limited. Source modes keep raw source output, which can exceed digital full scale: leave "
                "monitoring headroom.");
    page.outputSwitch({ 1274.0f, 622.0f, 236.0f, 40.0f }, cs80::P_outputMode);
    page.widget<OutputPanel>({ 1264.0f, 678.0f, 256.0f, 200.0f });
}

void buildArp(AdvancedPage& page, Cs80AudioProcessor& proc)
{
    page.module({ 64.0f, 108.0f, 760.0f, 448.0f }, "Arpeggiator", true, pageColour(3),
                "The arpeggiator holds the chord and retriggers the CS-80 voice envelopes at the selected clock. Up/Down, random and played-order "
                "modes cover the patterns used on the classic records. The pattern shows a demonstration chord, not a live readout of the notes "
                "you hold; with the arpeggiator off it is drawn as a dashed preview. PURE CS-80 masks the arpeggiator.",
                [](Cs80AudioProcessor& p) { return p.getParamValue(cs80::P_arpOn) >= 0.5f && p.getParamValue(cs80::P_parityMode) < 0.5f; });
    page.toggle({ 88.0f, 172.0f, 112.0f, 36.0f }, cs80::P_arpOn, "ARP");
    auto* modeCaption = page.captionLive("MODE", { 216.0f, 146.0f, 176.0f, 17.0f });
    auto* mode = page.choice({ 216.0f, 172.0f, 176.0f, 36.0f }, cs80::P_arpMode, kArpModes);
    auto* rate = page.knob({ 408.0f, 146.0f, 128.0f, 124.0f }, cs80::P_arpRate, "RATE");
    auto* oct = page.knob({ 544.0f, 146.0f, 120.0f, 124.0f }, cs80::P_arpOctaves, "OCTAVES");
    auto* clock = page.widget<ArpClock>({ 656.0f, 148.0f, 152.0f, 112.0f });
    page.widget<ArpPattern>({ 88.0f, 280.0f, 712.0f, 250.0f });
    page.dimWhen([&proc] { return proc.getParamValue(cs80::P_arpOn) < 0.5f; }, { modeCaption, mode, rate, oct, clock });
}

void buildPerformance(AdvancedPage& page)
{
    page.module({ 840.0f, 108.0f, 696.0f, 448.0f }, "Performance", true, pageColour(3),
                "KEY RELEASE is a floor under every rank's own release: the longer of the two wins, scaled by the MOTION macro. PORTA (the glide "
                "time), GLIDE (the glide mode), BEND (the initial bend) and SUS II are on the main panel. Portamento glides; Glissando steps in "
                "semitones. Both sustain modes release notes. Sustain II also cuts previous release tails at a new key press. MIDI CC64 hold is a "
                "separate extension, disabled by PURE CS-80. The glide plot shows the pitch path the main-panel settings give, with a dashed "
                "preview while glide is off.",
                [](Cs80AudioProcessor& p)
                {
                    return p.getParamValue(cs80::P_keyRelease) > 0.001f
                        || (p.getParamValue(cs80::P_portamento) > 0.0001f && juce::roundToInt(p.getParamValue(cs80::P_glideMode)) != 0);
                });
    page.knob({ 862.0f, 165.0f, 150.0f, 130.0f }, cs80::P_keyRelease, "KEY RELEASE");
    page.widget<ReleaseCurve>({ 1030.0f, 146.0f, 490.0f, 168.0f });
    page.widget<GlideCurve>({ 864.0f, 326.0f, 648.0f, 208.0f });
}

// --- Render policy: one three-way switch and the table of what it changes ------------------------------
void buildRenderPolicy(AdvancedPage& page, Cs80AudioProcessor& proc)
{
    page.module({ 64.0f, 572.0f, 1472.0f, 324.0f }, "Engine mode", true, pageColour(3),
                "NOT CALIBRATED - no hardware match established. The toolbar's ENGINE switch selects CIRCUIT (SOURCE PERFORMANCE below) or LEGACY; "
                "its PURE CS-80 switch is PARITY ISOLATION. SOURCE: current source models with MIDI, optional MPE and the modern overlays. "
                "ISOLATION wins when both are on: bend, CC1/11/64/74 and MPE are off and the modern effects are masked; pressure, ribbon and the "
                "on-screen pedal stay active. An effective mode change stops notes; keep headroom. The stored source request is retained but masked "
                "while isolation is on. Library programs reset the source request off and return both rank selectors to PANEL."
                "\n\nTERMS IN THE TABLE. The highlighted column is the policy in force. STUDY TABLE: the older hand-voiced table of tone studies. "
                "SOURCE MODELS: the circuit-informed voice models; NOMINAL uses their nominal component values. STORED MODE: the Sub oscillator's "
                "own ORIGINAL WAVEFORMS switch. GENERIC DELAY: the older stand-in chorus. OE2 BBD: a bucket-brigade delay chorus that follows the "
                "printed OE2 circuit. DC BLOCK + SOFT LIMIT: the legacy output stage; RAW leaves the source output untouched."
                "\n\nABOUT. A circuit-informed recreation of the Yamaha CS-80 (1977): eight voices, each with two independent ranks, for sixteen "
                "oscillator/filter/VCA chains. The signal path follows the original's high-pass into low-pass topology, linear VCA and global "
                "sub-oscillator. The primary panel exposes the controls a player touches; the Studio view exposes the modern layer (LFOs, MPE, effects, "
                "dispersion). All controls are host-automatable and saved in plugin state, except transient performance gestures. The library has "
                "22 legacy tone studies and 200 performance patches; loading one returns both ranks to PANEL and resets SOURCE PERFORMANCE off. "
                "Touch the ribbon above the keyboard to bend pitch; double-click any control to reset it, right-click it for MIDI learn, typed "
                "values and help.",
                [](Cs80AudioProcessor& p)
                { return p.getParamValue(cs80::P_parityMode) >= 0.5f || p.getParamValue(cs80::P_sourcePerformance) >= 0.5f; });
    // The three switches are the column heads of the table below them.
    page.label("WHAT EACH POLICY CHANGES", { 96.0f, 618.0f, 290.0f, 22.0f }, 12.5f, gold, Just::centredLeft, 0.1f);
    page.custom([](juce::Graphics& g)
    { text(g, "THE HIGHLIGHTED COLUMN IS IN FORCE", { 96.0f, 642.0f, 290.0f, 20.0f }, 12.5f, false, panelInk(), Just::centredLeft, 0.02f); });
    auto* legacy = page.policyCell({ 396.0f, 612.0f, 368.0f, 54.0f }, 0, "LEGACY", "STUDY VOICING, OVERLAYS LIVE");
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

    auto* sourcePerformance = page.policyCell({ 772.0f, 612.0f, 368.0f, 54.0f }, 1, "SOURCE PERFORMANCE", "SOURCE MODELS, OVERLAYS LIVE");
    sourcePerformance->setComponentID("source_performance");
    sourcePerformance->setTitle("Source performance (uncalibrated)");
    sourcePerformance->setTooltip("Opt-in source-performance policy: current source models with ordinary MIDI pitch bend, CC1/11/64/74, optional MPE and "
                                  "modern software overlays (macros, FX, pan and bypass). Not calibrated; no hardware match established. Keep output "
                                  "headroom. An effective mode change stops notes and clears controller/effect history. PARITY ISOLATION wins if both "
                                  "switches are ON; changing this source request while isolation stays ON is masked and does not stop notes. The source "
                                  "request is saved with sound presets; library programs reset it OFF and return both selectors to PANEL. Selected tones "
                                  "and memories still own only the 26 programmer controls; footage and global controls remain live.");
    sourcePerformance->setDescription(sourcePerformance->getTooltip());

    auto* parity = page.policyCell({ 1148.0f, 612.0f, 368.0f, 54.0f }, 2, "PARITY ISOLATION", "NOMINAL MODELS, OVERLAYS MASKED");
    parity->setComponentID("parity_mode");
    parity->setTooltip("Comparison fixture, not calibrated. No hardware match established. Enable before playing: toggling stops notes and clears "
                       "controller/effect history. MIDI pitch bend, CC1/11/64/74 and MPE are ignored; pressure, ribbon and the on-screen pedal "
                       "remain active. Hardware factory tabs use nominal source models; library programs still return to PANEL. Modern settings are "
                       "retained but inert. Isolation takes priority over SOURCE PERFORMANCE; its stored source request is masked, not cleared. "
                       "Output may exceed digital full scale.");
    parity->setDescription(parity->getTooltip());

    page.widget<PolicyTable>({ 80.0f, 672.0f, 1440.0f, 184.0f });

    // About: one line; the long text is in the module's info card.
    page.custom([](juce::Graphics& g)
    {
        etchLine(g, 96.0f, 861.0f, 1504.0f, 861.0f, 0.4f);
        engrave(g, "CS-80 REFERENCE", { 96.0f, 862.0f, 210.0f, 30.0f }, 14.0f, true, ivory, Just::centredLeft, 0.08f);
        text(g, "ENGINE  2x OVERSAMPLED, 8 VOICES x 2 RANKS", { 320.0f, 862.0f, 420.0f, 30.0f }, 12.5f, true, amber, Just::centredLeft, 0.02f);
        text(g, "NOT CALIBRATED  -  NO HARDWARE MATCH ESTABLISHED", { 760.0f, 862.0f, 640.0f, 30.0f }, 12.5f, true, gold, Just::centredLeft, 0.04f);
    });
}

// ---- The Studio tabs -------------------------------------------------------------------------------------
void buildModulationTab(AdvancedPage& page)
{
    buildLfos(page);
    buildWheels(page);
    page.modelNotes({ 560.0f, 564.0f, 976.0f, 332.0f }, "Modulation models", { "Sub oscillator", "Ring modulator", "Modern layer" });
}

void buildSubRingTab(AdvancedPage& page)
{
    buildSubOsc(page);
    buildRing(page);
    buildChorus(page);
    buildAnalogVoice(page);
}

void buildKeyboardTab(AdvancedPage& page, Cs80AudioProcessor& proc, std::function<QwertyKeyboard*()> qwerty)
{
    page.module({ 64.0f, 108.0f, 560.0f, 440.0f }, "Computer keyboard", true, pageColour(1),
                "Play the instrument from the computer keyboard. The middle row A S D F G H J K L ; plays the white keys from C, the row above "
                "W E T Y U O P the black keys. Z and X move the whole map an octave down or up; C and V lower or raise the velocity in steps of "
                "10 %. The notes go through the on-screen keyboard (MIDI channel 1), so they light its keys and play exactly as clicks do. "
                "While a text field has the keyboard (preset search, a typed value) the keys type text instead; chords with Ctrl or Cmd stay "
                "menu shortcuts. A note is released when its key comes up or the window loses the keyboard.",
                [](Cs80AudioProcessor& p) { return p.getEditorState().qwertyEnabled.load(); });
    page.adopt(std::make_unique<QwertyPanel>(proc, std::move(qwerty)), { 88.0f, 150.0f, QwertyPanel::kDesignW, QwertyPanel::kDesignH });
    page.module({ 640.0f, 108.0f, 896.0f, 440.0f }, "MIDI learn", true, pageColour(1),
                "Right-click any control and choose MIDI learn, then move a knob, slider or pedal on your MIDI controller: from then on that "
                "controller drives the control, and the host sees the moves as ordinary parameter changes. A controller drives one control and "
                "a control follows one controller; learning again moves it. A learned controller no longer has its usual MIDI meaning (for "
                "example CC 1 stops being the mod wheel). The map belongs to this session: it is saved with the host project, not with presets. "
                "Bank select, data entry, RPN/NRPN and channel-mode controllers are never learned. PURE CS-80 switches MIDI learn off and keeps "
                "the map for later.",
                [](Cs80AudioProcessor& p) { return p.getParamValue(cs80::P_parityMode) < 0.5f && p.getMidiLearn().count() > 0; });
    page.adopt(std::make_unique<MidiLearnPanel>(proc), { 664.0f, 150.0f, MidiLearnPanel::kDesignW, MidiLearnPanel::kDesignH });
    {
        AdvancedPage::Origin moved(page, -496.0f, 0.0f);
        buildPitchTuning(page);
    }
    page.modelNotes({ 1056.0f, 564.0f, 480.0f, 332.0f }, "Keyboard and touch models",
                    { "Keyboard scan and assignment", "Initial touch and aftertouch", "Ribbon" });
}

void buildArpTab(AdvancedPage& page, Cs80AudioProcessor& proc)
{
    buildArp(page, proc);
    buildPerformance(page);
    page.modelNotes({ 64.0f, 572.0f, 1472.0f, 324.0f }, "Envelope and voice models", { "Envelopes", "VCA", "Modern layer" });
}

void buildSettingsTab(AdvancedPage& page, Cs80AudioProcessor& proc)
{
    {
        AdvancedPage::Origin moved(page, 0.0f, -464.0f);
        buildRenderPolicy(page, proc);
    }
    page.evidenceRegister({ 64.0f, 448.0f, 1472.0f, 448.0f });
}

// Footnotes per built-in page: what lives on the main panel.
juce::String footNote(StudioTab tab, bool subRing)
{
    switch (tab)
    {
        case StudioTab::modulation:
            return subRing ? "SET ON THE MAIN PANEL:  SUB OSC AND RING MOD CONTROLS, CHORUS SPEED AND DEPTH" : juce::String();
        case StudioTab::keyboard: return "THE RIBBON AND KEYBOARD CONTOURS ARE ON THE MAIN PANEL";
        case StudioTab::arp: return "SET ON THE MAIN PANEL:  PORTA (TIME), GLIDE (MODE), BEND AND SUS II";
        case StudioTab::settings: return "NOT CALIBRATED  -  NO HARDWARE MATCH ESTABLISHED";
        case StudioTab::fx:
        case StudioTab::count: break;
    }
    return {};
}
} // namespace

// ---------------------------------------------------------------------------
// The Studio view
// ---------------------------------------------------------------------------

namespace
{
// Built-in pages of the former Advanced panel, by legacy index.
constexpr int kLegacyPages = 4;
} // namespace

StudioView::StudioView(Cs80AudioProcessor& p)
    : processor(p)
{
    setVisible(false);
    setTitle("Studio view");

    chassis = std::make_unique<AdvChassis>();
    addAndMakeVisible(*chassis);

    const char* const captions[] = { "LFOs, wheels, sub oscillator, ring modulator, chorus and analog voice",
                                     "computer keyboard, MIDI learn, pitch bend, MPE and tuning",
                                     "insert effects, macros and output", "arpeggiator, glide and key release",
                                     "engine mode and the evidence register" };
    for (int i = 0; i < kTabs; ++i)
    {
        auto tab = std::make_unique<AdvPageTab>(i, tabName((StudioTab) i), captions[i]);
        tab->onClick = [this, i] { showTab((StudioTab) i, activeSub[(size_t) i]); };
        tab->onStep = [this](int direction) { stepTab(direction); };
        addAndMakeVisible(*tab);
        tabs[(size_t) i] = std::move(tab);
    }
    for (int i = 0; i < kMaxSubTabs; ++i)
    {
        auto key = std::make_unique<StudioSubTab>();
        key->setExplicitFocusOrder(kTabs + 1 + i);
        key->onClick = [this, i] { showTab(activeTab, i); };
        key->onStep = [this](int direction) { stepSubPage(direction); };
        addChildComponent(*key);
        subTabs[(size_t) i] = std::move(key);
    }

    auto builtIn = [this](StudioTab tab, const juce::String& name, const std::function<void(AdvancedPage&)>& build)
    {
        auto page = std::make_unique<AdvancedPage>(processor);
        build(*page);
        auto* raw = page.get();
        mount(tab, std::move(page), name, false);
        pagesOf(tab).back().builtIn = raw;
        return raw;
    };
    std::array<AdvancedPage*, kLegacyPages> legacy {};
    legacy[0] = builtIn(StudioTab::modulation, "LFO / WHEELS", buildModulationTab);
    legacy[1] = builtIn(StudioTab::modulation, "SUB / RING", buildSubRingTab);
    builtIn(StudioTab::keyboard, "KEYBOARD", [this](AdvancedPage& page) { buildKeyboardTab(page, processor, [this] { return qwerty; }); });
    legacy[2] = builtIn(StudioTab::fx, "EFFECTS", [this](AdvancedPage& page) { buildFxMacros(page, processor); });
    builtIn(StudioTab::arp, "ARPEGGIO", [this](AdvancedPage& page) { buildArpTab(page, processor); });
    legacy[3] = builtIn(StudioTab::settings, "SETTINGS", [this](AdvancedPage& page) { buildSettingsTab(page, processor); });
    legacyPages = legacy;

    titleBlock = std::make_unique<AdvTitleBlock>();
    addAndMakeVisible(*titleBlock);
    footer = std::make_unique<AdvFooterStatus>();
    addAndMakeVisible(*footer);

    closeButton = std::make_unique<AdvCloseButton>("CLOSE");
    closeButton->setExplicitFocusOrder(kTabs + kMaxSubTabs + 1);
    closeButton->onClick = [this]
    {
        if (onClose)
            onClose();
    };
    addAndMakeVisible(*closeButton);

    info = std::make_unique<AdvInfoLayer>();
    addChildComponent(*info);
    for (auto& list : tabPages)
        for (auto& entry : list)
            if (entry.builtIn != nullptr)
                entry.builtIn->onInfo = [this](const juce::String& title, const juce::String& body, juce::Component* from)
                {
                    info->open(title, body, from);
                };

    showTab(StudioTab::modulation, 0);
}

StudioView::~StudioView()
{
    stopTimer();
}

juce::String StudioView::tabName(StudioTab tab)
{
    switch (tab)
    {
        case StudioTab::modulation: return "MODULATION";
        case StudioTab::keyboard: return "KEYBOARD";
        case StudioTab::fx: return "FX";
        case StudioTab::arp: return "ARP";
        case StudioTab::settings: return "SETTINGS";
        case StudioTab::count: break;
    }
    return {};
}

juce::Rectangle<float> StudioView::contentDesignArea() noexcept
{
    // Below the header rail (y 96) and above the footer rail (y 904), inside the cheek columns.
    return { 52.0f, 100.0f, kW - 104.0f, 800.0f };
}

juce::Rectangle<int> StudioView::getContentBounds() const
{
    const auto a = contentDesignArea();
    const float sx = (float) getWidth() / kW, sy = (float) getHeight() / kH;
    return { juce::roundToInt(a.getX() * sx), juce::roundToInt(a.getY() * sy),
             juce::roundToInt(a.getWidth() * sx), juce::roundToInt(a.getHeight() * sy) };
}

void StudioView::mount(StudioTab tab, std::unique_ptr<juce::Component> page, const juce::String& name, bool first)
{
    if (page == nullptr || tab == StudioTab::count)
        return;
    auto& list = pagesOf(tab);
    if (list.size() >= (size_t) kMaxSubTabs)
    {
        // Three sub-page keys fit the header. Remove a built-in page first (removeBuiltInPage) to make room.
        jassertfalse;
        return;
    }
    SubPage entry;
    entry.component = page.get();
    entry.name = name.isNotEmpty() ? name.toUpperCase()
               : page->getTitle().isNotEmpty() ? page->getTitle().toUpperCase() : tabName(tab);
    entry.owned = std::move(page);
    addChildComponent(*entry.component);
    list.insert(first ? list.begin() : list.end(), std::move(entry));
    if (info != nullptr)
        info->toFront(false);
    layoutPages();
    if (titleBlock != nullptr)
        showTab(activeTab, first && tab == activeTab ? 0 : activeSub[(size_t) activeTab]);
}

void StudioView::setPage(StudioTab tab, std::unique_ptr<juce::Component> page, const juce::String& name)
{
    mount(tab, std::move(page), name, true);
    if (tab != StudioTab::count)
        activeSub[(size_t) tab] = 0;
}

void StudioView::addPage(StudioTab tab, std::unique_ptr<juce::Component> page, const juce::String& name)
{
    mount(tab, std::move(page), name, false);
}

void StudioView::removeBuiltInPage(StudioTab tab)
{
    if (tab == StudioTab::count)
        return;
    auto& list = pagesOf(tab);
    const auto external = std::count_if(list.begin(), list.end(), [](const SubPage& e) { return e.builtIn == nullptr; });
    if (external == 0)
        return;
    for (auto it = list.begin(); it != list.end();)
    {
        if (it->builtIn != nullptr)
        {
            for (auto& slot : legacyPages)
                if (slot == it->builtIn)
                    slot = nullptr;
            removeChildComponent(it->component);
            it = list.erase(it);
        }
        else
            ++it;
    }
    activeSub[(size_t) tab] = 0;
    showTab(activeTab, activeSub[(size_t) activeTab]);
}

juce::Component* StudioView::getSubPage(StudioTab tab, int index) const noexcept
{
    if (tab == StudioTab::count)
        return nullptr;
    const auto& list = pagesOf(tab);
    return index >= 0 && index < (int) list.size() ? list[(size_t) index].component : nullptr;
}

void StudioView::showTab(StudioTab tab, int subPage)
{
    if (tab == StudioTab::count)
        return;
    if (info != nullptr)
        info->close();
    activeTab = tab;
    auto& list = pagesOf(tab);
    activeSub[(size_t) tab] = juce::jlimit(0, juce::jmax(0, (int) list.size() - 1), subPage);
    for (int t = 0; t < kTabs; ++t)
    {
        tabs[(size_t) t]->setToggleState(t == (int) tab, juce::dontSendNotification);
        auto& pages = tabPages[(size_t) t];
        for (int s = 0; s < (int) pages.size(); ++s)
            pages[(size_t) s].component->setVisible(t == (int) tab && s == activeSub[(size_t) t]);
    }
    refreshSubTabs();
    const auto& current = list[(size_t) activeSub[(size_t) tab]];
    titleBlock->setPage((int) tab, list.size() < 2);
    footer->setNote(current.builtIn != nullptr ? footNote(tab, current.builtIn == legacyPages[1]) : juce::String());
    if (current.builtIn != nullptr)
        current.builtIn->refresh();
    repaint();
}

void StudioView::refreshSubTabs()
{
    const auto& list = pagesOf(activeTab);
    const bool show = list.size() > 1;
    for (int i = 0; i < kMaxSubTabs; ++i)
    {
        auto& key = *subTabs[(size_t) i];
        const bool visible = show && i < (int) list.size();
        key.setVisible(visible);
        if (! visible)
            continue;
        key.setButtonText(list[(size_t) i].name);
        key.setToggleState(i == activeSub[(size_t) activeTab], juce::dontSendNotification);
        key.setTitle(tabName(activeTab) + " sub-page: " + list[(size_t) i].name);
        key.setTooltip(list[(size_t) i].name + "  -  page " + juce::String(i + 1) + " of " + juce::String((int) list.size())
                       + " in " + tabName(activeTab) + ". Left and Right arrows move between pages.");
    }
    resized();
}

void StudioView::showPage(int legacyIndex)
{
    auto* target = legacyIndex >= 0 && legacyIndex < kLegacyPages ? legacyPages[(size_t) legacyIndex] : nullptr;
    for (int t = 0; t < kTabs; ++t)
    {
        const auto& list = tabPages[(size_t) t];
        for (int s = 0; s < (int) list.size(); ++s)
            if (target != nullptr && list[(size_t) s].builtIn == target)
            {
                showTab((StudioTab) t, s);
                return;
            }
    }
    showTab(StudioTab::modulation, 0);
}

int StudioView::getActivePage() const noexcept
{
    const auto& list = pagesOf(activeTab);
    const auto* current = list.empty() ? nullptr : list[(size_t) activeSub[(size_t) activeTab]].builtIn;
    for (int i = 0; i < kLegacyPages; ++i)
        if (current != nullptr && legacyPages[(size_t) i] == current)
            return i;
    return -1;
}

void StudioView::stepTab(int direction)
{
    const int next = ((int) activeTab + direction + kTabs) % kTabs;
    showTab((StudioTab) next, activeSub[(size_t) next]);
    tabs[(size_t) next]->grabKeyboardFocus();
}

void StudioView::stepSubPage(int direction)
{
    const int count = (int) pagesOf(activeTab).size();
    if (count < 2)
        return;
    const int next = (activeSub[(size_t) activeTab] + direction + count) % count;
    showTab(activeTab, next);
    subTabs[(size_t) next]->grabKeyboardFocus();
}

bool StudioView::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && info->isVisible())
    {
        info->close();
        return true;
    }
    const int code = key.getKeyCode();
    if (key.getModifiers().isCommandDown() && code >= '1' && code < '1' + kTabs)
    {
        showTab((StudioTab) (code - '1'), activeSub[(size_t) (code - '1')]);
        return true;
    }
    return false;
}

void StudioView::visibilityChanged()
{
    if (isVisible())
    {
        const auto& current = pagesOf(activeTab)[(size_t) activeSub[(size_t) activeTab]];
        if (current.builtIn != nullptr)
            current.builtIn->refresh();
        startTimerHz(15);
    }
    else
    {
        if (info != nullptr)
            info->close();
        stopTimer();
    }
}

void StudioView::timerCallback()
{
    const auto& current = pagesOf(activeTab)[(size_t) activeSub[(size_t) activeTab]];
    if (current.builtIn != nullptr)
        current.builtIn->refresh();
}

void StudioView::paint(juce::Graphics&)
{
    // Nothing: the chassis paints the body and the editor's backdrop shows through the cheek columns.
}

void StudioView::layoutPages()
{
    const auto content = getContentBounds();
    for (auto& list : tabPages)
        for (auto& entry : list)
            entry.component->setBounds(entry.builtIn != nullptr ? getLocalBounds() : content);
}

void StudioView::resized()
{
    const float sx = (float) getWidth() / kW;
    const float sy = (float) getHeight() / kH;
    auto r = [sx, sy](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                                    juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };
    chassis->setBounds(getLocalBounds());
    for (int i = 0; i < kTabs; ++i)
        tabs[(size_t) i]->setBounds(r(kTabX0 + (float) i * kTabPitch - AdvPageTab::kPad, kTabY - AdvPageTab::kPad,
                                      kTabPlateW + 2.0f * AdvPageTab::kPad, kTabPlateH + 2.0f * AdvPageTab::kPad));
    layoutPages();
    // Sub-page keys share the title block's bottom line, x 64 to 350.
    const int keys = juce::jmin(kMaxSubTabs, (int) pagesOf(activeTab).size());
    if (keys > 1)
    {
        const float gap = 6.0f, w = (286.0f - gap * (float) (keys - 1)) / (float) keys;
        for (int i = 0; i < keys; ++i)
            subTabs[(size_t) i]->setBounds(r(64.0f + (float) i * (w + gap), 56.0f, w, 36.0f));
    }
    titleBlock->setBounds(r(64.0f, 14.0f, 286.0f, 78.0f));
    footer->setBounds(r(64.0f, 908.0f, 1472.0f, 26.0f));
    closeButton->setBounds(r(1420.0f, 26.0f, 116.0f, 38.0f));
    info->setBounds(getLocalBounds());
}

} // namespace cs80::ui
