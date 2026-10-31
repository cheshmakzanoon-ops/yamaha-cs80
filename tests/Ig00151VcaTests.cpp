// Plan step 1.4 gate: circuit-engine IG00151 voice VCA (src/core/Vca.h) with
// the tolerances pre-registered in tests/parity/tolerances-v2/ws1-voice.json.
// Checks the declared model laws; not hardware evidence (docs/IG00151_VCA_MODEL.md).
#include "core/Engine.h"
#include "core/Vca.h"
#include "Ws1Tolerances.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <vector>

using namespace cs80;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* text) { ++checks; if (!ok) { ++failures; std::cerr << "FAIL: " << text << '\n'; } }

// Static gain of a settled VCA (pedestal removed by subtracting the zero-input output).
double settledGain(double rate, float li, float ei, float env)
{
    Ig00151Vca v; v.prepare(rate); v.reset(li);
    const float with = v.process(1.0f, li, ei, env);
    Ig00151Vca z; z.prepare(rate); z.reset(li);
    const float without = z.process(0.0f, li, ei, env);
    return (double) with - (double) without;
}

void levelLaws(const ws1::Tolerances& tol)
{
    const double linTol = tol["linear_law_rel"], expTol = tol["exponential_law_rel"];
    for (double rate : { 88200.0, 96000.0, 192000.0 })
    {
        const double base = settledGain(rate, 0.8f, 0.0f, 0.7f);
        for (float k : { 0.25f, 0.5f, 1.0f, 2.0f })
            check(std::abs(settledGain(rate, 0.8f * k, 0.0f, 0.7f) / base / k - 1.0) <= linTol,
                  "gain proportional to the settled linear (LI) control");
        for (float oct : { -1.0f, -0.5f, 0.25f, 1.0f })
            check(std::abs(settledGain(rate, 0.8f, oct, 0.7f) / base / std::exp2((double) oct) - 1.0) <= expTol,
                  "gain follows 2^EI on the exponential input");
        // Envelope scales the same control current.
        check(std::abs(settledGain(rate, 0.8f, 0.0f, 0.35f) / base / 0.5 - 1.0) <= linTol, "envelope scales the control linearly");
        check(settledGain(rate, -0.3f, 0.0f, 1.0f) == 0.0, "negative linear drive floors at zero gain");
    }
}

void levelNode(const ws1::Tolerances& tol)
{
    const double tauTol = tol["li_tau_rel"];
    const double tau = kIg00151LevelNodeOhms * kIg00151LevelNodeFarads;
    check(std::abs(tau - 5.42e-4) < 2e-6, "declared LI time constant = Thevenin(180k, 560k, 3 x 270k) x 10 nF");
    for (double rate : { 88200.0, 96000.0, 192000.0 })
    {
        Ig00151Vca v; v.prepare(rate); v.reset(0.0f);
        // Crossing time interpolated between the bracketing samples, so the
        // measurement is not limited to one sample period (~2 % of tau).
        const double target = 1.0 - std::exp(-1.0);
        double crossed = -1.0, previous = 0.0;
        for (int n = 1; n < (int) (0.01 * rate); ++n)
        {
            v.process(0.0f, 1.0f, 0.0f, 0.0f);
            const double now = v.levelNode();
            if (crossed < 0.0 && now >= target) crossed = (n - 1 + (target - previous) / (now - previous)) / rate;
            previous = now;
        }
        std::printf("LI step at %g Hz: 1-1/e after %.4f ms (declared %.4f ms)\n", rate, crossed * 1e3, tau * 1e3);
        check(std::abs(crossed / tau - 1.0) <= tauTol, "LI node step response reaches 1 - 1/e at the declared RC");
    }
}

void pedestal(const ws1::Tolerances& tol)
{
    const double limit = tol["pedestal_db_max"];
    Ig00151Vca v; v.prepare(96000.0); v.reset(2.6f);
    const double dc = v.process(0.0f, 2.6f, 0.0f, 1.0f);
    const double gain = settledGain(96000.0, 2.6f, 0.0f, 1.0f);
    const double db = 20.0 * std::log10(std::abs(dc) / gain);
    std::printf("offset + feedthrough pedestal at full LI: %.2f dB re a unit signal\n", db);
    check(db <= limit, "DC pedestal at full level within pedestal_db_max");
    check(dc > 0.0, "the declared offset and feedthrough are present");
    Ig00151Vca closed; closed.prepare(96000.0); closed.reset(2.6f);
    check(closed.process(0.0f, 2.6f, 0.0f, 0.0f) == 0.0f, "no pedestal with the envelope closed (voices end without a DC step)");
    // The AC-coupled next stage leaves only an envelope-shaped transient.
    Ig00151Vca held; held.prepare(96000.0); held.reset(2.6f);
    float last = 0.0f;
    for (int n = 0; n < 96000 * 2; ++n) last = held.process(0.0f, 2.6f, 0.0f, 1.0f);
    check(std::abs(last) < 1e-5f * std::abs((float) dc), "output coupling removes the static pedestal (tau 0.1 s)");
}

void pan(const ws1::Tolerances& tol)
{
    const double limit = tol["pan_power_rel"];
    float l = 0.0f, r = 0.0f;
    constantPowerPan(0.0f, l, r);
    check(l == 0.5f && r == 0.5f, "centre pan keeps the former 0.5 per side exactly");
    bool constant = true;
    for (int i = -100; i <= 100; ++i)
    {
        constantPowerPan(i / 100.0f, l, r);
        constant = constant && std::abs((l * l + r * r) / 0.5 - 1.0) <= limit && l >= 0.0f && r >= 0.0f;
    }
    check(constant, "constant power over the whole pan range (pan_power_rel)");
    constantPowerPan(-1.0f, l, r);
    check(r == 0.0f && std::abs(l - std::sqrt(0.5f)) < 1e-7f, "hard left is -3 dB re a mono sum, right silent");
}

// SUB VCA modulation: exponential (constant dB depth) in the circuit engine,
// additive linear in legacy. Measure the output envelope ratio of a held tone.
double tremoloRatio(bool circuit, float depth)
{
    ParamValues p;
    p[P_sourcePerformance] = circuit ? 1.0f : 0.0f;
    p[P_mix] = 0.0f; p[P_detune] = 0.0f; p[P_drift] = 0.0f; p[P_voiceDispersion] = 0.0f;
    p[P_analogNoise] = 0.0f; p[P_chorusOn] = 0.0f; p[P_volume] = 0.2f;
    p[P_subOriginalMode] = 1.0f; p[P_subOriginalSpeed] = 2.0f; p[P_subVca] = depth;
    p[chParam(0, CP_saw)] = 0.0f; p[chParam(0, CP_pulse)] = 0.0f; p[chParam(0, CP_sine)] = 1.0f;
    p[chParam(0, CP_vcfLevel)] = 0.0f; p[chParam(0, CP_vS)] = 1.0f; p[chParam(0, CP_vA)] = 0.001f;
    p[chParam(0, CP_vLevel)] = 0.6f;
    Engine e; e.setParams(p); e.prepare(48000.0); e.noteOn(69, 1.0f);
    std::vector<float> out(48000 * 4 * 2);
    e.render(out.data(), 48000 * 4);
    // Peak in 10 ms windows over the last 2 s.
    double lo = 1e9, hi = 0.0;
    for (int w = 2 * 48000; w + 480 <= 4 * 48000; w += 480)
    {
        double peak = 0.0;
        for (int i = w; i < w + 480; ++i) peak = std::max(peak, (double) std::abs(out[(std::size_t) i * 2]));
        lo = std::min(lo, peak); hi = std::max(hi, peak);
    }
    return hi / lo;
}
void subOnExponentialInput()
{
    const double c25 = tremoloRatio(true, 0.25f), c50 = tremoloRatio(true, 0.5f);
    const double l25 = tremoloRatio(false, 0.25f), l50 = tremoloRatio(false, 0.5f);
    std::printf("SUB VCA tremolo max/min: circuit %.3f (d 0.25) %.3f (d 0.5); legacy %.3f %.3f\n", c25, c50, l25, l50);
    // Exponential: doubling depth squares the ratio (dB depth doubles).
    check(std::abs(std::log(c50) / std::log(c25) - 2.0) < 0.1, "circuit SUB tremolo depth is linear in dB (exponential input)");
    check(std::abs(c50 / std::exp2(2.0 * 0.5 * kIg00151SubOctaves) - 1.0) < 0.05, "full-swing ratio matches 2^(2 d octaves)");
    // Legacy additive law: (L + 0.45 d) / (L - 0.45 d) with L = 0.6.
    check(std::abs(l50 / ((0.6 + 0.225) / (0.6 - 0.225)) - 1.0) < 0.05, "legacy keeps its additive linear SUB law");
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const ws1::Tolerances tol(argc > 1 ? argv[1] : "tests/parity/tolerances-v2/ws1-voice.json", "ig00151_vca");
        levelLaws(tol);
        levelNode(tol);
        pedestal(tol);
        pan(tol);
        subOnExponentialInput();
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return 2;
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
