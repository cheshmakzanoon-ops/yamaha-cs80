#pragma once

#include "core/Params.h"
#include "PresetOrigin.h"
#include "core/ModernLayer.h"
#include <juce_core/juce_core.h>
#include <array>

namespace cs80
{
// The library's opening sound, not a session/undo-tree snapshot. All arrays
// are owned values, including the three MSEG shapes and sixteen matrix routes.
// Physical setup, PURE, tuning data, MIDI maps, UI state and A/B slots stay live.
struct PresetBrowserSound
{
    ParamValues values;
    modern::ModernConfig modern;
    std::array<int, 2> sources { 13, 13 };
    std::array<std::array<float, CP_count>, 4> memories {};
    juce::String name;
    int factoryIndex = 0;
    PresetOrigin origin; // Owned runtime provenance; never serialized.
};
} // namespace cs80
