#pragma once

#include "experimental/MemorySparseTypes.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cs80::experimental::memory_sparse::detail
{
struct DiodeCandidate { double amps = 0.0, siemens = 0.0; bool valid = false; };
// Bounded candidate generation, NOT certification. The independent enclosure
// verifier must reject a bad libm result or a stalled iteration before publish.
inline DiodeCandidate diodeCandidate(double across, double resistance, double is, double nvt) noexcept
{
    if (!std::isfinite(across) || !(resistance > 0.0) || !(is > 0.0) || !(nvt > 0.0)) return {};
    const double ri = resistance * is;
    double lo = across < 0.0 ? across / nvt : 0.0;
    double hi = across < 0.0 ? 0.0 : std::min(across / nvt, std::log1p(across / ri));
    if (!std::isfinite(lo) || !std::isfinite(hi)) return {};
    double y = across < 0.0 ? lo : hi;
    for (int iteration = 0; iteration < 48; ++iteration)
    {
        const double exponential = std::exp(y);
        const double h = ri * std::expm1(y) + nvt * y - across;
        if (!std::isfinite(exponential) || !std::isfinite(h)) return {};
        if (h > 0.0) hi = y; else lo = y;
        double next = y - h / (ri * exponential + nvt);
        if (std::abs(next - y) <= 4.0 * std::numeric_limits<double>::epsilon() * std::max(1.0, std::abs(y)))
        {
            const double amps = is * std::expm1(next), conductance = is * std::exp(next);
            const double g = conductance / (resistance * conductance + nvt);
            return {amps, g, std::isfinite(amps) && std::isfinite(g) && g >= 0.0};
        }
        if (!(next > lo && next < hi)) next = 0.5 * (lo + hi);
        y = next;
    }
    return {};
}
inline bool inputsValid(const Inputs& input) noexcept
{
    for (std::size_t i = 0; i < input.constantAmps.size(); ++i)
        if (!std::isfinite(input.constantAmps[i]) || ((i == 3 || i == 4) && input.constantAmps[i] != 0.0)) return false;
    for (double g : input.feedbackG) if (!std::isfinite(g) || g < 0.0) return false;
    return true;
}
inline bool preparedValid(const Prepared& p) noexcept
{
    if (!p.ready || p.activeBank < 0 || p.activeBank >= railCount || p.key[0] != modelVersion) return false;
    for (double v : {p.driverG, p.selectedVolts, p.diodeIs, p.diodeNvt, p.diodeR, p.followerR})
        if (!std::isfinite(v) || v <= 0.0) return false;
    for (int s = 0; s < railCount; ++s) for (int r = 0; r < busCount; ++r)
    {
        const auto bank = static_cast<std::size_t>(s), row = static_cast<std::size_t>(r);
        for (double v : {p.topG[bank][row], p.bottomG[bank][row], p.dividerG[bank][row]})
            if (!std::isfinite(v) || v <= 0.0) return false;
        const double ratio = p.followerRatio[bank][row];
        if (!std::isfinite(ratio) || ratio < 0.0 || ratio > 1.0) return false;
    }
    for (const auto& bank : p.factoryR) for (double r : bank) if (!std::isfinite(r) || r <= 0.0) return false;
    for (double g : p.load) if (!std::isfinite(g) || g <= 0.0) return false;
    return true;
}
inline double approximateMargin(const Prepared& p, const Inputs& input) noexcept
{
    double m = std::min(p.driverG, 1000.0);
    for (int s = 0; s < railCount; ++s) for (int r : wiperBuses)
        m = std::min(m, p.bottomG[static_cast<std::size_t>(s)][static_cast<std::size_t>(r)]);
    auto load = p.load;
    for (std::size_t i = 0; i < input.feedbackG.size(); ++i) load[static_cast<std::size_t>(feedbackTargets[i])] -= input.feedbackG[i];
    for (double g : load) m = std::min(m, g);
    return m;
}
struct Linearization
{
    Nodes residual {};
    std::array<double, railCount> railD {};
    std::array<double, busCount> busD {};
    std::array<std::array<double, wipersPerBank>, railCount> wiperD {}, wiperDiodeG {};
    std::array<std::array<double, railCount>, busCount> busRail {};
};
inline bool linearize(const Prepared& p, const Inputs& input, const Nodes& x, Linearization& destination) noexcept
{
    Linearization a;
    for (int s = 0; s < railCount; ++s)
    {
        const auto bank = static_cast<std::size_t>(s);
        a.residual[bank] = p.driverG * ((s == p.activeBank ? p.selectedVolts : 0.0) - x[bank]);
        a.railD[bank] = p.driverG;
        for (int r : {4, 6})
        {
            const auto row = static_cast<std::size_t>(r), bi = static_cast<std::size_t>(busBase + r);
            a.residual[bank] -= p.dividerG[bank][row] * x[bank]; a.railD[bank] += p.dividerG[bank][row];
            const auto d = diodeCandidate(p.followerRatio[bank][row] * x[bank] - x[bi], p.followerR, p.diodeIs, p.diodeNvt);
            if (!d.valid) return false;
            a.residual[bi] += d.amps; a.busD[row] += d.siemens;
            a.busRail[row][bank] -= d.siemens * p.followerRatio[bank][row];
        }
        for (int w = 0; w < wipersPerBank; ++w)
        {
            const auto wi = static_cast<std::size_t>(wiperNode(s, w)), col = static_cast<std::size_t>(w);
            const auto row = static_cast<std::size_t>(wiperBuses[col]), bi = static_cast<std::size_t>(busBase + wiperBuses[col]);
            const double gt = p.topG[bank][row], gb = p.bottomG[bank][row];
            const double current = gt * (x[bank] - x[wi]);
            a.residual[bank] -= current; a.residual[wi] += current - gb * x[wi];
            a.railD[bank] += gt;
            const auto d = diodeCandidate(x[wi] - x[bi], p.diodeR, p.diodeIs, p.diodeNvt);
            if (!d.valid) return false;
            a.residual[wi] -= d.amps; a.residual[bi] += d.amps;
            a.wiperD[bank][col] = gt + gb + d.siemens;
            a.wiperDiodeG[bank][col] = d.siemens; a.busD[row] += d.siemens;
        }
    }
    a.residual[emitterNode] = -1000.0 * x[emitterNode];
    for (int r = 0; r < busCount; ++r)
    {
        const auto row = static_cast<std::size_t>(r), bi = static_cast<std::size_t>(busBase + r);
        for (std::size_t column = 0; column < p.factoryR.size(); ++column)
        {
            if (!p.factoryPresent[column][row]) continue;
            const auto d = diodeCandidate(-x[bi], p.factoryR[column][row], p.diodeIs, p.diodeNvt);
            if (!d.valid) return false;
            a.residual[bi] += d.amps; a.busD[row] += d.siemens;
        }
        a.residual[bi] += input.constantAmps[static_cast<std::size_t>(outputs[row])] - p.load[row] * x[bi];
        a.busD[row] += p.load[row];
    }
    for (std::size_t i = 0; i < input.feedbackG.size(); ++i)
        a.residual[static_cast<std::size_t>(busBase + feedbackTargets[i])] += input.feedbackG[i]
            * x[static_cast<std::size_t>(busBase + feedbackSources[i])];
    for (double v : a.residual) if (!std::isfinite(v)) return false;
    destination = a; return true;
}
inline bool solveNewton(const Prepared& p, const Inputs& input, const Linearization& a, Nodes& destination) noexcept
{
    auto diagonal = a.busD; auto c = a.busRail;
    std::array<std::array<double, busCount>, railCount> b {};
    auto railD = a.railD;
    std::array<double, railCount> railRhs {};
    std::array<double, busCount> busRhs {};
    for (int s = 0; s < railCount; ++s) railRhs[static_cast<std::size_t>(s)] = a.residual[static_cast<std::size_t>(s)];
    for (int r = 0; r < busCount; ++r) busRhs[static_cast<std::size_t>(r)] = a.residual[static_cast<std::size_t>(busBase + r)];
    for (int s = 0; s < railCount; ++s) for (int w = 0; w < wipersPerBank; ++w)
    {
        const auto bank = static_cast<std::size_t>(s), col = static_cast<std::size_t>(w), row = static_cast<std::size_t>(wiperBuses[col]);
        const double d = a.wiperD[bank][col], gt = p.topG[bank][row], gd = a.wiperDiodeG[bank][col];
        if (!std::isfinite(d) || !(d > 0.0)) return false;
        railD[bank] -= gt * (gt / d); b[bank][row] -= gt * (gd / d);
        c[row][bank] -= gd * (gt / d); diagonal[row] -= gd * (gd / d);
        railRhs[bank] += (gt / d) * a.residual[static_cast<std::size_t>(wiperNode(s, w))];
        busRhs[row] += (gd / d) * a.residual[static_cast<std::size_t>(wiperNode(s, w))];
    }
    for (double d : diagonal) if (!std::isfinite(d) || !(d > 0.0)) return false;
    // D = diagonal - L. Because L connects disjoint source/target sets,
    // D^-1 rhs = diagonal^-1 rhs + diagonal^-1 L diagonal^-1 rhs exactly.
    const auto inverseBus = [&](const std::array<double, busCount>& rhs)
    {
        std::array<double, busCount> result {};
        for (std::size_t r = 0; r < result.size(); ++r) result[r] = rhs[r] / diagonal[r];
        for (std::size_t i = 0; i < input.feedbackG.size(); ++i)
        {
            const auto target = static_cast<std::size_t>(feedbackTargets[i]), source = static_cast<std::size_t>(feedbackSources[i]);
            result[target] += (input.feedbackG[i] / diagonal[target]) * (rhs[source] / diagonal[source]);
        }
        return result;
    };
    const auto inverseRhs = inverseBus(busRhs);
    std::array<std::array<double, busCount>, railCount> inverseC {};
    for (int s = 0; s < railCount; ++s)
    {
        std::array<double, busCount> rhs {};
        for (int r = 0; r < busCount; ++r) rhs[static_cast<std::size_t>(r)] = c[static_cast<std::size_t>(r)][static_cast<std::size_t>(s)];
        inverseC[static_cast<std::size_t>(s)] = inverseBus(rhs);
    }
    std::array<std::array<double, 4>, 3> system {};
    for (std::size_t row = 0; row < 3; ++row)
    {
        system[row][row] = railD[row]; system[row][3] = railRhs[row];
        for (std::size_t bus = 0; bus < busCount; ++bus)
        {
            for (std::size_t col = 0; col < 3; ++col) system[row][col] -= b[row][bus] * inverseC[col][bus];
            system[row][3] -= b[row][bus] * inverseRhs[bus];
        }
    }
    for (std::size_t col = 0; col < 3; ++col)
    {
        std::size_t pivot = col;
        for (std::size_t row = col + 1; row < 3; ++row) if (std::abs(system[row][col]) > std::abs(system[pivot][col])) pivot = row;
        std::swap(system[col], system[pivot]); const double d = system[col][col];
        if (!std::isfinite(d) || std::abs(d) < 1e-300) return false;
        for (double& v : system[col]) v /= d;
        for (std::size_t row = 0; row < 3; ++row) if (row != col)
        {
            const double factor = system[row][col];
            for (std::size_t j = 0; j < 4; ++j) system[row][j] -= factor * system[col][j];
        }
    }
    Nodes dx {};
    for (std::size_t s = 0; s < 3; ++s) dx[s] = system[s][3];
    for (std::size_t r = 0; r < busCount; ++r)
    {
        dx[static_cast<std::size_t>(busBase) + r] = inverseRhs[r];
        for (std::size_t s = 0; s < 3; ++s) dx[static_cast<std::size_t>(busBase) + r] -= inverseC[s][r] * dx[s];
    }
    for (int s = 0; s < railCount; ++s) for (int w = 0; w < wipersPerBank; ++w)
    {
        const auto bank = static_cast<std::size_t>(s), col = static_cast<std::size_t>(w), wi = static_cast<std::size_t>(wiperNode(s, w));
        const auto row = static_cast<std::size_t>(wiperBuses[col]);
        dx[wi] = (a.residual[wi] + p.topG[bank][row] * dx[bank]
                  + a.wiperDiodeG[bank][col] * dx[static_cast<std::size_t>(busBase) + row]) / a.wiperD[bank][col];
    }
    dx[emitterNode] = a.residual[emitterNode] / 1000.0;
    for (double v : dx) if (!std::isfinite(v)) return false;
    destination = dx; return true;
}
} // namespace cs80::experimental::memory_sparse::detail
