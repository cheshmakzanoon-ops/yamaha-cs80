#pragma once

#include "plugin/ModernModulationUi.h"
#include "plugin/PluginProcessor.h"
#include <array>
#include <memory>
#include <vector>

namespace cs80::tests
{
// Real ComboBox callbacks with a pending ModernLayerState change message, without
// a native window. No event-loop pump occurs between publishing and choosing.
template<class Check>
void testModMatrixRoutingUi(Check check)
{
    using namespace cs80::modern;
    using cs80::ui::modern_layer::ModMatrixPage;
    juce::TemporaryFile fixture("-cs80-matrix-routing");
    auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    auto& layer = host->getModernLayer();
    auto& undo = host->getUndoManager();
    ModMatrixPage page(*host);
    page.setSize(1100, 510);
    const ModSlot displayed {ModSource::lfo1, ModSource::modWheel, ModDestination::lpCutoff, ModTarget::rankI};
    const ModSlot live {ModSource::mseg3, ModSource::expression, ModDestination::pitch, ModTarget::rankII};
    const ModSlot chosen {ModSource::ribbon, ModSource::velocity, ModDestination::hpCutoff, ModTarget::both};
    auto boxes = [&](int slot)
    {
        return std::array<juce::ComboBox*, 4>{&page.sourceBox(slot), &page.viaBox(slot),
                                            &page.destinationBox(slot), &page.targetBox(slot)};
    };
    auto selection = [](const ModSlot& slot, int field)
    {
        switch (field)
        {
            case 0: return int(slot.source) + 1;
            case 1: return int(slot.via) + 1;
            case 2: return int(slot.destination) + 1;
            default: return int(slot.target) + 1;
        }
    };
    auto replacing = [](ModSlot slot, const ModSlot& replacement, int field)
    {
        switch (field)
        {
            case 0: slot.source = replacement.source; break;
            case 1: slot.via = replacement.via; break;
            case 2: slot.destination = replacement.destination; break;
            default: slot.target = replacement.target; break;
        }
        return slot;
    };
    // Non-neutral sentinels make amount/rate preservation observable even if a
    // future routing implementation accidentally resets a host parameter.
    for (int i = 0; i < kModSlots; ++i)
        host->setParamValue(P_mod1Amount + i, (i % 2 == 0 ? 1.f : -1.f) * .05f * float(i + 1));
    const float rates[] = {-1.5f, .5f, 1.5f};
    for (int i = 0; i < kMsegCount; ++i) host->setParamValue(P_mseg1Rate + i, rates[i]);
    host->getUndoableState().syncNow();
    bool seeded = true;
    for (int i = 0; i < kModSlots; ++i) seeded &= host->getParamValue(P_mod1Amount + i) != 0.f;
    for (int i = 0; i < kMsegCount; ++i) seeded &= host->getParamValue(P_mseg1Rate + i) != 0.f;
    check(seeded, "preservation fixture uses nonzero signed amounts and nonzero MSEG rates");
    struct Probe : juce::AudioProcessorParameter::Listener
    {
        int values = 0, gestures = 0;
        void parameterValueChanged(int, float) override { ++values; }
        void parameterGestureChanged(int, bool) override { ++gestures; }
    } probe;
    std::vector<float> parameters;
    for (auto* parameter : host->getParameters())
    {
        parameters.push_back(parameter->getValue());
        parameter->addListener(&probe);
    }
    auto unchangedParameters = [&]
    {
        for (int i = 0; i < host->getParameters().size(); ++i)
            if (host->getParameters()[i]->getValue() != parameters[std::size_t(i)]) return false;
        return true;
    };
    for (int slot = 0; slot < kModSlots; ++slot)
        for (int field = 0; field < 4; ++field)
    {
        ModernConfig initial;
        initial.slots.fill(displayed);
        layer.setConfig(initial);
        layer.sendSynchronousChangeMessage();
        auto latest = initial;
        latest.slots[std::size_t(slot)] = live;
        latest.msegs[1].points[0].level = .37f;
        latest.slots[std::size_t((slot + 1) % kModSlots)] = chosen;
        layer.setConfig(latest); // leave the page's asynchronous refresh pending
        undo.clearUndoHistory();
        const auto revision = layer.getRevision();
        check(boxes(slot)[0]->getSelectedId() == selection(displayed, 0), "fixture has an unrefreshed row after publication");
        auto expected = latest;
        expected.slots[std::size_t(slot)] = replacing(live, chosen, field);
        boxes(slot)[std::size_t(field)]->setSelectedId(selection(chosen, field), juce::sendNotificationSync);
        check(layer.getConfig() == expected && layer.getRevision() == revision + 1,
              "one routing choice preserves live sibling fields, other slots and MSEG shapes");
        bool refreshed = true;
        for (int f = 0; f < 4; ++f)
            refreshed &= boxes(slot)[std::size_t(f)]->getSelectedId() == selection(expected.slots[std::size_t(slot)], f);
        check(refreshed, "edited row immediately shows the merged live route");
        check(unchangedParameters() && probe.values == 0 && probe.gestures == 0,
              "routing choices preserve all host parameters and emit no host parameter changes or gestures");
        const char* names[] = {"source", "via", "destination", "rank"};
        check(undo.getUndoDescription() == "Matrix slot " + juce::String(slot + 1) + " " + names[field]
                  && undo.getNumActionsInCurrentTransaction() == 0,
              "each field edit owns one named, closed undo transaction");
        check(host->undo() && layer.getConfig() == latest && !undo.canUndo(), "one undo restores the latest route, not stale widget fields");
        check(host->redo() && layer.getConfig() == expected && !undo.canRedo(), "one redo restores the selected field only");
    }

    // Consecutive choices are separate reversible actions even in the same turn.
    ModernConfig initial;
    initial.slots[0] = displayed;
    layer.setConfig(initial);
    layer.sendSynchronousChangeMessage();
    undo.clearUndoHistory();
    page.sourceBox(0).setSelectedId(selection(chosen, 0), juce::sendNotificationSync);
    const auto first = layer.getConfig();
    page.viaBox(0).setSelectedId(selection(chosen, 1), juce::sendNotificationSync);
    const auto second = layer.getConfig();
    check(host->undo() && layer.getConfig() == first && undo.canUndo(), "first undo reverts only the second routing choice");
    check(host->undo() && layer.getConfig() == initial && !undo.canUndo(), "second undo reverts only the first routing choice");
    check(host->redo() && layer.getConfig() == first, "first redo restores only the first routing choice");
    check(host->redo() && layer.getConfig() == second && !undo.canRedo(), "second redo restores only the second routing choice");

    // Real host restoration, rather than only direct side-state publication.
    for (int field = 0; field < 4; ++field)
    {
        ModernConfig restored;
        restored.slots.fill(live);
        restored.msegs[2].points[0].level = .25f;
        layer.setConfig(restored);
        juce::MemoryBlock saved;
        host->getStateInformation(saved);
        layer.setConfig(initial);
        layer.sendSynchronousChangeMessage();
        host->setStateInformation(saved.getData(), int(saved.getSize()));
        check(layer.getConfig() == restored && page.sourceBox(0).getSelectedId() == selection(displayed, 0),
              "host restore publishes the new route before the page has refreshed");
        probe.values = probe.gestures = 0;
        auto expected = restored;
        expected.slots[0] = replacing(live, chosen, field);
        boxes(0)[std::size_t(field)]->setSelectedId(selection(chosen, field), juce::sendNotificationSync);
        check(layer.getConfig() == expected && unchangedParameters() && probe.values == 0 && probe.gestures == 0,
              "an immediate choice after host restore preserves the other restored fields and all host parameters");
        check(host->undo() && layer.getConfig() == restored && !undo.canUndo(),
              "undo after host restore returns to the restored sound in one step");
        check(host->redo() && layer.getConfig() == expected, "redo after host restore returns only the selected field");
    }

    // Undo publishes side-state synchronously but refreshes the page later.
    initial.slots[0] = live;
    layer.setConfig(initial);
    layer.sendSynchronousChangeMessage();
    undo.clearUndoHistory();
    page.destinationBox(0).setSelectedId(selection(chosen, 2), juce::sendNotificationSync);
    const auto destinationEdited = layer.getConfig();
    check(host->undo() && layer.getConfig() == initial
              && page.destinationBox(0).getSelectedId() == selection(chosen, 2),
          "fixture leaves the destination widget stale after undo");
    const auto noOpRevision = layer.getRevision();
    page.destinationBox(0).setSelectedId(selection(live, 2), juce::sendNotificationSync);
    check(layer.getConfig() == initial && layer.getRevision() == noOpRevision
              && !undo.canUndo() && undo.canRedo(),
          "choosing the already-live value refreshes the row without changing state or dropping redo");
    check(host->redo() && layer.getConfig() == destinationEdited, "no-op selection leaves the original action redoable");
    check(host->undo(), "the destination edit can be undone again before a new source choice");
    auto sourceEdited = initial;
    sourceEdited.slots[0].source = chosen.source;
    page.sourceBox(0).setSelectedId(selection(chosen, 0), juce::sendNotificationSync);
    check(layer.getConfig() == sourceEdited && !undo.canRedo(),
          "a new source choice after undo preserves the live destination and replaces only the redo branch");
    check(host->undo() && layer.getConfig() == initial && !undo.canUndo(),
          "undo of the new branch restores the live route without resurrecting the old destination");
    check(host->redo() && layer.getConfig() == sourceEdited, "redo of the new branch restores only the new source");

    // A global destination disables only rank editing, and changing it back
    // keeps the latent rank selection. Unrouted rows retain their amount.
    for (auto destination : {ModDestination::none, ModDestination::fx1Mix, ModDestination::mseg2Rate, ModDestination::pan})
    {
        page.destinationBox(0).setSelectedId(int(destination) + 1, juce::sendNotificationSync);
        check(layer.getSlot(0).target == live.target
                  && page.targetBox(0).isEnabled() == isRankDestination(destination)
                  && unchangedParameters(),
              "destination choices preserve rank and amounts while refreshing rank availability");
    }
    layer.sendSynchronousChangeMessage();
    const auto refreshRevision = layer.getRevision();
    const auto description = undo.getUndoDescription();
    layer.sendSynchronousChangeMessage();
    check(layer.getRevision() == refreshRevision && undo.getUndoDescription() == description,
          "repeated state refresh writes neither routing nor undo history");
    for (auto* parameter : host->getParameters()) parameter->removeListener(&probe);
}
}
