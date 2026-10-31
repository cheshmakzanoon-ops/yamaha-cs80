// ws4 Step 1: circuit-engine SUB (IG00150) speed converter and band-limited waveforms,
// held to the offline truth model in src/reference/Ig00150Sub.h by the pre-registered
// bounds in tests/Ws4Tolerances.h. Agreement with these equations is not agreement with
// a measured CS-80; see docs/workstreams/ws4-circuits.md.
#include "core/Engine.h"
#include "reference/Ig00150Sub.h"
#include "Ws4Tolerances.h"

#include <cmath>
#include <complex>
#include <cstdio>
#include <string>
#include <vector>

using namespace cs80;
namespace ref = cs80::reference::ig00150;
namespace tol = cs80::ws4_tolerances::sub;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (! ok)
    {
        if (failures < 40)
            std::fprintf(stderr, "FAIL: %s\n", what.c_str());
        ++failures;
    }
}
double relative(double a, double b) { return std::abs(a - b) / std::abs(b); }

void lawMatchesReference()
{
    for (double base : { 0.7, 1.3, 4.2, 6.0, 20.0, 60.0 })
        for (double depth : { 0.0, 0.25, 0.5, 1.0 })
            for (double pressure : { 0.0, 0.3, 1.0 })
            {
                const double got = OriginalSubOscillator::circuitTargetRate(base, depth, pressure);
                const double want = ref::speedHz(base, depth, pressure);
                check(relative(got, want) <= tol::kLawRelative,
                      "converter law matches node-current reference at " + std::to_string(base) + " Hz");
            }
    // Anchors: owner p49 endpoints with no touch, the 200 Hz point with full touch.
    check(relative(OriginalSubOscillator::circuitTargetRate(60, 1, 1), 200.0) <= tol::kLawRelative,
          "full SPEED plus full touch reaches the 200 Hz IG00150 anchor");
    check(relative(ref::speedHz(60, 1, 1), 200.0) <= tol::kLawRelative, "reference reaches 200 Hz anchor");
    check(relative(OriginalSubOscillator::circuitTargetRate(0.7, 0, 1), 0.7) <= tol::kLawRelative,
          "zero AT SPEED keeps the 0.7 Hz endpoint");
    check(relative(OriginalSubOscillator::circuitTargetRate(60, 0, 1), 60.0) <= tol::kLawRelative,
          "zero AT SPEED keeps the 60 Hz endpoint");
    // Exponential conversion: equal touch currents add equal octaves at every base speed.
    const double low = std::log2(OriginalSubOscillator::circuitTargetRate(0.7, 0.6, 1) / 0.7);
    const double high = std::log2(OriginalSubOscillator::circuitTargetRate(45, 0.6, 1) / 45);
    check(std::abs(low - high) < 1e-12, "touch adds octaves independent of base speed (not hertz)");
    // The reference solves the touch voltage from the node equation; it must be a
    // positive fraction of the SPEED span for the anchor to be reachable.
    const double touch = ref::fullTouchVolts({});
    check(touch > 0.0 && touch < 10.0, "solved full-touch voltage is a positive fraction of SPEED span");
    std::printf("IG00150: full touch = %.4f V on the 0-10 V SPEED scale, +%.4f octaves\n",
                touch, OriginalSubOscillator::touchOctaves());
}

void smoothingInLogDomain()
{
    const double rate = 96000.0;
    const double coef = -std::expm1(-1.0 / (0.010 * rate));
    OriginalSubOscillator osc;
    check(osc.circuitRate(6.0, coef) == std::exp(std::log(6.0)), "first call after reset starts at target");
    const int tau = 960;
    double r = 0.0;
    for (int n = 0; n < tau; ++n)
        r = osc.circuitRate(20.0, coef);
    const double fraction = std::log(r / 6.0) / std::log(20.0 / 6.0);
    check(std::abs(fraction - (1.0 - std::exp(-1.0))) <= tol::kLogSmoothingFraction,
          "10 ms smoothing acts on log frequency (control node)");
    osc.reset();
    check(osc.circuitRate(33.0, coef) == std::exp(std::log(33.0)), "reset re-primes the control node");
}

// --- Engine: rate from the rendered waveform ------------------------------------------
struct Measured
{
    double hz = 0.0, telemetry = 0.0;
};
Measured renderRate(double hostRate, bool isolation, float base, float depth, float pressure, double seconds)
{
    ParamValues p;
    p[P_sourcePerformance] = 1.0f;
    p[P_parityMode] = isolation ? 1.0f : 0.0f;
    p[P_subOriginalMode] = 1.0f;
    p[P_subOriginalFunction] = 0.0f; // sine: clean zero crossings
    p[P_subOriginalSpeed] = base;
    p[P_subAtSpeed] = depth;
    Engine e;
    e.setParams(p);
    e.prepare(hostRate);
    e.noteOn(60, 0.8f);
    e.polyPressure(60, pressure);
    float frame[2];
    for (int n = 0; n < (int) (hostRate * 0.3); ++n)
        e.render(frame, 1);
    double previous = e.getSubOscillatorValue(), first = -1.0, last = -1.0;
    int crossings = 0;
    const int frames = (int) (hostRate * seconds);
    for (int n = 1; n <= frames; ++n)
    {
        e.render(frame, 1);
        const double value = e.getSubOscillatorValue();
        if (previous < 0.0 && value >= 0.0)
        {
            const double t = (n - 1 + (-previous) / (value - previous)) / hostRate;
            if (crossings == 0) first = t;
            last = t;
            ++crossings;
        }
        previous = value;
    }
    Measured m;
    m.hz = crossings > 1 ? (crossings - 1) / (last - first) : 0.0;
    m.telemetry = e.getSubOscillatorRate();
    return m;
}

void engineAnchors()
{
    struct Case { float base, depth, pressure; };
    for (double hostRate : { 44100.0, 96000.0 })
        for (bool isolation : { false, true })
        {
            const std::string where = (isolation ? "isolation " : "source performance ")
                                    + std::to_string((int) hostRate);
            for (const Case c : { Case{ 60, 1, 1 }, Case{ 60, 0, 1 }, Case{ 6, 1, 0.5f }, Case{ 20, 0.5f, 1 } })
            {
                const auto m = renderRate(hostRate, isolation, c.base, c.depth, c.pressure, 1.0);
                const double want = ref::speedHz(c.base, c.depth, c.pressure);
                check(relative(m.hz, want) <= tol::kMeasuredRateRelative,
                      "rendered SUB period follows IG00150 law: " + where + " " + std::to_string(want));
                check(relative(m.telemetry, want) <= tol::kTelemetryRelative,
                      "settled SUB rate telemetry follows IG00150 law: " + where);
                if (c.base == 60 && c.depth == 1)
                    std::printf("SUB anchor %s: %.6f Hz rendered (reference %.6f)\n", where.c_str(), m.hz, want);
            }
            const auto slow = renderRate(hostRate, isolation, 0.7f, 0.0f, 0.0f, 0.05);
            check(relative(slow.telemetry, 0.7) <= tol::kTelemetryRelative, "0.7 Hz endpoint: " + where);
        }
    // The legacy policy keeps its provisional multiplier: 60 Hz x (1 + 2) = 180 Hz.
    ParamValues p;
    p[P_subOriginalMode] = 1.0f;
    p[P_subOriginalSpeed] = 60.0f;
    p[P_subAtSpeed] = 1.0f;
    Engine e;
    e.setParams(p);
    e.prepare(48000.0);
    e.noteOn(60, 0.8f);
    e.polyPressure(60, 1.0f);
    std::vector<float> block(2 * 4800);
    e.render(block.data(), 4800);
    // The legacy float smoother stalls a few mHz short of its target; that is legacy behaviour.
    check(std::abs(e.getSubOscillatorRate() - 180.0f) < 0.02f, "legacy policy keeps the provisional 180 Hz law");
}

// The engine's circuit SUB stream is the band-limited oscillator, not the legacy one.
void engineUsesBandLimitedPath()
{
    ParamValues p;
    p[P_parityMode] = 1.0f;
    p[P_subOriginalFunction] = 1.0f;
    p[P_subOriginalSpeed] = 173.3f;
    Engine e;
    e.setParams(p);
    e.prepare(44100.0);
    OriginalSubOscillator replay, naive;
    const double coef = -std::expm1(-1.0 / (0.010 * 88200.0));
    bool same = true, differsFromNaive = false;
    float frame[2];
    for (int n = 0; n < 4000; ++n)
    {
        e.render(frame, 1);
        float expected = 0.0f, legacy = 0.0f;
        for (int os = 0; os < 2; ++os)
        {
            const double rate = replay.circuitRate(OriginalSubOscillator::circuitTargetRate(173.3f, 0.0f, 0.0f), coef);
            expected = replay.tickCircuit(rate, 88200.0f, 1);
            legacy = naive.tick(173.3f, 88200.0f, 1);
        }
        same = same && e.getSubOscillatorValue() == expected;
        differsFromNaive = differsFromNaive || e.getSubOscillatorValue() != legacy;
    }
    check(same, "isolation SUB stream is the circuit (band-limited) oscillator");
    check(differsFromNaive, "isolation SUB stream differs from the naive legacy waveform at resets");
}

// --- Alias check ------------------------------------------------------------------------
void fft(std::vector<std::complex<double>>& a)
{
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1)
    {
        const double angle = -2.0 * 3.14159265358979323846 / (double) len;
        const std::complex<double> wl(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0);
            for (std::size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// In-band (20 Hz - 20 kHz) energy outside +/-6 bins of every harmonic, relative to the
// fundamental, with a 4-term Blackman-Harris window (sidelobes near -92 dB).
double inharmonicDb(const std::vector<double>& x, double rate, double f0)
{
    const std::size_t n = x.size();
    std::vector<std::complex<double>> a(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double t = 2.0 * 3.14159265358979323846 * (double) i / (double) n;
        const double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t);
        a[i] = x[i] * w;
    }
    fft(a);
    const double bin = rate / (double) n;
    std::vector<char> harmonic(n / 2, 0);
    for (int h = 1; h * f0 < rate / 2; ++h)
    {
        const long centre = std::lround(h * f0 / bin);
        for (long k = centre - 6; k <= centre + 6; ++k)
            if (k >= 0 && k < (long) n / 2) harmonic[(std::size_t) k] = 1;
    }
    double fundamental = 0.0, rest = 0.0;
    const long f0bin = std::lround(f0 / bin);
    for (long k = f0bin - 6; k <= f0bin + 6; ++k)
        fundamental += std::norm(a[(std::size_t) k]);
    for (std::size_t k = (std::size_t) std::ceil(20.0 / bin); k * bin <= 20000.0; ++k)
        if (! harmonic[k])
            rest += std::norm(a[k]);
    return 10.0 * std::log10(std::max(rest, 1e-300) / fundamental);
}

void aliasCheck()
{
    for (double rate : { 88200.0, 96000.0 })
        // Non-integer rate/f0 ratios: with an integer ratio naive aliases land on harmonics.
        for (double f0 : { 61.7, 173.3, 199.3 })
            for (int function : { 1, 2, 3 })
            {
                OriginalSubOscillator blep, naive;
                std::vector<double> a(1 << 17), b(1 << 17);
                for (std::size_t i = 0; i < a.size(); ++i)
                {
                    a[i] = blep.tickCircuit(f0, rate, function);
                    b[i] = naive.tick((float) f0, (float) rate, function);
                }
                const double after = inharmonicDb(a, rate, f0);
                const double before = inharmonicDb(b, rate, f0);
                const std::string where = "function " + std::to_string(function) + " at "
                                        + std::to_string(f0) + " Hz, " + std::to_string((int) rate);
                check(after <= tol::kAliasCeilingDb, "band-limited SUB alias below ceiling: " + where);
                check(before - after >= tol::kAliasImprovementDb, "band-limited SUB improves on naive: " + where);
                if (f0 == 173.3)
                    std::printf("SUB alias %s: naive %.1f dB -> band-limited %.1f dB\n", where.c_str(), before, after);
            }
}

// Harmonics with h * f0 <= 5 kHz against the additive reference, coherent DFT.
void harmonicAccuracy()
{
    for (double rate : { 88200.0, 96000.0 })
        for (int function : { 1, 2, 3 })
        {
            const int period = 512, periods = 64, n = period * periods;
            const double f0 = rate / period;
            OriginalSubOscillator osc;
            std::vector<double> x((std::size_t) n);
            for (int i = 0; i < n; ++i)
                x[(std::size_t) i] = osc.tickCircuit(f0, rate, function);
            for (int h = 1; h * f0 <= 5000.0; ++h)
            {
                std::complex<double> acc;
                for (int i = 0; i < n; ++i)
                    acc += x[(std::size_t) i] * std::polar(1.0, -2.0 * 3.14159265358979323846 * h * i / period);
                const double got = 2.0 * std::abs(acc) / n;
                const double want = ref::harmonicAmplitude(function, h);
                if (want == 0.0)
                    check(got < 1e-3 * ref::harmonicAmplitude(function, 1), "absent square harmonic stays absent");
                else
                    check(std::abs(20.0 * std::log10(got / want)) <= tol::kHarmonicDb,
                          "in-band SUB harmonic matches band-limited reference: h " + std::to_string(h));
            }
            // Pointwise sanity against additive synthesis away from the resets.
            double worst = 0.0;
            for (int i = 8; i < period / 2 - 8; ++i)
            {
                const double phase = (double) (i + 1) / period;
                worst = std::max(worst, std::abs(x[(std::size_t) i]
                                                 - ref::bandLimited(function, phase, f0, rate / 2)));
            }
            check(worst < 0.05, "band-limited waveform tracks additive reference between resets");
        }
}
} // namespace

int main()
{
    lawMatchesReference();
    smoothingInLogDomain();
    engineAnchors();
    engineUsesBandLimitedPath();
    aliasCheck();
    harmonicAccuracy();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
