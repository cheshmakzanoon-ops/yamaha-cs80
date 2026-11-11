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
        { ++displays; reported = host.getPresetMetadata(); reportedModern = host.getModernLayer().getConfig(); } // Reentrant access must be safe.
        Cs80AudioProcessor& host;
        std::vector<float> values;
        std::vector<int> counts;
        int gestures = 0, displays = 0;
        Cs80AudioProcessor::PresetMetadata reported;
        modern::ModernConfig reportedModern;
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
    static modern::ModernConfig openingModern()
    {
        modern::ModernConfig result;
        for (int m = 0; m < modern::kMsegCount; ++m)
        {
            auto& shape = result.msegs[(size_t) m];
            shape.count = modern::kMsegMaxPoints;
            for (int point = 0; point < shape.count; ++point)
                shape.points[(size_t) point] = { (float) point * 0.125f,
                    (float) ((point + m) % 9) * 0.125f, (float) ((point + m) % 5 - 2) * 0.5f };
            shape.loopStart = m + 1;
            shape.loopEnd = 29 - m;
            shape.loop = (modern::MsegLoop) m;
            shape.trigger = (modern::MsegTrigger) m;
            shape.sync = m != 0;
            shape.bipolar = m != 1;
        }
        for (int i = 0; i < modern::kModSlots; ++i)
            result.slots[(size_t) i] = {
                (modern::ModSource) (1 + i % (modern::kModSourceCount - 1)),
                (modern::ModSource) ((i + 7) % modern::kModSourceCount),
                (modern::ModDestination) (1 + i % (modern::kModDestinationCount - 1)),
                (modern::ModTarget) (i % 3) };
        return result;
    }
    template <typename Check>
    static void modernControls(Check check, const juce::File& directory)
    {
        auto host = std::make_unique<Cs80AudioProcessor>(directory);
        host->loadInitPatch();
        const auto opening = openingModern();
        host->getModernLayer().setConfig(opening);
        ui::PresetBrowser browser(*host);
        const bool audition = browser.isAuditioning();
        browser.setAudition(false);
        // No name, parameter or source mutation can accidentally mask a missed
        // side-state change. Each field is independently the sole difference.
        for (int m = 0; m < modern::kMsegCount; ++m)
            for (int field = 0; field < 10; ++field)
            {
                host->getModernLayer().setConfig(opening);
                browser.captureOpening();
                auto changed = opening.msegs[(size_t) m];
                if (field == 0) changed.points[1].time += 0.03125f;
                if (field == 1) changed.points[2].level = 0.9375f;
                if (field == 2) changed.points[3].curve = 0.25f;
                if (field == 3) --changed.count;
                if (field == 4) ++changed.loopStart;
                if (field == 5) --changed.loopEnd;
                if (field == 6) changed.loop = (modern::MsegLoop) ((m + 1) % 3);
                if (field == 7) changed.trigger = (modern::MsegTrigger) ((m + 1) % 3);
                if (field == 8) changed.sync = !changed.sync;
                if (field == 9) changed.bipolar = !changed.bipolar;
                host->getModernLayer().setShape(m, changed);
                check(browser.canRevert(), "each MSEG-only shape/option edit enables REVERT in all three envelopes");
                browser.pollLoaded();
                check(browser.revertButton.isEnabled(), "MSEG-only change reaches live REVERT button");
                browser.revertToOpening();
                check(host->getModernLayer().getConfig() == opening && !browser.canRevert(),
                      "REVERT restores each MSEG shape and option from value-owned opening data");
            }
        for (int i = 0; i < modern::kModSlots; ++i)
            for (int field = 0; field < 4; ++field)
            {
                host->getModernLayer().setConfig(opening);
                browser.captureOpening();
                auto changed = opening.slots[(size_t) i];
                if (field == 0) changed.source = modern::ModSource::constant;
                if (field == 1) changed.via = modern::ModSource::none;
                if (field == 2) changed.destination = modern::ModDestination::none;
                if (field == 3) changed.target = (modern::ModTarget) ((i + 1) % 3);
                host->getModernLayer().setSlot(i, changed);
                check(browser.canRevert(), "each route-only field edit enables REVERT in all 16 zero-amount slots");
                browser.revertToOpening();
                check(host->getModernLayer().getConfig() == opening && !browser.canRevert(),
                      "REVERT restores source/via/destination/target in each matrix slot");
            }
        host->getModernLayer().setConfig(opening);
        browser.captureOpening();
        host->getModernLayer().resetToDefaults();
        const auto saved = host->saveUserPreset("Modern audition fixture");
        check(saved.wasSaved(), "modern user audition fixture saved into disposable storage");
        host->getModernLayer().setConfig(opening);
        browser.refresh();
        int entry = -1;
        for (size_t i = 0; i < browser.entries.size(); ++i)
            if (browser.entries[i].userFile == saved.file) entry = (int) i;
        check(entry >= 0, "real browser catalogue finds modern audition fixture");
        browser.loadEntry(entry);
        check(host->getModernLayer().getConfig() == modern::ModernConfig {},
              "actual user-file audition replaces all MSEGs and routes");
        browser.revertToOpening();
        check(host->getModernLayer().getConfig() == opening && !browser.canRevert(),
              "actual user-file audition then REVERT restores all opening MSEGs and routes");
        browser.setAudition(audition);
    }
    template <typename Check>
    static void modernAdmissionControls(Check check, const juce::File& directory)
    {
        auto host = std::make_unique<Cs80AudioProcessor>(directory);
        host->getModernLayer().setConfig(openingModern());
        host->getUndoManager().clearUndoHistory();
        for (float value : { 0.125f, 0.25f })
        {
            Cs80AudioProcessor::ScopedUserEdit edit(host->getUndoableState(), "Admission history");
            host->setParamValue(P_brilliance, value);
        }
        check(host->undo() && host->getUndoManager().canUndo() && host->getUndoManager().canRedo(),
              "modern admission control has both undo and redo history");
        const auto valid = host->capturePresetBrowserSound();
        const auto treeHandle = host->getAPVTS().state;
        const auto layerHandle = treeHandle.getChildWithName(ModernLayerState::treeType);
        const auto beforeTree = host->getAPVTS().copyState();
        const auto undoDescription = host->getUndoManager().getUndoDescription();
        const auto redoDescription = host->getUndoManager().getRedoDescription();
        const auto reject = [&](PresetBrowserSound invalid)
        {
            invalid.values[P_brilliance] = 0.875f;
            invalid.sources[1] = 12;
            invalid.memories[3][CP_afterLevel] = 0.75f;
            invalid.name = "Must not publish";
            Probe probe(*host);
            check(!host->restorePresetBrowserSound(invalid), "invalid modern snapshot is rejected, never silently repaired");
            const auto actual = host->capturePresetBrowserSound();
            check(actual.values.v == valid.values.v && actual.sources == valid.sources
                      && actual.memories == valid.memories && actual.modern == valid.modern
                      && actual.name == valid.name && actual.factoryIndex == valid.factoryIndex
                      && treeHandle == host->getAPVTS().state
                      && layerHandle == host->getAPVTS().state.getChildWithName(ModernLayerState::treeType)
                      && beforeTree.isEquivalentTo(host->getAPVTS().copyState())
                      && probe.displays == 0 && probe.gestures == 0
                      && std::all_of(probe.counts.begin(), probe.counts.end(), [](int count) { return count == 0; })
                      && host->getUndoManager().canUndo() && host->getUndoManager().canRedo()
                      && host->getUndoManager().getUndoDescription() == undoDescription
                      && host->getUndoManager().getRedoDescription() == redoDescription,
                  "modern rejection leaves all values, metadata, tree identities, notifications and history unchanged");
        };
        for (int m = 0; m < modern::kMsegCount; ++m)
            for (int fault = 0; fault < 21; ++fault)
            {
                auto invalid = valid;
                auto& shape = invalid.modern.msegs[(size_t) m];
                if (fault == 0) shape.count = 1;
                if (fault == 1) shape.count = modern::kMsegMaxPoints + 1;
                if (fault == 2) shape.points[0].time = 0.125f;
                if (fault == 3) shape.points[1].time = std::numeric_limits<float>::quiet_NaN();
                if (fault == 4) shape.points[1].level = std::numeric_limits<float>::infinity();
                if (fault == 5) shape.points[1].curve = std::numeric_limits<float>::quiet_NaN();
                if (fault == 6) shape.points[1].time = -0.125f;
                if (fault == 7) shape.points[2].time = 0.0f;
                if (fault == 8) shape.points.back().time = modern::kMsegMaxTime + 1.0f;
                if (fault == 9) shape.points[1].level = -0.125f;
                if (fault == 10) shape.points[1].level = 1.125f;
                if (fault == 11) shape.points[1].curve = -1.125f;
                if (fault == 12) shape.points[1].curve = 1.125f;
                if (fault == 13) shape.loopStart = -1;
                if (fault == 14) shape.loopStart = shape.count;
                if (fault == 15) shape.loopEnd = shape.loopStart - 1;
                if (fault == 16) shape.loopEnd = shape.count;
                if (fault == 17) shape.loop = (modern::MsegLoop) -1;
                if (fault == 18) shape.loop = (modern::MsegLoop) 3;
                if (fault == 19) shape.trigger = (modern::MsegTrigger) -1;
                if (fault == 20) shape.trigger = (modern::MsegTrigger) 3;
                reject(invalid);
            }
        for (int i = 0; i < modern::kModSlots; ++i)
            for (int fault = 0; fault < 8; ++fault)
            {
                auto invalid = valid;
                auto& slot = invalid.modern.slots[(size_t) i];
                if (fault < 2) slot.source = (modern::ModSource) (fault == 0 ? -1 : modern::kModSourceCount);
                else if (fault < 4) slot.via = (modern::ModSource) (fault == 2 ? -1 : modern::kModSourceCount);
                else if (fault < 6) slot.destination = (modern::ModDestination) (fault == 4 ? -1 : modern::kModDestinationCount);
                else slot.target = (modern::ModTarget) (fault == 6 ? -1 : 3);
                reject(invalid);
            }
        // Exact valid boundaries, including coincident points, must not be
        // tightened by the browser's admission check. Unused cells have no authority.
        auto edge = valid;
        edge.modern = {};
        auto& shape = edge.modern.msegs[0];
        shape = {};
        shape.count = 3;
        shape.points[0] = { 0.0f, 0.0f, -1.0f };
        shape.points[1] = { 0.0f, 1.0f, 1.0f };
        shape.points[2] = { modern::kMsegMaxTime, 0.0f, 0.0f };
        shape.loopStart = shape.loopEnd = 2;
        shape.loop = modern::MsegLoop::always;
        shape.trigger = modern::MsegTrigger::free;
        shape.sync = shape.bipolar = true;
        shape.points.back() = { std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity(), -9.0f };
        edge.modern.slots.back() = { modern::ModSource::constant, modern::ModSource::constant,
                                    modern::ModDestination::mseg3Rate, modern::ModTarget::rankII };
        check(host->restorePresetBrowserSound(edge)
                  && host->getModernLayer().getConfig() == modern::sanitizeConfig(edge.modern),
              "valid boundary values restore exactly and unused point padding has no recall authority");
    }
    template <typename Check>
    static void modernLifecycleControls(Check check, const juce::File& directory)
    {
        auto host = std::make_unique<Cs80AudioProcessor>(directory);
        ui::PresetBrowser browser(*host);
        const bool audition = browser.isAuditioning();
        browser.setAudition(false);
        browser.captureOpening();
        check(!host->getAPVTS().state.getChildWithName(ModernLayerState::treeType).isValid()
                  && !browser.canRevert(), "missing modern child is the clean default opening sound");
        host->getModernLayer().setConfig(openingModern());
        browser.revertToOpening();
        check(host->getModernLayer().getConfig() == modern::ModernConfig {} && !browser.canRevert(),
              "default opening restores after route edits without depending on tree presence");
        host->getModernLayer().setConfig(openingModern());
        browser.captureOpening();
        const auto owned = host->capturePresetBrowserSound();
        auto copied = owned;
        copied.modern.msegs[0].points[1].level = 0.9375f;
        copied.modern.slots.back().source = modern::ModSource::none;
        check(owned.modern == openingModern(), "copying and editing a snapshot cannot alias its opening shapes or routes");
        // Keep root and sibling handles, and unrelated properties, live across recall.
        auto root = host->getAPVTS().state;
        juce::ValueTree unrelated("REVERT_UNRELATED");
        unrelated.setProperty("live", 42, nullptr);
        root.addChild(unrelated, -1, nullptr);
        root.setProperty("liveSessionProperty", "retain", nullptr);
        auto layer = root.getChildWithName(ModernLayerState::treeType);
        host->getModernLayer().resetToDefaults();
        host->abCopyCurrentTo(1); // The inactive slot must keep this different sound.
        const auto copySlot = host->getAbSlot();
        int callbacks = 0;
        browser.onPresetLoaded = [&] { ++callbacks; };
        {
            struct ModernProbe final : juce::ChangeListener
            {
                explicit ModernProbe(ModernLayerState& state) : layer(state) { layer.addChangeListener(this); }
                ~ModernProbe() override { layer.removeChangeListener(this); }
                void changeListenerCallback(juce::ChangeBroadcaster*) override { ++calls; reported = layer.getConfig(); }
                ModernLayerState& layer;
                int calls = 0;
                modern::ModernConfig reported;
            };
            host->getModernLayer().dispatchPendingMessages();
            ModernProbe modernProbe(host->getModernLayer());
            Probe probe(*host);
            browser.revertToOpening();
            check(probe.displays == 1 && probe.gestures == 0 && callbacks == 1
                      && probe.reportedModern == openingModern(),
                  "modern-only REVERT publishes one final display/callback with complete modern data and no user gestures");
            host->getModernLayer().dispatchPendingMessages();
            check(modernProbe.calls == 1 && modernProbe.reported == openingModern(),
                  "modern editor notification observes one complete restored configuration");
        }
        check(host->getAPVTS().state == root && root.getChildWithName(ModernLayerState::treeType) == layer
                  && root.getChildWithName("REVERT_UNRELATED") == unrelated
                  && (int) unrelated.getProperty("live") == 42
                  && root.getProperty("liveSessionProperty").toString() == "retain"
                  && host->getAbSlot() == copySlot && host->isAbSlotStored(1),
              "modern recall preserves root/layer identity, unrelated sibling state and A/B storage");
        check(ModernLayerState::fromTree(layer) == openingModern()
                  && !host->getUndoManager().canUndo() && !host->getUndoManager().canRedo(),
              "modern-only recall serializes opening config and starts a fresh undo boundary");
        {
            Cs80AudioProcessor::ScopedUserEdit edit(host->getUndoableState(), "Modern after REVERT");
            host->getModernLayer().setConfig(copied.modern);
        }
        check(host->undo() && host->getModernLayer().getConfig() == owned.modern,
              "new modern edit after REVERT undoes to the restored opening sound");
        check(host->redo() && host->getModernLayer().getConfig() == copied.modern,
              "new modern edit after REVERT redoes independently of the previous sound");
        host->getModernLayer().setConfig(owned.modern);
        check(host->getUndoManager().canUndo() && !browser.canRevert(),
              "editing modern state back to baseline remains a no-op for REVERT");
        {
            Probe probe(*host);
            browser.revertToOpening();
            check(callbacks == 1 && probe.displays == 0 && host->getUndoManager().canUndo(),
                  "modern no-op preserves undo and emits no duplicate notification");
        }
        // Ordinary session replacement must not mutate a captured by-value opening.
        juce::MemoryBlock state;
        host->getStateInformation(state);
        host->getModernLayer().resetToDefaults();
        browser.setVisible(false);
        browser.setVisible(true);
        check(!browser.canRevert(), "reopening captures changed modern-only state as the new baseline");
        host->setStateInformation(state.getData(), (int) state.getSize());
        check(browser.canRevert(), "session replacement cannot alias the newly reopened snapshot");
        browser.refresh();
        browser.revertToOpening();
        check(host->getModernLayer().getConfig() == modern::ModernConfig {} && !browser.canRevert(),
              "refresh and session replacement retain the independently owned reopened baseline");
        // Inactive A/B content itself was never overwritten by REVERT.
        host->abToggle();
        check(host->getModernLayer().getConfig() == modern::ModernConfig {},
              "inactive A/B slot retains its independent modern configuration after REVERT");
        browser.setAudition(audition);
    }
    template <typename Check>
    static void modernRenderControls(Check check, const juce::File& directory)
    {
        for (int m = 0; m < modern::kMsegCount; ++m)
            for (bool shapeChange : { false, true })
            {
                auto expected = std::make_unique<Cs80AudioProcessor>(directory);
                auto reverted = std::make_unique<Cs80AudioProcessor>(directory);
                auto negative = std::make_unique<Cs80AudioProcessor>(directory);
                const int slot = m == 0 ? 0 : m == 1 ? 7 : 15;
                const auto configure = [&](Cs80AudioProcessor& p)
                {
                    p.loadInitPatch();
                    p.setPure(false);
                    p.setParamValue(P_drift, 0.0f);
                    p.setParamValue(P_voiceDispersion, 0.0f);
                    p.setParamValue(P_analogNoise, 0.0f);
                    p.setParamValue(P_volume, 0.2f);
                    p.setParamValue(P_mod1Amount + slot, 0.25f);
                    modern::ModernConfig c;
                    auto& shape = c.msegs[(size_t) m];
                    shape = {};
                    shape.points[0] = { 0.0f, 0.25f, 0.0f };
                    shape.points[1] = { 1.0f, 0.25f, 0.0f };
                    c.slots[(size_t) slot] = { (modern::ModSource) ((int) modern::ModSource::mseg1 + m), modern::ModSource::none,
                                              modern::ModDestination::pitch, (modern::ModTarget) m };
                    p.getModernLayer().setConfig(c);
                };
                configure(*expected); configure(*reverted); configure(*negative);
                ui::PresetBrowser browser(*reverted);
                const auto mutate = [&](Cs80AudioProcessor& p)
                {
                    auto c = p.getModernLayer().getConfig();
                    if (shapeChange)
                        c.msegs[(size_t) m].points[0].level = c.msegs[(size_t) m].points[1].level = 0.75f;
                    else c.slots[(size_t) slot].source = modern::ModSource::constant;
                    p.getModernLayer().setConfig(c);
                };
                mutate(*reverted); mutate(*negative);
                check(browser.canRevert(), "audible modern-only shape/route change enables REVERT");
                browser.revertToOpening();
                const auto reference = render(*expected), actual = render(*reverted), changed = render(*negative);
                double energy = 0.0, difference = 0.0;
                bool finite = true;
                for (size_t i = 0; i < reference.size(); ++i)
                {
                    finite = finite && std::isfinite(reference[i]) && std::isfinite(actual[i]) && std::isfinite(changed[i]);
                    energy += (double) reference[i] * reference[i];
                    difference += std::abs((double) reference[i] - changed[i]);
                }
                check(finite && energy > 1.0e-8, "modern opening render is finite and non-silent for each MSEG and rank target");
                check(difference > 1.0e-6, "unreverted MSEG/route negative control audibly differs");
                check(reference == actual && !browser.canRevert(),
                      "REVERT publishes each MSEG/route to sample-identical fresh-note audio");
                check(reverted->getEngine().getModernConfig() == expected->getModernLayer().getConfig(),
                      "audio thread consumes the recalled modern mailbox on the next block");
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
        const auto modern = host->getModernLayer().getConfig();
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
              "bounded REVERT preserves MIDI/UI/pedal/tuning/A-B and the unchanged opening modulation");

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
        modernControls(check, fixture.getFile().getChildFile("modern-presets"));
        modernAdmissionControls(check, fixture.getFile().getChildFile("modern-admission"));
        modernLifecycleControls(check, fixture.getFile().getChildFile("modern-lifecycle"));
        modernRenderControls(check, fixture.getFile().getChildFile("modern-render"));
    }
};
} // namespace cs80::tests
