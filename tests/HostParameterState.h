#pragma once

#include "plugin/PluginProcessor.h"

namespace cs80::tests
{
// Test snapshots obey the same APVTS protocol as host save/restore: synchronize
// pending parameter edits while holding the undo tree lock, then copy the tree.
// The returned tree is detached and may be edited without touching live state.
inline juce::ValueTree copyHostParameterState(Cs80AudioProcessor& host)
{
    auto& state = host.getUndoableState();
    const juce::ScopedLock treeLock(state.treeLock());
    state.syncNow();
    return host.getAPVTS().copyState();
}
} // namespace cs80::tests
