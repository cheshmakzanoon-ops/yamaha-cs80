#pragma once
// OFFLINE truth model for the circuit-engine IG00156 nodal stage pair (plan
// step 1.1b, docs/IG00156_NODAL_MODEL.md). Double precision, straight from the
// node equations; never used for audio. It extends the conditional graph of
// IG00156Linear.h (same 1.2 MOhm/180 pF branch, 4.7 MOhm feedforward, 1.5 nF
// integrators, ideal amplifiers, unilateral Q current Iq = -gq * BP) with the
// series input coupling capacitor and source resistance of each stage's AI
// node, and the HP -> LP cascade. Which source/ground values the CS-80 uses is
// a separate (partly inferred) choice made by the caller; see the model doc.
#include "IG00156Linear.h"

#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <stdexcept>

namespace cs80::reference::ig00156
{
// One IG00156 stage plus its AI/FB node network. SI units throughout.
struct StageNetwork
{
    double sourceOhms;  // series source resistance into the AI node
    double groundOhms;  // AI-node shunt to ground
    double inputFarads; // series coupling capacitor between source and AI node
    double feedbackOhms = 1.2e6;
    double feedbackFarads = 180e-12;
    double c1Farads = 1.5e-9;
    double c2Farads = 1.5e-9;
    double feedforward1Ohms = 4.7e6;
    double feedforward2Ohms = 4.7e6;
};

// Transconductances of the two gm blocks and the Q block. otaVolts > 0 makes
// the two integrator gm blocks I = gm * L * tanh(v / L) (the declared OTA law);
// 0 keeps them linear. The Q block and both 4.7 MOhm paths stay linear.
struct StageControl
{
    double gm1Siemens = 0.0, gm2Siemens = 0.0, qSiemens = 0.0;
    double otaVolts = 0.0;
};

inline double totalConductance(const StageNetwork& n)
{
    return 1.0 / n.sourceOhms + 1.0 / n.groundOhms + 1.0 / n.feedbackOhms;
}
// Algebraic-limit (Cfb -> 0, coupling shorted) passband gain of AI-source to HP/LP.
inline double passbandGain(const StageNetwork& n)
{
    return 2.0 / n.sourceOhms / totalConductance(n);
}
// Unilateral-Q hypothesis: Q = Gtotal / (2 (Gf + gq)) in the algebraic limit
// with equal integrators (IG00156_MODEL_EVIDENCE.md), so the Q-block
// conductance that sets a low-frequency Q is Gtotal / (2 Q) - Gf.
inline double qSiemensForLowFrequencyQ(const StageNetwork& n, double q)
{
    positive(q);
    return std::max(0.0, totalConductance(n) / (2.0 * q) - 1.0 / n.feedbackOhms);
}

// State of one stage: [vin, x, b, l] = input-coupling capacitor voltage,
// AI - BP (180 pF voltage), BP, LP; all volts, physical (inverting-integrator)
// polarity. AI = x + b, HP = 2 AI - LP.
inline constexpr std::size_t kStageStates = 4;

namespace detail
{
// Writes one stage's rows into a larger system. sourceRow/sourceGain give the
// stage source u = sourceGain * (sourceRow . state) + inputGain * input.
template<std::size_t N>
void stageRows(Matrix<N>& a, Vector<N>& b, std::size_t o, const StageNetwork& n,
               const StageControl& c, const Vector<N>& sourceRow, double inputGain)
{
    const double gs = 1.0 / n.sourceOhms, gg = 1.0 / n.groundOhms, gf = 1.0 / n.feedbackOhms;
    const double w1 = (c.gm1Siemens + 1.0 / n.feedforward1Ohms) / n.c1Farads;
    const double w2 = (c.gm2Siemens + 1.0 / n.feedforward2Ohms) / n.c2Farads;
    // i = Gs (u - vin - x - b) flows through Rs and Cin into the AI node.
    Vector<N> i {};
    for (std::size_t j = 0; j < N; ++j) i[j] = gs * sourceRow[j];
    i[o] -= gs; i[o + 1] -= gs; i[o + 2] -= gs;
    for (std::size_t j = 0; j < N; ++j)
    {
        a[o][j] = i[j] / n.inputFarads;
        a[o + 1][j] = i[j] / n.feedbackFarads;
        a[o + 2][j] = a[o + 3][j] = 0.0;
    }
    b[o] = gs * inputGain / n.inputFarads;
    b[o + 1] = gs * inputGain / n.feedbackFarads;
    b[o + 2] = b[o + 3] = 0.0;
    // Cfb dx/dt = i - Gg (x + b) - Gf x + gq b
    a[o + 1][o + 1] -= (gg + gf) / n.feedbackFarads;
    a[o + 1][o + 2] += (c.qSiemens - gg) / n.feedbackFarads;
    // db/dt = -w1 (2x + 2b - l); dl/dt = -w2 b
    a[o + 2][o + 1] = -2.0 * w1; a[o + 2][o + 2] = -2.0 * w1; a[o + 2][o + 3] = w1;
    a[o + 3][o + 2] = -w2;
}
} // namespace detail

// Single stage driven by input * inputGain.
inline LinearSystem<4> stageSystem(const StageNetwork& n, const StageControl& c, double inputGain = 1.0)
{
    LinearSystem<4> s;
    detail::stageRows<4>(s.a, s.b, 0, n, c, Vector<4> {}, inputGain);
    return s;
}
// HP stage -> LP stage. The HP op-amp output (pin 15) is an ideal source for
// the LP input branch, so the cascade is one-way but keeps the coupling
// capacitor current; hpGain/lpGain are explicit source adapters.
inline LinearSystem<8> pairSystem(const StageNetwork& hp, const StageControl& hc,
                                  const StageNetwork& lp, const StageControl& lc,
                                  double hpGain = 1.0, double lpGain = 1.0)
{
    LinearSystem<8> s;
    detail::stageRows<8>(s.a, s.b, 0, hp, hc, Vector<8> {}, hpGain);
    Vector<8> hpOut {};
    hpOut[1] = 2.0 * lpGain; hpOut[2] = 2.0 * lpGain; hpOut[3] = -lpGain;
    detail::stageRows<8>(s.a, s.b, 4, lp, lc, hpOut, 0.0);
    return s;
}
template<std::size_t N> Vector<N> hpRow(std::size_t o)
{
    Vector<N> r {}; r[o + 1] = 2.0; r[o + 2] = 2.0; r[o + 3] = -1.0; return r;
}
template<std::size_t N> Vector<N> lpRow(std::size_t o) { Vector<N> r {}; r[o + 3] = 1.0; return r; }

// Uniformly prewarped system: every rate multiplied by p. Its trapezoidal
// step is the bilinear transform with all frequencies warped by p, i.e. the
// realtime discretization. p = tan(theta) / theta pins theta = w T / 2 exactly.
template<std::size_t N> LinearSystem<N> scaled(LinearSystem<N> s, double p)
{
    for (auto& row : s.a) for (double& v : row) v *= p;
    for (double& v : s.b) v *= p;
    return s;
}

// Exact continuous response C (sI - A)^-1 B by complex partial-pivot elimination.
template<std::size_t N>
std::complex<double> response(const LinearSystem<N>& s, const Vector<N>& c, std::complex<double> lap)
{
    using cd = std::complex<double>;
    std::array<std::array<cd, N + 1>, N> m {};
    for (std::size_t i = 0; i < N; ++i)
    {
        for (std::size_t j = 0; j < N; ++j) m[i][j] = -s.a[i][j];
        m[i][i] += lap;
        m[i][N] = s.b[i];
    }
    for (std::size_t col = 0; col < N; ++col)
    {
        std::size_t pivot = col;
        for (std::size_t r = col + 1; r < N; ++r)
            if (std::abs(m[r][col]) > std::abs(m[pivot][col])) pivot = r;
        if (std::abs(m[pivot][col]) == 0.0) throw std::runtime_error("IG00156 network: singular response");
        std::swap(m[pivot], m[col]);
        for (std::size_t r = col + 1; r < N; ++r)
        {
            const cd f = m[r][col] / m[col][col];
            for (std::size_t j = col; j <= N; ++j) m[r][j] -= f * m[col][j];
        }
    }
    std::array<cd, N> x {};
    for (std::size_t ii = N; ii > 0; --ii)
    {
        const std::size_t i = ii - 1;
        cd v = m[i][N];
        for (std::size_t j = i + 1; j < N; ++j) v -= m[i][j] * x[j];
        x[i] = v / m[i][i];
    }
    cd y = 0.0;
    for (std::size_t i = 0; i < N; ++i) y += c[i] * x[i];
    return y;
}

// det(sI - A) coefficients (monic, highest first) by Faddeev-LeVerrier, and
// its roots by Durand-Kerner: enough for the 4-state stage pole/Q readouts.
template<std::size_t N> std::array<double, N + 1> characteristic(const Matrix<N>& a)
{
    std::array<double, N + 1> c {};
    c[0] = 1.0;
    Matrix<N> m {}; // M_0 = 0; M_k = A M_(k-1) + c_(k-1) I; c_k = -trace(A M_k) / k
    for (std::size_t k = 1; k <= N; ++k)
    {
        Matrix<N> next {};
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t j = 0; j < N; ++j)
            {
                double v = i == j ? c[k - 1] : 0.0;
                for (std::size_t l = 0; l < N; ++l) v += a[i][l] * m[l][j];
                next[i][j] = v;
            }
        double trace = 0.0;
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t l = 0; l < N; ++l) trace += a[i][l] * next[l][i];
        c[k] = -trace / static_cast<double>(k);
        m = next;
    }
    return c;
}
template<std::size_t N> std::array<std::complex<double>, N> roots(const std::array<double, N + 1>& c)
{
    using cd = std::complex<double>;
    std::array<cd, N> z {};
    double radius = 0.0;
    for (std::size_t k = 1; k <= N; ++k) radius = std::max(radius, std::pow(std::abs(c[k]), 1.0 / k));
    for (std::size_t i = 0; i < N; ++i) z[i] = radius * std::polar(1.0, 0.4 + 6.283185307179586 * i / N);
    for (int it = 0; it < 4000; ++it)
    {
        double change = 0.0;
        for (std::size_t i = 0; i < N; ++i)
        {
            cd p = c[0];
            for (std::size_t k = 1; k <= N; ++k) p = p * z[i] + c[k];
            cd q = 1.0;
            for (std::size_t j = 0; j < N; ++j) if (j != i) q *= z[i] - z[j];
            const cd d = p / q;
            z[i] -= d;
            change = std::max(change, std::abs(d) / std::max(1.0, std::abs(z[i])));
        }
        if (change < 1e-15) break;
    }
    return z;
}

// Implicit trapezoid with full Newton on the NONLINEAR stage pair (OTA law in
// StageControl). Step it at a fine step for the large-signal truth.
class NonlinearPair
{
public:
    NonlinearPair(const StageNetwork& hp, const StageControl& hc, const StageNetwork& lp,
                  const StageControl& lc, double hpGain, double lpGain)
        : net { hp, lp }, ctl { hc, lc }, gain { hpGain, lpGain } {}
    const Vector<8>& state() const noexcept { return z; }
    double hp() const noexcept { return 2.0 * (z[1] + z[2]) - z[3]; }
    double lp() const noexcept { return z[7]; }
    // Advances by seconds with the input at both endpoints.
    void step(double u0, double u1, double seconds)
    {
        const auto f0 = derivative(z, u0, nullptr);
        Vector<8> next = z;
        for (std::size_t j = 0; j < 8; ++j) next[j] += seconds * f0[j];
        for (int it = 0; it < 50; ++it)
        {
            Matrix<8> jac {};
            const auto f1 = derivative(next, u1, &jac);
            Vector<8> r {};
            Matrix<8> m {};
            double size = 0.0, scale = 1e-30;
            for (std::size_t i = 0; i < 8; ++i)
            {
                r[i] = -(next[i] - z[i] - 0.5 * seconds * (f0[i] + f1[i]));
                for (std::size_t j = 0; j < 8; ++j) m[i][j] = (i == j ? 1.0 : 0.0) - 0.5 * seconds * jac[i][j];
                size = std::max(size, std::abs(r[i]));
                scale = std::max(scale, std::abs(next[i]));
            }
            if (size <= 1e-14 * scale) break;
            const auto d = solve(m, r).value;
            for (std::size_t j = 0; j < 8; ++j) next[j] += d[j];
        }
        z = next;
    }

private:
    Vector<8> derivative(const Vector<8>& s, double u, Matrix<8>* jac) const
    {
        Vector<8> d {};
        for (std::size_t k = 0; k < 2; ++k)
        {
            const auto& n = net[k];
            const auto& c = ctl[k];
            const std::size_t o = 4 * k;
            const double gs = 1.0 / n.sourceOhms, gg = 1.0 / n.groundOhms, gf = 1.0 / n.feedbackOhms;
            const double gff1 = 1.0 / n.feedforward1Ohms, gff2 = 1.0 / n.feedforward2Ohms;
            const double hpH = 2.0 * (s[1] + s[2]) - s[3];
            const double src = k == 0 ? gain[0] * u : gain[1] * hpH;
            const double vin = s[o], x = s[o + 1], b = s[o + 2], l = s[o + 3];
            const double a = x + b, h = 2.0 * a - l;
            const double i = gs * (src - vin - a);
            const auto ota = [&](double gm, double v, double& slope) {
                if (c.otaVolts <= 0.0) { slope = gm; return gm * v; }
                const double t = std::tanh(v / c.otaVolts);
                slope = gm * (1.0 - t * t);
                return gm * c.otaVolts * t;
            };
            double s1 = 0.0, s2 = 0.0;
            const double i1 = ota(c.gm1Siemens, h, s1), i2 = ota(c.gm2Siemens, b, s2);
            d[o] = i / n.inputFarads;
            d[o + 1] = (i - gg * a - gf * x + c.qSiemens * b) / n.feedbackFarads;
            d[o + 2] = -(i1 + gff1 * h) / n.c1Farads;
            d[o + 3] = -(i2 + gff2 * b) / n.c2Farads;
            if (!jac) continue;
            auto& J = *jac;
            // d i / d state
            Vector<8> di {};
            di[o] = -gs; di[o + 1] = -gs; di[o + 2] = -gs;
            if (k == 1) { di[1] += 2.0 * gs * gain[1]; di[2] += 2.0 * gs * gain[1]; di[3] -= gs * gain[1]; }
            for (std::size_t j = 0; j < 8; ++j)
            {
                J[o][j] = di[j] / n.inputFarads;
                J[o + 1][j] = di[j] / n.feedbackFarads;
            }
            J[o + 1][o + 1] -= (gg + gf) / n.feedbackFarads;
            J[o + 1][o + 2] += (c.qSiemens - gg) / n.feedbackFarads;
            const double dh = (s1 + gff1) / n.c1Farads;
            J[o + 2][o + 1] = -2.0 * dh; J[o + 2][o + 2] = -2.0 * dh; J[o + 2][o + 3] = dh;
            J[o + 3][o + 2] = -(s2 + gff2) / n.c2Farads;
        }
        return d;
    }
    std::array<StageNetwork, 2> net;
    std::array<StageControl, 2> ctl;
    std::array<double, 2> gain;
    Vector<8> z {};
};
} // namespace cs80::reference::ig00156
