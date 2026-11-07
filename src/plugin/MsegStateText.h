#pragma once

#include "core/Mseg.h"
#include <juce_core/juce_core.h>

#include <cmath>
#include <limits>

namespace cs80::detail
{
// Validate before JUCE's numeric reader: its exponent accumulator is an int.
// Shape sanitisation happens too late to prevent overflow while parsing XML.
inline float decodeMsegCoordinate(const juce::String& text)
{
    auto cursor = text.getCharPointer();
    if (*cursor == '+' || *cursor == '-') ++cursor;
    const auto digits = [&cursor]
    {
        bool found = false;
        while (*cursor >= '0' && *cursor <= '9') { found = true; ++cursor; }
        return found;
    };
    bool hasDigits = digits();
    if (*cursor == '.') { ++cursor; hasDigits = digits() || hasDigits; }
    if (!hasDigits) return 0.0f;
    if (*cursor == 'e' || *cursor == 'E')
    {
        ++cursor;
        if (*cursor == '+' || *cursor == '-') ++cursor;
        if (*cursor < '0' || *cursor > '9') return 0.0f;
        // Reserve space for the reader's mantissa exponent adjustment too.
        const int limit = std::numeric_limits<int>::max() - text.length();
        int exponent = 0;
        while (*cursor >= '0' && *cursor <= '9')
        {
            const int digit = (int) *cursor - '0';
            if (exponent > limit / 10 || (exponent == limit / 10 && digit > limit % 10))
                return 0.0f;
            exponent = exponent * 10 + digit;
            ++cursor;
        }
    }
    if (!cursor.isEmpty()) return 0.0f;
    const float value = text.getFloatValue();
    return std::isfinite(value) ? value : 0.0f;
}

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
        p.time = fields.size() > 0 ? decodeMsegCoordinate(fields[0]) : 0.0f;
        p.level = fields.size() > 1 ? decodeMsegCoordinate(fields[1]) : 0.0f;
        p.curve = fields.size() > 2 ? decodeMsegCoordinate(fields[2]) : 0.0f;
    }
}

} // namespace cs80::detail

