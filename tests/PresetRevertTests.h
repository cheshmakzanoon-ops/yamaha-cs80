#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/PresetBrowser.h"

#include <array>
#include <cmath>
#include <memory>
#include <limits>
#include <algorithm>
#include <vector>

namespace cs80::tests
{
struct PresetRevertAccess
{
    static void source(Cs80AudioProcessor& p, int channel, int value)
    {
        auto* parameter = p.getAPVTS().getParameter(Cs80AudioProcessor::hardwareSourceParameterId(channel));
        parameter->setValueNotifyingHost(parameter->convertTo0to1((float) value));
    }
    struct Probe final : juce::AudioProcessorParameter::Listener, juce::AudioProcessorListener
    {
        explicit Probe(Cs80AudioProcessor& p) : host(p), values((size_t) p.getParameters().size()), counts(values.size())
        {
            for (auto* parameter : host.getParameters()) parameter->addListener(this);
            host.addListener(this);
        }
        ~Probe() override
        {
            host.removeListener(this);
            for (auto* parameter : host.getParameters()) parameter->removeListener(this);
        }
        void parameterValueChanged(int index, float value) override
        { values[(size_t) index] = value; ++counts[(size_t) index]; }
        void parameterGestureChanged(int, bool) override { ++gestures; }
        void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override
        { ++displays; reported = host.getPresetMetadata(); } // Reentrant metadata access must be safe.
        Cs80AudioProcessor& host;
        std::vector<float> values;
        std::vector<int> counts;
        int gestures = 0, displays = 0;
        Cs80AudioProcessor::PresetMetadata reported;
    };
    static std::vector<float> render(Cs80AudioProcessor& host)
    {
        host.prepareToPlay(48000.0, 256);
        std::vector<float> result;
        for (int block = 0; block < 24; ++block)
        {
            juce::AudioBuffer<float> audio(2, 256);
            audio.clear();
            juce::MidiBuffer midi;
            if (block == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
            host.processBlock(audio, midi);
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                for (int channel = 0; channel < 2; ++channel)
                    result.push_back(audio.getSample(channel, sample));
        }
        host.releaseResources();
        return result;
    }
    template <typename Check>
    static void renderControls(Check check, const juce::File& directory)
    {
        // Deterministic new-note witnesses, not DSP-history snapshots or DAW tests.
        // Source and MEMORY negative controls must actually alter the output.
        for (int selected : { 0, 11, 12 })
        {
            auto expected = std::make_unique<Cs80AudioProcessor>(directory);
            auto reverted = std::make_unique<Cs80AudioProcessor>(directory);
            auto changed = std::make_unique<Cs80AudioProcessor>(directory);
            const auto configure = [&](Cs80AudioProcessor& p)
            {
                p.loadInitPatch();
                p.setPure(false);
                p.setParamValue(P_drift, 0.0f);
                p.setParamValue(P_voiceDispersion, 0.0f);
                p.setParamValue(P_analogNoise, 0.0f);
                p.setParamValue(P_volume, 0.2f);
                for (int bank = 0; bank < 4; ++bank)
                    for (int local = 0; local < CP_count; ++local)
                        if (Cs80AudioProcessor::isMemoryControl(local))
                            p.setMemoryValue(bank, local, p.getParamValue(chParam(bank % 2, local)));
                source(p, 0, selected);
                source(p, 1, selected);
                p.setPresetName("Rendered opening");
            };
            configure(*expected); configure(*reverted); configure(*changed);
            ui::PresetBrowser browser(*reverted);
            const auto mutate = [&](Cs80AudioProcessor& p)
            {
                if (selected < 11)
                {
                    source(p, 0, 7); source(p, 1, 7);
                }
                else
                {
                    for (int channel = 0; channel < 2; ++channel)
                        p.setMemoryValue(channel + (selected == 12 ? 2 : 0), CP_vLevel, 0.0f);
                }
            };
            mutate(*reverted); mutate(*changed);
            check(browser.canRevert(), "audible source/MEMORY edit alone enables REVERT");
            browser.revertToOpening();
            const auto reference = render(*expected);
            const auto actual = render(*reverted);
            const auto negative = render(*changed);
            double energy = 0.0, difference = 0.0;
            bool finite = true;
            for (size_t i = 0; i < reference.size(); ++i)
            {
                finite = finite && std::isfinite(reference[i]) && std::isfinite(actual[i]) && std::isfinite(negative[i]);
                energy += (double) reference[i] * reference[i];
                difference += std::abs((double) reference[i] - negative[i]);
            }
            check(finite && energy > 1.0e-8, "opening hardware factory/MEMORY render is finite and non-silent");
            check(difference > 1.0e-6, "unreverted factory/MEMORY negative control audibly differs");
            check(reference == actual, "REVERT restores sample-identical fresh-note factory/MEMORY audio");
            check(!browser.canRevert(), "audio prepare/release does not invalidate the restored opening snapshot");
        }
    }
    template <typename Check>
    static void run(Check check)
    {
        juce::TemporaryFile fixture("-cs80-preset-revert");
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile().getChildFile("presets"));
        host->loadFactoryPreset(legacyProgramCount + 3);
        host->setPresetName("Opening unsaved sound");
        ui::PresetBrowser browser(*host);
        const bool previousAudition = browser.isAuditioning();
        struct RestoreAudition
        {
            ui::PresetBrowser& browser;
            bool previous;
            ~RestoreAudition() { browser.setAudition(previous); }
        } restoreAudition { browser, previousAudition };
        browser.setAudition(false);
        int callbacks = 0;
        browser.onPresetLoaded = [&] { ++callbacks; };

        // Direct host edits deliberately preserve name/index: metadata must not
        // accidentally make these source-only changes detectable.
        for (int channel = 0; channel < 2; ++channel)
            for (int selected = 0; selected < 14; ++selected)
            {
                source(*host, channel, selected);
                browser.captureOpening();
                check(!browser.canRevert(), "new opening snapshot is clean for every hardware source");
                source(*host, channel, (selected + 1) % 14);
                check(browser.canRevert(), "source-only host edit enables REVERT for either rank and every source");
                browser.pollLoaded();
                check(browser.revertButton.isEnabled(), "source-only host change reaches the live REVERT button");
                browser.revertToOpening();
                check(host->getHardwareSource(channel) == selected,
                      "REVERT restores every factory, MEMORY and PANEL hardware source");
                check(!browser.canRevert(), "source REVERT returns to its complete opening baseline");
            }

        source(*host, 0, 13);
        source(*host, 1, 13);
        for (int bank = 0; bank < 4; ++bank)
            for (int local = 0; local < CP_count; ++local)
            {
                if (!Cs80AudioProcessor::isMemoryControl(local)) continue;
                const auto& def = kChannelDefs[local];
                host->setMemoryValue(bank, local, def.min);
                browser.captureOpening();
                host->setMemoryValue(bank, local, def.max);
                check(browser.canRevert(), "hidden MEMORY-only edit enables REVERT for every bank/control");
                browser.pollLoaded();
                check(browser.revertButton.isEnabled(), "hidden MEMORY-only change reaches the live REVERT button");
                browser.revertToOpening();
                check(host->getMemoryValue(bank, local) == def.min,
                      "REVERT restores every valid MEMORY value even when neither rank uses it");
                check(!browser.canRevert(), "MEMORY REVERT returns to its complete opening baseline");
            }

        host->loadFactoryPreset(legacyProgramCount + 5);
        host->setParamValue(P_brilliance, 0.375f);
        host->setPresetName("Edited custom opening");
        const auto meta = host->getPresetMetadata();
        const float brilliance = host->getParamValue(P_brilliance);
        source(*host, 0, 11);
        source(*host, 1, 12);
        browser.captureOpening();
        host->loadFactoryPreset(22);
        browser.revertToOpening();
        check(host->getPresetMetadata().name == meta.name
                  && host->getPresetMetadata().factoryIndex == meta.factoryIndex
                  && host->getCurrentProgram() == meta.factoryIndex
                  && host->getParamValue(P_brilliance) == brilliance,
              "REVERT restores custom name, original program index and unsaved core values together");
        check(host->getHardwareSource(0) == 11 && host->getHardwareSource(1) == 12,
              "mixed MEMORY A/B rank selection survives preset audition and REVERT");

        // Actual user-file audition can replace MEMORY data as well as core values.
        host->setMemoryValue(2, CP_lpfCutoff, 2345.0f);
        browser.captureOpening();
        host->setMemoryValue(2, CP_lpfCutoff, 111.0f);
        source(*host, 0, 4);
        source(*host, 1, 5);
        const auto auditionFile = host->saveUserPreset("Revert audition fixture");
        check(auditionFile.wasSaved(), "user audition fixture saves only into disposable storage");
        browser.revertToOpening();
        browser.refresh();
        int auditionEntry = -1;
        for (size_t i = 0; i < browser.entries.size(); ++i)
            if (browser.entries[i].userFile == auditionFile.file) auditionEntry = (int) i;
        check(auditionEntry >= 0, "real browser catalogue finds the saved audition fixture");
        browser.loadEntry(auditionEntry);
        check(host->getPresetName() == "Revert audition fixture" && host->getHardwareSource(0) == 4
                  && std::abs(host->getMemoryValue(2, CP_lpfCutoff) - 111.0f) < 0.01f,
              "real browser user load replaces source and MEMORY state");
        browser.revertToOpening();
        check(host->getPresetMetadata().name == meta.name
                  && host->getPresetMetadata().factoryIndex == meta.factoryIndex
                  && host->getHardwareSource(0) == 11 && host->getHardwareSource(1) == 12
                  && std::abs(host->getMemoryValue(2, CP_lpfCutoff) - 2345.0f) < 0.01f
                  && !browser.canRevert(),
              "REVERT after actual user-file audition restores opening name/index/source/MEMORY");

        // Opening snapshots remain stable through refresh, then reset only on
        // a genuine hide/reopen. REVERT never auditions a selected row.
        browser.setAudition(true);
        host->setPresetName("Reopened sound");
        host->setMemoryValue(3, CP_lpfCutoff, 3456.0f);
        source(*host, 0, 6);
        const auto reopenedMetadata = host->getPresetMetadata();
        browser.setVisible(false);
        browser.setVisible(true);
        check(!browser.canRevert(), "reopening captures the newly sounding source and MEMORY values");
        host->setMemoryValue(3, CP_lpfCutoff, 100.0f);
        source(*host, 0, 13);
        browser.refresh();
        const int beforeRevert = callbacks;
        browser.revertToOpening();
        check(host->getPresetName() == "Reopened sound" && host->getHardwareSource(0) == 6
                  && std::abs(host->getMemoryValue(3, CP_lpfCutoff) - 3456.0f) < 0.01f
                  && callbacks == beforeRevert + 1 && !browser.revertButton.isEnabled(),
              "refresh keeps opening sound and REVERT emits one load callback without audition");
        browser.revertToOpening();
        check(callbacks == beforeRevert + 1, "repeated no-op REVERT produces no additional load callback");

        browser.captureOpening();
        {
            Cs80AudioProcessor::ScopedUserEdit edit(host->getUndoableState(), "Live setup");
            for (int i = 0; i < P_numParams; ++i)
                if (Cs80AudioProcessor::isPhysicalSetupParameter(i) || i == P_parityMode)
                {
                    const auto& def = paramDef(i);
                    host->setParamValue(i, host->getParamValue(i) == def.min ? def.max : def.min);
                }
        }
        const auto liveSetup = host->currentValues();
        host->getMidiLearn().assign(74, P_brilliance);
        host->getEditorState().controllersShown.store(true);
        host->setExpressionFromUI(0.42f);
        check(host->loadScalaTuning("Live setup scale\n1\n2/1\n").isEmpty(), "isolated Scala fixture loads");
        const auto tuningRevision = host->scalaTuningRevision();
        auto modern = host->getModernLayer().getConfig();
        modern.slots[0].source = modern::ModSource::velocity;
        modern.slots[0].destination = modern::ModDestination::pitch;
        host->getModernLayer().setConfig(modern);
        host->abCopyCurrentTo(1);
        const int liveAb = host->getAbSlot();
        check(!browser.canRevert(), "setup/PURE and unrelated session-only changes do not enable REVERT");
        check(host->getUndoManager().canUndo(), "setup gesture supplies an undo history for no-op control");
        browser.revertToOpening();
        check(host->getUndoManager().canUndo(), "no-op REVERT does not clear unrelated setup undo history");

        host->setParamValue(P_brilliance, 0.91f);
        source(*host, 0, 12);
        host->setMemoryValue(3, CP_lpfCutoff, 100.0f);
        {
            Probe probe(*host);
            browser.revertToOpening();
            check(probe.gestures == 0 && probe.displays == 1
                      && probe.reported.name == "Reopened sound"
                      && probe.reported.factoryIndex == reopenedMetadata.factoryIndex,
                  "REVERT sends one final metadata display update and no synthetic user gestures");
            check(std::all_of(probe.counts.begin(), probe.counts.end(), [](int count) { return count <= 1; }),
                  "host receives no intermediate factory/PANEL parameter notifications");
            for (int channel = 0; channel < 2; ++channel)
            {
                auto* parameter = host->getAPVTS().getParameter(Cs80AudioProcessor::hardwareSourceParameterId(channel));
                check(probe.counts[(size_t) parameter->getParameterIndex()] == 1
                          && probe.values[(size_t) parameter->getParameterIndex()] == parameter->getValue(),
                      "each source is published once with its final recalled host value");
            }
            for (int bank = 0; bank < 4; ++bank)
                for (int local = 0; local < CP_count; ++local)
                    if (Cs80AudioProcessor::isMemoryControl(local))
                    {
                        auto* parameter = host->getAPVTS().getParameter(Cs80AudioProcessor::memoryParameterId(bank, local));
                        check(probe.counts[(size_t) parameter->getParameterIndex()] == 1
                                  && probe.values[(size_t) parameter->getParameterIndex()] == parameter->getValue(),
                              "every recalled MEMORY parameter notifies the host exactly once");
                    }
        }
        check(!host->getUndoManager().canUndo() && !host->getUndoManager().canRedo()
                  && !host->undo() && !host->redo(),
              "REVERT starts a fresh undo/redo history like loading a sound");
        for (int i = 0; i < P_numParams; ++i)
            if (Cs80AudioProcessor::isPhysicalSetupParameter(i) || i == P_parityMode)
                check(host->getParamValue(i) == liveSetup[i], "REVERT preserves every live physical/setup/PURE exclusion");
        check(host->getMidiLearn().parameterFor(74) == P_brilliance
                  && host->getEditorState().controllersShown.load()
                  && host->getExpressionValue() == 0.42f
                  && host->hasScalaTuning() && host->scalaTuningRevision() == tuningRevision
                  && host->getModernLayer().getConfig() == modern
                  && host->getAbSlot() == liveAb && host->isAbSlotStored(1),
              "bounded REVERT leaves MIDI/UI/pedal/tuning/modulation/A-B session side-state live");

        // A fresh user edit still records and replays after the recall boundary.
        const float before = host->getParamValue(P_brilliance);
        {
            Cs80AudioProcessor::ScopedUserEdit edit(host->getUndoableState(), "After REVERT");
            host->setParamValue(P_brilliance, 0.12f);
        }
        check(host->undo() && host->getParamValue(P_brilliance) == before,
              "new user edit after REVERT remains undoable");
        check(host->getUndoManager().canRedo(), "undone post-REVERT edit supplies a redo branch");
        source(*host, 0, 2);
        browser.revertToOpening();
        check(!host->getUndoManager().canRedo(), "a later real REVERT clears a previous sound's redo branch");

        // Malformed public snapshots must not partially publish before failure.
        {
            Cs80AudioProcessor::ScopedUserEdit edit(host->getUndoableState(), "Admission history control");
            host->setParamValue(P_outputLevel, host->getParamValue(P_outputLevel) == 0.0f ? 1.0f : 0.0f);
        }
        check(host->getUndoManager().canUndo(), "invalid snapshot admission has an existing history to preserve");
        const auto valid = host->capturePresetBrowserSound();
        for (int invalidCase = 0; invalidCase < 5; ++invalidCase)
        {
            auto invalid = valid;
            invalid.values[P_brilliance] = 0.55f; // An early valid change must not leak out.
            if (invalidCase == 0) invalid.memories[3][CP_afterLevel] = std::numeric_limits<float>::quiet_NaN();
            if (invalidCase == 1) invalid.values[P_numParams - 1] = std::numeric_limits<float>::infinity();
            if (invalidCase == 2) invalid.sources[1] = 14;
            if (invalidCase == 3) invalid.factoryIndex = -1;
            if (invalidCase == 4) invalid.memories[3][CP_afterLevel] = kChannelDefs[CP_afterLevel].max + 1.0f;
            Probe probe(*host);
            check(!host->restorePresetBrowserSound(invalid)
                      && host->currentValues().v == valid.values.v
                      && host->getPresetName() == valid.name && !browser.canRevert()
                      && probe.displays == 0 && probe.gestures == 0 && host->getUndoManager().canUndo()
                      && std::all_of(probe.counts.begin(), probe.counts.end(), [](int count) { return count == 0; }),
                  "invalid snapshot is rejected before any partial parameter/metadata/history publication");
        }
        auto ignored = valid;
        ignored.values[P_parityMode] = std::numeric_limits<float>::quiet_NaN();
        ignored.values[P_outputLevel] = std::numeric_limits<float>::infinity();
        ignored.memories[0][CP_feet] = std::numeric_limits<float>::quiet_NaN();
        check(host->restorePresetBrowserSound(ignored) && host->currentValues().v == valid.values.v,
              "excluded snapshot fields have no recall authority or validation requirement");

        // Ordinary recall must keep its original setup exclusions and PANEL policy.
        host->applyParameterValues(defaultValues());
        check(host->getHardwareSource(0) == 13 && host->getHardwareSource(1) == 13,
              "ordinary core preset recall still selects PANEL for both ranks");
        const auto saved = host->saveUserPreset("Ordinary recall control");
        check(saved.wasSaved(), "ordinary user recall control saves in isolated storage");
        const auto savedSetup = host->currentValues();
        for (int i = 0; i < P_numParams; ++i)
            if (Cs80AudioProcessor::isPhysicalSetupParameter(i) || i == P_parityMode)
            {
                const auto& def = paramDef(i);
                host->setParamValue(i, savedSetup[i] == def.min ? def.max : def.min);
            }
        const auto ordinarySetup = host->currentValues();
        host->loadUserPreset(saved.file);
        for (int i = 0; i < P_numParams; ++i)
            if (Cs80AudioProcessor::isPhysicalSetupParameter(i) || i == P_parityMode)
            {
                const bool live = i == P_parityMode || i == P_oversampling || i == P_outputJack || i == P_tuningMode;
                check(host->getParamValue(i) == (live ? ordinarySetup[i] : savedSetup[i]),
                      "ordinary user preset keeps its distinct saved-pedal/output and live session policy");
            }

        renderControls(check, fixture.getFile().getChildFile("render-presets"));
    }
};
} // namespace cs80::tests
