#pragma once

// Runs only inside the existing initialized UI smoke host. This exercises the
// real APVTS/modern tree actions alongside A/B side state; no new test gate.
#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace cs80::tests
{
template <typename Check>
void testAbCompareHistoryHost(Check&& check)
{
    struct Sound
    {
        cs80::ParamValues values;
        cs80::modern::ModernConfig modern;
        std::array<int, 2> sources {};
        std::array<std::array<float, cs80::CP_count>, 4> memories {};
        Cs80AudioProcessor::PresetMetadata metadata;
    };
    const auto capture = [](Cs80AudioProcessor& host)
    {
        Sound sound;
        sound.values = host.currentValues();
        sound.modern = host.getModernLayer().getConfig();
        sound.metadata = host.getPresetMetadata();
        for (int channel = 0; channel < 2; ++channel)
            sound.sources[(size_t) channel] = host.getHardwareSource(channel);
        for (int bank = 0; bank < 4; ++bank)
            for (int local = 0; local < cs80::CP_count; ++local)
                if (Cs80AudioProcessor::isMemoryControl(local))
                    sound.memories[(size_t) bank][(size_t) local] = host.getMemoryValue(bank, local);
        return sound;
    };
    const auto matches = [&](Cs80AudioProcessor& host, const Sound& expected)
    {
        const auto actual = capture(host);
        if (actual.modern != expected.modern || actual.sources != expected.sources
            || actual.metadata.name != expected.metadata.name
            || actual.metadata.factoryIndex != expected.metadata.factoryIndex)
            return false;
        for (int i = 0; i < cs80::P_numParams; ++i)
        {
            const auto& definition = cs80::paramDef(i);
            if (std::abs(actual.values[i] - expected.values[i])
                > std::max(1.0e-4f, (definition.max - definition.min) * 1.0e-5f))
                return false;
        }
        for (int bank = 0; bank < 4; ++bank)
            for (int local = 0; local < cs80::CP_count; ++local)
                if (Cs80AudioProcessor::isMemoryControl(local)
                    && std::abs(actual.memories[(size_t) bank][(size_t) local]
                                - expected.memories[(size_t) bank][(size_t) local]) > 0.05f)
                    return false;
        return true;
    };
    const auto toggle = [](Cs80AudioProcessor& host)
    {
        Cs80AudioProcessor::ScopedUserEdit edit(host.getUndoableState(), "Compare history");
        host.abToggle();
    };
    const auto copy = [](Cs80AudioProcessor& host, int destination)
    {
        Cs80AudioProcessor::ScopedUserEdit edit(host.getUndoableState(), "Copy compare");
        host.abCopyCurrentTo(destination);
    };
    Cs80AudioProcessor host, reopened;
    juce::MemoryBlock fresh;
    host.getStateInformation(fresh);
    host.loadFactoryPreset(22);
    host.setPresetName("History A");
    host.setParamValue(cs80::P_footswitchPlugged, 1.0f);
    host.setParamValue(cs80::P_mod1Amount, 0.25f);
    host.setMemoryValue(2, cs80::CP_lpfCutoff, 777.0f);
    host.selectHardwareSource(0, 11);
    auto modernA = cs80::modern::ModernConfig {};
    modernA.msegs[0].points[1].level = 0.25f;
    host.getModernLayer().setConfig(modernA);
    const auto a = capture(host);
    host.abToggle();
    host.loadFactoryPreset(42);
    host.setPresetName("History B");
    host.setParamValue(cs80::P_mod1Amount, 0.75f);
    host.setMemoryValue(2, cs80::CP_lpfCutoff, 1777.0f);
    host.selectHardwareSource(0, 12);
    auto modernB = modernA;
    modernB.msegs[0].points[1].level = 0.75f;
    host.getModernLayer().setConfig(modernB);
    const auto b = capture(host);
    host.abToggle();
    host.getUndoableState().syncNow();
    host.getUndoManager().clearUndoHistory();
    check(matches(host, a), "A/B undo fixture starts with A and preserves physical setup");
    juce::MemoryBlock fixture;
    host.getStateInformation(fixture);
    const auto reset = [&] { host.setStateInformation(fixture.getData(), (int) fixture.getSize()); };
    const auto roundTrip = [&](int active, const Sound& current, const Sound& other)
    {
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        reopened.setStateInformation(saved.getData(), (int) saved.getSize());
        check(reopened.getAbSlot() == active && matches(reopened, current),
              "A/B history save/reload preserves active slot, sound, name and index");
        reopened.abToggle();
        check(reopened.getAbSlot() == 1 - active && matches(reopened, other),
              "A/B history save/reload preserves the other complete sound");
    };

    for (int from = 0; from < 2; ++from)
    {
        reset();
        if (from == 1) host.abToggle();
        host.getUndoableState().syncNow();
        host.getUndoManager().clearUndoHistory();
        const auto& before = from == 0 ? a : b;
        const auto& after = from == 0 ? b : a;
        toggle(host);
        check(host.getAbSlot() == 1 - from && matches(host, after), "scoped A/B switch recalls the whole target sound");
        check(host.undo() && host.getAbSlot() == from && matches(host, before),
              "A/B undo restores selection, metadata, parameters, memories and modern sound together");
        roundTrip(from, before, after);
        check(host.redo() && host.getAbSlot() == 1 - from && matches(host, after),
              "A/B redo restores selection and the same complete sound");
        roundTrip(1 - from, after, before);
        check(!host.getUndoManager().canRedo(), "A/B redo does not create nested history actions");
    }

    reset();
    toggle(host); // A -> B
    {
        Cs80AudioProcessor::ScopedUserEdit edit(host.getUndoableState(), "Edit B");
        host.setParamValue(cs80::P_mod1Amount, 0.125f);
        host.getModernLayer().setConfig(modernA);
    }
    const auto editedB = capture(host);
    toggle(host); // B2 -> A
    check(host.undo() && matches(host, editedB), "undo switch restores edited B");
    check(host.undo() && matches(host, b), "undo B edit restores original B");
    check(host.undo() && host.getAbSlot() == 0 && matches(host, a), "undo original switch restores A");
    check(host.redo() && matches(host, b) && host.redo() && matches(host, editedB)
              && host.redo() && matches(host, a), "multi-switch and edit history redoes in original order");
    check(host.undo() && host.undo() && host.undo(), "multi-switch history can be unwound again");
    toggle(host);
    check(matches(host, b), "switch after multi-undo does not resurrect an undone B edit");

    reset();
    check(!host.getUndoableState().hasActiveUserEdit(), "side-state history is off outside an explicit edit scope");
    if (auto* parameter = host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(cs80::P_mod1Amount)))
    {
        parameter->beginChangeGesture();
        check(!host.getUndoableState().hasActiveUserEdit(), "an unrelated parameter gesture does not enable side-state history");
        parameter->endChangeGesture();
    }
    Sound groupedSound;
    {
        Cs80AudioProcessor::ScopedUserEdit outer(host.getUndoableState(), "Grouped compare");
        check(host.getUndoableState().hasActiveUserEdit(), "outer edit enables side-state history");
        toggle(host);
        host.setParamValue(cs80::P_mod1Amount, 0.125f);
        host.getModernLayer().setConfig(modernA);
        groupedSound = capture(host);
        copy(host, 0);
        toggle(host);
        check(host.getUndoableState().hasActiveUserEdit(), "nested edits preserve the outer edit scope");
    }
    check(!host.getUndoableState().hasActiveUserEdit() && host.getAbSlot() == 0 && matches(host, groupedSound),
          "grouped toggle/edit/Copy/toggle leaves the copied B sound in A");
    check(host.undo() && !host.getUndoManager().canUndo() && host.getAbSlot() == 0 && matches(host, a),
          "nested A/B and Copy scopes undo as exactly one transaction");
    roundTrip(0, a, b);
    check(host.redo() && !host.getUndoManager().canRedo() && matches(host, groupedSound),
          "nested A/B and Copy scopes redo as exactly one transaction");
    roundTrip(0, groupedSound, groupedSound);

    reset();
    toggle(host);
    check(host.undo(), "undo switch before copying");
    copy(host, 1);
    check(!host.getUndoManager().canRedo() && host.getAbSlot() == 0 && matches(host, a),
          "toolbar Copy starts its own history branch without changing current sound or metadata");
    roundTrip(0, a, a);
    check(host.undo() && matches(host, a), "Copy undo leaves current sound untouched");
    roundTrip(0, a, b);
    check(host.redo() && matches(host, a), "Copy redo leaves current sound untouched");
    roundTrip(0, a, a);
    host.setPresetName("Later external rename");
    check(host.undo() && host.getPresetName() == "Later external rename"
              && host.redo() && host.getPresetName() == "Later external rename",
          "Copy replay never overwrites later live metadata");

    host.setStateInformation(fresh.getData(), (int) fresh.getSize());
    const auto initial = capture(host);
    toggle(host);
    check(host.isAbSlotStored(0) && host.isAbSlotStored(1), "first scoped switch initializes both snapshots");
    check(host.undo() && host.getAbSlot() == 0 && !host.isAbSlotStored(0)
              && !host.isAbSlotStored(1) && matches(host, initial),
          "undo first switch restores the original empty-slot lifecycle");
    {
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        reopened.setStateInformation(saved.getData(), (int) saved.getSize());
        check(reopened.getAbSlot() == 0 && !reopened.isAbSlotStored(0)
                  && !reopened.isAbSlotStored(1) && matches(reopened, initial),
              "save/reload immediately after first-switch undo preserves empty snapshots");
    }
    check(host.redo() && host.getAbSlot() == 1 && host.isAbSlotStored(0)
              && host.isAbSlotStored(1) && matches(host, initial), "redo first switch restores initialized snapshots");
    roundTrip(1, initial, initial);
    host.setStateInformation(fresh.getData(), (int) fresh.getSize());
    copy(host, 1);
    check(host.undo() && !host.isAbSlotStored(1) && host.redo() && host.isAbSlotStored(1),
          "Copy undo/redo preserves the target's empty/stored flag");
    host.getUndoManager().clearUndoHistory();
    host.abCopyCurrentTo(0);
    check(!host.getUndoManager().canUndo(), "unscoped Copy keeps its existing non-history behavior");
    copy(host, -1);
    copy(host, 2);
    check(!host.getUndoManager().canUndo(), "invalid Copy destinations add no history");
}
} // namespace cs80::tests
