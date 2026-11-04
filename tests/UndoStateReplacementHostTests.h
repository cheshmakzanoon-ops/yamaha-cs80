#pragma once

// Headless processor regression for the two ValueTree replacement paths. The
// source-level lock/sync ordering is asserted separately by test_apvts_state_lock.py.

#include "plugin/PluginProcessor.h"

#include <cmath>

namespace cs80::tests
{
struct UndoReplayGestureProbe final : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0, changes = 0, depth = 0, unbracketed = 0;
    void parameterValueChanged(int, float) override
    {
        ++changes;
        if (depth != 1) ++unbracketed;
    }
    void parameterGestureChanged(int, bool starting) override
    {
        starting ? (++begins, ++depth) : (++ends, --depth);
    }
    bool exactlyOneCompleteGesture() const noexcept
    {
        return begins == 1 && ends == 1 && depth == 0 && changes > 0 && unbracketed == 0;
    }
    bool completelyUntouched() const noexcept
    {
        return begins == 0 && ends == 0 && changes == 0 && depth == 0;
    }
    void clear() noexcept { begins = ends = changes = depth = unbracketed = 0; }
};

template <typename Check>
void testUndoStateReplacementHost(Check check)
{
    juce::TemporaryFile root("-cs80-undo-replace-host");
    Cs80AudioProcessor host(root.getFile().getChildFile("presets"));
    host.prepareToPlay(48000.0, 128);
    juce::MessageManager::getInstance();

    host.setParamValue(P_mix, 0.23f);
    juce::MemoryBlock state;
    host.getStateInformation(state);
    const auto saved = host.saveUserPreset("Undo state replacement", Cs80AudioProcessor::PresetOverwrite::no);
    check(saved.wasSaved() && state.getSize() > 0, "headless fixture saved a valid session and user preset");
    if (!saved.wasSaved() || state.getSize() == 0) return;

    auto* mix = host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(P_mix));
    check(mix != nullptr, "mix parameter exposes a host gesture after state replacements");
    if (mix == nullptr) return;

    host.setParamValue(P_mix, 0.77f); // pending external value/tree synchronization
    host.setStateInformation(state.getData(), (int) state.getSize());
    host.getUndoableState().syncNow();
    check(std::abs(host.getParamValue(P_mix) - 0.23f) < 1e-6f,
          "host session restore atomically replaces pending parameter values");

    host.setParamValue(P_mix, 0.81f);
    host.loadUserPreset(saved.file);
    host.getUndoableState().syncNow();
    check(std::abs(host.getParamValue(P_mix) - 0.23f) < 1e-6f,
          "user-preset recall replaces pending parameter values without stale flush");

    // One user gesture after replacement belongs to the restored tree and
    // undoes to its restored value, not a transient pre-replacement edit.
    mix->beginChangeGesture();
    mix->setValueNotifyingHost(mix->convertTo0to1(0.49f));
    mix->endChangeGesture();
    host.getUndoableState().syncNow();
    check(std::abs(host.getParamValue(P_mix) - 0.49f) < 1e-6f,
          "post-restore host gesture updates the current parameter");
    check(host.undo() && std::abs(host.getParamValue(P_mix) - 0.23f) < 1e-6f,
          "undo after state replacement returns to the restored value");

    // UndoManager's ValueTree replay changes the host-facing parameter. The
    // host must observe begin/value/end in that order, exactly once, for the
    // changed parameter only. A matching begin/end emitted after undo() is
    // insufficient: the value callback would still have been unbracketed.
    auto* volume = host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(P_volume));
    check(volume != nullptr, "unchanged output-volume parameter can detect broadcast undo gestures");
    if (volume == nullptr) return;
    UndoReplayGestureProbe changed, unchanged;
    mix->addListener(&changed);
    volume->addListener(&unchanged);
    const auto redoPendingBefore = host.getUndoManager().getRedoDescriptions().size();
    const bool redone = host.redo();
    check(redone && std::abs(host.getParamValue(P_mix) - 0.49f) < 1e-6f,
          "redo of restored-state user edit applies exactly the edited mix value");
    check(changed.exactlyOneCompleteGesture() && unchanged.completelyUntouched(),
          "redo notifies host inside one balanced mix gesture without touching unrelated parameters");
    changed.clear();
    unchanged.clear();
    const bool undone = host.undo();
    check(undone && std::abs(host.getParamValue(P_mix) - 0.23f) < 1e-6f,
          "undo of replayed edit restores its original mix value");
    check(changed.exactlyOneCompleteGesture() && unchanged.completelyUntouched(),
          "undo notifies host inside one balanced mix gesture without touching unrelated parameters");
    check(host.getUndoManager().getRedoDescriptions().size() == redoPendingBefore,
          "host gesture replay never records itself as another undo transaction");
    volume->removeListener(&unchanged);
    mix->removeListener(&changed);
}
} // namespace cs80::tests
