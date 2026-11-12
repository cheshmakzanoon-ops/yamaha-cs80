#pragma once

// Normal tone-selector actions through the processor, without a native peer.
#include "plugin/PluginProcessor.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace cs80::tests
{
template <typename Check>
void testHardwareSourceHistory(Check&& check)
{
    const auto sameMetadata = [](const auto& a, const auto& b)
    {
        return a.name == b.name && a.factoryIndex == b.factoryIndex && a.origin == b.origin;
    };
    const auto sameSound = [&](Cs80AudioProcessor& host, const auto& sound)
    {
        const auto actual = host.capturePresetBrowserSound();
        if (actual.sources != sound.sources || actual.modern != sound.modern)
            return false;
        // A/B recall converts physical values through the existing parameter
        // normalisation, so use the established sound-comparison tolerance.
        for (int i = 0; i < P_numParams; ++i)
        {
            const auto& def = paramDef(i);
            if (std::abs(actual.values[i] - sound.values[i])
                > std::max(1.0e-4f, (def.max - def.min) * 1.0e-5f))
                return false;
        }
        for (int bank = 0; bank < 4; ++bank)
            for (int local = 0; local < CP_count; ++local)
                if (Cs80AudioProcessor::isMemoryControl(local))
                {
                    const auto& def = kChannelDefs[local];
                    if (std::abs(actual.memories[(size_t) bank][(size_t) local]
                                 - sound.memories[(size_t) bank][(size_t) local])
                        > std::max(1.0e-4f, (def.max - def.min) * 1.0e-5f))
                        return false;
                }
        return true;
    };
    struct Probe final : juce::AudioProcessorParameter::Listener
    {
        std::vector<char> events;
        void parameterValueChanged(int, float) override { events.push_back('V'); }
        void parameterGestureChanged(int, bool starting) override { events.push_back(starting ? 'B' : 'E'); }
    };

    for (int rank = 0; rank < 2; ++rank)
        for (int source = 0; source < 14; ++source)
        {
            Cs80AudioProcessor host;
            host.loadFactoryPreset(22);
            // A named edited patch with a different physical source in every case.
            host.selectHardwareSource(rank, source == 13 ? 11 : 13);
            host.setPresetName("My edited patch");
            host.setParamValue(P_volume, 0.43f);
            host.getUndoableState().syncNow();
            host.getUndoManager().clearUndoHistory();
            const auto before = host.capturePresetBrowserSound();
            const auto metadataBefore = host.getPresetMetadata();
            host.selectHardwareSource(rank, source);
            const auto after = host.capturePresetBrowserSound();
            const auto metadataAfter = host.getPresetMetadata();
            check(host.getHardwareSource(rank) == source && metadataAfter.name != metadataBefore.name,
                  "tone selection changes the requested source and descriptive name");
            check(metadataAfter.origin == metadataBefore.origin
                      && metadataAfter.factoryIndex == metadataBefore.factoryIndex,
                  "tone selection retains the loaded preset identity and program index");
            check(host.getUndoManager().getUndoDescriptions().size() == 1,
                  "tone selection records one transaction");
            Probe probe;
            auto* parameter = host.getAPVTS().getParameter(Cs80AudioProcessor::hardwareSourceParameterId(rank));
            parameter->addListener(&probe);
            check(host.undo() && sameSound(host, before),
                  "tone-selection undo restores the complete previous sound");
            check(probe.events == std::vector<char> {'B', 'V', 'E'},
                  "source undo retains one balanced host parameter gesture");
            check(sameMetadata(host.getPresetMetadata(), metadataBefore),
                  "tone-selection undo restores the previous patch name and identity");
            check(!host.getUndoManager().canUndo() && host.getUndoManager().canRedo(),
                  "tone-selection undo leaves exactly one redo step");
            probe.events.clear();
            check(host.redo() && sameSound(host, after)
                      && sameMetadata(host.getPresetMetadata(), metadataAfter),
                  "tone-selection redo restores the same sound and descriptive name");
            check(probe.events == std::vector<char> {'B', 'V', 'E'},
                  "source redo retains one balanced host parameter gesture");
            parameter->removeListener(&probe);
            check(!host.getUndoManager().canRedo(), "tone-selection redo does not record nested history");
        }

    // A deliberate click on the current source still assigns its source name.
    // That name-only change is an edit too, rather than silent loss of a user name.
    {
        Cs80AudioProcessor host;
        host.loadFactoryPreset(22);
        host.setPresetName("Keep my patch name");
        const auto before = host.getPresetMetadata();
        const auto sound = host.capturePresetBrowserSound();
        host.selectHardwareSource(0, host.getHardwareSource(0));
        const auto after = host.getPresetMetadata();
        check(after.name != before.name && sameSound(host, sound),
              "reselecting a source preserves its existing deliberate rename behavior");
        check(host.undo() && sameMetadata(host.getPresetMetadata(), before) && sameSound(host, sound),
              "undo can restore the patch name after reselecting its current source");
        check(host.redo() && sameMetadata(host.getPresetMetadata(), after) && sameSound(host, sound),
              "redo reapplies a source-only rename without changing sound");
        host.getUndoManager().clearUndoHistory();
        host.selectHardwareSource(0, host.getHardwareSource(0));
        check(!host.getUndoManager().canUndo(), "repeating the same source and name adds no empty step");
        {
            Cs80AudioProcessor::ScopedUserEdit edit(host.getUndoableState(), "Volume edit");
            host.setParamValue(P_volume, 0.67f);
        }
        check(host.undo() && host.getUndoManager().canRedo(), "prepare a valid redo branch after source naming");
        host.selectHardwareSource(0, host.getHardwareSource(0));
        check(!host.getUndoManager().canUndo() && host.getUndoManager().canRedo(),
              "same source and name preserves an existing redo branch");
    }

    // Preserve same-message-loop grouping and explicit multi-control edits.
    for (bool scoped : { false, true })
    {
        Cs80AudioProcessor host;
        host.loadFactoryPreset(42);
        const auto before = host.capturePresetBrowserSound();
        const auto metadataBefore = host.getPresetMetadata();
        const auto selectBoth = [&]
        {
            host.selectHardwareSource(0, 2);
            host.selectHardwareSource(1, 11);
        };
        if (scoped)
        {
            Cs80AudioProcessor::ScopedUserEdit edit(host.getUndoableState(), "Both ranks");
            selectBoth();
            host.setParamValue(P_volume, 0.37f);
        }
        else
            selectBoth();
        const auto after = host.capturePresetBrowserSound();
        const auto metadataAfter = host.getPresetMetadata();
        check(host.getUndoManager().getUndoDescriptions().size() == 1,
              "source selections retain existing same-pass and scoped transaction grouping");
        check(host.undo() && sameSound(host, before)
                  && sameMetadata(host.getPresetMetadata(), metadataBefore),
              "grouped source undo restores both ranks and original metadata");
        check(host.redo() && sameSound(host, after)
                  && sameMetadata(host.getPresetMetadata(), metadataAfter),
              "grouped source redo restores both ranks and final metadata");
    }

    // Real saved-user, restored-session and MIDI program identities all pass
    // through the same selector API. Only normal, self-generated state is used.
    for (int origin = 0; origin < 3; ++origin)
    {
        juce::TemporaryFile storage("-cs80-source-history");
        struct Cleanup
        {
            juce::File folder;
            ~Cleanup() { folder.deleteRecursively(); }
        } cleanup { storage.getFile() };
        Cs80AudioProcessor host(storage.getFile());
        host.loadFactoryPreset(22);
        const auto saved = host.saveUserPreset("Original named sound");
        check(saved.wasSaved() && host.loadUserPreset(saved.file).wasOk(),
              "source-history identity fixture saves and loads an ordinary user preset");
        if (origin == 1)
        {
            juce::MemoryBlock session;
            host.getStateInformation(session);
            host.setStateInformation(session.getData(), (int) session.getSize());
        }
        else if (origin == 2)
        {
            host.prepareToPlay(48000.0, 32);
            juce::AudioBuffer<float> audio(2, 32);
            audio.clear();
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 0, 1), 0);
            midi.addEvent(juce::MidiMessage::programChange(1, 42), 1);
            host.processBlock(audio, midi);
        }
        host.getUndoableState().syncNow();
        host.getUndoManager().clearUndoHistory();
        const auto before = host.capturePresetBrowserSound();
        const auto metadataBefore = host.getPresetMetadata();
        check(metadataBefore.origin.kind == (origin == 0 ? PresetOrigin::Kind::userFile
                  : origin == 1 ? PresetOrigin::Kind::unknown : PresetOrigin::Kind::factory),
              "source-history fixture has the intended user, session or MIDI provenance");
        host.selectHardwareSource(1, 12);
        check(host.undo() && sameSound(host, before)
                  && sameMetadata(host.getPresetMetadata(), metadataBefore),
              "source undo retains original user-file, unknown-session or effective MIDI identity");
        // An A/B snapshot after Undo must carry the recovered name, not the
        // label of the undone tone. It is an ordinary current-sound capture.
        host.abCopyCurrentTo(1);
        host.setPresetName("A later name");
        host.abToggle();
        check(sameSound(host, before) && sameMetadata(host.getPresetMetadata(), metadataBefore),
              "A/B recalls the recovered sound and patch name captured after source undo");
        // No change to file naming policy: an empty Save name uses current metadata.
        const auto resaved = host.saveUserPreset({}, Cs80AudioProcessor::PresetOverwrite::yes);
        check(resaved.wasSaved() && resaved.file.getFileNameWithoutExtension()
                  == juce::File::createLegalFileName(metadataBefore.name),
              "saving after source undo uses the recovered patch name");
    }
}
} // namespace cs80::tests
