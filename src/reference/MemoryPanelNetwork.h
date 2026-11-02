#pragma once

// Offline truth model of one rank's programmer source network (ws6-library,
// 2026-10-31): all eleven factory columns (selected emitter solved, the others
// at 0 V), both memories and PANEL (rails, every pot, wipers, followers on
// rows 7/9, isolation diodes) and the 24 assembled analog row buses with their
// one 10 k shunt and compiled receiver loads. Double precision, Newton with an
// analytic Jacobian and exact per-branch diode solves. Evidence and declared
// inferences: src/core/MemoryPanels.h. Not a hardware measurement.

#include "core/MemoryPanels.h"

#include <array>
#include <cmath>
#include <vector>

namespace cs80::reference::memory_panel
{
namespace mp = cs80::memory_panels;
namespace sf = cs80::source_factory;

struct Setup
{
    int rank = 0;
    int selector = 13;
    // [0] the rank's first memory, [1] its second memory, [2] PANEL.
    std::array<mp::Positions, 3> banks {};
    mp::Electrical electrical = mp::nominalElectrical;
    // Independent differential-source fixture; positive current enters a bus.
    std::array<double, 26> injectedAmps {};
};

struct Result
{
    std::array<double, 26> busVolts {};
    std::array<double, 3> railVolts {};
    double emitterVolts = 0.0;      // selected factory column (0 if none)
    double worstInactiveAmps = 0.0; // largest |current| of any unselected source branch
    double residualAmps = 0.0;
    int iterations = 0;
    bool converged = false;
};

// Exact current through a series resistance and the Shockley diode for a given
// voltage across both (from - to), with its derivative.
struct BranchCurrent { double amps = 0.0, siemens = 0.0; };
inline BranchCurrent diodeBranch(double across, double seriesOhms, const sf::DiodeModel& d) noexcept
{
    const double is = d.saturationAmps, nvt = d.emission * d.thermalVolts, rs = seriesOhms + d.seriesOhms;
    // g(i) = i*rs + nvt*log1p(i/is) - across, increasing in i on (-is, inf).
    double lo = -is * (1.0 - 1e-15), hi = std::max(is, (std::abs(across) + 1.0) / std::max(rs, 1e-3));
    double i = across > 0.0 && rs > 0.0 ? std::min(across / rs, hi) * 0.5 : 0.0;
    for (int k = 0; k < 300; ++k)
    {
        const double g = i * rs + nvt * std::log1p(i / is) - across;
        if (g > 0.0) hi = i; else lo = i;
        const double slope = rs + nvt / (i + is);
        double step = i - g / slope;
        if (!(step > lo && step < hi)) step = 0.5 * (lo + hi);
        if (std::abs(step - i) <= 1e-21 + 1e-15 * std::abs(i)) { i = step; break; }
        i = step;
    }
    return { i, 1.0 / (rs + nvt / (i + is)) };
}

inline Result solve(const Setup& setup)
{
    Result out;
    const auto& e = setup.electrical;
    const auto& d = e.boundary.diode;
    const double e0 = e.boundary.driver.openCircuitVolts, rout = e.boundary.driver.outputOhms;
    const auto selected = mp::sourceFor(setup.selector);
    const int activeBank = selected == mp::Source::memoryA ? 0 : selected == mp::Source::memoryB ? 1
                         : selected == mp::Source::panel ? 2 : -1;

    // Analog rows and the unknown layout.
    std::vector<int> rows;
    for (int output = 1; output <= 26; ++output)
        if (!mp::isSwitchRow(output)) rows.push_back(output);
    const int nRows = (int) rows.size();
    std::vector<int> unbuffered;
    for (int r = 0; r < nRows; ++r)
        if (!mp::isBufferedRow(rows[(size_t) r])) unbuffered.push_back(r);
    const int nWipers = (int) unbuffered.size();
    const int railBase = 0, wiperBase = 3, emitterIndex = wiperBase + 3 * nWipers, busBase = emitterIndex + 1;
    const int n = busBase + nRows;

    // Factory column branches (open = absent; unknown = manifest nominal).
    std::array<std::array<double, 26>, 11> columnSeries {};
    std::array<std::array<bool, 26>, 11> columnConducts {};
    for (int column = 0; column < 11; ++column)
    {
        sf::ToneControls tone;
        sf::compileTone(setup.rank, column, tone);
        for (int r = 0; r < nRows; ++r)
        {
            const int output = rows[(size_t) r];
            const auto i = (size_t) (output - 1);
            const auto b = tone.hypotheses[i] != nullptr ? sf::hypothesisBranch(tone.hypothesisExcitations[i])
                                                         : hardware::findBranch(tone.source.board, tone.source.column, output);
            columnConducts[(size_t) column][(size_t) r] = b.kind != hardware::BranchKind::open;
            columnSeries[(size_t) column][(size_t) r] = b.kind == hardware::BranchKind::resistor ? b.resistanceOhms() : 0.0;
        }
    }
    const auto potTop = [&](int bank, int r) { return e.potOhms * (1.0 - mp::clampedFraction(setup.banks[(size_t) bank][(size_t) (rows[(size_t) r] - 1)])) + e.endOhms; };
    const auto potBottom = [&](int bank, int r) { return e.potOhms * mp::clampedFraction(setup.banks[(size_t) bank][(size_t) (rows[(size_t) r] - 1)]) + e.endOhms; };
    const auto load = [&](int r) { return 1.0 / sf::assembledShuntOhms + sf::compiledReceiverSiemens(rows[(size_t) r], sf::Grade::B, sf::Grade::B); };

    std::vector<double> x((size_t) n, 0.0), f((size_t) n), jac((size_t) (n * n));
    // Start: active rail/emitter near the driver, wipers at their dividers.
    for (int s = 0; s < 3; ++s) x[(size_t) (railBase + s)] = s == activeBank ? e0 : 0.0;
    x[(size_t) emitterIndex] = activeBank < 0 ? e0 : 0.0;
    for (int s = 0; s < 3; ++s)
        for (int w = 0; w < nWipers; ++w)
        {
            const int r = unbuffered[(size_t) w];
            x[(size_t) (wiperBase + s * nWipers + w)] = x[(size_t) (railBase + s)] * potBottom(s, r) / (potTop(s, r) + potBottom(s, r));
        }
    for (int r = 0; r < nRows; ++r) x[(size_t) (busBase + r)] = 1.0;

    double worstInactive = 0.0;
    const auto evaluate = [&](bool jacobian)
    {
        std::fill(f.begin(), f.end(), 0.0);
        if (jacobian) std::fill(jac.begin(), jac.end(), 0.0);
        worstInactive = 0.0;
        const auto J = [&](int row, int col, double v) { if (jacobian) jac[(size_t) (row * n + col)] += v; };
        // A branch from node a (or a fixed voltage when a < 0) into bus r.
        const auto branch = [&](int a, double fixedVolts, int r, double series, bool active)
        {
            const int bi = busBase + r;
            const double va = a >= 0 ? x[(size_t) a] : fixedVolts;
            const auto c = diodeBranch(va - x[(size_t) bi], series, d);
            if (!active) worstInactive = std::max(worstInactive, std::abs(c.amps));
            f[(size_t) bi] += c.amps;
            J(bi, bi, -c.siemens);
            if (a >= 0) { f[(size_t) a] -= c.amps; J(a, a, -c.siemens); J(a, bi, c.siemens); J(bi, a, c.siemens); }
            return c;
        };
        // Rails and pots.
        for (int s = 0; s < 3; ++s)
        {
            const int ri = railBase + s;
            const double es = s == activeBank ? e0 : 0.0;
            if (rout > 0.0) { f[(size_t) ri] += (es - x[(size_t) ri]) / rout; J(ri, ri, -1.0 / rout); }
            else { f[(size_t) ri] += 1e3 * (es - x[(size_t) ri]); J(ri, ri, -1e3); }
            for (int r = 0; r < nRows; ++r)
            {
                const double top = potTop(s, r), bottom = potBottom(s, r);
                if (mp::isBufferedRow(rows[(size_t) r]))
                {
                    f[(size_t) ri] -= x[(size_t) ri] / (top + bottom);
                    J(ri, ri, -1.0 / (top + bottom));
                    // Follower: output = wiper divider voltage, source of the branch.
                    const double ratio = bottom / (top + bottom);
                    const int bi = busBase + r;
                    const auto c = diodeBranch(ratio * x[(size_t) ri] - x[(size_t) bi], e.followerOhms, d);
                    if (s != activeBank) worstInactive = std::max(worstInactive, std::abs(c.amps));
                    f[(size_t) bi] += c.amps;
                    J(bi, bi, -c.siemens);
                    J(bi, ri, c.siemens * ratio); // follower draws from its supply, not the pot
                }
            }
            for (int w = 0; w < nWipers; ++w)
            {
                const int r = unbuffered[(size_t) w], wi = wiperBase + s * nWipers + w;
                const double top = potTop(s, r), bottom = potBottom(s, r);
                const double topAmps = (x[(size_t) ri] - x[(size_t) wi]) / top;
                f[(size_t) ri] -= topAmps; J(ri, ri, -1.0 / top); J(ri, wi, 1.0 / top);
                f[(size_t) wi] += topAmps - x[(size_t) wi] / bottom;
                J(wi, ri, 1.0 / top); J(wi, wi, -1.0 / top - 1.0 / bottom);
                branch(wi, 0.0, r, 0.0, s == activeBank);
            }
        }
        // Factory columns: the selected one from the solved emitter, the rest from 0 V.
        if (activeBank < 0)
        {
            if (rout > 0.0) { f[(size_t) emitterIndex] += (e0 - x[(size_t) emitterIndex]) / rout; J(emitterIndex, emitterIndex, -1.0 / rout); }
            else { f[(size_t) emitterIndex] += 1e3 * (e0 - x[(size_t) emitterIndex]); J(emitterIndex, emitterIndex, -1e3); }
        }
        else
        {
            f[(size_t) emitterIndex] += 1e3 * (0.0 - x[(size_t) emitterIndex]);
            J(emitterIndex, emitterIndex, -1e3);
        }
        for (int column = 0; column < 11; ++column)
            for (int r = 0; r < nRows; ++r)
            {
                if (!columnConducts[(size_t) column][(size_t) r]) continue;
                const bool active = activeBank < 0 && column == setup.selector;
                const double series = columnSeries[(size_t) column][(size_t) r] + (active ? 0.0 : rout);
                branch(active ? emitterIndex : -1, 0.0, r, series, active);
            }
        // Bus shunt and receivers.
        for (int r = 0; r < nRows; ++r)
        {
            const int bi = busBase + r;
            f[(size_t) bi] -= x[(size_t) bi] * load(r);
            f[(size_t) bi] += setup.injectedAmps[(size_t) (rows[(size_t) r] - 1)];
            J(bi, bi, -load(r));
        }
    };

    for (int iteration = 0; iteration < 100; ++iteration)
    {
        evaluate(true);
        double worst = 0.0;
        for (double v : f) worst = std::max(worst, std::abs(v));
        out.iterations = iteration;
        if (worst <= 1e-14) break;
        // Solve J dx = -f by Gaussian elimination with partial pivoting.
        std::vector<double> a = jac, b(f.size());
        for (int i = 0; i < n; ++i) b[(size_t) i] = -f[(size_t) i];
        for (int col = 0; col < n; ++col)
        {
            int pivot = col;
            for (int row = col + 1; row < n; ++row)
                if (std::abs(a[(size_t) (row * n + col)]) > std::abs(a[(size_t) (pivot * n + col)])) pivot = row;
            if (pivot != col)
            {
                for (int k = 0; k < n; ++k) std::swap(a[(size_t) (col * n + k)], a[(size_t) (pivot * n + k)]);
                std::swap(b[(size_t) col], b[(size_t) pivot]);
            }
            const double diag = a[(size_t) (col * n + col)];
            if (std::abs(diag) < 1e-300) return out;
            for (int row = col + 1; row < n; ++row)
            {
                const double factor = a[(size_t) (row * n + col)] / diag;
                if (factor == 0.0) continue;
                for (int k = col; k < n; ++k) a[(size_t) (row * n + k)] -= factor * a[(size_t) (col * n + k)];
                b[(size_t) row] -= factor * b[(size_t) col];
            }
        }
        std::vector<double> dx((size_t) n);
        for (int row = n - 1; row >= 0; --row)
        {
            double s = b[(size_t) row];
            for (int k = row + 1; k < n; ++k) s -= a[(size_t) (row * n + k)] * dx[(size_t) k];
            dx[(size_t) row] = s / a[(size_t) (row * n + row)];
        }
        // Damped step: no node moves more than 0.5 V per iteration.
        double largest = 0.0;
        for (double v : dx) largest = std::max(largest, std::abs(v));
        const double scale = largest > 0.5 ? 0.5 / largest : 1.0;
        for (int i = 0; i < n; ++i) x[(size_t) i] += scale * dx[(size_t) i];
    }
    evaluate(false);
    double worst = 0.0;
    for (double v : f) worst = std::max(worst, std::abs(v));
    out.residualAmps = worst;
    out.worstInactiveAmps = worstInactive;
    for (int r = 0; r < nRows; ++r) out.busVolts[(size_t) (rows[(size_t) r] - 1)] = x[(size_t) (busBase + r)];
    for (int s = 0; s < 3; ++s) out.railVolts[(size_t) s] = x[(size_t) (railBase + s)];
    out.emitterVolts = activeBank < 0 ? x[(size_t) emitterIndex] : 0.0;
    out.converged = std::isfinite(worst) && worst <= 1e-12;
    return out;
}
} // namespace cs80::reference::memory_panel
