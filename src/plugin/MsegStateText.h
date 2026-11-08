#pragma once

#include "core/Mseg.h"
#include <juce_core/juce_core.h>

#include <limits>

namespace cs80::detail
{
// Locale-independent, exact Float32 round trips. Fixed six-decimal text loses
// ordinary UI coordinates and can collapse a short loop into a sustain point.
// Scientific notation keeps max_digits10 significant digits even for subnormal
// times/levels; JUCE's existing reader already accepts this format.
inline juce::String encodeMsegPoints(const modern::MsegShape& s)
{
    juce::StringArray items;
    for (int i = 0; i < s.count; ++i)
    {
        const auto& p = s.points[(std::size_t) i];
        constexpr int places = std::numeric_limits<float>::max_digits10 - 1;
        items.add(juce::String(p.time, places, true) + " " + juce::String(p.level, places, true)
                  + " " + juce::String(p.curve, places, true));
    }
    return items.joinIntoString(";");
}

inline void decodeMsegPoints(const juce::String& text, modern::MsegShape& s)
{
    juce::StringArray items;
    items.addTokens(text, ";", {});
    items.removeEmptyStrings();
    if (items.size() < 2)
        return; // keep the default shape
    s.count = std::min(items.size(), modern::kMsegMaxPoints);
    for (int i = 0; i < s.count; ++i)
    {
        juce::StringArray fields;
        fields.addTokens(items[i], " ", {});
        fields.removeEmptyStrings();
        auto& p = s.points[(std::size_t) i];
        p.time = fields.size() > 0 ? fields[0].getFloatValue() : 0.0f;
        p.level = fields.size() > 1 ? fields[1].getFloatValue() : 0.0f;
        p.curve = fields.size() > 2 ? fields[2].getFloatValue() : 0.0f;
    }
}

} // namespace cs80::detail
