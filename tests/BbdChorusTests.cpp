// BbdChorus checks the declared OE2 delay-path model against its own specification
// (docs/BBD_CHORUS_MODEL.md). Agreement with these equations is not agreement with a CS-80.
#include "core/BbdChorus.h"

#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

using cs80::BbdChorus;
using cs80::Chorus;
using Complex = std::complex<double>;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* what)
{
    ++checks;
    if (! ok)
    {
        if (failures < 20)
            std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

constexpr double kPi = 3.14159265358979323846;
// Manufacturer convention: 512 stages, two clock phases per period. Kept as a literal so a
// changed library constant cannot also change the expectation.
constexpr double kPeriodsPerDelay = 256.0;
constexpr double kWetScale = BbdChorus::kWetGain * BbdChorus::kWetPolarity;

// Analytic OE2 delay path: input LP, BBD delay, output coupling, reconstruction LP,
// interstage coupling. Both low-pass sections are the printed Sallen-Key transfer.
Complex skLowPass(double hz)
{
    const double w = hz / BbdChorus::lowPassHz();
    return 1.0 / Complex(1.0 - w * w, w / BbdChorus::lowPassQ());
}
Complex highPass(double hz, double cornerHz)
{
    const Complex x(0.0, hz / cornerHz);
    return x / (1.0 + x);
}
Complex expectedWet(double hz, double clockHz)
{
    const double tau = kPeriodsPerDelay / clockHz;
    return skLowPass(hz) * skLowPass(hz) * highPass(hz, BbdChorus::kOutputCouplingHz)
         * highPass(hz, BbdChorus::kInterstageHighPassHz) * std::polar(1.0, -2.0 * kPi * hz * tau);
}

// Wet-only signal in chorus mode: out = in + kWet * kPolarity * wet.
struct Rig
{
    BbdChorus chorus;
    double rate;
    explicit Rig(double sampleRate) : rate(sampleRate) { chorus.prepare((float) sampleRate); }
    float wet(float in, float speed, float depth)
    {
        const auto out = chorus.process(in, in, speed, depth, true, false);
        return (float) ((out.l - in) / kWetScale);
    }
};

Complex toneResponse(double rate, double hz)
{
    Rig rig(rate);
    const int window = (int) (rate / 5.0);           // multiples of 5 Hz fit whole cycles
    const int settle = (int) rate;                   // 1.59 Hz coupling: tau = 100 ms
    Complex acc;
    for (int n = 0; n < settle + window; ++n)
    {
        const double phase = 2.0 * kPi * hz * (double) n / rate;
        const float w = rig.wet((float) std::sin(phase), 1.0f, 0.0f);
        if (n >= settle)
            acc += (double) w * std::polar(1.0, -phase);
    }
    return acc * (2.0 / (double) window) * Complex(0.0, 1.0);   // sine reference -> complex gain
}

void responseMatchesCircuit()
{
    for (double rate : { 88200.0, 96000.0, 192000.0 })
    {
        for (double hz : { 5.0, 20.0, 100.0, 160.0, 500.0, 1000.0, 2000.0 })
        {
            const Complex got = toneResponse(rate, hz);
            const Complex want = expectedWet(hz, BbdChorus::kNominalClockHz);
            check(std::abs(std::abs(got) / std::abs(want) - 1.0) < 0.03, "wet magnitude follows OE2 filters");
            double dphi = std::arg(got / want);
            check(std::abs(dphi) < 0.06, "wet phase follows 256-period delay and filter phase");
        }
        const double low = std::abs(toneResponse(rate, 2000.0));
        const double high = std::abs(toneResponse(rate, 20000.0));
        check(high < low, "reconstruction low-pass rolls off above the corner");
    }
}

// Oracle for the clock count: the same LFO and clock law, evaluated independently.
struct ClockOracle
{
    double rate, lfo = 0.0, periods = 0.0;
    std::vector<double> count;                        // count[n] = clock periods at sample n
    ClockOracle(double sampleRate, float speed, float depth, int samples) : rate(sampleRate)
    {
        for (int n = 0; n < samples; ++n)
        {
            lfo += (double) speed / rate;
            lfo -= std::floor(lfo);
            const double octaves = (double) depth * BbdChorus::kClockModulationOctaves * std::sin(2.0 * kPi * lfo);
            periods += BbdChorus::kNominalClockHz * std::exp2(octaves) / rate;
            count.push_back(periods);
        }
    }
    // Fractional sample index at which 256 more periods have elapsed since sample n0.
    double arrival(int n0) const
    {
        const double target = count[(size_t) n0] + kPeriodsPerDelay;
        size_t i = (size_t) n0;
        while (i + 1 < count.size() && count[i + 1] < target)
            ++i;
        return (double) i + (target - count[i]) / (count[i + 1] - count[i]);
    }
};

// Sub-sample position of the delayed pulse's main lobe (parabola through the peak).
double pulsePeak(double rate, float speed, float depth, int pulseAt, int total)
{
    Rig rig(rate);
    std::vector<double> wet((size_t) total);
    for (int n = 0; n < total; ++n)
    {
        const double d = (double) (n - pulseAt) / 12.0;
        wet[(size_t) n] = rig.wet((float) std::exp(-0.5 * d * d), speed, depth);
    }
    int peak = pulseAt + 50;
    for (int n = pulseAt + 50; n < total - 1; ++n)
        if (wet[(size_t) n] > wet[(size_t) peak])
            peak = n;
    const double a = wet[(size_t) peak - 1], b = wet[(size_t) peak], c = wet[(size_t) peak + 1];
    return (double) peak + 0.5 * (a - c) / (a - 2.0 * b + c);
}

void delayFollowsIntegratedClock()
{
    const double rate = 96000.0;
    const float speed = 3.0f;
    for (float depth : { 0.5f, 1.0f })
    {
        for (int pulseAt : { 20000, 20019, 20041 })    // different LFO phases and fractions
        {
            const int total = pulseAt + 700;
            const ClockOracle moving(rate, speed, depth, total);
            const ClockOracle still(rate, speed, 0.0f, total);
            const double expectShift = moving.arrival(pulseAt) - still.arrival(pulseAt);
            const double shift = pulsePeak(rate, speed, depth, pulseAt, total)
                               - pulsePeak(rate, speed, 0.0f, pulseAt, total);
            check(std::abs(shift - expectShift) < 0.6, "arrival time follows integrated clock count");
            check(std::abs(expectShift) > 3.0 || depth < 0.5f || pulseAt == 20000,
                  "fixture exercises a real delay change");
        }
    }
    // Full depth must move the delay roughly twice as far as half depth at a fixed phase.
    const ClockOracle half(rate, speed, 0.5f, 21000), full(rate, speed, 1.0f, 21000), flat(rate, speed, 0.0f, 21000);
    double widest = 0.0;
    for (int n = 10000; n < 20000; n += 97)
        widest = std::max(widest, std::abs(full.arrival(n) - flat.arrival(n)));
    check(widest > 50.0, "depth 1 swings the delay by tens of samples at 96 kHz");
    check(std::abs(half.arrival(15000) - flat.arrival(15000)) < std::abs(full.arrival(15000) - flat.arrival(15000)) + 1e-9,
          "depth scales the clock excursion");
}

void policyCompatibility()
{
    // Off: exact pass-through, even while the delay path keeps running.
    {
        Rig rig(96000.0);
        bool exact = true;
        for (int n = 0; n < 4000; ++n)
        {
            const float in = 0.3f * std::sin(0.05f * (float) n);
            const auto out = rig.chorus.process(in, -in, 0.55f, 0.55f, false, false);
            exact = exact && out.l == in && out.r == -in;
        }
        check(exact, "chorus off passes input bit-exactly");
    }
    // Tremolo keeps the generic amplitude law of the legacy chorus, bit for bit.
    {
        BbdChorus bbd;
        Chorus legacy;
        bbd.prepare(96000.0f);
        legacy.prepare(96000.0f);
        bool same = true;
        for (int n = 0; n < 6000; ++n)
        {
            const float in = 0.4f * std::sin(0.031f * (float) n);
            const bool on = n % 2500 < 2000;
            const auto a = bbd.process(in, 0.5f * in, 1.7f, 0.8f, on, true);
            const auto b = legacy.process(in, 0.5f * in, 1.7f, 0.8f, on, true);
            same = same && a.l == b.l && a.r == b.r;
        }
        check(same, "tremolo output is identical to the legacy law");
    }
    // The delay path is state-continuous: enabling it after a tone is already filled
    // gives a wet signal at once, where a freshly reset chain is still empty.
    {
        Rig warm(96000.0), cold(96000.0);
        for (int n = 0; n < 6000; ++n)
            warm.chorus.process(0.5f * std::sin(0.06f * (float) n), 0.0f, 0.55f, 0.55f, false, false);
        float warmPeak = 0.0f, coldPeak = 0.0f;
        for (int n = 6000; n < 6040; ++n)
        {
            const float in = 0.5f * std::sin(0.06f * (float) n);
            warmPeak = std::max(warmPeak, std::abs(warm.chorus.process(in, in, 0.55f, 0.55f, true, false).l - in));
            coldPeak = std::max(coldPeak, std::abs(cold.chorus.process(in, in, 0.55f, 0.55f, true, false).l - in));
        }
        check(warmPeak > 0.01f && coldPeak < 0.001f, "delay path keeps running while the effect is off");
    }
    // Reset empties the chain.
    {
        Rig rig(96000.0);
        for (int n = 0; n < 5000; ++n)
            rig.wet(0.5f * std::sin(0.06f * (float) n), 0.55f, 0.55f);
        rig.chorus.reset();
        float peak = 0.0f;
        for (int n = 0; n < 100; ++n)
            peak = std::max(peak, std::abs(rig.wet(0.0f, 0.55f, 0.55f)));
        check(peak == 0.0f, "reset empties the delay chain");
    }
}

void stability()
{
    for (double rate : { 16000.0, 88200.0, 192000.0, 1536000.0 })
    {
        Rig rig(rate);
        bool finite = true;
        float peak = 0.0f;
        for (int n = 0; n < (int) (rate * 1.5); ++n)
        {
            const float in = 0.8f * std::sin(0.4f * (float) n) + 0.1f * (float) (((long long) n * 7919) % 200 - 100) / 100.0f;
            const float w = rig.wet(in, 8.0f, 1.0f);
            finite = finite && std::isfinite(w);
            peak = std::max(peak, std::abs(w));
        }
        check(finite && peak < 10.0f, "extreme speed and depth stay finite and bounded");
    }
}
} // namespace

int main()
{
    check(std::abs(BbdChorus::lowPassHz() - 14390.0) < 20.0, "printed Sallen-Key corner is 14.39 kHz");
    check(std::abs(BbdChorus::lowPassQ() - 0.8135) < 0.001, "printed Sallen-Key Q is 0.8135");
    check(std::abs(BbdChorus::kNominalClockHz - 1.0e6 / std::sqrt(7.0 * 12.5)) < 1.0,
          "nominal clock is the geometric centre of the 7-12.5 us interval");
    responseMatchesCircuit();
    delayFollowsIntegratedClock();
    policyCompatibility();
    stability();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
