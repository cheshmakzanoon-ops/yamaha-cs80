#include "plugin/LfoScopePreview.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>

namespace
{
int failures = 0;
void check(bool ok, const char* message)
{
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}

// Independent equations and RNG, rather than calling production lfoShape/ModulationLfo.
double expected(int wave, double position)
{
    const int cycle = static_cast<int>(std::floor(position));
    const double p = position - cycle;
    switch (wave)
    {
        case 0: return std::sin(6.2831853071795864769 * p);
        case 1: return p < 0.25 ? 4.0 * p : p < 0.75 ? 2.0 - 4.0 * p : 4.0 * p - 4.0;
        case 2: return p < 0.5 ? 2.0 * p : 2.0 * p - 2.0;
        case 3: return p < 0.5 ? -2.0 * p : 2.0 - 2.0 * p;
        case 4: return p < 0.5 ? 1.0 : -1.0;
        default:
        {
            std::uint32_t state = 0x41c64e6du;
            const auto next = [&]()
            {
                state ^= state << 13; state ^= state >> 17; state ^= state << 5;
                return static_cast<double>(state >> 8) / 8388608.0 - 1.0;
            };
            double from = next();
            for (int i = 0; i < cycle; ++i) from = next();
            if (wave == 5) return from;
            const double to = next();
            return from + (to - from) * (0.5 - 0.5 * std::cos(3.14159265358979323846 * p));
        }
    }
}

bool same(const std::vector<cs80::ui::LfoPreviewPoint>& a, const std::vector<cs80::ui::LfoPreviewPoint>& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].x != b[i].x || a[i].value != b[i].value) return false;
    return true;
}
}

int main()
{
    using cs80::ui::makeLfoScopePreview;
    for (int wave = 0; wave < cs80::kLfoWaveCount; ++wave)
    {
        for (const float rate : { 0.05f, 0.37f, 1.0f, 3.7f, 20.0f })
        {
            const auto points = makeLfoScopePreview(wave, rate);
            check(points.size() > 2 && points.size() < 5500, "preview has a bounded nonempty point count");
            check(points.front().x == 0.0f && points.back().x == 1.0f, "preview spans exactly four seconds");
            check(same(points, makeLfoScopePreview(wave, rate)), "all shapes, including random, are stable across repaints");
            for (size_t i = 0; i < points.size(); ++i)
            {
                const auto pt = points[i];
                check(std::isfinite(pt.x) && std::isfinite(pt.value) && pt.x >= 0.0f && pt.x <= 1.0f
                          && pt.value >= -1.000001f && pt.value <= 1.000001f,
                      "preview points remain finite and within plot bounds");
                if (i == 0) continue;
                const auto previous = points[i - 1];
                check(previous.x <= pt.x, "preview runs monotonically from left to right");
                if (pt.x == previous.x) continue; // Explicit vertical edge.
                const double position = 4.0 * rate * (static_cast<double>(previous.x) + pt.x) * 0.5;
                const double midpoint = (static_cast<double>(previous.value) + pt.value) * 0.5;
                check(std::abs(midpoint - expected(wave, position)) < 0.0013,
                      "rendered segment matches independently calculated waveform, including random interpolation");
                if (wave == 4 || wave == 5)
                    check(previous.value == pt.value, "square and S&H contain only horizontal holds and vertical jumps");
            }
        }
    }

    // Phase convention, orientation, corner extrema and exact edge placement at 1 Hz.
    for (int wave = 0; wave < 7; ++wave)
    {
        const auto points = makeLfoScopePreview(wave, 1.0f);
        for (double p : { 0.0, 0.25, 0.5, 0.75, 1.0, 3.5, 4.0 })
        {
            float actual = 9.0f;
            int atBoundary = 0;
            for (const auto pt : points)
                if (pt.x == static_cast<float>(p / 4.0)) { actual = pt.value; ++atBoundary; }
            check(std::abs(actual - expected(wave, p)) < 0.000001,
                  "knots use DSP phase convention across every analytic and random waveform");
            const bool jump = p > 0.0 && (((wave == 2 || wave == 3) && p - std::floor(p) == 0.5)
                              || (wave == 4 && std::fmod(p, 0.5) == 0.0)
                              || (wave == 5 && p == std::floor(p)));
            check(atBoundary == (jump ? 2 : 1), "discontinuities include both endpoint values at the same time");
        }
    }
    for (float rate : { -100.0f, 0.0f, std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() })
        check(same(makeLfoScopePreview(6, rate), makeLfoScopePreview(6, 0.05f)), "invalid or low rate uses finite minimum");
    check(same(makeLfoScopePreview(6, 1e20f), makeLfoScopePreview(6, 20.0f)), "high rate cannot exceed bounded preview work");
    check(same(makeLfoScopePreview(-1, 1.0f), makeLfoScopePreview(0, 1.0f)), "waveform index has a lower bound");
    check(same(makeLfoScopePreview(99, 1.0f), makeLfoScopePreview(6, 1.0f)), "waveform index has an upper bound");
    std::printf("LFO scope preview: %d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
