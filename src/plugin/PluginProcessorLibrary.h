#pragma once

// [ws6-library] Processor-side state for the program library: MIDI Program
// Change and bank select, the A/B compare slots and the host-display notifier.
// Only PluginProcessor*.cpp include this; see PluginProcessorLibrary.cpp.

#include "PluginProcessor.h"

#include <array>
#include <atomic>

struct Cs80AudioProcessor::LibraryState final : private juce::AsyncUpdater
{
    explicit LibraryState(Cs80AudioProcessor& p) : owner(p) {}
    ~LibraryState() override { cancelPendingUpdate(); }

    // Program change: notify the host (program list/name) from the message thread.
    void notifyProgramChanged() { triggerAsyncUpdate(); }

    Cs80AudioProcessor& owner;

    // Parameter objects resolved once, so the audio thread never builds IDs.
    std::array<juce::RangedAudioParameter*, cs80::P_numParams> parameters {};
    std::array<juce::RangedAudioParameter*, 2> sources {};

    // Audio-thread bank select state (CC0 MSB, CC32 LSB), MIDI 1.0 semantics:
    // each byte holds until it is sent again.
    int bankMsb = 0, bankLsb = 0;
    // >= 0 while the sounding program was recalled by MIDI Program Change and
    // its name has not been replaced since; read by getPresetMetadata().
    std::atomic<int> midiProgram { -1 };

    struct Snapshot
    {
        bool stored = false;
        cs80::ParamValues values;
        std::array<int, 2> sources { 13, 13 };
        std::array<std::array<float, cs80::CP_count>, 4> memories {};
        juce::String name;
        int program = 0;
    };
    // Message thread and state-restore threads; never the audio thread.
    mutable juce::CriticalSection abLock;
    std::array<Snapshot, 2> ab;
    int abActive = 0;

private:
    void handleAsyncUpdate() override
    {
        owner.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withProgramChanged(true));
    }
};
