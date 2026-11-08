// The modern plate's brightness must represent physical filter times, not a fixed
// number of internal samples. These checks intentionally include damped tails: the
// original modern-FX decay checks use brightness 1, where tank damping is bypassed.
#include "core/FxModels.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace cs80::fx;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* what)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", what); }
}

// Stereo Schroeder integration in a physical octave band. Compare decay in seconds
// rather than raw impulse energy, which changes with the sampling rate.
double bandRt60(const std::vector<StereoSample>& ir, double rate, double hz)
{
    const auto hp = Eq::highPass(rate, hz / std::sqrt(2.0), 0.7071);
    const auto lp = Eq::lowPass(rate, hz * std::sqrt(2.0), 0.7071);
    std::array<Eq, 2> hp1 { hp, hp }, hp2 { hp, hp }, lp1 { lp, lp }, lp2 { lp, lp };
    std::vector<double> energy(ir.size());
    for (std::size_t i = 0; i < ir.size(); ++i)
    {
        const float in[2] = { ir[i].l, ir[i].r };
        for (std::size_t c = 0; c < 2; ++c)
        {
            const double y = lp2[c].process(lp1[c].process(hp2[c].process(hp1[c].process(in[c]))));
            energy[i] += y * y;
        }
    }
    for (std::size_t i = energy.size() - 1; i-- > 0;)
        energy[i] += energy[i + 1];
    if (!(energy[0] > 0.0)) return -1.0;
    double t5 = -1.0;
    for (std::size_t i = 0; i < energy.size(); ++i)
    {
        const double db = 10.0 * std::log10(std::max(energy[i] / energy[0], 1.0e-300));
        if (t5 < 0.0 && db <= -5.0) t5 = (double) i / rate;
        if (db <= -35.0) return t5 < 0.0 ? -1.0 : 2.0 * ((double) i / rate - t5);
    }
    return -1.0;
}

std::array<double, 3> decayBands(double rate, float brightness)
{
    PlateReverbFx plate;
    plate.prepare(rate);
    std::vector<StereoSample> ir((std::size_t) (7.0 * rate));
    for (std::size_t i = 0; i < ir.size(); ++i)
        ir[i] = plate.process(i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, 0.7f, brightness);
    return { bandRt60(ir, rate, 1000.0), bandRt60(ir, rate, 4000.0), bandRt60(ir, rate, 12000.0) };
}

void testPhysicalDecay()
{
    // Matched-pole conversion preserves physical pole decay, not an exactly equal
    // digital frequency response. Allow 8% for that approximation, tank modulation
    // and rounded delay lengths. The original dark 12 kHz tail differs by >50%.
    constexpr double maxRelativeDifference = 0.08;
    for (float brightness : { 0.0f, 0.5f, 1.0f })
    {
        const auto reference = decayBands(96000.0, brightness);
        for (double rate : { 88200.0, 192000.0, 384000.0 })
        {
            const auto measured = decayBands(rate, brightness);
            for (std::size_t band = 0; band < reference.size(); ++band)
            {
                std::printf("plate brightness %.1f rate %.0f band %zu: %.5f / %.5f s\n",
                            brightness, rate, band, measured[band], reference[band]);
                check(reference[band] > 0.0 && measured[band] > 0.0
                      && std::abs(measured[band] / reference[band] - 1.0) < maxRelativeDifference,
                      "plate band decay stays stable in physical time across internal rates");
            }
        }
    }
}

bool identical(StereoSample a, StereoSample b)
{
    return a.l == b.l && a.r == b.r;
}

void testPrepareAndReset()
{
    for (float brightness : { 0.0f, 0.5f, 1.0f })
        for (const auto rates : { std::array<double, 2> { 96000.0, 384000.0 },
                                  std::array<double, 2> { 384000.0, 88200.0 },
                                  std::array<double, 2> { 88200.0, 96000.0 } })
        {
            PlateReverbFx reused, fresh;
            reused.prepare(rates[0]);
            // Establish coefficients and excite all parts of the old tank.
            for (int i = 0; i < (int) (0.8 * rates[0]); ++i)
                (void) reused.process(0.1f, -0.05f, 0.7f, brightness);
            reused.prepare(rates[1]);
            fresh.prepare(rates[1]);
            bool same = true;
            for (int i = 0; i < (int) (0.8 * rates[1]); ++i)
            {
                const float x = i < 32 ? 0.25f : 0.0f;
                same = identical(reused.process(x, x, 0.7f, brightness),
                                 fresh.process(x, x, 0.7f, brightness)) && same;
            }
            check(same, "reprepare with unchanged brightness matches a fresh plate at the new rate");
            reused.reset();
            fresh.prepare(rates[1]);
            same = true;
            for (int i = 0; i < (int) (0.8 * rates[1]); ++i)
            {
                // Change both controls after reset to exercise cache updates as well.
                const float b = i < (int) (0.3 * rates[1]) ? brightness : 1.0f - brightness;
                const float x = i < 32 ? 0.25f : 0.0f;
                same = identical(reused.process(x, x, 0.4f, b), fresh.process(x, x, 0.4f, b)) && same;
            }
            check(same, "reset and live brightness changes match fresh plate processing");
        }
}
}

int main()
{
    testPhysicalDecay();
    testPrepareAndReset();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
