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
// Undo and redo write the tree; effective parameter replays are bracketed by host edit gestures.
// The guarded entry points reject replay during a user edit, reentrant replay, or off the message
// thread. The UndoManager accessor is for queries and recording side-state actions, not replay.
//
// Thread safety: the tree is touched on the message thread (timer, gestures) and by the host's
// state calls; treeLock() serialises them (the processor takes it around copyStateForSave/replaceState).
// The parameter listener only stores atomics when it is called off the message thread.

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>
#include <vector>

namespace cs80
{

class UndoableParameterState final : public juce::AudioProcessorValueTreeState,
                                     private juce::AudioProcessorParameter::Listener
{
public:
    UndoableParameterState(juce::AudioProcessor& processor, juce::UndoManager& history,
                           const juce::Identifier& valueTreeType, ParameterLayout layout);
    ~UndoableParameterState() override;

    // Brings the tree up to date now: pending non-user changes without the UndoManager, pending
    // user edits into the current transaction. Message thread.
    void syncNow();
    // Audio-safe publication only. The existing message-thread timer delivers
    // the coalesced host-display notification after synchronising parameters.
    void requestProgramDisplayUpdate() noexcept
    {
        static_assert(std::atomic<bool>::is_always_lock_free);
        programDisplayPending.store(true, std::memory_order_release);
    }
    juce::CriticalSection& treeLock() noexcept { return lock; }

    // Message thread, outside any gesture/ScopedUserEdit. Unsafe calls are rejected before sync;
    // safe calls may sync pending tree state even when no undo/redo action is available.
    bool undo();
    bool redo();

    // Any non-audio thread: snapshot the latest parameter atomics without
    // JUCE copyState() flushing them through the live tree's UndoManager.
    // Reads are per parameter, not a transaction across concurrent automation.
    juce::ValueTree copyStateForSave();

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

    // Side-state edits join only an explicit message-thread ScopedUserEdit.
    // A direct API call or unrelated parameter gesture must not record them.
    bool hasActiveUserEdit() const;
    // UI admission only. Includes gestures opened before a component was constructed.
    bool hasActiveParameterGesture() const;

private:
    struct ParameterValueAction;
    bool replayHistory(bool redo);
    void beginUserEdit(const juce::String& name);
    void endUserEdit();
    void timerCallback() override;   // replaces the base class's flush (see the file comment)
    void parameterValueChanged(int index, float) override;
    void parameterGestureChanged(int index, bool starting) override;
    void openTransaction(const juce::String& name);
    void flushUser();
    void syncExternal();
    void write(int index, juce::UndoManager* undoManager);

    std::atomic<bool> programDisplayPending { false };
    juce::AudioProcessor& owner;
    juce::UndoManager& history;
    juce::CriticalSection lock;
    std::vector<juce::RangedAudioParameter*> parameters;   // by host parameter index
    std::unique_ptr<std::atomic<bool>[]> externalPending, userPending;
    int openGestures = 0, userScopes = 0;
    bool writing = false, groupOpen = false, replayingHistory = false;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoableParameterState)
};

} // namespace cs80
