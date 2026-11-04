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

juce::String hertzText(float hz)
{
    return hz >= 1000.0f ? juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz" : juce::String(juce::roundToInt(hz)) + " Hz";
}

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
    setDescription("Live engine control taps: oscillator frequency, effective cutoffs, and both envelope outputs of the chosen physical voice and rank. The curves show recent engine values at a 15 Hz viewing rate.");
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
    voiceSelect.setTitle("Monitored physical voice");
    voiceSelect.setComponentID("scope_voice");
    voiceSelect.setTooltip("Choose a physical voice card; voices 9-16 belong to the second instrument in sixteen-voice mode.");
    for (int i = 0; i < 8; ++i) voiceSelect.addItem("VOICE " + juce::String(i + 1), i + 1);
    voiceSelect.setSelectedId(1, juce::dontSendNotification);
    voiceSelect.onChange = [this] { setVoice(voiceSelect.getSelectedId() - 1); };
    voiceSelect.setExplicitFocusOrder(++order);
    addAndMakeVisible(voiceSelect);
    setChannel(0);
}

ScopeDrawer::~ScopeDrawer()
{
    stopTimer();
    processor.setVoiceMonitorEnabled(false);
}

void ScopeDrawer::setChannel(int newChannel)
{
    channel = juce::jlimit(0, 1, newChannel);
    historyCount = historyNext = 0;
    channelI->setToggleState(channel == 0, juce::dontSendNotification);
    channelII->setToggleState(channel == 1, juce::dontSendNotification);
    snap = sample();
    updateControls();
    repaint();
}

void ScopeDrawer::setVoice(int newVoice)
{
    voice = juce::jlimit(0, processor.getVoiceCapacity() - 1, newVoice);
    voiceSelect.setSelectedId(voice + 1, juce::dontSendNotification);
    historyCount = historyNext = 0;
    snap = sample();
    updateControls();
    repaint();
}

cs80::Engine::RankInfo ScopeDrawer::displayedRank() const
{
    return { snap.hpfHz, snap.lpfHz, snap.filterEnvelope, snap.ampEnvelope,
             snap.oscillatorHz, snap.keyVolts, snap.hpfQ, snap.lpfQ, snap.hpfEnabled, snap.lpfEnabled };
}

ScopeDrawer::Snapshot ScopeDrawer::sample() const
{
    cs80::VoiceMonitorFrame frame;
    if (!processor.readVoiceMonitor(frame)) return snap;
    Snapshot s;
    s.channel = channel;
    s.capacity = frame.capacity;
    s.voice = juce::jlimit(0, s.capacity - 1, voice);
    s.source = frame.sources[(size_t) channel];
    s.serial = frame.serial;
    const auto& selected = frame.voices[(size_t) s.voice];
    const auto& rank = selected.ranks[(size_t) channel];
    s.note = selected.info.note;
    s.assignmentSerial = selected.info.assignmentSerial;
    s.oscillatorHz = rank.oscillatorHz;
    s.hpfHz = rank.hpfHz; s.lpfHz = rank.lpfHz;
    s.filterEnvelope = rank.filterEnvelope; s.ampEnvelope = rank.ampEnvelope;
    s.keyVolts = rank.keyVolts; s.hpfQ = rank.hpfQ; s.lpfQ = rank.lpfQ;
    s.hpfEnabled = rank.hpfEnabled; s.lpfEnabled = rank.lpfEnabled;
    for (int i = 0; i < s.capacity; ++i)
        if (frame.voices[(size_t) i].info.active) s.voices |= 1u << i;
    return s;
}

void ScopeDrawer::timerCallback()
{
    const auto now = sample();
    if (!(now == snap))
    {
        if (now.assignmentSerial != snap.assignmentSerial || now.voice != snap.voice || now.channel != snap.channel)
            historyCount = historyNext = 0;
        history[historyNext] = now;
        historyNext = (historyNext + 1) % history.size();
        historyCount = std::min(historyCount + 1, history.size());
        snap = now;
        voice = snap.voice;
        updateControls();
        repaint();
    }
}

void ScopeDrawer::updateControls()
{
    // Selection can change while the published audio frame stays the same. Do not wait
    // for another block to populate the second instrument's choices or accessible text.
    if (voiceSelect.getNumItems() != snap.capacity)
    {
        voiceSelect.clear(juce::dontSendNotification);
        for (int i = 0; i < snap.capacity; ++i) voiceSelect.addItem("VOICE " + juce::String(i + 1), i + 1);
    }
    voiceSelect.setSelectedId(voice + 1, juce::dontSendNotification);
    setDescription("Engine control taps, voice " + juce::String(voice + 1) + (channel == 0 ? ", rank I" : ", rank II")
        + ": oscillator " + hertzText(snap.oscillatorHz) + ", HPF " + hertzText(snap.hpfHz) + ", LPF " + hertzText(snap.lpfHz)
        + ", filter envelope " + juce::String(snap.filterEnvelope, 3) + ", amplitude envelope " + juce::String(snap.ampEnvelope, 3)
        + ". Curves show recent engine values at a 15 Hz viewing rate.");
}

void ScopeDrawer::visibilityChanged()
{
    processor.setVoiceMonitorEnabled(isVisible());
    if (isVisible())
    {
        snap = sample();
        updateControls();
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

    // One lamp per physical slot; extension slots are a second row.
    int count = 0;
    for (int i = 0; i < snap.capacity; ++i)
    {
        const bool on = (snap.voices >> i) & 1u;
        count += on ? 1 : 0;
        drawJewelLamp(g, { 30.0f + (float) (i % 8) * 22.0f, snap.capacity == 16 ? 66.0f + 15.0f * (float) (i / 8) : 70.0f },
                      snap.capacity == 16 ? 4.5f : 6.0f, amber, on);
    }
    say(g, juce::String(count) + "/" + juce::String(snap.capacity), { 196.0f, 62.0f, 36.0f, 18.0f }, 10.5f, ink());

    const char* names[] = { "VCO", "HPF", "LPF", "VCA" };
    const bool active[] = { true, snap.hpfEnabled, snap.lpfEnabled, true };
    const float y = 92.0f, h = 40.0f, w = 80.0f;
    for (int i = 0; i < 4; ++i)
    {
        const Rf box(22.0f + (float) i * 98.0f, y, w, h);
        if (i < 3)
        {
            g.setColour(goldInk());
            g.drawArrow(juce::Line<float>(box.getRight() + 1.0f, y + h * 0.5f, box.getRight() + 18.0f, y + h * 0.5f), 1.0f, 5.0f, 5.0f);
        }
        g.setColour(active[i] ? juce::Colour(0xff2f2c27) : field());
        g.fillRoundedRectangle(box, 3.0f);
        g.setColour(goldInk().withAlpha(active[i] ? 0.85f : 0.35f));
        g.drawRoundedRectangle(box.reduced(0.5f), 3.0f, 1.0f);
        const auto detail = i == 0 ? hertzText(snap.oscillatorHz)
                          : i == 1 ? (active[i] ? hertzText(snap.hpfHz) : juce::String("BYPASS"))
                          : i == 2 ? (active[i] ? hertzText(snap.lpfHz) : juce::String("BYPASS"))
                          : juce::String(snap.ampEnvelope, 3);
        say(g, names[i], box.withTrimmedBottom(h * 0.48f).translated(0.0f, 2.0f), 13.0f, ink(), juce::Justification::centred);
        say(g, detail, box.withTrimmedTop(h * 0.50f).withTrimmedBottom(2.0f), 10.5f, curveInk(), juce::Justification::centred, false);
    }
    const auto note = snap.note >= 0 ? juce::MidiMessage::getMidiNoteName(snap.note, true, true, 3) : juce::String("IDLE");
    say(g, "VOICE " + juce::String(snap.voice + 1) + "  /  " + note + "  /  SINE BYPASSES THE FILTERS",
        { 22.0f, 142.0f, 376.0f, 20.0f }, 10.5f, goldInk(), juce::Justification::centred);

    auto historyAt = [&](std::size_t i) -> const Snapshot&
    {
        return history[(historyNext + history.size() - historyCount + i) % history.size()];
    };
    auto trace = [&](Rf plot, auto valueAt, float low, float high, juce::Colour colour)
    {
        if (historyCount == 0) return;
        juce::Path line;
        for (std::size_t i = 0; i < historyCount; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) std::max<std::size_t>(1, historyCount - 1);
            const float yValue = plot.getBottom() - plot.getHeight() * juce::jlimit(0.0f, 1.0f, (valueAt(historyAt(i)) - low) / (high - low));
            if (i == 0) line.startNewSubPath(x, yValue); else line.lineTo(x, yValue);
        }
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(plot.expanded(1.0f).toNearestIntEdges());
        stroke(g, line, colour);
    };
    auto envelope = [&](Rf r, bool filter)
    {
        const float value = filter ? snap.filterEnvelope : snap.ampEnvelope;
        frame(g, r, filter ? "VCF ENVELOPE" : "VCA ENVELOPE", juce::String(value, 3));
        const Rf plot(r.getX() + 8.0f, r.getY() + 24.0f, r.getWidth() - 16.0f, r.getHeight() - 44.0f);
        grid(g, plot, 8, 4);
        float low = 0.0f, high = 1.0f;
        if (filter)
            for (std::size_t i = 0; i < historyCount; ++i)
            {
                low = std::min(low, historyAt(i).filterEnvelope);
                high = std::max(high, historyAt(i).filterEnvelope);
            }
        trace(plot, [filter](const Snapshot& s) { return filter ? s.filterEnvelope : s.ampEnvelope; }, low, high, curveInk());
        say(g, "RECENT ENGINE VALUES", { r.getX() + 6.0f, r.getBottom() - 17.0f, r.getWidth() - 12.0f, 14.0f },
            10.0f, goldInk(), juce::Justification::centred, false);
    };
    envelope({ 22.0f, 176.0f, 184.0f, 120.0f }, true);
    envelope({ 214.0f, 176.0f, 184.0f, 120.0f }, false);
    const Rf r(22.0f, 304.0f, 376.0f, 140.0f);
    frame(g, r, "EFFECTIVE CUTOFF", "HPF / LPF");
    const Rf plot(r.getX() + 30.0f, r.getY() + 22.0f, r.getWidth() - 40.0f, r.getHeight() - 46.0f);
    grid(g, plot, 8, 3);
    const auto logHz = [](float hz) { return std::log10(std::max(20.0f, hz)); };
    trace(plot, [&](const Snapshot& s) { return logHz(s.hpfHz); }, logHz(20.0f), logHz(20000.0f), juce::Colour(0xff9ccea3));
    trace(plot, [&](const Snapshot& s) { return logHz(s.lpfHz); }, logHz(20.0f), logHz(20000.0f), curveInk());
    const char* labels[] = { "20k", "2k", "200", "20" };
    for (int i = 0; i < 4; ++i)
        say(g, labels[i], { r.getX() + 3.0f, plot.getY() + plot.getHeight() * (float) i / 3.0f - 6.0f, 24.0f, 12.0f },
            9.5f, goldInk(), juce::Justification::centredRight, false);
    say(g, "HPF " + hertzText(snap.hpfHz) + "  /  LPF " + hertzText(snap.lpfHz),
        { r.getX() + 8.0f, r.getBottom() - 18.0f, r.getWidth() - 16.0f, 14.0f }, 10.5f, ink(), juce::Justification::centred, false);
    say(g, "ENGINE CONTROL TAPS  /  15 HZ VIEW", { 22.0f, 448.0f, 376.0f, 14.0f },
        10.0f, goldInk(), juce::Justification::centred, false);
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
    voiceSelect.setBounds(r(236.0f, 56.0f, 162.0f, 35.0f));
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
