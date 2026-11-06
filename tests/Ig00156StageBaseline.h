#pragma once
// Frozen verbatim from b5e0f1f:src/core/Ig00156Filter.h, class name only renamed.
// Shares unchanged network definitions and helpers with the production header.
#include "core/Ig00156Filter.h"
namespace cs80 {
class Ig00156StageBaseline
{
public:
    explicit Ig00156StageBaseline(const Ig00156Network& n = kIg00156HighPassNetwork) noexcept : net(n)
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
        if (gmCutoffHz == lastCutoff && q == lastQ && sampleRate == lastRate && couplingsUnwarped == lastUnwarped)
            return;
        lastCutoff = gmCutoffHz; lastQ = q; lastRate = sampleRate; lastUnwarped = couplingsUnwarped;
        const double fs = sampleRate;
        qSiemensValue = std::max(0.0, gTotal / (2.0 * q) - gf);
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
        const double ki = kHalfCoupling / net.inputFarads, kf = kHalfCoupling / net.feedbackFarads;
        kiGs = ki * gs;
        const double a11 = 1.0 + kiGs, a21 = kf * gs, a22 = 1.0 + kf * gTotal;
        const double det = a11 * a22 - kiGs * a21;
        xFromFb = a11 / det;
        xFromIn = -a21 / det;
        xFromU = kf * gs / det;
        xFromBp = (a11 * kf * (qSiemensValue - gs - gg) + a21 * kiGs) / det;
        inFromX = kiGs / a11;
        invA11 = 1.0 / a11;
    }

    struct Output { float lp = 0.0f, hp = 0.0f, bp = 0.0f; };

    Output process(float input) noexcept
    {
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
    double zIn = 0.0, zFb = 0.0, zBp = 0.0, zLp = 0.0;
    double vIn = 0.0, vFb = 0.0, vBp = 0.0, vLp = 0.0;
    double slope1 = 1.0, slope2 = 1.0;
    float lastCutoff = -1.0f, lastQ = -1.0f, lastRate = -1.0f;
    bool lastUnwarped = false;
};

} // namespace cs80
