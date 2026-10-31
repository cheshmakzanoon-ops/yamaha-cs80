// ws4 Step 2: circuit-engine ring modulator, held to the offline truth model in
// src/reference/RingModulator.h by the pre-registered bounds in tests/Ws4Tolerances.h.
// Agreement with these equations is not agreement with a measured CS-80; see
// docs/workstreams/ws4-circuits.md.
#include "core/Engine.h"
#include "core/RingModulatorCircuit.h"
#include "reference/RingModulator.h"
#include "Ws4Tolerances.h"

#include <cmath>
#include <complex>
#include <cstdio>
#include <string>
#include <vector>

using namespace cs80;
namespace ref = cs80::reference::ring;
namespace tol = cs80::ws4_tolerances::ring;

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
double db(double ratio) { return 20.0 * std::log10(ratio); }
constexpr double kPiD = 3.14159265358979323846;

void converterLaw()
{
    check(relative(RingModulatorCircuit::hzForVolts(5.0), 1.0 / 0.150) <= tol::kLawRelative,
          "VC1 = +5 V gives the 150 ms ADJ p19 period");
    check(relative(RingModulatorCircuit::hzForVolts(10.0), 1.0 / 0.005) <= tol::kLawRelative,
          "maximum SPEED gives the 5 ms ADJ p19 period");
    for (double v = -2.0; v <= 11.0; v += 0.25)
    {
        check(relative(RingModulatorCircuit::hzForVolts(v), ref::hzForVolts(v)) <= tol::kLawRelative,
              "converter matches anchor-solved reference at " + std::to_string(v) + " V");
        check(std::abs(RingModulatorCircuit::voltsForHz(RingModulatorCircuit::hzForVolts(v)) - v) < 1e-9,
              "converter inverse");
    }
    for (float base : { 0.1f, 1.0f / 0.15f, 40.0f, 200.0f, 500.0f })
        for (float depth : { 0.0f, 0.3f, 1.0f })
            for (float env : { 0.0f, 0.5f, 1.0f })
            {
                const double got = RingModulatorCircuit::frequency(base, depth, env);
                const double want = ref::carrierHz(base, depth, env);
                check(relative(got, want) <= tol::kEnvelopeLawRelative,
                      "envelope -> frequency law matches reference");
            }
    check(RingModulatorCircuit::frequency(77.0f, 0.0f, 0.7f) == 77.0f, "DEPTH 0 returns SPEED exactly");
    check(RingModulatorCircuit::frequency(77.0f, 0.9f, 0.0f) == 77.0f, "envelope 0 returns SPEED exactly");
    std::printf("Ring converter: %.4f oct/V, full depth +%.3f octaves\n",
                RingModulatorCircuit::octavesPerVolt(),
                RingModulatorCircuit::octavesPerVolt() * RingModulatorCircuit::kEnvelopePeakVolts);
}

ParamValues ringPatch(bool isolation, float speed, float depth, float amount)
{
    ParamValues p;
    p[P_sourcePerformance] = 1.0f;
    p[P_parityMode] = isolation ? 1.0f : 0.0f;
    p[P_ringSpeed] = speed;
    p[P_ringDepth] = depth;
    p[P_ringAmount] = amount;
    p[P_chorusOn] = 0.0f;
    return p;
}

// With no notes and MODULATION fully up, the output is the multiplier's carrier
// feedthrough, so the rendered period is the carrier period.
double renderedPeriod(double hostRate, float speed)
{
    Engine e;
    e.setParams(ringPatch(true, speed, 0.0f, 1.0f));
    e.prepare(hostRate);
    std::vector<float> block(2 * 512);
    const int settle = (int) hostRate, total = (int) (hostRate * (speed < 20.0f ? 4.0 : 1.5));
    double previous = 0.0, first = -1.0, last = -1.0;
    int crossings = 0;
    for (int done = 0; done < total; done += 512)
    {
        e.render(block.data(), 512);
        for (int i = 0; i < 512; ++i)
        {
            const double value = block[(std::size_t) 2 * i];
            const int n = done + i;
            if (n > settle && previous < 0.0 && value >= 0.0)
            {
                const double t = (n - 1 + (-previous) / (value - previous)) / hostRate;
                if (crossings == 0) first = t;
                last = t;
                ++crossings;
            }
            previous = value;
        }
    }
    return crossings > 1 ? (last - first) / (crossings - 1) : 0.0;
}

void engineAnchorPeriods()
{
    for (double hostRate : { 44100.0, 96000.0 })
    {
        const double slow = renderedPeriod(hostRate, (float) (1.0 / 0.150));
        const double fast = renderedPeriod(hostRate, 200.0f);
        check(relative(slow, 0.150) <= tol::kAnchorPeriodRelative, "rendered VC1 = +5 V period is 150 ms");
        check(relative(fast, 0.005) <= tol::kAnchorPeriodRelative, "rendered maximum-SPEED period is 5 ms");
        std::printf("Ring ADJ p19 anchors at %d Hz: %.4f ms (150 +/- 7.5), %.5f ms (5 +/- 0.25)\n",
                    (int) hostRate, slow * 1e3, fast * 1e3);
    }
}

void pureMask()
{
    std::vector<float> block(2 * 64);
    Engine pure, performance;
    pure.setParams(ringPatch(true, 500.0f, 0.0f, 0.5f));
    performance.setParams(ringPatch(false, 500.0f, 0.0f, 0.5f));
    pure.prepare(48000.0);
    performance.prepare(48000.0);
    pure.render(block.data(), 64);
    performance.render(block.data(), 64);
    check(pure.getRingFrequency() == 200.0f, "PURE CS-80 masks the 500 Hz SPEED extension at 200 Hz");
    check(performance.getRingFrequency() == 500.0f, "Source Performance keeps the 500 Hz extension");
}

void envelopeLaw()
{
    for (bool isolation : { false, true })
        for (float depth : { 0.5f, 1.0f })
        {
            auto p = ringPatch(isolation, 40.0f, depth, 0.5f);
            p[P_ringAttack] = 0.08f;
            p[P_ringDecay] = 0.45f;
            Engine e;
            e.setParams(p);
            e.prepare(48000.0);
            e.noteOn(60, 0.8f);
            float frame[2], peakEnv = 0.0f, peakHz = 0.0f;
            bool lawHolds = true;
            for (int n = 0; n < 48000; ++n)
            {
                e.render(frame, 1);
                const float env = e.getRingEnvelope(), hz = e.getRingFrequency();
                lawHolds = lawHolds && relative(hz, ref::carrierHz(40.0, depth, std::clamp(env, 0.0f, 1.0f)))
                                           <= tol::kEnvelopeLawRelative;
                if (env > peakEnv) { peakEnv = env; peakHz = hz; }
            }
            check(lawHolds, "engine carrier follows the envelope -> frequency law at every frame");
            check(peakEnv > 0.9f, "fixture envelope reaches its peak");
            check(peakHz > 40.0f * std::pow(30.0f, depth * 0.9f), "full sweep spans most of the declared octaves");
        }
    // Legacy policy keeps the provisional one-octave law.
    ParamValues p;
    p[P_ringSpeed] = 40.0f;
    p[P_ringDepth] = 1.0f;
    Engine e;
    e.setParams(p);
    e.prepare(48000.0);
    e.noteOn(60, 0.8f);
    float frame[2];
    bool legacy = true;
    for (int n = 0; n < 24000; ++n)
    {
        e.render(frame, 1);
        legacy = legacy && relative(e.getRingFrequency(), ringModulationFrequency(40.0f, 1.0f, e.getRingEnvelope())) < 1e-6;
    }
    check(legacy, "legacy policy keeps the provisional ring frequency law");
}

// Unit-level multiplier spectrum: coherent DFT lines versus the analytic reference.
std::complex<double> dftBin(const std::vector<float>& x, int bin)
{
    std::complex<double> acc;
    const double n = (double) x.size();
    for (std::size_t i = 0; i < x.size(); ++i)
        acc += (double) x[i] * std::polar(1.0, -2.0 * kPiD * bin * (double) i / n);
    return acc;
}
double lineAmplitude(const std::vector<float>& x, int bin)
{
    const double n = (double) x.size();
    return (bin == 0 ? 1.0 : 2.0) * std::abs(dftBin(x, bin)) / n;
}

void multiplierLines()
{
    const int n = 4096, signalBin = 400, carrierBin = 37;
    for (float amplitude : { 0.1f, 0.5f })
        for (float amount : { 0.3f, 1.0f })
        {
            std::vector<float> y((std::size_t) n);
            double energy = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const float x = amplitude * (float) std::sin(2.0 * kPiD * signalBin * i / n);
                const float c = (float) std::sin(2.0 * kPiD * carrierBin * i / n);
                y[(std::size_t) i] = RingModulatorCircuit::blend(x, c, amount);
                energy += (double) y[(std::size_t) i] * y[(std::size_t) i];
            }
            const auto want = ref::lines(amplitude, amount);
            const auto near = [&](int bin, double expected, const char* what)
            {
                check(std::abs(db(lineAmplitude(y, bin) / expected)) <= tol::kLineDb, what);
            };
            near(0, want.dc, "multiplier DC offset line");
            near(carrierBin, want.carrier, "carrier feedthrough line");
            near(signalBin, want.signal, "signal feedthrough line");
            near(signalBin - carrierBin, want.lowerSideband, "lower sideband");
            near(signalBin + carrierBin, want.upperSideband, "upper sideband");
            double lineEnergy = 0.0;
            for (int bin : { 0, carrierBin, signalBin, signalBin - carrierBin, signalBin + carrierBin })
            {
                const double a = lineAmplitude(y, bin);
                lineEnergy += bin == 0 ? a * a * n : a * a * n / 2.0;
            }
            check(std::abs(energy - lineEnergy) <= 1e-6 * energy, "offset multiplier makes no other lines");
        }
}

// Engine: a sustained sine voice through the circuit-engine multiplier.
struct Spectrum
{
    std::vector<std::complex<double>> bins;
    double rate = 0.0;
    std::size_t n = 0;
};
double windowedLine(const std::vector<float>& left, double rate, double hz)
{
    // Blackman-Harris 4-term, amplitude-normalized; the line sits near a bin centre.
    const std::size_t n = left.size();
    double coherent = 0.0;
    std::complex<double> acc;
    for (std::size_t i = 0; i < n; ++i)
    {
        const double t = 2.0 * kPiD * (double) i / (double) n;
        const double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t);
        coherent += w;
        acc += (double) left[i] * w * std::polar(1.0, -2.0 * kPiD * hz * (double) i / rate);
    }
    return 2.0 * std::abs(acc) / coherent;
}
std::vector<float> renderVoice(float amount, bool withNote)
{
    auto p = ringPatch(true, 100.0f, 0.0f, amount);
    p[P_mix] = 0.0f;
    p[P_detune] = 0.0f;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_saw)] = p[chParam(rank, CP_pulse)] = 0.0f;
        p[chParam(rank, CP_noise)] = 0.0f;
        p[chParam(rank, CP_sine)] = rank == 0 ? 1.0f : 0.0f;
        p[chParam(rank, CP_vLevel)] = 1.0f;
        p[chParam(rank, CP_vA)] = 0.001f;
        p[chParam(rank, CP_vS)] = 1.0f;
        p[chParam(rank, CP_initLevel)] = p[chParam(rank, CP_afterLevel)] = 0.0f;
    }
    Engine e;
    e.setParams(p);
    e.prepare(48000.0);
    if (withNote)
        e.noteOn(69, 0.8f);
    std::vector<float> block(2 * 48000);
    e.render(block.data(), 48000);                // settle
    std::vector<float> left((std::size_t) 1 << 17), buffer(2 * left.size());
    e.render(buffer.data(), (int) left.size());
    for (std::size_t i = 0; i < left.size(); ++i)
        left[i] = buffer[2 * i];
    return left;
}

void engineSidebands()
{
    const auto ring = renderVoice(1.0f, true);
    const auto half = renderVoice(0.5f, true);
    const auto silent = renderVoice(1.0f, false);
    const double lower = windowedLine(ring, 48000.0, 340.0), upper = windowedLine(ring, 48000.0, 540.0);
    const double signal = windowedLine(ring, 48000.0, 440.0);
    const double carrier = windowedLine(ring, 48000.0, 100.0), carrierHalf = windowedLine(half, 48000.0, 100.0);
    const double carrierSilent = windowedLine(silent, 48000.0, 100.0);
    check(lower > 1e-6 && upper > 1e-6, "engine ring fixture produces sidebands");
    check(std::abs(db(lower / upper)) <= tol::kEngineLineRatioDb, "engine sidebands are balanced");
    check(std::abs(db(signal / upper) - db(2.0 * RingModulatorCircuit::kCarrierOffset)) <= tol::kEngineLineRatioDb,
          "engine signal feedthrough sits 2 x carrier-port offset below the sidebands");
    check(std::abs(db(carrier / carrierHalf) - db(2.0)) <= tol::kEngineLineRatioDb,
          "engine carrier feedthrough scales with MODULATION");
    check(std::abs(db(carrier / carrierSilent)) <= tol::kEngineLineRatioDb,
          "engine carrier feedthrough is independent of the signal");
    std::printf("Engine ring lines re upper sideband: signal %.1f dB, carrier %.1f dB, lower %.2f dB\n",
                db(signal / upper), db(carrier / upper), db(lower / upper));
}
} // namespace

int main()
{
    converterLaw();
    engineAnchorPeriods();
    pureMask();
    envelopeLaw();
    multiplierLines();
    engineSidebands();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
