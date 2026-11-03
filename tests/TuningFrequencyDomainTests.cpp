// Scala table admission must agree with the core's nominal float midiToHz.
// This does not qualify arbitrary pitch modulation or a different audio-thread
// floating-point environment, and does not impose an audible-frequency range.
#include "core/Dsp.h"
#include "core/Tuning.h"

#include <bit>
#include <cstdio>
#include <limits>
#include <string>

namespace tn = cs80::tuning;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        if (failures <= 40) std::printf("FAIL: %s\n", what.c_str());
    }
}
bool same(const tn::TuningTable& a, const tn::TuningTable& b)
{
    for (std::size_t n = 0; n < 128; ++n)
        if (std::bit_cast<std::uint32_t>(a.pitch[n]) != std::bit_cast<std::uint32_t>(b.pitch[n])
            || a.mapped[n] != b.mapped[n]) return false;
    return true;
}
tn::TuningTable sentinel()
{
    tn::TuningTable t;
    for (std::size_t n = 0; n < 128; ++n)
    {
        t.pitch[n] = static_cast<float>(n) - 63.75f;
        t.mapped[n] = n % 2 == 0;
    }
    return t;
}
bool nominallyRenderable(float pitch)
{
    const float hz = cs80::midiToHz(pitch);
    return std::isfinite(hz) && hz > 0.0f;
}

// Only one key has the exceptional degree; the other 127 keys remain at A440.
// Key 127 makes a rejection happen after all preceding temporary entries were
// computed, so the destination's transactional behavior is tested as well.
void testPitch(float pitch, int key, const std::string& label)
{
    tn::Scale scale { "single exceptional degree", { (static_cast<double>(pitch) - 69.0) * 100.0, 1200.0 } };
    tn::KeyboardMapping mapping;
    mapping.size = 128; mapping.middle = 0; mapping.reference = 69; mapping.octaveDegree = 2;
    mapping.map.assign(128, 0); mapping.map[static_cast<std::size_t>(key)] = 1;
    auto actual = sentinel(); const auto before = actual;
    const bool expected = nominallyRenderable(pitch);
    const auto error = tn::buildTable(scale, mapping, actual);
    check(error.empty() == expected, label + " follows actual nominal float conversion");
    if (expected)
    {
        tn::TuningTable want;
        want.pitch.fill(69.0f); want.pitch[static_cast<std::size_t>(key)] = pitch;
        check(same(actual, want), label + " retains exact stored pitches and mapping");
        bool finitePositive = true;
        for (auto p : actual.pitch) finitePositive = finitePositive && nominallyRenderable(p);
        check(finitePositive, label + " all table entries reconstruct positive finite frequencies");
    }
    else
    {
        check(error == "tuning produced an unrenderable nominal float frequency", label + " identifies frequency-domain failure");
        check(same(actual, before), label + " leaves destination bits unchanged");
    }
}

void testBoundaries()
{
    // Find adjacent float pitches on each edge using the core's actual function,
    // not an independently rounded Hz threshold or a presumed libm cutoff.
    float under = -2000.0f, low = -1000.0f;
    float high = 1000.0f, over = 2000.0f;
    check(!nominallyRenderable(under) && nominallyRenderable(low)
              && nominallyRenderable(high) && !nominallyRenderable(over), "nominal boundary search brackets");
    for (int i = 0; i < 100; ++i)
    {
        const float mid = (under + low) * 0.5f;
        if (mid == under || mid == low) break;
        if (nominallyRenderable(mid)) low = mid; else under = mid;
    }
    for (int i = 0; i < 100; ++i)
    {
        const float mid = (high + over) * 0.5f;
        if (mid == high || mid == over) break;
        if (nominallyRenderable(mid)) high = mid; else over = mid;
    }
    const float inf = (std::numeric_limits<float>::infinity)();
    check(std::nextafter(under, inf) == low && std::nextafter(high, inf) == over,
          "frequency boundaries are adjacent float pitches");
    check(cs80::midiToHz(under) == 0.0f && std::isinf(cs80::midiToHz(over)),
          "boundaries exercise zero and infinity rather than just a conservative limit");
    float lo = low, hi = high;
    for (int i = 0; i < 32; ++i) { lo = std::nextafter(lo, -inf); hi = std::nextafter(hi, -inf); }
    for (int i = 0; i < 65; ++i)
    {
        for (int key : { 0, 127 })
        {
            testPitch(lo, key, "lower edge " + std::to_string(i) + " key " + std::to_string(key));
            testPitch(hi, key, "upper edge " + std::to_string(i) + " key " + std::to_string(key));
        }
        lo = std::nextafter(lo, inf); hi = std::nextafter(hi, inf);
    }
    std::printf("Nominal accepted pitch edges: %.9g (%.9g Hz), %.9g (%.9g Hz)\n",
                low, cs80::midiToHz(low), high, cs80::midiToHz(high));

    // A final double-to-float Hz cast is not an equivalent gate. The core rounds
    // its pow result to float before multiplying by 440, so it can reach zero
    // even when the correctly rounded final double frequency is still positive.
    constexpr float intermediateUnderflow = -1800.0f;
    const double hz = 440.0 * std::exp2((static_cast<double>(intermediateUnderflow) - 69.0) / 12.0);
    check(static_cast<float>(hz) > 0.0f && cs80::midiToHz(intermediateUnderflow) == 0.0f,
          "intermediate float underflow distinguishes the actual operation order");
    testPitch(intermediateUnderflow, 127, "intermediate pow underflow");
    for (float p : { -3000.0f, 3000.0f }) testPitch(p, 127, "finite-pitch unrenderable frequency");
    // Positive subnormal and enormous-but-finite frequencies are still admitted.
    testPitch(low, 127, "positive subnormal frequency remains accepted");
    testPitch(high, 127, "largest accepted nominal frequency remains accepted");
}

void testParsedScales()
{
    tn::Scale scale;
    check(tn::parseScale("wide period\n1\n12000.0\n", scale).empty(), "wide period is valid Scala input");
    auto actual = sentinel(); const auto before = actual;
    check(!tn::buildTable(scale, tn::defaultMapping(scale), actual).empty() && same(actual, before),
          "wide period rejected atomically instead of publishing zero/infinite nominal frequencies");
    for (int count : { 1, 7, 12, 19, 31, 53 })
    {
        std::string text = "equal division\n" + std::to_string(count) + '\n';
        for (int i = 1; i <= count; ++i) text += std::to_string(1200.0 * i / count) + '\n';
        check(tn::parseScale(text, scale).empty(), "ordinary EDO scale parses");
        actual = sentinel();
        check(tn::buildTable(scale, tn::defaultMapping(scale), actual).empty(), "ordinary EDO table remains accepted");
        bool valid = true;
        for (auto p : actual.pitch) valid = valid && nominallyRenderable(p);
        check(valid, "ordinary EDO table is nominally renderable");
    }
    check(tn::parseScale("just\n5\n9/8\n5/4\n3/2\n5/3\n2/1\n", scale).empty(), "ratio scale parses");
    tn::KeyboardMapping mapping;
    check(tn::parseKeyboardMapping("0\n0\n127\n60\n69\n432.0\n5\n", mapping).empty(), "ordinary KBM reference parses");
    check(tn::buildTable(scale, mapping, actual).empty(), "ratio scale with KBM remains accepted");

    // Deliberately very small but valid reference frequency, with only key 127
    // mapped. All entries, including fallback pitches for unmapped notes, must
    // meet the table's nominal-frequency contract.
    check(tn::parseScale("octave\n1\n2/1\n", scale).empty(), "octave scale parses");
    const std::string tiny = "0." + std::string(39, '0') + '1'; // 1e-40 Hz
    check(tn::parseKeyboardMapping("0\n127\n127\n127\n127\n" + tiny + "\n1\n", mapping).empty(),
          "tiny positive reference is syntactically valid");
    check(nominallyRenderable(tn::TuningTable::pitchFromHz(mapping.referenceHz)), "mapped reference remains nominally renderable");
    actual = before;
    check(!tn::buildTable(scale, mapping, actual).empty() && same(actual, before),
          "unmapped fallback entry outside nominal domain is rejected atomically");
}
} // namespace

int main()
{
    testBoundaries();
    testParsedScales();
    std::printf("Tuning frequency domain: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
