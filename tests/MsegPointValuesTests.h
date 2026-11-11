#pragma once
#include "MsegShapeCopyTests.h"
#include "plugin/MsegPointValues.h"
#include <limits>
#include <bit>
#include <cstdint>

namespace cs80::tests
{
namespace mseg_values_detail
{
struct CommaPunctuation : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
template<class T> T* child(juce::Component& parent, const juce::String& title)
{
    for (auto* c : parent.getChildren())
        if (c->getTitle() == title) return dynamic_cast<T*>(c);
    return nullptr;
}
struct Form
{
    juce::Component* panel;
    juce::ComboBox* point;
    juce::TextEditor *time, *level, *curve;
    juce::Button *apply, *cancel;
    explicit Form(cs80::ui::modern_layer::MsegPage& page) : panel(page.getPointValuesEditor()),
        point(child<juce::ComboBox>(*panel, "MSEG point number")),
        time(child<juce::TextEditor>(*panel, "Point time")),
        level(child<juce::TextEditor>(*panel, "Point normalized level")),
        curve(child<juce::TextEditor>(*panel, "Point outgoing curve")),
        apply(child<juce::Button>(*panel, "Apply MSEG point values")),
        cancel(child<juce::Button>(*panel, "Cancel MSEG point values")) {}
    void choose(int index) { point->setSelectedId(index + 1, juce::sendNotificationSync); }
};
}

// Direct real component callbacks without a native peer or OS focus simulation.
template<class Check> void testMsegPointValues(Check check)
{
    using namespace mseg_values_detail;
    using namespace mseg_copy_detail;
    using namespace cs80::ui::modern_layer;
    for (const auto* text : { "", " ", "nan", "NaN", "inf", "-inf", "1e99999", "1e-99999", "1x", "1 2", "0,5", "0x1", "--1", "+-1", "1e", ".", "1.2.3" })
        check(!parseMsegPointValue(text), "invalid, nonfinite, overflow, underflow and trailing junk never become zero");
    for (const auto* text : { "0", " -1 ", "+.5", "1.", "2.5e-1", "-0" })
        check(bool(parseMsegPointValue(text)), "finite decimal syntax supports signs, whitespace and exponents");
    for (float v : { 0.f, -1.f, 1.f, .1f, .33333334f, 64.f, .00000001f,
                     std::numeric_limits<float>::denorm_min(), std::nextafter(1.f, 0.f) })
        check(parseMsegPointValue(formatMsegPointValue(v)) == v, "displayed decimal round trips every Float32 coordinate exactly");

    {
        const auto original = std::locale();
        std::locale::global(std::locale(original, new CommaPunctuation));
        check(parseMsegPointValue("0.125") == .125f && !parseMsegPointValue("0,125")
              && formatMsegPointValue(.125f) == "0.125", "parser and display ignore an ambient comma-decimal C++ locale");
        std::locale::global(original);
    }
    for (float value : {0.f, -0.f, std::numeric_limits<float>::denorm_min(),
                       std::nextafter(std::numeric_limits<float>::min(), 0.f), std::numeric_limits<float>::min(),
                       std::nextafter(std::numeric_limits<float>::min(), 1.f)})
    {
        const auto parsed = parseMsegPointValue(formatMsegPointValue(value));
        check(parsed && std::bit_cast<std::uint32_t>(*parsed) == std::bit_cast<std::uint32_t>(value),
              "signed zero and subnormal/normal boundary decimals round trip bit-exactly");
    }

    {
        const int original = std::fegetround();
        struct Decimal { const char* text; float expected; };
        for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO})
        {
            check(std::fesetround(mode) == 0, "test selects caller rounding mode");
            for (const auto& example : {
                Decimal{"0.0000000000000000000000000000000000000001", 1.e-40f},
                Decimal{"0.500000029802322387695312499999999999999", .5f},
                Decimal{"0.5000000298023223876953125", .5f},
                Decimal{"0.500000029802322387695312500000000000001", 0x1.000002p-1f},
                Decimal{"-0.500000029802322387695312500000000000001", -0x1.000002p-1f},
                Decimal{"-0", -0.f}, Decimal{"1e-00000000040", 1.e-40f}})
            {
                const auto parsed = parseMsegPointValue(example.text);
                check(parsed && std::bit_cast<std::uint32_t>(*parsed) == std::bit_cast<std::uint32_t>(example.expected),
                      "long fixed fractions and binary32 halfway neighbors convert directly with nearest rounding");
                check(std::fegetround() == mode, "successful parsing restores caller rounding mode");
            }
            check(!parseMsegPointValue("1e99999") && !parseMsegPointValue("1e-99999") && std::fegetround() == mode,
                  "failed parsing also restores caller rounding mode");
            check(parseMsegPointValue(formatMsegPointValue(std::numeric_limits<float>::denorm_min()))
                  == std::numeric_limits<float>::denorm_min(), "formatter round trips subnormals under all caller rounding modes");
        }
        std::fesetround(original);
    }

    juce::TemporaryFile root("-cs80-mseg-values");
    auto host = std::make_unique<Cs80AudioProcessor>(root.getFile());
    auto& layer = host->getModernLayer(); auto& undo = host->getUndoManager();
    host->getUndoableState().syncNow();
    Probe probe;
    std::vector<float> parameters;
    for (auto* p : host->getParameters()) { parameters.push_back(p->getValue()); p->addListener(&probe); }
    auto unchangedParameters = [&]
    {
        bool same = true;
        for (int i = 0; i < host->getParameters().size(); ++i) same &= parameters[(std::size_t) i] == host->getParameters()[i]->getValue();
        return same && probe.changes == 0 && probe.gestures == 0;
    };
    MsegPage page(*host);
    auto setup = [&](int variant = 0, int slot = 0)
    {
        layer.setConfig(fixture(variant)); page.selectMseg(slot); page.setSize(1100, 300);
        undo.clearUndoHistory(); page.showPointValues();
    };
    for (int variant = 0; variant < 3; ++variant)
        for (int slot = 0; slot < kMsegCount; ++slot)
    {
        const int count = fixture(variant).msegs[(std::size_t) slot].count;
        for (int index = 0; index < count; ++index)
        {
            setup(variant, slot); Form f(page); f.choose(index);
            const auto before = layer.getConfig(); auto expected = before;
            auto& p = expected.msegs[(std::size_t) slot].points[(std::size_t) index];
            check(f.time->isReadOnly() == (index == 0) && f.curve->isReadOnly() == (index == count - 1),
                  "first time and last outgoing curve are read-only only at those endpoints");
            const auto revision = layer.getRevision();
            check(!undo.canUndo(), "opening and selecting points never adds history");
            p.level = .375f; f.level->setText("0.375", false);
            if (index > 0) { p.time = index == count - 1 ? 64.f : before.msegs[(std::size_t) slot].points[(std::size_t) index + 1].time; f.time->setText(formatMsegPointValue(p.time), false); }
            if (index < count - 1) { p.curve = -.25f; f.curve->setText("-0.25", false); }
            auto once = f.apply->onClick; once();
            check(layer.getConfig() == expected && layer.getRevision() == revision + 1 && !f.panel->isVisible(),
                  "all slots/point positions accept exact time, normalized level and outgoing curve in one edit");
            check(unchangedParameters(), "point editing preserves all host parameters/rates and emits no host notifications");
            check(undo.getUndoDescription() == "Edit MSEG " + juce::String(slot + 1) + " point " + juce::String(index + 1)
                  && undo.getNumActionsInCurrentTransaction() == 0, "precision edit is named and closes its transaction");
            check(host->undo() && layer.getConfig() == before && !undo.canUndo(), "one undo restores the whole shape");
            once(); check(layer.getConfig() == before && !undo.canUndo(), "accepted callback is single-use even after undo");
            check(host->redo() && layer.getConfig() == expected && !undo.canRedo(), "one redo restores the exact precision edit");
        }
    }
    setup();
    { Form f(page); const auto before = layer.getConfig(); const auto revision = layer.getRevision();
      f.apply->onClick(); check(layer.getConfig() == before && layer.getRevision() == revision && !undo.canUndo(), "unchanged round-trip text is a complete no-op"); }
    setup();
    {
        Form old(page); old.cancel->onClick(); auto& editor = page.getEditor();
        const auto event = mouse(editor, editor.pointPosition(3)); editor.mouseDown(event); editor.mouseUp(event);
        page.showPointValues(); Form f(page);
        check(f.point->getSelectedId() == 4, "Point values starts at the selected graph point");
        f.choose(4); f.level->setText(".625", false); f.level->onReturnKey();
        check(layer.getShape(0).points[4].level == .625f && !f.panel->isVisible(), "Return in a field applies the selected point atomically");
        page.showPointValues(); Form again(page);
        check(again.point->getSelectedId() == 5, "applied point stays selected when reopened");
        again.level->setText(".4", false); const auto before = layer.getConfig();
        again.level->onEscapeKey(); check(layer.getConfig() == before && !again.panel->isVisible(), "Escape in a field discards the pending edit");
    }
    for (float value : {.123456789f, std::numeric_limits<float>::denorm_min(), std::nextafter(1.f, 0.f)})
    {
        setup(); auto exact = layer.getShape(0); exact.points[1].level = value; exact.points[1].curve = -value;
        layer.setShape(0, exact); undo.clearUndoHistory(); page.showPointValues(); Form f(page); f.choose(1);
        const auto revision = layer.getRevision(); f.apply->onClick();
        check(layer.getShape(0) == exact && layer.getRevision() == revision && !undo.canUndo(), "unmodified nontrivial and subnormal fields stay bit-exact without history");
    }
    for (float value : {0.f, -0.f, std::numeric_limits<float>::denorm_min(),
                       std::nextafter(std::numeric_limits<float>::min(), 0.f), std::numeric_limits<float>::min(),
                       std::nextafter(std::numeric_limits<float>::min(), 1.f)})
        for (int index : {0, 1})
    {
        MsegShape exact; exact.count = 2;
        exact.points[0] = {0.f, value, -value}; exact.points[1] = {value, value, -value};
        layer.setShape(0, exact); page.selectMseg(0); undo.clearUndoHistory(); page.showPointValues(); Form f(page); f.choose(index);
        const auto before = layer.getConfig(); const auto revision = layer.getRevision(); f.apply->onClick();
        check(layer.getConfig() == before && layer.getRevision() == revision && !undo.canUndo() && !f.panel->isVisible(),
              "every field preserves subnormal/normal boundary values and signed zero on Apply without history");
    }
    setup();
    { Form f(page); const auto before = layer.getConfig(); f.level->setText(".9", false); auto late = f.apply->onClick;
      f.cancel->onClick(); late(); check(layer.getConfig() == before && !undo.canUndo() && !f.panel->isVisible(), "cancel and a delayed Apply cannot write"); }
    setup();
    { Form f(page); f.choose(1); const auto before = layer.getConfig(); const auto revision = layer.getRevision();
      for (const auto* bad : { "-0.01", "0.126", "65", "nan", "0.1junk" })
      { f.time->setText(bad, false); f.apply->onClick(); check(layer.getConfig() == before && layer.getRevision() == revision && !undo.canUndo() && f.panel->isVisible(), "invalid neighbor time is rejected, never clamped or reordered"); }
      f.time->setText(".125", false);
      for (const auto* bad : { "-0.1", "1.01", "inf", "junk" })
      { f.level->setText(bad, false); f.apply->onClick(); check(layer.getConfig() == before && !undo.canUndo(), "invalid normalized/bipolar level is rejected"); }
      f.level->setText(".5", false);
      for (const auto* bad : { "-1.1", "1.1", "nan", "junk" })
      { f.curve->setText(bad, false); f.apply->onClick(); check(layer.getConfig() == before && !undo.canUndo(), "invalid outgoing curve is rejected"); }
      f.curve->setText("1", false); f.apply->onClick();
      check(layer.getShape(0).points[1] == MsegPoint{.125f, .5f, 1.f}, "correcting invalid input can apply without reopening"); }
    for (float level : {0.f, 1.f}) for (float curve : {-1.f, 0.f, 1.f})
    {
        setup(); Form f(page); f.choose(1); f.time->setText("0", false);
        f.level->setText(formatMsegPointValue(level), false); f.curve->setText(formatMsegPointValue(curve), false);
        f.apply->onClick(); check(layer.getShape(0).points[1] == MsegPoint{0.f, level, curve}, "time lower bound, level extremes and all curve signs are accepted");
    }
    setup();
    {
        Form f(page); f.choose(3); f.level->setText(".123456789", false); f.apply->onClick();
        const auto edited = layer.getConfig();
        auto later = layer.getShape(0); later.points[0].level = .375f; layer.setShape(0, later);
        check(host->undo() && layer.getConfig() == edited, "later direct editing cannot join the closed precision transaction");
        check(host->undo() && layer.getConfig() == fixture() && !undo.canUndo(), "precision edit remains its own step after a later edit");
        host->redo(); juce::MemoryBlock saved; host->getStateInformation(saved);
        juce::TemporaryFile restoredRoot("-cs80-values-restored"); Cs80AudioProcessor restored(restoredRoot.getFile());
        restored.setStateInformation(saved.getData(), (int) saved.getSize());
        check(restored.getModernLayer().getConfig() == edited, "precisely edited shape survives session state serialization unchanged");
    }
    for (int endpoint : { 0, 5 })
    { setup(); Form f(page); f.choose(endpoint); const auto before = layer.getConfig();
      if (endpoint == 0) f.time->setText(".01", false); else f.curve->setText(".123", false);
      f.apply->onClick(); check(layer.getConfig() == before && !undo.canUndo(), "even direct mutation cannot change locked endpoint fields"); }
    for (int mode = 0; mode < 13; ++mode)
    {
        setup(); Form f(page); f.level->setText(".9", false); auto pending = f.apply->onClick;
        juce::Component parent; parent.addAndMakeVisible(page);
        if (mode < 2) { page.selectMseg(1); if (mode == 1) page.selectMseg(0); }
        else if (mode < 6)
        {
            auto changed = layer.getConfig();
            if (mode == 2 || mode == 3) changed.msegs[0].points[0].level = .1f;
            else if (mode == 4) changed.msegs[0].sync = !changed.msegs[0].sync;
            else changed.slots[0].source = ModSource::ribbon;
            layer.setConfig(changed); if (mode == 3) host->undo();
        }
        else if (mode == 6) { page.setEnabled(false); page.setEnabled(true); }
        else if (mode == 7) { parent.setEnabled(false); parent.setEnabled(true); }
        else if (mode == 8) page.showPointValues();
        else if (mode == 9) { auto& e = page.getEditor(); auto event = mouse(e, e.pointPosition(1)); e.mouseDown(event); e.mouseUp(event); }
        else if (mode == 10) host->setPure(true);
        else if (mode == 11) page.getEditor().setEnabled(false);
        else { auto* rate = child<juce::Slider>(page, "MSEG rate"); juce::Slider::ScopedDragNotification gesture(*rate); }
        const auto before = layer.getConfig(); const auto revision = layer.getRevision();
        pending(); check(layer.getConfig() == before && layer.getRevision() == revision, "rebind, ABA, state/unit/routing changes, disablement, supersession, gesture and PURE reject old fields");
        // A stale form can always be dismissed, even after rate-generation changes.
        Form current(page); current.cancel->onClick(); check(!current.panel->isVisible(), "Cancel closes stale as well as current point editor");
        page.getEditor().setEnabled(true); host->setPure(false); parent.removeChildComponent(&page);
    }
    for (int kind = 0; kind < 4; ++kind)
    {
        setup(); Form f(page); f.cancel->onClick();
        auto s = layer.getShape(0); s.loop = MsegLoop::always; s.loopEnd = s.count - 1; layer.setShape(0, s); page.selectMseg(0); undo.clearUndoHistory();
        auto& e = page.getEditor(); const auto p = kind == 0 ? e.pointPosition(1) : kind == 1 ? e.toScreen(.4, .1) : e.loopMarkerArea(kind == 3).getCentre();
        e.mouseDown(mouse(e, p)); page.showPointValues();
        check(!page.getPointValuesEditor()->isVisible(), "active point, curve and loop drags cannot open a precision edit");
        e.mouseUp(mouse(e, p));
        const auto before = layer.getConfig(); const auto start = e.pointPosition(1), moved = start + juce::Point<float>(0, -10);
        e.mouseDown(mouse(e, start)); e.mouseDrag(mouse(e, moved)); e.mouseUp(mouse(e, moved));
        check(layer.getConfig() != before, "ordinary point dragging still edits after canceled/denied precision forms");
    }
    setup();
    { Form f(page); f.cancel->onClick(); auto* rate = child<juce::Slider>(page, "MSEG rate");
      { juce::Slider::ScopedDragNotification gesture(*rate); page.showPointValues(); check(!page.getPointValuesEditor()->isVisible(), "rate gesture cannot open precision editor"); } }
    std::function<void()> orphan;
    { MsegPage temporary(*host); temporary.setSize(1100,300); temporary.showPointValues(); Form f(temporary); f.level->setText(".4", false); orphan = f.apply->onClick; }
    const auto beforeOrphan = layer.getConfig(); orphan(); check(layer.getConfig() == beforeOrphan, "destroyed page and form callbacks safely do nothing");
    for (auto* p : host->getParameters()) p->removeListener(&probe);

    // Render both supported widths offscreen, inspect all new control bounds.
    ModernModulationPage rendered(*host);
    auto& renderedPage = rendered.getMsegPage();
    for (int width : {1100, 1560})
    {
        layer.setConfig(fixture()); rendered.setSize(width, width == 1100 ? 640 : 820);
        renderedPage.selectMseg(0); renderedPage.showPointValues(); Form f(renderedPage);
        check(renderedPage.getLocalBounds().contains(f.panel->getBounds()), "point overlay fits both supported native page layouts");
        for (auto* c : f.panel->getChildren()) check(f.panel->getLocalBounds().contains(c->getBounds()) && !c->getBounds().isEmpty(), "all precision controls have visible in-panel bounds");
        auto* button = child<juce::Button>(renderedPage, "Edit MSEG point values");
        check(button && renderedPage.getLocalBounds().contains(button->getBounds()), "discoverable Point values button fits both layouts");
        for (auto* c : renderedPage.getChildren()) if (c != button && c != f.panel && c != &renderedPage.getEditor())
            check(!button->getBounds().intersects(c->getBounds()), "new button does not overlap existing selectors, presets or shape copy");
        const auto image = rendered.createComponentSnapshot(rendered.getLocalBounds(), true, 1.f);
        check(image.isValid(), "real point editor renders offscreen without a desktop peer");
        const auto directory = juce::SystemStats::getEnvironmentVariable("CS80_MSEG_VALUES_SCREENSHOTS", {});
        if (directory.isNotEmpty())
        {
            const juce::File dir(directory); dir.createDirectory();
            auto stream = dir.getChildFile("mseg-point-values-" + juce::String(width) + ".png").createOutputStream();
            check(stream != nullptr && stream->setPosition(0) && stream->truncate().wasOk()
                  && juce::PNGImageFormat().writeImageToStream(image, *stream), "offscreen precision editor evidence is saved");
        }
    }
}
}
