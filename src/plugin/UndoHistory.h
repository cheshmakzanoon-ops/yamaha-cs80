#pragma once

// Undo history for the plugin's parameters: an AudioProcessorValueTreeState constructed with a
// juce::UndoManager, recording one transaction per user gesture and nothing else.
//
// Why a subclass: the stock APVTS copies every parameter change into its ValueTree from a
// private timer, through the UndoManager it was given. Host automation and preset loads would
// then land in whatever transaction the last user gesture opened, and undo would replay them.
// This class takes over that timer (the only flush that runs on its own) and decides, per
// parameter change, whether it belongs to the history:
//
//   * user edit: a change made on the message thread while a host gesture is open (sliders,
//     buttons, combo boxes, the context menu and typed values all bracket their edits) or inside
//     a ScopedUserEdit. The gesture's first begin opens a transaction named after the parameter;
//     every gesture begun in the same message-loop pass joins it (one action that sets two
//     parameters is one step); the final values are written into the tree, through the
//     UndoManager, when the gesture ends.
//   * anything else: host automation (any thread), MIDI learn (audio thread), preset loads and
//     state restores, programmatic setters. Its value is written into the tree without the
//     UndoManager, so the history never sees it. Loading a sound clears the history, as
//     AudioProcessorValueTreeState::replaceState already does for state restores.
//
// Undo and redo use parameter-aware actions: only the affected parameter is bracketed in a
// host gesture before the APVTS turns the tree write into its value notification.
//
// Thread safety: the tree is touched on the message thread (timer, gestures) and by the host's
// state calls; treeLock() serialises them (the processor takes it around copyState/replaceState).
// The parameter listener only stores atomics when it is called off the message thread.

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>
#include <vector>

namespace cs80
{
namespace tests { struct UndoableStateHistoryTestAccess; }

class UndoableParameterState final : public juce::AudioProcessorValueTreeState,
                                     private juce::AudioProcessorParameter::Listener
{
public:
    UndoableParameterState(juce::AudioProcessor& processor, juce::UndoManager& history,
                           const juce::Identifier& valueTreeType, ParameterLayout layout);
    ~UndoableParameterState() override;

    // Any non-audio caller may reconcile external changes without the UndoManager.
    // Only the message thread records pending user edits in their transaction.
    void syncNow();
    juce::CriticalSection& treeLock() noexcept { return lock; }

    // A host may save while the message thread is blocked waiting for that host.
    // JUCE's base copyState flushes through its UndoManager. Copy the synchronized
    // tree and overlay current parameter atomics on the detached result instead;
    // a pending user gesture remains owned by the message thread.
    juce::ValueTree copyState();

    // A programmatic edit that the player should be able to undo in one step (the A/B switch,
    // a randomise, a copy between ranks). Message thread.
    class ScopedUserEdit
    {
    public:
        ScopedUserEdit(UndoableParameterState& s, const juce::String& name) : state(s) { state.beginUserEdit(name); }
        ~ScopedUserEdit() { state.endUserEdit(); }
        ScopedUserEdit(const ScopedUserEdit&) = delete;
        ScopedUserEdit& operator=(const ScopedUserEdit&) = delete;

    private:
        UndoableParameterState& state;
    };

    juce::UndoManager& getHistory() noexcept { return history; }

    struct HistorySnapshot
    {
        bool canUndo = false, canRedo = false;
        juce::String undoDescription, redoDescription;
        bool operator==(const HistorySnapshot&) const = default;
    };
    // Detached toolbar data: a worker restore may clear history concurrently.
    HistorySnapshot getHistorySnapshot() const;

private:
    class ParameterChangeAction;
    void beginUserEdit(const juce::String& name);
    void endUserEdit();
    void timerCallback() override;   // replaces the base class's flush (see the file comment)
    void parameterValueChanged(int index, float) override;
    void parameterGestureChanged(int index, bool starting) override;
    void openTransaction(const juce::String& name);
    using HistoryReader = HistorySnapshot (*)(void*, const juce::UndoManager&);
    using TransactionWriter = void (*)(void*, juce::UndoManager&, const juce::String&);
    HistorySnapshot readHistoryUsing(void* context, HistoryReader reader) const;
    void openTransactionUsing(const juce::String& name, void* context, TransactionWriter writer);
    friend struct tests::UndoableStateHistoryTestAccess;
    void flushUser();
    void syncExternal();
    void write(int index, juce::UndoManager* undoManager);

    juce::AudioProcessor& owner;
    juce::UndoManager& history;
    mutable juce::CriticalSection lock;
    std::vector<juce::RangedAudioParameter*> parameters;   // by host parameter index
    std::unique_ptr<std::atomic<bool>[]> externalPending, userPending;
    int openGestures = 0, userScopes = 0;
    bool writing = false, groupOpen = false;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoableParameterState)
};

} // namespace cs80
