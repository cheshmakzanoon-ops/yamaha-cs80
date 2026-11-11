#pragma once

#include "plugin/ModernModulationUi.h"
#include "plugin/PluginProcessor.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace cs80::tests
{
namespace mseg_copy_detail
{
using namespace cs80::modern;
using cs80::ui::modern_layer::MsegPage;

inline juce::PopupMenu::Item itemFor(const juce::PopupMenu& menu, int destination)
{
    for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
        if (it.getItem().text == "Copy shape to MSEG " + juce::String(destination + 1)) return it.getItem();
    return {};
}

inline ModernConfig fixture(int variant = 0)
{
    ModernConfig config;
    for (int m = 0; m < kMsegCount; ++m)
    {
        auto& s = config.msegs[(std::size_t) m];
        s.count = variant == 2 ? kMsegMaxPoints : variant == 1 ? 2 : 6;
        for (int i = 0; i < s.count; ++i)
            s.points[(std::size_t) i] = { i * .125f, ((i + m) % 5) * .25f, ((i + m) % 3 - 1) * .75f };
        if (s.count > 2) s.points[2].time = s.points[1].time; // coincident step boundary
        s.loop = (MsegLoop) ((m + variant) % 3);
        s.loopStart = 1;
        s.loopEnd = variant == 0 ? 1 : s.count - 1; // singleton sustain and ordinary loops
        s.trigger = (MsegTrigger) ((m + variant) % 3);
        s.sync = ((m + variant) % 2) != 0;
        s.bipolar = ((m + variant) % 2) == 0;
        s = sanitizeShape(s);
    }
    config.slots[0] = {ModSource::mseg1, ModSource::modWheel, ModDestination::pitch, ModTarget::rankI};
    config.slots[7] = {ModSource::mseg3, ModSource::none, ModDestination::pan, ModTarget::rankII};
    return config;
}

inline juce::MouseEvent mouse(juce::Component& c, juce::Point<float> p)
{
    return {juce::Desktop::getInstance().getMainMouseSource(), p,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.f, 0.f, 0.f, 0.f, 0.f,
            &c, &c, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false};
}

struct Probe : juce::AudioProcessorParameter::Listener
{
    int changes = 0, gestures = 0;
    void parameterValueChanged(int, float) override { ++changes; }
    void parameterGestureChanged(int, bool) override { ++gestures; }
};
}

// Real component menu callbacks, deliberately not a claim of OS popup interaction.
template<class Check>
void testMsegShapeCopy(Check check)
{
    using namespace mseg_copy_detail;
    juce::TemporaryFile root("-cs80-mseg-copy");
    auto host = std::make_unique<Cs80AudioProcessor>(root.getFile());
    auto& layer = host->getModernLayer();
    auto& undo = host->getUndoManager();
    for (int i = 0; i < kMsegCount; ++i)
    {
        auto* p = host->getAPVTS().getParameter(Cs80AudioProcessor::parameterId(P_mseg1Rate + i));
        p->setValueNotifyingHost(p->convertTo0to1(float(i - 1)));
    }
    for (int i = 0; i < kModSlots; ++i)
    {
        auto* p = host->getAPVTS().getParameter(Cs80AudioProcessor::parameterId(P_mod1Amount + i));
        p->setValueNotifyingHost(p->convertTo0to1(float(i - 8) / 10.f));
    }
    host->getUndoableState().syncNow();
    std::vector<float> parameters;
    Probe probe;
    for (auto* p : host->getParameters()) { parameters.push_back(p->getValue()); p->addListener(&probe); }
    auto unchangedParameters = [&]
    {
        bool same = true;
        for (int i = 0; i < host->getParameters().size(); ++i)
            same &= host->getParameters()[i]->getValue() == parameters[(std::size_t) i];
        return same;
    };
    auto setup = [&](MsegPage& page, int source = 0, int variant = 0)
    {
        layer.setConfig(fixture(variant));
        page.selectMseg(source);
        page.setSize(1100, 300);
        undo.clearUndoHistory();
    };
    {
        MsegPage page(*host);
        // Six ordered pairs, all enum options, short/max shapes and coincident points.
        for (int variant = 0; variant < 3; ++variant)
            for (int source = 0; source < kMsegCount; ++source)
                for (int destination = 0; destination < kMsegCount; ++destination)
        {
            if (source == destination) continue;
            setup(page, source, variant);
            const auto before = layer.getConfig();
            auto expected = before;
            expected.msegs[(std::size_t) destination] = before.msegs[(std::size_t) source];
            const auto revision = layer.getRevision();
            const auto menu = page.buildShapeCopyMenu();
            const auto item = itemFor(menu, destination);
            check(item.isEnabled && bool(item.action) && !itemFor(menu, source).action,
                  "copy menu names each other slot and offers no self-copy");
            check(layer.getRevision() == revision && !undo.canUndo(), "building a copy menu does not write state or history");
            if (item.action) item.action();
            check(layer.getConfig() == expected && layer.getRevision() == revision + 1
                  && page.getEditor().getMsegIndex() == source,
                  "all six ordered slot pairs copy all shape fields and preserve source, third slot and routes");
            check(unchangedParameters() && probe.changes == 0 && probe.gestures == 0,
                  "shape copy preserves every APVTS parameter and emits no host parameter notifications");
            check(undo.getNumActionsInCurrentTransaction() == 0
                  && undo.getUndoDescription() == "Copy MSEG " + juce::String(source + 1)
                                               + " shape to MSEG " + juce::String(destination + 1),
                  "shape copy has one named, closed undo transaction");
            check(host->undo() && layer.getConfig() == before && !undo.canUndo(), "one undo restores the complete previous destination");
            check(host->redo() && layer.getConfig() == expected && !undo.canRedo(), "one redo restores the complete copied shape");
            auto changed = layer.getShape(destination); changed.points[0].level = .123f;
            layer.setShape(destination, changed);
            check(layer.getShape(source) == before.msegs[(std::size_t) source], "editing the destination cannot alias the source shape");
            changed = layer.getShape(source); changed.points[0].level = .987f;
            layer.setShape(source, changed);
            check(layer.getShape(destination).points[0].level == .123f, "editing the source cannot alias its earlier copy");
        }
        setup(page);
        layer.setShape(1, layer.getShape(0)); undo.clearUndoHistory();
        const auto duplicateBefore = layer.getConfig();
        const auto duplicateRevision = layer.getRevision();
        auto identical = itemFor(page.buildShapeCopyMenu(), 1);
        check(!identical.isEnabled, "identical destinations are visibly unavailable");
        if (identical.action) identical.action();
        check(layer.getConfig() == duplicateBefore && layer.getRevision() == duplicateRevision && !undo.canUndo(),
              "even direct invocation of an identical copy is a notification/history no-op");

        setup(page);
        auto copy = itemFor(page.buildShapeCopyMenu(), 1).action;
        if (copy) copy();
        const auto copied = layer.getConfig();
        auto later = layer.getShape(1); later.points[0].level = .375f;
        layer.setShape(1, later);
        check(host->undo() && layer.getConfig() == copied, "later direct shape editing cannot join the copy transaction");
        check(host->undo() && layer.getConfig() == fixture() && !undo.canUndo(), "copy remains its own undo step after a subsequent shape edit");

        setup(page, 2, 2);
        copy = itemFor(page.buildShapeCopyMenu(), 0).action;
        if (copy) copy();
        juce::MemoryBlock saved;
        host->getStateInformation(saved);
        {
            juce::TemporaryFile restoredRoot("-cs80-mseg-copy-restored");
            Cs80AudioProcessor restored(restoredRoot.getFile());
            restored.setStateInformation(saved.getData(), (int) saved.getSize());
            check(restored.getModernLayer().getConfig() == layer.getConfig(),
                  "a copied maximum-point shape survives existing session serialization unchanged");
        }

        // Staleness is checked synchronously, without waiting for ChangeBroadcaster delivery.
        for (int mode = 0; mode < 10; ++mode)
        {
            setup(page);
            auto pending = itemFor(page.buildShapeCopyMenu(), 1).action;
            if (mode < 2) { page.selectMseg(2); if (mode == 1) page.selectMseg(0); }
            else if (mode < 8)
            {
                auto changed = layer.getConfig();
                if (mode < 4) changed.msegs[0].points[0].level = .123f;
                else if (mode < 6) changed.msegs[1].points[0].level = .123f;
                else changed.slots[0].source = ModSource::ribbon;
                layer.setConfig(changed);
                if ((mode % 2) != 0) layer.setConfig(fixture());
            }
            else if (mode == 8) { page.setEnabled(false); page.setEnabled(true); }
            else page.buildShapeCopyMenu(); // superseding context
            const auto before = layer.getConfig(); const auto revision = layer.getRevision();
            if (pending) pending();
            check(layer.getConfig() == before && layer.getRevision() == revision,
                  "slot, shape, routing, disablement or new menu invalidates delayed copy including A-B-A");
        }
        setup(page);
        auto once = itemFor(page.buildShapeCopyMenu(), 1).action;
        if (once) once();
        check(host->undo(), "copied shape can be undone before a duplicate callback");
        if (once) once();
        check(layer.getConfig() == fixture() && !undo.canUndo(), "a consumed callback never copies again after undo");

        for (int disabled = 0; disabled < 4; ++disabled)
        {
            setup(page);
            juce::Component parent;
            parent.addAndMakeVisible(page);
            auto* copyButton = [&]() -> juce::Button* {
                for (auto* child : page.getChildren())
                    if (child->getTitle() == "Copy MSEG shape to another slot") return dynamic_cast<juce::Button*>(child);
                return nullptr;
            }();
            check(copyButton != nullptr, "shape copy has a discoverable, named button");
            auto setEnabled = [&](bool enabled)
            {
                if (disabled == 0) page.setEnabled(enabled);
                else if (disabled == 1) parent.setEnabled(enabled);
                else if (disabled == 2) page.getEditor().setEnabled(enabled);
                else if (copyButton) copyButton->setEnabled(enabled);
            };
            auto pending = itemFor(page.buildShapeCopyMenu(), 1).action;
            setEnabled(false);
            if (pending) pending();
            const auto denied = itemFor(page.buildShapeCopyMenu(), 1);
            check(!denied.isEnabled && layer.getConfig() == fixture() && !undo.canUndo(),
                  "disabled page, ancestor, editor or copy button rejects an already pending copy");
            setEnabled(true);
            if (denied.action) denied.action();
            check(layer.getConfig() == fixture() && !undo.canUndo(), "a menu created disabled never becomes live after enabling");
            parent.removeChildComponent(&page);
        }
        setup(page);
        auto pendingPure = itemFor(page.buildShapeCopyMenu(), 1).action;
        host->setPure(true);
        if (pendingPure) pendingPure();
        const auto pureItem = itemFor(page.buildShapeCopyMenu(), 1);
        check(!pureItem.isEnabled && layer.getConfig() == fixture(), "PURE rejects pending copies and disables menu entries");
        host->setPure(false);
        if (pureItem.action) pureItem.action();
        check(layer.getConfig() == fixture(), "PURE-created context cannot become live after leaving PURE");

        // All editor drag kinds plus the attached rate slider. A drag owns its transaction.
        for (int kind = 0; kind < 4; ++kind)
        {
            setup(page, 0, 1);
            auto s = fixture().msegs[0]; s.loop = MsegLoop::always; s.loopEnd = s.count - 1;
            layer.setShape(0, s); page.selectMseg(0); undo.clearUndoHistory();
            auto& editor = page.getEditor();
            const auto start = kind == 0 ? editor.pointPosition(1)
                : kind == 1 ? editor.toScreen(.4, .1) : editor.loopMarkerArea(kind == 3).getCentre();
            const auto before = layer.getConfig();
            auto pending = itemFor(page.buildShapeCopyMenu(), 1).action;
            editor.mouseDown(mouse(editor, start));
            if (pending) pending();
            const auto denied = itemFor(page.buildShapeCopyMenu(), 1);
            check(!denied.isEnabled && layer.getConfig() == before, "point, curve and loop drags reject pending and newly built copy menus");
            editor.mouseUp(mouse(editor, start));
            if (pending) pending();
            if (denied.action) denied.action();
            check(layer.getConfig() == before && !undo.canUndo(), "drag completion cannot revive old or drag-time copy callbacks");
            const auto p = editor.pointPosition(1), moved = p + juce::Point<float>(0.f, -10.f);
            editor.mouseDown(mouse(editor, p)); editor.mouseDrag(mouse(editor, moved)); editor.mouseUp(mouse(editor, moved));
            check(layer.getShape(0) != before.msegs[0] && layer.getShape(1) == before.msegs[1],
                  "fresh ordinary point editing still works after rejected copy");
        }
        setup(page);
        for (auto* child : page.getChildren())
            if (auto* rate = dynamic_cast<juce::Slider*>(child))
        {
            auto pending = itemFor(page.buildShapeCopyMenu(), 1).action;
            juce::PopupMenu::Item denied;
            {
                juce::Slider::ScopedDragNotification gesture(*rate);
                if (pending) pending();
                denied = itemFor(page.buildShapeCopyMenu(), 1);
                check(!denied.isEnabled && layer.getConfig() == fixture(), "rate gesture prevents shape copy from splitting its undo transaction");
            }
            if (pending) pending();
            if (denied.action) denied.action();
            check(layer.getConfig() == fixture(), "completed rate gesture cannot revive stale copy callbacks");
        }

        setup(page);
        for (auto* child : page.getChildren())
            if (auto* presets = dynamic_cast<juce::ComboBox*>(child); presets && presets->getTitle() == "MSEG shape preset")
                for (int preset = 1; preset <= 7; ++preset)
        {
            const auto before = layer.getShape(0);
            const auto neighbors = layer.getConfig();
            presets->setSelectedId(preset, juce::sendNotificationSync);
            const auto after = layer.getShape(0);
            check(after.count >= 2 && after.sync == before.sync && after.trigger == before.trigger
                  && presets->getSelectedId() == 0 && layer.getShape(1) == neighbors.msegs[1]
                  && layer.getShape(2) == neighbors.msegs[2], "all seven factory presets retain their original local-slot behavior");
        }
    }
    // Page destruction, not just an index change, invalidates captured native actions.
    std::function<void()> orphan;
    {
        MsegPage temporary(*host); setup(temporary);
        orphan = itemFor(temporary.buildShapeCopyMenu(), 1).action;
    }
    const auto beforeDestruction = layer.getConfig();
    if (orphan) orphan();
    check(layer.getConfig() == beforeDestruction && !undo.canUndo(), "destroyed page callback is a safe no-op");
    for (auto* p : host->getParameters()) p->removeListener(&probe);
}
}
