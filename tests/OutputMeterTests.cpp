#include "plugin/OutputMeter.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* text)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", text); }
}
std::uint32_t bits(float x) { return std::bit_cast<std::uint32_t>(x); }
cs80::OutputMeterBlock block(float sample)
{
    cs80::OutputMeterBlock result;
    result.addSample(sample);
    return result;
}
}

int main()
{
    cs80::OutputMeter meter;
    auto empty = meter.consume();
    check(bits(empty.samplePeak) == 0 && !empty.overRange && !empty.nonFinite, "fresh telemetry is empty");
    const std::array<float, 6> samples { -.5f, .75f, -1.25f, 0, -0.0f, .125f };
    const auto untouched = samples;
    cs80::OutputMeterBlock measured;
    measured.addSamples(samples.data(), (int) samples.size());
    check(samples == untouched, "measuring samples never modifies input");
    check(measured.reading().samplePeak == 1.25f && measured.reading().overRange
              && !measured.reading().nonFinite, "finite absolute peak and strict-over-range classification");
    meter.publish(measured);
    meter.publish(block(.25f));
    auto value = meter.consume();
    check(value.samplePeak == 1.25f && value.overRange && !value.nonFinite,
          "unread blocks accumulate maximum and event flags instead of replacing a transient");
    value = meter.consume();
    check(value.samplePeak == 0 && !value.overRange && !value.nonFinite, "consume acknowledges telemetry once");
    meter.publish(block(-1.0f));
    value = meter.consume();
    check(value.samplePeak == 1 && !value.overRange, "exact full scale is not mislabeled as above full scale");
    meter.publish(block(std::nextafter(1.0f, 2.0f)));
    check(meter.consume().overRange, "first representable sample above full scale is detected");
    for (auto sampleBits : {0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffc00000u, 0x7f800001u})
    {
        cs80::OutputMeterBlock invalid;
        invalid.addSample(std::bit_cast<float>(sampleBits));
        invalid.addSample(-.5f);
        meter.publish(invalid);
        value = meter.consume();
        check(value.samplePeak == .5f && value.nonFinite && !value.overRange,
              "NaN/Inf signal separate event without contaminating finite sample peak");
    }
    cs80::OutputMeterBlock mixed;
    mixed.addSample(std::numeric_limits<float>::infinity());
    mixed.addSample(std::numeric_limits<float>::max());
    meter.publish(mixed);
    value = meter.consume();
    check(bits(value.samplePeak) == 0x7f7fffffu && value.overRange && value.nonFinite,
          "maximum finite output and nonfinite event can coexist");
    for (auto sampleBits : {1u, 0x007fffffu, 0x00800000u, 0x3f000000u})
    {
        meter.publish(block(std::bit_cast<float>(sampleBits | 0x80000000u)));
        value = meter.consume();
        check(bits(value.samplePeak) == sampleBits && !value.overRange && !value.nonFinite,
              "finite magnitude bit ordering preserves subnormal and normal samples exactly");
    }
    meter.publish(mixed);
    meter.reset();
    value = meter.consume();
    check(value.samplePeak == 0 && !value.overRange && !value.nonFinite,
          "nonconcurrent lifecycle reset clears old telemetry");
    meter.publish(block(.75f));
    (void) meter.consume(); // Editor-open discard, not an audio-thread reset.
    meter.publish(block(1.5f));
    value = meter.consume();
    check(value.samplePeak == 1.5f && value.overRange,
          "post-open-clear publications remain visible");
    cs80::OutputMeterBlock noSamples;
    noSamples.addSamples(nullptr, 100);
    noSamples.addSamples(samples.data(), 0);
    noSamples.addSamples(samples.data(), -1);
    check(noSamples.reading().samplePeak == 0 && !noSamples.reading().nonFinite,
          "empty/invalid-length windows do not inspect samples");
    std::printf("Output meter: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
