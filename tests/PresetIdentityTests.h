#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/PluginEditor.h"
#include "plugin/PresetBrowser.h"

#include <functional>
#include <memory>

namespace cs80::tests
{
struct PresetIdentityAccess
{
    // Baseline witness uses the real writer, XML files, list handlers and toolbar
    // navigation. Equal display names are intentional; filenames stay distinct.
    template <typename Check>
    static void run(Check check)
    {
        juce::TemporaryFile fixture("-cs80-preset-identity");
        const auto root = fixture.getFile();
        auto host = std::make_unique<Cs80AudioProcessor>(root.getChildFile("presets"));
        host->setParamValue(P_brilliance, 0.125f);
        const auto first = host->saveUserPreset("A Identity");
        host->setParamValue(P_brilliance, 0.875f);
        const auto second = host->saveUserPreset("B Identity");
        check(first.wasSaved() && second.wasSaved(), "identity fixture saves distinct real files");
        if (!first.wasSaved() || !second.wasSaved()) return;
        for (const auto& file : {first.file, second.file})
        {
            auto xml = juce::XmlDocument::parse(file);
            if (!xml) { check(false, "identity fixture parses"); return; }
            xml->setAttribute("presetName", "Shared display name");
            check(xml->writeTo(file), "identity fixture writes duplicate display names");
        }
        host->loadInitPatch();
        ui::PresetBrowser browser(*host);
        const bool oldAudition = browser.isAuditioning();
        struct RestoreAudition
        {
            ui::PresetBrowser& browser; bool old;
            ~RestoreAudition() { browser.setAudition(old); }
        } restore { browser, oldAudition };
        browser.setAudition(false);
        browser.scopeFilter = 4;
        browser.sortMode = 1;
        browser.sortReverse = false;
        browser.rebuildFilter();
        juce::ListBox* list = nullptr;
        for (auto* child : browser.getChildren())
            if (auto* candidate = dynamic_cast<juce::ListBox*>(child)) list = candidate;
        check(list != nullptr && browser.getNumRows() == 2,
              "identity witness reaches two duplicate-name browser rows");
        if (list == nullptr || browser.getNumRows() != 2) return;
        list->selectRow(1);
        browser.loadButton.onClick();
        check(host->getParamValue(P_brilliance) == 0.875f,
              "duplicate-name LOAD recalls the selected second file's sound");
        check(browser.loadedKey == "U:B Identity.cs80xml"
                  && browser.loadButton.getButtonText() == "RELOAD",
              "duplicate-name LOAD marks the actual second file, not the first name match");
        browser.loadAdjacent(1);
        check(host->getParamValue(P_brilliance) == 0.125f
                  && browser.loadedKey == "U:A Identity.cs80xml",
              "NEXT wraps from the actual second file to the first duplicate");
        browser.loadAdjacent(-1);
        check(host->getParamValue(P_brilliance) == 0.875f
                  && browser.loadedKey == "U:B Identity.cs80xml",
              "PREV wraps from the actual first file to the second duplicate");
        using Origin = cs80::PresetOrigin;
        const auto isFile = [&](const juce::File& file)
        {
            const auto meta = host->getPresetMetadata();
            return meta.origin.kind == Origin::Kind::userFile && meta.origin.file == file;
        };
        const auto load = [&](const juce::File& file)
        {
            check(host->loadUserPreset(file).wasOk(), "external processor file recall succeeds");
            browser.pollLoaded();
        };
        const auto bKey = juce::String("U:B Identity.cs80xml");
        const auto aKey = juce::String("U:A Identity.cs80xml");
        check(!browser.loadedEdited, "switching equal display names does not reuse another file's edit baseline");
        browser.captureOpening();
        load(first.file);
        check(browser.loadedKey == aKey, "external file recall invalidates the old browser marker");
        browser.revertToOpening();
        check(isFile(second.file) && browser.loadedKey == bKey
                  && host->getParamValue(P_brilliance) == 0.875f,
              "REVERT restores the captured duplicate file origin and sound");
        browser.setVisible(false);
        browser.setVisible(true);
        browser.refresh();
        check(browser.loadedKey == bKey && !browser.canRevert(),
              "hide/reopen captures the current origin while refresh retains it");
        {
            ui::PresetBrowser other(*host);
            check(other.loadedKey == bKey && other.selectedEntry() != nullptr
                      && other.selectedEntry()->userFile == second.file,
                  "a separate browser instance selects the same authoritative file");
            load(first.file);
            other.pollLoaded();
            check(other.loadedKey == aKey && browser.loadedKey == aKey,
                  "independent browsers observe an external same-name load");
        }
        load(second.file);
        host->setPresetName("Edited label only");
        browser.pollLoaded();
        check(isFile(second.file) && browser.loadedKey == bKey && !browser.loadedEdited,
              "metadata-only rename retains file origin without changing sound");
        host->setParamValue(P_brilliance, 0.5f);
        browser.pollLoaded();
        check(browser.loadedEdited, "renamed file-origin sound still compares edits against its exact file");
        host->selectHardwareSource(0, 11);
        browser.pollLoaded();
        check(isFile(second.file) && browser.loadedKey == bKey,
              "hardware-source name edits retain the actual file origin");
        load(second.file);
        auto secondXml = juce::XmlDocument::parse(second.file);
        secondXml->setAttribute("presetName", "External metadata edit");
        check(secondXml->writeTo(second.file), "external XML metadata-only edit succeeds");
        browser.refresh();
        check(browser.loadedKey == bKey && isFile(second.file) && !browser.loadedEdited,
              "catalogue metadata edits do not relabel the loaded file as its duplicate");
        const auto validB = second.file.loadFileAsString();
        const auto beforeFailure = host->getPresetMetadata();
        check(second.file.replaceWithText("<CS80_PARAMS><"), "corrupt only the isolated loaded-file fixture");
        check(host->loadUserPreset(second.file).failed() && isFile(second.file)
                  && host->getPresetName() == beforeFailure.name,
              "failed file reload preserves existing origin and metadata");
        check(second.file.replaceWithText(validB), "repair the isolated loaded-file fixture");
        load(second.file);
        check(browser.loadedKey == bKey, "repaired exact-file retry retains correct origin");
        const auto beforeSaveFailure = host->getPresetMetadata();
        check(!host->saveUserPreset("A Identity").wasSaved() && isFile(second.file)
                  && host->getPresetName() == beforeSaveFailure.name,
              "rejected overwrite leaves the previously loaded file identity intact");
        const auto overwritten = host->saveUserPreset("A Identity", Cs80AudioProcessor::PresetOverwrite::yes);
        browser.refresh();
        check(overwritten.wasSaved() && isFile(first.file) && browser.loadedKey == aKey,
              "successful overwrite publishes its destination as the new origin");

        // File provenance is a path identity, not a file watcher or content hash.
        // Moving/removing a loaded file cannot silently transfer ownership to a
        // duplicate. Refresh keeps the captured REVERT sound independently.
        load(second.file);
        auto missingAlias = juce::XmlDocument::parse(first.file);
        missingAlias->setAttribute("presetName", host->getPresetName());
        check(missingAlias->writeTo(first.file), "missing-file case retains another same-name catalogue entry");
        browser.refresh();
        browser.captureOpening();
        const auto moved = root.getChildFile("moved.cs80xml");
        check(second.file.moveFileTo(moved), "move loaded file out of the isolated catalogue");
        browser.refresh();
        check(isFile(second.file) && browser.loadedKey.isEmpty(),
              "missing known file does not fall back to another display-name match");
        host->loadInitPatch();
        browser.revertToOpening();
        check(isFile(second.file) && browser.loadedKey.isEmpty()
                  && host->getParamValue(P_brilliance) == 0.875f,
              "REVERT restores a missing file's captured sound without reading or substituting a file");
        check(moved.moveFileTo(second.file), "restore exact file path after simulated deletion");
        browser.refresh();
        check(browser.loadedKey == bKey, "restored known path regains its loaded marker on refresh");
        const auto outside = root.getChildFile("outside").getChildFile(second.file.getFileName());
        check(outside.getParentDirectory().createDirectory().wasOk()
                  && second.file.copyFileTo(outside), "create an outside file with the same basename");
        load(outside);
        check(isFile(outside) && browser.loadedKey.isEmpty(),
              "same basename outside this library is not the catalogue file");
        browser.loadAdjacent(1);
        check(browser.loadedKey == aKey, "NEXT from a known outside-file origin starts at the filter's first row");

        // The exact same name/index is legal for a factory program and a file.
        const int factory = cs80::legacyProgramCount;
        const auto factoryName = juce::String(host->getFactoryPresets()[(size_t) factory].name);
        secondXml = juce::XmlDocument::parse(second.file);
        secondXml->setAttribute("presetName", factoryName);
        secondXml->setAttribute("factoryIndex", factory);
        check(secondXml->writeTo(second.file), "write deliberate factory-name collision");
        browser.refresh();
        load(second.file);
        check(browser.loadedKey == bKey && isFile(second.file),
              "user-file provenance wins even when name and factory index exactly match a factory program");
        juce::MemoryBlock state;
        host->getStateInformation(state);
        const auto stateText = juce::String::fromUTF8(static_cast<const char*>(state.getData()), (int) state.getSize());
        check(!stateText.contains(root.getFullPathName()) && !second.file.loadFileAsString().contains(root.getFullPathName()),
              "session and preset XML contain no transient filesystem provenance");
        host->setCurrentProgram(factory);
        browser.pollLoaded();
        check(browser.loadedKey == "F:" + factoryName
                  && host->getPresetMetadata().origin.kind == Origin::Kind::factory,
              "host program recall replaces file provenance despite unchanged display metadata");
        host->setStateInformation(state.getData(), (int) state.getSize());
        browser.pollLoaded();
        check(browser.loadedKey.isEmpty() && host->getPresetMetadata().origin.kind == Origin::Kind::unknown
                  && host->getParamValue(P_brilliance) == 0.875f,
              "path-free session restore keeps sound but deliberately claims no exact library origin");
        load(second.file);
        host->setStateInformation("<NOT_A_PRESET/>", 15);
        check(isFile(second.file), "rejected session state cannot clear a known file origin");

        // In-process compare history owns origins; serialized compare slots do not.
        check(first.file.replaceWithText(second.file.loadFileAsString()),
              "A/B fixture has identical name/index/sound but distinct file origins");
        browser.refresh();
        load(first.file);
        host->abCopyCurrentTo(0);
        load(second.file);
        host->abCopyCurrentTo(1);
        load(first.file);
        {
            Cs80AudioProcessor::ScopedUserEdit edit(host->getUndoableState(), "Compare origin");
            host->abToggle();
        }
        browser.pollLoaded();
        check(isFile(second.file) && browser.loadedKey == bKey, "A/B recall restores exact second-file provenance");
        check(host->undo() && isFile(first.file), "A/B undo restores first-file provenance");
        check(host->redo() && isFile(second.file), "A/B redo restores second-file provenance");
        {
            Cs80AudioProcessor::ScopedUserEdit edit(host->getUndoableState(), "Random origin");
            host->randomiseWithinOriginalRanges(42);
        }
        check(isFile(second.file) && host->undo() && isFile(second.file)
                  && host->redo() && isFile(second.file),
              "randomise edits and metadata undo/redo retain current file provenance");
        host->getStateInformation(state);
        const auto compareText = juce::String::fromUTF8(static_cast<const char*>(state.getData()), (int) state.getSize());
        check(compareText.contains("AB_COMPARE") && !compareText.contains(root.getFullPathName()),
              "serialized A/B state also omits runtime file origins");
        host->setStateInformation(state.getData(), (int) state.getSize());
        host->abToggle();
        check(host->getPresetMetadata().origin.kind == Origin::Kind::unknown,
              "restored inactive A/B slot has unknown rather than stale file provenance");

        load(second.file);
        host->setParamValue(P_parityMode, 0.0f);
        host->prepareToPlay(48000.0, 64);
        juce::AudioBuffer<float> audio(2, 64);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::programChange(1, 0), 0);
        host->processBlock(audio, midi);
        check(host->getPresetMetadata().origin.kind == Origin::Kind::factory,
              "MIDI program publication overrides previous file origin without browser intervention");
        host->setPresetName("MIDI then rename");
        browser.pollLoaded();
        check(host->getPresetMetadata().origin.kind == Origin::Kind::factory
                  && host->getPresetMetadata().origin.file == juce::File()
                  && browser.loadedKey == "F:" + factoryName,
              "rename after pending MIDI recall cannot resurrect the old user-file origin");
        host->selectHardwareSource(1, 11);
        browser.pollLoaded();
        check(browser.loadedKey == "F:" + factoryName,
              "hardware-source rename after MIDI also retains factory provenance");
        host->releaseResources();

        // Identity alone can differ: REVERT must restore it even for byte-equal
        // duplicate sounds with equal display names and parameter values.
        check(first.file.replaceWithText(second.file.loadFileAsString()), "make two equal-sound duplicate fixtures");
        browser.refresh();
        load(first.file);
        browser.captureOpening();
        load(second.file);
        check(browser.canRevert(), "different exact origins enable REVERT for otherwise equal sounds");
        browser.revertToOpening();
        check(isFile(first.file) && browser.loadedKey == aKey && !browser.canRevert(),
              "identity-only REVERT restores the opening file instead of a name match");
        // The real editor shares the browser's edited verdict. Its origin cache
        // must change even when two file loads keep the same display name/index.
        load(second.file);
        {
            auto editor = std::make_unique<Cs80AudioProcessorEditor>(*host);
            std::function<juce::Component*(juce::Component&, const juce::String&)> find;
            find = [&](juce::Component& parent, const juce::String& id) -> juce::Component*
            {
                if (parent.getComponentID() == id) return &parent;
                for (auto* child : parent.getChildren())
                    if (auto* result = find(*child, id)) return result;
                return nullptr;
            };
            std::function<ui::PresetBrowser*(juce::Component&)> findBrowser;
            findBrowser = [&](juce::Component& parent) -> ui::PresetBrowser*
            {
                if (auto* found = dynamic_cast<ui::PresetBrowser*>(&parent)) return found;
                for (auto* child : parent.getChildren())
                    if (auto* found = findBrowser(*child)) return found;
                return nullptr;
            };
            auto* tag = dynamic_cast<juce::Label*>(find(*editor, "preset_category_tag"));
            auto* embedded = findBrowser(*editor);
            check(tag != nullptr && embedded != nullptr, "identity integration reaches real editor labels and browser");
            if (tag != nullptr && embedded != nullptr)
            {
                check(tag->getText() == "USER PATCH", "editor does not label a factory-name user file as FACTORY");
                const auto sync = [&] { embedded->onPresetLoaded(); embedded->pollLoaded(); };
                host->setParamValue(P_brilliance, 0.3125f);
                sync();
                check(embedded->loadedEdited, "editor-backed browser reports a live file edit");
                load(first.file); // Same XML name/index, different full path.
                sync();
                check(tag->getText() == "USER PATCH" && !embedded->loadedEdited
                          && embedded->loadedKey == aKey,
                      "editor and embedded browser reset the baseline on a same-name different-file recall");
                host->setCurrentProgram(factory);
                sync();
                check(tag->getText() != "USER PATCH" && embedded->loadedKey == "F:" + factoryName,
                      "same-name host factory recall changes editor classification and loaded marker");
                load(second.file);
                sync();
                check(tag->getText() == "USER PATCH" && embedded->loadedKey == bKey,
                      "same-name file recall restores the editor USER classification");
            }
        }
        {
            // Reentrant host display readers see the completed save's metadata
            // and origin together, not the previous file plus the new name.
            struct DisplayProbe final : juce::AudioProcessorListener
            {
                std::function<void()> changed;
                void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
                void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override { changed(); }
            } probe;
            const auto destination = root.getChildFile("presets/Published origin.cs80xml");
            bool consistent = true;
            int notifications = 0;
            probe.changed = [&]
            {
                ++notifications;
                const auto meta = host->getPresetMetadata();
                consistent = consistent && meta.name == "Published origin"
                    && meta.origin == Origin::userFile(destination) && destination.existsAsFile();
            };
            host->addListener(&probe);
            const auto published = host->saveUserPreset("Published origin");
            host->removeListener(&probe);
            check(published.wasSaved() && notifications > 0 && consistent,
                  "successful save publishes complete origin/name before reentrant host display notification");
        }
        const auto validOpening = host->capturePresetBrowserSound();
        auto invalidOpening = validOpening;
        invalidOpening.sources[0] = -1;
        check(!host->restorePresetBrowserSound(invalidOpening)
                  && host->getPresetMetadata().origin == validOpening.origin,
              "rejected opening-sound preflight preserves current origin");


    }

};
} // namespace cs80::tests
