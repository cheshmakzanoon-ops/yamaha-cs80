#include "StudioDrawers.h"

#include "ArtCommon.h"
#include "PluginProcessor.h"

#include <cmath>

namespace cs80::ui
{

namespace
{
using Rf = juce::Rectangle<float>;

juce::Colour ink() { return juce::Colour(0xfff0e6c4); }
juce::Colour goldInk() { return juce::Colour(0xffcdb878); }
juce::Colour curveInk() { return juce::Colour(0xfff2dc98); }
juce::Colour field() { return juce::Colour(0xff131210); }

juce::Font font(float height, bool bold = true, float kern = 0.06f)
{
    return art::displayFont(juce::jmax(9.0f, height), bold ? juce::Font::bold : juce::Font::plain).withExtraKerningFactor(kern);
}

void say(juce::Graphics& g, const juce::String& s, Rf r, float h, juce::Colour c, juce::Justification j = juce::Justification::centredLeft,
         bool bold = true)
{
    g.setFont(font(h, bold));
    g.setColour(c);
    g.drawFittedText(s, r.toNearestInt(), j, 1, 0.85f);
}

juce::String timeText(float seconds)
{
    const int ms = juce::roundToInt(seconds * 1000.0f);
    if (ms < 1000) return juce::String(ms) + " ms";
    return juce::String(seconds, seconds < 10.0f ? 2 : 1) + " s";
}

juce::String hertzText(float hz)
{
    return hz >= 1000.0f ? juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz" : juce::String(juce::roundToInt(hz)) + " Hz";
}

// Display widths of the four envelope stages: logarithmic in time, so 5 ms and 5 s both stay readable.
float stageWidth(float seconds)
{
    const float t = juce::jlimit(0.0f, 1.0f, std::log10(juce::jmax(0.001f, seconds) * 1000.0f) / 4.3f);
    return 0.20f + 0.80f * t;
}

float approach(float t, float k) { return (1.0f - std::exp(-k * t)) / (1.0f - std::exp(-k)); }

void frame(juce::Graphics& g, Rf r, const juce::String& title, const juce::String& readout)
{
    g.setColour(field());
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(goldInk().withAlpha(0.22f));
    g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 0.9f);
    say(g, title, { r.getX() + 8.0f, r.getY() + 4.0f, r.getWidth() * 0.5f, 14.0f }, 10.5f, goldInk());
    say(g, readout, { r.getX() + r.getWidth() * 0.35f, r.getY() + 4.0f, r.getWidth() * 0.65f - 8.0f, 14.0f }, 10.5f, ink().withAlpha(0.85f),
        juce::Justification::centredRight, false);
}

void grid(juce::Graphics& g, Rf plot, int cols, int rows)
{
    g.setColour(goldInk().withAlpha(0.10f));
    for (int i = 0; i <= rows; ++i)
        g.drawHorizontalLine(juce::roundToInt(plot.getY() + plot.getHeight() * (float) i / (float) rows), plot.getX(), plot.getRight());
    for (int i = 0; i <= cols; ++i)
        g.drawVerticalLine(juce::roundToInt(plot.getX() + plot.getWidth() * (float) i / (float) cols), plot.getY(), plot.getBottom());
}

void stroke(juce::Graphics& g, const juce::Path& line, juce::Colour c = curveInk())
{
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.strokePath(line, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                 juce::AffineTransform::translation(0.7f, 1.0f));
    g.setColour(c);
    g.strokePath(line, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// Plain magnitude of a resonant second-order section (an illustration of the panel settings, not the engine's
// circuit model, which is nonlinear in the circuit engine).
float sectionDb(float f, float fc, float res, bool highPass)
{
    const float q = 0.6f + 6.0f * res * res;
    const float w = f / juce::jmax(1.0f, fc);
    const float re = 1.0f - w * w, im = w / q;
    const float mag = (highPass ? w * w : 1.0f) / std::sqrt(re * re + im * im);
    return 20.0f * std::log10(juce::jmax(1.0e-6f, mag));
}

class DrawerKey final : public juce::Button
{
public:
    DrawerKey(const juce::String& text, const juce::String& title) : juce::Button(text)
    {
        setTitle(title);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
    }
    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced(1.0f);
        drawSatinPlate(g, r, getToggleState(), down, hovered, false);
        say(g, getButtonText(), r, juce::jlimit(9.0f, 13.0f, r.getHeight() * 0.42f), getToggleState() ? amberBright : ivory,
            juce::Justification::centred);
        if (hasKeyboardFocus(true))
            drawFocusRing(g, r);
    }
};
} // namespace

// ---------------------------------------------------------------------------
// Voice monitor
// ---------------------------------------------------------------------------
ScopeDrawer::ScopeDrawer(Cs80AudioProcessor& p) : processor(p)
{
    setTitle("Voice monitor");
    setDescription("Live voice monitor: signal path, envelopes and filter response of the chosen channel, and the active eight or sixteen voice cards.");
    setWantsKeyboardFocus(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    setOpaque(false);
    closeKey = std::make_unique<DrawerKey>("CLOSE", "Close the voice monitor");
    channelI = std::make_unique<DrawerKey>("I", "Monitor channel I");
    channelII = std::make_unique<DrawerKey>("II", "Monitor channel II");
    closeKey->setTooltip("Close the voice monitor (Esc)");
    channelI->setTooltip("Show channel I");
    channelII->setTooltip("Show channel II");
    closeKey->setComponentID("scope_close");
    int order = 0;
    for (auto* k : { channelI.get(), channelII.get(), closeKey.get() })
    {
        k->setExplicitFocusOrder(++order);
        addAndMakeVisible(*k);
    }
    closeKey->onClick = [this] { if (onClose) onClose(); };
    channelI->onClick = [this] { setChannel(0); };
    channelII->onClick = [this] { setChannel(1); };
    setChannel(0);
}

ScopeDrawer::~ScopeDrawer()
{
    stopTimer();
}

void ScopeDrawer::setChannel(int newChannel)
{
    channel = juce::jlimit(0, 1, newChannel);
    channelI->setToggleState(channel == 0, juce::dontSendNotification);
    channelII->setToggleState(channel == 1, juce::dontSendNotification);
    snap = sample();
    repaint();
}

ScopeDrawer::Snapshot ScopeDrawer::sample() const
{
    Snapshot s;
    s.channel = channel;
    s.source = processor.getHardwareSource(channel);
    for (int local = 0; local < cs80::CP_count; ++local)
        s.v[(size_t) local] = processor.getParamValue(cs80::chParam(channel, local));
    // What is sounding: a tone tab or a memory owns the 26 programmer controls (as in the processor).
    if (s.source < 11)
    {
        cs80::ParamValues tone;
        cs80::applyHardwareTone(tone, channel, s.source);
        for (int local = 0; local < cs80::CP_count; ++local)
            if (Cs80AudioProcessor::isMemoryControl(local))
                s.v[(size_t) local] = tone[cs80::chParam(channel, local)];
    }
    else if (s.source < 13)
    {
        const int bank = channel + (s.source == 12 ? 2 : 0);
        for (int local = 0; local < cs80::CP_count; ++local)
            if (Cs80AudioProcessor::isMemoryControl(local))
                s.v[(size_t) local] = processor.getMemoryValue(bank, local);
    }
    const auto activity = processor.getVoiceActivitySnapshot();
    s.voices = activity.mask;
    s.capacity = activity.capacity;
    // Sample-coherent copy of model nodes published by processBlock. Direct
    // getEngine().getRankInfo() from this 15 Hz UI timer would be a data race.
    s.live = processor.getVoiceMonitorSnapshot();
    return s;
}

void ScopeDrawer::timerCallback()
{
    const auto now = sample();
    if (! (now == snap))
    {
        snap = now;
        repaint();
    }
}

void ScopeDrawer::visibilityChanged()
{
    if (isVisible())
    {
        snap = sample();
        startTimerHz(15);
    }
    else
        stopTimer();
}

bool ScopeDrawer::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose) onClose();
        return true;
    }
    return false;
}

void ScopeDrawer::paint(juce::Graphics& g)
{
    const float sx = (float) getWidth() / kDesignW;
    g.addTransform(juce::AffineTransform::scale(sx, (float) getHeight() / kDesignH));
    const Rf body(0.0f, 0.0f, kDesignW, kDesignH);
    // The drawer: a raised charcoal plate with a cast shadow, gold rule under the title.
    softShadow(g, body.reduced(4.0f), 6.0f, 0.0f, 6.0f, 10.0f, 0.75f);
    drawBevelRect(g, body.reduced(4.0f), 6.0f, juce::Colour(0xff26241f), 1.6f, true);
    etchRect(g, body.reduced(10.0f), 4.0f, 0.35f);
    say(g, "VOICE MONITOR", { 22.0f, 16.0f, 220.0f, 20.0f }, 15.0f, ivory);
    const auto sourceName = snap.source < 11 ? juce::String(cs80::hardwareToneName(snap.source, snap.channel)).toUpperCase() + " TONE"
                          : snap.source == 13 ? juce::String("PANEL") : "MEMORY " + juce::String(snap.channel + (snap.source == 12 ? 3 : 1));
    say(g, juce::String(snap.channel == 0 ? "CHANNEL I" : "CHANNEL II") + "  /  " + sourceName, { 22.0f, 38.0f, 260.0f, 16.0f }, 11.0f, goldInk());

    // Voice cards: original eight jewels, or two rows with both engines present.
    int count = 0;
    for (int i = 0; i < snap.capacity; ++i)
    {
        const bool on = (snap.voices >> i) & 1u;
        count += on ? 1 : 0;
        if (snap.capacity == 8)
            drawJewelLamp(g, { 30.0f + (float) i * 22.0f, 70.0f }, 6.0f, amber, on);
        else
            drawJewelLamp(g, { 30.0f + (float) (i % 8) * 22.0f, i < 8 ? 65.0f : 78.0f }, 4.6f, amber, on);
    }
    say(g, "VOICES  " + juce::String(count) + " / " + juce::String(snap.capacity), { 210.0f, 62.0f, 120.0f, 16.0f }, 11.0f, ink());

    // Signal path: VCO -> HPF -> LPF -> VCA, with the sine bypass under it.
    const char* names[] = { "VCO", "HPF", "LPF", "VCA" };
    const bool active[] = { true, p(cs80::CP_hpfOn) >= 0.5f, p(cs80::CP_lpfOn) >= 0.5f, true };
    const auto& tap = snap.live.ranks[(size_t) channel];
    const float effectiveHpfHz = tap.active && tap.hpfHz > 0.0f ? tap.hpfHz : p(cs80::CP_hpfCutoff);
    const float effectiveLpfHz = tap.active && tap.lpfHz > 0.0f ? tap.lpfHz : p(cs80::CP_lpfCutoff);
    const float y = 92.0f, h = 40.0f, w = 80.0f;
    for (int i = 0; i < 4; ++i)
    {
        const Rf box(22.0f + (float) i * 98.0f, y, w, h);
        if (i < 3)
        {
            g.setColour(goldInk().withAlpha(0.9f));
            g.drawLine(box.getRight() + 1.0f, y + h * 0.5f, box.getRight() + 14.0f, y + h * 0.5f, 1.0f);
            juce::Path head;
            head.addTriangle(box.getRight() + 18.0f, y + h * 0.5f, box.getRight() + 12.0f, y + h * 0.5f - 3.4f, box.getRight() + 12.0f, y + h * 0.5f + 3.4f);
            g.fillPath(head);
        }
        g.setColour(active[i] ? juce::Colour(0xff2f2c27) : field());
        g.fillRoundedRectangle(box, 3.0f);
        g.setColour(goldInk().withAlpha(active[i] ? 0.85f : 0.35f));
        g.drawRoundedRectangle(box.reduced(0.5f), 3.0f, 1.0f);
        juce::String detail;
        if (i == 0)
        {
            juce::StringArray waves;
            if (p(cs80::CP_saw) >= 0.5f) waves.add("SAW");
            if (p(cs80::CP_pulse) >= 0.5f) waves.add("PLS");
            if (p(cs80::CP_noise) > 0.02f) waves.add("NSE");
            detail = waves.isEmpty() ? juce::String("---") : waves.joinIntoString("+");
        }
        else if (i == 1) detail = active[i] ? hertzText(effectiveHpfHz) : juce::String("BYPASS");
        else if (i == 2) detail = active[i] ? hertzText(effectiveLpfHz) : juce::String("BYPASS");
        else detail = juce::String(juce::roundToInt(juce::jlimit(0.0f, 1.0f, p(cs80::CP_vLevel)) * 100.0f)) + " %";
        say(g, names[i], box.withTrimmedBottom(h * 0.48f).translated(0.0f, 2.0f), 13.0f, active[i] ? ink() : juce::Colour(0xff8a8168),
            juce::Justification::centred);
        say(g, detail, box.withTrimmedTop(h * 0.50f).withTrimmedBottom(2.0f), 10.5f, active[i] ? curveInk() : juce::Colour(0xff8a8168),
            juce::Justification::centred, false);
    }
    {
        const float sine = juce::jlimit(0.0f, 1.0f, p(cs80::CP_sine));
        const float yy = y + h + 14.0f;
        juce::Path route;
        route.startNewSubPath(62.0f, y + h);
        route.lineTo(62.0f, yy);
        route.lineTo(356.0f, yy);
        route.lineTo(356.0f, y + h + 3.0f);
        g.setColour(goldInk().withAlpha(0.35f + 0.65f * sine));
        g.strokePath(route, juce::PathStrokeType(1.0f));
        g.setColour(juce::Colour(0xff26241f));
        g.fillRect(160.0f, yy - 8.0f, 100.0f, 16.0f);
        say(g, "SINE  " + juce::String(juce::roundToInt(sine * 100.0f)) + " %", { 160.0f, yy - 8.0f, 100.0f, 16.0f }, 10.5f, ink(),
            juce::Justification::centred);
    }

    // VCF envelope: IL down, AL up, decay to zero, release back to IL (octaves of cutoff).
    auto envelope = [&](Rf r, bool vcf)
    {
        const float a = p(vcf ? cs80::CP_fA : cs80::CP_vA), d = p(vcf ? cs80::CP_fD : cs80::CP_vD), rel = p(vcf ? cs80::CP_fR : cs80::CP_vR);
        const float sus = vcf ? 0.0f : juce::jlimit(0.0f, 1.0f, p(cs80::CP_vS));
        // VCF is an offset in the reconstructed model, not a calibrated
        // hardware percentage; VCA is the normalized amplitude envelope.
        frame(g, r, vcf ? "VCF ENVELOPE" : "VCA ENVELOPE",
              ! tap.active ? juce::String("NO VOICE")
              : vcf ? "LIVE " + juce::String(tap.filterEnvelope, 2)
                    : "LIVE " + juce::String(juce::roundToInt(tap.ampEnvelope * 100.0f)) + "%");
        // The readout runs under the plot: a 184-unit frame has no room for it beside the title.
        say(g, "A " + timeText(a) + "  D " + timeText(d) + (vcf ? juce::String() : "  S " + juce::String(juce::roundToInt(sus * 100.0f)) + "%")
                   + "  R " + timeText(rel),
            { r.getX() + 6.0f, r.getBottom() - 17.0f, r.getWidth() - 12.0f, 14.0f }, 10.0f, ink().withAlpha(0.85f), juce::Justification::centred,
            false);
        const Rf plot(r.getX() + 8.0f, r.getY() + 22.0f, r.getWidth() - 16.0f, r.getHeight() - 42.0f);
        grid(g, plot, 8, 4);
        const float wa = stageWidth(a), wd = stageWidth(d), wh = 0.55f, wr = stageWidth(rel);
        const float u = plot.getWidth() / (wa + wd + wh + wr);
        const float x0 = plot.getX(), xA = x0 + wa * u, xD = xA + wd * u, xS = xD + wh * u, xR = plot.getRight();
        const float il = p(cs80::CP_fIl), al = p(cs80::CP_fAl);
        const float v0 = vcf ? -il : 0.0f, vA = vcf ? al : 1.0f;
        auto yOf = [&](float v)
        {
            return vcf ? plot.getCentreY() - v * plot.getHeight() * 0.46f : plot.getBottom() - 2.0f - v * (plot.getHeight() - 5.0f);
        };
        if (vcf)
        {
            g.setColour(goldInk().withAlpha(0.35f));
            const float dash[] = { 3.0f, 3.0f };
            g.drawDashedLine(juce::Line<float>(plot.getX(), plot.getCentreY(), plot.getRight(), plot.getCentreY()), dash, 2, 0.8f);
        }
        juce::Path line;
        line.startNewSubPath(x0, yOf(v0));
        auto seg = [&](float xa, float xb, float va, float vb, float k)
        {
            for (int i = 1; i <= 16; ++i)
            {
                const float t = (float) i / 16.0f;
                line.lineTo(xa + (xb - xa) * t, yOf(va + (vb - va) * approach(t, k)));
            }
        };
        seg(x0, xA, v0, vA, 2.2f);
        seg(xA, xD, vA, sus, 4.0f);
        line.lineTo(xS, yOf(sus));
        seg(xS, xR, sus, v0, 4.0f);
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(plot.expanded(1.0f).toNearestIntEdges());
        stroke(g, line);
    };
    envelope({ 22.0f, 176.0f, 184.0f, 120.0f }, true);
    envelope({ 214.0f, 176.0f, 184.0f, 120.0f }, false);

    // Filter response of the HPF into LPF pair, 20 Hz to 20 kHz.
    {
        const Rf r(22.0f, 304.0f, 376.0f, 140.0f);
        const bool hp = p(cs80::CP_hpfOn) >= 0.5f, lp = p(cs80::CP_lpfOn) >= 0.5f;
        frame(g, r, "FILTER RESPONSE", (hp ? "HPF " + hertzText(effectiveHpfHz) : juce::String("HPF BYPASS")) + "   "
                                           + (lp ? "LPF " + hertzText(effectiveLpfHz) : juce::String("LPF BYPASS")));
        const Rf plot(r.getX() + 8.0f, r.getY() + 22.0f, r.getWidth() - 16.0f, r.getHeight() - 44.0f);
        grid(g, plot, 3, 4);
        juce::Path line;
        for (int i = 0; i <= 120; ++i)
        {
            const float t = (float) i / 120.0f;
            const float f = 20.0f * std::pow(1000.0f, t);
            float db = 0.0f;
            if (hp) db += sectionDb(f, effectiveHpfHz, p(cs80::CP_hpfRes), true);
            if (lp) db += sectionDb(f, effectiveLpfHz, p(cs80::CP_lpfRes), false);
            const float yy = plot.getY() + juce::jlimit(0.0f, 1.0f, (12.0f - db) / 60.0f) * plot.getHeight();
            if (i == 0) line.startNewSubPath(plot.getX(), yy); else line.lineTo(plot.getX() + plot.getWidth() * t, yy);
        }
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(plot.expanded(1.0f).toNearestIntEdges());
            stroke(g, line);
        }
        const char* ticks[] = { "20", "200", "2k", "20k" };
        for (int i = 0; i < 4; ++i)
            say(g, ticks[i], { plot.getX() + plot.getWidth() * (float) i / 3.0f - 20.0f, plot.getBottom() + 3.0f, 40.0f, 14.0f }, 10.0f,
                ink().withAlpha(0.75f), juce::Justification::centred, false);
        say(g, "MODEL TAP CUTOFFS; RESPONSE ILLUSTRATED, NOT MEASURED", { r.getX() + 8.0f, r.getBottom() - 2.0f, r.getWidth() - 16.0f, 14.0f },
            9.5f, goldInk().withAlpha(0.8f), juce::Justification::centredRight, false);
    }
}

void ScopeDrawer::resized()
{
    const float sx = (float) getWidth() / kDesignW, sy = (float) getHeight() / kDesignH;
    auto r = [sx, sy](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(juce::roundToInt(x * sx), juce::roundToInt(y * sy), juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };
    channelI->setBounds(r(254.0f, 18.0f, 36.0f, 36.0f));
    channelII->setBounds(r(292.0f, 18.0f, 40.0f, 36.0f));
    closeKey->setBounds(r(338.0f, 18.0f, 62.0f, 36.0f));
}

// ---------------------------------------------------------------------------
// Controllers strip
// ---------------------------------------------------------------------------
ControllersStrip::ControllersStrip(Cs80AudioProcessor& p) : processor(p)
{
    setTitle("Controllers strip");
    setDescription("Pitch and modulation wheels. A modern addition: the CS-80 has a ribbon instead. Hidden in PURE CS-80.");
    pitch.setTitle("Pitch wheel");
    pitch.setDescription("Spring-loaded pitch wheel: MIDI-style pitch bend over the WHEEL BEND range. A modern addition.");
    mod.setTitle("Modulation wheel");
    mod.setDescription("Modulation wheel: adds sub oscillator vibrato and filter movement (Studio > Modulation > Wheels). A modern addition.");
    pitch.onValueChange = [this](float v) { processor.setPitchWheelFromUI(v); };
    mod.onValueChange = [this](float v) { processor.setModWheelFromUI(v); };
    addAndMakeVisible(pitch);
    addAndMakeVisible(mod);
}

ControllersStrip::~ControllersStrip() = default;

void ControllersStrip::paint(juce::Graphics& g)
{
    const float sx = (float) getWidth() / kDesignW;
    g.addTransform(juce::AffineTransform::scale(sx, (float) getHeight() / kDesignH));
    const Rf body(0.0f, 0.0f, kDesignW, kDesignH);
    softShadow(g, body.reduced(3.0f), 5.0f, 2.0f, 4.0f, 8.0f, 0.7f);
    drawBevelRect(g, body.reduced(3.0f), 5.0f, juce::Colour(0xff201e1a), 1.4f, true);
    say(g, "CONTROLLERS", { 12.0f, 8.0f, kDesignW - 24.0f, 16.0f }, 11.0f, goldInk(), juce::Justification::centred);
    say(g, "MODERN ADDITION", { 12.0f, kDesignH - 24.0f, kDesignW - 24.0f, 14.0f }, 9.5f, goldInk().withAlpha(0.8f), juce::Justification::centred,
        false);
}

void ControllersStrip::resized()
{
    const float sx = (float) getWidth() / kDesignW, sy = (float) getHeight() / kDesignH;
    auto r = [sx, sy](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(juce::roundToInt(x * sx), juce::roundToInt(y * sy), juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };
    pitch.setBounds(r(24.0f, 30.0f, 44.0f, 176.0f));
    mod.setBounds(r(82.0f, 30.0f, 44.0f, 176.0f));
}

} // namespace cs80::ui
