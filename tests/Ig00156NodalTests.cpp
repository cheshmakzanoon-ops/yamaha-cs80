// Plan step 1.1b gate: circuit-engine IG00156 nodal stage (src/core/Ig00156Filter.h)
// against its offline truth model (src/reference/IG00156Network.h), with the
// pre-registered tolerances of tests/parity/tolerances-v2/ws1-voice.json.
// Agreement validates the solver against the declared equations; it is not
// hardware evidence (docs/IG00156_NODAL_MODEL.md).
#include "core/Engine.h"
#include "core/Ig00156Filter.h"
#include "core/SourceFactoryTones.h"
#include "reference/IG00156Network.h"
#include "reference/IG00156PhaseCorrection.h"
#include "Ws1Tolerances.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <iostream>
#include <vector>

using namespace cs80;
namespace ref = cs80::reference::ig00156;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* text) { ++checks; if (!ok) { ++failures; std::cerr << "FAIL: " << text << '\n'; } }
constexpr double pi = 3.14159265358979323846;

ref::StageNetwork toRef(const Ig00156Network& n)
{
    return { n.sourceOhms, n.groundOhms, n.inputFarads, n.feedbackOhms, n.feedbackFarads,
             n.integratorFarads, n.integratorFarads, n.feedforwardOhms, n.feedforwardOhms };
}
const ref::StageNetwork hpNet = toRef(kIg00156HighPassNetwork), lpNet = toRef(kIg00156LowPassNetwork);
const double ffHz = 1.0 / (2.0 * pi * 4.7e6 * 1.5e-9);

// Truth control for a TOTAL cutoff (law + feedforward) and a low-frequency Q,
// through the realtime's declared control mapping (ig00156IntegratorRate), so
// both models share the same gm. compensate = false is the uncompensated
// alternative (gm from the law alone).
const Ig00156Network& rtNet(const ref::StageNetwork& n)
{
    return n.sourceOhms == kIg00156HighPassNetwork.sourceOhms ? kIg00156HighPassNetwork : kIg00156LowPassNetwork;
}
ref::StageControl control(const ref::StageNetwork& n, double totalHz, double q, double ota = 0.0, bool compensate = true)
{
    const double gq = ref::qSiemensForLowFrequencyQ(n, q);
    const double rate = compensate ? ig00156IntegratorRate(rtNet(n), totalHz, gq) : 2.0 * pi * totalHz;
    const double gm = std::max(0.0, rate * n.c1Farads - 1.0 / n.feedforward1Ohms);
    return { gm, gm, gq, ota };
}
double prewarp(double totalHz, double fs) { const double t = pi * totalHz / fs; return std::tan(t) / t; }

// Realtime pair; hpHz/lpHz are TOTAL cutoffs, <= 0 bypasses the stage.
struct Realtime
{
    Ig00156Stage hp { kIg00156HighPassNetwork }, lp { kIg00156LowPassNetwork };
    double hpHz, lpHz, q, fs;
    Realtime(double h, double l, double qq, double rate) : hpHz(h), lpHz(l), q(qq), fs(rate)
    {
        if (hpHz > 0) hp.set((float) (hpHz - ffHz), (float) q, (float) fs);
        if (lpHz > 0) lp.set((float) (lpHz - ffHz), (float) q, (float) fs);
    }
    float operator()(float x)
    {
        if (hpHz > 0) x = hp.process(x).hp;
        if (lpHz > 0) x = lp.process(x).lp;
        return x;
    }
};

// Truth linear system for the same configuration, with explicit adapters.
struct Truth
{
    ref::LinearSystem<8> pair; ref::LinearSystem<4> single; ref::Vector<8> out8 {}; ref::Vector<4> out4 {};
    bool isPair = false;
    Truth(double hpHz, double lpHz, double q, double fs, bool warped)
    {
        const double gh = 1.0 / ref::passbandGain(hpNet), gl = 1.0 / ref::passbandGain(lpNet);
        if (hpHz > 0 && lpHz > 0)
        {
            isPair = true;
            pair = ref::pairSystem(hpNet, control(hpNet, hpHz, q), lpNet, control(lpNet, lpHz, q), gh, gl);
            out8 = ref::lpRow<8>(4);
            // The realtime prewarps each stage at its own cutoff; scale rows per stage.
            if (warped)
            {
                const double ph = prewarp(hpHz, fs), pl = prewarp(lpHz, fs);
                for (int i = 0; i < 8; ++i)
                {
                    const double p = i < 4 ? ph : pl;
                    for (auto& v : pair.a[(std::size_t) i]) v *= p;
                    pair.b[(std::size_t) i] *= p;
                }
            }
        }
        else
        {
            const bool hpStage = hpHz > 0;
            const auto& n = hpStage ? hpNet : lpNet;
            const double hz = hpStage ? hpHz : lpHz;
            single = ref::stageSystem(n, control(n, hz, q), hpStage ? gh : gl);
            out4 = hpStage ? ref::hpRow<4>(0) : ref::lpRow<4>(0);
            if (warped) single = ref::scaled(single, prewarp(hz, fs));
        }
    }
    std::complex<double> at(std::complex<double> s) const
    {
        return isPair ? ref::response(pair, out8, s) : ref::response(single, out4, s);
    }
};

// Independent per-stage four-state matrix discretization. The continuous
// source network and normalized adapters above are unchanged; this oracle
// does not call the production scalar elimination or its Jacobian caches.
struct DiscreteTruth
{
    ref::PhaseCorrectedLinearStage hp, lp;
    bool hpOn, lpOn;
    DiscreteTruth(double hpHz, double lpHz, double q, double fs)
        : hp(ref::stageSystem(hpNet, control(hpNet, std::max(hpHz, 1.0), q), 1.0/ref::passbandGain(hpNet)), fs),
          lp(ref::stageSystem(lpNet, control(lpNet, std::max(lpHz, 1.0), q), 1.0/ref::passbandGain(lpNet)), fs),
          hpOn(hpHz>0), lpOn(lpHz>0) {}
    double step(double input)
    {
        if(hpOn) input=hp.step(input,true);
        if(lpOn) input=lp.step(input,false);
        return input;
    }
    std::complex<double> at(double frequency) const
    {
        return (hpOn?hp.discrete(frequency,true):std::complex<double>(1,0))
             * (lpOn?lp.discrete(frequency,false):std::complex<double>(1,0));
    }
};

// 1. Realtime == the independent full-matrix phase-corrected discrete oracle.
void equivalence(const ws1::Tolerances& tol)
{
    const double limit = tol["equivalence_max_rel"];
    double worstAll = 0.0;
    for (double fs : { 88200.0, 96000.0, 192000.0, 384000.0 })
        for (double fc : { 60.0, 250.0, 1000.0, 4000.0, 12000.0 })
            for (double q : { 0.5, 1.0, 2.0, 5.0, 9.0 })
                for (int config = 0; config < 3; ++config)
                {
                    const double hpHz = config == 1 ? 0.0 : (config == 2 ? std::max(60.0, fc / 4.0) : fc);
                    const double lpHz = config == 0 ? 0.0 : fc;
                    Realtime rt(hpHz, lpHz, q, fs);
                    DiscreteTruth truth(hpHz, lpHz, q, fs);
                    double worst = 0.0, peak = 0.0;
                    const int n = 3000;
                    for (int i = 0; i < n; ++i)
                    {
                        const double t = i / fs;
                        const double u = (i < 40 ? 1e-4 : 0.0)
                                       + 1e-4 * std::sin(2 * pi * 0.73 * fc * t) + 6e-5 * std::sin(2 * pi * 2.9 * fc * t + 0.4);
                        const float y = rt((float) u);
                        const double yt=truth.step(u);
                        worst = std::max(worst, std::abs((double) y - yt));
                        peak = std::max(peak, std::abs(yt));
                    }
                    worstAll = std::max(worstAll, worst / peak);
                    if (worst > limit * peak)
                    {
                        std::fprintf(stderr, "equivalence fs=%g fc=%g q=%g config=%d rel=%g\n", fs, fc, q, config, worst / peak);
                        check(false, "realtime equals the independent full-matrix discrete truth (pre-registered equivalence_max_rel)");
                    }
                }
    std::printf("equivalence: worst %.3g of output peak (limit %.3g)\n", worstAll, limit);
    check(worstAll <= limit, "equivalence over the whole grid");
}

// 2. Discrete small-signal response versus the unchanged continuous truth.
// All registered cutoffs, Q values, configurations and rates are asserted;
// there are no passing-subgrid exemptions or widened tolerances.
struct SmallSignalResult { double core = 0.0, band = 0.0; };
SmallSignalResult smallSignalError(double fs, double fc)
{
    SmallSignalResult r;
    for (double q : { 0.5, 1.0, 2.0, 5.0, 9.0 })
        for (int config = 0; config < 3; ++config)
        {
            const double hpHz = config == 1 ? 0.0 : (config == 2 ? std::max(60.0, fc / 4.0) : fc);
            const double lpHz = config == 0 ? 0.0 : fc;
            Truth cont(hpHz, lpHz, q, fs, false);
            DiscreteTruth disc(hpHz, lpHz, q, fs);
            double peak = 0.0;
            for (double f = 5.0; f < std::min(24000.0, 0.45 * fs); f *= 1.01)
                peak = std::max(peak, std::abs(cont.at({ 0.0, 2 * pi * f })));
            const double hi = std::min(20000.0, fs / 5.0);
            for (double f = 20.0; f <= hi; f *= 1.02)
            {
                const auto hc = cont.at({ 0.0, 2 * pi * f });
                // Independent rational response including the causal input-history term.
                const auto hd = disc.at(f);
                const double e = std::abs(hd - hc) / peak;
                if (f >= 0.5 * fc && f <= 2.0 * fc && f <= fs / 8.0) r.core = std::max(r.core, e);
                r.band = std::max(r.band, e);
            }
        }
    return r;
}
void smallSignal(const ws1::Tolerances& tol)
{
    const double core = tol["small_signal_core_rel"], band = tol["small_signal_band_rel"];
    const double bandHi = tol["small_signal_band_hi_rate_rel"];
    bool gateMet = true;
    for (double fs : { 88200.0, 96000.0, 192000.0, 384000.0 })
    {
        const double bandLimit = fs >= 192000.0 ? bandHi : band;
        std::printf("small signal vs continuous truth at %g Hz:", fs);
        for (double fc : { 60.0, 250.0, 1000.0, 4000.0, 12000.0 })
        {
            const auto r = smallSignalError(fs, fc);
            const bool met = r.core <= core && r.band <= bandLimit;
            gateMet = gateMet && met;
            std::printf("  fc %g core %.4f band %.4f%s", fc, r.core, r.band, met ? "" : " (over)");
            check(r.core <= core, "whole-grid core error within small_signal_core_rel");
            check(r.band <= bandLimit, "whole-grid full-band error within the registered limit");
        }
        std::printf("\n");
    }
    std::printf("pre-registered small-signal gate over the whole grid: %s\n",
                gateMet ? "MET" : "NOT MET");

    // Direct lock-in measurement of the running realtime stage (not via the
    // discrete formula) at a few points, against the continuous truth.
    double worstDirect = 0.0;
    for (double fs : { 88200.0, 96000.0, 192000.0, 384000.0 })
      for (double fc : { 250.0, 4000.0, 12000.0 })
        for (double f : { 0.5 * fc, fc, 2.0 * fc })
        {
            if (f > std::min(20000.0, fs / 5.0)) continue;
            const double q = 5.0;
            Realtime rt(0.0, fc, q, fs);
            Truth cont(0.0, fc, q, fs, false);
            const int settle = (int) (2.0 * fs), measure = (int) (std::round(0.2 * f) / f * fs);
            std::complex<double> acc = 0.0;
            for (int i = 0; i < settle + measure; ++i)
            {
                const double ph = 2 * pi * f * i / fs;
                const float y = rt((float) (1e-4 * std::sin(ph)));
                if (i >= settle) acc += (double) y * std::complex<double>(std::sin(ph), std::cos(ph));
            }
            const auto h = acc * (2.0 / measure / 1e-4);
            double peak = 0.0;
            for (double g = 5.0; g < 24000.0; g *= 1.01) peak = std::max(peak, std::abs(cont.at({ 0.0, 2 * pi * g })));
            const auto hc = cont.at({ 0.0, 2 * pi * f });
            // y = Im(H e^{j ph}), so the lock-in sum estimates H directly.
            worstDirect = std::max(worstDirect, std::abs(h - hc) / peak);
        }
    std::printf("direct lock-in vs continuous truth: %.4f (core limit %.3f)\n", worstDirect, core);
    check(worstDirect <= core, "direct realtime measurement agrees with the truth response");
}

// 3. Large signal: realtime (OTA in the loop) versus the nonlinear truth at 16x.
void nonlinear(const ws1::Tolerances& tol)
{
    const double limit = tol["nonlinear_rms_rel"];
    double worst = 0.0;
    for (double q : { 5.0, 1.0 })
        for (double f : { 500.0, 1000.0 })
            for (double amp : { 1.0, 2.5 })
            {
                const double fs = 96000.0, fc = 1000.0;
                const int over = 16, n = (int) (1.5 * fs), tail = (int) (0.1 * fs);
                Realtime rt(fc, fc, q, fs);
                ref::NonlinearPair truth(hpNet, control(hpNet, fc, q, kIg00156OtaLevel), lpNet,
                                         control(lpNet, fc, q, kIg00156OtaLevel),
                                         1.0 / ref::passbandGain(hpNet), 1.0 / ref::passbandGain(lpNet));
                double err = 0.0, power = 0.0;
                for (int i = 0; i < n; ++i)
                {
                    for (int k = 0; i > 0 && k < over; ++k)
                    {
                        const double t0 = (i - 1 + (double) k / over) / fs, t1 = (i - 1 + (double) (k + 1) / over) / fs;
                        truth.step(amp * std::sin(2 * pi * f * t0), amp * std::sin(2 * pi * f * t1), 1.0 / (fs * over));
                    }
                    const float y = rt((float) (amp * std::sin(2 * pi * f * i / fs)));
                    if (i >= n - tail) { err += std::pow(y - truth.lp(), 2); power += std::pow(truth.lp(), 2); }
                }
                const double rel = std::sqrt(err / power);
                worst = std::max(worst, rel);
                std::printf("nonlinear q=%g f=%g amp=%g: rms rel %.4f\n", q, f, amp, rel);
            }
    check(worst <= limit, "large-signal agreement with the nonlinear truth (pre-registered nonlinear_rms_rel)");
}

struct PoleQ { double q, hz; };
PoleQ stagePoles(const ref::StageNetwork& n, double totalHz, double q, bool compensate = true)
{
    const auto sys = ref::stageSystem(n, control(n, totalHz, q, 0.0, compensate));
    auto r = ref::roots<4>(ref::characteristic<4>(sys.a));
    std::sort(r.begin(), r.end(), [](auto a, auto b) { return std::abs(a) < std::abs(b); });
    const auto p1 = r[1], p2 = r[2]; // drop the coupling (slowest) and 180 pF (fastest) poles
    const double w = std::sqrt((p1 * p2).real());
    return { w / std::abs((p1 + p2).real()), w / (2.0 * pi) };
}

// 4. W1 anchors: Q endpoints, cutoff control points, Q-block law consistency.
void anchors(const ws1::Tolerances& tol)
{
    const double qTol = tol["q_anchor_rel"], fTol = tol["cutoff_anchor_rel"];
    for (const auto* n : { &hpNet, &lpNet })
    {
        const Ig00156Network& rt = n == &hpNet ? kIg00156HighPassNetwork : kIg00156LowPassNetwork;
        for (double vq : { 0.0, 10.0 })
        {
            const double target = vq == 0.0 ? 5.0 : 0.5;
            const double law = source_factory::qFromVolts(vq);
            check(std::abs(law - target) < 1e-12, "documented Q endpoints of the low-frequency map");
            // The Q-block law in volts gives the same conductance the stage derives from Q.
            check(std::abs(ig00156QBlockSiemens(rt, vq) - ref::qSiemensForLowFrequencyQ(*n, target)) < 1e-15,
                  "Q-block law endpoints");
            const auto p = stagePoles(*n, 200.0, law);
            std::printf("Q anchor %s VQ=%g: pole Q %.4f (target %g)\n", n == &hpNet ? "HP" : "LP", vq, p.q, target);
            check(std::abs(p.q / target - 1.0) <= qTol, "pole-derived Q meets the documented endpoint (q_anchor_rel)");
        }
        // Intermediate VQ: affine gq <=> affine 1/Q, the existing qFromVolts map.
        for (double vq = 0.0; vq <= 10.0; vq += 0.5)
            check(std::abs(ig00156QBlockSiemens(rt, vq) - ref::qSiemensForLowFrequencyQ(*n, source_factory::qFromVolts(vq))) < 1e-12,
                  "Q-block law equals qFromVolts at every VQ");
        // Cutoff control points (service p42 / guide p34) at Q = 5.
        for (auto cp : { std::array<double, 2> { 0.25, 5.0 }, { 0.5, 5.0 }, { 0.25, 6.0 } })
        {
            const double gmHz = nominalFilterCutoff((float) cp[0], (float) cp[1]);
            const auto p = stagePoles(*n, gmHz + ffHz, 5.0);
            const auto u = stagePoles(*n, gmHz + ffHz, 5.0, false);
            std::printf("cutoff anchor %s %gV/%gV: law %.3f Hz, pole %.3f Hz (law + feedforward %.3f; uncompensated alternative %.3f)\n",
                        n == &hpNet ? "HP" : "LP", cp[0], cp[1], gmHz, p.hz, gmHz + ffHz, u.hz);
            check(std::abs(p.hz / (gmHz + ffHz) - 1.0) <= fTol, "cutoff anchor reproduced (cutoff_anchor_rel)");
        }
        // The calibration is defined, finite and strictly increasing over every
        // reachable target (0 .. 0.45 x 1.536 MHz) and Q.
        bool monotonic = true;
        for (double gq : { 0.0, ref::qSiemensForLowFrequencyQ(*n, 0.35), ref::qSiemensForLowFrequencyQ(*n, 9.0) })
        {
            double previous = 0.0;
            for (double hz = 1.0; hz <= 0.45 * 1536000.0; hz *= 1.01)
            {
                const double w = ig00156IntegratorRate(rt, hz, gq);
                monotonic = monotonic && std::isfinite(w) && w > previous && w >= 2.0 * pi * hz;
                previous = w;
            }
        }
        check(monotonic, "cutoff calibration finite, increasing and never below the target rate");
        // Model prediction of the 180 pF branch: less resonance at a high cutoff.
        double low = 0.0, high = 0.0;
        for (double hz : { 200.0, 1000.0, 4000.0, 12000.0 })
        {
            const auto p = stagePoles(*n, hz, 5.0), u = stagePoles(*n, hz, 5.0, false);
            std::printf("frequency-dependent resonance %s: Q setting 5 at %g Hz -> pole Q %.3f at %.1f Hz (uncompensated %.3f at %.1f Hz)\n",
                        n == &hpNet ? "HP" : "LP", hz, p.q, p.hz, u.q, u.hz);
            if (hz == 200.0) low = p.q;
            if (hz == 12000.0) high = p.q;
        }
        check(high < low, "resonance at 12 kHz is below resonance at 200 Hz (180 pF branch)");
    }
}

// 5. ADJ p11 service points in physical volts (linear stages, physical gains).
// Source-derived terminal map: CP2 probes the HP stage output, whereas CP3
// is wired to CP8 at the LP output (Yamaha M11/12 wiring p44). These are
// still ideal-stage *model* voltages, not measured or fully loaded terminals.
struct ServicePointVpp { double cp2Hp, cp3Lp; };
ServicePointVpp settledVpp(double hpTotal, double qHp, double lpTotal, double qLp,
                           const ref::StageNetwork& hpInput = hpNet, double pulseMixerGain = 0.5)
{
    const double fs = 192000.0, f0 = 261.6255653005986; // C1 at 2'
    // Keep the physical control values fixed at the original setup when
    // varying only the external mixer termination in an offline probe.
    auto sys = ref::pairSystem(hpInput, control(hpNet, hpTotal, qHp), lpNet, control(lpNet, lpTotal, qLp), 1.0, 1.0);
    ref::Vector<8> z {};
    double prev = 0.0, loHp = 1e9, hiHp = -1e9, loLp = 1e9, hiLp = -1e9;
    const int n = (int) (3.0 * fs);
    for (int i = 0; i < n; ++i)
    {
        const double phase = std::fmod(f0 * (i + 1) / fs, 1.0);
        const double pulse = phase < 0.5 ? 1.5 : -1.5;  // 3 Vpp square
        const double u = pulseMixerGain * pulse;          // saw gated off; NI boundary specified by caller
        z = sys.step(z, prev, u, 1.0 / fs).value;
        prev = u;
        if (i > n - (int) (0.2 * fs))
        {
            const double hpOut = 2.0 * (z[1] + z[2]) - z[3];
            loHp = std::min(loHp, hpOut); hiHp = std::max(hiHp, hpOut);
            loLp = std::min(loLp, z[7]); hiLp = std::max(hiLp, z[7]);
        }
    }
    return {hiHp - loHp, hiLp - loLp};
}
void serviceSetup(const ws1::Tolerances& tol)
{
    const auto hpBand = tol.range("hp_fh_4v0"), lpBand = tol.range("lp_fl_3v3");
    const double kv = source_factory::alignment.referenceKeyVolts;
    const double hpGm = 125.0 * kv * std::exp2(source_factory::alignment.hpEffectiveCv);
    const double lpGm = 125.0 * kv * std::exp2(source_factory::alignment.lpEffectiveCv);
    const double open = 18000.0, closed = 0.0;
    const auto hp = settledVpp(hpGm + ffHz, 5.0, open + ffHz, 5.0);
    const auto lp = settledVpp(closed + ffHz, 10.0 / 11.0, lpGm + ffHz, 5.0);
    // Pre-registered model-only NI termination endpoints (2026-11-02):
    // original NI-open = 150k pulse || 150k ground -> 75k, gain 1/2;
    // ideal zero-volt NO1 behind NI 150k adds a third equal conductance
    // -> 50k, gain 1/3. The hardware NO1 voltage/output Z are unknown.
    auto niZeroHp = hpNet;
    niZeroHp.sourceOhms = 50.0e3;
    const auto hpNiZero = settledVpp(hpGm + ffHz, 5.0, open + ffHz, 5.0, niZeroHp, 1.0 / 3.0);
    const auto lpNiZero = settledVpp(closed + ffHz, 10.0 / 11.0, lpGm + ffHz, 5.0, niZeroHp, 1.0 / 3.0);
    const double hpVpp = hp.cp3Lp, lpVpp = lp.cp3Lp;
    std::printf("service HP setup (FH 4.0 V): %.3f Vpp, documented %.1f..%.1f\n", hpVpp, hpBand.first, hpBand.second);
    std::printf("service LP setup (FL 3.3 V): %.3f Vpp, documented %.1f..%.1f\n", lpVpp, lpBand.first, lpBand.second);
    std::printf("service model stage probes (NOT hardware): HP setup CP2 %.3f Vpp, downstream CP3/CP8 %.3f Vpp; LP setup CP2 %.3f Vpp, downstream CP3/CP8 %.3f Vpp\n",
                hp.cp2Hp, hp.cp3Lp, lp.cp2Hp, lp.cp3Lp);
    std::printf("service model NI ideal-zero hypothesis (NOT hardware; NO1 source/impedance unknown): HP setup CP2 %.3f Vpp, downstream CP3/CP8 %.3f Vpp; LP setup CP2 %.3f Vpp, downstream CP3/CP8 %.3f Vpp\n",
                hpNiZero.cp2Hp, hpNiZero.cp3Lp, lpNiZero.cp2Hp, lpNiZero.cp3Lp);
    std::printf("physical passband gains: HP %.4f, LP %.4f (normalized to 1 in audio)\n",
                ref::passbandGain(hpNet), ref::passbandGain(lpNet));
    // Registered as reported, never fitted: print the verdict, assert only that
    // the evaluation itself is sound.
    std::printf("ADJ p11 HP point: %s; LP point: %s\n",
                hpVpp >= hpBand.first && hpVpp <= hpBand.second ? "within band" : "MISS",
                lpVpp >= lpBand.first && lpVpp <= lpBand.second ? "within band" : "MISS");
    check(std::isfinite(hpVpp) && hpVpp > 0.0 && std::isfinite(lpVpp) && lpVpp > 0.0
              && std::isfinite(hp.cp2Hp) && hp.cp2Hp > 0.0
              && std::isfinite(lp.cp2Hp) && lp.cp2Hp > 0.0
              && std::isfinite(hpNiZero.cp2Hp) && hpNiZero.cp2Hp > 0.0
              && std::isfinite(hpNiZero.cp3Lp) && hpNiZero.cp3Lp > 0.0
              && std::isfinite(lpNiZero.cp2Hp) && lpNiZero.cp2Hp > 0.0
              && std::isfinite(lpNiZero.cp3Lp) && lpNiZero.cp3Lp > 0.0,
          "service-point evaluation is finite");
}

// 6. Stability on hard noise at every supported internal rate.
void stability(const ws1::Tolerances& tol)
{
    // The bound keeps the first-stage rationale: each stage driven directly by
    // +/-8 noise has resonant gain at most Q (9), so 200 rejects only
    // divergence. The cascade (gain up to Q^2) is checked for finiteness and
    // for no growth between the two halves of the run.
    const double bound = tol["stability_bound"];
    Rng rng(0xf11);
    bool finite = true, bounded = true, settled = true;
    for (float fs : { 88200.0f, 96000.0f, 192000.0f, 384000.0f })
        for (float fc : { 0.0f, 18.0f, 200.0f, 2000.0f, 20000.0f, 0.45f * fs })
            for (float q : { 0.35f, 1.0f, 5.0f, 9.0f })
            {
                Ig00156Stage hp { kIg00156HighPassNetwork }, lp { kIg00156LowPassNetwork };
                Ig00156Stage hpC { kIg00156HighPassNetwork }, lpC { kIg00156LowPassNetwork };
                hp.set(fc, q, fs); lp.set(fc, q, fs); hpC.set(fc, q, fs); lpC.set(fc, q, fs);
                float firstHalf = 0.0f, secondHalf = 0.0f;
                for (int n = 0; n < 20000; ++n)
                {
                    const float x = 8.0f * rng.nextBipolar();
                    const auto a = hp.process(x);
                    const auto b = lp.process(x);
                    for (float v : { a.hp, a.lp, a.bp, b.hp, b.lp, b.bp })
                    {
                        finite = finite && std::isfinite(v);
                        bounded = bounded && std::abs(v) < bound;
                    }
                    const auto c = lpC.process(hpC.process(x).hp);
                    float& half = n < 10000 ? firstHalf : secondHalf;
                    for (float v : { c.hp, c.lp, c.bp })
                    {
                        finite = finite && std::isfinite(v);
                        half = std::max(half, std::abs(v));
                    }
                }
                settled = settled && secondHalf <= 2.0f * firstHalf + 1.0f;
            }
    check(finite, "nodal stages stay finite on hard noise at all rates, cutoffs and Q");
    check(bounded, "each nodal stage stays bounded on +/-8 noise (stability_bound)");
    check(settled, "the HP -> LP cascade does not grow on hard noise");
    // Modulated cutoff and Q at audio rate stay finite.
    Ig00156Stage s { kIg00156LowPassNetwork };
    bool modulated = true;
    for (int n = 0; n < 96000; ++n)
    {
        s.set(2000.0f + 1900.0f * std::sin(n * 0.01f), 5.0f + 4.0f * std::sin(n * 0.003f), 96000.0f);
        modulated = modulated && std::isfinite(s.process(3.0f * std::sin(n * 0.07f)).lp);
    }
    check(modulated, "audio-rate cutoff and Q modulation stays finite");
}

// 7. The input and interstage coupling capacitors block DC.
void couplingBlocksDc()
{
    // An LP stage alone would pass DC; only its series 1 uF coupling removes it.
    // (A pair cannot isolate this: the HP response already blocks DC.)
    for (const auto* net : { &kIg00156HighPassNetwork, &kIg00156LowPassNetwork })
    {
        Ig00156Stage s { *net };
        s.set(4000.0f, 1.0f, 96000.0f);
        float y = 0.0f, early = 0.0f;
        for (int i = 0; i < 96000 * 4; ++i)
        {
            y = s.process(0.5f).lp;
            if (i < 960) early = std::max(early, std::abs(y));
        }
        check(std::abs(y) < 1e-4f, "series 1 uF coupling removes a held DC input from the LP node");
        check(early > 0.1f, "the step itself passes before the coupling recovers");
    }
}

// 8. Resonance compresses at high level (in-loop saturation, not an input clip).
double resonantGain(double amplitude)
{
    Ig00156Stage f { kIg00156LowPassNetwork };
    f.set((float) (1000.0 - ffHz), 5.0f, 96000.0f);
    double peak = 0.0;
    for (int n = 0; n < 96000; ++n)
    {
        const double y = f.process((float) (amplitude * std::sin(2 * pi * 1000.0 * n / 96000.0))).lp;
        if (n > 48000) peak = std::max(peak, std::abs(y));
    }
    return peak / amplitude;
}
void saturationInLoop()
{
    const double quiet = resonantGain(0.01), loud = resonantGain(2.0);
    std::printf("LP resonant gain at 1 kHz, Q 5: quiet %.3f, loud %.3f\n", quiet, loud);
    check(quiet > 4.0 && quiet < 5.5, "small-signal resonant gain close to Q at 1 kHz");
    check(loud < 0.6 * quiet, "resonance compresses at high level (in-loop saturation)");
    check(loud > 0.0, "resonance does not collapse at high level");
}

void engineSelection()
{
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
    check(legacyA != circuit, "circuit policy uses the IG00156 nodal stage");
    bool finite = true;
    for (float v : circuit) finite = finite && std::isfinite(v);
    check(finite, "circuit render is finite");
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const ws1::Tolerances tol(argc > 1 ? argv[1] : "tests/parity/tolerances-v2/ws1-voice.json", "ig00156_nodal");
        equivalence(tol);
        smallSignal(tol);
        anchors(tol);
        serviceSetup(tol);
        stability(tol);
        couplingBlocksDc();
        saturationInLoop();
        engineSelection();
        nonlinear(tol);
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return 2;
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
