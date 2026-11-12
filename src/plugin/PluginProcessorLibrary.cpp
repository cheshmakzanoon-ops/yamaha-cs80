// [ws6-library] Program library host integration: MIDI Program Change and bank
// select, A/B compare and "randomise within original ranges". Member functions
// of Cs80AudioProcessor kept out of PluginProcessor.cpp so its edits stay small.
// Rules and evidence: docs/workstreams/ws6-library.md.
#include "PluginProcessorLibrary.h"

#include "core/Randomise.h"

#include <algorithm>
#include <cmath>
#include <limits>

void Cs80AudioProcessor::initialiseLibrary()
{
    library = std::make_unique<LibraryState>(*this);
    for (int i = 0; i < cs80::P_numParams; ++i)
        library->parameters[(size_t) i] = apvts.getParameter(parameterId(i));
    for (int channel = 0; channel < 2; ++channel)
        library->sources[(size_t) channel] = apvts.getParameter(hardwareSourceParameterId(channel));
}

// ---------------------------------------------------------------------------
// MIDI Program Change (audio thread)
// ---------------------------------------------------------------------------
// Addressing (cs80::programIndexForMidi): bank MSB 0 = curated library, MSB 1 =
// Legacy Library, bank LSB = page of 128, Program Change = program on the page.
// MPE Program Change follows negotiated manager roles. Bank Select uses the
// same shared-bank product policy; channel 1 without a zone keeps its existing
// conventional fallback. PURE ignores both: the original has no MIDI.
namespace
{
bool acceptsMidiProgramChannel(const cs80::Engine& engine, int channel) noexcept
{
    if (cs80::isPureCs80(engine.getRenderPolicy())) return false;
    if (!engine.isMpeEnabled()) return true;
    const auto role = engine.getMpeZones().role(channel);
    return role == cs80::MpeZoneLayout::Role::lowerMaster
        || role == cs80::MpeZoneLayout::Role::upperMaster
        || (channel == 1 && role == cs80::MpeZoneLayout::Role::none);
}
}
void Cs80AudioProcessor::handleBankSelect(int controller, int value, int midiChannel) noexcept
{
    if (library == nullptr || !acceptsMidiProgramChannel(engine, midiChannel))
        return;
    if (controller == 0)
        library->bankMsb = value;
    else if (controller == 32)
        library->bankLsb = value;
}

bool Cs80AudioProcessor::handleProgramChange(int midiChannel, int program)
{
    if (library == nullptr || !acceptsMidiProgramChannel(engine, midiChannel))
        return false;
    const int index = cs80::programIndexForMidi(library->bankMsb, library->bankLsb, program);
    const auto& programs = getFactoryPresets();
    if (index < 0 || index >= (int) programs.size())
        return false;

    // Same recall contract as loadFactoryPreset(): both selectors to PANEL,
    // physical setup and isolation untouched. Resolved parameter objects and
    // the immutable library mean no allocation or ID construction here; JUCE
    // parameter writes from the audio thread are the same path as host
    // automation write-back.
    const auto& values = programs[(size_t) index].values;
    // Curve recalls retain their existing queue/aggregate handling. Ordinary
    // recalls also publish the frame to an already-active extension below.
    const bool vst3CurveRecall = vst3CurveRecallInProcess
        || (hostAutomationVst3 && hostAutomationBlock
            && std::any_of(automationCurves.begin(), automationCurves.end(),
                           [](const auto& curve) { return curve.active(); }));
    // Seed only after a valid, permitted recall sees actual VST3 curves. Keep
    // it for later recalls even when this one cancels the borrowed queues.
    vst3CurveRecallInProcess = vst3CurveRecall;
    // Finalize old endpoints before preset writes so they cannot overwrite a
    // recalled Legacy program. Same-policy modern curves remain attached.
    const auto recalledPolicy = cs80::renderPolicy(cached[cs80::P_parityMode],
                                                   values[cs80::P_sourcePerformance]);
    if (hostAutomationVst3 && hostAutomationBlock && !cs80::usesSourceModels(recalledPolicy))
        endHostAutomationBlock();
    for (auto* source : library->sources)
        if (source != nullptr)
            source->setValueNotifyingHost(source->convertTo0to1(13.0f));
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        if (isPhysicalSetupParameter(i) || i == cs80::P_parityMode)
            continue;
        if (auto* p = library->parameters[(size_t) i])
            p->setValueNotifyingHost(p->convertTo0to1(values[i]));
    }
    currentProgram.store(index, std::memory_order_relaxed);
    library->midiProgram.store(index, std::memory_order_release);
    // Host program display is coalesced by the existing message-thread state timer.
    library->notifyProgramChanged();

    // The new sound starts at the Program Change's sample position.
    readParameters();
    const auto previousPolicy = engine.getRenderPolicy();
    if (vst3CurveRecall)
    {
        // Keep main's physical-unit aggregate coordinate and active-extension
        // lifetime. Do not shut voices down at the program event.
        forEachEngine([&](cs80::Engine& target)
        {
            const auto prior = target.getRenderPolicy();
            target.setControlFrame({ cached, cachedHardwareSources });
            if (target.getRenderPolicy() != prior)
                target.setExpression(expressionValue.load(std::memory_order_relaxed));
        });
    }
    else
    {
        engine.setControlFrame({ cached, cachedHardwareSources });
        if (extensionActive)
        {
            const auto prior = extensionEngine->getRenderPolicy();
            extensionEngine->setControlFrame({ cached, cachedHardwareSources });
            if (extensionEngine->getRenderPolicy() != prior)
                extensionEngine->setExpression(expressionValue.load(std::memory_order_relaxed));
        }
    }
    if (engine.getRenderPolicy() == previousPolicy)
        return false;
    // A program on the other engine is a policy transition, exactly as at a
    // block boundary: the engine resets and the keyboard handoff is revoked.
    if (!vst3CurveRecall)
        engine.setExpression(expressionValue.load(std::memory_order_relaxed));
    resetKeyboardHandoff();
    return true;
}

// ---------------------------------------------------------------------------
// A/B compare (message thread)
// ---------------------------------------------------------------------------
// These actions restore side state only. The existing parameter and modern
// ValueTree actions remain responsible for the sound. Calling recall here
// would recapture snapshots or recursively record new actions during replay.
Cs80AudioProcessor::LibraryState::CompareHistoryState
Cs80AudioProcessor::LibraryState::captureCompareHistoryState() const
{
    CompareHistoryState result;
    {
        const juce::ScopedLock lock(abLock);
        result.slots = ab;
        result.active = abActive;
    }
    result.metadata = owner.getPresetMetadata();
    return result;
}

struct Cs80AudioProcessor::LibraryState::ToggleAction final : juce::UndoableAction
{
    ToggleAction(LibraryState& state, CompareHistoryState oldState, CompareHistoryState newState)
        : library(state), before(std::move(oldState)), after(std::move(newState)) {}

    bool perform() override
    {
        // The initial switch has already staged these snapshots. Record the
        // action before recall writes, without moving its normal notification.
        if (firstPerform)
            firstPerform = false;
        else
            apply(after);
        return true;
    }

    bool undo() override { apply(before); return true; }
    int getSizeInUnits() override
    {
        size_t bytes = sizeof(*this);
        for (const auto* state : { &before, &after })
        {
            bytes += state->metadata.name.getNumBytesAsUTF8() + state->metadata.origin.file.getFullPathName().getNumBytesAsUTF8() + 2;
            for (const auto& snapshot : state->slots)
                bytes += snapshot.name.getNumBytesAsUTF8() + snapshot.origin.file.getFullPathName().getNumBytesAsUTF8() + 2;
        }
        return (int) std::min(bytes, (size_t) std::numeric_limits<int>::max());
    }

    void apply(const CompareHistoryState& state)
    {
        const juce::ScopedLock treeLock(library.owner.apvts.treeLock());
        {
            const juce::ScopedLock lock(library.abLock);
            library.ab = state.slots;
            library.abActive = state.active;
        }
        library.owner.setPresetMetadata(state.metadata.name, state.metadata.factoryIndex, state.metadata.origin);
        // Redo applies this action before the sound's tree actions. The existing
        // async notifier must wait until the whole transaction has completed.
        library.notifyProgramChanged();
    }

    LibraryState& library;
    const CompareHistoryState before, after;
    bool firstPerform = true;
};

struct Cs80AudioProcessor::LibraryState::CopyAction final : juce::UndoableAction
{
    CopyAction(LibraryState& state, int destination, Snapshot oldState, Snapshot newState)
        : library(state), slot(destination), before(std::move(oldState)), after(std::move(newState)) {}

    bool perform() override
    {
        if (firstPerform)
            firstPerform = false;
        else
            apply(after);
        return true;
    }

    bool undo() override { apply(before); return true; }
    int getSizeInUnits() override
    {
        const size_t bytes = sizeof(*this) + before.name.getNumBytesAsUTF8()
                             + after.name.getNumBytesAsUTF8()
                             + before.origin.file.getFullPathName().getNumBytesAsUTF8()
                             + after.origin.file.getFullPathName().getNumBytesAsUTF8() + 4;
        return (int) std::min(bytes, (size_t) std::numeric_limits<int>::max());
    }

    void apply(const Snapshot& snapshot)
    {
        const juce::ScopedLock treeLock(library.owner.apvts.treeLock());
        const juce::ScopedLock lock(library.abLock);
        library.ab[(size_t) slot] = snapshot;
        // Copy never changes the active slot, sounding patch or its metadata.
    }

    LibraryState& library;
    const int slot;
    const Snapshot before, after;
    bool firstPerform = true;
};

void Cs80AudioProcessor::captureAbSnapshot(int slot)
{
    LibraryState::Snapshot s;
    s.stored = true;
    s.values = currentValues();
    s.modern = modernLayer.getConfig();
    for (int channel = 0; channel < 2; ++channel)
        s.sources[(size_t) channel] = getHardwareSource(channel);
    for (int bank = 0; bank < 4; ++bank)
        for (int local = 0; local < cs80::CP_count; ++local)
            s.memories[(size_t) bank][(size_t) local] = isMemoryControl(local) ? getMemoryValue(bank, local) : 0.0f;
    const auto metadata = getPresetMetadata();
    s.name = metadata.name;
    s.program = metadata.factoryIndex;
    s.origin = metadata.origin;
    const juce::ScopedLock lock(library->abLock);
    library->ab[(size_t) slot] = s;
}

void Cs80AudioProcessor::recallAbSnapshot(int slot)
{
    LibraryState::Snapshot s;
    {
        const juce::ScopedLock lock(library->abLock);
        s = library->ab[(size_t) slot];
    }
    if (! s.stored)
        return;
    const auto write = [](juce::RangedAudioParameter* p, float value)
    {
        if (p != nullptr)
            p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    for (int channel = 0; channel < 2; ++channel)
        write(library->sources[(size_t) channel], (float) s.sources[(size_t) channel]);
    for (int i = 0; i < cs80::P_numParams; ++i)
        if (! isPhysicalSetupParameter(i) && i != cs80::P_parityMode)
            write(library->parameters[(size_t) i], s.values[i]);
    for (int bank = 0; bank < 4; ++bank)
        for (int local = 0; local < cs80::CP_count; ++local)
            if (isMemoryControl(local))
                write(apvts.getParameter(memoryParameterId(bank, local)), s.memories[(size_t) bank][(size_t) local]);
    {
        // The modulation shape/routing is sound data, just like its parameter
        // amounts. Keep its tree write serialized with host state calls, and
        // outside abLock to preserve the treeLock -> abLock lock order.
        const juce::ScopedLock treeLock(apvts.treeLock());
        modernLayer.setConfig(s.modern);
    }
    setPresetMetadata(s.name, s.program, s.origin);
    updateHostDisplay();
}

void Cs80AudioProcessor::abToggle()
{
    if (library == nullptr)
        return;
    const juce::ScopedLock treeLock(apvts.treeLock());
    const bool record = apvts.hasActiveUserEdit();
    const auto before = record ? library->captureCompareHistoryState() : LibraryState::CompareHistoryState {};
    int target = 0;
    {
        const juce::ScopedLock lock(library->abLock);
        target = 1 - library->abActive;
    }
    captureAbSnapshot(1 - target);
    {
        const juce::ScopedLock lock(library->abLock);
        if (! library->ab[(size_t) target].stored)
            library->ab[(size_t) target] = library->ab[(size_t) (1 - target)];
        library->abActive = target;
    }
    if (record)
    {
        auto after = library->captureCompareHistoryState();
        const auto& destination = after.slots[(size_t) target];
        after.metadata = { destination.name, destination.program, destination.origin };
        undoManager.perform(new LibraryState::ToggleAction(*library, before, std::move(after)));
    }
    recallAbSnapshot(target);
}

void Cs80AudioProcessor::abCopyCurrentTo(int slot)
{
    if (library == nullptr || slot < 0 || slot > 1)
        return;
    const juce::ScopedLock treeLock(apvts.treeLock());
    // Only the inactive slot is persisted. Compare the serialized view so active-slot,
    // identical and in-process-origin-only copies do not dirty the host session.
    // This path is message-thread only; no serialization is added to the audio callback.
    juce::ValueTree beforeSavedState("AB_COPY_STATE");
    writeLibraryState(beforeSavedState);
    const bool record = apvts.hasActiveUserEdit();
    LibraryState::Snapshot before;
    if (record)
    {
        const juce::ScopedLock lock(library->abLock);
        before = library->ab[(size_t) slot];
    }
    captureAbSnapshot(slot);
    if (record)
    {
        LibraryState::Snapshot after;
        {
            const juce::ScopedLock lock(library->abLock);
            after = library->ab[(size_t) slot];
        }
        undoManager.perform(new LibraryState::CopyAction(*library, slot, std::move(before), std::move(after)));
    }
    juce::ValueTree afterSavedState("AB_COPY_STATE");
    writeLibraryState(afterSavedState);
    if (! beforeSavedState.isEquivalentTo(afterSavedState))
        apvts.requestNonParameterStateUpdate();

}

int Cs80AudioProcessor::getAbSlot() const noexcept
{
    if (library == nullptr)
        return 0;
    const juce::ScopedLock lock(library->abLock);
    return library->abActive;
}

bool Cs80AudioProcessor::isAbSlotStored(int slot) const
{
    if (library == nullptr || slot < 0 || slot > 1)
        return false;
    const juce::ScopedLock lock(library->abLock);
    return library->ab[(size_t) slot].stored;
}

// The inactive slot is saved with the session as <AB_COMPARE>; the active slot
// is the session's own sound. A state without it clears both slots.
void Cs80AudioProcessor::writeLibraryState(juce::ValueTree& state) const
{
    if (library == nullptr)
        return;
    const juce::ScopedLock lock(library->abLock);
    const int other = 1 - library->abActive;
    const auto& s = library->ab[(size_t) other];
    if (! s.stored)
        return;
    juce::ValueTree node("AB_COMPARE");
    node.setProperty("version", 1, nullptr);
    node.setProperty("active", library->abActive, nullptr);
    juce::ValueTree slot("SLOT");
    slot.setProperty("index", other, nullptr);
    slot.setProperty("presetName", s.name, nullptr);
    slot.setProperty("factoryIndex", s.program, nullptr);
    for (int channel = 0; channel < 2; ++channel)
        slot.setProperty(hardwareSourceParameterId(channel), s.sources[(size_t) channel], nullptr);
    for (int i = 0; i < cs80::P_numParams; ++i)
        slot.setProperty(parameterId(i), s.values[i], nullptr);
    for (int bank = 0; bank < 4; ++bank)
        for (int local = 0; local < cs80::CP_count; ++local)
            if (isMemoryControl(local))
                slot.setProperty(memoryParameterId(bank, local), s.memories[(size_t) bank][(size_t) local], nullptr);
    slot.addChild(cs80::ModernLayerState::toTree(s.modern), -1, nullptr);
    node.addChild(slot, -1, nullptr);
    state.addChild(node, -1, nullptr);
}

void Cs80AudioProcessor::readLibraryState(const juce::ValueTree& state)
{
    if (library == nullptr)
        return;
    LibraryState::Snapshot restored;
    int active = 0;
    const auto node = state.getChildWithName("AB_COMPARE");
    const auto slot = node.getChildWithName("SLOT");
    const int index = (int) slot.getProperty("index", -1);
    if (node.isValid() && (int) node.getProperty("version", 0) == 1 && slot.isValid() && (index == 0 || index == 1))
    {
        active = 1 - index;
        // Same completion/validation as a restored session: missing or invalid
        // values take their declared defaults, ranges and choices are enforced.
        const auto read = [&](const juce::String& id, float fallback, juce::RangedAudioParameter* p)
        {
            float value = (float) slot.getProperty(id, fallback);
            if (! std::isfinite(value)) value = fallback;
            if (p == nullptr) return value;
            const auto& range = p->getNormalisableRange();
            return range.snapToLegalValue(juce::jlimit(range.start, range.end, value));
        };
        restored.stored = true;
        // Older A/B snapshots stored no modulation config: both slots used
        // the session's top-level data. Inherit that incoming state (never
        // the receiving instance's live edits) only when the child is absent.
        const auto modern = slot.getChildWithName(cs80::ModernLayerState::treeType);
        restored.modern = cs80::ModernLayerState::fromTree(modern.isValid()
            ? modern : state.getChildWithName(cs80::ModernLayerState::treeType));
        restored.name = slot.getProperty("presetName", "Init Patch").toString();
        restored.program = juce::jlimit(0, juce::jmax(0, (int) getFactoryPresets().size() - 1),
                                        (int) slot.getProperty("factoryIndex", 0));
        for (int channel = 0; channel < 2; ++channel)
            restored.sources[(size_t) channel] = juce::roundToInt(
                read(hardwareSourceParameterId(channel), 13.0f, library->sources[(size_t) channel]));
        for (int i = 0; i < cs80::P_numParams; ++i)
            restored.values[i] = read(parameterId(i), cs80::paramDef(i).def, library->parameters[(size_t) i]);
        for (int bank = 0; bank < 4; ++bank)
            for (int local = 0; local < cs80::CP_count; ++local)
                if (isMemoryControl(local))
                    restored.memories[(size_t) bank][(size_t) local] = read(
                        memoryParameterId(bank, local), cs80::kChannelDefs[local].def,
                        apvts.getParameter(memoryParameterId(bank, local)));
    }
    const juce::ScopedLock lock(library->abLock);
    library->ab = {};
    library->abActive = active;
    if (restored.stored)
        library->ab[(size_t) index] = restored;
}

// ---------------------------------------------------------------------------
// Metadata for named sound edits (message thread)
// ---------------------------------------------------------------------------
struct Cs80AudioProcessor::LibraryState::MetadataAction final : juce::UndoableAction
{
    MetadataAction(LibraryState& state, PresetMetadata oldState, PresetMetadata newState)
        : library(state), before(std::move(oldState)), after(std::move(newState)) {}

    bool perform() override
    {
        // The original operation still sets its name and notifies the host.
        // Replays only restore metadata; the existing tree actions own sound.
        if (firstPerform)
            firstPerform = false;
        else
            apply(after);
        return true;
    }

    bool undo() override { apply(before); return true; }
    int getSizeInUnits() override
    {
        const size_t bytes = sizeof(*this) + before.name.getNumBytesAsUTF8()
                             + after.name.getNumBytesAsUTF8()
                             + before.origin.file.getFullPathName().getNumBytesAsUTF8()
                             + after.origin.file.getFullPathName().getNumBytesAsUTF8() + 4;
        return (int) std::min(bytes, (size_t) std::numeric_limits<int>::max());
    }

    void apply(const PresetMetadata& metadata)
    {
        const juce::ScopedLock treeLock(library.owner.apvts.treeLock());
        library.owner.setPresetMetadata(metadata.name, metadata.factoryIndex, metadata.origin);
        // Redo runs this before the parameter actions. Defer the display
        // notification instead of reporting a partly replayed sound.
        library.notifyProgramChanged();
    }

    LibraryState& library;
    const PresetMetadata before, after;
    bool firstPerform = true;
};

void Cs80AudioProcessor::LibraryState::recordMetadataChange(PresetMetadata before, PresetMetadata after)
{
    if (before.name == after.name && before.factoryIndex == after.factoryIndex && before.origin == after.origin)
        return;
    const juce::ScopedLock treeLock(owner.apvts.treeLock());
    owner.undoManager.perform(new MetadataAction(*this, std::move(before), std::move(after)));
}

// ---------------------------------------------------------------------------
// Randomise within original ranges (message thread)
// ---------------------------------------------------------------------------
void Cs80AudioProcessor::randomiseWithinOriginalRanges(juce::uint32 seed)
{
    const juce::ScopedLock treeLock(apvts.treeLock());
    const auto next = cs80::library::randomiseWithinOriginalRanges(currentValues(), seed);
    const auto name = "Random " + juce::String::toHexString((juce::int64) seed).paddedLeft('0', 8).toUpperCase();
    if (library != nullptr && apvts.hasActiveUserEdit())
    {
        const auto before = getPresetMetadata();
        // Randomise preserves the effective program index, including a prior
        // MIDI recall. Register before parameter callbacks can flush history.
        library->recordMetadataChange(before, PresetMetadata { name, before.factoryIndex, before.origin });
    }
    // A randomised patch is a PANEL sound; physical setup and isolation stay.
    applyValues(next);
    setPresetName(name);
}
