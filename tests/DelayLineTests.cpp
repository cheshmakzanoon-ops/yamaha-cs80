// Linear delay-reader wrap safety. Pure core DSP, without plugin/GUI startup.
#include "core/Dsp.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{
int checks = 0, failures = 0;
void check(bool condition, const char* description)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

void testRoundedWrap()
{
    for (float rate : {16000.0f, 44100.0f, 48000.0f, 88200.0f, 96000.0f,
                       192000.0f, 384000.0f, 1536000.0f})
    {
        cs80::DelayLine line;
        line.prepare(0.08f, rate);
        line.write(0.375f);
        const float delay = std::nextafter(1.0f, 2.0f);
        const float wrapped = (1.0f - delay) + (float) line.capacity();
        check(wrapped == (float) line.capacity(), "fixture rounds the negative position to capacity");
        check(line.read(delay) == 0.375f, "rounded wrap reads sample zero, not one past the buffer");
        line.reset();
        check(line.read(delay) == 0.0f, "reset also clears the wrapped read");
    }

    // Exact production chorus failure at 44.1 kHz host, 2x internal rate.
    cs80::DelayLine line;
    line.prepare(0.08f, 88200.0f);
    check(line.capacity() == 7058, "production chorus capacity");
    for (int i = 0; i < 1212; ++i) line.write(i == 0 ? 0.625f : -0.25f);
    check(line.read(1212.000244140625f) == 0.625f, "production chorus rounded-wrap delay");
}

void testUnchangedInterpolation()
{
    // Reproduce the old float arithmetic only where it produces a valid index.
    // Bit-exact equality, rather than a tolerance, protects legacy DSP output.
    for (float rate : {16000.0f, 88200.0f, 96000.0f, 384000.0f})
    {
        cs80::DelayLine line;
        line.prepare(0.08f, rate);
        const int size = line.capacity();
        std::vector<float> reference((size_t) size, 0.0f);
        int writePos = 0;
        cs80::Rng rng(0xD31A70u);
        bool exact = true;
        int valid = 0, rounded = 0;
        for (int n = 0; n < size * 3; ++n)
        {
            const float x = rng.nextBipolar();
            line.write(x);
            reference[(size_t) writePos] = x;
            if (++writePos == size) writePos = 0;
            const std::array<float, 8> delays {
                -10.0f, 1.0f, 1.25f, 2.75f, (float) size - 2.0f, (float) size + 1.0f,
                1.0f + rng.nextUnipolar() * (float) (size - 3),
                std::nextafter((float) writePos, (float) size)
            };
            for (float delay : delays)
            {
                const float bounded = cs80::clampf(delay, 1.0f, (float) (size - 2));
                float pos = (float) writePos - bounded;
                while (pos < 0.0f) pos += (float) size;
                const int i0 = (int) pos;
                if (i0 == size)
                {
                    ++rounded;
                    exact = exact && line.read(delay) == reference[0];
                    continue;
                }
                ++valid;
                const float expected = cs80::lerp(reference[(size_t) i0],
                    reference[(size_t) ((i0 + 1) % size)], pos - (float) i0);
                exact = exact && std::bit_cast<std::uint32_t>(line.read(delay))
                    == std::bit_cast<std::uint32_t>(expected);
            }
        }
        check(valid > 0 && rounded > 0, "compatibility sweep covers ordinary and rounded wraps");
        check(exact, "all formerly valid interpolation samples stay bit-identical");
    }
}

void testClassicChorus()
{
    // These are the classic FX slot's production rate/depth, not the OE2 model.
    // The original code reads past the buffer by sample 262358 at 88200 Hz.
    for (float rate : {88200.0f, 96000.0f})
    {
        cs80::Chorus chorus;
        chorus.prepare(rate);
        bool valid = true;
        double energy = 0.0;
        for (int n = 0; n < 300000; ++n)
        {
            const float input = 0.1f * std::sin(cs80::kTwoPi * (float) n * 220.0f / rate);
            const auto y = chorus.process(input, -input, 0.62f, 0.62f, true, false);
            valid = valid && std::isfinite(y.l) && std::isfinite(y.r)
                && std::abs(y.l) < 0.2f && std::abs(y.r) < 0.2f;
            energy += (double) y.l * y.l + (double) y.r * y.r;
        }
        check(valid && energy > 1.0, "classic chorus remains finite and audible through repeated wraps");
    }
}
}

int main()
{
    testRoundedWrap();
    testUnchangedInterpolation();
    testClassicChorus();
    std::printf("Delay line: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
