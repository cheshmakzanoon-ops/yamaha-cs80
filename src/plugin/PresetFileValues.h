#pragma once

#include "core/Params.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <unordered_map>
#include <vector>

namespace cs80::ui
{
// Read the browser's preview/edit-reference values without creating a
// processor. Only accepted preset documents may supply parameter values.
// Missing controls retain defaults; the last matching child is authoritative,
// including a child whose value is missing or non-finite. Retired policy slots
// always resolve to the circuit instrument, matching processor migration. This
// preview reader does not replace the processor's full schema migration.
inline void readPresetFileValues(const juce::XmlElement* xml, ParamValues& values,
                                 std::vector<char>* used = nullptr)
{
    values = {};
    values[P_sourcePerformance] = 1.0f;
    values[P_parityMode] = 0.0f;
    if (used != nullptr)
        used->assign((size_t) P_numParams, 0);
    if (xml == nullptr || xml->getTagName() != "CS80_PARAMS")
        return;

    static const auto ids = []
    {
        std::unordered_map<std::string, int> map;
        for (int i = 0; i < P_numParams; ++i)
            map.emplace(paramId(i), i);
        return map;
    }();
    for (auto* child : xml->getChildIterator())
    {
        const auto found = ids.find(child->getStringAttribute("id").toStdString());
        if (found == ids.end())
            continue;
        const auto index = found->second;
        const auto& def = paramDef(index);
        // Original SUB's restored fallback predates its fresh-instance default.
        const float fallback = index == P_subOriginalMode ? 0.0f : def.def;
        const float value = (float) child->getDoubleAttribute("value", fallback);
        // Match restore's fallback before clamping. NaN survives clampValue,
        // and an infinity clamped to a range edge misrepresents the loaded sound.
        values[index] = std::isfinite(value) ? clampValue(value, def.min, def.max) : fallback;
        if (used != nullptr)
            (*used)[(size_t) index] = 1;
    }
    // These host addresses remain readable for older documents; they no longer
    // select a different sound engine or suppress performance controls.
    values[P_sourcePerformance] = 1.0f;
    values[P_parityMode] = 0.0f;
}
} // namespace cs80::ui
