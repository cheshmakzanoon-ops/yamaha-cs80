#pragma once
// Circuit-engine IG00156 nodal stage (plan step 1.1b, docs/IG00156_NODAL_MODEL.md).
// Used by the circuit engine (Source Performance and Isolation) only; legacy
// keeps Svf::process. Offline truth model: src/reference/IG00156Network.h.
//
// Per stage, four capacitor states from the CS-80 M41 graph
// (docs/IG00156_MODEL_EVIDENCE.md):
//   vin  series 1 uF input coupling into the AI/FB node     (source-derived C)
//   x    AI - BP across the 1.2 MOhm || 180 pF branch        (source-derived)
//   b    BP, 1.5 nF integrator, 4.7 MOhm feedforward          (source-derived)
//   l    LP, 1.5 nF integrator, 4.7 MOhm feedforward          (source-derived)
// with ideal amplifiers (HP = 2 AI - LP, inverting integrators; CS20M F-3/F-4)
// and the unilateral Q-block current Iq = -gq * BP.
//
// INFERRED (alternatives in the model doc):
// - Q-block law: gq is affine in VQ (gm proportional to a control current
//   I0 + VQ / Rq), calibrated per stage so Q = 5 at VQ = 0 V and Q = 0.5 at
//   VQ = 10 V (service p42) in the algebraic limit. Equivalently 1/Q is affine
//   in VQ there, which is exactly the existing provisionalFilterQ and
//   source_factory::qFromVolts maps, so the stage takes that low-frequency Q.
// - Source resistances: HP mixing node 75 kOhm (two 150 kOhm waveform
//   branches); LP 150 kOhm in series with the interstage 1 uF.
// - Integrator OTAs saturate as gm * L * tanh(v / L), L = kIg00156OtaLevel in
//   engine units at each stage's normalized scale. The Q block and the 4.7 MOhm
//   paths are linear.
// - Each stage is driven at 1 / (2 Gs / Gtotal) so its nominal passband gain is
//   unity in engine units: no absolute port-voltage calibration exists. The
//   physical gains are reported by passbandGain().
// - Cutoff calibration: the documented law plus the 4.7 MOhm contribution is
//   the network's natural frequency; gm is raised to undo the 180 pF branch's
//   frequency depression (ig00156IntegratorRate). Uncompensated alternative:
//   -2.3 % at 1 kHz, -4.4 % at 2 kHz, -20 % at 12 kHz.
//
// Discretization: trapezoidal companion model with every capacitor prewarped
// by the same factor p = tan(theta) / theta, theta = pi * fTotal / fs (the
// bilinear transform warped to pin the total cutoff exactly). The linear
// states are eliminated per sample; one scalar Newton solve on BP remains, with
// the same cached-slope start and 1e-7 tolerance as the first-stage model.
#include "Dsp.h"
#include "FastMath.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace cs80
{
struct Ig00156Network
{
    double sourceOhms, groundOhms, inputFarads;
    double feedbackOhms = 1.2e6, feedbackFarads = 180e-12;
    double integratorFarads = 1.5e-9, feedforwardOhms = 4.7e6;
};
// HP IC10: AI1 receives the passive mixing node of the 150 kOhm waveform
// branches through 1 uF NP; 33 kOhm AI shunt (M41).
inline constexpr Ig00156Network kIg00156HighPassNetwork { 75.0e3, 33.0e3, 1.0e-6 };
// LP IC11: HP pin 15 -> 1 uF NP -> CP2 -> CP7 -> AI1 (M41, harness p44).
// INFERRED: the 150 kOhm drawn at this node is read as the series source.
inline constexpr Ig00156Network kIg00156LowPassNetwork { 150.0e3, 33.0e3, 1.0e-6 };

inline double ig00156TotalConductance(const Ig00156Network& n) noexcept
{
    return 1.0 / n.sourceOhms + 1.0 / n.groundOhms + 1.0 / n.feedbackOhms;
}

// Integrator rate w = (gm + 1/Rff) / C, rad/s, that places the stage's
// natural frequency at targetHz. In the ideal graph the 180 pF branch feeds BP
// back into OP1 and lowers the natural frequency by about
// 1 / sqrt(1 + 2 w Cfb / Gtotal) (-2.3 % at 1 kHz, -19 % at 10 kHz).
// INFERRED calibration: the documented control law (service p42, guide p34)
// describes the network's cutoff, so gm is raised to compensate. First-order
// (algebraic AI node) model: W^2 = w^2 / (1 + 2 w / a), a = Gtotal / Cfb, solved
// in closed form for w. Against the exact 3-state poles at Q 5 the network
// lands +0.3 % high at 1 kHz, +0.6 % at 2 kHz, about +3 % at 10 kHz. An exact
// cubic-factorization Newton was rejected: it has no stable root for
// overdamped (low-Q) settings above about 5 kHz. The law only matters for
// audible cutoffs and the first-order model fails near the 33-39 kHz AI-node
// corner, so above 20 kHz the compensation ratio is held at its 20 kHz value.
// Monotonic, always defined.
inline constexpr double kIg00156CompensationTopHz = 20000.0;
inline double ig00156IntegratorRate(const Ig00156Network& n, double targetHz, double /*qSiemens*/) noexcept
{
    const double alpha = ig00156TotalConductance(n) / n.feedbackFarads;
    const double twoPi = 2.0 * 3.14159265358979323846;
    const double om = twoPi * std::min(targetHz, kIg00156CompensationTopHz), om2 = om * om;
    if (om <= 0.0) return 0.0;
    const double beta = om2 / alpha; // half of 2 W^2 / a
    return (beta + std::sqrt(beta * beta + om2)) / om * twoPi * targetHz;
}

class Ig00156Stage
{
public:
    explicit Ig00156Stage(const Ig00156Network& n = kIg00156HighPassNetwork) noexcept : net(n)
    {
        gs = 1.0 / net.sourceOhms;
        gg = 1.0 / net.groundOhms;
        gf = 1.0 / net.feedbackOhms;
        gTotal = gs + gg + gf;
        inputScale = gTotal / (2.0 * gs);
        feedforwardHz = 1.0 / (2.0 * 3.14159265358979323846 * net.feedforwardOhms * net.integratorFarads);
    }

    // Trapezoidal companion histories, volts at the normalized scale.
    struct State { double input = 0.0, feedback = 0.0, bp = 0.0, lp = 0.0; };
    State getState() const noexcept { return { zIn, zFb, zBp, zLp }; }
    void reset() noexcept
    {
        zIn = zFb = zBp = zLp = 0.0;
        vIn = vFb = vBp = vLp = 0.0;
        slope1 = slope2 = 1.0;
    }

    // Node voltages of the last processed sample (the same four capacitors).
    using Nodes = State;
    Nodes getNodes() const noexcept { return { vIn, vFb, vBp, vLp }; }
    // Free-running idle path (plan step 2.1): restore histories and the node
    // voltages they belong to, e.g. from an independent reference.
    void setState(const State& histories, const Nodes& nodes) noexcept
    {
        zIn = histories.input; zFb = histories.feedback; zBp = histories.bp; zLp = histories.lp;
        vIn = nodes.input; vFb = nodes.feedback; vBp = nodes.bp; vLp = nodes.lp;
    }
    // Compact memory for the idle snapshot ring (histories, node voltages,
    // Newton slope caches and the half steps they were clocked with). Restoring
    // it forces the next set() to recompute every coefficient.
    struct Memory { State z; Nodes v; double slope1 = 1.0, slope2 = 1.0, halfIntegrators = 0.0, halfCouplings = 0.0; };
    Memory getMemory() const noexcept
    {
        return { getState(), getNodes(), slope1, slope2, kHalf, kHalfCoupling };
    }
    void restoreMemory(const Memory& m) noexcept
    {
        setState(m.z, m.v);
        slope1 = m.slope1; slope2 = m.slope2;
        kHalf = m.halfIntegrators; kHalfCoupling = m.halfCouplings;
        lastCutoff = lastQ = lastRate = -1.0f;
    }
    // Half steps of the present coefficients: integrators (p * T / 2) and the
    // two coupling capacitors (equal unless couplingsUnwarped).
    struct HalfSteps { double integrators = 0.0, couplings = 0.0; };
    HalfSteps halfSteps() const noexcept { return { kHalf, kHalfCoupling }; }
    // Re-express the trapezoidal histories for a new clock. For every
    // capacitor z = v + kHalf * dv/dt after a sample, so keeping the node
    // voltages v and their derivatives and replacing kHalf is the
    // rate-independent transfer. Call after set() at the new rate, passing
    // the half steps the histories were last clocked with.
    void reReference(const HalfSteps& previous) noexcept
    {
        if (previous.couplings > 0.0 && previous.couplings != kHalfCoupling)
        {
            const double r = kHalfCoupling / previous.couplings;
            zIn = vIn + r * (zIn - vIn);
            zFb = vFb + r * (zFb - vFb);
        }
        if (previous.integrators > 0.0 && previous.integrators != kHalf)
        {
            const double r = kHalf / previous.integrators;
            zBp = vBp + r * (zBp - vBp);
            zLp = vLp + r * (zLp - vLp);
        }
    }

    // gmCutoffHz is the documented control-law part (125 Kv 2^V); the 4.7 MOhm
    // feedforward adds feedforwardHz internally. q is the low-frequency Q.
    // couplingsUnwarped (free-running idle path only, plan step 2.1): the two
    // non-integrator capacitors (1 uF input coupling, 180 pF branch) use the
    // plain trapezoidal half step instead of the integrators' prewarp. At the
    // 3 kHz idle step a cutoff clamped near Nyquist would otherwise stretch the
    // 0.1-0.2 s coupling poles by tan(theta)/theta (about 4.5x). The audio path
    // never sets it, so its coefficients are unchanged.
    void set(float gmCutoffHz, float q, float sampleRate, bool couplingsUnwarped = false) noexcept
    {
        q = clampf(q, 0.35f, 9.0f);
        // The two companion half-steps, gm and the linear 2x2 elimination
        // depend on cutoff/rate, not on resonance. A moving Q pot should only
        // change the affine Q current and its one coefficient in that block;
        // in particular it must not re-evaluate tan(), sqrt() and the inverse
        // while the clock is unchanged. Retain the original expression order
        // below so Q-only retargeting is bit-identical to a full prepare.
        const bool clockChanged = gmCutoffHz != lastCutoff
                               || sampleRate != lastRate
                               || couplingsUnwarped != lastUnwarped;
        if (!clockChanged && q == lastQ)
            return;
        lastCutoff = gmCutoffHz; lastQ = q; lastRate = sampleRate; lastUnwarped = couplingsUnwarped;
        qSiemensValue = std::max(0.0, gTotal / (2.0 * q) - gf);
        if (clockChanged)
        {
            const double fs = sampleRate;
            // Network natural frequency = control law + 4.7 MOhm feedforward.
            const double total = std::clamp(static_cast<double>(gmCutoffHz), 0.0,
                                            std::max(0.0, 0.45 * fs - feedforwardHz)) + feedforwardHz;
            const double rate = ig00156IntegratorRate(net, total, qSiemensValue);
            const double ffRate = 1.0 / (net.feedforwardOhms * net.integratorFarads);
            const double theta = 3.14159265358979323846 * total / fs;
            // CPU pass: bounded-error tan (relative 1e-10 here, FastMath.h).
            kHalf = 0.5 * (fastmath::tan(theta) / theta) / fs; // p * T / 2, warp pinned at total
            kHalfCoupling = couplingsUnwarped ? 0.5 / fs : kHalf;
            gmPart = kHalf * std::max(0.0, rate - ffRate); // kHalf * gm / C
            ffPart = kHalf * ffRate;
            // Linear block (vin, x) given BP: [a11 a12; a21 a22] [vin; x] = rhs.
            const double ki = kHalfCoupling / net.inputFarads;
            cachedKf = kHalfCoupling / net.feedbackFarads;
            kiGs = ki * gs;
            cachedA11 = 1.0 + kiGs;
            cachedA21 = cachedKf * gs;
            const double a22 = 1.0 + cachedKf * gTotal;
            cachedDet = cachedA11 * a22 - kiGs * cachedA21;
            xFromFb = cachedA11 / cachedDet;
            xFromIn = -cachedA21 / cachedDet;
            xFromU = cachedKf * gs / cachedDet;
            inFromX = kiGs / cachedA11;
            invA11 = 1.0 / cachedA11;
        }
        // The exact old expression, including its floating-point order,
        // now uses cached clock-dependent factors on resonance-only edits.
        xFromBp = (cachedA11 * cachedKf * (qSiemensValue - gs - gg) + cachedA21 * kiGs) / cachedDet;
    }

    struct Output { float lp = 0.0f, hp = 0.0f, bp = 0.0f; };

    Output process(float input) noexcept
    {
        // Exact silent fixed point of all four capacitor equations. Every
        // untouched stage spends its lifetime here until the first real
        // waveform; the implicit BP Newton solve would evaluate two tanh
        // functions just to rediscover zero on every sample. The positive-zero
        // bit guards also preserve the old solver's signed-zero output (BP is
        // -0), and the slope guards keep restoreMemory() semantics exact.
        // Node voltages are overwritten by the full solve, so do likewise.
        if (std::bit_cast<std::uint32_t>(input) == 0u
            && std::bit_cast<std::uint64_t>(zIn) == 0u
            && std::bit_cast<std::uint64_t>(zFb) == 0u
            && std::bit_cast<std::uint64_t>(zBp) == 0u
            && std::bit_cast<std::uint64_t>(zLp) == 0u
            && slope1 == 1.0 && slope2 == 1.0)
        {
            vIn = vFb = vBp = vLp = 0.0;
            return { 0.0f, 0.0f, -0.0f };
        }
        const double u = input * inputScale;
        const double level = kIg00156OtaLevel, inv = 1.0 / level;
        const double g1 = gmPart, f1 = ffPart, g2 = gmPart, f2 = ffPart;
        // HP = c0 + c1 * BP + (BP-integrator current term), x = x0 + xFromBp * BP.
        const double x0 = xFromFb * zFb + xFromIn * zIn + xFromU * u;
        const double c0 = 2.0 * x0 - zLp, c1 = 2.0 * (xFromBp + 1.0);
        // Linear start from each OTA's previous effective gain (cached slope).
        const double e1 = g1 * slope1 + f1, e2 = g2 * slope2 + f2;
        double bp = (zBp - e1 * c0) / (1.0 + e1 * (c1 + e2));
        double hp = 0.0, t1 = 0.0, t2 = 0.0;
        for (int iteration = 0;; ++iteration)
        {
            t2 = fastTanh(bp * inv);
            hp = c0 + c1 * bp + g2 * level * t2 + f2 * bp;
            t1 = fastTanh(hp * inv);
            const double f = bp - zBp + g1 * level * t1 + f1 * hp;
            if (std::abs(f) < 1.0e-7 || iteration == 4)
                break;
            const double dh = c1 + g2 * (1.0 - t2 * t2) + f2;
            bp -= f / (1.0 + (g1 * (1.0 - t1 * t1) + f1) * dh);
        }
        const double lp = zLp - g2 * level * t2 - f2 * bp;
        const double x = x0 + xFromBp * bp;
        const double vin = (zIn + kiGs * u - kiGs * bp) * invA11 - inFromX * x;
        slope1 = std::abs(hp * inv) > 1.0e-9 ? t1 / (hp * inv) : 1.0;
        slope2 = std::abs(bp * inv) > 1.0e-9 ? t2 / (bp * inv) : 1.0;
        zIn = 2.0 * vin - zIn;
        zFb = 2.0 * x - zFb;
        zBp = 2.0 * bp - zBp;
        zLp = 2.0 * lp - zLp;
        vIn = vin; vFb = x; vBp = bp; vLp = lp;
        // BP is reported in the normalized (non-inverted) SVF polarity.
        return { static_cast<float>(lp), static_cast<float>(hp), static_cast<float>(-bp) };
    }

    const Ig00156Network& network() const noexcept { return net; }
    // Physical algebraic-limit gain from the AI source to HP/LP, normalized away.
    double passbandGain() const noexcept { return 1.0 / inputScale; }
    double qSiemens() const noexcept { return qSiemensValue; }
    double feedforwardCutoffHz() const noexcept { return feedforwardHz; }

private:
    Ig00156Network net;
    double gs = 0.0, gg = 0.0, gf = 0.0, gTotal = 0.0, inputScale = 1.0, feedforwardHz = 0.0;
    double kHalf = 0.0, kHalfCoupling = 0.0, gmPart = 0.0, ffPart = 0.0, qSiemensValue = 0.0;
    double kiGs = 0.0, xFromFb = 1.0, xFromIn = 0.0, xFromU = 0.0, xFromBp = 0.0;
    double inFromX = 0.0, invA11 = 1.0;
    // Constant Jacobian/elimination factors of the linear AI/FB block.
    // A Q-only update changes xFromBp but not this cached 2x2 inverse.
    double cachedKf = 0.0, cachedA11 = 1.0, cachedA21 = 0.0, cachedDet = 1.0;
    double zIn = 0.0, zFb = 0.0, zBp = 0.0, zLp = 0.0;
    double vIn = 0.0, vFb = 0.0, vBp = 0.0, vLp = 0.0;
    double slope1 = 1.0, slope2 = 1.0;
    float lastCutoff = -1.0f, lastQ = -1.0f, lastRate = -1.0f;
    bool lastUnwarped = false;
};

// Q-block conductance of the selected affine law at a Q-pin voltage, for the
// anchors and the doc: gq(VQ) = gq(0) + (gq(10) - gq(0)) * VQ / 10 with
// gq(0) and gq(10) fixed by Q = 5 and Q = 0.5.
inline double ig00156QBlockSiemens(const Ig00156Network& n, double vqVolts) noexcept
{
    const double total = 1.0 / n.sourceOhms + 1.0 / n.groundOhms + 1.0 / n.feedbackOhms;
    const double gf = 1.0 / n.feedbackOhms;
    const double at0 = total / 10.0 - gf, at10 = total / 1.0 - gf;
    return at0 + (at10 - at0) * std::clamp(vqVolts, 0.0, 10.0) / 10.0;
}
} // namespace cs80
