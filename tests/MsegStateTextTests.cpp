#include "plugin/MsegStateText.h"

#include <array>
#include <bit>
#include <cmath>
#include <clocale>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

namespace
{
int checks = 0, failures = 0;
void check(bool pass, const char* message)
{
    ++checks;
    if (!pass)
    {
        ++failures;
        if (failures < 20) std::cerr << "FAIL: " << message << '\n';
    }
}

cs80::modern::MsegShape roundTrip(const cs80::modern::MsegShape& shape)
{
    // Actual codec and XML attribute boundary used by sessions, user presets
    // and inactive A/B snapshots. No processor or message loop is constructed.
    juce::XmlElement xml("MSEG");
    xml.setAttribute("points", cs80::detail::encodeMsegPoints(shape));
    const auto restored = juce::XmlDocument::parse(xml.toString());
    check(restored != nullptr, "encoded points survive XML serialization");
    auto copy = shape;
    if (restored != nullptr)
        cs80::detail::decodeMsegPoints(restored->getStringAttribute("points"), copy);
    return cs80::modern::sanitizeShape(copy);
}

void testExactRoundTrips()
{
    using namespace cs80::modern;
    const std::array<float, 14> values {
        0.0f, 1.0f, 0.123456789f, std::nextafter(0.5f, 0.0f), std::nextafter(0.5f, 1.0f),
        std::nextafter(1.0f, 0.0f), 1.0e-7f, 1.0e-12f,
        std::numeric_limits<float>::min(), std::numeric_limits<float>::denorm_min(),
        0.00000049f, 0.00000149f, 0.99999994f, 0.31415927f
    };
    for (const auto value : values)
    {
        MsegShape shape;
        shape.points[0] = {0.0f, value, -value};
        shape.points[1] = {value, 1.0f - value, value};
        check(roundTrip(shape) == shape, "every finite float field round-trips exactly");
    }
    for (float time : { 0.123456789f, std::nextafter(1.0f, 2.0f),
                        std::nextafter(64.0f, 0.0f), 64.0f })
    {
        MsegShape shape;
        shape.points[1] = {time, 0.123456789f, -0.31415927f};
        check(roundTrip(shape) == shape, "long and fractional times round-trip exactly");
    }
    // Cover exponents down to subnormal values, not merely ordinary UI
    // decimal examples that happen to fit six places.
    std::uint32_t rng = 0x9e3779b9u;
    for (int i = 0; i < 4096; ++i)
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        const float value = std::bit_cast<float>(rng % 0x3f800001u);
        MsegShape shape;
        shape.points[0] = {0.0f, value, -value};
        shape.points[1] = {value, value, value};
        check(roundTrip(shape) == shape, "finite float bit-pattern samples retain every field");
    }
}

void testCompleteShapes()
{
    using namespace cs80::modern;
    for (int index = 0; index < kMsegCount; ++index)
    {
        const auto shape = defaultShape(index);
        check(roundTrip(shape) == shape, "default shapes retain every field");
    }
    MsegShape shape;
    shape.count = kMsegMaxPoints;
    shape.loopStart = 1;
    shape.loopEnd = kMsegMaxPoints - 2;
    shape.loop = MsegLoop::sustain;
    shape.trigger = MsegTrigger::free;
    shape.sync = shape.bipolar = true;
    for (int i = 0; i < shape.count; ++i)
        shape.points[(std::size_t) i] = {i == 0 ? 0.0f : (float) (i / 7.123456789),
                                         (float) (i / 31.0), (float) (i / 15.5 - 1.0)};
    // Adjacent but distinct times and exact vertical steps must stay distinct.
    shape.points[1].time = std::nextafter(shape.points[2].time, 0.0f);
    shape.points[4].time = shape.points[3].time;
    auto restored = shape;
    for (int cycle = 0; cycle < 10; ++cycle)
    {
        restored = roundTrip(restored);
        check(restored == shape, "full-size shape remains exact across repeated save/load cycles");
    }
}

void testShortLoopRemainsALoop()
{
    using namespace cs80::modern;
    MsegShape shape;
    shape.count = 3;
    shape.points[0] = {0.0f, 0.0f, 0.0f};
    shape.points[1] = {0.0000002f, 1.0f, 0.0f};
    shape.points[2] = {0.0000004f, 0.0f, 0.0f};
    shape.loop = MsegLoop::always;
    shape.loopStart = 0;
    shape.loopEnd = 2;
    const auto restored = roundTrip(shape);
    check(restored.points[2].time > restored.points[0].time,
          "saving a short loop must not collapse it into a sustain point");
    MsegRunner before, after;
    before.trigger(shape);
    after.trigger(restored);
    bool same = true, moved = false;
    for (int sample = 0; sample < 256; ++sample)
    {
        const auto original = before.tick(shape, msegAdvance(0.125, false, 0.0, 96000.0), true);
        const auto recalled = after.tick(restored, msegAdvance(0.125, false, 0.0, 96000.0), true);
        same = same && original == recalled && before.getPosition() == after.getPosition();
        moved = moved || original > 0.1f;
    }
    check(moved, "source short-loop fixture has nonconstant modulation");
    check(same, "saved short loops retain sample-exact modulation after recall");
}

void testLocaleIndependence()
{
    using namespace cs80::modern;
    const std::string originalLocale = std::setlocale(LC_NUMERIC, nullptr);
    MsegShape shape;
    shape.points[0] = {0.0f, 0.123456789f, -0.31415927f};
    shape.points[1] = {0.0000002f, 0.987654321f, 0.111111111f};
    const auto encoded = cs80::detail::encodeMsegPoints(shape);
    for (const auto* locale : {"C", "de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8"})
    {
        // Locale names are platform-dependent. Exercise additional installed
        // locales when present; the C locale is always available.
        const auto* selected = std::setlocale(LC_NUMERIC, locale);
        if (selected == nullptr) continue;
        const std::string selectedLocale = selected;
        check(cs80::detail::encodeMsegPoints(shape) == encoded,
              "saved text is independent of the process numeric locale");
        check(roundTrip(shape) == shape, "comma-decimal locales preserve exact numeric fields");
        check(std::string(std::setlocale(LC_NUMERIC, nullptr)) == selectedLocale,
              "encoding and decoding leave the caller's locale unchanged");
    }
    std::setlocale(LC_NUMERIC, originalLocale.c_str());
}

void testExistingTextCompatibility()
{
    using namespace cs80::modern;
    auto shape = defaultShape(0);
    cs80::detail::decodeMsegPoints("0.000000 0.125000 -0.500000;0.250000 1.000000 0.000000", shape);
    check(shape.count == 2 && shape.points[0] == MsegPoint{0.0f, 0.125f, -0.5f}
              && shape.points[1] == MsegPoint{0.25f, 1.0f, 0.0f},
          "existing fixed-decimal sessions remain readable");
    const auto original = defaultShape(0);
    auto missing = original;
    cs80::detail::decodeMsegPoints("", missing);
    check(missing == original, "missing points retain the existing default shape");
}

void testMalformedCoordinates()
{
    using namespace cs80::modern;
    for (const auto* token : {"1e99999999999999999999", "1e-99999999999999999999",
                              "1e2147483647", "1e-2147483648", "1e+", "nan", "inf",
                              "1junk", "--1", ".", "1e309", "-1e309"})
        for (int field = 0; field < 3; ++field)
        {
            juce::StringArray values {"0.5", "0.25", "-0.5"};
            values.set(field, token);
            juce::XmlElement xml("MSEG");
            xml.setAttribute("points", "0 0 0;" + values.joinIntoString(" "));
            const auto parsed = juce::XmlDocument::parse(xml.toString());
            check(parsed != nullptr, "malformed numeric token remains valid XML");
            MsegShape shape;
            cs80::detail::decodeMsegPoints(parsed->getStringAttribute("points"), shape);
            const auto point = shape.points[1];
            check((field == 0 ? point.time : field == 1 ? point.level : point.curve) == 0.0f,
                  "invalid coordinate is rejected before the JUCE numeric reader");
            check(field == 0 || point.time == 0.5f, "other time survives malformed field");
            check(field == 1 || point.level == 0.25f, "other level survives malformed field");
            check(field == 2 || point.curve == -0.5f, "other curve survives malformed field");
        }
}
} // namespace

int main()
{
    testMalformedCoordinates();
    testShortLoopRemainsALoop();
    testExactRoundTrips();
    testCompleteShapes();
    testExistingTextCompatibility();
    testLocaleIndependence();
    std::cout << "MSEG state text: " << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}

