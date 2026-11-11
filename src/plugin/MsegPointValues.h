#pragma once

#include "core/Mseg.h"
#include <juce_core/juce_core.h>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <cmath>
#include <cfenv>
#include <cstdlib>
#include <locale.h>
#if defined(__APPLE__)
 #include <xlocale.h>
#endif
#include <optional>

namespace cs80::ui::modern_layer
{
// Locale-independent decimal UI input. Unlike the state-recovery codec, invalid
// text is not converted to zero. Float32 round trips preserve untouched fields.
inline std::optional<float> parseMsegPointValue(const juce::String& input)
{
    const auto text = input.trim();
    if (text.isEmpty() || text.length() > 128) return {};
    const auto bytes = text.toStdString();
    std::size_t at = 0;
    if (bytes[at] == '+' || bytes[at] == '-') ++at;
    bool digits = false, nonzero = false;
    auto readDigits = [&]
    {
        while (at < bytes.size() && bytes[at] >= '0' && bytes[at] <= '9')
        {
            digits = true;
            nonzero |= bytes[at] != '0';
            ++at;
        }
    };
    readDigits();
    if (at < bytes.size() && bytes[at] == '.') { ++at; readDigits(); }
    if (!digits) return {};
    if (at < bytes.size() && (bytes[at] == 'e' || bytes[at] == 'E'))
    {
        ++at;
        if (at < bytes.size() && (bytes[at] == '+' || bytes[at] == '-')) ++at;
        const auto start = at;
        while (at < bytes.size() && bytes[at] >= '0' && bytes[at] <= '9') ++at;
        if (at == start) return {};
    }
    if (at != bytes.size()) return {};
    // Direct Float32 conversion avoids double rounding and JUCE's 17-digit
    // mantissa truncation. Use an owned C locale, never the process locale.
    // These platform APIs also avoid missing Apple float from_chars and libc++
    // float-stream ERANGE failures for representable subnormals. UI thread only.
    struct NumericLocale
    {
       #if JUCE_WINDOWS
        _locale_t value = _create_locale(LC_NUMERIC, "C");
        ~NumericLocale() { if (value) _free_locale(value); }
       #else
        locale_t value = newlocale(LC_NUMERIC_MASK, "C", nullptr);
        ~NumericLocale() { if (value) freelocale(value); }
       #endif
    };
    static const NumericLocale locale;
    if (!locale.value) return {};
    const int rounding = std::fegetround();
    if (rounding == -1) return {};
    const bool changed = rounding != FE_TONEAREST;
    if (changed && std::fesetround(FE_TONEAREST) != 0) return {};
    char* end = nullptr;
   #if JUCE_WINDOWS
    const float value = _strtof_l(bytes.c_str(), &end, locale.value);
   #else
    const float value = strtof_l(bytes.c_str(), &end, locale.value);
   #endif
    if (changed && std::fesetround(rounding) != 0) return {};
    // ERANGE alone also means a valid nonzero subnormal. Reject only nonfinite
    // or nonzero-to-zero values, and never accept an unconsumed suffix.
    if (end != bytes.c_str() + bytes.size() || !std::isfinite(value) || (value == 0.0f && nonzero)) return {};
    return value;
}

inline juce::String formatMsegPointValue(float value)
{
    // Prefer a short decimal, but never change the stored Float32 on a no-op.
    for (int precision = 1; precision <= std::numeric_limits<float>::max_digits10; ++precision)
    {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::setprecision(precision) << value;
        const juce::String text(stream.str());
        if (parseMsegPointValue(text) == value) return text;
    }
    jassertfalse; // Every finite Float32 has a max_digits10 round trip.
    return {};
}

// Validate the entire request before writing. Coincident times are intentional
// steps, not an error. The last point has no outgoing segment: preserve its curve.
inline juce::String validateMsegPointValues(const modern::MsegShape& shape, int point,
                                          float time, float level, float curve)
{
    if (point < 0 || point >= shape.count) return "Select a valid point.";
    if (!std::isfinite(time) || !std::isfinite(level) || !std::isfinite(curve))
        return "Enter finite decimal numbers.";
    const float lo = point == 0 ? 0.0f : shape.points[(std::size_t) point - 1].time;
    const float hi = point == 0 ? 0.0f : point + 1 < shape.count
        ? shape.points[(std::size_t) point + 1].time : modern::kMsegMaxTime;
    if (time < lo || time > hi)
        return "Time must be " + formatMsegPointValue(lo) + " to " + formatMsegPointValue(hi)
               + (shape.sync ? " beats." : " seconds.");
    if (level < 0.0f || level > 1.0f) return "Level must be 0 to 1 (normalized).";
    if (curve < -1.0f || curve > 1.0f) return "Curve must be -1 to 1.";
    if (point == shape.count - 1 && curve != shape.points[(std::size_t) point].curve)
        return "The last point has no outgoing curve.";
    return {};
}
}
