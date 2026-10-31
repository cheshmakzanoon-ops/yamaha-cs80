#include "ModernModulationUi.h"

#include "ArtCommon.h"
#include "PluginProcessor.h"
#include "UiComponents.h"
#include "core/FxModels.h"
#include "core/ModulationLfo.h"

#include <cmath>

namespace cs80::ui::modern_layer
{

namespace
{
using namespace cs80::modern;

constexpr float kMargin = 10.0f;
constexpr float kMarkerStrip = 18.0f;
constexpr float kPointRadius = 5.0f;
constexpr float kHitRadius = 9.0f;

juce::RangedAudioParameter* parameter(Cs80AudioProcessor& p, int index)
{
    return p.getAPVTS().getParameter(juce::String(cs80::paramId(index)));
}

// The embedded panel typeface (ws7 ArtCommon), never under the 10.5 px screen legend floor.
juce::Font uiFont(float height, bool bold = false)
{
    return cs80::art::displayFont(std::max(height, cs80::art::kLegendFloorPx), bold ? juce::Font::bold : juce::Font::plain);
}

void styleLabel(juce::Label& label, const juce::String& text, float height = 15.0f)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(uiFont(height));
    label.setColour(juce::Label::textColourId, ivory);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setInterceptsMouseClicks(false, false);
}

void styleCombo(juce::ComboBox& box, const juce::String& title)
{
    box.setColour(juce::ComboBox::backgroundColourId, panelDark.brighter(0.08f));
    box.setColour(juce::ComboBox::textColourId, ivory);
    box.setColour(juce::ComboBox::outlineColourId, lineColour);
    box.setColour(juce::ComboBox::arrowColourId, amber);
    box.setTitle(title);
}

void styleSlider(juce::Slider& slider, const juce::String& title)
{
    slider.setColour(juce::Slider::trackColourId, amber.withAlpha(0.85f));
    slider.setColour(juce::Slider::backgroundColourId, panelDark.brighter(0.15f));
    slider.setColour(juce::Slider::thumbColourId, ivory);
    slider.setColour(juce::Slider::textBoxTextColourId, ivory);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, panelDark);
    slider.setColour(juce::Slider::textBoxOutlineColourId, lineColour);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 20);
    slider.setTitle(title);
}

// Compact readout. Call after the parameter attachment, which installs the parameter's own
// (long) text conversion.
void compactText(juce::Slider& slider, int decimals, const juce::String& suffix = {}, bool signedValue = false)
{
    slider.textFromValueFunction = [decimals, suffix, signedValue](double v)
    {
        return (signedValue && v > 0.0 ? "+" : "") + juce::String(v, decimals) + suffix;
    };
    slider.updateText();
}

void fillPage(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title)
{
    g.setColour(panelDark);
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(lineColour);
    g.drawRoundedRectangle(area.reduced(0.5f), 6.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour(amber);
        g.setFont(uiFont(14.0f, true));
        g.drawText(title, area.reduced(10.0f, 3.0f).removeFromTop(17.0f), juce::Justification::centredLeft);
    }
}

void addSourceItems(juce::ComboBox& box, bool viaList)
{
    for (int s = 0; s < kModSourceCount; ++s)
        box.addItem(s == 0 && viaList ? juce::String("(no via)") : juce::String(sourceName((ModSource) s)), s + 1);
}

MsegShape presetShape(int preset, const MsegShape& current)
{
    MsegShape s;
    s.trigger = current.trigger;
    s.sync = current.sync;
    auto set = [&s](std::initializer_list<MsegPoint> points, MsegLoop loop, int start, int end, bool bipolar)
    {
        s.count = 0;
        for (const auto& p : points) s.points[(std::size_t) s.count++] = p;
        s.loop = loop;
        s.loopStart = start;
        s.loopEnd = end;
        s.bipolar = bipolar;
    };
    switch (preset)
    {
        case 1: { const auto d = defaultShape(0); s.points = d.points; s.count = d.count; s.loop = d.loop;
                  s.loopStart = d.loopStart; s.loopEnd = d.loopEnd; s.bipolar = false; break; }
        case 2: set({ { 0.0f, 0.0f, 0.4f }, { 0.5f, 1.0f, -0.4f }, { 1.0f, 0.0f, 0.0f } }, MsegLoop::always, 0, 2, true); break;
        case 3: set({ { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }, MsegLoop::off, 0, 0, false); break;
        case 4: set({ { 0.0f, 1.0f, 0.8f }, { 1.5f, 0.0f, 0.0f } }, MsegLoop::off, 0, 0, false); break;
        case 5: set({ { 0.0f, 0.5f, 0.0f }, { 0.25f, 1.0f, 0.0f }, { 0.75f, 0.0f, 0.0f }, { 1.0f, 0.5f, 0.0f } },
                    MsegLoop::always, 0, 3, true); break;
        case 6: set({ { 0.0f, 1.0f, 0.0f }, { 0.5f, 1.0f, 0.0f }, { 0.5f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
                    MsegLoop::always, 0, 3, false); break;
        default: set({ { 0.0f, 0.25f, 0.0f }, { 0.25f, 0.25f, 0.0f }, { 0.25f, 0.9f, 0.0f }, { 0.5f, 0.9f, 0.0f },
                       { 0.5f, 0.5f, 0.0f }, { 0.75f, 0.5f, 0.0f }, { 0.75f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
                     MsegLoop::always, 0, 7, false); break;
    }
    return sanitizeShape(s);
}
} // namespace

// ---- MsegEditor -------------------------------------------------------------------------

MsegEditor::MsegEditor(Cs80AudioProcessor& p) : processor(p)
{
    setWantsKeyboardFocus(true);
    setTitle("MSEG editor");
    setDescription("Drag points to move them, drag a segment to bend it, double-click to add or delete a point, "
                   "drag the S and E flags to set the loop.");
    processor.getModernLayer().addChangeListener(this);
    reload();
    startTimerHz(30);
}

MsegEditor::~MsegEditor()
{
    processor.getModernLayer().removeChangeListener(this);
}

void MsegEditor::setMsegIndex(int index)
{
    msegIndex = juce::jlimit(0, kMsegCount - 1, index);
    selected = hovered = -1;
    reload();
}

void MsegEditor::reload()
{
    shape = processor.getModernLayer().getShape(msegIndex);
    if (selected >= shape.count) selected = -1;
    repaint();
}

void MsegEditor::commit()
{
    processor.getModernLayer().setShape(msegIndex, shape);
    shape = processor.getModernLayer().getShape(msegIndex);
    repaint();
}

juce::Rectangle<float> MsegEditor::plotArea() const
{
    return getLocalBounds().toFloat().reduced(kMargin).withTrimmedTop(kMarkerStrip).withTrimmedBottom(14.0f);
}

float MsegEditor::viewLength() const
{
    if (frozenViewLength > 0.0f)
        return frozenViewLength;
    return std::max({ shape.endTime() * 1.12f, shape.sync ? 1.0f : 0.25f });
}

juce::Point<float> MsegEditor::toScreen(double time, double level) const
{
    const auto a = plotArea();
    return { a.getX() + (float) (time / viewLength()) * a.getWidth(), a.getBottom() - (float) level * a.getHeight() };
}

juce::Point<float> MsegEditor::pointPosition(int point) const
{
    const auto& p = shape.points[(std::size_t) juce::jlimit(0, shape.count - 1, point)];
    return toScreen(p.time, p.level);
}

double MsegEditor::timeAt(float x) const
{
    const auto a = plotArea();
    return std::max(0.0, (double) (x - a.getX()) / std::max(1.0f, a.getWidth()) * viewLength());
}

double MsegEditor::levelAt(float y) const
{
    const auto a = plotArea();
    return juce::jlimit(0.0, 1.0, (double) (a.getBottom() - y) / std::max(1.0f, a.getHeight()));
}

juce::Rectangle<float> MsegEditor::loopMarkerArea(bool end) const
{
    const float x = pointPosition(end ? shape.loopEnd : shape.loopStart).x;
    const float top = kMargin;
    return { x - (end ? 0.0f : 14.0f), top, 14.0f, kMarkerStrip - 3.0f };
}

int MsegEditor::hitPoint(juce::Point<float> position) const
{
    int best = -1;
    float bestDistance = kHitRadius;
    for (int i = 0; i < shape.count; ++i)
    {
        const float d = pointPosition(i).getDistanceFrom(position);
        if (d <= bestDistance) { best = i; bestDistance = d; }
    }
    return best;
}

int MsegEditor::segmentAt(float x) const
{
    int segment = 0;
    for (int i = 0; i + 1 < shape.count; ++i)
        if (pointPosition(i).x <= x) segment = i;
    return juce::jlimit(0, std::max(0, shape.count - 2), segment);
}

int MsegEditor::nearestPoint(float x) const
{
    int best = 0;
    for (int i = 1; i < shape.count; ++i)
        if (std::abs(pointPosition(i).x - x) < std::abs(pointPosition(best).x - x)) best = i;
    return best;
}

void MsegEditor::paint(juce::Graphics& g)
{
    fillPage(g, getLocalBounds().toFloat(), {});
    const auto a = plotArea();
    g.setColour(charcoal.darker(0.35f));
    g.fillRect(a);

    // Grid: level quarters, and seconds or beats along time.
    g.setColour(lineColour.withAlpha(0.55f));
    for (int q = 0; q <= 4; ++q)
    {
        const float y = a.getBottom() - a.getHeight() * (float) q / 4.0f;
        g.drawHorizontalLine((int) y, a.getX(), a.getRight());
    }
    if (shape.bipolar)
    {
        g.setColour(ivory.withAlpha(0.35f));
        g.drawHorizontalLine((int) a.getCentreY(), a.getX(), a.getRight());
    }
    const float length = viewLength();
    const float step = length > 16.0f ? 4.0f : length > 4.0f ? 1.0f : length > 1.0f ? 0.25f : 0.125f;
    g.setFont(uiFont(10.0f));
    for (float t = 0.0f; t <= length; t += step)
    {
        const float x = toScreen(t, 0.0).x;
        g.setColour(lineColour.withAlpha(0.5f));
        g.drawVerticalLine((int) x, a.getY(), a.getBottom());
        g.setColour(ivoryDim);
        g.drawText(juce::String(t, step < 1.0f ? 2 : 0) + (shape.sync ? " b" : " s"),
                   juce::Rectangle<float>(x + 2.0f, a.getBottom() + 1.0f, 48.0f, 12.0f), juce::Justification::centredLeft);
    }

    // Loop region.
    if (shape.loop != MsegLoop::off)
    {
        const float x0 = pointPosition(shape.loopStart).x, x1 = pointPosition(shape.loopEnd).x;
        g.setColour(teal.withAlpha(shape.loop == MsegLoop::always ? 0.20f : 0.13f));
        g.fillRect(juce::Rectangle<float>(x0, a.getY(), std::max(2.0f, x1 - x0), a.getHeight()));
        for (bool end : { false, true })
        {
            const auto m = loopMarkerArea(end);
            juce::Path flag;
            const float x = end ? m.getX() : m.getRight();
            flag.addTriangle(x, m.getY(), x, m.getBottom(), end ? m.getRight() : m.getX(), m.getCentreY());
            g.setColour(drag == (end ? Drag::loopEnd : Drag::loopStart) ? amberBright : teal.brighter(0.4f));
            g.fillPath(flag);
            g.drawVerticalLine((int) x, m.getBottom(), a.getBottom());
            g.setColour(ivory);
            g.setFont(uiFont(9.0f, true));
            g.drawText(end ? "E" : "S", m.translated(end ? 1.0f : -1.0f, 0.0f), juce::Justification::centred);
        }
    }

    // The curve itself, sampled per pixel, then held flat after the last point.
    juce::Path path;
    const double end = shape.endTime();
    for (float x = a.getX(); x <= a.getRight(); x += 1.5f)
    {
        const double t = timeAt(x);
        const auto pt = toScreen(t, msegLevelAt(shape, std::min(t, end)));
        if (x == a.getX()) path.startNewSubPath(pt);
        else path.lineTo(pt);
    }
    g.setColour(amber.withAlpha(0.18f));
    juce::Path fill(path);
    fill.lineTo(a.getRight(), a.getBottom());
    fill.lineTo(a.getX(), a.getBottom());
    fill.closeSubPath();
    g.fillPath(fill);
    g.setColour(amberBright);
    g.strokePath(path, juce::PathStrokeType(1.8f));

    for (int i = 0; i < shape.count; ++i)
    {
        const auto p = pointPosition(i);
        const bool active = i == selected || (drag == Drag::point && i == dragIndex);
        g.setColour(active ? amberBright : (i == hovered ? ivory : ivoryDim));
        if (active) g.fillEllipse(p.x - kPointRadius, p.y - kPointRadius, 2 * kPointRadius, 2 * kPointRadius);
        else g.drawEllipse(p.x - kPointRadius, p.y - kPointRadius, 2 * kPointRadius, 2 * kPointRadius, 1.6f);
    }

    if (playhead >= 0.0)
    {
        const float x = toScreen(playhead, 0.0).x;
        g.setColour(ivory.withAlpha(0.8f));
        g.drawVerticalLine((int) x, a.getY(), a.getBottom());
    }

    g.setColour(ivoryDim);
    g.setFont(uiFont(10.0f));
    const char* loopNames[] = { "one shot", "sustain loop", "loop" };
    const char* triggerNames[] = { "poly", "mono", "free" };
    g.drawText("MSEG " + juce::String(msegIndex + 1) + "  " + loopNames[(int) shape.loop] + ", "
                   + triggerNames[(int) shape.trigger] + (shape.sync ? ", synced" : "") + (shape.bipolar ? ", bipolar" : "")
                   + "  " + juce::String(shape.count) + " points",
               getLocalBounds().toFloat().reduced(kMargin + 18.0f, 0.0f).removeFromTop(kMarkerStrip + kMargin - 1.0f),
               juce::Justification::centredRight);
}

void MsegEditor::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    frozenViewLength = viewLength();
    dragStart = e.position;
    drag = Drag::none;
    if (auto* undo = processor.getAPVTS().undoManager)
        undo->beginNewTransaction("MSEG edit");
    if (shape.loop != MsegLoop::off && loopMarkerArea(true).expanded(2.0f).contains(e.position))
        drag = Drag::loopEnd;
    else if (shape.loop != MsegLoop::off && loopMarkerArea(false).expanded(2.0f).contains(e.position))
        drag = Drag::loopStart;
    else if (const int i = hitPoint(e.position); i >= 0)
    {
        drag = Drag::point;
        dragIndex = selected = i;
    }
    else if (plotArea().contains(e.position) && shape.count >= 2)
    {
        drag = Drag::curve;
        dragIndex = segmentAt(e.position.x);
        dragStartCurve = shape.points[(std::size_t) dragIndex].curve;
    }
    repaint();
}

void MsegEditor::mouseDrag(const juce::MouseEvent& e)
{
    const bool fine = e.mods.isShiftDown();
    switch (drag)
    {
        case Drag::point:
        {
            auto& p = shape.points[(std::size_t) dragIndex];
            const auto target = fine ? dragStart + (e.position - dragStart) * 0.25f : e.position;
            if (dragIndex > 0)
            {
                const double lo = shape.points[(std::size_t) dragIndex - 1].time;
                const double hi = dragIndex + 1 < shape.count ? shape.points[(std::size_t) dragIndex + 1].time : (double) kMsegMaxTime;
                p.time = (float) juce::jlimit(lo, hi, timeAt(target.x));
            }
            p.level = (float) levelAt(target.y);
            commit();
            break;
        }
        case Drag::curve:
        {
            const float dy = (dragStart.y - e.position.y) / (fine ? 320.0f : 80.0f);
            // Up bends a rising segment toward a fast start, a falling one toward a slow start.
            const bool rising = shape.points[(std::size_t) dragIndex + 1].level >= shape.points[(std::size_t) dragIndex].level;
            shape.points[(std::size_t) dragIndex].curve = juce::jlimit(-1.0f, 1.0f, dragStartCurve + (rising ? dy : -dy));
            commit();
            break;
        }
        case Drag::loopStart:
            shape.loopStart = std::min(nearestPoint(e.position.x), shape.loopEnd);
            commit();
            break;
        case Drag::loopEnd:
            shape.loopEnd = std::max(nearestPoint(e.position.x), shape.loopStart);
            commit();
            break;
        case Drag::none: break;
    }
}

void MsegEditor::mouseUp(const juce::MouseEvent&)
{
    drag = Drag::none;
    frozenViewLength = 0.0f;
    repaint();
}

void MsegEditor::mouseMove(const juce::MouseEvent& e)
{
    const int h = hitPoint(e.position);
    if (h != hovered)
    {
        hovered = h;
        repaint();
    }
    setMouseCursor(h >= 0 ? juce::MouseCursor::DraggingHandCursor
                          : plotArea().contains(e.position) ? juce::MouseCursor::UpDownResizeCursor
                                                            : juce::MouseCursor::NormalCursor);
}

void MsegEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    drag = Drag::none;
    if (const int i = hitPoint(e.position); i > 0 && shape.count > 2)
    {
        for (int k = i; k + 1 < shape.count; ++k)
            shape.points[(std::size_t) k] = shape.points[(std::size_t) k + 1];
        --shape.count;
        if (shape.loopStart >= i) shape.loopStart = std::max(0, shape.loopStart - 1);
        if (shape.loopEnd >= i) shape.loopEnd = std::max(shape.loopStart, shape.loopEnd - 1);
        selected = -1;
        commit();
        return;
    }
    if (! plotArea().contains(e.position) || shape.count >= kMsegMaxPoints || hitPoint(e.position) >= 0)
        return;
    const auto t = (float) timeAt(e.position.x), l = (float) levelAt(e.position.y);
    int at = shape.count;
    for (int i = 1; i < shape.count; ++i)
        if (shape.points[(std::size_t) i].time > t) { at = i; break; }
    for (int k = shape.count; k > at; --k)
        shape.points[(std::size_t) k] = shape.points[(std::size_t) k - 1];
    shape.points[(std::size_t) at] = { t, l, 0.0f };
    ++shape.count;
    if (shape.loopStart >= at) ++shape.loopStart;
    if (shape.loopEnd >= at) ++shape.loopEnd;
    selected = at;
    commit();
}

bool MsegEditor::keyPressed(const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) && selected > 0 && shape.count > 2)
    {
        for (int k = selected; k + 1 < shape.count; ++k)
            shape.points[(std::size_t) k] = shape.points[(std::size_t) k + 1];
        --shape.count;
        if (shape.loopStart >= selected) shape.loopStart = std::max(0, shape.loopStart - 1);
        if (shape.loopEnd >= selected) shape.loopEnd = std::max(shape.loopStart, shape.loopEnd - 1);
        selected = -1;
        commit();
        return true;
    }
    return false;
}

void MsegEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (drag == Drag::none)
        reload();
}

void MsegEditor::timerCallback()
{
    const double position = processor.getModernLayer().getDisplayPosition(msegIndex);
    if (position != playhead)
    {
        playhead = position;
        repaint();
    }
}

// ---- MsegPage ---------------------------------------------------------------------------

MsegPage::MsegPage(Cs80AudioProcessor& p) : processor(p), editor(p)
{
    for (int i = 0; i < kMsegCount; ++i)
    {
        auto& b = selectors[(size_t) i];
        b.setButtonText("MSEG " + juce::String(i + 1));
        b.setClickingTogglesState(true);
        b.setRadioGroupId(0x5e9);
        b.setColour(juce::TextButton::buttonColourId, panelDark.brighter(0.1f));
        b.setColour(juce::TextButton::buttonOnColourId, amber.darker(0.2f));
        b.setColour(juce::TextButton::textColourOffId, ivory);
        b.setColour(juce::TextButton::textColourOnId, charcoal);
        b.onClick = [this, i] { if (selectors[(size_t) i].getToggleState()) selectMseg(i); };
        addAndMakeVisible(b);
    }
    addAndMakeVisible(editor);
    loopBox.addItemList({ "One shot", "Sustain loop", "Loop always" }, 1);
    triggerBox.addItemList({ "Poly (per note)", "Mono (legato)", "Free run" }, 1);
    presetBox.setTextWhenNothingSelected("Shape preset...");
    presetBox.addItemList({ "Envelope (sustain point)", "Rise / fall loop", "Ramp up", "Decay", "Triangle LFO",
                            "Square gate", "Steps" }, 1);
    styleCombo(loopBox, "MSEG loop mode");
    styleCombo(triggerBox, "MSEG trigger mode");
    styleCombo(presetBox, "MSEG shape preset");
    for (auto* t : { &syncButton, &bipolarButton })
    {
        t->setColour(juce::ToggleButton::textColourId, ivory);
        t->setColour(juce::ToggleButton::tickColourId, amber);
        addAndMakeVisible(*t);
    }
    syncButton.setTooltip("Segment times in beats of the host tempo (120 BPM when the host reports none)");
    bipolarButton.setTooltip("Read the shape as -1..+1 instead of 0..1 in the matrix");
    loopBox.onChange = [this] { writeShape([this](MsegShape& s) { s.loop = (MsegLoop) (loopBox.getSelectedId() - 1); }); };
    triggerBox.onChange = [this] { writeShape([this](MsegShape& s) { s.trigger = (MsegTrigger) (triggerBox.getSelectedId() - 1); }); };
    syncButton.onClick = [this] { writeShape([this](MsegShape& s) { s.sync = syncButton.getToggleState(); }); };
    bipolarButton.onClick = [this] { writeShape([this](MsegShape& s) { s.bipolar = bipolarButton.getToggleState(); }); };
    presetBox.onChange = [this]
    {
        if (refreshing || presetBox.getSelectedId() <= 0) return;
        const int preset = presetBox.getSelectedId();
        processor.getModernLayer().setShape(editor.getMsegIndex(),
                                            presetShape(preset, processor.getModernLayer().getShape(editor.getMsegIndex())));
        presetBox.setSelectedId(0, juce::dontSendNotification);
    };
    for (auto* c : { &loopBox, &triggerBox, &presetBox })
        addAndMakeVisible(*c);
    styleSlider(rate, "MSEG rate");
    styleLabel(rateLabel, "Rate");
    addAndMakeVisible(rate);
    addAndMakeVisible(rateLabel);
    processor.getModernLayer().addChangeListener(this);
    selectMseg(0);
}

MsegPage::~MsegPage()
{
    processor.getModernLayer().removeChangeListener(this);
}

void MsegPage::selectMseg(int index)
{
    index = juce::jlimit(0, kMsegCount - 1, index);
    selectors[(size_t) index].setToggleState(true, juce::dontSendNotification);
    editor.setMsegIndex(index);
    rateAttachment.reset();
    if (auto* p = parameter(processor, P_mseg1Rate + index))
        rateAttachment = std::make_unique<juce::SliderParameterAttachment>(*p, rate);
    rate.setDoubleClickReturnValue(true, 0.0); // octaves: 0 = 1x
    rate.textFromValueFunction = [](double octaves) { return juce::String(std::exp2(octaves), 2) + "x"; };
    rate.valueFromTextFunction = [](const juce::String& text)
    { return juce::jlimit(-3.0, 3.0, std::log2(juce::jmax(0.125, text.getDoubleValue()))); };
    rate.updateText();
    refreshControls();
}

void MsegPage::refreshControls()
{
    const juce::ScopedValueSetter<bool> guard(refreshing, true);
    const auto s = processor.getModernLayer().getShape(editor.getMsegIndex());
    loopBox.setSelectedId((int) s.loop + 1, juce::dontSendNotification);
    triggerBox.setSelectedId((int) s.trigger + 1, juce::dontSendNotification);
    syncButton.setToggleState(s.sync, juce::dontSendNotification);
    bipolarButton.setToggleState(s.bipolar, juce::dontSendNotification);
}

void MsegPage::writeShape(const std::function<void(MsegShape&)>& edit)
{
    if (refreshing) return;
    auto s = processor.getModernLayer().getShape(editor.getMsegIndex());
    edit(s);
    processor.getModernLayer().setShape(editor.getMsegIndex(), s);
}

void MsegPage::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refreshControls();
}

void MsegPage::paint(juce::Graphics& g)
{
    fillPage(g, getLocalBounds().toFloat(), {});
}

void MsegPage::resized()
{
    auto area = getLocalBounds().reduced(8);
    auto top = area.removeFromTop(26);
    for (auto& b : selectors)
        b.setBounds(top.removeFromLeft(78).reduced(2, 0));
    top.removeFromLeft(10);
    presetBox.setBounds(top.removeFromRight(190).reduced(2, 0));
    area.removeFromTop(6);
    auto controls = area.removeFromBottom(28);
    area.removeFromBottom(6);
    editor.setBounds(area);
    loopBox.setBounds(controls.removeFromLeft(140).reduced(2, 2));
    triggerBox.setBounds(controls.removeFromLeft(150).reduced(2, 2));
    syncButton.setBounds(controls.removeFromLeft(70).reduced(2, 2));
    bipolarButton.setBounds(controls.removeFromLeft(86).reduced(2, 2));
    rateLabel.setBounds(controls.removeFromLeft(40));
    rate.setBounds(controls.reduced(2, 2));
}

// ---- LfoPanel ---------------------------------------------------------------------------

LfoPanel::LfoPanel(Cs80AudioProcessor& p)
{
    const int waveIds[3] = { P_lfo1Wave, P_lfo2Wave, P_lfo3Wave };
    const int rateIds[3] = { P_lfo1Rate, P_lfo2Rate, P_lfo3Rate };
    const int depthIds[3] = { P_lfo1Depth, P_lfo2Depth, P_lfo3Depth };
    const int destIds[3] = { P_lfo1Dest, P_lfo2Dest, P_lfo3Dest };
    for (int i = 0; i < 3; ++i)
    {
        auto& r = rows[(size_t) i];
        styleLabel(r.title, "LFO " + juce::String(i + 1), 14.0f);
        for (int w = 0; w < kLfoWaveCount; ++w)
            r.wave.addItem(lfoWaveName(w), w + 1);
        r.destination.addItemList({ "No direct route", "Pitch", "Filter", "Amp", "Pan", "PWM" }, 1);
        styleCombo(r.wave, "LFO " + juce::String(i + 1) + " waveform");
        styleCombo(r.destination, "LFO " + juce::String(i + 1) + " direct destination");
        styleSlider(r.rate, "LFO " + juce::String(i + 1) + " rate");
        styleSlider(r.depth, "LFO " + juce::String(i + 1) + " depth");
        if (auto* q = parameter(p, waveIds[i])) r.waveAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*q, r.wave);
        if (auto* q = parameter(p, destIds[i])) r.destinationAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*q, r.destination);
        if (auto* q = parameter(p, rateIds[i])) r.rateAttachment = std::make_unique<juce::SliderParameterAttachment>(*q, r.rate);
        if (auto* q = parameter(p, depthIds[i])) r.depthAttachment = std::make_unique<juce::SliderParameterAttachment>(*q, r.depth);
        compactText(r.rate, 2, " Hz");
        compactText(r.depth, 2);
        for (juce::Component* c : { (juce::Component*) &r.title, (juce::Component*) &r.wave, (juce::Component*) &r.destination,
                                    (juce::Component*) &r.rate, (juce::Component*) &r.depth })
            addAndMakeVisible(*c);
    }
}

void LfoPanel::paint(juce::Graphics& g)
{
    fillPage(g, getLocalBounds().toFloat(), "LFOS");
}

void LfoPanel::resized()
{
    auto area = getLocalBounds().reduced(8).withTrimmedTop(20);
    const int h = area.getHeight() / 3;
    auto fit = [](juce::Rectangle<int> b) { return b.withSizeKeepingCentre(b.getWidth(), juce::jmin(b.getHeight(), 26)); };
    for (auto& r : rows)
    {
        auto row = area.removeFromTop(h).reduced(0, 2);
        auto line1 = row.removeFromTop(row.getHeight() / 2);
        r.title.setBounds(line1.removeFromLeft(52));
        r.wave.setBounds(fit(line1.removeFromLeft(juce::jmax(110, line1.getWidth() / 2)).reduced(2, 1)));
        r.destination.setBounds(fit(line1.reduced(2, 1)));
        row.removeFromLeft(52);
        r.rate.setBounds(fit(row.removeFromLeft(row.getWidth() / 2).reduced(2, 1)));
        r.depth.setBounds(fit(row.reduced(2, 1)));
    }
}

// ---- ModMatrixPage ----------------------------------------------------------------------

ModMatrixPage::ModMatrixPage(Cs80AudioProcessor& p) : processor(p)
{
    for (int i = 0; i < kModSlots; ++i)
    {
        auto& r = rows[(size_t) i];
        styleLabel(r.index, juce::String(i + 1), 13.0f);
        r.index.setJustificationType(juce::Justification::centred);
        addSourceItems(r.source, false);
        addSourceItems(r.via, true);
        for (int d = 0; d < kModDestinationCount; ++d)
            r.destination.addItem(destinationName((ModDestination) d), d + 1);
        r.target.addItemList({ "I+II", "I", "II" }, 1);
        const auto n = juce::String(i + 1);
        styleCombo(r.source, "Slot " + n + " source");
        styleCombo(r.via, "Slot " + n + " via");
        styleCombo(r.destination, "Slot " + n + " destination");
        styleCombo(r.target, "Slot " + n + " rank");
        styleSlider(r.amount, "Slot " + n + " amount");
        r.amount.setDoubleClickReturnValue(true, 0.0);
        if (auto* q = parameter(p, P_mod1Amount + i))
            r.amountAttachment = std::make_unique<juce::SliderParameterAttachment>(*q, r.amount);
        compactText(r.amount, 2, {}, true);
        r.amount.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
        for (auto* c : { &r.source, &r.via, &r.destination, &r.target })
            c->onChange = [this, i] { write(i); };
        for (juce::Component* c : { (juce::Component*) &r.index, (juce::Component*) &r.source, (juce::Component*) &r.via,
                                    (juce::Component*) &r.destination, (juce::Component*) &r.target, (juce::Component*) &r.amount })
            addAndMakeVisible(*c);
    }
    processor.getModernLayer().addChangeListener(this);
    refresh();
}

ModMatrixPage::~ModMatrixPage()
{
    processor.getModernLayer().removeChangeListener(this);
}

void ModMatrixPage::refresh()
{
    const juce::ScopedValueSetter<bool> guard(refreshing, true);
    const auto config = processor.getModernLayer().getConfig();
    for (int i = 0; i < kModSlots; ++i)
    {
        auto& r = rows[(size_t) i];
        const auto& s = config.slots[(size_t) i];
        r.source.setSelectedId((int) s.source + 1, juce::dontSendNotification);
        r.via.setSelectedId((int) s.via + 1, juce::dontSendNotification);
        r.destination.setSelectedId((int) s.destination + 1, juce::dontSendNotification);
        r.target.setSelectedId((int) s.target + 1, juce::dontSendNotification);
        const bool routed = s.source != ModSource::none && s.destination != ModDestination::none;
        r.amount.setAlpha(routed ? 1.0f : 0.55f);
        r.target.setEnabled(isRankDestination(s.destination));
    }
}

void ModMatrixPage::write(int slot)
{
    if (refreshing) return;
    auto& r = rows[(size_t) slot];
    ModSlot s;
    s.source = (ModSource) juce::jmax(0, r.source.getSelectedId() - 1);
    s.via = (ModSource) juce::jmax(0, r.via.getSelectedId() - 1);
    s.destination = (ModDestination) juce::jmax(0, r.destination.getSelectedId() - 1);
    s.target = (ModTarget) juce::jmax(0, r.target.getSelectedId() - 1);
    processor.getModernLayer().setSlot(slot, s);
    refresh();
}

void ModMatrixPage::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refresh();
}

void ModMatrixPage::paint(juce::Graphics& g)
{
    fillPage(g, getLocalBounds().toFloat(), "MODULATION MATRIX");
    auto header = getLocalBounds().reduced(8).withTrimmedTop(20).removeFromTop(16);
    g.setColour(ivoryDim);
    g.setFont(uiFont(11.5f));
    const int w = header.getWidth() - 28;
    header.removeFromLeft(28);
    const float columns[] = { 0.23f, 0.19f, 0.22f, 0.12f, 0.24f };
    const char* names[] = { "SOURCE", "VIA", "DESTINATION", "RANK", "AMOUNT" };
    for (int c = 0; c < 5; ++c)
        g.drawText(names[c], header.removeFromLeft((int) (w * columns[c])), juce::Justification::centredLeft);
}

void ModMatrixPage::resized()
{
    auto area = getLocalBounds().reduced(8).withTrimmedTop(38);
    const int h = juce::jmax(22, area.getHeight() / kModSlots);
    for (auto& r : rows)
    {
        auto row = area.removeFromTop(h).reduced(0, 1);
        r.index.setBounds(row.removeFromLeft(28));
        const int w = row.getWidth();
        r.source.setBounds(row.removeFromLeft((int) (w * 0.23f)).reduced(2, 0));
        r.via.setBounds(row.removeFromLeft((int) (w * 0.19f)).reduced(2, 0));
        r.destination.setBounds(row.removeFromLeft((int) (w * 0.22f)).reduced(2, 0));
        r.target.setBounds(row.removeFromLeft((int) (w * 0.12f)).reduced(2, 0));
        r.amount.setBounds(row.reduced(2, 0));
    }
}

// ---- KeyboardModulationPage -------------------------------------------------------------

KeyboardModulationPage::KeyboardModulationPage(Cs80AudioProcessor& p)
{
    const int ids[4] = { P_modVelocityCurve, P_modPressureCurve, P_modWheelCurve, P_modKeyCurve };
    const char* titles[4] = { "VELOCITY", "AFTERTOUCH (poly, channel, shared)", "MOD WHEEL", "KEY FOLLOW (61-key surface)" };
    for (int i = 0; i < 4; ++i)
    {
        auto& c = curves[(size_t) i];
        styleLabel(c.title, titles[i], 14.0f);
        styleSlider(c.slider, juce::String(titles[i]) + " curve");
        c.slider.setDoubleClickReturnValue(true, 0.0);
        c.slider.setTooltip("-1 hard (x^4), 0 linear, +1 soft (x^1/4)");
        c.bipolar = i == 3;
        if (auto* q = parameter(p, ids[i]))
            c.attachment = std::make_unique<juce::SliderParameterAttachment>(*q, c.slider);
        compactText(c.slider, 2, {}, true);
        c.slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible(c.title);
        addAndMakeVisible(c.slider);
    }
    styleLabel(note, "These curves shape the matrix sources only. The original initial and after touch paths "
                     "(TWS/TRG) are not altered.", 12.5f);
    note.setColour(juce::Label::textColourId, ivoryDim);
    addAndMakeVisible(note);
}

void KeyboardModulationPage::paint(juce::Graphics& g)
{
    fillPage(g, getLocalBounds().toFloat(), "KEYBOARD MODULATION");
    for (const auto& c : curves)
    {
        const auto r = c.plot.toFloat();
        g.setColour(charcoal.darker(0.3f));
        g.fillRect(r);
        g.setColour(lineColour);
        g.drawRect(r);
        g.drawLine(r.getX(), r.getBottom(), r.getRight(), r.getY(), 0.8f); // linear reference
        juce::Path path;
        const float shape = (float) c.slider.getValue();
        for (int k = 0; k <= 64; ++k)
        {
            const float x = (float) k / 64.0f;
            const float in = c.bipolar ? 2.0f * x - 1.0f : x;
            const float y = c.bipolar ? 0.5f + 0.5f * bipolarResponseCurve(in, shape) : responseCurve(in, shape);
            const juce::Point<float> pt(r.getX() + x * r.getWidth(), r.getBottom() - y * r.getHeight());
            if (k == 0) path.startNewSubPath(pt);
            else path.lineTo(pt);
        }
        g.setColour(amberBright);
        g.strokePath(path, juce::PathStrokeType(1.8f));
    }
}

void KeyboardModulationPage::resized()
{
    auto area = getLocalBounds().reduced(10).withTrimmedTop(22);
    note.setBounds(area.removeFromBottom(34));
    const int w = area.getWidth() / 4;
    for (auto& c : curves)
    {
        auto column = area.removeFromLeft(w).reduced(6, 0);
        c.title.setBounds(column.removeFromTop(20));
        c.slider.setBounds(column.removeFromBottom(26));
        column.removeFromBottom(6);
        const int side = juce::jmin(column.getWidth(), column.getHeight());
        c.plot = column.withSizeKeepingCentre(side, side);
    }
}

// ---- ModernFxPage -----------------------------------------------------------------------

ModernFxPage::ModernFxPage(Cs80AudioProcessor& p) : processor(p)
{
    for (int i = 0; i < 3; ++i)
    {
        auto& s = slots[(size_t) i];
        const int base = P_fx1Model + i * (P_fx2Model - P_fx1Model);
        const int typeId = i == 0 ? P_fx1Type : i == 1 ? P_fx2Type : P_fx3Type;
        const int mixId = i == 0 ? P_fx1Mix : i == 1 ? P_fx2Mix : P_fx3Mix;
        const auto n = juce::String(i + 1);
        styleLabel(s.title, "FX " + n, 15.0f);
        for (int m = 0; m < fx::kModelCount; ++m)
            s.model.addItem(fx::modelName(m), m + 1);
        s.type.addItemList({ "Off", "Chorus", "Phaser", "Tape delay", "Plate reverb", "Drive" }, 1);
        s.oversampling.addItemList({ "1x", "2x", "4x", "8x" }, 1);
        styleCombo(s.model, "FX " + n + " model");
        styleCombo(s.type, "FX " + n + " classic type");
        styleCombo(s.oversampling, "FX " + n + " drive oversampling");
        styleSlider(s.mix, "FX " + n + " mix");
        styleSlider(s.a, "FX " + n + " parameter A");
        styleSlider(s.b, "FX " + n + " parameter B");
        styleLabel(s.mixLabel, "Mix");
        styleLabel(s.aLabel, "A");
        styleLabel(s.bLabel, "B");
        auto attachCombo = [&p](int index, juce::ComboBox& box, std::unique_ptr<juce::ComboBoxParameterAttachment>& out)
        { if (auto* q = parameter(p, index)) out = std::make_unique<juce::ComboBoxParameterAttachment>(*q, box); };
        auto attachSlider = [&p](int index, juce::Slider& slider, std::unique_ptr<juce::SliderParameterAttachment>& out)
        { if (auto* q = parameter(p, index)) out = std::make_unique<juce::SliderParameterAttachment>(*q, slider); };
        attachCombo(base, s.model, s.modelAttachment);
        attachCombo(typeId, s.type, s.typeAttachment);
        attachCombo(base + 3, s.oversampling, s.oversamplingAttachment);
        attachSlider(mixId, s.mix, s.mixAttachment);
        attachSlider(base + 1, s.a, s.aAttachment);
        attachSlider(base + 2, s.b, s.bAttachment);
        for (auto* slider : { &s.mix, &s.a, &s.b })
            compactText(*slider, 2);
        for (juce::Component* c : { (juce::Component*) &s.title, (juce::Component*) &s.mixLabel,
                                    (juce::Component*) &s.aLabel, (juce::Component*) &s.bLabel,
                                    (juce::Component*) &s.model, (juce::Component*) &s.type, (juce::Component*) &s.oversampling,
                                    (juce::Component*) &s.mix, (juce::Component*) &s.a, (juce::Component*) &s.b })
            addAndMakeVisible(*c);
    }
    gainComp.setColour(juce::ToggleButton::textColourId, ivory);
    gainComp.setColour(juce::ToggleButton::tickColourId, amber);
    gainComp.setTooltip("Each slot's K-weighted (ITU-R BS.1770) output loudness follows its input loudness");
    if (auto* q = parameter(p, P_fxGainCompensation))
        gainCompAttachment = std::make_unique<juce::ButtonParameterAttachment>(*q, gainComp);
    addAndMakeVisible(gainComp);
    refreshLabels();
    startTimerHz(10);
}

void ModernFxPage::refreshLabels()
{
    for (int i = 0; i < 3; ++i)
    {
        auto& s = slots[(size_t) i];
        const int model = juce::jmax(0, s.model.getSelectedId() - 1);
        if (model == s.shownModel) continue;
        s.shownModel = model;
        const bool classic = model == 0;
        s.aLabel.setText(classic ? "-" : fx::paramAName(model), juce::dontSendNotification);
        s.bLabel.setText(classic ? "-" : fx::paramBName(model), juce::dontSendNotification);
        s.type.setEnabled(classic);
        s.a.setEnabled(! classic);
        s.b.setEnabled(! classic);
        s.oversampling.setEnabled(model == (int) fx::Model::drive);
    }
}

void ModernFxPage::timerCallback()
{
    refreshLabels();
}

void ModernFxPage::paint(juce::Graphics& g)
{
    fillPage(g, getLocalBounds().toFloat(), "FX  (three serial slots after the CS-80 chorus)");
    auto area = getLocalBounds().reduced(8).withTrimmedTop(22).withTrimmedBottom(34);
    const int w = area.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
    {
        g.setColour(lineColour.withAlpha(0.6f));
        g.drawRoundedRectangle(area.removeFromLeft(w).reduced(4).toFloat(), 4.0f, 1.0f);
    }
}

void ModernFxPage::resized()
{
    auto area = getLocalBounds().reduced(8).withTrimmedTop(22);
    gainComp.setBounds(area.removeFromBottom(30).reduced(6, 4));
    const int w = area.getWidth() / 3;
    for (auto& s : slots)
    {
        auto column = area.removeFromLeft(w).reduced(12, 10);
        const int h = juce::jmin(30, column.getHeight() / 7);
        s.title.setBounds(column.removeFromTop(h));
        s.model.setBounds(column.removeFromTop(h).reduced(0, 2));
        s.type.setBounds(column.removeFromTop(h).reduced(0, 2));
        auto mixRow = column.removeFromTop(h);
        s.mixLabel.setBounds(mixRow.removeFromLeft(54));
        s.mix.setBounds(mixRow.reduced(0, 2));
        auto aRow = column.removeFromTop(h);
        s.aLabel.setBounds(aRow.removeFromLeft(54));
        s.a.setBounds(aRow.reduced(0, 2));
        auto bRow = column.removeFromTop(h);
        s.bLabel.setBounds(bRow.removeFromLeft(54));
        s.b.setBounds(bRow.reduced(0, 2));
        s.oversampling.setBounds(column.removeFromTop(h).reduced(0, 2).removeFromLeft(90));
    }
}

// ---- ModernModulationPage ---------------------------------------------------------------

ModernModulationPage::ModernModulationPage(Cs80AudioProcessor& p) : msegPage(p), lfoPanel(p), matrixPage(p)
{
    addAndMakeVisible(msegPage);
    addAndMakeVisible(lfoPanel);
    matrixViewport.setViewedComponent(&matrixPage, false);
    matrixViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(matrixViewport);
}

void ModernModulationPage::paint(juce::Graphics& g)
{
    g.fillAll(charcoal);
}

void ModernModulationPage::resized()
{
    auto area = getLocalBounds().reduced(4);
    msegPage.setBounds(area.removeFromTop(juce::jmax(220, (int) (area.getHeight() * 0.48f))));
    area.removeFromTop(4);
    lfoPanel.setBounds(area.removeFromLeft(juce::jmax(300, (int) (area.getWidth() * 0.34f))));
    area.removeFromLeft(4);
    matrixViewport.setBounds(area);
    const int scrollbar = matrixViewport.getScrollBarThickness();
    matrixPage.setSize(area.getWidth() - scrollbar, juce::jmax(area.getHeight(), 46 + kModSlots * 24));
}

} // namespace cs80::ui::modern_layer
