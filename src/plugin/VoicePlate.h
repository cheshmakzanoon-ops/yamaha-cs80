#pragma once

// The live voice-flow plate: signal path, envelopes and filter response of one channel, computed from the
// panel (or the sounding tone or memory). It is a software display with no counterpart on the original
// panel; the editor no longer mounts it on the main view, and a Studio drawer can host it as a monitor.

#include "PanelWidgets.h"

#include <array>
#include <vector>

namespace cs80::panel
{
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

    static void panelText(juce::Graphics& g, const juce::String& text, float x, float y,
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
    static void paintBezel(juce::Graphics& g, bool optionsVisible)
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
            const bool two = ! art::isSmallEditor();
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
    static float rowHeight() { return juce::jmax(10.0f, art::designFloor() * 1.05f); }
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
        const float below = 2.0f * rowHeight() + 3.0f + (art::isSmallEditor() ? 0.0f : 10.0f);
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
        if (art::isSmallEditor()) return;
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
        const auto plot = plotArea(r, art::isSmallEditor() ? 0.0f : 14.0f);       // IL gutter: the tag sits left of the start dot
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
        marker(g, { s.x0, yOf(v0) }, art::isSmallEditor() ? "" : "IL", plot, true);
        marker(g, { s.xA, yOf(vA) }, art::isSmallEditor() ? "" : "AL", plot, false);
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
        cutoffMark(hpf, art::isSmallEditor() ? "" : "H", hpOn);
        cutoffMark(lpf, art::isSmallEditor() ? "" : "L", lpOn);
    }

    Snapshot snap;
    juce::String sourceLabel { "PANEL" };
    bool scopes = true;
};
} // namespace cs80::panel
