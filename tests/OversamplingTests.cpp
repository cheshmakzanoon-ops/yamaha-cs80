// Oversampling quality (IMPROVEMENT_PLAN 1.6): numerical contracts, not hardware evidence.
#include "core/Engine.h"
#include "Ws2EngineTolerances.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

// Count every allocation in this executable so the render-thread quality
// switch can be shown to be allocation-free.
static std::atomic<long> allocations { 0 };
void* operator new(std::size_t size)
{
    ++allocations;
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
constexpr double pi = 3.14159265358979323846;

void testFactorRule()
{
    using M = OversamplingMode;
    for (double rate : { 44100.0, 48000.0 })
    {
        check(oversamplingFactor(M::eco, false, false, rate) == 2
                  && oversamplingFactor(M::standard, false, false, rate) == 2
                  && oversamplingFactor(M::hq, false, false, rate) == 8,
              "legacy keeps 2x unless HQ is selected");
        check(oversamplingFactor(M::standard, false, true, rate) == 2,
              "legacy offline bounce keeps its historical factor");
        check(oversamplingFactor(M::eco, true, false, rate) == 2
                  && oversamplingFactor(M::standard, true, false, rate) == kStandardOversampling
                  && oversamplingFactor(M::hq, true, false, rate) == 8,
              "circuit engine Eco 2x, Standard measured factor, HQ 8x");
        for (auto mode : { M::eco, M::standard, M::hq })
            check(oversamplingFactor(mode, true, true, rate) == 8, "circuit offline bounce is always HQ");
    }
    // The internal rate, not the factor, is held across host rates.
    check(oversamplingFactor(M::hq, true, false, 96000.0) == 4
              && oversamplingFactor(M::standard, true, false, 96000.0) == std::max(2, kStandardOversampling / 2)
              && oversamplingFactor(M::eco, true, false, 96000.0) == 2,
          "96 kHz host halves the factor");
    check(oversamplingFactor(M::hq, true, true, 192000.0) == 2
              && oversamplingFactor(M::hq, true, true, 768000.0) == 2,
          "very high host rates never drop below 2x");
    check(oversamplingMode(0.0f) == M::eco && oversamplingMode(1.0f) == M::standard
              && oversamplingMode(2.0f) == M::hq, "parameter values map to modes");
    check(paramDef(P_oversampling).def == 1.0f && std::string(paramDef(P_oversampling).id) == "oversampling",
          "OVERSAMPLING defaults to Standard");
}

double response(const OversamplingDecimator& d, double f)
{
    double re = 0, im = 0;
    for (int tap = 0; tap < d.getTaps(); ++tap)
    {
        re += d.getCoefficient(tap) * std::cos(2 * pi * f * tap);
        im -= d.getCoefficient(tap) * std::sin(2 * pi * f * tap);
    }
    return std::hypot(re, im);
}

void testDecimatorDesign()
{
    Decimator established;
    established.prepare(96000.0f);
    OversamplingDecimator two;
    two.prepare(2);
    double error = 0;
    for (int tap = 0; tap < Decimator::kTaps; ++tap)
        error = std::max(error, std::abs((double) two.getCoefficient(tap)
                                         - established.getCoefficients()[(size_t) tap]));
    check(two.getTaps() == Decimator::kTaps && error <= ws2::kDecimatorFactor2CoefficientError,
          "factor 2 is the established 255-tap decimator exactly");
    Rng rng(0x0a5e1u);
    bool identical = true;
    for (int n = 0; n < 4096; ++n)
    {
        const float x = rng.nextBipolar();
        established.push(x);
        two.push(x);
        if ((n & 1) == Decimator::kRetainedPhase)
            identical = identical && established.output() == two.output();
    }
    check(identical, "factor 2 output is bit-identical to Decimator");

    for (int factor : { 2, 4, 8 })
    {
        OversamplingDecimator d;
        d.prepare(factor);
        check(d.getTaps() == 128 * factor - 1 && d.getHostLatency() == ws2::kReportedLatencySamples,
              "taps 128N-1 and 63 host-sample latency per factor");
        // Dense DFT of the rounded float coefficients, in internal normalized frequency.
        const double passEdge = (20000.0 / 88200.0) * 2.0 / factor;
        const double stopEdge = 0.5 / factor;
        double minPass = 10, maxPass = 0, maxStop = 0;
        const int bins = 4096 * factor;
        for (int bin = 0; bin <= bins; ++bin)
        {
            const double f = 0.5 * bin / bins;
            if (f > passEdge && f < stopEdge) continue;
            const double m = response(d, f);
            if (f <= passEdge) { minPass = std::min(minPass, m); maxPass = std::max(maxPass, m); }
            else maxStop = std::max(maxStop, m);
        }
        std::printf("decimator %dx: taps %d passband %+.4f/%+.4f dB stopband %.1f dB latency %d\n", factor,
                    d.getTaps(), 20 * std::log10(minPass), 20 * std::log10(maxPass), 20 * std::log10(maxStop),
                    d.getHostLatency());
        check(20 * std::log10(minPass) >= -ws2::kDecimatorPassbandDb
                  && 20 * std::log10(maxPass) <= ws2::kDecimatorPassbandDb,
              "passband within 0.1 dB through 20 kHz at a 44.1 kHz host");
        check(20 * std::log10(maxStop) <= ws2::kDecimatorStopbandDb, "at least 80 dB above host Nyquist");

        // Measured delay of the render-path use: push N, read the last phase.
        const double hostRate = 48000.0, tone = 1000.0;
        double cs = 0, sn = 0, cc = 0, ss = 0;
        d.reset();
        for (int i = 0; i < 20000; ++i)
        {
            float y = 0;
            for (int os = 0; os < factor; ++os)
            {
                d.push((float) std::sin(2 * pi * tone * (factor * i + os) / (factor * hostRate)));
                if (os == d.getRetainedPhase()) y = d.output();
            }
            if (i < 4000) continue;
            const double th = 2 * pi * tone * i / hostRate;
            cs += y * std::cos(th); sn += y * std::sin(th);
            cc += std::cos(th) * std::cos(th); ss += std::sin(th) * std::sin(th);
        }
        const double phase = std::atan2(-cs / cc, sn / ss);
        double delay = phase / (2 * pi * tone / hostRate);
        const double period = hostRate / tone;
        delay -= period * std::round((delay - ws2::kReportedLatencySamples) / period);
        check(std::abs(delay - ws2::kReportedLatencySamples) <= ws2::kMeasuredLatencyErrorSamples,
              "measured render-path delay equals the reported 63 host samples");
    }
}

ParamValues circuit(float quality)
{
    ParamValues p;
    p[P_sourcePerformance] = 1.0f;
    p[P_oversampling] = quality;
    return p;
}

std::vector<float> phrase(Engine& e, int blocks = 24)
{
    std::vector<float> audio, block(2 * 96);
    e.noteOn(48, 0.8f);
    e.noteOn(67, 0.6f);
    for (int b = 0; b < blocks; ++b)
    {
        if (b == 12) e.noteOff(48);
        e.render(block.data(), 96);
        audio.insert(audio.end(), block.begin(), block.end());
    }
    return audio;
}

void testEngine()
{
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (float quality : { 0.0f, 1.0f, 2.0f })
            for (bool source : { false, true })
            {
                Engine e;
                e.prepare(rate);
                auto p = circuit(quality);
                p[P_sourcePerformance] = source ? 1.0f : 0.0f;
                e.setParams(p);
                const int expected = oversamplingFactor(oversamplingMode(quality), source, false, rate);
                check(e.getOversamplingFactor() == expected && e.getInternalSampleRate() == rate * expected,
                      "engine runs at the rule's factor");
                // ws1 merge, intentional circuit-engine change: the reconstructed
                // oscillator's 8-internal-sample kernel is reported too (8/N).
                check(e.getLatencySamples() == ws2::kReportedLatencySamples + (source ? 8 / expected : 0),
                      "reported latency: 63 plus the circuit oscillator's 8/N in every mode");
            }

    // A quality change on the render thread leaves the same state as a full
    // prepare at that mode. Both engines share one history, because the RNG
    // streams deliberately survive resets (legacy noise sequencing).
    for (auto [from, to] : { std::pair { 0.0f, 2.0f }, { 2.0f, 0.0f }, { 1.0f, 2.0f }, { 2.0f, 1.0f }, { 0.0f, 1.0f } })
    {
        Engine switched, reprepared;
        for (Engine* e : { &switched, &reprepared })
        {
            e->setParams(circuit(from));
            e->prepare(48000.0);
            (void) phrase(*e, 4);
            e->setParams(circuit(to));
        }
        // Integration (kStandardOversampling = 2 after ws1's oscillator): Eco and
        // Standard can share a factor; a quality change that keeps the factor is
        // not a re-prepare and leaves notes sounding.
        const bool factorChanges = oversamplingFactor(oversamplingMode(from), true, false, 48000.0)
                                   != oversamplingFactor(oversamplingMode(to), true, false, 48000.0);
        if (! factorChanges)
        {
            check(switched.activeVoices() > 0, "a quality change that keeps the factor leaves notes sounding");
            continue;
        }
        check(switched.activeVoices() == 0, "factor change stops sounding notes like a policy change");
        reprepared.prepare(48000.0);
        check(switched.getOversamplingFactor() == reprepared.getOversamplingFactor()
                  && phrase(switched) == phrase(reprepared),
              "render-thread switch equals a full prepare at the new mode");
    }

    // Offline flag: circuit engine goes to HQ, legacy does not move.
    {
        Engine e;
        e.prepare(48000.0);
        e.setParams(circuit(0.0f));
        e.setNonRealtime(true);
        check(e.getOversamplingFactor() == 8, "circuit offline render uses HQ");
        e.setNonRealtime(false);
        check(e.getOversamplingFactor() == 2, "returning to realtime restores the selected mode");
        Engine legacy;
        legacy.prepare(48000.0);
        legacy.setNonRealtime(true);
        check(legacy.getOversamplingFactor() == 2 && legacy.getRenderPolicy() == RenderPolicy::legacy,
              "legacy default renders offline at its historical 2x");
    }

    // No allocation on the render-thread boundary once prepared.
    {
        Engine e;
        e.prepare(48000.0);
        ControlFrame frame;
        frame.effectiveValues = circuit(1.0f);
        std::vector<float> block(2 * 64);
        const long before = allocations.load();
        for (float quality : { 2.0f, 0.0f, 1.0f, 2.0f })
        {
            frame.effectiveValues[P_oversampling] = quality;
            e.setControlFrame(frame);
            e.render(block.data(), 64);
        }
        e.setNonRealtime(true);
        e.render(block.data(), 64);
        e.setNonRealtime(false);
        frame.effectiveValues[P_sourcePerformance] = 0.0f;
        e.setControlFrame(frame);
        e.render(block.data(), 64);
        check(allocations.load() == before, "quality, offline and policy switches do not allocate");
    }

    // Engine-level check of the whole render path at each factor: a pure SINE
    // voice (no filters, saturation or noise in its path) must render with the
    // same delay and level at 2x, 4x and 8x, so every factor's decimator runs
    // with its own design and the same 63-sample delay. The tone is 421.875 Hz
    // (TUNING REF, note 69): its phase increment is 9/2048, 9/4096 and 9/8192,
    // exact in float, so the float phase accumulator cannot wander (at an
    // arbitrary pitch it wanders by up to +/-0.5 host sample between rates).
    // What remains is the oscillator's one-internal-sample lead: 1/2 - 1/N.
    {
        auto sineVoice = [](float quality)
        {
            auto p = circuit(quality);
            p[P_analogNoise] = p[P_drift] = p[P_voiceDispersion] = p[P_chorusOn] = 0.0f;
            p[P_mix] = 0.0f;
            p[P_tuningRef] = 421.875f;
            p[chParam(0, CP_saw)] = p[chParam(0, CP_pulse)] = 0.0f;
            p[chParam(0, CP_sine)] = 1.0f;
            Engine e;
            e.prepare(48000.0);
            e.setParams(p);
            e.noteOn(69, 0.8f);
            std::vector<float> audio(2 * 24000);
            e.render(audio.data(), 24000);
            return audio;
        };
        const auto reference = sineVoice(0.0f);
        const int window = ws2::kCrossFactorWindowSamples;
        auto left = [](const std::vector<float>& a, int n) { return n >= 0 && n < (int) a.size() / 2 ? (double) a[(size_t) (2 * n)] : 0.0; };
        for (float quality : { 1.0f, 2.0f })
        {
            const auto other = sineVoice(quality);
            // Cross-correlation lag of `other` against Eco, from note-on through the onset.
            auto correlation = [&](int lag)
            {
                double sum = 0;
                for (int n = 0; n < window; ++n) sum += left(reference, n) * left(other, n + lag);
                return sum;
            };
            int best = 0;
            for (int lag = -64; lag <= 64; ++lag)
                if (correlation(lag) > correlation(best)) best = lag;
            const double cm = correlation(best - 1), c0 = correlation(best), cp = correlation(best + 1);
            // ws1 merge, intentional circuit-engine change: the oscillator's
            // 8-internal-sample kernel delays 4/2/1 host samples at 2x/4x/8x and
            // is reported as latency, so compare delays net of the reported latency.
            const int reportedDifference = 8 / 2 - 8 / (quality > 1.5f ? 8 : kStandardOversampling);
            const double lag = best + 0.5 * (cm - cp) / (cm - 2 * c0 + cp) + reportedDifference;
            double eRef = 0, eOther = 0;
            for (int n = 0; n < window; ++n) { eRef += left(reference, n) * left(reference, n); eOther += left(other, n) * left(other, n); }
            const double level = std::sqrt(eOther / std::max(eRef, 1.0e-30));
            std::printf("sine render %s vs Eco: lag %+.4f host samples, level ratio %.5f\n",
                        quality > 1.5f ? "HQ" : "Standard", lag, level);
            check(eRef > 1.0 && std::abs(lag) <= ws2::kCrossFactorLagSamples,
                  "every factor renders with the same host-sample delay");
            check(std::abs(level - 1.0) <= ws2::kCrossFactorLevelRelative, "every factor renders the same level");
        }
    }

    // The circuit engine's default quality is Standard; legacy default stays 2x.
    {
        Engine e;
        e.prepare(48000.0);
        check(e.getOversamplingFactor() == 2, "default ParamValues (legacy) stays 2x");
        e.setParams(circuit(paramDef(P_oversampling).def));
        check(e.getOversamplingFactor() == kStandardOversampling, "circuit default is Standard");
    }
}
} // namespace

int main()
{
    testFactorRule();
    testDecimatorDesign();
    testEngine();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
