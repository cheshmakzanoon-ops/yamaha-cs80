// Exercise the production parser without constructing a processor, APVTS,
// MessageManager, timer or GUI. Host restore coverage lives in
// ModernLayerStateRangeHostTests.h and remains necessary.
#include "plugin/ModernStateInteger.h"

#include <array>
#include <cstdio>
#include <limits>
#include <utility>

int main()
{
    int failures = 0, checks = 0;
    const auto check = [&](const juce::var& value, int expected, int fallback)
    {
        ++checks;
        const int actual = cs80::detail::modernStateInteger(value, fallback);
        if (actual != expected)
        {
            std::fprintf(stderr, "FAIL value='%s': expected %d, got %d (fallback %d)\n",
                         value.toString().toRawUTF8(), expected, actual, fallback);
            ++failures;
        }
    };
    for (const auto* text : { "", " ", "\t\r\n", "banana", "1suffix", "0suffix", "1 2",
                             ".", "+", "-", "+.", "-.", "1.2.3", "1e", "1e+", "1e-",
                             "1e0suffix", "0x1", "--1", "+-1", "- 1", "NaN", "Inf",
                             "-Infinity", "1,0", "1_0", " 1x ", "1\n0",
                             "1e4294967296", "1e-4294967296", "1e+4294967297",
                             "1e0004294967296", "1e2147483647", "1e-2147483647",
                             "0e4294967296", "1e9999999999999999999999999999" })
        for (const int fallback : { -1, 0, 7 })
            check(text, fallback, fallback);

    for (const auto& [text, expected] : std::array<std::pair<const char*, int>, 26> {{
             { "0", 0 }, { "-0", 0 }, { "+0", 0 }, { "01", 1 }, { "  +1 \t", 1 },
             { "0.49", 0 }, { ".5", 1 }, { "1.49", 1 }, { "1.5", 2 },
             { "-.49", 0 }, { "-.5", -1 }, { "-1.5", -2 }, { "1.", 1 },
             { "1.e0", 1 }, { "1e0", 1 }, { "1E+0", 1 }, { "10e-1", 1 },
             { ".15e1", 2 }, { "-15E-1", -2 }, { "2147483647", 2147483647 },
             { "-2147483648", (-2147483647 - 1) }, { "2147483647.49", 2147483647 },
             { "-2147483648.49", (-2147483647 - 1) }, { "\r\n2\t", 2 },
             { "1e0000000000000000000", 1 }, { "1e-999999999", 0 } }})
        check(text, expected, 7);

    for (const auto& [value, expected] : std::array<std::pair<double, int>, 10> {{
             { 0.49, 0 }, { 0.5, 1 }, { 1.49, 1 }, { 1.5, 2 }, { -0.49, 0 },
             { -0.5, -1 }, { 2147483647.0, 2147483647 },
             { -2147483648.0, (-2147483647 - 1) },
             { 2147483647.49, 2147483647 }, { -2147483648.49, (-2147483647 - 1) } }})
        check(value, expected, 7);
    check(2, 2, 7);
    check((juce::int64) 2, 2, 7);

    for (const auto& value : std::array<juce::var, 18> {{
             "2147483648", "-2147483649", "4294967296", "4294967297",
             "2147483647.5", "-2147483648.5", "-4294967295", "1e100", "-1e100",
             "1e9999", "-1e9999", (juce::int64) 4294967297LL,
             (juce::int64) -4294967295LL, 4294967298.0, -4294967294.0,
             std::numeric_limits<double>::max(), std::numeric_limits<double>::infinity(),
             std::numeric_limits<double>::quiet_NaN() }})
        check(value, -1, -1);
    for (const auto& value : { juce::var {}, juce::var(true), juce::var(false),
                              juce::var(juce::Array<juce::var> { 1 }) })
        check(value, -1, -1);

    std::printf("Modern state integer parser: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
