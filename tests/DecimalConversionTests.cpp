#include "tools/DecimalConversion.h"
#include <array>
#include <bit>
#include <cfenv>
#include <clocale>
#include <cstdint>
#include <iostream>
#include <limits>
#include <locale>
#include <string>
#if !defined(__APPLE__)
 #include <charconv>
#endif

namespace {
struct CommaPunctuation : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
};
}

int main() {
    int failures = 0, checks = 0;
    auto check = [&](bool ok, const std::string& message) {
        ++checks;
        if (!ok) { ++failures; std::cerr << message << '\n'; }
    };
    auto bits = [](double value) { return std::bit_cast<std::uint64_t>(value); };
    struct Fixture { const char* token; double expected; };
    const std::array fixtures {
        Fixture{"0", 0.0}, Fixture{"-0", -0.0}, Fixture{"-0.000e+99", -0.0},
        Fixture{"0e999999", 0.0}, Fixture{"0e-999999", 0.0},
        Fixture{".5", .5}, Fixture{"-.5", -.5}, Fixture{"1.", 1.0},
        Fixture{"0001.0", 1.0}, Fixture{"1.25e-2", .0125},
        Fixture{"2.2250738585072014e-308", std::numeric_limits<double>::min()},
        Fixture{"4.9406564584124654e-324", std::numeric_limits<double>::denorm_min()},
        Fixture{"-5e-324", -std::numeric_limits<double>::denorm_min()},
        Fixture{"2.5e-324", std::numeric_limits<double>::denorm_min()},
        Fixture{"1.7976931348623157e308", std::numeric_limits<double>::max()},
        Fixture{"1.00000000000000011102230246251565404236316680908203125", 1.0},
        Fixture{"1.00000000000000011102230246251565404236316680908203126", 0x1.0000000000001p0}
    };
    const int originalRounding = std::fegetround();
    for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        check(std::fesetround(mode) == 0, "Set rounding mode");
        for (const auto& fixture : fixtures) {
            double value = 123;
            check(cs80::tools::convertValidatedDecimal(fixture.token, value),
                  std::string("Rejected representable decimal: ") + fixture.token);
            check(bits(value) == bits(fixture.expected),
                  std::string("Wrong nearest-rounded value: ") + fixture.token);
            check(std::fegetround() == mode, "Caller rounding mode changed");
        }
        for (const auto token : {"1e99999", "-1e99999", "1e-99999", "-1e-99999",
                                 "2.4e-324", "-2.4e-324", "1.7976931348623159e308",
                                 "", "1x", "1.0extra", "nan", "inf"}) {
            double value = 123;
            check(!cs80::tools::convertValidatedDecimal(token, value),
                  std::string("Admitted invalid/unrepresentable decimal: ") + token);
            check(value == 123, "Failed conversion changed output");
            check(std::fegetround() == mode, "Failure changed caller rounding mode");
        }
    }
    auto floatBits = [](float value) { return std::bit_cast<std::uint32_t>(value); };
    struct FloatFixture { const char* token; float expected; };
    const std::array floatFixtures {
        FloatFixture{"0", 0.0f}, FloatFixture{"-0", -0.0f},
        FloatFixture{"-0.000e+99", -0.0f}, FloatFixture{"0e999999", 0.0f},
        FloatFixture{"0e-999999", 0.0f}, FloatFixture{".5", .5f},
        FloatFixture{"-.5", -.5f}, FloatFixture{"1.", 1.0f},
        FloatFixture{"1.1754943508222875e-38", std::numeric_limits<float>::min()},
        FloatFixture{"1.401298464324817e-45", std::numeric_limits<float>::denorm_min()},
        FloatFixture{"-1.401298464324817e-45", -std::numeric_limits<float>::denorm_min()},
        FloatFixture{"7.1e-46", std::numeric_limits<float>::denorm_min()},
        FloatFixture{"3.4028234663852886e38", std::numeric_limits<float>::max()},
        FloatFixture{"0.5000000298023223876953125", .5f},
        FloatFixture{"0.500000029802322387695312499999999999999", .5f},
        FloatFixture{"0.500000029802322387695312500000000000001", 0x1.000002p-1f},
        FloatFixture{"-0.500000029802322387695312500000000000001", -0x1.000002p-1f}
    };
    for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        check(std::fesetround(mode) == 0, "Set float rounding mode");
        for (const auto& fixture : floatFixtures) {
            float value = 123;
            check(cs80::tools::convertValidatedDecimal(fixture.token, value),
                  std::string("Rejected representable float: ") + fixture.token);
            check(floatBits(value) == floatBits(fixture.expected),
                  std::string("Wrong nearest-rounded float: ") + fixture.token);
            check(std::fegetround() == mode, "Float conversion changed rounding mode");
        }
        for (const auto token : {"1e99999", "-1e99999", "1e-99999", "-1e-99999",
                                 "7e-46", "-7e-46", "3.4028236e38",
                                 "", "1x", "1.0extra", "nan", "inf"}) {
            float value = 123;
            check(!cs80::tools::convertValidatedDecimal(token, value),
                  std::string("Admitted invalid/unrepresentable float: ") + token);
            check(value == 123, "Failed float conversion changed output");
            check(std::fegetround() == mode, "Float failure changed rounding mode");
        }
    }
    check(std::fesetround(FE_TONEAREST) == 0, "Set nearest mode for differential corpus");
    const std::string trailing = "1.25trailing";
    double sliced = 0;
    check(cs80::tools::convertValidatedDecimal(std::string_view(trailing.data(), 4), sliced)
          && sliced == 1.25, "Non-NUL-terminated view not bounded");
    const std::string embeddedNull("1\0x", 3);
    check(!cs80::tools::convertValidatedDecimal(embeddedNull, sliced), "Embedded NUL admitted");
    float slicedFloat = 0;
    check(cs80::tools::convertValidatedDecimal(std::string_view(trailing.data(), 4), slicedFloat)
          && slicedFloat == 1.25f, "Float non-NUL-terminated view not bounded");
    check(!cs80::tools::convertValidatedDecimal(embeddedNull, slicedFloat), "Float embedded NUL admitted");
#if !defined(__APPLE__)
    // On Linux this also compares the forced Apple-compatible path with the
    // existing from_chars contract. Apple must not instantiate that overload.
    for (int significand = -97; significand <= 97; ++significand) {
        for (int exponent = -330; exponent <= 310; exponent += 7) {
            const auto token = std::to_string(significand) + ".12345678901234567e" + std::to_string(exponent);
            double expected = 123, actual = 123;
            const auto parsed = std::from_chars(token.data(), token.data() + token.size(), expected,
                                               std::chars_format::general);
            const bool valid = parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size()
                               && std::isfinite(expected);
            const bool converted = cs80::tools::convertValidatedDecimal(token, actual);
            check(valid == converted && (!valid || bits(expected) == bits(actual)),
                  "Differential decimal mismatch: " + token);
        }
    }
    for (int significand = -97; significand <= 97; ++significand) {
        for (int exponent = -55; exponent <= 45; ++exponent) {
            const auto token = std::to_string(significand) + ".12345678901234567e" + std::to_string(exponent);
            float expected = 123, actual = 123;
            const auto parsed = std::from_chars(token.data(), token.data() + token.size(), expected,
                                               std::chars_format::general);
            const bool valid = parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size()
                               && std::isfinite(expected);
            const bool converted = cs80::tools::convertValidatedDecimal(token, actual);
            check(valid == converted && (!valid || floatBits(expected) == floatBits(actual)),
                  "Differential float mismatch: " + token);
        }
    }
#endif
    const std::locale originalCppLocale = std::locale();
    std::locale::global(std::locale(originalCppLocale, new CommaPunctuation));
    double value = 0;
    check(cs80::tools::convertValidatedDecimal("1.25", value) && value == 1.25,
          "C++ comma locale changed decimal parsing");
    check(!cs80::tools::convertValidatedDecimal("1,25", value), "Comma token admitted");
    float floatValue = 0;
    check(cs80::tools::convertValidatedDecimal("1.25", floatValue) && floatValue == 1.25f,
          "C++ comma locale changed float parsing");
    check(!cs80::tools::convertValidatedDecimal("1,25", floatValue), "Float comma token admitted");
    std::locale::global(originalCppLocale);
    const std::string originalCLocale = std::setlocale(LC_NUMERIC, nullptr);
    bool exercisedCLocale = false;
    for (const char* locale : {"de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "de_DE", "German_Germany.1252"}) {
        if (std::setlocale(LC_NUMERIC, locale) == nullptr) continue;
        exercisedCLocale = true;
        const std::string selected = std::setlocale(LC_NUMERIC, nullptr);
        check(cs80::tools::convertValidatedDecimal("1.25", value) && value == 1.25,
              "C numeric locale changed decimal parsing");
        check(cs80::tools::convertValidatedDecimal("1.25", floatValue) && floatValue == 1.25f,
              "C numeric locale changed float parsing");
        check(selected == std::setlocale(LC_NUMERIC, nullptr), "Conversion changed process C locale");
        break;
    }
    check(std::setlocale(LC_NUMERIC, originalCLocale.c_str()) != nullptr, "Restore C locale");
    if (!exercisedCLocale) std::cout << "Non-C system locale unavailable; C++ comma locale exercised\n";
    check(std::fesetround(originalRounding) == 0, "Restore original rounding");
    std::cout << "Decimal conversion: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
