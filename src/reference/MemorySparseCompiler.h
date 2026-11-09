#pragma once

#include "experimental/MemorySparseTypes.h"
#include "reference/MemoryReceiverNetwork.h"

#include <bit>
#include <cmath>

namespace cs80::reference::memory_panel
{
// Compile only fixed electrical coefficients. No nonlinear source solve is
// hidden in preparation. A caller generation is included in the complete key,
// but identity also compares every numerical source parameter, not just a hash.
inline bool compileSparse(const Setup& source, std::uint64_t generation,
                          experimental::memory_sparse::Prepared& destination)
{
    namespace sp = experimental::memory_sparse;
    const auto& e = source.electrical;
    if (source.rank < 0 || source.rank > 1 || source.selector < 11 || source.selector > 13
        || !sf::valid(e.boundary) || !std::isfinite(e.potOhms) || e.potOhms <= 0.0
        || !std::isfinite(e.endOhms) || e.endOhms <= 0.0
        || !std::isfinite(e.followerOhms) || e.followerOhms < 0.0
        || e.boundary.diode.seriesOhms <= 0.0) return false;
    sp::Prepared p;
    std::size_t key = 0;
    p.key[key++] = sp::modelVersion; p.key[key++] = generation;
    p.key[key++] = static_cast<std::uint64_t>(source.rank);
    p.key[key++] = static_cast<std::uint64_t>(source.selector);
    p.key[key++] = e.boundary.version;
    p.key[key++] = sf::modelVersion; p.key[key++] = sf::sourceFingerprint;
    for (const auto& bank : source.banks) for (double v : bank)
    {
        if (!std::isfinite(v) || v < 0.0 || v > 1.0) return false;
        p.key[key++] = std::bit_cast<std::uint64_t>(v);
    }
    const auto& d = e.boundary.diode;
    for (double v : {e.potOhms, e.endOhms, e.followerOhms, e.boundary.driver.openCircuitVolts,
                     e.boundary.driver.outputOhms, d.saturationAmps, d.emission, d.thermalVolts, d.seriesOhms})
        p.key[key++] = std::bit_cast<std::uint64_t>(v);
    for (std::size_t i = 0; i < 26; ++i)
    {
        const double v = source.injectedAmps[i];
        if (!std::isfinite(v) || ((i == 3 || i == 4) && v != 0.0)) return false;
        p.backgroundAmps[i] = v; p.key[key++] = std::bit_cast<std::uint64_t>(v);
    }
    if (key != p.key.size()) return false;
    p.activeBank = source.selector - 11;
    p.driverG = e.boundary.driver.outputOhms > 0.0 ? 1.0 / e.boundary.driver.outputOhms : 1000.0;
    p.selectedVolts = e.boundary.driver.openCircuitVolts;
    p.diodeIs = d.saturationAmps; p.diodeNvt = d.emission * d.thermalVolts;
    p.diodeR = d.seriesOhms; p.followerR = e.followerOhms + d.seriesOhms;
    for (double v : {p.driverG, p.diodeNvt, p.followerR}) if (!std::isfinite(v) || v <= 0.0) return false;
    for (int s = 0; s < sp::railCount; ++s) for (int r = 0; r < sp::busCount; ++r)
    {
        const auto bank = static_cast<std::size_t>(s), row = static_cast<std::size_t>(r);
        const double fraction = source.banks[bank][static_cast<std::size_t>(sp::outputs[row])];
        const double top = e.potOhms * (1.0 - fraction) + e.endOhms;
        const double bottom = e.potOhms * fraction + e.endOhms;
        p.topG[bank][row] = 1.0 / top; p.bottomG[bank][row] = 1.0 / bottom;
        p.followerRatio[bank][row] = bottom / (top + bottom); p.dividerG[bank][row] = 1.0 / (top + bottom);
        for (double v : {p.topG[bank][row], p.bottomG[bank][row], p.dividerG[bank][row]})
            if (!std::isfinite(v) || v <= 0.0) return false;
        if (!std::isfinite(p.followerRatio[bank][row]) || p.followerRatio[bank][row] < 0.0
            || p.followerRatio[bank][row] > 1.0) return false;
    }
    for (int column = 0; column < 11; ++column)
    {
        sf::ToneControls tone; if (!sf::compileTone(source.rank, column, tone)) return false;
        for (int r = 0; r < sp::busCount; ++r)
        {
            const auto row = static_cast<std::size_t>(r), i = static_cast<std::size_t>(sp::outputs[row]);
            const auto b = tone.hypotheses[i] != nullptr ? sf::hypothesisBranch(tone.hypothesisExcitations[i])
                : hardware::findBranch(tone.source.board, tone.source.column, static_cast<int>(i) + 1);
            p.factoryPresent[static_cast<std::size_t>(column)][row] = b.kind != hardware::BranchKind::open;
            // Match the reference's resistance grouping before freezing the
            // binary64 coefficient as an exact constant of this prepared graph.
            p.factoryR[static_cast<std::size_t>(column)][row] =
                ((b.kind == hardware::BranchKind::resistor ? b.resistanceOhms() : 0.0) + e.boundary.driver.outputOhms) + d.seriesOhms;
            if (!std::isfinite(p.factoryR[static_cast<std::size_t>(column)][row])
                || p.factoryR[static_cast<std::size_t>(column)][row] <= 0.0) return false;
        }
    }
    for (int r = 0; r < sp::busCount; ++r)
    {
        const auto row = static_cast<std::size_t>(r);
        p.load[row] = 1.0 / sf::assembledShuntOhms + sf::compiledReceiverSiemens(sp::outputs[row] + 1, sf::Grade::B, sf::Grade::B);
        if (!std::isfinite(p.load[row]) || p.load[row] <= 0.0) return false;
    }
    p.ready = true; destination = p; return true;
}
inline bool sparseInputs(const experimental::memory_sparse::Prepared& p, const ReceiverDrives& drives,
                         experimental::memory_sparse::Inputs& destination)
{
    namespace sp = experimental::memory_sparse;
    if (!p.ready) return false;
    const auto injection = receiverInjection(drives); if (!injection.valid) return false;
    sp::Inputs input;
    for (std::size_t r = 0; r < 26; ++r)
    {
        input.constantAmps[r] = p.backgroundAmps[r] + injection.constantAmps[r];
        if (!std::isfinite(input.constantAmps[r])) return false;
    }
    for (std::size_t i = 0; i < 6; ++i)
        input.feedbackG[i] = injection.feedback.siemens[static_cast<std::size_t>(sp::outputs[static_cast<std::size_t>(sp::feedbackTargets[i])])]
                                                       [static_cast<std::size_t>(sp::outputs[static_cast<std::size_t>(sp::feedbackSources[i])])];
    destination = input; return true;
}
} // namespace cs80::reference::memory_panel
