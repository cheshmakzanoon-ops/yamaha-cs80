#pragma once

#include <algorithm>
#include <cmath>

// FX loudness matching (IMPROVEMENT_PLAN Phase 6, "gain-compensated FX"). Loudness is the
// ITU-R BS.1770-4 K-weighted mean square: a high-shelf pre-filter and the RLB high-pass
// (Annex 1, Tables 1 and 2 give the 48 kHz coefficients). The coefficients here are
// re-derived for any rate from the analogue prototypes used by libebur128, which
// reproduce those tables at 48 kHz (checked in tests). Modern-FX metering, not CS-80
// circuitry.
namespace cs80::fx
{

struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    double process(double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0.0; }
};

inline Biquad kWeightingShelf(double sampleRate) noexcept
{
    constexpr double pi = 3.14159265358979323846;
    const double f0 = 1681.974450955533, gainDb = 3.999843853973347, q = 0.7071752369554196;
    const double k = std::tan(pi * f0 / sampleRate);
    const double vh = std::pow(10.0, gainDb / 20.0);
    const double vb = std::pow(vh, 0.4996667741545416);
    const double a0 = 1.0 + k / q + k * k;
    Biquad f;
    f.b0 = (vh + vb * k / q + k * k) / a0;
    f.b1 = 2.0 * (k * k - vh) / a0;
    f.b2 = (vh - vb * k / q + k * k) / a0;
    f.a1 = 2.0 * (k * k - 1.0) / a0;
    f.a2 = (1.0 - k / q + k * k) / a0;
    return f;
}

inline Biquad kWeightingHighPass(double sampleRate) noexcept
{
    constexpr double pi = 3.14159265358979323846;
    const double f0 = 38.13547087602444, q = 0.5003270373238773;
    const double k = std::tan(pi * f0 / sampleRate);
    const double a0 = 1.0 + k / q + k * k;
    Biquad f;
    f.b0 = 1.0;
    f.b1 = -2.0;
    f.b2 = 1.0;
    f.a1 = 2.0 * (k * k - 1.0) / a0;
    f.a2 = (1.0 - k / q + k * k) / a0;
    return f;
}

struct KWeighting
{
    Biquad shelf, highPass;
    void prepare(double sampleRate) noexcept
    {
        shelf = kWeightingShelf(sampleRate);
        highPass = kWeightingHighPass(sampleRate);
    }
    void reset() noexcept { shelf.reset(); highPass.reset(); }
    double process(double x) noexcept { return highPass.process(shelf.process(x)); }
};

// Stereo in/out loudness matcher. It measures K-weighted energy of the slot input and of
// the slot output before this gain, and steers the gain toward sqrt(Ein / Eout).
// Ballistics (engineering choices for a modern control):
// - 1 s energy averages: slow enough not to follow the music, ~ a short-term meter;
// - the gain only moves while the input is within 20 dB of its recent peak and above
//   -70 dBFS, so release tails and silence hold the last gain instead of pumping;
// - +/-18 dB range and a 150 ms gain slew.
class LoudnessMatcher
{
public:
    static constexpr double kEnergySeconds = 1.0;
    static constexpr double kPeakSeconds = 3.0;
    static constexpr double kGainSeconds = 0.15;
    static constexpr double kGateFloor = 1.0e-7;      // -70 dBFS mean square
    static constexpr double kGateBelowPeak = 0.01;    // -20 dB
    static constexpr double kMaxGain = 7.943282347;   // +18 dB

    void prepare(double sampleRate) noexcept
    {
        for (auto* k : { &inL, &inR, &outL, &outR })
            k->prepare(sampleRate);
        energyCoef = 1.0 - std::exp(-1.0 / (kEnergySeconds * sampleRate));
        peakCoef = std::exp(-1.0 / (kPeakSeconds * sampleRate));
        gainCoef = 1.0 - std::exp(-1.0 / (kGainSeconds * sampleRate));
        reset();
    }

    void reset() noexcept
    {
        for (auto* k : { &inL, &inR, &outL, &outR })
            k->reset();
        energyIn = energyOut = peakIn = 0.0;
        gain = 1.0;
        target = 1.0;
    }

    // Returns the gain to apply to (outLeft, outRight) for this sample.
    float process(float inLeft, float inRight, float outLeft, float outRight) noexcept
    {
        const double a = inL.process(inLeft), b = inR.process(inRight);
        const double c = outL.process(outLeft), d = outR.process(outRight);
        energyIn += energyCoef * (0.5 * (a * a + b * b) - energyIn);
        energyOut += energyCoef * (0.5 * (c * c + d * d) - energyOut);
        peakIn = std::max(energyIn, peakIn * peakCoef);
        if (energyIn > kGateFloor && energyIn > kGateBelowPeak * peakIn && energyOut > 1.0e-12)
            target = std::clamp(std::sqrt(energyIn / energyOut), 1.0 / kMaxGain, kMaxGain);
        gain += gainCoef * (target - gain);
        return (float) gain;
    }

    double currentGain() const noexcept { return gain; }

private:
    KWeighting inL, inR, outL, outR;
    double energyCoef = 0.0, peakCoef = 0.0, gainCoef = 0.0;
    double energyIn = 0.0, energyOut = 0.0, peakIn = 0.0;
    double gain = 1.0, target = 1.0;
};

} // namespace cs80::fx
