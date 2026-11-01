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

// The amplitude EG can become Idle while the *downstream* coupling capacitor
// still stores charge. The original free-running idle shortcut silently
// discarded -coupling, causing an abrupt output cut at the active/idle
// boundary. Use a charged zero-signal pedestal so the assertion cannot be
// passed by oscillator phase coincidences or by making the VCA fade away.
// Truth: the analytical zero-drive RC solution, independently of processClosed.
void closedCapacitorDischarge(const ws1::Tolerances& tol)
{
    const double truthTol = tol["closed_discharge_truth_abs"];
    const double stepTol = tol["closed_handoff_step_abs"];
    for (double rate : { 88200.0, 96000.0, 192000.0 })
    {
        Ig00151Vca vca, continuing;
        vca.prepare(rate);
        continuing.prepare(rate);
        vca.reset(2.0f);
        continuing.reset(2.0f);
        Adsr amp;
        amp.reset();
        amp.noteOn(0.002f, 0.015f, 0.7f, 0.005f);
        float outputAtClose = 0.0f;
        for (int sample = 0; sample < (int) std::lround(0.2 * rate); ++sample)
        {
            const float envelope = amp.tick((float) rate, true, true);
            outputAtClose = vca.process(0.0f, 2.0f, 0.0f, envelope);
            continuing.process(0.0f, 2.0f, 0.0f, envelope);
        }
        amp.noteOff();
        int releaseSamples = 0;
        do
        {
            const float envelope = amp.tick((float) rate, true, true);
            outputAtClose = vca.process(0.0f, 2.0f, 0.0f, envelope);
            continuing.process(0.0f, 2.0f, 0.0f, envelope);
            ++releaseSamples;
        }
        while (!amp.isIdle() && releaseSamples < (int) std::lround(0.05 * rate));
        check(amp.isIdle() && releaseSamples > 0, "fast released EG reaches Idle before the coupling capacitor discharges");
        check(std::abs(outputAtClose) > 100.0 * stepTol,
              "charged VCA coupling output remains audible at envelope closure; hard mute would make a step");

        // This closed form is independent of the realtime recurrence. The
        // first closed sample must be near the previous one, not a forced 0.
        bool analyticMatch = true, fullPathMatch = true;
        for (int sample = 1; sample <= (int) std::lround(0.2 * rate); ++sample)
        {
            const float closed = vca.processClosed();
            const float full = continuing.process(0.0f, 2.0f, 0.0f, 0.0f);
            const double truth = (double) outputAtClose
                               * std::exp(-sample / (kIg00151CouplingSeconds * rate));
            analyticMatch = analyticMatch && std::abs((double) closed - truth) <= truthTol;
            fullPathMatch = fullPathMatch && std::abs((double) closed - full) <= truthTol;
            if (sample == 1)
                check(std::abs(closed - outputAtClose) <= stepTol,
                      "VCA active/idle handoff preserves its charged downstream capacitor continuously");
        }
        check(analyticMatch, "closed VCA output equals analytical downstream coupling discharge at every sample");
        check(fullPathMatch, "closed fast path equals the full zero-envelope VCA process at every sample");
    }
}

// Engine wiring regression, distinct from the closed-circuit unit truth. Keep
// two fully deterministic engines bit-identical until both have retired the
// same released key. An explicit MIDI All Sound Off then clears only the
// second engine's coupling state. If the first one's downstream capacitor
// really stays in the rank mix after amplitude Idle, their next 50 ms differ;
// if the engine silently discards processClosed(), they remain identical.
// Silent oscillators make the difference VCA pedestal-driven, not waveform or
// allocation phase. Deliberate panic must still hard-mute the second engine.
void closedEngineTailInMix(const ws1::Tolerances& tol)
{
    constexpr int hostRate = 48000;
    constexpr int heldFrames = hostRate / 4;
    constexpr int comparisonFrames = hostRate / 20;

    ParamValues p;
    p[P_sourcePerformance] = 1.0f;
    p[P_parityMode] = 0.0f;
    p[P_mix] = 0.0f;
    p[P_keyRelease] = 0.0f;
    p[P_masterSustain] = 0.0f;
    p[P_drift] = 0.0f;
    p[P_voiceDispersion] = 0.0f;
    p[P_analogNoise] = 0.0f;
    p[P_chorusOn] = 0.0f;
    p[P_arpOn] = 0.0f;
    p[P_volume] = 0.6f;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_saw)] = 0.0f;
        p[chParam(rank, CP_pulse)] = 0.0f;
        p[chParam(rank, CP_sine)] = 0.0f;
        p[chParam(rank, CP_noise)] = 0.0f;
        p[chParam(rank, CP_vcfLevel)] = 0.0f;
        p[chParam(rank, CP_vLevel)] = 2.6f;
        p[chParam(rank, CP_vA)] = 0.002f;
        p[chParam(rank, CP_vD)] = 0.015f;
        p[chParam(rank, CP_vS)] = 0.7f;
        p[chParam(rank, CP_vR)] = 0.005f;
    }

    Engine normal, panic;
    normal.setParams(p);
    panic.setParams(p);
    normal.prepare(hostRate);
    panic.prepare(hostRate);
    normal.noteOn(60, 0.9f);
    panic.noteOn(60, 0.9f);
    float n[2] {}, h[2] {};
    bool identicalBeforePanic = true;
    auto clockTwins = [&] {
        normal.render(n, 1);
        panic.render(h, 1);
        identicalBeforePanic = identicalBeforePanic && n[0] == h[0] && n[1] == h[1];
    };
    for (int frame = 0; frame < heldFrames; ++frame)
        clockTwins();
    normal.noteOff(60);
    panic.noteOff(60);

    int releasedFrames = 0;
    while (normal.activeVoices() > 0 && releasedFrames < hostRate)
    {
        clockTwins();
        ++releasedFrames;
    }
    check(releasedFrames > 0 && releasedFrames < hostRate && normal.activeVoices() == 0
          && panic.activeVoices() == 0,
          "both engine twins retire the identical fast-release amplitude envelope");
    check(identicalBeforePanic, "two silent-source engine twins are sample-identical before the diagnostic panic");

    panic.allNotesOff(false);
    double maxTailDifference = 0.0;
    for (int frame = 0; frame < comparisonFrames; ++frame)
    {
        normal.render(n, 1);
        panic.render(h, 1);
        maxTailDifference = std::max(maxTailDifference,
                                     std::max(std::abs((double) n[0] - h[0]), std::abs((double) n[1] - h[1])));
    }
    std::printf("engine capacitor-tail peak after retirement (normal minus hard panic): %.9g\n", maxTailDifference);
    check(maxTailDifference > tol["closed_engine_tail_min_peak_abs"],
          "inactive circuit's capacitor discharge remains wired into the engine output mix");
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
        closedCapacitorDischarge(tol);
        closedEngineTailInMix(tol);
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
