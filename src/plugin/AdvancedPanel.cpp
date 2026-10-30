#include "AdvancedPanel.h"

#include "PluginProcessor.h"
#include "core/Dsp.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
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

const juce::StringArray kDestinations { "OFF", "PITCH", "VCF", "VCA", "PWM", "PAN" };
const juce::StringArray kFxTypes { "OFF", "CHORUS", "PHASER", "TAPE DELAY", "PLATE REVERB", "DRIVE" };
const juce::StringArray kSubWaves { "SINE", "TRIANGLE", "SAW", "SQUARE", "S&H", "RANDOM" };
const juce::StringArray kOriginalSubWaves { "SINE", "SAW DOWN", "SAW UP", "SQUARE", "WHITE NOISE", "EXT" };
const juce::StringArray kArpModes { "UP", "DOWN", "UP / DOWN", "RANDOM", "PLAYED" };
const juce::StringArray kGlideModes { "OFF", "PORTAMENTO", "GLISSANDO" };

const juce::Colour kInk { 0xff0a0907 };          // deep glass black
const juce::Colour kGlow { 0xfff3d98a };         // lit amber glass, top
const juce::Colour kGlowDeep { 0xffc98f2a };     // lit amber glass, bottom

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

void paragraph(juce::Graphics& g, const juce::String& s, Rect r, float h, juce::Colour c,
               bool bold = false, float spacing = 1.6f)
{
    juce::AttributedString a;
    a.setJustification(Just::topLeft);
    a.setWordWrap(juce::AttributedString::byWord);
    a.setLineSpacing(spacing);
    a.append(s, fnt(h, bold), c);
    a.draw(g, r);
}

// A footnote: small amber hairline in the margin, dim cream copy.
void footnote(juce::Graphics& g, const juce::String& s, Rect r, float h = 10.6f,
              juce::Colour c = ivoryDim, bool bar = true)
{
    if (bar)
    {
        g.setColour(amber.withAlpha(0.55f));
        g.fillRect(r.getX(), r.getY() + 1.0f, 2.0f, juce::jmin(r.getHeight(), 12.0f));
    }
    paragraph(g, s, r.withTrimmedLeft(bar ? 9.0f : 0.0f), h, c);
}

// Gold etched bracket carrying a cream title, as on the main control surface.
void bracketTitle(juce::Graphics& g, float x0, float x1, float y, const juce::String& title,
                  float size = 13.0f)
{
    const float tw = textWidth(title, size, true, 0.14f) + 26.0f;
    const float mid = (x0 + x1) * 0.5f;
    etchLine(g, x0, y, mid - tw * 0.5f, y, 0.8f);
    etchLine(g, mid + tw * 0.5f, y, x1, y, 0.8f);
    etchLine(g, x0, y, x0, y + 7.0f, 0.8f);
    etchLine(g, x1, y, x1, y + 7.0f, 0.8f);
    engrave(g, title, { mid - tw * 0.5f, y - 10.0f, tw, 20.0f }, size, true, ivory, Just::centred, 0.14f);
}

// A raised charcoal sub-panel with a cast shadow, lit upper-left edge, dark
// lower-right edge, screwed down at the corners.
void drawModule(juce::Graphics& g, Rect r, const juce::String& title, bool primary)
{
    constexpr float rad = 6.0f;
    softShadow(g, r, rad, 0.0f, 4.0f, 8.0f, 0.62f);
    juce::Path p;
    p.addRoundedRectangle(r, rad);
    g.saveState();
    g.reduceClipRegion(p);
    fillPanelFace(g, r);
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
    bracketTitle(g, r.getX() + 32.0f, r.getRight() - 32.0f, r.getY() + 22.0f, title.toUpperCase(),
                 primary ? 13.5f : 12.5f);
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
void inertOverlay(juce::Graphics& g, Rect r)
{
    g.saveState();
    g.reduceClipRegion(r.toNearestInt());
    g.setColour(kInk.withAlpha(0.5f));
    g.fillRect(r);
    g.setColour(amber.withAlpha(0.10f));
    for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 10.0f)
        g.drawLine(x, r.getBottom(), x + r.getHeight(), r.getY(), 1.3f);
    g.restoreState();
    const Rect badge = Rect(0.0f, 0.0f, 168.0f, 17.0f).withCentre(r.getCentre());
    g.setColour(kInk.withAlpha(0.9f));
    g.fillRoundedRectangle(badge, 3.0f);
    g.setColour(amber.withAlpha(0.7f));
    g.drawRoundedRectangle(badge, 3.0f, 1.0f);
    text(g, "ISOLATION  -  CONTROL INERT", badge, 9.2f, true, amber, Just::centred, 0.08f);
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

juce::String noteName(int midi)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String(names[((midi % 12) + 12) % 12]) + juce::String(midi / 12 - 1);
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

    float v(int index) const { return proc.getParamValue(index); }
    bool isolated() const { return v(cs80::P_parityMode) >= 0.5f; }
    // 0 legacy, 1 source performance, 2 isolation (isolation wins).
    int policy() const
    {
        return v(cs80::P_parityMode) >= 0.5f ? 2 : (v(cs80::P_sourcePerformance) >= 0.5f ? 1 : 0);
    }

    Cs80AudioProcessor& proc;
    float designW, designH;

private:
    Sig last {};
    bool haveLast = false;
};

} // namespace

namespace
{
// ---- LFO scope: sine LFO at the set rate, ghost = full scale, trace = depth ----------
class LfoScope final : public LiveWidget
{
public:
    LfoScope(Cs80AudioProcessor& p, int index)
        : LiveWidget(p, 440.0f, 200.0f), n(index)
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
            vline(g, x, plot.getY(), plot.getBottom(), i == 0 || i == 4 ? 0.16f : 0.07f);
            text(g, i == 4 ? "4 s" : juce::String(i), { x - 12.0f, plot.getBottom() + 2.0f, 24.0f, 12.0f },
                 9.0f, false, ivoryDim.withAlpha(0.7f), Just::centred);
        }
        hline(g, plot.getX(), plot.getRight(), plot.getCentreY(), 0.22f);
        hline(g, plot.getX(), plot.getRight(), plot.getY() + 4.0f, 0.07f);
        hline(g, plot.getX(), plot.getRight(), plot.getBottom() - 4.0f, 0.07f);
        text(g, "SINE  -  4 S WINDOW  -  FAINT = FULL DEPTH", { 14.0f, 6.0f, 280.0f, 14.0f },
             9.0f, true, ivoryDim.withAlpha(0.8f), Just::centredLeft, 0.04f);
    }

private:
    static Rect plotRect() { return { 14.0f, 26.0f, 412.0f, 150.0f }; }

    void sample(Sig& s) const override
    {
        s[0] = v(rateIdx); s[1] = v(depthIdx); s[2] = v(destIdx); s[3] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const float rate = v(rateIdx), depth = v(depthIdx);
        const int dest = juce::jlimit(0, 5, juce::roundToInt(v(destIdx)));
        const bool iso = isolated();
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
        g.setColour(ivory.withAlpha(0.20f));
        g.strokePath(build(1.0f), juce::PathStrokeType(1.0f));

        const bool live = dest > 0 && depth > 0.001f && ! iso;
        auto trace = build(depth);
        if (live)
        {
            auto area = trace;
            area.lineTo(plot.getRight(), cy);
            area.lineTo(plot.getX(), cy);
            area.closeSubPath();
            g.setColour(amber.withAlpha(0.15f));
            g.fillPath(area);
            g.setColour(amberBright.withAlpha(0.35f));
            g.strokePath(trace, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour(live ? amberBright : ivoryDim.withAlpha(0.85f));
        g.strokePath(trace, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        text(g, juce::String(rate, rate < 10.0f ? 2 : 1) + " Hz   T " + periodString(1.0f / juce::jmax(0.05f, rate)),
             { 296.0f, 6.0f, 130.0f, 14.0f }, 9.5f, true, amber, Just::centredRight, 0.03f);
        if (! live && ! iso)
            text(g, dest == 0 ? "NO DESTINATION" : "DEPTH AT ZERO", { 14.0f, plot.getY() + 4.0f, 412.0f, 16.0f },
                 9.0f, true, ivoryDim, Just::centred, 0.14f);
        if (iso)
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }

    int n, rateIdx, depthIdx, destIdx;
};

// ---- LFO target readout: what one unit of depth does in the engine --------------------
class LfoReadout final : public LiveWidget
{
public:
    LfoReadout(Cs80AudioProcessor& p, int index)
        : LiveWidget(p, 188.0f, 50.0f), n(index) {}

private:
    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_lfo1Depth + n * 3); s[1] = v(cs80::P_lfo1Dest + n * 3); s[2] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const float depth = v(cs80::P_lfo1Depth + n * 3);
        const int dest = juce::jlimit(0, 5, juce::roundToInt(v(cs80::P_lfo1Dest + n * 3)));
        const bool live = dest > 0 && ! isolated();
        // Engine scale factors: pitch 2 st, filter 1.6 oct, level 0.25, pan 0.7, PWM 0.18.
        static const float scale[] = { 0.0f, 2.0f, 1.6f, 0.25f, 0.7f, 0.18f };
        static const char* const unit[] = { "", " st", " oct", " lvl", " pan", " pw" };
        static const char* const what[] = { "NO TARGET", "OSCILLATOR PITCH", "FILTER CUTOFF", "VOICE LEVEL",
                                            "PAN POSITION", "PULSE WIDTH" };
        const juce::String big = dest == 0 ? juce::String("OFF")
            : juce::String(juce::CharPointer_UTF8("\xc2\xb1")) + juce::String(depth * scale[dest], 2) + unit[dest];
        text(g, big, { 12.0f, 5.0f, 168.0f, 26.0f }, 19.0f, true, live ? amberBright : ivoryDim.withAlpha(0.7f),
             Just::centredLeft, 0.02f);
        text(g, juce::String(what[dest]) + "  -  DEPTH " + juce::String(juce::roundToInt(depth * 100.0f)) + "%",
             { 12.0f, 31.0f, 168.0f, 12.0f }, 8.8f, true, ivoryDim, Just::centredLeft, 0.06f);
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRect(12.0f, 45.0f, 164.0f, 2.5f);
        g.setColour(live ? amber : ivoryDim.withAlpha(0.5f));
        g.fillRect(12.0f, 45.0f, 164.0f * depth, 2.5f);
    }

    int n;
};

// ---- Mod wheel routing ------------------------------------------------------------------
class WheelRouting final : public LiveWidget
{
public:
    explicit WheelRouting(Cs80AudioProcessor& p) : LiveWidget(p, 176.0f, 132.0f) {}

private:
    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_modWheelSub); s[1] = v(cs80::P_modWheelVcf); s[2] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const float sub = v(cs80::P_modWheelSub), vcf = v(cs80::P_modWheelVcf);
        text(g, "MOD WHEEL AT FULL TRAVEL", { 12.0f, 7.0f, 156.0f, 14.0f }, 9.0f, true, ivoryDim, Just::centredLeft, 0.07f);
        auto row = [&](float y, const juce::String& label, float amount, const juce::String& result)
        {
            text(g, label, { 12.0f, y, 156.0f, 14.0f }, 9.0f, true, ivory, Just::centredLeft, 0.05f);
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.fillRoundedRectangle(12.0f, y + 19.0f, 152.0f, 6.0f, 3.0f);
            g.setColour(isolated() ? ivoryDim.withAlpha(0.4f) : amber);
            g.fillRoundedRectangle(12.0f, y + 19.0f, juce::jmax(4.0f, 152.0f * amount), 6.0f, 3.0f);
            text(g, result, { 12.0f, y + 29.0f, 156.0f, 16.0f }, 11.5f, true, amberBright, Just::centredLeft, 0.03f);
        };
        row(28.0f, "SUB OSC  >  PITCH", sub, "up to " + juce::String(juce::CharPointer_UTF8("\xc2\xb1")) + juce::String(sub * 2.0f, 2) + " st");
        row(76.0f, "WHEEL  >  VCF CUTOFF", vcf, "up to +" + juce::String(vcf * 1.5f, 2) + " oct");
        if (isolated())
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }
};

// ---- Tuning readout --------------------------------------------------------------------------
class TuningDisplay final : public LiveWidget
{
public:
    explicit TuningDisplay(Cs80AudioProcessor& p) : LiveWidget(p, 340.0f, 74.0f) {}

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
        text(g, "SOUNDING A4", { 14.0f, 6.0f, 120.0f, 12.0f }, 9.0f, true, ivoryDim, Just::centredLeft, 0.08f);
        text(g, juce::String(a4, 2) + " Hz", { 14.0f, 17.0f, 190.0f, 30.0f }, 24.0f, true, amberBright);
        text(g, "C4  " + juce::String(a4 * std::pow(2.0f, -9.0f / 12.0f), 2) + " Hz", { 14.0f, 50.0f, 190.0f, 14.0f },
             9.5f, true, ivoryDim, Just::centredLeft, 0.04f);
        text(g, iso ? "REFERENCE  440.0 Hz (ISOLATION)" : "REFERENCE  " + juce::String(ref, 1) + " Hz",
             { 196.0f, 18.0f, 138.0f, 14.0f }, 9.2f, true, ivory, Just::centredRight, 0.03f);
        text(g, "MASTER TUNE  " + signedString(mt, 2, " st"), { 196.0f, 34.0f, 138.0f, 14.0f }, 9.2f, true, ivory,
             Just::centredRight, 0.03f);
        text(g, "= " + signedString(mt * 100.0f, 0, " ct"), { 196.0f, 50.0f, 138.0f, 14.0f }, 9.2f, true, ivoryDim,
             Just::centredRight, 0.03f);
    }
};

// ---- Bend ranges ------------------------------------------------------------------------------
class BendRanges final : public LiveWidget
{
public:
    explicit BendRanges(Cs80AudioProcessor& p) : LiveWidget(p, 328.0f, 74.0f) {}

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
            text(g, label, { 14.0f, y, 200.0f, 12.0f }, 9.0f, true, ivoryDim, Just::centredLeft, 0.07f);
            text(g, juce::String(juce::CharPointer_UTF8("\xc2\xb1")) + juce::String(juce::roundToInt(range)) + " st",
                 { 214.0f, y - 1.0f, 100.0f, 14.0f }, 10.5f, true, active ? amberBright : ivoryDim, Just::centredRight);
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.fillRoundedRectangle(14.0f, y + 14.0f, 300.0f, 6.0f, 3.0f);
            g.setColour(active ? amber : ivoryDim.withAlpha(0.45f));
            g.fillRoundedRectangle(14.0f, y + 14.0f, juce::jmax(4.0f, 300.0f * range / 96.0f), 6.0f, 3.0f);
        };
        bar(8.0f, "PITCH LEVER / MIDI BEND", v(cs80::P_pitchBendRange), ! iso);
        bar(40.0f, v(cs80::P_mpeOn) >= 0.5f ? "MPE MEMBER CHANNELS" : "MPE MEMBER CHANNELS (MPE OFF)",
            v(cs80::P_mpeBendRange), ! iso && v(cs80::P_mpeOn) >= 0.5f);
        if (iso)
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }
};

// ---- Sub oscillator scope: the waveform the engine would actually run ---------------------
class SubScope final : public LiveWidget
{
public:
    explicit SubScope(Cs80AudioProcessor& p) : LiveWidget(p, 712.0f, 100.0f)
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
            vline(g, plot.getX() + plot.getWidth() * (float) i / 4.0f, plot.getY(), plot.getBottom(),
                  i == 0 || i == 4 ? 0.14f : 0.07f);
        hline(g, plot.getX(), plot.getRight(), plot.getCentreY(), 0.2f);
        text(g, "1 s WINDOW", { designW - 90.0f, designH - 15.0f, 80.0f, 12.0f }, 8.8f, true,
             ivoryDim.withAlpha(0.6f), Just::centredRight, 0.06f);
    }

private:
    static Rect plotRect() { return { 14.0f, 24.0f, 684.0f, 64.0f }; }

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
        const bool holdFn = ! original && fn == 4;

        juce::Path path;
        if (ext)
        {
            g.setColour(ivoryDim.withAlpha(0.7f));
            const float dash[] = { 5.0f, 4.0f };
            g.drawDashedLine(juce::Line<float>(plot.getX(), cy, plot.getRight(), cy), dash, 2, 1.4f);
        }
        else if (noiseFn)
        {
            for (int i = 0; i < 684; i += 2)
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

        const juce::String mode = iso ? "ORIGINAL  (FORCED BY PARITY ISOLATION)"
                                      : (original ? "ORIGINAL" : "LEGACY");
        text(g, mode + "  -  " + name, { 14.0f, 5.0f, 460.0f, 15.0f }, 9.5f, true, amber, Just::centredLeft, 0.1f);
        juce::String right;
        if (ext)
            right = "EXT LEVEL " + juce::String(juce::roundToInt(v(cs80::P_subExtLevel) * 100.0f)) + "%  -  BUS "
                    + (proc.isExternalInputEnabled() ? "ENABLED" : "DISABLED (HOST ROUTING REQUIRED)");
        else if (noiseFn)
            right = "SPEED IGNORED";
        else
            right = juce::String(rate, rate < 10.0f ? 2 : 1) + " Hz";
        text(g, right, { 330.0f, 5.0f, 368.0f, 15.0f }, 9.5f, true, ext ? ivory : amber, Just::centredRight, 0.06f);
    }

    std::array<float, 400> noise {};
};

// ---- Ring modulator envelope and carrier --------------------------------------------------
class RingCurve final : public LiveWidget
{
public:
    explicit RingCurve(Cs80AudioProcessor& p) : LiveWidget(p, 332.0f, 372.0f) {}

    void paintStatic(juce::Graphics& g) override
    {
        glassWell(g, { 0.0f, 0.0f, designW, designH });
        const auto plot = plotRect();
        for (int i = 0; i <= 4; ++i)
            hline(g, plot.getX(), plot.getRight(), plot.getY() + plot.getHeight() * (float) i / 4.0f,
                  i == 4 ? 0.22f : 0.07f);
        text(g, "ENVELOPE (KEY HELD)  /  CARRIER FREQUENCY", { 14.0f, 6.0f, 250.0f, 14.0f }, 9.0f, true,
             ivoryDim.withAlpha(0.75f), Just::centredLeft, 0.06f);
    }

private:
    Rect plotRect() const { return { 18.0f, 34.0f, 296.0f, designH - 104.0f }; }

    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_ringAttack); s[1] = v(cs80::P_ringDecay); s[2] = v(cs80::P_ringSpeed);
        s[3] = v(cs80::P_ringDepth); s[4] = v(cs80::P_ringAmount);
    }

    void draw(juce::Graphics& g) override
    {
        const float a = juce::jmax(0.001f, v(cs80::P_ringAttack)), d = juce::jmax(0.001f, v(cs80::P_ringDecay));
        const float speed = v(cs80::P_ringSpeed), depth = v(cs80::P_ringDepth), amount = v(cs80::P_ringAmount);
        const auto plot = plotRect();
        // Engine Adsr: attack 1 - exp(-6 t / A) until ~0.999, then exp(-6 t / D) down to the zero sustain.
        const float ta = 1.151f * a, td = 1.267f * d, total = ta + td;
        auto env = [&](float t)
        {
            if (t <= ta) return 1.0f - std::exp(-6.0f * t / a);
            return std::exp(-6.0f * (t - ta) / d);
        };
        juce::Path envPath, carrier;
        constexpr int count = 360;
        for (int i = 0; i <= count; ++i)
        {
            const float t = total * (float) i / (float) count;
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) count;
            const float e = env(t);
            const float yEnv = plot.getBottom() - e * plot.getHeight();
            const float yFreq = plot.getBottom() - depth * e * plot.getHeight();
            if (i == 0)
            {
                envPath.startNewSubPath(x, yEnv);
                carrier.startNewSubPath(x, yFreq);
            }
            else
            {
                envPath.lineTo(x, yEnv);
                carrier.lineTo(x, yFreq);
            }
        }
        auto fill = envPath;
        fill.lineTo(plot.getRight(), plot.getBottom());
        fill.lineTo(plot.getX(), plot.getBottom());
        fill.closeSubPath();
        const bool live = amount > 0.001f;
        g.setColour(amber.withAlpha(live ? 0.17f : 0.08f));
        g.fillPath(fill);
        g.setColour(live ? amberBright : ivoryDim);
        g.strokePath(envPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(ivory.withAlpha(live ? 0.9f : 0.4f));
        const float dash[] = { 4.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType(1.5f).createDashedStroke(dashed, carrier, dash, 2);
        g.fillPath(dashed);

        const float xa = plot.getX() + plot.getWidth() * ta / total;
        vline(g, xa, plot.getY(), plot.getBottom(), 0.18f, amber);
        text(g, "A " + (a < 1.0f ? juce::String(juce::roundToInt(a * 1000.0f)) + " ms" : juce::String(a, 2) + " s"),
             { plot.getX(), plot.getBottom() + 3.0f, 120.0f, 13.0f }, 8.8f, true, amber, Just::centredLeft, 0.04f);
        text(g, "D " + (d < 1.0f ? juce::String(juce::roundToInt(d * 1000.0f)) + " ms" : juce::String(d, 2) + " s"),
             { plot.getX() + 84.0f, plot.getBottom() + 3.0f, 100.0f, 13.0f }, 8.8f, true, amber, Just::centredLeft, 0.04f);
        text(g, "SPAN " + periodString(total), { plot.getRight() - 110.0f, plot.getBottom() + 3.0f, 110.0f, 13.0f },
             8.8f, true, ivoryDim, Just::centredRight, 0.04f);
        text(g, "CARRIER  " + juce::String(speed, speed < 10.0f ? 2 : 1) + " Hz  >  "
                    + juce::String(speed * (1.0f + depth), speed < 10.0f ? 2 : 1) + " Hz peak",
             { 14.0f, designH - 44.0f, 310.0f, 15.0f }, 10.2f, true, ivory, Just::centredLeft, 0.03f);
        text(g, live ? "WET " + juce::String(juce::roundToInt(amount * 100.0f)) + "%   RELEASE 0.35 s (FIXED)"
                     : "AMOUNT 0  -  BYPASSED",
             { 14.0f, designH - 26.0f, 310.0f, 15.0f }, 9.2f, true, live ? amber : ivoryDim, Just::centredLeft, 0.06f);
    }
};

// ---- Chorus / tremolo sweep -----------------------------------------------------------------
class ChorusSweep final : public LiveWidget
{
public:
    explicit ChorusSweep(Cs80AudioProcessor& p) : LiveWidget(p, 436.0f, 172.0f) {}

private:
    static Rect plotRect() { return { 40.0f, 30.0f, 380.0f, 104.0f }; }

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

        // Axis range per mode.
        float lo = 0.0f, hi = 20.0f;
        juce::String unit = " ms", title = "LEGACY  -  GENERIC DELAY LINE  -  DELAY";
        if (trem) { hi = 1.0f; unit = ""; title = "TREMOLO  -  GENERIC AMPLITUDE LAW (ALL POLICIES)  -  GAIN"; }
        else if (bbd) { hi = 4.0f; title = "OE2 BBD DELAY  -  256 CLOCK PERIODS (CLOCK RANGE INFERRED)  -  DELAY"; }

        for (int i = 0; i <= 2; ++i)
        {
            const float y = plot.getBottom() - plot.getHeight() * (float) i / 2.0f;
            hline(g, plot.getX(), plot.getRight(), y, i == 0 ? 0.20f : 0.07f);
            const float val = lo + (hi - lo) * (float) i / 2.0f;
            text(g, juce::String(val, hi <= 1.0f ? 1 : 0) + (i == 2 ? unit : juce::String()), { 0.0f, y - 6.0f, 37.0f, 12.0f },
                 9.0f, false, ivoryDim.withAlpha(0.8f), Just::centredRight);
        }
        for (int i = 0; i <= 4; ++i)
            vline(g, plot.getX() + plot.getWidth() * (float) i / 4.0f, plot.getY(), plot.getBottom(),
                  i % 2 == 0 ? 0.12f : 0.05f);

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
        g.setColour((on ? amberBright : ivoryDim));
        g.strokePath(main, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        text(g, title, { 14.0f, 6.0f, 410.0f, 14.0f }, 9.0f, true, amber, Just::centredLeft, 0.05f);
        text(g, "2 PERIODS  -  SPEED " + juce::String(speed, speed < 10.0f ? 2 : 1) + " Hz  -  T "
                    + periodString(1.0f / juce::jmax(0.05f, speed)),
             { 14.0f, 140.0f, 280.0f, 14.0f }, 9.2f, true, ivoryDim, Just::centredLeft, 0.05f);
        juce::String range;
        if (trem) range = "GAIN 1.00 - " + juce::String(1.0f - depth * 0.45f, 2);
        else if (bbd) range = juce::String(1000.0f * 256.0f / (106904.5f * std::exp2(depth * 0.5f)), 2) + " - "
                              + juce::String(1000.0f * 256.0f / (106904.5f * std::exp2(-depth * 0.5f)), 2) + " ms";
        else range = "13.5 - " + juce::String(13.5f + 4.5f * depth, 1) + " ms";
        text(g, range, { 270.0f, 140.0f, 150.0f, 14.0f }, 10.0f, true, amber, Just::centredRight, 0.03f);
        text(g, on ? (bbd ? "WET PATH: ONE SHARED CHAIN  (LEFT = RIGHT)" : "FAINT TRACE = RIGHT CHANNEL, PHASE +0.95 PI")
                   : "CHORUS SWITCH OFF  -  SIGNAL BYPASSES THIS STAGE",
             { 14.0f, 154.0f, 410.0f, 13.0f }, 8.8f, true, ivoryDim.withAlpha(0.8f), Just::centredLeft, 0.05f);
    }
};

// ---- Analog voice: numbers + eight cards --------------------------------------------------
class VoiceLcd final : public LiveWidget
{
public:
    explicit VoiceLcd(Cs80AudioProcessor& p) : LiveWidget(p, 272.0f, 108.0f) {}

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
            text(g, k, { 14.0f, y, 90.0f, 16.0f }, 8.8f, true, ivoryDim, Just::centredLeft, 0.1f);
            text(g, val, { 100.0f, y - 1.0f, 160.0f, 18.0f }, 12.5f, true, amberBright, Just::centredRight, 0.02f);
        };
        const juce::String pm = juce::String(juce::CharPointer_UTF8("\xc2\xb1"));
        row(8.0f, "TUNING", pm + juce::String(disp * cs80::kProvisionalVoiceTuneSpreadCents, 1) + " ct");
        row(32.0f, "PAN", pm + juce::String(disp * 0.8f, 2));
        row(56.0f, "DRIFT", juce::String(juce::roundToInt(drift * 100.0f)) + "%  /  32 s");
        row(80.0f, "NOISE FLOOR", noise <= 0.001f ? juce::String("OFF")
                                                   : juce::String(juce::roundToInt(20.0f * std::log10(noise * 0.0008f))) + " dB");
        if (isolated())
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }
};

class VoiceCards final : public LiveWidget
{
public:
    explicit VoiceCards(Cs80AudioProcessor& p) : LiveWidget(p, 648.0f, 92.0f)
    {
        // Same generator and draw order as Engine::prepare: tune, cutoff, level, env, pan per rank.
        cs80::Rng rng(0xC5801234u);
        for (int voice = 0; voice < 8; ++voice)
            for (int rank = 0; rank < 2; ++rank)
            {
                tune[(size_t) voice][(size_t) rank] = rng.nextBipolar();
                rng.nextBipolar();
                rng.nextBipolar();
                rng.nextBipolar();
                pan[(size_t) voice][(size_t) rank] = rng.nextBipolar();
            }
    }

private:
    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_voiceDispersion); s[1] = v(cs80::P_drift); s[2] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const float disp = isolated() ? 0.0f : v(cs80::P_voiceDispersion);
        const float drift = isolated() ? 0.0f : v(cs80::P_drift);
        for (int i = 0; i < 8; ++i)
        {
            const Rect card(8.0f + (float) i * 79.5f, 7.0f, 74.0f, 78.0f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff26231f), card.getX(), card.getY(),
                                                   juce::Colour(0xff15130f), card.getX(), card.getBottom(), false));
            g.fillRoundedRectangle(card, 3.0f);
            g.setColour(gold.withAlpha(0.35f));
            g.drawRoundedRectangle(card, 3.0f, 0.9f);
            text(g, "CARD " + juce::String(i + 1), { card.getX() + 6.0f, card.getY() + 3.0f, 62.0f, 11.0f }, 8.8f, true,
                 ivoryDim, Just::centredLeft, 0.08f);
            for (int rank = 0; rank < 2; ++rank)
            {
                const float o = tune[(size_t) i][(size_t) rank] * disp;
                const float y = card.getY() + 20.0f + (float) rank * 16.0f;
                const float cx = card.getX() + 32.0f;
                text(g, rank == 0 ? "I" : "II", { card.getX() + 5.0f, y - 1.0f, 12.0f, 11.0f }, 8.8f, true, ivoryDim,
                     Just::centredLeft);
                g.setColour(juce::Colours::black.withAlpha(0.7f));
                g.fillRect(cx - 18.0f, y + 2.0f, 36.0f, 5.0f);
                vline(g, cx, y, y + 9.0f, 0.4f);
                g.setColour(amber);
                g.fillRect(o >= 0.0f ? cx : cx + o * 18.0f, y + 2.5f, juce::jmax(1.0f, std::abs(o) * 18.0f), 4.0f);
                text(g, signedString(o * cs80::kProvisionalVoiceTuneSpreadCents, 1, ""),
                     { card.getX() + 49.0f, y - 1.0f, 23.0f, 11.0f }, 8.8f, false, ivory, Just::centredRight);
            }
            // Pan scatter of the two ranks on one L-R rule.
            const float py = card.getY() + 59.0f;
            hline(g, card.getX() + 8.0f, card.getRight() - 8.0f, py, 0.3f);
            text(g, "L", { card.getX() + 3.0f, py - 5.0f, 7.0f, 10.0f }, 8.2f, false, ivoryDim, Just::centred);
            text(g, "R", { card.getRight() - 9.0f, py - 5.0f, 7.0f, 10.0f }, 8.2f, false, ivoryDim, Just::centred);
            for (int rank = 0; rank < 2; ++rank)
            {
                const float p = pan[(size_t) i][(size_t) rank] * disp * 0.8f;
                const float px = card.getCentreX() + p * 27.0f;
                g.setColour(rank == 0 ? amberBright : ivory);
                g.fillEllipse(px - 2.4f, py - 2.4f, 4.8f, 4.8f);
            }
            if (drift > 0.001f)
            {
                g.setColour(amber.withAlpha(0.5f));
                g.fillRect(card.getX() + 8.0f, card.getBottom() - 8.0f, 58.0f * drift, 2.0f);
                g.setColour(ivoryDim.withAlpha(0.3f));
                g.fillRect(card.getX() + 8.0f + 58.0f * drift, card.getBottom() - 8.0f, 58.0f * (1.0f - drift), 2.0f);
            }
        }
        if (isolated())
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }

    std::array<std::array<float, 2>, 8> tune {}, pan {};
};

// ---- FX chain ------------------------------------------------------------------------------------
class FxChain final : public LiveWidget
{
public:
    explicit FxChain(Cs80AudioProcessor& p) : LiveWidget(p, 1472.0f, 62.0f) {}

private:
    void sample(Sig& s) const override
    {
        for (int i = 0; i < 3; ++i)
        {
            s[(size_t) i] = v(cs80::P_fx1Type + i * 2);
            s[(size_t) (3 + i)] = v(cs80::P_fx1Mix + i * 2);
        }
        s[6] = v(cs80::P_chorusOn); s[7] = v(cs80::P_chorusMode); s[8] = (float) policy();
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
            juce::String detail = type == 0 ? "OFF" : kFxTypes[type] + "  " + juce::String(juce::roundToInt(mix * 100.0f)) + "%";
            if (pol == 2)
                detail = "MASKED";
            nodes.push_back({ "FX " + juce::String(i + 1), detail, active && pol != 2, pol == 2 });
        }
        nodes.push_back({ "OUTPUT", pol == 0 ? "DC BLOCK + SOFT LIMIT" : "RAW SOURCE OUTPUT", true, false });

        const float count = (float) nodes.size();
        const float gap = 30.0f;
        const float nodeW = (designW - 16.0f - gap * (count - 1.0f)) / count;
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const float x = 8.0f + (float) i * (nodeW + gap);
            const Rect r(x, 8.0f, nodeW, designH - 16.0f);
            const auto& node = nodes[i];
            // Plastic plate; lit nodes are back-lit amber glass.
            if (node.lit)
            {
                g.setColour(amber.withAlpha(0.12f));
                g.fillRoundedRectangle(r.expanded(3.0f), 7.0f);
            }
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRoundedRectangle(r.translated(0.0f, 1.5f), 5.0f);
            g.setGradientFill(node.lit ? juce::ColourGradient(juce::Colour(0xff4a3a1c), r.getX(), r.getY(),
                                                              juce::Colour(0xff271d0e), r.getX(), r.getBottom(), false)
                                       : juce::ColourGradient(juce::Colour(0xff2b2926), r.getX(), r.getY(),
                                                              juce::Colour(0xff151311), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle(r, 5.0f);
            g.setColour(node.lit ? amber.withAlpha(0.75f) : juce::Colours::black.withAlpha(0.85f));
            g.drawRoundedRectangle(r, 5.0f, 1.0f);
            g.setColour(juce::Colours::white.withAlpha(node.lit ? 0.16f : 0.07f));
            g.drawLine(r.getX() + 5.0f, r.getY() + 1.2f, r.getRight() - 5.0f, r.getY() + 1.2f, 1.0f);
            jewel(g, { r.getX() + 15.0f, r.getCentreY() }, 4.0f, amber, node.lit);
            text(g, node.name, { r.getX() + 28.0f, r.getY() + 7.0f, r.getWidth() - 34.0f, 14.0f }, 9.0f, true,
                 node.lit ? amberBright : ivoryDim, Just::centredLeft, 0.12f);
            text(g, node.detail, { r.getX() + 28.0f, r.getY() + 23.0f, r.getWidth() - 34.0f, 16.0f }, 10.8f, true,
                 node.lit ? ivory : ivoryDim.withAlpha(0.65f), Just::centredLeft, 0.03f);
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
};

// ---- FX slot diagram -------------------------------------------------------------------------------
class FxDiagram final : public LiveWidget
{
public:
    FxDiagram(Cs80AudioProcessor& p, int slot) : LiveWidget(p, 304.0f, 192.0f), slotIndex(slot)
    {
        cs80::Rng rng(0x7a11u + (uint32_t) slot);
        for (auto& x : scatter)
            x = rng.nextUnipolar();
    }

private:
    static Rect plotRect() { return { 18.0f, 30.0f, 268.0f, 122.0f }; }

    void sample(Sig& s) const override
    {
        s[0] = v(cs80::P_fx1Type + slotIndex * 2); s[1] = v(cs80::P_fx1Mix + slotIndex * 2); s[2] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const int type = juce::jlimit(0, 5, juce::roundToInt(v(cs80::P_fx1Type + slotIndex * 2)));
        const float mix = v(cs80::P_fx1Mix + slotIndex * 2);
        const bool iso = isolated();
        static const float factor[] = { 0.0f, 0.62f, 0.72f, 0.58f, 0.62f, 1.0f };
        const float wet = juce::jlimit(0.0f, 1.0f, mix * factor[type]);
        const auto plot = plotRect();
        constexpr float two = juce::MathConstants<float>::twoPi;
        const bool active = type > 0 && mix > 0.001f && ! iso;
        const juce::Colour trace = active ? amberBright : ivoryDim.withAlpha(0.75f);
        juce::String title, foot;

        hline(g, plot.getX(), plot.getRight(), plot.getBottom(), 0.2f);
        switch (type)
        {
            case 0:
            {
                title = "EMPTY SLOT  -  PASSES THROUGH";
                g.setColour(ivoryDim.withAlpha(0.7f));
                const float y = plot.getCentreY();
                g.drawLine(plot.getX() + 10.0f, y, plot.getRight() - 10.0f, y, 1.6f);
                g.fillEllipse(plot.getX() + 6.0f, y - 3.5f, 7.0f, 7.0f);
                g.fillEllipse(plot.getRight() - 13.0f, y - 3.5f, 7.0f, 7.0f);
                foot = "BYPASS";
                break;
            }
            case 1: // modern stereo chorus: 0.62 Hz, depth 0.62, generic delay line
            {
                title = "CHORUS  -  DELAY SWEEP 0.62 Hz";
                for (int ch = 0; ch < 2; ++ch)
                {
                    juce::Path path;
                    for (int i = 0; i <= 160; ++i)
                    {
                        const float t = (float) i / 160.0f;
                        const float mod = std::sin(two * 2.0f * t + (ch == 0 ? 0.0f : juce::MathConstants<float>::pi * 0.95f));
                        const float ms = 13.5f + 4.5f * 0.62f * (0.5f + 0.5f * mod);
                        const float x = plot.getX() + plot.getWidth() * t;
                        const float y = plot.getBottom() - plot.getHeight() * ms / 20.0f;
                        if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
                    }
                    g.setColour(ch == 0 ? trace : ivory.withAlpha(active ? 0.5f : 0.25f));
                    g.strokePath(path, juce::PathStrokeType(ch == 0 ? 1.8f : 1.2f));
                }
                foot = "DELAY 13.5 - 16.3 ms  /  0 - 20 ms AXIS";
                break;
            }
            case 2: // six-stage all-pass phaser, bilinear response at 96 kHz
            {
                title = "PHASER  -  6 STAGES, 0.32 Hz";
                const float m = wet;
                constexpr float sr = 96000.0f;
                for (int pass = 0; pass < 3; ++pass)
                {
                    const float mod = pass == 0 ? -0.72f : (pass == 1 ? 0.0f : 0.72f);
                    const float cutoff = juce::jlimit(90.0f, sr * 0.42f, 680.0f * std::pow(2.0f, 2.9f * mod));
                    const float tn = std::tan(juce::MathConstants<float>::pi * cutoff / sr);
                    const float a = (tn - 1.0f) / (tn + 1.0f);
                    juce::Path path;
                    for (int i = 0; i <= 120; ++i)
                    {
                        const float t = (float) i / 120.0f;
                        const float f = 20.0f * std::pow(40000.0f / 20.0f, t);
                        const auto zinv = std::polar(1.0f, -two * f / sr);
                        const std::complex<float> h1 = (std::complex<float>(-a, 0.0f) + zinv) / (1.0f - a * zinv);
                        auto h = std::complex<float>(1.0f, 0.0f);
                        for (int s = 0; s < 6; ++s) h *= h1;
                        const float mag = std::abs((1.0f - m) + m * h);
                        const float db = juce::jlimit(-30.0f, 3.0f, 20.0f * std::log10(juce::jmax(mag, 1.0e-4f)));
                        const float x = plot.getX() + plot.getWidth() * t;
                        const float y = plot.getY() + plot.getHeight() * (3.0f - db) / 33.0f;
                        if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
                    }
                    g.setColour(pass == 1 ? trace : ivory.withAlpha(active ? 0.38f : 0.2f));
                    g.strokePath(path, juce::PathStrokeType(pass == 1 ? 1.8f : 1.1f));
                }
                text(g, "20 Hz", { plot.getX(), plot.getBottom() + 2.0f, 50.0f, 13.0f }, 8.8f, false, ivoryDim, Just::centredLeft);
                text(g, "40 kHz", { plot.getRight() - 50.0f, plot.getBottom() + 2.0f, 50.0f, 13.0f }, 8.8f, false, ivoryDim, Just::centredRight);
                {
                    const float x1k = plot.getX() + plot.getWidth() * std::log(1000.0f / 20.0f) / std::log(40000.0f / 20.0f);
                    vline(g, x1k, plot.getY(), plot.getBottom(), 0.1f);
                    text(g, "1 kHz", { x1k - 25.0f, plot.getBottom() + 2.0f, 50.0f, 13.0f }, 8.8f, false, ivoryDim, Just::centred);
                }
                foot = "LOW / MID / HIGH SWEEP AT A 48 kHz HOST, +3 TO -30 dB";
                break;
            }
            case 3: // tape delay: 340 ms taps, cross-feedback 0.36
            {
                title = "TAPE DELAY  -  340 ms TAPS";
                const float t0 = plot.getX() + 8.0f;
                const float step = (plot.getWidth() - 16.0f) / 5.0f;
                auto stem = [&](float x, float height, juce::Colour c)
                {
                    const float h = juce::jmax(2.0f, plot.getHeight() * height);
                    g.setColour(c);
                    g.fillRoundedRectangle(x - 3.0f, plot.getBottom() - h, 6.0f, h, 1.5f);
                };
                stem(t0, 1.0f - wet, ivory.withAlpha(active ? 0.9f : 0.4f));
                float amp = wet;
                for (int i = 1; i <= 5; ++i)
                {
                    stem(t0 + step * (float) i * 0.92f, amp, (i % 2 == 1 ? trace : trace.withAlpha(0.55f)));
                    amp *= 0.36f;
                }
                foot = "DRY, THEN ECHOES x0.36 EACH, SIDE TO SIDE";
                break;
            }
            case 4: // plate reverb: dry stem plus a dense decaying tail
            {
                title = "PLATE  -  DECAYING TAIL";
                g.setColour(ivory.withAlpha(active ? 0.9f : 0.4f));
                g.fillRoundedRectangle(plot.getX() + 5.0f, plot.getBottom() - plot.getHeight() * (1.0f - wet), 6.0f,
                                       plot.getHeight() * (1.0f - wet), 1.5f);
                for (size_t i = 0; i < scatter.size(); ++i)
                {
                    const float t = (float) i / (float) scatter.size();
                    const float x = plot.getX() + 22.0f + (plot.getWidth() - 28.0f) * t;
                    const float env = std::exp(-3.6f * t) * (0.45f + 0.55f * scatter[i]);
                    const float h = plot.getHeight() * wet * 0.95f * env;
                    g.setColour((i % 2 ? trace : trace.withAlpha(0.6f)));
                    g.fillRect(x, plot.getBottom() - juce::jmax(1.0f, h), 1.6f, juce::jmax(1.0f, h));
                }
                foot = "WET LEVEL SCALES THE TAIL";
                break;
            }
            default: // drive
            {
                title = "DRIVE  -  TRANSFER CURVE";
                const Rect box(plot.getCentreX() - 56.0f, plot.getCentreY() - 56.0f, 112.0f, 112.0f);
                g.setColour(kInk.withAlpha(0.6f));
                g.fillRect(box);
                hline(g, box.getX(), box.getRight(), box.getCentreY(), 0.2f);
                vline(g, box.getCentreX(), box.getY(), box.getBottom(), 0.2f);
                g.setColour(ivory.withAlpha(0.28f));
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
                g.strokePath(path, juce::PathStrokeType(1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                foot = "MIX 0 = DIAGONAL, MIX 1 = x3.2 SOFT CLIP, OUT x0.68";
                break;
            }
        }
        text(g, title, { 16.0f, 7.0f, 220.0f, 16.0f }, 9.2f, true, amber, Just::centredLeft, 0.04f);
        text(g, foot, { 16.0f, 171.0f, 276.0f, 14.0f }, 8.8f, false, ivoryDim.withAlpha(0.95f), Just::centredLeft, 0.02f);
        if (type > 0 && ! iso)
            text(g, "WET " + juce::String(juce::roundToInt(wet * 100.0f)) + "%", { 216.0f, 7.0f, 72.0f, 16.0f }, 9.6f,
                 true, ivory, Just::centredRight, 0.04f);
        if (iso)
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }

    int slotIndex;
    std::array<float, 90> scatter {};
};

// ---- Macro destination chips -----------------------------------------------------------------------
class MacroChips final : public LiveWidget
{
public:
    MacroChips(Cs80AudioProcessor& p, int index) : LiveWidget(p, 168.0f, 100.0f), m(index) {}
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
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const Rect r(2.0f, 2.0f + (float) i * 32.0f, designW - 4.0f, 26.0f);
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRoundedRectangle(r.translated(0.0f, 1.2f), 4.0f);
            g.setGradientFill(lit ? juce::ColourGradient(juce::Colour(0xff40331b), r.getX(), r.getY(),
                                                         juce::Colour(0xff231a0d), r.getX(), r.getBottom(), false)
                                  : juce::ColourGradient(juce::Colour(0xff252320), r.getX(), r.getY(),
                                                         juce::Colour(0xff12100e), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(lit ? amber.withAlpha(0.6f) : juce::Colours::black.withAlpha(0.85f));
            g.drawRoundedRectangle(r, 4.0f, 0.9f);
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.drawLine(r.getX() + 4.0f, r.getY() + 1.0f, r.getRight() - 4.0f, r.getY() + 1.0f, 1.0f);
            text(g, rows[i].name, { r.getX() + 8.0f, r.getY(), 90.0f, r.getHeight() }, 9.0f, true,
                 lit ? ivory : ivoryDim, Just::centredLeft, 0.05f);
            text(g, rows[i].value, { r.getX() + 70.0f, r.getY(), r.getWidth() - 78.0f, r.getHeight() }, 10.0f, true,
                 lit ? amberBright : ivoryDim.withAlpha(0.8f), Just::centredRight, 0.02f);
        }
    }

    int m;
};

// ---- Channel mix ----------------------------------------------------------------------------------------
class MixBar final : public LiveWidget
{
public:
    explicit MixBar(Cs80AudioProcessor& p) : LiveWidget(p, 400.0f, 74.0f) {}

private:
    void sample(Sig& s) const override { s[0] = v(cs80::P_mix); s[1] = v(cs80::P_outputMode); }

    void draw(juce::Graphics& g) override
    {
        const float mix = v(cs80::P_mix);
        const bool stereo = v(cs80::P_outputMode) >= 0.5f;
        auto bar = [&](float y, const juce::String& label, float gain)
        {
            text(g, label, { 14.0f, y, 80.0f, 16.0f }, 9.0f, true, ivory, Just::centredLeft, 0.1f);
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.fillRoundedRectangle(88.0f, y + 4.0f, 220.0f, 8.0f, 4.0f);
            g.setColour(amber);
            g.fillRoundedRectangle(88.0f, y + 4.0f, juce::jmax(4.0f, 220.0f * gain), 8.0f, 4.0f);
            text(g, juce::String(juce::roundToInt(gain * 100.0f)) + "%", { 316.0f, y, 70.0f, 16.0f }, 10.5f, true,
                 amberBright, Just::centredRight);
        };
        bar(9.0f, "CHANNEL I", 1.0f - mix);
        bar(30.0f, "CHANNEL II", mix);
        text(g, stereo ? "OUTPUT  STEREO  L / R" : "OUTPUT  MONO  (L + R) / 2", { 14.0f, 53.0f, 372.0f, 14.0f }, 9.0f, true,
             ivoryDim, Just::centredLeft, 0.1f);
    }
};

// ---- Arpeggiator ----------------------------------------------------------------------------------------
class ArpClock final : public LiveWidget
{
public:
    explicit ArpClock(Cs80AudioProcessor& p) : LiveWidget(p, 200.0f, 104.0f) {}

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
            text(g, k, { 14.0f, y, 80.0f, 16.0f }, 9.2f, true, ivoryDim, Just::centredLeft, 0.1f);
            text(g, val, { 70.0f, y - 1.0f, 120.0f, 18.0f }, 12.0f, true, on ? amberBright : ivory.withAlpha(0.7f),
                 Just::centredRight, 0.02f);
        };
        row(8.0f, "STEP", periodString(1.0f / rate));
        row(32.0f, "CLOCK", juce::String(rate, rate < 10.0f ? 2 : 1) + " /s");
        row(56.0f, "GATE", juce::String(random ? 45 : 58) + "% OF STEP");
        jewel(g, { 20.0f, 88.0f }, 4.2f, amber, on);
        text(g, on ? "ARPEGGIATOR ON" : (isolated() ? "OFF  -  ISOLATION" : "ARPEGGIATOR OFF"),
             { 34.0f, 80.0f, 160.0f, 16.0f }, 9.2f, true, on ? amberBright : ivoryDim, Just::centredLeft, 0.1f);
    }
};

class ArpPattern final : public LiveWidget
{
public:
    explicit ArpPattern(Cs80AudioProcessor& p) : LiveWidget(p, 676.0f, 224.0f) {}

private:
    static Rect plotRect() { return { 16.0f, 30.0f, 644.0f, 160.0f }; }

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
        for (int n = lowest; n <= highest; ++n)
        {
            const float y = plot.getBottom() - (float) (n - lowest + 1) * rowH;
            const bool chordTone = n % 12 == 0 || n % 12 == 4 || n % 12 == 7;
            if (chordTone)
            {
                g.setColour(ivory.withAlpha(0.05f));
                g.fillRect(plot.getX(), y, plot.getWidth(), rowH);
            }
        }
        for (size_t i = 0; i <= seq.size(); ++i)
            vline(g, plot.getX() + stepW * (float) i, plot.getY(), plot.getBottom(), i == 0 ? 0.16f : 0.06f);
        const bool random = mode == 3;
        const float gate = random ? 0.45f : 0.58f;
        for (size_t i = 0; i < seq.size(); ++i)
        {
            const int note = seq[i];
            const float y = plot.getBottom() - (float) (note - lowest + 1) * rowH;
            const Rect bar(plot.getX() + stepW * (float) i + stepW * 0.06f, y + 1.0f, stepW * gate, rowH - 2.0f);
            if (random)
            {
                g.setColour(on ? amber.withAlpha(0.28f) : ivoryDim.withAlpha(0.18f));
                g.fillRoundedRectangle(bar.withWidth(stepW * 0.86f), 1.5f);
                continue;
            }
            if (on)
            {
                g.setColour(amberBright.withAlpha(0.22f));
                g.fillRoundedRectangle(bar.expanded(1.8f), 2.5f);
            }
            g.setGradientFill(juce::ColourGradient(on ? amberBright : ivory.withAlpha(0.7f), bar.getX(), bar.getY(),
                                                   on ? amber.darker(0.2f) : ivoryDim.withAlpha(0.6f), bar.getX(),
                                                   bar.getBottom(), false));
            g.fillRoundedRectangle(bar, 1.8f);
            if (stepW > 22.0f)
                text(g, noteName(note), { bar.getX() - 4.0f, plot.getBottom() + 3.0f, bar.getWidth() + 8.0f, 12.0f }, 8.8f,
                     false, ivoryDim, Just::centred);
        }
        if (random)
            text(g, "RANDOM STEP FROM THE HELD NOTES, ALL OCTAVES", plot, 10.5f, true, amberBright, Just::centred, 0.1f);
        text(g, "DEMO CHORD C4 E4 G4" + juce::String(mode == 4 ? "  PRESSED E4 G4 C4" : "") + "  -  "
                    + juce::String((int) seq.size()) + " STEPS PER CYCLE",
             { 14.0f, 7.0f, 420.0f, 14.0f }, 9.0f, true, amber, Just::centredLeft, 0.07f);
        text(g, kArpModes[mode] + "  -  " + juce::String(octaves) + (octaves == 1 ? " OCTAVE" : " OCTAVES"),
             { 440.0f, 7.0f, 220.0f, 14.0f }, 9.0f, true, ivory, Just::centredRight, 0.07f);
        if (! on)
            text(g, isolated() ? "ARPEGGIATOR OFF IN ISOLATION" : "ARPEGGIATOR OFF", { 14.0f, 207.0f, 400.0f, 14.0f }, 8.8f, true,
                 ivoryDim, Just::centredLeft, 0.08f);
        if (isolated())
            inertOverlay(g, { 0.0f, 0.0f, designW, designH });
    }
};

// ---- Glide curve ----------------------------------------------------------------------------------------
class GlideCurve final : public LiveWidget
{
public:
    explicit GlideCurve(Cs80AudioProcessor& p) : LiveWidget(p, 688.0f, 224.0f) {}

    void paintStatic(juce::Graphics& g) override
    {
        glassWell(g, { 0.0f, 0.0f, designW, designH });
        const auto plot = plotRect();
        for (int i = 0; i <= 4; ++i)
        {
            const float y = plot.getBottom() - plot.getHeight() * (float) i / 4.0f;
            hline(g, plot.getX(), plot.getRight(), y, i == 0 ? 0.2f : 0.07f);
            text(g, juce::String(i * 3) + (i == 4 ? " st" : ""), { 0.0f, y - 6.0f, 43.0f, 12.0f }, 9.0f, false,
                 ivoryDim.withAlpha(0.8f), Just::centredRight);
        }
    }

private:
    static Rect plotRect() { return { 48.0f, 30.0f, 620.0f, 158.0f }; }

    void sample(Sig& s) const override { s[0] = v(cs80::P_portamento); s[1] = v(cs80::P_glideMode); }

    void draw(juce::Graphics& g) override
    {
        const float tau = v(cs80::P_portamento);
        const int mode = juce::jlimit(0, 2, juce::roundToInt(v(cs80::P_glideMode)));
        const auto plot = plotRect();
        const bool gliding = tau > 0.0001f;
        const float span = gliding ? tau * 5.0f : 1.0f;
        for (int i = 0; i <= 5; ++i)
            vline(g, plot.getX() + plot.getWidth() * (float) i / 5.0f, plot.getY(), plot.getBottom(), 0.06f);
        // A fresh key a full octave above the previous one: pitch offset against time.
        juce::Path path;
        constexpr int count = 400;
        for (int i = 0; i <= count; ++i)
        {
            const float t01 = (float) i / (float) count;
            float semis = gliding ? 12.0f * (1.0f - std::exp(-5.0f * t01)) : 12.0f;
            if (mode == 2 && gliding)
                semis = std::round(semis);
            const float x = plot.getX() + plot.getWidth() * t01;
            const float y = plot.getBottom() - plot.getHeight() * semis / 12.0f;
            if (i == 0)
                path.startNewSubPath(x, gliding ? y : plot.getBottom());
            if (! gliding && i == 0)
                path.lineTo(x, y);
            else if (i > 0)
                path.lineTo(x, y);
        }
        g.setColour(amberBright.withAlpha(0.28f));
        g.strokePath(path, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(amberBright);
        g.strokePath(path, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (gliding)
        {
            const float x63 = plot.getX() + plot.getWidth() / 5.0f;
            g.setColour(ivory.withAlpha(0.35f));
            const float dash[] = { 3.0f, 3.0f };
            g.drawDashedLine(juce::Line<float>(x63, plot.getBottom(), x63, plot.getBottom() - plot.getHeight() * 0.632f),
                             dash, 2, 1.0f);
            text(g, "63% at " + periodString(tau), { x63 - 60.0f, plot.getBottom() + 3.0f, 120.0f, 12.0f }, 9.0f, true,
                 amber, Just::centred, 0.04f);
            text(g, periodString(span), { plot.getRight() - 80.0f, plot.getBottom() + 3.0f, 80.0f, 12.0f }, 9.0f, true,
                 ivoryDim, Just::centredRight, 0.04f);
        }
        text(g, "ONE OCTAVE UP FROM THE PREVIOUS NOTE  -  EXPONENTIAL, TIME CONSTANT = GLIDE TIME", { 14.0f, 7.0f, 660.0f, 14.0f },
             9.0f, true, ivoryDim.withAlpha(0.8f), Just::centredLeft, 0.05f);
        text(g, gliding ? (mode == 2 ? "GLISSANDO  -  STEPS OF A SEMITONE" : "SMOOTH PITCH GLIDE") : "GLIDE TIME OFF  -  NO GLIDE",
             { 14.0f, 205.0f, 400.0f, 14.0f }, 9.2f, true, gliding ? amber : ivoryDim, Just::centredLeft, 0.08f);
        text(g, kGlideModes[mode], { 480.0f, 205.0f, 192.0f, 14.0f }, 9.2f, true, ivory, Just::centredRight, 0.1f);
    }
};

// ---- Render policy lamps ------------------------------------------------------------------------------
class PolicyLamps final : public LiveWidget
{
public:
    PolicyLamps(Cs80AudioProcessor& p, float w, float h, bool compactLayout)
        : LiveWidget(p, w, h), compact(compactLayout) {}
    void paintStatic(juce::Graphics&) override {}

private:
    void sample(Sig& s) const override
    {
        s[0] = (float) policy(); s[1] = v(cs80::P_sourcePerformance); s[2] = v(cs80::P_parityMode);
    }

    void draw(juce::Graphics& g) override
    {
        const int pol = policy();
        const bool requested = v(cs80::P_sourcePerformance) >= 0.5f;
        static const char* const names[] = { "LEGACY", "SOURCE PERFORMANCE", "PARITY ISOLATION" };
        const float cell = designW / 3.0f;
        for (int i = 0; i < 3; ++i)
        {
            const bool on = pol == i;
            const bool masked = i == 1 && pol == 2 && requested;
            const float x = (float) i * cell;
            const Pt c = compact ? Pt(x + 12.0f, designH * 0.5f) : Pt(x + cell * 0.5f, 12.0f);
            jewel(g, c, 4.6f, amber, on);
            if (masked)
            {
                g.setColour(amber.withAlpha(0.8f));
                g.drawEllipse(c.x - 6.5f, c.y - 6.5f, 13.0f, 13.0f, 1.0f);
            }
            const Rect label = compact ? Rect(x + 24.0f, 0.0f, cell - 26.0f, designH)
                                       : Rect(x, 24.0f, cell, 14.0f);
            text(g, juce::String(names[i]) + (masked ? " (MASKED)" : ""), label, 9.2f, true,
                 on ? amberBright : (masked ? amber.withAlpha(0.8f) : ivoryDim),
                 compact ? Just::centredLeft : Just::centred, 0.1f);
        }
    }

    bool compact;
};

} // namespace

// ---------------------------------------------------------------------------
// Page: records static art (modules, labels, wells, footnotes) in design space
// and hosts the live controls and widgets.
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
        for (auto* w : widgets)
            w->refresh();
    }

    // ---- static art (call module() before the things inside it) ----------------------------
    void module(Rect r, const juce::String& title, bool primary = true)
    {
        artOps.push_back([r, title, primary](juce::Graphics& g) { drawModule(g, r, title, primary); });
    }

    void label(const juce::String& s, Rect r, float size = 9.0f, juce::Colour c = ivoryDim,
               Just j = Just::centredLeft, float kern = 0.09f)
    {
        artOps.push_back([=](juce::Graphics& g) { engrave(g, s, r, size, true, c, j, kern); });
    }

    void note(const juce::String& s, Rect r, float size = 11.0f, juce::Colour c = ivoryDim)
    {
        artOps.push_back([=](juce::Graphics& g) { footnote(g, s, r, size, c); });
    }

    void plainText(const juce::String& s, Rect r, float size, juce::Colour c, bool bold = false)
    {
        artOps.push_back([=](juce::Graphics& g) { paragraph(g, s, r, size, c, bold); });
    }

    void bracket(float x0, float x1, float y, const juce::String& title, float size = 11.0f)
    {
        artOps.push_back([=](juce::Graphics& g) { bracketTitle(g, x0, x1, y, title, size); });
    }

    void custom(std::function<void(juce::Graphics&)> op) { artOps.push_back(std::move(op)); }

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

private:
    Cs80AudioProcessor& proc;
    std::unique_ptr<ArtLayer> art;
    std::vector<std::function<void(juce::Graphics&)>> artOps;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<Placed> placed;
    std::vector<LiveWidget*> widgets;
    int focusCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedPage)
};

// ---------------------------------------------------------------------------
// Chassis: wooden cheeks, charcoal body, header and footer rails. Cached once.
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
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / kW, (float) getHeight() / kH));
        g.fillAll(juce::Colour(0xff0c0b0a));

        const Rect body(30.0f, 0.0f, kW - 60.0f, kH);
        fillPanelFace(g, body);

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

        // Tab strip: a recessed black glass slot the illuminated tabs sit in.
        glassWell(g, { 452.0f, 14.0f, 836.0f, 68.0f }, 7.0f);

        // Corner screws holding the body to the cheeks.
        for (float x : { 46.0f, kW - 46.0f })
            for (float y : { 14.0f, kH - 14.0f })
                drawScrew(g, { x, y }, 4.0f, x < 800.0f ? 0.7f : 1.9f);

        drawCheek(g, { 0.0f, 0.0f, 30.0f, kH }, true);
        drawCheek(g, { kW - 30.0f, 0.0f, 30.0f, kH }, false);
    }

private:
    static void drawCheek(juce::Graphics& g, Rect r, bool left)
    {
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff7b3a1d), r.getX(), 0.0f,
                                               juce::Colour(0xff4b2010), r.getRight(), 0.0f, false));
        g.fillRect(r);
        juce::Random rng(left ? 17 : 29);
        for (int i = 0; i < 46; ++i)
        {
            const float x = r.getX() + rng.nextFloat() * r.getWidth();
            juce::Path grain;
            float y = 0.0f;
            grain.startNewSubPath(x, y);
            while (y < r.getBottom())
            {
                y += 60.0f + rng.nextFloat() * 90.0f;
                grain.lineTo(x + (rng.nextFloat() - 0.5f) * 3.0f, y);
            }
            g.setColour((rng.nextBool() ? juce::Colour(0xff2a0f06) : juce::Colour(0xffa65a2e))
                            .withAlpha(0.10f + rng.nextFloat() * 0.18f));
            g.strokePath(grain, juce::PathStrokeType(0.6f + rng.nextFloat() * 1.2f));
        }
        // Varnish sheen and the shadow where the body meets the wood.
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.0f), r.getX(), 0.0f,
                                               juce::Colours::white.withAlpha(0.16f), r.getX() + r.getWidth() * 0.35f, 0.0f, false));
        g.fillRect(r.withWidth(r.getWidth() * 0.35f));
        const float edge = left ? r.getRight() : r.getX();
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.55f), edge, 0.0f,
                                               juce::Colours::transparentBlack, edge + (left ? -9.0f : 9.0f), 0.0f, false));
        g.fillRect(left ? r.getRight() - 9.0f : r.getX(), 0.0f, 9.0f, kH);
        g.setColour(gold.withAlpha(0.5f));
        g.drawLine(edge + (left ? -0.5f : 0.5f), 0.0f, edge + (left ? -0.5f : 0.5f), kH, 1.0f);
    }
};

// ---------------------------------------------------------------------------
// Page tabs: back-lit plastic legends with a vector icon each.
// ---------------------------------------------------------------------------

class AdvPageTab final : public juce::Button
{
public:
    AdvPageTab(int index, const juce::String& name, const juce::String& caption)
        : juce::Button(name), page(index), sub(caption)
    {
        setClickingTogglesState(true);
        setRadioGroupId(0x80 + 7);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setExplicitFocusOrder(index + 1);
        setTitle(name);
        setDescription("Advanced page " + juce::String(index + 1) + " of 4. Shortcut Control+" + juce::String(index + 1)
                       + ". Left and Right arrows move between pages.");
        setTooltip(name + "  (Ctrl+" + juce::String(index + 1) + ")");
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
        constexpr float dw = 198.0f, dh = 48.0f;
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / dw, (float) getHeight() / dh));
        const bool on = getToggleState();
        const Rect face(1.5f, 1.0f, dw - 3.0f, dh - 5.0f);
        const float travel = on || down ? 1.5f : 0.0f;
        const auto r = face.translated(0.0f, travel);
        if (on)
        {
            g.setColour(amber.withAlpha(0.10f));
            g.fillRoundedRectangle(r.expanded(5.0f), 9.0f);
            g.setColour(amber.withAlpha(0.14f));
            g.fillRoundedRectangle(r.expanded(2.5f), 7.0f);
        }
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRoundedRectangle(r.translated(0.0f, 2.6f), 5.0f);
        g.setGradientFill(on ? juce::ColourGradient(kGlow, r.getX(), r.getY(), kGlowDeep, r.getX(), r.getBottom(), false)
                             : juce::ColourGradient(juce::Colour(hovered ? 0xff3a3733 : 0xff2e2c28), r.getX(), r.getY(),
                                                    juce::Colour(0xff151311), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(juce::Colours::white.withAlpha(on ? 0.55f : 0.12f));
        g.drawLine(r.getX() + 5.0f, r.getY() + 1.0f, r.getRight() - 5.0f, r.getY() + 1.0f, 1.1f);
        g.setColour(on ? juce::Colour(0xff6b4510).withAlpha(0.9f) : juce::Colours::black.withAlpha(0.9f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
        if (on)
        {
            // Light pooling at the bottom edge of the lit plastic.
            g.setGradientFill(juce::ColourGradient(juce::Colours::transparentWhite, r.getX(), r.getBottom() - 14.0f,
                                                   juce::Colour(0x40fff3c4), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle(r.reduced(1.0f), 5.0f);
        }

        const juce::Colour ink = on ? juce::Colour(0xff2a1c07) : ivoryDim.withAlpha(hovered ? 1.0f : 0.78f);
        drawIcon(g, page, { r.getX() + 11.0f, r.getY() + 7.0f, 30.0f, 28.0f }, ink, on);
        g.setFont(fnt(11.6f, true, 0.07f));
        g.setColour(on ? juce::Colours::white.withAlpha(0.3f) : juce::Colours::black.withAlpha(0.7f));
        g.drawText(getButtonText(), r.withTrimmedLeft(50.0f).translated(0.4f, on ? 0.8f : -0.4f).withHeight(24.0f).translated(0, 3.0f), Just::centredLeft, false);
        g.setColour(ink);
        g.drawText(getButtonText(), r.withTrimmedLeft(50.0f).withHeight(24.0f).translated(0.0f, 2.2f), Just::centredLeft, false);
        ui::text(g, sub, r.withTrimmedLeft(50.0f).withTrimmedTop(26.0f).withHeight(14.0f), 8.8f, true,
             on ? juce::Colour(0xff3c260a) : ivoryDim.withAlpha(0.82f), Just::centredLeft, 0.05f);
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
                    g.fillRoundedRectangle(b.getX() + 2.0f + (float) i * 7.2f, b.getBottom() - 2.0f - h, 5.0f, h, 1.2f);
                }
                break;
            }
        }
    }

    int page;
    juce::String sub;
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
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / 392.0f, (float) getHeight() / 78.0f));
        static const char* const titles[] = { "MODULATION", "SUB OSC / RING", "FX / MACROS", "ARP / PERFORMANCE" };
        static const char* const sub[] = {
            "Three sine LFOs, wheel routing, pitch range and tuning",
            "Sub oscillator, ring modulator, chorus and analog voice",
            "Three serial insert effects, four macros and the output stage",
            "Arpeggiator, glide and sustain, and the render policy" };
        text(g, "ADVANCED CONTROL  -  PAGE " + juce::String(page + 1) + " OF 4", { 0.0f, 0.0f, 392.0f, 13.0f }, 9.0f, true,
             gold, Just::centredLeft, 0.2f);
        engrave(g, titles[page], { 0.0f, 15.0f, 392.0f, 34.0f }, 25.0f, true, ivory, Just::centredLeft, 0.08f);
        text(g, sub[page], { 1.0f, 52.0f, 392.0f, 18.0f }, 11.2f, false, ivoryDim, Just::centredLeft, 0.02f);
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
        lamps = std::make_unique<PolicyLamps>(p, 640.0f, 22.0f, true);
        addAndMakeVisible(*lamps);
    }

    void refresh() { lamps->refresh(); }

    void paint(juce::Graphics& g) override
    {
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / 1472.0f, (float) getHeight() / 26.0f));
        engrave(g, "RENDER POLICY", { 0.0f, 0.0f, 130.0f, 26.0f }, 9.0f, true, gold, Just::centredLeft, 0.16f);
        text(g, "Ctrl+1 to 4 switch pages   -   Esc closes   -   double-click any knob to reset it",
             { 800.0f, 0.0f, 440.0f, 26.0f }, 9.0f, false, ivoryDim, Just::centredRight, 0.02f);
        text(g, "ALL PARAMETERS HOST-AUTOMATABLE", { 1180.0f, 0.0f, 292.0f, 26.0f }, 9.2f, true, amber.withAlpha(0.9f),
             Just::centredRight, 0.12f);
    }

    void resized() override
    {
        const float sx = (float) getWidth() / 1472.0f, sy = (float) getHeight() / 26.0f;
        lamps->setBounds(juce::roundToInt(140.0f * sx), juce::roundToInt(2.0f * sy), juce::roundToInt(640.0f * sx),
                         juce::roundToInt(22.0f * sy));
    }

private:
    std::unique_ptr<PolicyLamps> lamps;
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
    const char* const notes[3] = {
        "Free-running global sine LFO. Route it to pitch, filter, amplitude, PWM or pan.",
        "Second LFO, independent rate and destination. Use it for slow filter drift.",
        "Third LFO. A fast setting gives the CS-80 its characteristic shimmer." };
    for (int i = 0; i < 3; ++i)
    {
        const float x = xs[i], y = 108.0f;
        page.module({ x, y, 480.0f, 444.0f }, "LFO " + juce::String(i + 1));
        page.widget<LfoScope>({ x + 20.0f, y + 40.0f, 440.0f, 200.0f }, i);
        page.knob({ x + 28.0f, y + 256.0f, 112.0f, 112.0f }, rateParams[i], "RATE");
        page.knob({ x + 148.0f, y + 256.0f, 112.0f, 112.0f }, depthParams[i], "DEPTH");
        page.label("DESTINATION", { x + 288.0f, y + 258.0f, 176.0f, 14.0f });
        page.choice({ x + 288.0f, y + 276.0f, 176.0f, 30.0f }, destParams[i], kDestinations);
        page.widget<LfoReadout>({ x + 286.0f, y + 320.0f, 180.0f, 50.0f }, i);
        page.note(notes[i], { x + 28.0f, y + 386.0f, 424.0f, 44.0f });
    }

    // Routing and expression.
    page.module({ 64.0f, 568.0f, 724.0f, 328.0f }, "Modulation routing / expression");
    page.knob({ 88.0f, 612.0f, 116.0f, 120.0f }, cs80::P_modWheelSub, "WHEEL > SUB");
    page.knob({ 212.0f, 612.0f, 116.0f, 120.0f }, cs80::P_modWheelVcf, "WHEEL > VCF");
    page.knob({ 336.0f, 612.0f, 116.0f, 120.0f }, cs80::P_initBend, "INITIAL BEND");
    page.knob({ 460.0f, 612.0f, 116.0f, 120.0f }, cs80::P_portamento, "GLIDE");
    page.widget<WheelRouting>({ 596.0f, 614.0f, 176.0f, 132.0f });
    page.label("GLIDE MODE", { 88.0f, 756.0f, 190.0f, 14.0f });
    page.choice({ 88.0f, 774.0f, 190.0f, 30.0f }, cs80::P_glideMode, kGlideModes);
    page.toggle({ 304.0f, 774.0f, 130.0f, 30.0f }, cs80::P_mpeOn, "MPE");
    page.toggle({ 446.0f, 774.0f, 150.0f, 30.0f }, cs80::P_sustainMode, "SUSTAIN II");
    page.toggle({ 608.0f, 774.0f, 140.0f, 30.0f }, cs80::P_outputMode, "STEREO");
    page.note("The CS-80's pressure and velocity are separate performance dimensions. Initial touch sets the attack "
              "brightness; aftertouch bends the filter while held. MPE maps per-note bend/pressure when enabled; "
              "isolation disables MPE.",
              { 88.0f, 826.0f, 660.0f, 48.0f });

    page.module({ 804.0f, 568.0f, 732.0f, 328.0f }, "Pitch / tuning / expression");
    page.knob({ 828.0f, 612.0f, 120.0f, 120.0f }, cs80::P_pitchBendRange, "WHEEL BEND");
    page.knob({ 966.0f, 612.0f, 120.0f, 120.0f }, cs80::P_mpeBendRange, "MPE BEND");
    page.knob({ 1104.0f, 612.0f, 120.0f, 120.0f }, cs80::P_tuningRef, "A4 TUNING");
    page.knob({ 1242.0f, 612.0f, 120.0f, 120.0f }, cs80::P_keyRelease, "KEY RELEASE");
    page.knob({ 1380.0f, 612.0f, 120.0f, 120.0f }, cs80::P_masterTune, "MASTER TUNE");
    page.widget<TuningDisplay>({ 828.0f, 752.0f, 340.0f, 74.0f });
    page.widget<BendRanges>({ 1184.0f, 752.0f, 328.0f, 74.0f });
    page.note("PLAY RANGE  61-key panel / MIDI notes 0-127. A4 sets the reference frequency for twelve-tone equal "
              "temperament. MIDI Tuning Standard and tuning-table import are not implemented.",
              { 828.0f, 840.0f, 340.0f, 48.0f });
    page.note("The pitch lever is spring-loaded; MPE member channels use their own bend range. The ribbon is a "
              "separate continuous controller and returns to centre.",
              { 1184.0f, 840.0f, 328.0f, 48.0f });
}

void buildSubRing(AdvancedPage& page)
{
    // --- Sub oscillator -------------------------------------------------------------------------
    page.module({ 64.0f, 108.0f, 760.0f, 480.0f }, "Sub oscillator");
    auto* originalMode = page.toggle({ 88.0f, 148.0f, 214.0f, 28.0f }, cs80::P_subOriginalMode, "STORED ORIGINAL MODE");
    originalMode->setComponentID("sub_original_mode");
    originalMode->setTooltip("Stored preference: on selects original six functions and 0.7-60 Hz; off selects legacy. "
                             "PARITY ISOLATION forces original regardless of this retained setting; legacy controls are inert while isolated.");
    page.label("ORIGINAL FUNCTION", { 88.0f, 190.0f, 190.0f, 14.0f });
    page.choice({ 88.0f, 208.0f, 190.0f, 30.0f }, cs80::P_subOriginalFunction, kOriginalSubWaves);
    page.knob({ 292.0f, 184.0f, 96.0f, 100.0f }, cs80::P_subOriginalSpeed, "ORIGINAL Hz");
    auto* extLevel = page.knob({ 396.0f, 184.0f, 96.0f, 100.0f }, cs80::P_subExtLevel, "EXT LEVEL");
    extLevel->getSlider().setDescription("External modulation input LEVEL. The optional mono bus is disabled by default and requires host routing. "
                                         "At maximum LEVEL, a peak 1 passband sine gives nominal inverted modulation peak 1, with 63.5 host samples "
                                         "of causal reconstruction delay. Software normalization, not physical voltage calibration.");
    page.custom([](juce::Graphics& g) { etchLine(g, 514.0f, 190.0f, 514.0f, 282.0f, 0.55f); });
    page.label("LEGACY FUNCTION", { 534.0f, 190.0f, 160.0f, 14.0f });
    page.choice({ 534.0f, 208.0f, 160.0f, 30.0f }, cs80::P_subWave, kSubWaves);
    page.knob({ 704.0f, 184.0f, 92.0f, 100.0f }, cs80::P_subSpeed, "LEGACY Hz");
    page.widget<SubScope>({ 88.0f, 298.0f, 712.0f, 100.0f });
    page.bracket(88.0f, 420.0f, 418.0f, "DEPTH");
    page.bracket(460.0f, 792.0f, 418.0f, "PRESSURE");
    const char* const routeNames[6] = { "VCO", "VCF", "VCA", "AT SPEED", "AT VCO", "AT VCF" };
    const int routeParams[6] = { cs80::P_subVco, cs80::P_subVcf, cs80::P_subVca, cs80::P_subAtSpeed, cs80::P_subAtVco,
                                 cs80::P_subAtVcf };
    const float routeX[6] = { 88.0f, 202.0f, 316.0f, 460.0f, 574.0f, 688.0f };
    for (int i = 0; i < 6; ++i)
        page.knob({ routeX[i], 432.0f, 104.0f, 92.0f }, routeParams[i], routeNames[i]);
    page.note("PARITY ISOLATION forces ORIGINAL; stored mode and legacy settings are retained but inert. Noise / EXT ignore "
              "SPEED. EXT: optional mono input, disabled by default; host routing required. Six original functions; analog "
              "waveform, depth and touch laws remain provisional. Full depth moves VCO 1.5 st, VCF 1.5 oct, VCA 0.45 level.",
              { 88.0f, 534.0f, 700.0f, 50.0f }, 10.8f);

    // --- Ring modulator ---------------------------------------------------------------------------------
    page.module({ 840.0f, 108.0f, 696.0f, 480.0f }, "Ring modulator");
    page.fader({ 864.0f, 150.0f, 52.0f, 372.0f }, cs80::P_ringSpeed, "SPEED", capCream);
    page.fader({ 924.0f, 150.0f, 52.0f, 372.0f }, cs80::P_ringDepth, "DEPTH", capGrey);
    page.fader({ 984.0f, 150.0f, 52.0f, 372.0f }, cs80::P_ringAttack, "ATTACK", capBlack);
    page.fader({ 1044.0f, 150.0f, 52.0f, 372.0f }, cs80::P_ringDecay, "DECAY", capBlack);
    page.fader({ 1104.0f, 150.0f, 52.0f, 372.0f }, cs80::P_ringAmount, "AMOUNT", capGrey);
    page.widget<RingCurve>({ 1180.0f, 150.0f, 332.0f, 372.0f });
    page.note("A global ring modulator feeds the output alongside the voices. The envelope is retriggered by the first key of a "
              "phrase. DEPTH sweeps oscillator frequency; AMOUNT sets the dry/wet mix. The analog response remains provisional.",
              { 864.0f, 540.0f, 648.0f, 40.0f }, 11.4f);

    // --- Chorus / tremolo ---------------------------------------------------------------------------------
    page.module({ 64.0f, 604.0f, 760.0f, 292.0f }, "Chorus / tremolo");
    page.knob({ 88.0f, 648.0f, 116.0f, 108.0f }, cs80::P_chorusSpeed, "SPEED");
    page.knob({ 212.0f, 648.0f, 116.0f, 108.0f }, cs80::P_chorusDepth, "DEPTH");
    page.widget<ChorusSweep>({ 352.0f, 642.0f, 448.0f, 176.0f });
    page.toggle({ 88.0f, 776.0f, 116.0f, 30.0f }, cs80::P_chorusOn, "CHORUS");
    page.toggle({ 212.0f, 776.0f, 116.0f, 30.0f }, cs80::P_chorusMode, "TREMOLO");
    page.note("Legacy: provisional generic delay line. Source Performance and Isolation: OE2 BBD delay path (256 clock "
              "periods, printed filters); clock range, wet level and polarity are inferred, not validated against original hardware.",
              { 88.0f, 828.0f, 700.0f, 56.0f }, 11.4f);

    // --- Analog voice ----------------------------------------------------------------------------------------
    page.module({ 840.0f, 604.0f, 696.0f, 292.0f }, "Analog voice");
    page.knob({ 864.0f, 644.0f, 116.0f, 104.0f }, cs80::P_voiceDispersion, "DISPERSION");
    page.knob({ 988.0f, 644.0f, 116.0f, 104.0f }, cs80::P_drift, "DRIFT");
    page.knob({ 1112.0f, 644.0f, 116.0f, 104.0f }, cs80::P_analogNoise, "NOISE");
    page.widget<VoiceLcd>({ 1244.0f, 642.0f, 268.0f, 108.0f });
    page.widget<VoiceCards>({ 864.0f, 760.0f, 648.0f, 88.0f });
    page.note("Dispersion spreads the eight voice cards in tuning and pan; drift slowly moves each card; noise adds a "
              "provisional background floor. Set them to zero for a repeatable render; this is not hardware calibration.",
              { 864.0f, 852.0f, 648.0f, 38.0f }, 10.8f);
}

void buildFxMacros(AdvancedPage& page)
{
    page.label("SIGNAL ORDER", { 64.0f, 106.0f, 200.0f, 14.0f }, 9.0f, gold, Just::centredLeft, 0.16f);
    page.widget<FxChain>({ 64.0f, 122.0f, 1472.0f, 62.0f });

    const float xs[3] = { 64.0f, 560.0f, 1056.0f };
    const int typeParams[3] = { cs80::P_fx1Type, cs80::P_fx2Type, cs80::P_fx3Type };
    const int mixParams[3] = { cs80::P_fx1Mix, cs80::P_fx2Mix, cs80::P_fx3Mix };
    for (int i = 0; i < 3; ++i)
    {
        const float x = xs[i], y = 204.0f;
        page.module({ x, y, 480.0f, 300.0f }, "FX " + juce::String(i + 1));
        page.widget<FxDiagram>({ x + 20.0f, y + 40.0f, 304.0f, 192.0f }, i);
        page.label("TYPE", { x + 342.0f, y + 44.0f, 120.0f, 14.0f });
        page.choice({ x + 342.0f, y + 62.0f, 120.0f, 30.0f }, typeParams[i], kFxTypes);
        page.knob({ x + 342.0f, y + 112.0f, 120.0f, 112.0f }, mixParams[i], "MIX");
        page.note("Three insert slots, processed in series after the voice mixer and chorus. Tape Delay includes modulation "
                  "and a filtered feedback path. Plate Reverb is a dense, bright algorithmic plate.",
                  { x + 28.0f, y + 248.0f, 424.0f, 46.0f }, 10.6f);
    }

    page.module({ 64.0f, 520.0f, 724.0f, 376.0f }, "Performance macros");
    const char* const macroNames[4] = { "BRIGHTNESS", "MOTION", "WIDTH", "SPACE" };
    for (int i = 0; i < 4; ++i)
    {
        const float x = 80.0f + (float) i * 176.0f;
        page.knob({ x + 14.0f, 574.0f, 136.0f, 122.0f }, cs80::P_macro1 + i, macroNames[i]);
        page.widget<MacroChips>({ x, 718.0f, 168.0f, 100.0f }, i);
    }
    page.note("Macros are patch-level offsets. At centre they are neutral; turning them up pushes brilliance, envelope motion, "
              "detune/chorus width and the sub/ring depth respectively. They never replace the underlying panel settings.",
              { 88.0f, 838.0f, 660.0f, 42.0f }, 11.4f);

    page.module({ 804.0f, 520.0f, 732.0f, 376.0f }, "Output stage");
    page.knob({ 828.0f, 574.0f, 128.0f, 122.0f }, cs80::P_volume, "VOLUME");
    page.knob({ 968.0f, 574.0f, 128.0f, 122.0f }, cs80::P_mix, "MIX I/II");
    page.toggle({ 1136.0f, 584.0f, 150.0f, 30.0f }, cs80::P_outputMode, "STEREO");
    page.toggle({ 1300.0f, 584.0f, 150.0f, 30.0f }, cs80::P_mpeOn, "MPE");
    page.note("Stereo retains the left and right outputs. Mono averages them after effects and limiting; phase "
              "cancellation can change the result.",
              { 1136.0f, 636.0f, 372.0f, 44.0f }, 10.6f);
    page.widget<MixBar>({ 828.0f, 724.0f, 400.0f, 74.0f });
    page.note("Effects are bypassed at 0% mix; isolation masks them. The engine runs at 2x. Legacy output is DC-blocked and "
              "soft-limited. Source modes keep raw source output, which can exceed digital full scale: leave monitoring headroom.",
              { 828.0f, 826.0f, 680.0f, 46.0f }, 10.9f);
}

void buildArpPerformance(AdvancedPage& page)
{
    page.module({ 64.0f, 108.0f, 724.0f, 452.0f }, "Arpeggiator");
    page.toggle({ 88.0f, 148.0f, 150.0f, 30.0f }, cs80::P_arpOn, "ARP ON");
    page.label("MODE", { 88.0f, 200.0f, 160.0f, 14.0f });
    page.choice({ 88.0f, 218.0f, 220.0f, 30.0f }, cs80::P_arpMode, kArpModes);
    page.knob({ 338.0f, 144.0f, 116.0f, 112.0f }, cs80::P_arpRate, "RATE");
    page.knob({ 462.0f, 144.0f, 116.0f, 112.0f }, cs80::P_arpOctaves, "OCTAVES");
    page.widget<ArpClock>({ 592.0f, 148.0f, 176.0f, 92.0f });
    page.widget<ArpPattern>({ 88.0f, 268.0f, 676.0f, 224.0f });
    page.note("The arpeggiator holds the chord and retriggers the CS-80 voice envelopes at the selected clock. Up/Down, "
              "random and played-order modes cover the patterns used on the classic records.",
              { 88.0f, 506.0f, 664.0f, 40.0f }, 11.2f);

    page.module({ 804.0f, 108.0f, 732.0f, 452.0f }, "Performance");
    page.knob({ 828.0f, 144.0f, 120.0f, 112.0f }, cs80::P_portamento, "GLIDE");
    page.knob({ 960.0f, 144.0f, 120.0f, 112.0f }, cs80::P_keyRelease, "RELEASE");
    page.knob({ 1092.0f, 144.0f, 120.0f, 112.0f }, cs80::P_initBend, "INIT BEND");
    page.label("GLIDE MODE", { 1260.0f, 152.0f, 220.0f, 14.0f });
    page.choice({ 1260.0f, 170.0f, 230.0f, 30.0f }, cs80::P_glideMode, kGlideModes);
    page.toggle({ 1260.0f, 216.0f, 230.0f, 30.0f }, cs80::P_sustainMode, "SUSTAIN II");
    page.widget<GlideCurve>({ 828.0f, 268.0f, 688.0f, 224.0f });
    page.note("Portamento glides; Glissando steps in semitones. Both sustain modes release notes. Sustain II also cuts "
              "previous release tails at a new key press. MIDI CC64 hold is a separate extension, disabled by parity isolation.",
              { 828.0f, 506.0f, 680.0f, 40.0f }, 11.2f);

    // --- About, and the render policy ---------------------------------------------------------------------
    page.module({ 64.0f, 576.0f, 1472.0f, 320.0f }, "About this instrument");
    page.custom([](juce::Graphics& g)
    {
        engrave(g, "CS-80 REFERENCE", { 96.0f, 618.0f, 500.0f, 28.0f }, 21.0f, true, ivory, Just::centredLeft, 0.1f);
    });
    page.plainText("A circuit-informed recreation of the Yamaha CS-80 (1977): eight voices, each with two independent ranks, for "
                   "sixteen oscillator/filter/VCA chains. The signal path follows the original's high-pass into low-pass "
                   "topology, linear VCA and global sub-oscillator.\n\n"
                   "The primary panel exposes the controls a player touches; this page exposes the modern layer (LFOs, MPE, "
                   "effects, dispersion) without cluttering the instrument. All controls are host-automatable and saved in "
                   "plugin state, except transient performance gestures. The library has 22 legacy tone studies and 200 "
                   "performance patches; loading one returns both ranks to PANEL and resets SOURCE PERFORMANCE off. Current "
                   "source models are available with SOURCE PERFORMANCE or comparison isolation at right.",
                   { 96.0f, 656.0f, 600.0f, 176.0f }, 12.6f, ivoryDim);
    page.note("Touch the ribbon above the keyboard to bend pitch. Drag the pitch lever (PITCH) or modulation wheel (MOD) at the "
              "left of the keyboard. Double-click any knob to reset it.",
              { 96.0f, 840.0f, 600.0f, 40.0f }, 11.2f);

    page.custom([](juce::Graphics& g)
    {
        const Rect plate(730.0f, 620.0f, 350.0f, 110.0f);
        glassWell(g, plate, 5.0f);
        etchRect(g, plate.reduced(6.0f), 3.0f, 0.5f);
        const char* const lines[3] = { "ENGINE   2x oversampled  -  8 voices  -  16 ranks", "FORMAT   VST3 + AU (macOS) + standalone",
                                       "BUILD    Reference Instruments  -  1977-CS80" };
        for (int i = 0; i < 3; ++i)
            text(g, lines[i], { plate.getX() + 18.0f, plate.getY() + 18.0f + (float) i * 25.0f, plate.getWidth() - 30.0f, 20.0f },
                 10.2f, true, amber, Just::centredLeft, 0.03f);
        text(g, "2x host rate  -  MPE + poly pressure", { plate.getX() + 18.0f, plate.getBottom() + 8.0f, 330.0f, 14.0f }, 9.0f, true,
             ivoryDim, Just::centredLeft, 0.04f);
        etchLine(g, 1096.0f, 624.0f, 1096.0f, 872.0f, 0.5f);
    });
    page.label("EFFECTIVE POLICY", { 1124.0f, 614.0f, 300.0f, 14.0f }, 9.0f, gold, Just::centredLeft, 0.16f);
    page.widget<PolicyLamps>({ 1124.0f, 632.0f, 388.0f, 44.0f }, 388.0f, 44.0f, false);
    auto* sourcePerformance = page.toggle({ 1124.0f, 690.0f, 388.0f, 30.0f }, cs80::P_sourcePerformance, "SOURCE PERFORMANCE");
    sourcePerformance->setComponentID("source_performance");
    sourcePerformance->setTitle("Source performance (uncalibrated)");
    sourcePerformance->setTooltip("Opt-in source-performance policy: current source models with ordinary MIDI pitch bend, CC1/11/64/74, optional MPE and "
                                  "modern software overlays (macros, FX, pan and bypass). Not calibrated; no hardware match established. Keep output "
                                  "headroom. An effective mode change stops notes and clears controller/effect history. PARITY ISOLATION wins if both "
                                  "switches are ON; changing this source request while isolation stays ON is masked and does not stop notes. The source "
                                  "request is saved with sound presets; library programs reset it OFF and return both selectors to PANEL. Selected tones "
                                  "and memories still own only the 26 programmer controls; footage and global controls remain live.");
    sourcePerformance->setDescription(sourcePerformance->getTooltip());
    auto* parity = page.toggle({ 1124.0f, 728.0f, 388.0f, 30.0f }, cs80::P_parityMode, "PARITY ISOLATION");
    parity->setComponentID("parity_mode");
    parity->setTooltip("Comparison fixture, not calibrated. No hardware match established. Enable before playing: toggling stops notes and clears "
                       "controller/effect history. MIDI pitch bend, CC1/11/64/74 and MPE are ignored; pressure, ribbon and the on-screen pedal "
                       "remain active. Hardware factory tabs use nominal source models; library programs still return to PANEL. Modern settings are "
                       "retained but inert. Isolation takes priority over SOURCE PERFORMANCE; its stored source request is masked, not cleared. "
                       "Output may exceed digital full scale.");
    parity->setDescription(parity->getTooltip());
    page.plainText("NOT CALIBRATED - no hardware match established.\n"
                   "SOURCE: current models + MIDI / MPE / modern overlays.\n"
                   "ISOLATION wins: bend, CC1/11/64/74 + MPE off; FX masked.\n"
                   "Pressure, ribbon + on-screen PEDAL stay active.\n"
                   "Effective mode changes stop notes; keep headroom.\n"
                   "Source request is retained but masked in isolation.",
                   { 1124.0f, 774.0f, 388.0f, 108.0f }, 10.4f, amber);
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
    const char* const captions[] = { "LFO - WHEELS - TUNING", "CHORUS - ANALOG VOICE",
                                     "FX - MACROS - OUTPUT", "ARP - GLIDE - POLICY" };
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
    buildFxMacros(*pages[2]);
    buildArpPerformance(*pages[3]);

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

    showPage(0);
}

AdvancedPanel::~AdvancedPanel()
{
    stopTimer();
}

void AdvancedPanel::showPage(int index)
{
    activePage = juce::jlimit(0, 3, index);
    for (int i = 0; i < 4; ++i)
    {
        pages[(size_t) i]->setVisible(i == activePage);
        tabs[(size_t) i]->setToggleState(i == activePage, juce::dontSendNotification);
    }
    titleBlock->setPage(activePage);
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
        stopTimer();
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
        tabs[(size_t) i]->setBounds(r(462.0f + (float) i * 206.0f, 22.0f, 198.0f, 48.0f));
        pages[(size_t) i]->setBounds(getLocalBounds());
    }
    titleBlock->setBounds(r(58.0f, 10.0f, 392.0f, 78.0f));
    footer->setBounds(r(64.0f, 908.0f, 1472.0f, 26.0f));
    closeButton->setBounds(r(1420.0f, 26.0f, 116.0f, 38.0f));
}

} // namespace cs80::ui
