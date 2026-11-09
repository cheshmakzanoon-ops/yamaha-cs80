#pragma once

#include "reference/MemoryPanelNetwork.h"
#include "experimental/MemoryPanelPortMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace cs80::reference::memory_panel
{
// OFFLINE compiler: the nonlinear reference and its dense Jacobian allocate.
// The prepared map alone has a bounded allocation-free evaluation path.
inline bool compileLoadedPortMap(const Setup& setup, experimental::MemoryPanelPortMap& result)
{
    using Map = experimental::MemoryPanelPortMap;
    if (setup.rank < 0 || setup.rank > 1 || setup.selector < 11 || setup.selector > 13) return false;
    const auto& e = setup.electrical;
    if (!sf::valid(e.boundary) || !std::isfinite(e.potOhms) || e.potOhms <= 0.0
        || !std::isfinite(e.endOhms) || e.endOhms <= 0.0
        || !std::isfinite(e.followerOhms) || e.followerOhms < 0.0) return false;
    for (const auto& bank : setup.banks)
        for (double x : bank) if (!std::isfinite(x) || x < 0.0 || x > 1.0) return false;
    for (std::size_t i = 0; i < setup.injectedAmps.size(); ++i)
        if (!std::isfinite(setup.injectedAmps[i])
            || (Map::digital(i) && setup.injectedAmps[i] != 0.0)) return false;

    Jacobian jac;
    const auto point = solve(setup, &jac);
    // Existing reference layout: three rails, 22 wipers per bank, one inactive
    // emitter coordinate, then 24 buses. Reject a changed graph rather than
    // silently treating a new internal coupling as one of these three rails.
    constexpr int rails = 3, buses = 24, expectedBusBase = rails + rails * 22 + 1;
    const int n = jac.dimension, base = jac.busBase;
    if (!point.converged || base != expectedBusBase || n != base + buses
        || jac.values.size() != static_cast<std::size_t>(n * n)) return false;
    auto k = jac.values;
    for (double& v : k) { if (!std::isfinite(v)) return false; v = -v; }
    const auto at = [&](int row, int col) -> double& { return k[static_cast<std::size_t>(row * n + col)]; };

    // Schur-eliminate the wiper/emitter coordinates. Their positive diagonals
    // come from passive local connections; retaining the rail/bus rows preserves
    // inactive-diode loading and the followers' nonsymmetric KCL derivatives.
    for (int pivot = rails; pivot < base; ++pivot)
    {
        const double d = at(pivot, pivot);
        if (!std::isfinite(d) || !(d > 0.0)) return false;
        for (int row = 0; row < n; ++row)
        {
            if (row == pivot || at(row, pivot) == 0.0) continue;
            const double factor = at(row, pivot) / d;
            for (int col = 0; col < n; ++col)
                if (col != pivot) at(row, col) -= factor * at(pivot, col);
            at(row, pivot) = 0.0;
        }
    }
    std::array<int, buses> rows {};
    for (int output = 1, i = 0; output <= 26; ++output)
        if (!mp::isSwitchRow(output)) rows[static_cast<std::size_t>(i++)] = output;
    Map next;
    next.rank = setup.rank; next.selector = setup.selector;
    next.operatingPoint = point.busVolts;
    next.referenceCurrents = setup.injectedAmps;
    std::array<double, buses> inverseD {};
    for (int row = 0; row < buses; ++row)
    {
        // Each eliminated wiper connects only its own output. A future graph
        // with direct bus-to-bus coupling needs a different factorization.
        for (int col = 0; col < buses; ++col)
            if (row != col && at(base + row, base + col) != 0.0) return false;
        const double d = at(base + row, base + row);
        if (!std::isfinite(d) || !(d > 0.0)) return false;
        const auto output = static_cast<std::size_t>(rows[static_cast<std::size_t>(row)] - 1);
        inverseD[static_cast<std::size_t>(row)] = next.localOhms[output] = 1.0 / d;
        for (int rail = 0; rail < rails; ++rail)
            next.railToBus[static_cast<std::size_t>(rail)][output] = -at(base + row, rail) / d;
    }

    // [A B; C D] [rail;bus] = [0;injection]. Solve
    // (A - B D^-1 C) rail = -B D^-1 injection, for all 24 RHS together.
    std::array<std::array<double, rails + buses>, rails> augmented {};
    for (int row = 0; row < rails; ++row)
    {
        for (int col = 0; col < rails; ++col)
        {
            double value = at(row, col);
            for (int bus = 0; bus < buses; ++bus)
                value -= at(row, base + bus) * inverseD[static_cast<std::size_t>(bus)] * at(base + bus, col);
            augmented[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = value;
        }
        for (int bus = 0; bus < buses; ++bus)
            augmented[static_cast<std::size_t>(row)][static_cast<std::size_t>(rails + bus)]
                = -at(row, base + bus) * inverseD[static_cast<std::size_t>(bus)];
    }
    for (int col = 0; col < rails; ++col)
    {
        int pivot = col;
        for (int row = col + 1; row < rails; ++row)
            if (std::abs(augmented[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)])
                > std::abs(augmented[static_cast<std::size_t>(pivot)][static_cast<std::size_t>(col)])) pivot = row;
        std::swap(augmented[static_cast<std::size_t>(col)], augmented[static_cast<std::size_t>(pivot)]);
        const double d = augmented[static_cast<std::size_t>(col)][static_cast<std::size_t>(col)];
        if (!std::isfinite(d) || std::abs(d) < 1e-300) return false;
        for (double& v : augmented[static_cast<std::size_t>(col)]) v /= d;
        for (int row = 0; row < rails; ++row)
        {
            if (row == col) continue;
            const double factor = augmented[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            for (int i = 0; i < rails + buses; ++i)
                augmented[static_cast<std::size_t>(row)][static_cast<std::size_t>(i)]
                    -= factor * augmented[static_cast<std::size_t>(col)][static_cast<std::size_t>(i)];
        }
    }
    for (int rail = 0; rail < rails; ++rail)
        for (int bus = 0; bus < buses; ++bus)
        {
            const auto output = static_cast<std::size_t>(rows[static_cast<std::size_t>(bus)] - 1);
            const double value = augmented[static_cast<std::size_t>(rail)][static_cast<std::size_t>(rails + bus)];
            if (!std::isfinite(value) || !std::isfinite(next.railToBus[static_cast<std::size_t>(rail)][output])) return false;
            next.currentToRail[static_cast<std::size_t>(rail)][output] = value;
        }
    for (double v : next.operatingPoint) if (!std::isfinite(v)) return false;
    next.ready = true;
    result = next;
    return true;
}
} // namespace cs80::reference::memory_panel
