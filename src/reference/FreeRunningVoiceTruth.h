#pragma once

// Offline truth model for free-running voice continuity (IMPROVEMENT_PLAN 2.1).
// NOT used by Engine. One rank chain (oscillator -> IG00156 HP stage -> LP stage)
// advanced at the full internal rate on EVERY sample, in double precision, with
// the nodal-stage implicit equation solved to convergence. It restates the
// declared realtime stage law (src/core/Ig00156Filter.h: the M41 graph with
// the trapezoidal companion model and the inferred OTA tanh) without the
// realtime model's float controls, cached Newton start, fastTanh or iteration
// cap, so the cheap idle path in src/experimental/ContinuousVoiceCircuit.h can
// be held to it.
//
// v2 (ws1 + ws3 merge, 2026-10-31): the circuit engine's oscillator is the
// event-driven reconstructed WSC (src/core/ReconstructedOscillator.h). It is
// already a double-precision continuous-time model whose own accuracy is gated
// by cs80_reconstructed_oscillator_tests and cs80_oscillator_promotion_tests,
// so the truth ticks that model on every sample rather than restating its
// 300-line event solver; the idle path is held to per-sample ticking of it.
// v1 restated the PolyBLEP oscillator and the first-stage Svf circuit, which
// the circuit engine no longer runs.
//
// It is a numerical reference for the declared model, not a measured CS-80.

#include "core/Ig00156Filter.h"
#include "core/ReconstructedOscillator.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace cs80::reference::free_running
{
inline constexpr double kPi = 3.14159265358979323846;

struct RankControls
{
    double frequency = 261.6255653;
    double pulseWidth = 0.5;              // panel duty proxy
    bool saw = true, pulse = false;
    double noise = 0.0;                   // the truth model is noise-free; must stay 0
    double hpCutoff = 25.0, hpQ = 0.5;    // control-law part; the stage adds 4.7 MOhm feedforward
    double lpCutoff = 2200.0, lpQ = 0.7;
    bool hpOn = true, lpOn = true;
};

// Converged nodal IG00156 stage: the equations of Ig00156Stage::set/process.
class NodalStage
{
public:
    explicit NodalStage(const Ig00156Network& n) : net(n) {}
    std::array<double, 4> z {}, v {}; // histories and node voltages (vin, x, bp, lp)

    struct Output { double lp = 0.0, hp = 0.0, bp = 0.0; };
    Output tick(double input, double gmCutoffHz, double q, double fs)
    {
        const double gs = 1.0 / net.sourceOhms, gg = 1.0 / net.groundOhms, gf = 1.0 / net.feedbackOhms;
        const double gTotal = gs + gg + gf, inputScale = gTotal / (2.0 * gs);
        const double ffHz = 1.0 / (2.0 * kPi * net.feedforwardOhms * net.integratorFarads);
        q = std::clamp(q, 0.35, 9.0);
        const double qs = std::max(0.0, gTotal / (2.0 * q) - gf);
        const double total = std::clamp(gmCutoffHz, 0.0, std::max(0.0, 0.45 * fs - ffHz)) + ffHz;
        const double rate = ig00156IntegratorRate(net, total, qs);
        const double ffRate = 1.0 / (net.feedforwardOhms * net.integratorFarads);
        const double theta = kPi * total / fs;
        const double kHalf = 0.5 * (std::tan(theta) / theta) / fs;
        const double g = kHalf * std::max(0.0, rate - ffRate), f = kHalf * ffRate;
        const double ki = kHalf / net.inputFarads, kf = kHalf / net.feedbackFarads, kiGs = ki * gs;
        const double a11 = 1.0 + kiGs, a21 = kf * gs, a22 = 1.0 + kf * gTotal;
        const double det = a11 * a22 - kiGs * a21;
        const double xFromFb = a11 / det, xFromIn = -a21 / det, xFromU = kf * gs / det;
        const double xFromBp = (a11 * kf * (qs - gs - gg) + a21 * kiGs) / det;
        const double u = input * inputScale, level = kIg00156OtaLevel, inv = 1.0 / level;
        const double x0 = xFromFb * z[1] + xFromIn * z[0] + xFromU * u;
        const double c0 = 2.0 * x0 - z[3], c1 = 2.0 * (xFromBp + 1.0);
        double bp = (z[2] - (g + f) * c0) / (1.0 + (g + f) * (c1 + g + f));
        double hp = 0.0, t1 = 0.0, t2 = 0.0;
        for (int iteration = 0; iteration < 64; ++iteration)
        {
            t2 = std::tanh(bp * inv);
            hp = c0 + c1 * bp + g * level * t2 + f * bp;
            t1 = std::tanh(hp * inv);
            const double r = bp - z[2] + g * level * t1 + f * hp;
            if (std::abs(r) < 1.0e-14) break;
            const double dh = c1 + g * (1.0 - t2 * t2) + f;
            bp -= r / (1.0 + (g * (1.0 - t1 * t1) + f) * dh);
        }
        t2 = std::tanh(bp * inv);
        hp = c0 + c1 * bp + g * level * t2 + f * bp;
        const double lp = z[3] - g * level * t2 - f * bp;
        const double x = x0 + xFromBp * bp;
        const double vin = (z[0] + kiGs * u - kiGs * bp) / a11 - (kiGs / a11) * x;
        v = { vin, x, bp, lp };
        for (std::size_t k = 0; k < 4; ++k) z[k] = 2.0 * v[k] - z[k];
        return { lp, hp, -bp };
    }

private:
    Ig00156Network net;
};

class RankTruth
{
public:
    explicit RankTruth(double rate)
    {
        (void) osc.prepare(rate, OscillatorNominalProfile {});
    }
    // Synchronise to a realtime chain: oscillator state and both stages.
    void reset(const ReconstructedOscillator::State& oscillator,
               const std::array<double, 4>& hpZ, const std::array<double, 4>& hpV,
               const std::array<double, 4>& lpZ, const std::array<double, 4>& lpV)
    {
        osc.restoreState(oscillator);
        hp.z = hpZ; hp.v = hpV;
        lp.z = lpZ; lp.v = lpV;
    }
    // One internal sample. Returns the filtered (pre-VCA) rank signal.
    double tick(const RankControls& c, double rate)
    {
        WscControls w;
        w.pulseThresholdVolts = ReconstructedOscillator::panelThresholdVoltsInferred(c.pulseWidth);
        w.sawEnabled = c.saw;
        w.pulseEnabled = c.pulse;
        const auto pins = osc.tick(c.frequency, w);
        double filtered = (pins.sawPin5 + pins.pulsePin7) * kWscEngineUnitsPerVolt * 0.9;
        if (c.hpOn) filtered = hp.tick(filtered, c.hpCutoff, c.hpQ, rate).hp;
        if (c.lpOn) filtered = lp.tick(filtered, c.lpCutoff, c.lpQ, rate).lp;
        return filtered;
    }
    double getPhase() const { return osc.getPhase(); }
    const ReconstructedOscillator::State& oscillatorState() const { return osc.getState(); }
    const NodalStage& hpStage() const { return hp; }
    const NodalStage& lpStage() const { return lp; }

private:
    ReconstructedOscillator osc;
    NodalStage hp { kIg00156HighPassNetwork }, lp { kIg00156LowPassNetwork };
};
} // namespace cs80::reference::free_running
