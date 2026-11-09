#include "core/Dsp.h"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <limits>

int main()
{
    std::size_t checks = 0, failures = 0;
    const auto same = [&](float expected, float actual)
    {
        ++checks;
        if (std::bit_cast<std::uint32_t>(expected) != std::bit_cast<std::uint32_t>(actual)) ++failures;
    };
    cs80::PitchPowMemo pitchMemo;
    const auto checkPitch = [&](float note, float tuning)
    { same(cs80::midiToHz(note, tuning), cs80::midiToHz(note, tuning, pitchMemo)); };
    std::uint32_t random = 0x719fea61;
    float note = 60.f;
    for (int i = 0; i < 1000000; ++i)
    {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        if (i % 4 == 0) note = -100.f + (random % 320000) * .001f;
        checkPitch(note, i % 3 ? 440.f : 415.f);
    }
    for (float pitch : { 0.f, -0.f, 69.f, -10000.f, 10000.f,
                         std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::quiet_NaN() })
        for (float tuning : { 0.f, -0.f, 440.f, -440.f,
                              std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN() })
        { checkPitch(pitch, tuning); checkPitch(pitch, tuning); }

    cs80::PitchPowMemo first, copy;
    const auto checkExponent = [&](cs80::PitchPowMemo& memo, float x)
    { same(std::pow(2.f, x), memo(x)); };
    const float infinity = std::numeric_limits<float>::infinity();
    for (float pivot : { -150.f, -149.f, -126.f, -1.f, -0.f, 0.f, 1.f, 127.f, 128.f })
        for (float x : { std::nextafter(pivot, -infinity), pivot, std::nextafter(pivot, infinity) })
        {
            checkExponent(first, x); checkExponent(first, x);
            copy = first;
            checkExponent(copy, x); checkExponent(copy, 1.f); checkExponent(first, x);
        }
    random = 0x936814ab;
    for (int i = 0; i < 65536; ++i)
    {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        const float x = std::bit_cast<float>(random);
        checkExponent(first, x); checkExponent(first, x);
        if (i % 37 == 0) { copy = first; checkExponent(copy, x); }
    }
    std::printf("%zu checks, %zu failures\n", checks, failures);
    return failures ? 1 : 0;
}
