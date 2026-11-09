#pragma once

// Offline, loaded small-signal port model. Never use as an unloaded source or
// audio-rate approximation. See docs/analysis/memory-panel-ports.md.
#include "reference/MemoryPanelNetwork.h"
#include <algorithm>

namespace cs80::reference::memory_panel
{
struct Ports
{
    static constexpr size_t count = 24;
    Result operatingPoint;
    std::array<int, count> rows {}; // one-based physical output rows
    // Row-major d(bus voltage)/d(current injected INTO bus), in ohms.
    // Includes the existing shunt and receiver loads; generally nonsymmetric.
    std::array<double, count * count> loadedOhms {};
    bool valid = false;
};

inline Ports linearise(const Setup& setup)
{
    Ports out;
    Jacobian derivative;
    out.operatingPoint = solve(setup, &derivative);
    size_t rowIndex = 0;
    for (int row = 1; row <= 26; ++row)
        if (!mp::isSwitchRow(row)) out.rows[rowIndex++] = row;
    const int n = derivative.dimension, base = derivative.busBase;
    if (!out.operatingPoint.converged || n <= 0 || base < 0
        || base + static_cast<int>(Ports::count) != n
        || derivative.values.size() != static_cast<size_t>(n * n)) return out;
    auto a = derivative.values;
    for (double value : a) if (!std::isfinite(value)) return out;
    // Solve J dV = -dI for all 24 right-hand sides in one elimination.
    // Full Jacobian is intentional: no reciprocity or independent-row assumption.
    std::vector<double> b(static_cast<size_t>(n) * Ports::count, 0.0);
    for (size_t port = 0; port < Ports::count; ++port)
        b[static_cast<size_t>(base + static_cast<int>(port)) * Ports::count + port] = -1.0;
    for (int col = 0; col < n; ++col)
    {
        int pivot = col;
        for (int row = col + 1; row < n; ++row)
            if (std::abs(a[static_cast<size_t>(row * n + col)])
                > std::abs(a[static_cast<size_t>(pivot * n + col)])) pivot = row;
        for (int k = col; k < n; ++k)
            std::swap(a[static_cast<size_t>(col * n + k)], a[static_cast<size_t>(pivot * n + k)]);
        for (size_t k = 0; k < Ports::count; ++k)
            std::swap(b[static_cast<size_t>(col) * Ports::count + k], b[static_cast<size_t>(pivot) * Ports::count + k]);
        const double diag = a[static_cast<size_t>(col * n + col)];
        if (!std::isfinite(diag) || std::abs(diag) < 1e-300) return out;
        for (int row = col + 1; row < n; ++row)
        {
            const double factor = a[static_cast<size_t>(row * n + col)] / diag;
            for (int k = col; k < n; ++k)
                a[static_cast<size_t>(row * n + k)] -= factor * a[static_cast<size_t>(col * n + k)];
            for (size_t k = 0; k < Ports::count; ++k)
                b[static_cast<size_t>(row) * Ports::count + k] -= factor * b[static_cast<size_t>(col) * Ports::count + k];
        }
    }
    for (int row = n - 1; row >= 0; --row)
        for (size_t port = 0; port < Ports::count; ++port)
        {
            double value = b[static_cast<size_t>(row) * Ports::count + port];
            for (int k = row + 1; k < n; ++k)
                value -= a[static_cast<size_t>(row * n + k)] * b[static_cast<size_t>(k) * Ports::count + port];
            value /= a[static_cast<size_t>(row * n + row)];
            if (!std::isfinite(value)) return out;
            b[static_cast<size_t>(row) * Ports::count + port] = value;
        }
    for (size_t row = 0; row < Ports::count; ++row)
        for (size_t col = 0; col < Ports::count; ++col)
            out.loadedOhms[row * Ports::count + col] = b[(static_cast<size_t>(base) + row) * Ports::count + col];
    out.valid = true;
    return out;
}
} // namespace cs80::reference::memory_panel
