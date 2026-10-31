// Circuit-engine IG00156 filter stage (docs/IG00156_REALTIME_MODEL.md).
// Checks the declared model: linear small-signal agreement, in-loop
// saturation, stability, the source-derived feedforward floor and the fast
// tanh bound. Not hardware evidence.
#include "core/Dsp.h"
#include "core/Engine.h"
#include <array>
#include <cmath>
#include <iostream>
#include <vector>
using namespace cs80;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* text) { ++checks; if (!ok) { ++failures; std::cerr << text << '\n'; } }

// Plain linear TPT SVF (no clip) for the small-signal reference.
struct LinearTpt
{
    double s1 = 0.0, s2 = 0.0, g = 0.0, k = 1.0;
    void set(double fc, double q, double sr) { g = std::tan(kPi * fc / sr); k = 1.0 / q; }
    std::array<double, 3> process(double x)
    {
        const double hp = (x - (g + k) * s1 - s2) / (1.0 + g * (g + k));
        const double bp = s1 + g * hp, lp = s2 + g * bp;
        s1 = 2.0 * bp - s1; s2 = 2.0 * lp - s2;
        return { lp, hp, bp };
    }
};

void smallSignalMatchesLinear()
{
    for (float sr : { 88200.0f, 96000.0f, 192000.0f })
        for (float fc : { 40.0f, 400.0f, 4000.0f, 16000.0f })
            for (float q : { 0.5f, 1.0f, 5.0f })
            {
                Svf circuit; LinearTpt linear;
                circuit.set(fc, q, sr); linear.set(fc, q, sr);
                double worst = 0.0, scale = 0.0;
                for (int n = 0; n < 4000; ++n)
                {
                    const double x = 1e-4 * std::sin(0.013 * n) + 5e-5 * std::sin(0.31 * n);
                    const auto a = circuit.processCircuit((float) x);
                    const auto b = linear.process(x);
                    worst = std::max({ worst, std::abs(a.lp - b[0]), std::abs(a.hp - b[1]), std::abs(a.bp - b[2]) });
                    scale = std::max({ scale, std::abs(b[0]), std::abs(b[1]), std::abs(b[2]) });
                }
                check(worst <= 2e-3 * scale + 1e-9, "small signals reproduce the linear TPT response");
            }
}

double resonantGain(double amplitude)
{
    Svf f;
    f.set(1000.0f, 5.0f, 96000.0f);
    double peak = 0.0;
    for (int n = 0; n < 96000; ++n)
    {
        const double y = f.processCircuit((float) (amplitude * std::sin(kTwoPi * 1000.0 * n / 96000.0))).bp;
        if (n > 48000) peak = std::max(peak, std::abs(y));
    }
    return peak / amplitude;
}

void saturationIsInsideTheLoop()
{
    // Linear (and input-clipped) filters keep the same resonant gain at any
    // level; the in-loop OTA law compresses resonance as the level rises.
    const double quiet = resonantGain(0.01), loud = resonantGain(2.0);
    check(quiet > 4.5 && quiet < 5.5, "small-signal resonant gain equals Q");
    check(loud < 0.6 * quiet, "resonance compresses at high level (in-loop saturation)");
    check(loud > 0.0, "resonance does not collapse at high level");
}

void stableEverywhere()
{
    Rng rng(0xf11);
    bool finite = true, bounded = true;
    for (float sr : { 88200.0f, 96000.0f, 192000.0f, 384000.0f })
        for (float fc : { 18.0f, 200.0f, 2000.0f, 20000.0f, 0.45f * sr })
            for (float q : { 0.35f, 1.0f, 5.0f, 9.0f })
            {
                Svf f;
                f.set(fc, q, sr);
                for (int n = 0; n < 20000; ++n)
                {
                    const float x = 8.0f * rng.nextBipolar();
                    const auto y = f.processCircuit(x);
                    finite = finite && std::isfinite(y.lp) && std::isfinite(y.hp) && std::isfinite(y.bp);
                    // Resonant gain is at most Q (9) on the +/-8 input; the bound only
                    // rejects divergence, not a level law.
                    bounded = bounded && std::abs(y.lp) < 200.0f && std::abs(y.bp) < 200.0f;
                }
            }
    check(finite, "circuit filter stays finite for hard noise at all rates, cutoffs and Q");
    check(bounded, "circuit filter state stays bounded");
}

void feedforwardFloorIsDerived()
{
    const double derived = 1.0 / (kTwoPi * 4.7e6 * 1.5e-9);
    check(std::abs(kIg00156FeedforwardHz - derived) < 0.01, "feedforward floor equals 1/(2 pi 4.7M 1.5n)");
}

void fastTanhBound()
{
    double worst = 0.0, worstCore = 0.0, previous = -2.0;
    bool monotonic = true, odd = true;
    for (int i = -80000; i <= 80000; ++i)
    {
        const double x = i * 1e-4, y = fastTanh(x);
        worst = std::max(worst, std::abs(y - std::tanh(x)));
        if (std::abs(x) < 3.0) worstCore = std::max(worstCore, std::abs(y - std::tanh(x)));
        if (y < previous) monotonic = false;
        if (fastTanh(-x) != -y) odd = false;
        previous = y;
    }
    check(worst < 1e-4 && worstCore < 1e-6, "fastTanh error bounds hold");
    check(monotonic && odd, "fastTanh is monotonic and odd");
}

void engineUsesCircuitStageOnlyInCircuitPolicy()
{
    // A loud low note through a resonant LPF: legacy (input clip) and circuit
    // (in-loop) renders must differ, and legacy must equal a second legacy run.
    auto render = [](float source) {
        auto p = defaultValues();
        p[P_sourcePerformance] = source;
        for (int ch = 0; ch < 2; ++ch) { p[chParam(ch, CP_lpfRes)] = 0.9f; p[chParam(ch, CP_lpfCutoff)] = 900.0f; }
        Engine e; e.setParams(p); e.prepare(48000.0); e.noteOn(36, 1.0f);
        std::vector<float> out(48000 * 2);
        e.render(out.data(), 48000);
        return out;
    };
    const auto legacyA = render(0.0f), legacyB = render(0.0f), circuit = render(1.0f);
    check(legacyA == legacyB, "legacy render is deterministic");
    check(legacyA != circuit, "circuit policy uses the IG00156 stage");
    bool finite = true;
    for (float v : circuit) finite = finite && std::isfinite(v);
    check(finite, "circuit render is finite");
}
} // namespace

int main()
{
    smallSignalMatchesLinear();
    saturationIsInsideTheLoop();
    stableEverywhere();
    feedforwardFloorIsDerived();
    fastTanhBound();
    engineUsesCircuitStageOnlyInCircuitPolicy();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
