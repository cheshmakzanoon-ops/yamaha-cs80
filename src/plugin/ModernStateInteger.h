#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <limits>

namespace cs80::detail
{
inline int modernStateInteger(const juce::var& value, int fallback)
{
    if (! (value.isInt() || value.isInt64() || value.isDouble() || value.isString()))
        return fallback;

    double number;
    if (value.isString())
    {
        const auto text = value.toString().trim();
        auto cursor = text.getCharPointer();
        if (*cursor == '+' || *cursor == '-') ++cursor;
        const auto digits = [&cursor]
        {
            bool found = false;
            while (*cursor >= '0' && *cursor <= '9')
            {
                found = true;
                ++cursor;
            }
            return found;
        };
        bool hasDigits = digits();
        if (*cursor == '.')
        {
            ++cursor;
            hasDigits = digits() || hasDigits;
        }
        if (! hasDigits) return fallback;
        if (*cursor == 'e' || *cursor == 'E')
        {
            ++cursor;
            if (*cursor == '+' || *cursor == '-') ++cursor;
            if (*cursor < '0' || *cursor > '9') return fallback;
            // JUCE accumulates the exponent in int, then adds a mantissa
            // adjustment. Leave enough room for every digit in this token.
            const int exponentLimit = std::numeric_limits<int>::max() - text.length();
            int exponent = 0;
            while (*cursor >= '0' && *cursor <= '9')
            {
                const int digit = (int) *cursor - '0';
                if (exponent > exponentLimit / 10
                    || (exponent == exponentLimit / 10 && digit > exponentLimit % 10))
                    return fallback;
                exponent = exponent * 10 + digit;
                ++cursor;
            }
        }
        // JUCE's numeric reader accepts prefixes and returns zero for missing
        // numbers. Require the entire token so malformed indices cannot select
        // an existing MSEG/slot. Keep decimal and exponent notation for XML.
        if (! cursor.isEmpty()) return fallback;
        number = text.getDoubleValue();
    }
    else
    {
        number = (double) value;
    }
    if (! std::isfinite(number)) return fallback;
    // Round before narrowing, preserving the existing nearest-integer rule.
    const double rounded = std::round(number);
    if (rounded < (double) std::numeric_limits<int>::min()
        || rounded > (double) std::numeric_limits<int>::max())
        return fallback;
    return (int) rounded;
}
} // namespace cs80::detail
