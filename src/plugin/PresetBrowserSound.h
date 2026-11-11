#pragma once

#include "core/Params.h"
#include <juce_core/juce_core.h>
#include <array>

namespace cs80
{
// The library's opening parameter sound, not a session/undo-tree snapshot.
// Physical setup and PURE are ignored on comparison/recall. Modern modulation
// shapes/routes, tuning data, MIDI maps, UI state and A/B slots are not recalled.
struct PresetBrowserSound
{
    ParamValues values;
    std::array<int, 2> sources { 13, 13 };
    std::array<std::array<float, CP_count>, 4> memories {};
    juce::String name;
    int factoryIndex = 0;
};
} // namespace cs80
