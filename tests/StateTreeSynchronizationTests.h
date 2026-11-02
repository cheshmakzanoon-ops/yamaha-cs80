#pragma once

// Registered in state-tree-synchronization.json before evaluation. These
// worker/host tests use joins and nonblocking lock witnesses, never a timed
// sleep to infer exclusion. They do not render audio. The final fixture drives
// the real MSEG editor directly, with no message-loop or timer flush.
#include "plugin/PluginProcessor.h"
#include "plugin/ModernModulationUi.h"
#include "Ws1Tolerances.h"

#include <bit>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <semaphore>
#include <thread>
#include <type_traits>

namespace cs80::tests
{
struct ModernLayerStateTestAccess
{
    template <class Reader>
    static void read(ModernLayerState& state, Reader&& reader)
    {
        state.reloadFromTreeUsing(&reader, [](void* context, const juce::ValueTree& tree) {
            return (*static_cast<std::remove_reference_t<Reader>*>(context))(tree);
        });
    }
};

struct UndoableStateHistoryTestAccess
{
    template <class Reader>
    static UndoableParameterState::HistorySnapshot read(const UndoableParameterState& state, Reader&& reader)
    {
        return state.readHistoryUsing(&reader, [](void* context, const juce::UndoManager& history) {
            return (*static_cast<std::remove_reference_t<Reader>*>(context))(history);
        });
    }
    template <class Writer>
    static void open(UndoableParameterState& state, const juce::String& name, Writer&& writer)
    {
        state.openTransactionUsing(name, &writer, [](void* context, juce::UndoManager& history, const juce::String& title) {
            (*static_cast<std::remove_reference_t<Writer>*>(context))(history, title);
        });
    }
};

namespace state_tree_detail
{
using Check = std::function<void(bool, const char*)>;

struct GestureProbe final : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0, changes = 0, depth = 0, malformed = 0;
    void parameterValueChanged(int, float) override
    { ++changes; if (depth != 1) ++malformed; }
    void parameterGestureChanged(int, bool starting) override
    {
        if (starting) { if (depth != 0) ++malformed; ++depth; ++begins; }
        else { if (depth != 1) ++malformed; --depth; ++ends; }
    }
    bool replay(const ws1::Tolerances& limits) const
    {
        return begins == limits["required_begin_gestures_per_changed_parameter_replay"]
            && ends == limits["required_end_gestures_per_changed_parameter_replay"]
            && changes > 0 && depth == 0 && malformed == 0;
    }
};

inline bool otherThreadIsExcluded(juce::CriticalSection& lock)
{
    bool entered = false;
    std::thread probe([&] { entered = lock.tryEnter(); if (entered) lock.exit(); });
    probe.join();
    return !entered;
}

class HistoryClearWitness final : public juce::UndoableAction
{
public:
    explicit HistoryClearWitness(std::function<void()> witness) : onDestruction(std::move(witness)) {}
    ~HistoryClearWitness() override { onDestruction(); }
    bool perform() override { return true; }
    bool undo() override { return true; }

private:
    std::function<void()> onDestruction;
};

class TreeWriteWitness final : private juce::ValueTree::Listener
{
public:
    explicit TreeWriteWitness(UndoableParameterState& s) : state(s)
    {
        const juce::ScopedLock guard(state.treeLock());
        state.state.addListener(this);
    }
    ~TreeWriteWitness() override
    {
        const juce::ScopedLock guard(state.treeLock());
        state.state.removeListener(this);
    }
    int calls = 0, unprotected = 0;

private:
    static bool modernTree(juce::ValueTree tree)
    {
        for (; tree.isValid(); tree = tree.getParent())
            if (tree.hasType(ModernLayerState::treeType)) return true;
        return false;
    }
    void observe()
    {
        ++calls;
        if (!otherThreadIsExcluded(state.treeLock())) ++unprotected;
    }
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier&) override
    { if (modernTree(tree)) observe(); }
    void valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) override
    { if (modernTree(parent) || modernTree(child)) observe(); }
    void valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int) override
    { if (modernTree(parent) || modernTree(child)) observe(); }
    void valueTreeChildOrderChanged(juce::ValueTree& parent, int, int) override
    { if (modernTree(parent)) observe(); }
    UndoableParameterState& state;
};

class WorkerWriteRendezvous final : private juce::ValueTree::Listener
{
public:
    WorkerWriteRendezvous(UndoableParameterState& s, juce::String parameterId)
        : state(s), id(std::move(parameterId))
    {
        const juce::ScopedLock guard(state.treeLock());
        state.state.addListener(this);
    }
    ~WorkerWriteRendezvous() override
    {
        const juce::ScopedLock guard(state.treeLock());
        state.state.removeListener(this);
    }
    void workerCompleted()
    {
        // A mutation that omits synchronization must fail, not deadlock waiting
        // for a callback that cannot arrive.
        if (!observed.load()) reached.release();
    }
    bool waitForWrite() { reached.acquire(); return observed.load(); }
    void releaseWrite() { if (observed.load()) resume.release(); }

private:
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override
    {
        const auto* mm = juce::MessageManager::getInstanceWithoutCreating();
        if (property == juce::Identifier("value") && tree.getProperty("id").toString() == id
            && mm != nullptr && !mm->isThisTheMessageThread() && !observed.exchange(true))
        {
            reached.release();
            resume.acquire();
        }
    }
    UndoableParameterState& state;
    const juce::String id;
    std::atomic<bool> observed { false };
    std::binary_semaphore reached { 0 }, resume { 0 };
};

inline juce::ValueTree decoded(const juce::MemoryBlock& bytes)
{
    if (auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(
            static_cast<const char*>(bytes.getData()), static_cast<int>(bytes.getSize()))))
        return juce::ValueTree::fromXml(*xml);
    return {};
}

inline float savedValue(const juce::ValueTree& snapshot, const juce::String& id)
{
    const auto child = snapshot.getChildWithProperty("id", id);
    return child.isValid() ? static_cast<float>(child.getProperty("value"))
                           : std::numeric_limits<float>::quiet_NaN();
}

inline void gesture(juce::RangedAudioParameter& parameter, float value)
{
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost(value);
    parameter.endChangeGesture();
}

inline juce::MouseEvent mouse(juce::Component& component, juce::Point<float> position, juce::Point<float> down)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        &component, &component, now, down, now, 1, position != down);
}

inline modern::ModernConfig configuration(bool alternate)
{
    modern::ModernConfig value;
    auto& shape = value.msegs[0];
    shape = {};
    shape.count = 4;
    shape.points[0] = { 0.0f, 0.0f, -0.5f };
    shape.points[1] = { 0.125f, alternate ? 0.5f : 0.75f, 0.25f };
    shape.points[2] = { 0.5f, 0.25f, 0.0f };
    shape.points[3] = { 1.0f, 0.0f, 0.0f };
    shape.loopStart = 1; shape.loopEnd = 2;
    shape.loop = alternate ? modern::MsegLoop::always : modern::MsegLoop::sustain;
    shape.trigger = alternate ? modern::MsegTrigger::free : modern::MsegTrigger::mono;
    shape.sync = !alternate; shape.bipolar = !alternate;
    value.slots[0] = { modern::ModSource::mseg1, modern::ModSource::velocity,
        alternate ? modern::ModDestination::level : modern::ModDestination::lpCutoff,
        modern::ModTarget::rankI };
    value.slots[15] = { modern::ModSource::constant, modern::ModSource::polyPressure,
        modern::ModDestination::pitch, modern::ModTarget::rankII };
    return modern::sanitizeConfig(value);
}
} // namespace state_tree_detail

inline void testStateTreeSynchronization(const state_tree_detail::Check& check)
{
    using namespace state_tree_detail;
    const auto contract = juce::File(CS80_GOLDEN_DIR).getParentDirectory()
        .getChildFile("parity/tolerances-v2/state-tree-synchronization.json");
    const ws1::Tolerances limits(contract.getFullPathName().toStdString(), "state_tree_synchronization");
    const auto ownershipContract = contract.getSiblingFile("state-tree-recording-ownership.json");
    const ws1::Tolerances ownership(ownershipContract.getFullPathName().toStdString(), "state_tree_recording_ownership");
    const auto historyContract = contract.getSiblingFile("state-tree-history-lock.json");
    const ws1::Tolerances historyLimits(historyContract.getFullPathName().toStdString(), "state_tree_history_lock");
    const auto clearContract = contract.getSiblingFile("state-tree-history-clear.json");
    const ws1::Tolerances clearLimits(clearContract.getFullPathName().toStdString(), "state_tree_history_clear");
    const auto close = [&](double a, double b, const char* key) { return std::abs(a - b) <= limits[key]; };

    // No timer or GUI dispatch occurs between the worker automation and save.
    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        auto& state = host->getUndoableState();
        auto& history = host->getUndoManager();
        auto* edited = state.getParameter(Cs80AudioProcessor::parameterId(P_volume));
        auto* automated = state.getParameter(Cs80AudioProcessor::parameterId(P_mix));
        const float original = edited->getValue();
        gesture(*edited, 0.625f);
        const int actions = history.getNumActionsInCurrentTransaction();
        juce::MemoryBlock bytes;
        float latest = 0.0f;
        std::thread worker([&] {
            automated->setValueNotifyingHost(0.25f);
            latest = host->getParamValue(P_mix);
            host->getStateInformation(bytes);
        });
        worker.join();
        check(close(savedValue(decoded(bytes), automated->getParameterID()), latest,
                    "maximum_snapshot_parameter_difference"),
              "worker snapshot immediately includes automation without a message-thread flush");
        check(history.getNumActionsInCurrentTransaction() - actions <= limits["maximum_unexpected_undo_actions"],
              "worker snapshot adds no automation or generic property undo action");
        GestureProbe editProbe, automationProbe;
        edited->addListener(&editProbe); automated->addListener(&automationProbe);
        check(host->undo() && edited->getValue() == original, "worker snapshot leaves the original user undo intact");
        check(close(host->getParamValue(P_mix), latest, "maximum_unrelated_parameter_difference_after_undo"),
              "undo after worker snapshot preserves the unrelated automated value");
        check(editProbe.replay(limits) && automationProbe.begins == 0 && automationProbe.ends == 0 && automationProbe.changes == 0,
              "undo after worker snapshot brackets only the user-edited parameter");
        editProbe = {}; automationProbe = {};
        check(host->redo() && editProbe.replay(limits) && automationProbe.begins == 0 && automationProbe.changes == 0,
              "redo after worker snapshot retains exact host gesture ownership");
        edited->removeListener(&editProbe); automated->removeListener(&automationProbe);
    }
    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        auto& state = host->getUndoableState();
        auto* parameter = state.getParameter(Cs80AudioProcessor::parameterId(P_mix));
        float treeValue = 0.0f, parameterValue = 0.0f;
        std::thread worker([&] {
            parameter->setValueNotifyingHost(0.375f);
            parameterValue = host->getParamValue(P_mix);
            const juce::ScopedLock treeLock(state.treeLock());
            state.syncNow();
            treeValue = savedValue(state.state, parameter->getParameterID());
        });
        worker.join();
        check(close(treeValue, parameterValue, "maximum_snapshot_parameter_difference"),
              "worker syncNow reconciles external values into the live tree");
        check(!host->getUndoManager().canUndo(), "worker external synchronization never records user history");
    }
    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        auto& state = host->getUndoableState();
        auto& history = host->getUndoManager();
        auto* parameter = state.getParameter(Cs80AudioProcessor::parameterId(P_volume));
        const float original = parameter->getValue();
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(0.625f);
        const float current = host->getParamValue(P_volume);
        const int actions = history.getNumActionsInCurrentTransaction();
        juce::MemoryBlock bytes;
        std::thread worker([&] { host->getStateInformation(bytes); });
        worker.join();
        check(close(savedValue(decoded(bytes), parameter->getParameterID()), current,
                    "maximum_snapshot_parameter_difference"),
              "worker snapshot overlays the current pending user gesture");
        check(history.getNumActionsInCurrentTransaction() - actions <= limits["maximum_unexpected_undo_actions"],
              "worker snapshot does not flush a pending user gesture into history");
        parameter->endChangeGesture();
        check(history.getUndoDescriptions().size() == 1, "pending user gesture still becomes one transaction at its end");
        GestureProbe probe;
        parameter->addListener(&probe);
        check(host->undo() && parameter->getValue() == original && probe.replay(limits),
              "pending gesture captured by worker retains parameter-aware undo gestures");
        probe = {};
        check(host->redo() && probe.replay(limits), "pending gesture captured by worker retains parameter-aware redo gestures");
        parameter->removeListener(&probe);
    }
    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        auto& state = host->getUndoableState();
        auto& modern = host->getModernLayer();
        TreeWriteWitness witness(state);
        const auto first = configuration(false), second = configuration(true);
        std::thread worker([&] { modern.setConfig(first); });
        worker.join();
        check(witness.calls > 0 && witness.unprotected == 0,
              "modern child insertion owns the common APVTS tree lock during actual callbacks");
        check(!host->getUndoManager().canUndo(), "worker modern configuration is external to user history");
        const int inserted = witness.calls;
        {
            UndoableParameterState::ScopedUserEdit edit(state, "Modern routing");
            modern.setConfig(second);
        }
        check(witness.calls > inserted && witness.unprotected == 0,
              "modern child replacement owns the common APVTS tree lock during actual callbacks");
        bool readObserved = false, readProtected = false;
        ModernLayerStateTestAccess::read(modern, [&](const juce::ValueTree& tree) {
            readObserved = true;
            readProtected = otherThreadIsExcluded(state.treeLock());
            return ModernLayerState::fromTree(tree);
        });
        check(readObserved && readProtected, "production modern tree reader owns the common APVTS tree lock");
        const int beforeUndo = witness.calls;
        check(host->undo() && modern.getConfig() == first, "modern configuration undo restores the exact prior structure");
        check(witness.calls > beforeUndo && witness.unprotected == 0,
              "modern subtree undo holds the common lock across its actual mutation callbacks");
        const int beforeRedo = witness.calls;
        check(host->redo() && modern.getConfig() == second, "modern configuration redo restores the exact edited structure");
        check(witness.calls > beforeRedo && witness.unprotected == 0,
              "modern subtree redo holds the common lock across its actual mutation callbacks");
        host->setParamValue(P_mix, 0.375f);
        juce::MemoryBlock bytes;
        std::thread saver([&] { host->getStateInformation(bytes); });
        saver.join();
        auto restored = std::make_unique<Cs80AudioProcessor>();
        std::thread loader([&] { restored->setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize())); });
        loader.join();
        check(restored->getModernLayer().getConfig() == second,
              "worker snapshot and restore preserve exact MSEG fields and matrix routing");
        check(close(restored->getParamValue(P_mix), host->getParamValue(P_mix), "maximum_snapshot_parameter_difference"),
              "modern snapshot roundtrip preserves unrelated host parameters");
        check(!restored->getUndoManager().canUndo(), "worker state restore starts with no stale user history");
    }
    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        auto& state = host->getUndoableState();
        auto* edited = state.getParameter(Cs80AudioProcessor::parameterId(P_volume));
        auto* automated = state.getParameter(Cs80AudioProcessor::parameterId(P_mix));
        const float original = edited->getValue();
        edited->beginChangeGesture();
        edited->setValueNotifyingHost(0.625f);
        state.syncNow(); // record and clear the first pending user update
        WorkerWriteRendezvous rendezvous(state, automated->getParameterID());
        std::thread worker([&] {
            automated->setValueNotifyingHost(0.25f);
            state.syncNow();
            rendezvous.workerCompleted();
        });
        const bool pausedInsideWrite = rendezvous.waitForWrite();
        if (pausedInsideWrite) edited->setValueNotifyingHost(0.875f);
        rendezvous.releaseWrite();
        worker.join();
        edited->endChangeGesture();
        check(pausedInsideWrite, "concurrent user edit rendezvous observes the actual worker tree write");
        GestureProbe probe;
        edited->addListener(&probe);
        check(host->undo() && edited->getValue() == original && probe.replay(ownership),
              "concurrent user edit retains the original undo value and host gestures");
        probe = {};
        check(host->redo() && std::abs(edited->getValue() - 0.875f) <= ownership["maximum_lost_user_value_difference"]
                  && probe.replay(ownership),
              "worker tree write never suppresses the final concurrent user edit on redo");
        check(automated->getValue() == 0.25f, "concurrent user undo and redo preserve the unrelated worker value");
        edited->removeListener(&probe);
    }
    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        auto& state = host->getUndoableState();
        constexpr int parameters[] = { P_volume, P_tuningRef, P_portamento, P_subSpeed, P_ringSpeed,
            P_ch1Start + CP_hpfCutoff, P_ch1Start + CP_lpfCutoff,
            P_ch2Start + CP_hpfCutoff, P_ch2Start + CP_lpfCutoff };
        double liveDifference = 0.0, snapshotDifference = 0.0, parameterDifference = 0.0;
        int notifications = 0, cases = 0, roundtripDifferences = 0;
        bool physicalParametersPresent = true;
        const auto difference = [](float a, float b) {
            return std::isfinite(a) && std::isfinite(b)
                ? std::abs(static_cast<double>(a) - static_cast<double>(b))
                : std::numeric_limits<double>::infinity();
        };
        std::thread worker([&] {
            for (const int index : parameters)
            {
                auto* parameter = state.getParameter(Cs80AudioProcessor::parameterId(index));
                auto* physicalParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);
                auto* raw = state.getRawParameterValue(parameter->getParameterID());
                if (physicalParameter == nullptr || raw == nullptr)
                { physicalParametersPresent = false; continue; }
                GestureProbe probe;
                parameter->addListener(&probe);
                for (int i = 1; i < 64; ++i)
                {
                    const float centre = static_cast<float>(i) / 64.0f;
                    for (const float value : { centre, std::nextafter(centre, 0.0f), std::nextafter(centre, 1.0f) })
                    {
                        parameter->setValueNotifyingHost(value);
                        const float normalBefore = parameter->getValue();
                        const float rawBefore = raw->load();
                        const float physicalBefore = physicalParameter->get();
                        roundtripDifferences += parameter->convertFrom0to1(normalBefore) != rawBefore;
                        const int notificationsBefore = probe.changes;
                        {
                            const juce::ScopedLock treeLock(state.treeLock());
                            state.syncNow();
                            liveDifference = std::max(liveDifference,
                                difference(savedValue(state.state, parameter->getParameterID()), rawBefore));
                            const auto snapshot = state.copyState();
                            snapshotDifference = std::max(snapshotDifference,
                                difference(savedValue(snapshot, parameter->getParameterID()), rawBefore));
                        }
                        parameterDifference = std::max({ parameterDifference,
                            difference(parameter->getValue(), normalBefore),
                            difference(raw->load(), rawBefore),
                            difference(physicalParameter->get(), physicalBefore) });
                        notifications += probe.changes - notificationsBefore;
                        ++cases;
                    }
                }
                parameter->removeListener(&probe);
            }
        });
        worker.join();
        check(physicalParametersPresent && cases == 1701, "exact host-value matrix covers all registered legal setters");
        check(liveDifference <= ownership["maximum_live_tree_physical_value_difference"],
              "worker synchronization preserves the exact APVTS physical value in the live tree");
        check(snapshotDifference <= ownership["maximum_snapshot_physical_value_difference"],
              "worker snapshots preserve the exact APVTS physical value across nonlinear ranges");
        check(parameterDifference <= ownership["maximum_sync_parameter_side_effect_difference"],
              "worker synchronization and saving never change a legal host parameter value");
        check(notifications <= ownership["maximum_sync_value_notifications"],
              "worker synchronization and saving emit no extra parameter value notifications");
        std::cout << "State tree exact host-value matrix: " << cases << " cases; " << roundtripDifferences
                  << " normalized-roundtrip/APVTS-atomic differences before synchronization\n";
    }
    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        auto& state = host->getUndoableState();
        int observations = 0, unprotected = 0, mismatches = 0;
        const auto readFields = [](const juce::UndoManager& manager) {
            return UndoableParameterState::HistorySnapshot { manager.canUndo(), manager.canRedo(),
                manager.getUndoDescription(), manager.getRedoDescription() };
        };
        const auto verifySnapshot = [&] {
            const auto witnessed = UndoableStateHistoryTestAccess::read(state, [&](const juce::UndoManager& manager) {
                ++observations;
                if (!otherThreadIsExcluded(state.treeLock())) ++unprotected;
                return readFields(manager);
            });
            const auto snapshot = state.getHistorySnapshot();
            const juce::ScopedLock treeLock(state.treeLock());
            mismatches += snapshot != readFields(host->getUndoManager());
            mismatches += snapshot != witnessed;
        };
        verifySnapshot();
        auto* parameter = state.getParameter(Cs80AudioProcessor::parameterId(P_volume));
        gesture(*parameter, 0.625f);
        verifySnapshot();
        check(host->undo(), "history snapshot fixture undoes a real user edit");
        verifySnapshot();
        check(host->redo(), "history snapshot fixture redoes a real user edit");
        verifySnapshot();
        check(observations == 4 && unprotected <= historyLimits["maximum_unprotected_history_reads"],
              "production history snapshot reader owns the common APVTS lock");
        check(mismatches <= historyLimits["maximum_history_snapshot_field_mismatches"],
              "detached history snapshots preserve all four toolbar fields exactly");
    }
    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        auto& state = host->getUndoableState();
        int observations = 0, unprotected = 0;
        const juce::String title("Registered transaction witness");
        UndoableStateHistoryTestAccess::open(state, title, [&](juce::UndoManager& manager, const juce::String& name) {
            ++observations;
            if (!otherThreadIsExcluded(state.treeLock())) ++unprotected;
            manager.beginNewTransaction(name);
        });
        check(observations == 1 && unprotected <= historyLimits["maximum_unprotected_transaction_opens"],
              "production user transaction open owns the common APVTS lock");
        const juce::ScopedLock treeLock(state.treeLock());
        check(host->getUndoManager().getCurrentTransactionName() == title,
              "transaction writer witness opens the real requested undo transaction");
    }
    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        auto& state = host->getUndoableState();
        auto& history = host->getUndoManager();
        int observations = 0, unprotected = 0;
        {
            const juce::ScopedLock treeLock(state.treeLock());
            history.beginNewTransaction("Factory clear witness");
            check(history.perform(new HistoryClearWitness([&] {
                ++observations;
                if (!otherThreadIsExcluded(state.treeLock())) ++unprotected;
            })), "factory-clear fixture records the real destructor-witness action");
            check(observations == 0 && history.canUndo(),
                  "factory-clear witness remains alive until the production factory load");
        }
        host->loadFactoryPreset(0);
        check(observations == 1 && unprotected <= clearLimits["maximum_unprotected_factory_history_clears"],
              "factory load owns the common APVTS lock during actual history action destruction");
        const auto snapshot = state.getHistorySnapshot();
        check(!snapshot.canUndo && !snapshot.canRedo && snapshot.undoDescription.isEmpty() && snapshot.redoDescription.isEmpty(),
              "factory load clears the actual undo and redo history");
        // Keep the witness's captured counters alive even if a future mutation
        // omits clearing altogether. Cleanup cannot satisfy the earlier checks.
        const juce::ScopedLock treeLock(state.treeLock());
        history.clearUndoHistory();
    }
    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        auto& state = host->getUndoableState();
        auto* edited = state.getParameter(Cs80AudioProcessor::parameterId(P_volume));
        auto* automated = state.getParameter(Cs80AudioProcessor::parameterId(P_mix));
        const float original = edited->getValue();
        gesture(*edited, 0.625f);
        const float userValue = edited->getValue();
        automated->setValueNotifyingHost(0.25f);
        const float externalValue = host->getParamValue(P_mix);
        {
            const juce::ScopedLock treeLock(state.treeLock());
            check(savedValue(state.state, automated->getParameterID()) != externalValue,
                  "MSEG fixture starts with an external value pending in the live tree");
        }
        ui::modern_layer::MsegEditor editor(*host);
        editor.setSize(640, 240);
        const auto before = host->getModernLayer().getShape(0);
        const auto start = editor.pointPosition(1);
        const auto finish = start + juce::Point<float>(30.0f, 20.0f);
        editor.mouseDown(mouse(editor, start, start));
        {
            const juce::ScopedLock treeLock(state.treeLock());
            check(std::abs(savedValue(state.state, automated->getParameterID()) - externalValue)
                      <= clearLimits["maximum_mseg_preceding_external_value_difference"],
                  "MSEG mouse-down synchronizes a pending external value before the first drag");
        }
        editor.mouseDrag(mouse(editor, (start + finish) * 0.5f, start));
        editor.mouseDrag(mouse(editor, finish, start));
        editor.mouseUp(mouse(editor, finish, start));
        const auto after = host->getModernLayer().getShape(0);
        {
            const juce::ScopedLock treeLock(state.treeLock());
            check(after != before && host->getUndoManager().getUndoDescriptions().size() == 2
                      && host->getUndoManager().getUndoDescription() == "MSEG edit",
                  "real MSEG drag records one named transaction after the earlier user edit");
        }
        check(host->undo() && host->getModernLayer().getShape(0) == before && edited->getValue() == userValue,
              "MSEG undo restores the exact shape while preserving the earlier user edit");
        check(host->redo() && host->getModernLayer().getShape(0) == after && edited->getValue() == userValue,
              "MSEG redo restores the exact edited shape while preserving the earlier user edit");
        check(host->getParamValue(P_mix) == externalValue,
              "MSEG undo and redo preserve the external parameter value");
        check(host->undo() && host->undo() && edited->getValue() == original
                  && host->getParamValue(P_mix) == externalValue,
              "the preceding user transaction remains independently undoable after an MSEG drag");
    }
    {
        // Additional admitted inputs for the existing exact MSEG round-trip
        // requirement. No tolerance is changed: serialized fields retain their
        // actual float bits, including small nonzero coordinates and boundaries.
        const float subnormal = std::numeric_limits<float>::denorm_min();
        const float normal = std::numeric_limits<float>::min();
        const std::array<modern::MsegPoint, 14> points {{
            { 0.0f, 0.0f, 0.0f },
            { subnormal, subnormal, -subnormal },
            { std::nextafter(subnormal, 1.0f), std::nextafter(subnormal, 1.0f), subnormal },
            { std::nextafter(normal, 0.0f), normal, -normal },
            { normal, std::nextafter(normal, 0.0f), std::nextafter(normal, 0.0f) },
            { 1.0e-30f, 1.0e-30f, -1.0e-30f },
            { 1.0e-12f, 1.0e-12f, -1.0e-12f },
            { 1.0e-7f, 1.0e-7f, -1.0e-7f },
            { std::nextafter(0.08f, 1.0f), std::nextafter(0.6f, 1.0f), std::nextafter(0.6f, -1.0f) },
            { std::bit_cast<float>(0x3e06a3b5u), std::bit_cast<float>(0x3f64c416u), 0.6f },
            { std::nextafter(1.0f, 0.0f), std::nextafter(1.0f, 0.0f), std::nextafter(-1.0f, 0.0f) },
            { 1.0f, 1.0f, -1.0f },
            { std::nextafter(modern::kMsegMaxTime, 0.0f), std::nextafter(1.0f, 0.0f), std::nextafter(1.0f, 0.0f) },
            { modern::kMsegMaxTime, 1.0f, 1.0f },
        }};
        auto host = std::make_unique<Cs80AudioProcessor>();
        auto restored = std::make_unique<Cs80AudioProcessor>();
        int admittedMismatches = 0, roundtripMismatches = 0, subnormalFields = 0;
        const auto samePointBits = [](const modern::MsegPoint& a, const modern::MsegPoint& b) {
            return std::bit_cast<std::uint32_t>(a.time) == std::bit_cast<std::uint32_t>(b.time)
                && std::bit_cast<std::uint32_t>(a.level) == std::bit_cast<std::uint32_t>(b.level)
                && std::bit_cast<std::uint32_t>(a.curve) == std::bit_cast<std::uint32_t>(b.curve);
        };
        std::thread worker([&] {
            for (const auto point : points)
            {
                auto expected = configuration(false);
                expected.msegs[0].points[1] = point;
                expected.msegs[0].points[2].time = std::max(1.0f, point.time);
                expected.msegs[0].points[3].time = modern::kMsegMaxTime;
                expected = modern::sanitizeConfig(expected);
                admittedMismatches += !samePointBits(expected.msegs[0].points[1], point);
                for (const float value : { point.time, point.level, point.curve })
                    subnormalFields += std::fpclassify(value) == FP_SUBNORMAL;
                host->getModernLayer().setConfig(expected);
                juce::MemoryBlock bytes;
                host->getStateInformation(bytes);
                restored->setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
                const auto actual = restored->getModernLayer().getConfig();
                roundtripMismatches += actual != expected;
                for (int m = 0; m < modern::kMsegCount; ++m)
                    for (int p = 0; p < modern::kMsegMaxPoints; ++p)
                        roundtripMismatches += !samePointBits(actual.msegs[(size_t) m].points[(size_t) p],
                                                           expected.msegs[(size_t) m].points[(size_t) p]);
            }
        });
        worker.join();
        check(admittedMismatches == 0 && subnormalFields > 0,
              "MSEG precision fixture uses model-admitted subnormal and boundary coordinates");
        check(roundtripMismatches == 0,
              "worker MSEG snapshots preserve exact float bits at subnormal and legal boundaries");
        std::cout << "State tree MSEG precision matrix: " << points.size() << " cases; " << subnormalFields
                  << " admitted subnormal fields; " << roundtripMismatches << " round-trip mismatches\n";
    }
}
} // namespace cs80::tests
