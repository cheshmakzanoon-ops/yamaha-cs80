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

class VoiceMonitorStatus final : public juce::Component
{
public:
    VoiceMonitorStatus()
    {
        setComponentID("voice_monitor_status");
        setInterceptsMouseClicks(false, false);
        setWantsKeyboardFocus(false);
        setDescription("Read-only visual summary, not an audio waveform or measured circuit response.");
    }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::staticText);
    }
};

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
    setDescription("Settings shows signal path, envelope and filter illustrations, not measured audio taps. LIVE inspects one physical voice slot and rank from the latest callback, with last-processed envelope and frequency values. Live activity includes release tails; slots 9 to 16 belong to the extension.");
    setWantsKeyboardFocus(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    setOpaque(false);
    accessibleStatus = std::make_unique<VoiceMonitorStatus>();
    addAndMakeVisible(*accessibleStatus);
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
    liveKey = std::make_unique<DrawerKey>("LIVE", "Switch between settings illustrations and live callback voice inspection");
    previousSlot = std::make_unique<DrawerKey>("<", "Inspect previous physical voice slot");
    nextSlot = std::make_unique<DrawerKey>(">", "Inspect next physical voice slot");
    liveKey->setComponentID("scope_live"); previousSlot->setComponentID("scope_previous_slot"); nextSlot->setComponentID("scope_next_slot");
    for (auto* key : {liveKey.get(), previousSlot.get(), nextSlot.get()}) { key->setExplicitFocusOrder(++order); addAndMakeVisible(*key); }
    liveKey->onClick = [this] { setLiveInspection(!liveInspection); };
    previousSlot->onClick = [this] { setInspectedSlot((inspectedSlot + 15) % 16); };
    nextSlot->onClick = [this] { setInspectedSlot((inspectedSlot + 1) % 16); };
    previousSlot->setVisible(false); nextSlot->setVisible(false);
    closeKey->onClick = [this] { if (onClose) onClose(); };
    channelI->onClick = [this] { setChannel(0); };
    channelII->onClick = [this] { setChannel(1); };
    setChannel(0);
}

ScopeDrawer::~ScopeDrawer()
{
    processor.requestVoiceInspection(inspectedSlot, channel, false);
    stopTimer();
}

void ScopeDrawer::setChannel(int newChannel)
{
    channel = juce::jlimit(0, 1, newChannel);
    channelI->setToggleState(channel == 0, juce::dontSendNotification);
    channelII->setToggleState(channel == 1, juce::dontSendNotification);
    processor.requestVoiceInspection(inspectedSlot, channel, liveInspection && isVisible());
    snap = sample();
    refreshAccessibleStatus();
    repaint();
}

void ScopeDrawer::setLiveInspection(bool enabled)
{
    liveInspection = enabled;
    liveKey->setToggleState(enabled, juce::dontSendNotification);
    previousSlot->setVisible(enabled); nextSlot->setVisible(enabled);
    processor.requestVoiceInspection(inspectedSlot, channel, enabled && isVisible());
    snap = sample(); refreshAccessibleStatus(); repaint();
}
void ScopeDrawer::setInspectedSlot(int slot)
{
    inspectedSlot = juce::jlimit(0, 15, slot);
    processor.requestVoiceInspection(inspectedSlot, channel, liveInspection && isVisible());
    snap = sample(); refreshAccessibleStatus(); repaint();
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
    s.voices = processor.getVoiceActivity();
    s.inspection = processor.getVoiceInspection();
    return s;
}

void ScopeDrawer::refreshAccessibleStatus()
{
    if (accessibleStatus == nullptr) return;
    juce::String text = "Voice monitor. ";
    if (!liveInspection)
        text << "Settings illustrations for channel " << (channel == 0 ? "I" : "II")
             << ". These are settings, not measured audio taps.";
    else
    {
        text << "Live inspection. Physical slot " << inspectedSlot + 1 << ", rank " << (channel == 0 ? "I" : "II") << ". ";
        const auto& r = snap.inspection;
        const bool matching = r.ready && cs80::VoiceInspection::slot(r.request) == inspectedSlot
            && cs80::VoiceInspection::rank(r.request) == channel && cs80::VoiceInspection::enabled(r.request);
        if (!matching) text << "Waiting for audio callback.";
        else if (!r.available) text << "Slot not available in 8-voice mode.";
        else if (!r.active) text << "Inactive slot. Values unavailable.";
        else
        {
            text << (r.held ? "Key held. " : r.pedalHeld ? "Pedal held. " : "Release tail. ")
                 << "MIDI note " << r.note << ", channel " << r.channel << ". ";
            if (!r.rendered) text << "Rank was not rendered. Values unavailable.";
            else text << "VCA envelope " << juce::String(r.amplitude, 4)
                      << ", VCF envelope " << juce::String(r.filter, 4)
                      << ", oscillator " << juce::String(r.oscillatorHz, 2)
                      << " Hz, HP cutoff " << juce::String(r.hpfHz, 2)
                      << " Hz, LP cutoff " << juce::String(r.lpfHz, 2) << " Hz.";
        }
    }
    // Read on demand by accessibility clients; do not announce audio-rate value
    // changes or add a keyboard stop to the existing control traversal.
    accessibleStatus->setTitle(text);
}

void ScopeDrawer::timerCallback()
{
    const auto now = sample();
    if (! (now == snap))
    {
        snap = now;
        refreshAccessibleStatus();
        repaint();
    }
}

void ScopeDrawer::visibilityChanged()
{
    processor.requestVoiceInspection(inspectedSlot, channel, liveInspection && isVisible());
    if (isVisible())
    {
        snap = sample();
        refreshAccessibleStatus();
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
    say(g, "VOICE MONITOR", { 22.0f, 16.0f, 150.0f, 20.0f }, 15.0f, ivory);
    const auto sourceName = snap.source < 11 ? juce::String(cs80::hardwareToneName(snap.source, snap.channel)).toUpperCase() + " TONE"
                          : snap.source == 13 ? juce::String("PANEL") : "MEMORY " + juce::String(snap.channel + (snap.source == 12 ? 3 : 1));
    say(g, juce::String(snap.channel == 0 ? "CHANNEL I" : "CHANNEL II") + "  /  " + sourceName, { 22.0f, 38.0f, 260.0f, 16.0f }, 11.0f, goldInk());

    if (liveInspection)
    {
        const auto& r = snap.inspection;
        say(g, "PHYSICAL SLOT " + juce::String(inspectedSlot + 1) + "  /  RANK " + (channel == 0 ? "I" : "II"),
            {70,78,280,24}, 14, ivory, juce::Justification::centred);
        const bool matching = r.ready && cs80::VoiceInspection::slot(r.request)==inspectedSlot
            && cs80::VoiceInspection::rank(r.request)==channel && cs80::VoiceInspection::enabled(r.request);
        juce::String state = !matching ? "WAITING FOR AUDIO CALLBACK" : !r.available ? "SLOT NOT AVAILABLE IN 8-VOICE MODE"
            : !r.active ? "INACTIVE SLOT" : r.held ? "KEY HELD" : r.pedalHeld ? "PEDAL HELD" : "RELEASE TAIL";
        say(g,state,{22,116,376,26},13,goldInk(),juce::Justification::centred);
        if (matching && r.available && r.active)
            say(g,"MIDI NOTE " + juce::String(r.note) + "   CHANNEL " + juce::String(r.channel),{22,151,376,24},13,ivory,juce::Justification::centred);
        const char* labels[] {"VCA ENVELOPE", "VCF ENVELOPE", "OSCILLATOR Hz", "HP CUTOFF Hz", "LP CUTOFF Hz"};
        const float values[] {r.amplitude,r.filter,r.oscillatorHz,r.hpfHz,r.lpfHz};
        for (int i=0;i<5;++i)
        {
            const float y = 198.0f + float(i)*34;
            say(g,labels[i],{30,y,210,24},12,goldInk());
            say(g,matching && r.rendered ? juce::String(values[i],i<2?4:2) : juce::String("--"),{240,y,145,24},14,ivory,juce::Justification::centredRight);
        }
        say(g,"LATEST CALLBACK / LAST PROCESSED RANK SAMPLE",{20,385,380,20},10.5f,goldInk(),juce::Justification::centred);
        say(g,"Not an audio waveform or measured circuit response",{20,409,380,20},10.5f,ivory,juce::Justification::centred);
        say(g,"Inactive and unrendered values are unavailable",{20,433,380,20},10.5f,ivory,juce::Justification::centred);
        return;
    }

    // The unchanged eight-card row gains a lower extension row only in 16 mode.
    for (int i = 0; i < snap.voices.capacity; ++i)
    {
        const bool extended = snap.voices.capacity == 16;
        const float y = extended ? (i < 8 ? 65.0f : 79.0f) : 70.0f;
        drawJewelLamp(g, { 30.0f + (float) (i % 8) * 22.0f, y }, extended ? 4.5f : 6.0f,
                      amber, snap.voices.active(i));
    }
    say(g, "VOICES  " + juce::String(snap.voices.count()) + " / " + juce::String(snap.voices.capacity),
        { 210.0f, 62.0f, snap.voices.capacity == 16 ? 150.0f : 120.0f, 16.0f }, 11.0f, ink());

    // Signal path: VCO -> HPF -> LPF -> VCA, with the sine bypass under it.
    const char* names[] = { "VCO", "HPF", "LPF", "VCA" };
    const bool active[] = { true, p(cs80::CP_hpfOn) >= 0.5f, p(cs80::CP_lpfOn) >= 0.5f, true };
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
        else if (i == 1) detail = active[i] ? hertzText(p(cs80::CP_hpfCutoff)) : juce::String("BYPASS");
        else if (i == 2) detail = active[i] ? hertzText(p(cs80::CP_lpfCutoff)) : juce::String("BYPASS");
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
        frame(g, r, vcf ? "VCF ENVELOPE" : "VCA ENVELOPE", {});
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
        frame(g, r, "FILTER RESPONSE", (hp ? "HPF " + hertzText(p(cs80::CP_hpfCutoff)) : juce::String("HPF BYPASS")) + "   "
                                           + (lp ? "LPF " + hertzText(p(cs80::CP_lpfCutoff)) : juce::String("LPF BYPASS")));
        const Rf plot(r.getX() + 8.0f, r.getY() + 22.0f, r.getWidth() - 16.0f, r.getHeight() - 44.0f);
        grid(g, plot, 3, 4);
        juce::Path line;
        for (int i = 0; i <= 120; ++i)
        {
            const float t = (float) i / 120.0f;
            const float f = 20.0f * std::pow(1000.0f, t);
            float db = 0.0f;
            if (hp) db += sectionDb(f, p(cs80::CP_hpfCutoff), p(cs80::CP_hpfRes), true);
            if (lp) db += sectionDb(f, p(cs80::CP_lpfCutoff), p(cs80::CP_lpfRes), false);
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
        say(g, "ILLUSTRATION OF THE PANEL SETTINGS, NOT A MEASUREMENT", { r.getX() + 8.0f, r.getBottom() - 2.0f, r.getWidth() - 16.0f, 14.0f },
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
    accessibleStatus->setBounds(r(20,60,380,390));
    liveKey->setBounds(r(176,16,70,20));
    previousSlot->setBounds(r(22,72,40,36)); nextSlot->setBounds(r(358,72,40,36));
}

// ---------------------------------------------------------------------------
// Controllers strip
// ---------------------------------------------------------------------------
ControllersStrip::ControllersStrip(Cs80AudioProcessor& p) : processor(p)
{
    setTitle("Controllers strip");
    setDescription("Pitch and modulation wheels. A modern addition: the CS-80 has a ribbon instead. Hidden in PURE CS-80.");
    pitch.setTitle("Pitch wheel");
    pitch.setDescription("Spring-loaded pitch wheel: MIDI-style pitch bend over the WHEEL BEND range. A modern addition. " + pitch.getDescription());
    mod.setTitle("Modulation wheel");
    mod.setDescription("Modulation wheel: adds sub oscillator vibrato and filter movement (Studio > Modulation > Wheels). A modern addition. " + mod.getDescription());
    pitch.onValueChange = [this](float v) { processor.setPitchWheelFromUI(v); };
    mod.onValueChange = [this](float v) { processor.setModWheelFromUI(v); };
    addAndMakeVisible(pitch);
    addAndMakeVisible(mod);
}

ControllersStrip::~ControllersStrip()
{
    // The callbacks still have a live owner/processor here. Do not invoke them
    // later from a child component destructor after its owner's fields die.
    pitch.cancelGesture();
    mod.cancelGesture();
}

void ControllersStrip::visibilityChanged()
{
    if (! isVisible())
    {
        pitch.cancelGesture();
        mod.cancelGesture();
    }
}

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
