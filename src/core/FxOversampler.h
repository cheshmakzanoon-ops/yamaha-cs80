#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// Per-slot oversampling for the modern drive (IMPROVEMENT_PLAN Phase 6). Modern-FX
// numerics, not CS-80 circuitry.
//
// Each 2x stage is a polyphase IIR half-band filter: two parallel chains of first-order
// all-pass sections in z^-2, H(z) = 0.5 * (A0(z^2) + z^-1 A1(z^2)). The coefficients are
// the elliptic half-band design used by Laurent de Soras' HIIR library (after Valenzuela
// and Constantinides, "Digital signal processing schemes for efficient interpolation and
// decimation", IEE Proc. 1983): passband to (0.25 - transition/2) fs, stopband from
// (0.25 + transition/2) fs, attenuation chosen below. The audio of interest lies under
// 24 kHz at every supported host rate, so a wide transition keeps the stages short. No
// latency is reported for these filters: their group delay is a few samples.
namespace cs80::fx
{

inline constexpr int kMaxHalfbandCoefficients = 16;
inline constexpr double kHalfbandTransition = 0.1;       // of the high sample rate
inline constexpr double kHalfbandAttenuationDb = 100.0;  // stopband target

struct HalfbandDesign
{
    std::array<double, kMaxHalfbandCoefficients> coefficients {};
    int count = 0;
};

namespace detail
{
inline double halfbandAccNum(double q, int order, int c)
{
    double acc = 0.0, term = 0.0;
    int i = 0, sign = 1;
    do
    {
        term = std::pow(q, (double) (i * (i + 1))) * std::sin((double) ((i * 2 + 1) * c) * 3.14159265358979323846 / order) * sign;
        acc += term;
        sign = -sign;
        ++i;
    } while (std::abs(term) > 1.0e-100 && i < 1000);
    return acc;
}

inline double halfbandAccDen(double q, int order, int c)
{
    double acc = 0.0, term = 0.0;
    int i = 1, sign = -1;
    do
    {
        term = std::pow(q, (double) (i * i)) * std::cos((double) (i * 2 * c) * 3.14159265358979323846 / order) * sign;
        acc += term;
        sign = -sign;
        ++i;
    } while (std::abs(term) > 1.0e-100 && i < 1000);
    return acc;
}
} // namespace detail

// Elliptic polyphase half-band design for a stopband attenuation and transition width.
inline HalfbandDesign designHalfband(double attenuationDb = kHalfbandAttenuationDb,
                                     double transition = kHalfbandTransition)
{
    constexpr double pi = 3.14159265358979323846;
    double k = std::tan((1.0 - transition * 2.0) * pi / 4.0);
    k *= k;
    const double kksqrt = std::pow(1.0 - k * k, 0.25);
    const double e = 0.5 * (1.0 - kksqrt) / (1.0 + kksqrt);
    const double e4 = e * e * e * e;
    const double q = e * (1.0 + e4 * (2.0 + e4 * (15.0 + 150.0 * e4)));

    // Smallest odd order whose attenuation reaches the target.
    const double attn = std::pow(10.0, -attenuationDb / 10.0);
    const double a = attn / (1.0 - attn);
    int order = (int) std::ceil(std::log(a * a / 16.0) / std::log(q));
    if (order % 2 == 0) ++order;
    order = std::max(order, 3);
    HalfbandDesign design;
    design.count = std::min((order - 1) / 2, kMaxHalfbandCoefficients);
    order = design.count * 2 + 1;
    for (int index = 0; index < design.count; ++index)
    {
        const int c = index + 1;
        const double num = detail::halfbandAccNum(q, order, c) * std::pow(q, 0.25);
        const double den = detail::halfbandAccDen(q, order, c) + 0.5;
        const double ww = num / den;
        const double wwsq = ww * ww;
        const double x = std::sqrt((1.0 - wwsq * k) * (1.0 - wwsq / k)) / (1.0 + wwsq);
        design.coefficients[(std::size_t) index] = (1.0 - x) / (1.0 + x);
    }
    return design;
}

inline const HalfbandDesign& sharedHalfband()
{
    static const HalfbandDesign design = designHalfband();
    return design;
}

// One all-pass chain running at the LOW rate of a stage: y = c (x - y1) + x1 per section.
class AllpassChain
{
public:
    void setup(const HalfbandDesign& d, int phase) noexcept
    {
        count = 0;
        for (int i = phase; i < d.count; i += 2)
            coefficient[(std::size_t) count++] = (float) d.coefficients[(std::size_t) i];
        reset();
    }
    void reset() noexcept { x1.fill(0.0f); y1.fill(0.0f); }
    float process(float x) noexcept
    {
        for (int i = 0; i < count; ++i)
        {
            const float y = coefficient[(std::size_t) i] * (x - y1[(std::size_t) i]) + x1[(std::size_t) i];
            x1[(std::size_t) i] = x;
            y1[(std::size_t) i] = y;
            x = y;
        }
        return x;
    }

private:
    std::array<float, kMaxHalfbandCoefficients> coefficient {}, x1 {}, y1 {};
    int count = 0;
};

class HalfbandUpsampler
{
public:
    void setup(const HalfbandDesign& d) noexcept { path0.setup(d, 0); path1.setup(d, 1); }
    void reset() noexcept { path0.reset(); path1.reset(); }
    // One low-rate sample in, two high-rate samples out (earlier first).
    void process(float x, float& first, float& second) noexcept
    {
        first = path0.process(x);
        second = path1.process(x);
    }

private:
    AllpassChain path0, path1;
};

class HalfbandDownsampler
{
public:
    void setup(const HalfbandDesign& d) noexcept { path0.setup(d, 0); path1.setup(d, 1); }
    void reset() noexcept { path0.reset(); path1.reset(); }
    // Two high-rate samples in (earlier first), one low-rate sample out.
    float process(float first, float second) noexcept
    {
        return 0.5f * (path0.process(second) + path1.process(first));
    }

private:
    AllpassChain path0, path1;
};

// 1x, 2x, 4x or 8x around a per-sample function, one channel.
class Oversampler
{
public:
    static constexpr int kMaxStages = 3;
    static constexpr int kMaxFactor = 1 << kMaxStages;

    Oversampler()
    {
        const auto& d = sharedHalfband();
        for (auto& u : up) u.setup(d);
        for (auto& dn : down) dn.setup(d);
    }

    void reset() noexcept
    {
        for (auto& u : up) u.reset();
        for (auto& d : down) d.reset();
    }

    // stages: 0 (1x) .. 3 (8x). fn(float) -> float runs at base rate * 2^stages.
    template <typename Fn>
    float process(float x, int stages, Fn&& fn) noexcept
    {
        stages = std::clamp(stages, 0, kMaxStages);
        std::array<float, kMaxFactor> low {}, high {};
        low[0] = x;
        int n = 1;
        // Each stage's filters see their samples in time order.
        for (int s = 0; s < stages; ++s)
        {
            for (int i = 0; i < n; ++i)
                up[(std::size_t) s].process(low[(std::size_t) i], high[(std::size_t) (2 * i)], high[(std::size_t) (2 * i + 1)]);
            n *= 2;
            std::copy_n(high.begin(), n, low.begin());
        }
        for (int i = 0; i < n; ++i)
            low[(std::size_t) i] = fn(low[(std::size_t) i]);
        for (int s = stages - 1; s >= 0; --s)
        {
            n /= 2;
            for (int i = 0; i < n; ++i) // in place: sample i is written after 2i and 2i+1 are read
                low[(std::size_t) i] = down[(std::size_t) s].process(low[(std::size_t) (2 * i)], low[(std::size_t) (2 * i + 1)]);
        }
        return low[0];
    }

private:
    std::array<HalfbandUpsampler, kMaxStages> up;
    std::array<HalfbandDownsampler, kMaxStages> down;
};

} // namespace cs80::fx
