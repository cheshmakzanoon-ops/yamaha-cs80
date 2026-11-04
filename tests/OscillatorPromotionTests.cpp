// Plan step 1.2 gate: the reconstructed IG00153/IG00158 oscillator promoted to
// the circuit engine ahead of the IG00156 nodal stage (docs/OSCILLATOR_PROMOTION.md).
// Numerical properties of the declared model, not hardware evidence.
#include "core/Engine.h"
#include "core/ReconstructedOscillator.h"
#include "Ws1Tolerances.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <iostream>
#include <vector>

using namespace cs80;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* text) { ++checks; if (!ok) { ++failures; std::cerr << "FAIL: " << text << '\n'; } }
constexpr double pi = 3.14159265358979323846;

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
        const double ang = -2.0 * pi / (double) len;
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0);
            for (std::size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl;
            }
        }
    }
}

// Same fixture and analysis as cs80_engine_audit --suite alias (aliasTest):
// rank I only, no detune/drift/dispersion/noise/chorus, HP off, LP open,
// one sustained note, 65536-point Blackman-Harris FFT after 0.6 s, everything
// in 20 Hz..20 kHz outside +/-6 bins of a partial is non-harmonic.
double aliasNonHarmonicDb(int rate, int note, bool pulse, int feetIndex)
{
    ParamValues p;
    p[P_sourcePerformance] = 1.0f; p[P_parityMode] = 0.0f;
    p[P_mix] = 0.0f; p[P_detune] = 0.0f; p[P_drift] = 0.0f; p[P_voiceDispersion] = 0.0f;
    p[P_analogNoise] = 0.0f; p[P_chorusOn] = 0.0f; p[P_volume] = 0.30f; p[P_resonance] = 0.0f;
    p[P_subOriginalMode] = 1.0f;
    const int ch = 0;
    p[chParam(ch, CP_saw)] = pulse ? 0.0f : 1.0f;
    p[chParam(ch, CP_pulse)] = pulse ? 1.0f : 0.0f;
    p[chParam(ch, CP_pw)] = 0.5f;
    p[chParam(ch, CP_feet)] = (float) feetIndex;
    p[chParam(ch, CP_hpfOn)] = 0.0f;
    p[chParam(ch, CP_lpfCutoff)] = 18000.0f;
    p[chParam(ch, CP_fIl)] = 0.0f; p[chParam(ch, CP_fAl)] = 0.0f;
    p[chParam(ch, CP_vS)] = 1.0f; p[chParam(ch, CP_vA)] = 0.001f; p[chParam(ch, CP_vD)] = 0.001f;
    p[chParam(ch, CP_vLevel)] = 0.5f; p[chParam(ch, CP_vcfLevel)] = 1.0f;
    Engine e;
    e.prepare(rate);
    ControlFrame frame;
    frame.effectiveValues = p;
    check(e.setControlFrame(frame), "alias fixture frame accepted");
    e.reset();
    const std::size_t n = 65536;
    const double warm = 0.6, seconds = warm + (double) n / rate + 0.05;
    const long frames = (long) std::llround(seconds * rate);
    std::vector<float> l((std::size_t) frames), r((std::size_t) frames), buf(64 * 2);
    bool started = false;
    for (long f = 0; f < frames; f += 64)
    {
        const int count = (int) std::min<long>(64, frames - f);
        if (!started && (long) std::ceil(0.05 * rate / 64) * 64 <= f) { e.noteOn(note, 1.0f, 1); started = true; }
        e.setControlFrame(frame);
        e.render(buf.data(), count);
        for (int i = 0; i < count; ++i) { l[(std::size_t) (f + i)] = buf[(std::size_t) i * 2]; r[(std::size_t) (f + i)] = buf[(std::size_t) i * 2 + 1]; }
    }
    std::vector<std::complex<double>> a(n);
    const long start = (long) std::llround(warm * rate);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double x = (double) i / (double) (n - 1);
        const double w = 0.35875 - 0.48829 * std::cos(2 * pi * x) + 0.14128 * std::cos(4 * pi * x) - 0.01168 * std::cos(6 * pi * x);
        a[i] = w * 0.5 * ((double) l[(std::size_t) start + i] + (double) r[(std::size_t) start + i]);
    }
    fft(a);
    const double f0 = midiToHz((float) note) * kFeetRatios[std::clamp(feetIndex, 0, 5)];
    const double binHz = (double) rate / (double) n;
    std::vector<char> harmonic(n / 2, 0);
    for (int k = 1; k * f0 < 0.5 * rate; ++k)
    {
        const double c = k * f0 / binHz;
        for (long b = (long) std::floor(c) - 6; b <= (long) std::ceil(c) + 6; ++b)
            if (b >= 0 && b < (long) n / 2) harmonic[(std::size_t) b] = 1;
    }
    double harm = 0.0, non = 0.0;
    const std::size_t lo = (std::size_t) std::ceil(20.0 / binHz), hi = std::min((std::size_t) std::floor(20000.0 / binHz), n / 2 - 1);
    for (std::size_t b = lo; b <= hi; ++b) (harmonic[b] ? harm : non) += std::norm(a[b]);
    return 10.0 * std::log10(std::max(non, 1e-30) / std::max(harm, 1e-30));
}

void aliasGate(const ws1::Tolerances& tol)
{
    const double limit = tol["alias_worst_nonharmonic_db_48k"];
    double worst[2] = { -999.0, -999.0 };
    for (bool pulse : { false, true })
        for (int note : { 72, 84, 96, 108 })
            for (int feet : { 1, 5 })
            {
                if (note + 12.0 * std::log2(kFeetRatios[feet]) > 120.0) continue;
                const double db = aliasNonHarmonicDb(48000, note, pulse, feet);
                worst[pulse] = std::max(worst[pulse], db);
            }
    std::printf("alias 48 kHz circuit engine: worst saw %.2f dB, worst pulse %.2f dB (limit %.1f)\n", worst[0], worst[1], limit);
    check(worst[0] <= limit, "saw worst non-harmonic within the pre-registered alias target");
    check(worst[1] <= limit, "pulse worst non-harmonic within the pre-registered alias target");
}

// Pin-level anchors (service p42 nominal VCO 3.5 Vpp, WSC sine 3 Vpp) at the
// declared 261.6 Hz reference, at the engine's internal rates.
void pinAnchors()
{
    for (double rate : { 88200.0, 96000.0, 192000.0 })
    {
        ReconstructedOscillator o;
        check(o.prepare(rate, {}), "engine-rate preparation");
        o.reset(0.0, 261.6);
        WscControls c;
        double pinLo = 1e9, pinHi = -1e9, sineLo = 1e9, sineHi = -1e9;
        const int settle = (int) (2.0 * rate);
        for (int n = 0; n < settle + (int) (0.1 * rate); ++n)
        {
            const auto v = o.tick(261.6, c);
            if (n < settle) continue;
            const auto& d = o.getDiagnostics();
            pinLo = std::min(pinLo, d.rawPin7Volts); pinHi = std::max(pinHi, d.rawPin7Volts);
            sineLo = std::min(sineLo, d.raw.sinePin2); sineHi = std::max(sineHi, d.raw.sinePin2);
            (void) v;
        }
        std::printf("pins at %g Hz: pin7 %.4f Vpp, sine %.4f Vpp\n", rate, pinHi - pinLo, sineHi - sineLo);
        check(std::abs((pinHi - pinLo) / 3.5 - 1.0) < 1e-3, "raw pin7 is 3.5 Vpp at the reference condition");
        check(std::abs((sineHi - sineLo) / 3.0 - 1.0) < 1e-3, "WSC sine pin is 3 Vpp at the reference condition");
    }
}

// One immutable residual basis per (rate, RC), shared and reference counted.
void sharedBasis()
{
    ReconstructedOscillator a, b;
    a.prepare(96000.0, {}); b.prepare(96000.0, {});
    check(a.sharedBasis() != nullptr && a.sharedBasis() == b.sharedBasis(), "same rate shares one basis");
    const int refs = OscillatorBasisRegistry::references(a.sharedBasis());
    {
        ReconstructedOscillator copy = a;
        check(copy.sharedBasis() == a.sharedBasis(), "a copy shares the basis");
        check(OscillatorBasisRegistry::references(a.sharedBasis()) == refs + 1, "a copy holds a reference");
    }
    check(OscillatorBasisRegistry::references(a.sharedBasis()) == refs, "destruction releases the reference");
    ReconstructedOscillator c;
    c.prepare(88200.0, {});
    check(c.sharedBasis() != a.sharedBasis(), "a different rate gets its own basis");
    check(sizeof(ReconstructedOscillator) < 8192, "per-instance storage no longer holds the 16 KB residual tables");
    std::printf("sizeof(ReconstructedOscillator) = %zu bytes, basis %zu bytes shared\n",
                sizeof(ReconstructedOscillator), sizeof(OscillatorExponentialBasis));
    // Output through a shared basis equals a fresh private preparation.
    ReconstructedOscillator x, y;
    x.prepare(96000.0, {}); y.prepare(96000.0, {});
    x.reset(0.3, 440.0); y.reset(0.3, 440.0);
    bool same = true;
    for (int n = 0; n < 4096; ++n)
    {
        WscControls k; k.pulseThresholdVolts = 0.5 * std::sin(n * 0.01);
        const auto p = x.tick(440.0, k), q = y.tick(440.0, k);
        same = same && p.sawPin5 == q.sawPin5 && p.pulsePin7 == q.pulsePin7 && p.sinePin2 == q.sinePin2;
    }
    check(same, "shared-basis oscillators are bit-identical");
}

// 2026-11-01 pre-registered event-cache gate. Existing comparator-timing,
// zero-history, state-replay and alias gates remain the numerical authority;
// this gate checks that the same analytic segment is not re-solved each tick.
void segmentCacheGate(const ws1::Tolerances& tol)
{
    ReconstructedOscillator oscillator;
    check(oscillator.prepare(96000.0, {}), "segment-cache fixture prepares");
    oscillator.reset(0.0, 261.6);
    WscControls controls;
    for (int i = 0; i < 9600; ++i) oscillator.tick(261.6, controls);
    std::uint64_t searches = 0;
    for (int i = 0; i < 4096; ++i)
    {
        oscillator.tick(261.6, controls);
        searches += oscillator.getDiagnostics().branchRootSearches;
    }
    std::printf("oscillator event-root searches per 4096 stable ticks: %llu (limit %.0f)\n",
                static_cast<unsigned long long>(searches), tol["stable_root_solves_per_4096_ticks_max"]);
    check(searches > 0 && searches <= tol["stable_root_solves_per_4096_ticks_max"],
          "event roots are reused until a real analytic-segment boundary");
    const auto checkInvalidation = [&](double hz, const char* description)
    {
        oscillator.tick(hz, controls);
        check(oscillator.getDiagnostics().branchRootSearches >= tol["invalidation_root_solves_min"],
              description);
    };
    checkInvalidation(440.0, "frequency change invalidates cached branch event");
    controls.pulseThresholdVolts = 0.6;
    checkInvalidation(440.0, "comparator threshold change invalidates cached branch event");
    controls.sawEnabled = false;
    checkInvalidation(440.0, "waveform gate change invalidates cached branch event");
    const auto snapshot = oscillator.getState();
    oscillator.tick(440.0, controls);
    oscillator.restoreState(snapshot);
    checkInvalidation(440.0, "state restore invalidates cached branch event");
    oscillator.advanceIdle(0.002, 440.0, controls);
    checkInvalidation(440.0, "idle advance invalidates cached branch event");
    const auto idle = oscillator.getIdleState();
    oscillator.tick(440.0, controls);
    oscillator.restoreIdleState(idle);
    checkInvalidation(440.0, "idle snapshot restore invalidates cached branch event");
}

// The circuit engine's sine is the folded triangle -> PWL shaper, not
// sin + 3.5 % third harmonic: measure the third harmonic at the output.
double thirdHarmonicRatio(float source)
{
    ParamValues p;
    p[P_sourcePerformance] = source; p[P_mix] = 0.0f; p[P_detune] = 0.0f; p[P_drift] = 0.0f;
    p[P_voiceDispersion] = 0.0f; p[P_analogNoise] = 0.0f; p[P_chorusOn] = 0.0f; p[P_volume] = 0.3f;
    p[chParam(0, CP_saw)] = 0.0f; p[chParam(0, CP_pulse)] = 0.0f; p[chParam(0, CP_sine)] = 1.0f;
    p[chParam(0, CP_vcfLevel)] = 0.0f; p[chParam(0, CP_vS)] = 1.0f; p[chParam(0, CP_vA)] = 0.001f;
    Engine e; e.setParams(p); e.prepare(48000.0); e.noteOn(57, 1.0f); // 220 Hz
    std::vector<float> out(48000 * 2 * 2);
    e.render(out.data(), 48000 * 2);
    std::complex<double> h1 = 0.0, h3 = 0.0;
    const double f = 220.0;
    for (int i = 48000; i < 96000; ++i)
    {
        const double t = i / 48000.0, x = out[(std::size_t) i * 2];
        h1 += x * std::polar(1.0, -2 * pi * f * t);
        h3 += x * std::polar(1.0, -2 * pi * 3 * f * t);
    }
    return std::abs(h3) / std::abs(h1);
}
void sineShaper()
{
    const double legacy = thirdHarmonicRatio(0.0f), circuit = thirdHarmonicRatio(1.0f);
    std::printf("third harmonic / fundamental: legacy %.4f, circuit %.4f\n", legacy, circuit);
    check(std::abs(legacy - 0.035) < 0.005, "legacy keeps the 3.5 % third-harmonic placeholder");
    check(circuit < 0.01, "circuit engine sine comes from the PWL shaper, not the 3.5 % placeholder");
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const ws1::Tolerances tol(argc > 1 ? argv[1] : "tests/parity/tolerances-v2/ws1-voice.json", "oscillator_promotion");
        const ws1::Tolerances cacheTol(argc > 1 ? argv[1] : "tests/parity/tolerances-v2/ws1-voice.json", "oscillator_segment_cache");
        pinAnchors();
        sharedBasis();
        segmentCacheGate(cacheTol);
        sineShaper();
        aliasGate(tol);
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return 2;
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
