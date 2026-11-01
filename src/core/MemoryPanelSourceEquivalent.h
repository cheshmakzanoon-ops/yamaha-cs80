#pragma once

#include "MemoryPanels.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

// Control-rate-only reduction of the compiled M1..M4/PANEL source network to
// a LOCAL small-signal Thevenin equivalent per output. This is not an Engine
// connection and does not alter any factory tone, Legacy or V1 gate.
//
// Source topology: BA27/BA28, MBK63, service S31-S38; declared 50k pot,
// 2SC458 driver, ideal NJM4558 follower and 1S1555 surrogate parameters in
// MemoryPanels.h. The nonlinear diode and selected rail make a single GLOBAL
// Thevenin voltage/impedance impossible; this is a tangent at the settled
// DC operating point, with all other output receiver loads held in place.
// Distinct outputs remain coupled through their shared selected rail; a diagonal
// equivalent does not purport to reproduce their full 24x24 transfer matrix.
namespace cs80::memory_panels::source_equivalent
{
inline constexpr std::uint32_t modelVersion = 1;
inline constexpr std::string_view modelId = "memory-panel-selected-rail-diagonal-thevenin-v1";

struct Port
{
    sf::SourceEquivalent source {};  // open-circuit local tangent, including assembled 10k shunt
    double loadedBusVolts = 0.0;     // solved with the receiver conductance still attached
    double loadedPortOhms = 0.0;     // dVbus/dIinjection with receiver still attached
    double railBackreactionOhms = 0.0;
    double selectedBranchSiemens = 0.0;
    bool analog = false;
};
struct Result
{
    std::array<Port, 26> ports {};
    Switches switches;
    double selectedRailVolts = 0.0;
    bool ok = false;
};

// Let t/b be the pot's top/bottom resistance, a=b/(t+b), rw=t*b/(t+b),
// gb=1/(rw(or follower R)+diode series R+diode dV/dI), and
// G=1/10k+Greceiver. At the selected rail, only UNBUFFERED branch current
// draws through its pot; the ideal follower's current comes from its supply.
// The one-port response to injecting current J into output i is:
//
// D = 1/Rdriver + sum_k[1/(t_k+b_k)]
//     + sum_unbuffered,k[a_k*a_k*gb_k*G_k/(G_k+gb_k)]
// dVbus_i/dJ = 1/(G_i+gb_i) + [a_i*gb_i/(G_i+gb_i)]^2/D (unbuffered),
//              1/(G_i+gb_i) (buffered).
// The common shunt stays INSIDE the source; only downstream receiver load
// gReceiver is removed: Rth=1/(1/Rloaded-gReceiver),
// Eth=Vloaded*(1+Rth*gReceiver). This Eth is the tangent intercept, NOT the
// global nonlinear no-load voltage of the Shockley diode.
inline Result compile(int rank, int selector, const Positions& selected,
                      const Electrical& e = nominalElectrical) noexcept
{
    Result out;
    if (rank < 0 || rank > 1 || selector < 11 || selector > 13 || !sf::valid(e.boundary)
        || !(e.potOhms > 0.0) || !(e.endOhms > 0.0) || !(e.followerOhms >= 0.0))
        return out;

    const auto dc = compileBuses(rank, selector, selected, e);
    if (!dc.ok) return out;
    out.switches = switchStates(rank, selector, selected);
    out.selectedRailVolts = dc.railVolts;
    std::array<double, 26> alpha {}, branchG {}, totalLoadG {}, receiverG {};
    double railStiffness = e.boundary.driver.outputOhms > 0.0
        ? 1.0 / e.boundary.driver.outputOhms : 0.0;
    const bool stiffRail = e.boundary.driver.outputOhms == 0.0;

    for (int output = 1; output <= 26; ++output)
    {
        if (isSwitchRow(output)) continue;
        const auto i = static_cast<std::size_t>(output - 1);
        const double x = clampedFraction(selected[i]);
        const double top = e.potOhms * (1.0 - x) + e.endOhms;
        const double bottom = e.potOhms * x + e.endOhms;
        const double sum = top + bottom;
        alpha[i] = bottom / sum;
        receiverG[i] = sf::compiledReceiverSiemens(output, sf::Grade::B, sf::Grade::B);
        totalLoadG[i] = 1.0 / sf::assembledShuntOhms + receiverG[i];
        if (!stiffRail) railStiffness += 1.0 / sum;

        const double wiper = dc.railVolts * alpha[i];
        if (wiper > 0.0)
        {
            sf::BoundaryModel local = e.boundary;
            local.driver = {wiper, isBufferedRow(output) ? e.followerOhms : top * bottom / sum};
            sf::BranchSolution branch;
            if (!sf::solveBranch(0.0, local, receiverG[i], 0.0, branch)) return {};
            const double series = local.driver.outputOhms + e.boundary.diode.seriesOhms + branch.dynamicOhms;
            if (!(series > 0.0) || !std::isfinite(series)) return {};
            branchG[i] = 1.0 / series;
        }
        if (!stiffRail && !isBufferedRow(output))
            railStiffness += alpha[i] * alpha[i] * branchG[i] * totalLoadG[i]
                             / (totalLoadG[i] + branchG[i]);
    }
    if (!stiffRail && !(railStiffness > 0.0)) return {};

    for (int output = 1; output <= 26; ++output)
    {
        const auto i = static_cast<std::size_t>(output - 1);
        auto& port = out.ports[i];
        if (isSwitchRow(output))
        {
            const bool enabled = output == 4 ? out.switches.pulse : out.switches.saw;
            port.source = {enabled ? 10.0 : 0.0, 0.0};
            continue; // no analogue Thevenin model of a logical gate
        }
        const double denominator = totalLoadG[i] + branchG[i];
        double rLoaded = 1.0 / denominator;
        if (!stiffRail && !isBufferedRow(output))
        {
            const double coupling = alpha[i] * branchG[i] / denominator;
            port.railBackreactionOhms = coupling * coupling / railStiffness;
            rLoaded += port.railBackreactionOhms;
        }
        const double sourceDenominator = 1.0 / rLoaded - receiverG[i];
        if (!(sourceDenominator > 0.0) || !std::isfinite(sourceDenominator)) return {};
        const double rSource = 1.0 / sourceDenominator;
        const double eSource = dc.busVolts[i] * (1.0 + rSource * receiverG[i]);
        port.source = {eSource, rSource};
        port.loadedBusVolts = dc.busVolts[i];
        port.loadedPortOhms = rLoaded;
        port.selectedBranchSiemens = branchG[i];
        port.analog = true;
        if (!sf::valid(port.source) || port.source.ohms > sf::assembledShuntOhms * (1.0 + 1e-8))
            return {};
    }
    out.ok = true;
    return out;
}
} // namespace cs80::memory_panels::source_equivalent
