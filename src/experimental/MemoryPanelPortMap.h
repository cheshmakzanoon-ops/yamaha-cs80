#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace cs80::experimental
{
// Fixed-size, loaded small-signal map, NOT an unloaded Thevenin source.
// Preparation belongs outside the audio callback. No shipping Engine route
// consumes this experiment. Its validity does not certify nonlinear excursions.
struct MemoryPanelPortMap
{
    static constexpr std::size_t outputCount = 26, railCount = 3, analogCount = 24;
    using Currents = std::array<double, outputCount>;
    using Voltages = std::array<double, outputCount>;
    static constexpr bool digital(std::size_t i) noexcept { return i == 3 || i == 4; }

    int rank = -1, selector = -1;
    Voltages operatingPoint {};
    Currents referenceCurrents {};
    std::array<double, outputCount> localOhms {};
    std::array<std::array<double, outputCount>, railCount> railToBus {}, currentToRail {};
    bool ready = false;

    // deltaCurrent is relative to referenceCurrents, in amps INTO each physical
    // output row (index = row - 1). Digital outputs are not electrical ports.
    // Failure leaves the caller's prior output unchanged. No allocation or solve.
    bool evaluate(const Currents& deltaCurrent, Voltages& result) const noexcept
    {
        if (!ready) return false;
        bool changed = false;
        for (std::size_t i = 0; i < outputCount; ++i)
        {
            if (!std::isfinite(deltaCurrent[i]) || (digital(i) && deltaCurrent[i] != 0.0)) return false;
            changed |= deltaCurrent[i] != 0.0;
        }
        if (!changed) { result = operatingPoint; return true; }
        std::array<double, railCount> rail {};
        for (std::size_t k = 0; k < railCount; ++k)
            for (std::size_t i = 0; i < outputCount; ++i)
                rail[k] += currentToRail[k][i] * deltaCurrent[i];
        Voltages next {};
        for (std::size_t i = 0; i < outputCount; ++i)
        {
            next[i] = operatingPoint[i] + localOhms[i] * deltaCurrent[i];
            for (std::size_t k = 0; k < railCount; ++k) next[i] += railToBus[k][i] * rail[k];
            if (!std::isfinite(next[i])) return false;
        }
        result = next;
        return true;
    }

    // A derivative, in loaded ohms. Physical rows are one-based, as in #103.
    double derivative(int outputRow, int inputRow) const noexcept
    {
        if (!ready || outputRow < 1 || outputRow > static_cast<int>(outputCount)
            || inputRow < 1 || inputRow > static_cast<int>(outputCount)) return 0.0;
        const auto i = static_cast<std::size_t>(outputRow - 1), j = static_cast<std::size_t>(inputRow - 1);
        double value = i == j ? localOhms[i] : 0.0;
        for (std::size_t k = 0; k < railCount; ++k) value += railToBus[k][i] * currentToRail[k][j];
        return value;
    }
};
static_assert(sizeof(MemoryPanelPortMap) <= 2048, "Prepared memory map must stay fixed and small");
} // namespace cs80::experimental
