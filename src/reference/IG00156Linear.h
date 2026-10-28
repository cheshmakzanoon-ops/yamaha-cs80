#pragma once
// OFFLINE experiment only. See docs/IG00156_REFERENCE_EXPERIMENT.md.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <initializer_list>
#include <stdexcept>

namespace cs80::reference::ig00156
{
struct PhysicalParameters
{
    // Rs and Rg are explicit bench-fixture choices, not CS-80 calibration.
    double sourceOhms;
    double groundOhms;
    double gm1Siemens;
    double gm2Siemens;
    double qSiemens; // HYPOTHESIS: current leaving AI = -qSiemens * BP.
    double feedbackOhms = 1.2e6;
    double feedbackFarads = 180e-12;
    double c1Farads = 1.5e-9;
    double c2Farads = 1.5e-9;
    double feedforward1Ohms = 4.7e6;
    double feedforward2Ohms = 4.7e6;
};

inline void positive(double v)
{
    if (!std::isfinite(v) || v <= 0.0)
        throw std::invalid_argument("IG00156: expected finite positive physical quantity");
}
inline void finite(double v)
{
    if (!std::isfinite(v))
        throw std::invalid_argument("IG00156: nonfinite state, input or derived coefficient");
}
inline void validate(const PhysicalParameters& p, bool algebraic)
{
    for (double v : {p.sourceOhms, p.groundOhms, p.feedbackOhms, p.c1Farads,
                     p.c2Farads, p.feedforward1Ohms, p.feedforward2Ohms})
        positive(v);
    for (double v : {p.gm1Siemens, p.gm2Siemens, p.qSiemens})
        if (!std::isfinite(v) || v < 0.0)
            throw std::invalid_argument("IG00156: gm must be finite and nonnegative");
    if (algebraic)
    {
        if (p.feedbackFarads != 0.0)
            throw std::invalid_argument("IG00156: algebraic model requires exactly Cfb=0");
    }
    else positive(p.feedbackFarads);
}
template<std::size_t N> using Vector = std::array<double, N>;
template<std::size_t N> using Matrix = std::array<Vector<N>, N>;

struct SolveDiagnostics { double minimumRelativePivot = 1.0; double relativeResidual = 0.0; };
template<std::size_t N> struct SolveResult
{
    Vector<N> value {};
    SolveDiagnostics diagnostics {};
};

// Partial pivoting. The pivot threshold is a diagnostic rejection policy, not
// a condition-number estimate. No clipping, fallback or silent state repair.
template<std::size_t N>
SolveResult<N> solve(Matrix<N> m, Vector<N> rhs)
{
    const auto original = m;
    const auto originalRhs = rhs;
    double scale = 0.0;
    for (const auto& row : m)
        for (double v : row) { finite(v); scale = std::max(scale, std::abs(v)); }
    for (double v : rhs) finite(v);
    if (scale == 0.0) throw std::runtime_error("IG00156: singular matrix");
    SolveResult<N> result;
    constexpr double threshold = 256.0 * std::numeric_limits<double>::epsilon();
    for (std::size_t col = 0; col < N; ++col)
    {
        std::size_t pivot = col;
        for (std::size_t row = col + 1; row < N; ++row)
            if (std::abs(m[row][col]) > std::abs(m[pivot][col])) pivot = row;
        const double relative = std::abs(m[pivot][col]) / scale;
        result.diagnostics.minimumRelativePivot =
            std::min(result.diagnostics.minimumRelativePivot, relative);
        if (!std::isfinite(relative) || relative <= threshold)
            throw std::runtime_error("IG00156: singular or ill-conditioned pivot");
        std::swap(m[pivot], m[col]);
        std::swap(rhs[pivot], rhs[col]);
        for (std::size_t row = col + 1; row < N; ++row)
        {
            const double factor = m[row][col] / m[col][col];
            for (std::size_t j = col + 1; j < N; ++j) m[row][j] -= factor * m[col][j];
            m[row][col] = 0.0;
            rhs[row] -= factor * rhs[col];
        }
    }
    for (std::size_t ii = N; ii > 0; --ii)
    {
        const std::size_t i = ii - 1;
        double v = rhs[i];
        for (std::size_t j = i + 1; j < N; ++j) v -= m[i][j] * result.value[j];
        result.value[i] = v / m[i][i];
        finite(result.value[i]);
    }
    double residual = 0.0, normA = 0.0, normX = 0.0, normB = 0.0;
    for (std::size_t i = 0; i < N; ++i)
    {
        double r = -originalRhs[i], rowNorm = 0.0;
        for (std::size_t j = 0; j < N; ++j)
        {
            r += original[i][j] * result.value[j];
            rowNorm += std::abs(original[i][j]);
        }
        residual = std::max(residual, std::abs(r));
        normA = std::max(normA, rowNorm);
        normX = std::max(normX, std::abs(result.value[i]));
        normB = std::max(normB, std::abs(originalRhs[i]));
    }
    const double denominator = normA * normX + normB;
    finite(denominator);
    result.diagnostics.relativeResidual = denominator == 0.0 ? residual : residual / denominator;
    if (!std::isfinite(result.diagnostics.relativeResidual)
        || result.diagnostics.relativeResidual > 1e-11)
        throw std::runtime_error("IG00156: unacceptable solve residual");
    return result;
}

template<std::size_t N> struct LinearSystem
{
    Matrix<N> a {};
    Vector<N> b {};
    Vector<N> derivative(const Vector<N>& state, double inputVolts) const
    {
        finite(inputVolts);
        for (double v : state) finite(v);
        Vector<N> out {};
        for (std::size_t i = 0; i < N; ++i)
        {
            out[i] = b[i] * inputVolts;
            for (std::size_t j = 0; j < N; ++j) out[i] += a[i][j] * state[j];
            finite(out[i]);
        }
        return out;
    }
    SolveResult<N> step(const Vector<N>& state, double previousInputVolts,
                        double nextInputVolts, double seconds) const
    {
        positive(seconds);
        finite(nextInputVolts);
        auto rhs = derivative(state, previousInputVolts);
        Matrix<N> lhs {};
        for (std::size_t i = 0; i < N; ++i)
        {
            rhs[i] = state[i] + (seconds * 0.5) * (rhs[i] + b[i] * nextInputVolts);
            for (std::size_t j = 0; j < N; ++j)
                lhs[i][j] = (i == j ? 1.0 : 0.0) - (seconds * 0.5) * a[i][j];
        }
        return solve(lhs, rhs);
    }
};
struct Outputs { double aiVolts, hpVolts, bpVolts, lpVolts; };
struct DynamicModel
{
    LinearSystem<3> system; // state [AI-BP, BP, LP], all volts.
    explicit DynamicModel(const PhysicalParameters& p)
    {
        validate(p, false);
        const double gs = 1.0/p.sourceOhms, gg = 1.0/p.groundOhms, gf = 1.0/p.feedbackOhms;
        const double w1 = (p.gm1Siemens + 1.0/p.feedforward1Ohms)/p.c1Farads;
        const double w2 = (p.gm2Siemens + 1.0/p.feedforward2Ohms)/p.c2Farads;
        system.a = {{{-(gs+gg+gf)/p.feedbackFarads, (p.qSiemens-gs-gg)/p.feedbackFarads, 0.0},
                     {-2*w1, -2*w1, w1}, {0.0, -w2, 0.0}}};
        system.b = {gs/p.feedbackFarads, 0.0, 0.0};
        for (const auto& row : system.a) for (double v : row) finite(v);
        for (double v : system.b) finite(v);
        positive(gs); positive(gg); positive(gf); positive(w1); positive(w2);
    }
    static Outputs outputs(const Vector<3>& z)
    {
        for (double v : z) finite(v);
        const double ai = z[0]+z[1], hp = 2*ai-z[2];
        finite(ai); finite(hp);
        return {ai, hp, z[1], z[2]};
    }
};
struct AlgebraicModel
{
    LinearSystem<2> system; // state [BP, LP]; x eliminated, never divided by Cfb.
    double inputGain, damping;
    explicit AlgebraicModel(const PhysicalParameters& p)
    {
        validate(p, true);
        const double gs = 1.0/p.sourceOhms, gg = 1.0/p.groundOhms, gf = 1.0/p.feedbackOhms;
        const double total = gs+gg+gf;
        const double w1 = (p.gm1Siemens+1.0/p.feedforward1Ohms)/p.c1Farads;
        const double w2 = (p.gm2Siemens+1.0/p.feedforward2Ohms)/p.c2Farads;
        positive(gs); positive(gg); positive(gf); positive(total); positive(w1); positive(w2);
        inputGain = 2*gs/total; damping = 2*(gf+p.qSiemens)/total;
        positive(inputGain); positive(damping);
        system.a = {{{-w1*damping, w1}, {-w2, 0.0}}};
        system.b = {-w1*inputGain, 0.0};
        for (const auto& row : system.a) for (double v : row) finite(v);
        for (double v : system.b) finite(v);
    }
    Outputs outputs(const Vector<2>& z, double inputVolts) const
    {
        finite(inputVolts); for (double v : z) finite(v);
        const double ai = 0.5*(inputGain*inputVolts+damping*z[0]);
        const double hp = 2*ai-z[1];
        finite(ai); finite(hp);
        return {ai, hp, z[0], z[1]};
    }
};
} // namespace cs80::reference::ig00156
