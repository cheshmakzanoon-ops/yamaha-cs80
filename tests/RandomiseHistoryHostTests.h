#pragma once

// Real processor regression coverage, called only from the initialized UI
// smoke host. Parameter history and preset metadata must describe one sound.
#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace cs80::tests
{
template <typename Check>
void testRandomiseHistoryHost(Check&& check)
{
    struct Sound
    {
        cs80::ParamValues values;
        std::array<int, 2> sources;
        Cs80AudioProcessor::PresetMetadata metadata;
    };
    const auto capture = [](Cs80AudioProcessor& host)
    {
        return Sound { host.currentValues(), { host.getHardwareSource(0), host.getHardwareSource(1) },
                       host.getPresetMetadata() };
    };
    const auto matches = [&](Cs80AudioProcessor& host, const Sound& expected)
    {
        const auto actual = capture(host);
        if (actual.sources != expected.sources || actual.metadata.name != expected.metadata.name
            || actual.metadata.factoryIndex != expected.metadata.factoryIndex)
            return false;
        for (int i = 0; i < cs80::P_numParams; ++i)
        {
            const auto& definition = cs80::paramDef(i);
            if (std::abs(actual.values[i] - expected.values[i])
                > std::max(1.0e-4f, (definition.max - definition.min) * 1.0e-5f))
                return false;
        }
        return true;
    };
    const auto randomise = [](Cs80AudioProcessor& host, juce::uint32 seed)
    {
        Cs80AudioProcessor::ScopedUserEdit edit(host.getUndoableState(), "Randomise");
        host.randomiseWithinOriginalRanges(seed);
    };
    const auto compareState = [](Cs80AudioProcessor& host)
    {
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        const auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(
            static_cast<const char*>(saved.getData()), (int) saved.getSize()));
        if (xml == nullptr) return juce::String();
        const auto compare = juce::ValueTree::fromXml(*xml).getChildWithName("AB_COMPARE");
        const auto compareXml = compare.createXml();
        return compareXml != nullptr ? compareXml->toString() : juce::String();
    };

    Cs80AudioProcessor host, reference;
    juce::MemoryBlock fresh;
    host.getStateInformation(fresh);
    const auto setup = [&](bool midiMetadata)
    {
        host.setStateInformation(fresh.getData(), (int) fresh.getSize());
        host.loadFactoryPreset(32);
        host.setParamValue(cs80::P_footswitchPlugged, 1.0f);
        host.selectHardwareSource(0, 11);
        host.setPresetName("Original user patch");
        if (midiMetadata)
        {
            host.setParamValue(cs80::P_mpeOn, 0.0f);
            host.setParamValue(cs80::P_parityMode, 0.0f);
            host.prepareToPlay(48000.0, 256);
            juce::AudioBuffer<float> audio(2, 256);
            audio.clear();
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 0, 1), 0); // Legacy bank
            midi.addEvent(juce::MidiMessage::programChange(1, 22), 1);
            host.processBlock(audio, midi);
            check(host.getPresetMetadata().factoryIndex == 22
                      && host.getPresetName() == host.getProgramName(22),
                  "Randomise history fixture starts from effective MIDI program metadata");
        }
        else
            host.setParamValue(cs80::P_parityMode, 1.0f);
        host.abCopyCurrentTo(1);
        host.getUndoableState().syncNow();
        host.getUndoManager().clearUndoHistory();
    };
    const auto roundTrip = [&](const Sound& expected)
    {
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        reference.setStateInformation(saved.getData(), (int) saved.getSize());
        check(matches(reference, expected), "Randomise history save/reload preserves sound, selectors, name and program index");
    };

    for (bool midiMetadata : { false, true })
        for (juce::uint32 seed : { 0x1234abcdu, 0xffffffffu })
        {
            setup(midiMetadata);
            const auto before = capture(host);
            const auto compareBefore = compareState(host);
            check(compareBefore.isNotEmpty(), "Randomise history fixture contains an inactive A/B snapshot");
            juce::MemoryBlock initial;
            host.getStateInformation(initial);
            reference.setStateInformation(initial.getData(), (int) initial.getSize());
            reference.randomiseWithinOriginalRanges(seed); // existing direct-call behavior
            const auto direct = capture(reference);
            if (midiMetadata)
                check(host.getPresetMetadata().factoryIndex == 22
                          && host.getPresetName() == host.getProgramName(22),
                      "effective MIDI metadata remains intact immediately before Randomise");
            randomise(host, seed);
            const auto after = capture(host);
            check(matches(host, direct), "recording metadata leaves seeded Randomise output and initial name unchanged");
            check(host.getPresetName() == (seed == 0x1234abcdu ? "Random 1234ABCD" : "Random FFFFFFFF"),
                  "Randomise retains its uppercase eight-digit seed label");
            check(after.metadata.factoryIndex == before.metadata.factoryIndex
                      && host.getAbSlot() == 0 && compareState(host) == compareBefore,
                  "Randomise preserves its source program index and A/B snapshots");
            check(host.undo() && matches(host, before), "Randomise undo restores old sound and effective metadata together");
            roundTrip(before);
            check(!host.getUndoManager().canUndo() && host.getUndoManager().canRedo(),
                  "Randomise and its metadata remain exactly one undo transaction after saving");
            check(host.redo() && matches(host, after), "Randomise redo restores its sound and generated name together");
            roundTrip(after);
            check(compareState(host) == compareBefore, "Randomise undo/redo never changes inactive A/B sound data");
        }

    setup(false);
    const auto beforeGroup = capture(host);
    {
        Cs80AudioProcessor::ScopedUserEdit outer(host.getUndoableState(), "Grouped Randomise");
        randomise(host, 1);
        randomise(host, 2);
    }
    const auto afterGroup = capture(host);
    check(host.undo() && !host.getUndoManager().canUndo() && matches(host, beforeGroup),
          "nested Randomise scopes undo sound and both name changes as one transaction");
    check(host.redo() && !host.getUndoManager().canRedo() && matches(host, afterGroup),
          "nested Randomise scopes redo the final seeded sound and name");

    setup(false);
    const auto beforeReentrantSave = capture(host);
    struct SaveOnParameter final : juce::AudioProcessorParameter::Listener
    {
        explicit SaveOnParameter(Cs80AudioProcessor& p) : owner(p) {}
        void parameterValueChanged(int, float) override
        {
            if (!fired)
            {
                fired = true;
                owner.getStateInformation(saved); // synchronously flush pending values into history
            }
        }
        void parameterGestureChanged(int, bool) override {}
        Cs80AudioProcessor& owner;
        bool fired = false;
        juce::MemoryBlock saved;
    } listener(host);
    auto* parameter = host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(cs80::P_detune));
    check(parameter != nullptr, "Randomise callback fixture resolves its parameter");
    if (parameter != nullptr)
    {
        parameter->addListener(&listener);
        randomise(host, 0xabcdefu);
        parameter->removeListener(&listener);
        const auto afterReentrantSave = capture(host);
        check(listener.fired && listener.saved.getSize() != 0, "host save callback flushes history during Randomise");
        check(host.undo() && matches(host, beforeReentrantSave), "reentrant save does not detach Randomise metadata from undo");
        check(host.redo() && matches(host, afterReentrantSave), "reentrant save preserves complete Randomise redo");
    }

    setup(false);
    const auto beforeRepeatedSeed = capture(host);
    randomise(host, 3);
    randomise(host, 3);
    check(host.undo() && !host.getUndoManager().canUndo() && matches(host, beforeRepeatedSeed),
          "repeating an unchanged seed adds no empty metadata-only undo step");

    setup(false);
    host.randomiseWithinOriginalRanges(3);
    host.getUndoableState().syncNow();
    check(!host.getUndoManager().canUndo() && host.getPresetName() == "Random 00000003",
          "unscoped Randomise keeps its existing non-history behavior");
}
} // namespace cs80::tests
