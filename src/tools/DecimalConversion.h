#pragma once

#include <cmath>
#include <string_view>

#if defined(__APPLE__) || defined(CS80_FORCE_C_LOCALE_DECIMAL)
 #include <cfenv>
 #include <cstdlib>
 #include <locale.h>
 #include <string>
 #if defined(__APPLE__)
  #include <xlocale.h>
 #endif
#else
 #include <charconv>
#endif

namespace cs80::tools {

// Offline tools only. Callers first enforce their strict ASCII decimal grammar
// and remove an optional leading '+'. No whitespace, hex or special values are
// admitted by that grammar. Leave output unchanged when conversion fails.
inline bool convertValidatedDecimal(std::string_view token, double& output) {
    if (token.empty()) return false;
#if defined(__APPLE__) || defined(CS80_FORCE_C_LOCALE_DECIMAL)
    // The libc++ shipped with Xcode 16.4 has no floating-point from_chars.
    // Use an immutable, private C locale rather than the process-wide locale.
    struct NumericLocale {
        locale_t value = newlocale(LC_NUMERIC_MASK, "C", nullptr);
        ~NumericLocale() { if (value) freelocale(value); }
    };
    static const NumericLocale locale;
    if (!locale.value) return false;
    const std::string terminated(token);
    char* end = nullptr;
    // strtod_l observes the calling thread's rounding mode; from_chars does
    // not. Restore it before returning, including on out-of-range input.
    const int rounding = std::fegetround();
    if (rounding == -1) return false;
    const bool changed = rounding != FE_TONEAREST;
    if (changed && std::fesetround(FE_TONEAREST) != 0) return false;
    const double value = strtod_l(terminated.c_str(), &end, locale.value);
    if (changed && std::fesetround(rounding) != 0) return false;
    if (end != terminated.c_str() + terminated.size() || !std::isfinite(value)) return false;
    // ERANGE also denotes a *representable* subnormal, which from_chars
    // accepts. Reject only nonzero decimal significands rounded down to zero.
    if (value == 0) {
        for (char c : token) {
            if (c == 'e' || c == 'E') break;
            if (c >= '1' && c <= '9') return false;
        }
    }
#else
    double value = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(),
                                        value, std::chars_format::general);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()
        || !std::isfinite(value)) return false;
#endif
    output = value;
    return true;
}

} // namespace cs80::tools
