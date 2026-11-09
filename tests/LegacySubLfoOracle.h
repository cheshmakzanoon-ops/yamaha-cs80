#pragma once

// Test-owned reference, intentionally independent of core::Lfo and core::Rng.
// Two allowed product/add contracts, not a per-sample rounding tolerance.
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace legacy_sub_oracle {
static_assert(std::numeric_limits<float>::is_iec559
              && std::numeric_limits<float>::digits == 24 && sizeof(float) == 4);

inline float rounded(float x) noexcept
{
    volatile float stored = x;
    return stored;
}
inline bool sameBits(float a, float b) noexcept
{
    return std::isfinite(a) && std::isfinite(b)
        && std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

struct Observation { float speed = 4.2f, value = 0.0f; };
enum class Arithmetic { split, fused };

struct Track
{
    explicit Track(Arithmetic law) noexcept : arithmetic(law) {}

    void tick(float target, float rate, int wave) noexcept
    {
        const float delta = rounded(target - current.speed);
        current.speed = arithmetic == Arithmetic::fused
            ? std::fma(delta, 0.0015f, current.speed)
            : rounded(current.speed + rounded(delta * 0.0015f));
        const float before = phase;
        phase = rounded(phase + rounded(current.speed / rate));
        phase = rounded(phase - std::floor(phase));
        const bool wrapped = phase < before;
        wraps += wrapped ? 1u : 0u;
        ++ticks;
        constexpr float twoPi = 6.28318530717958647692f;
        switch (wave)
        {
            case 0: current.value = std::sin(twoPi * phase); break;
            case 1: current.value = 1.0f - 4.0f * std::abs(phase - 0.5f); break;
            case 2: current.value = 2.0f * phase - 1.0f; break;
            case 3: current.value = phase < 0.5f ? 1.0f : -1.0f; break;
            case 4:
                if (!started || wrapped) { held = nextBipolar(); started = true; }
                current.value = held;
                break;
            case 5: current.value = nextBipolar(); break;
            default: current.value = 0.0f; break;
        }
    }

    float nextBipolar() noexcept
    {
        // Literal xorshift32 and exact 24-bit power-of-two mapping. Sharing
        // production RNG here would hide a common implementation defect.
        std::uint32_t x = random;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        random = x;
        ++draws;
        const float unipolar = static_cast<float>(x >> 8) * (1.0f / 16777216.0f);
        return unipolar * 2.0f - 1.0f;
    }

    Arithmetic arithmetic;
    Observation current {};
    float phase = 0.0f, held = 0.0f;
    bool started = false;
    std::uint32_t random = 0x9e3779b9u;
    std::uint64_t ticks = 0, wraps = 0, draws = 0;
};

struct Pair
{
    void tick(float target, float rate, int wave) noexcept
    {
        split.tick(target, rate, wave);
        fused.tick(target, rate, wave);
    }
    bool observe(Observation actual) noexcept
    {
        if (!sameBits(actual.speed, split.current.speed)
            || !sameBits(actual.value, split.current.value)) eligible &= ~1u;
        if (!sameBits(actual.speed, fused.current.speed)
            || !sameBits(actual.value, fused.current.value)) eligible &= ~2u;
        return eligible != 0;
    }
    // Cleared bits cannot revive. One whole trace must match both fields.
    Track split {Arithmetic::split}, fused {Arithmetic::fused};
    unsigned eligible = 3;
};
} // namespace legacy_sub_oracle
