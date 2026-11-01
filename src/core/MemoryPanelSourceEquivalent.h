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
    double inactiveBranchSiemens = 0.0; // reverse/zero-bias diodes on other physical sources
    bool analog = false;
};
struct Result
{
    std::array<Port, 26> ports {};
    Switches switches;
    double selectedRailVolts = 0.0;
    bool ok = false;
};

// The independent selected-pot limit (x near zero with the 1-ohm physical
// end stop) can put a 1S1555 below its usual forward-bias voltage. The common
// factory branch solver uses a post-Newton current residual divided by its
// *small series resistance* and may reject such an otherwise valid tiny
// current. This tightly bracketed fallback instead solves the same exact
// Shockley diode KVL in volts, only when that existing solver declines it.
// This is a numerical method, not a new diode curve or hardware calibration.
namespace detail
{
inline bool solveShockleyTangentFallback(double wiperVolts, double driverOhms,
                                         double receiverSiemens,
                                         const sf::DiodeModel& diode,
                                         double& branchSiemens) noexcept
{
    const double nvt = diode.emission * diode.thermalVolts;
    const double series = driverOhms + diode.seriesOhms;
    const double load = 1.0 / sf::assembledShuntOhms + receiverSiemens;
    const double total = series + 1.0 / load;
    if (!(wiperVolts > 0.0) || !(total > 0.0) || !(nvt > 0.0)
        || !(diode.saturationAmps > 0.0) || !std::isfinite(total))
        return false;

    const double is = diode.saturationAmps;
    double lo = 0.0, hi = wiperVolts / total;
    // Linearized Shockley is an excellent initial point near the knee; the
    // root remains bracketed by zero current and the ideal-diode current.
    double current = wiperVolts / (total + nvt / is);
    bool converged = false;
    for (int iteration = 0; iteration < 120; ++iteration)
    {
        const double residual = current * total + nvt * std::log1p(current / is) - wiperVolts;
        if (std::abs(residual) <= 1e-12)
        {
            converged = true;
            break;
        }
        if (residual > 0.0) hi = current; else lo = current;
        const double slope = total + nvt / (current + is);
        double next = current - residual / slope;
        if (!(next > lo && next < hi)) next = lo + 0.5 * (hi - lo);
        current = next;
    }
    if (!converged || !(current >= 0.0) || !std::isfinite(current)) return false;
    const double dynamic = nvt / (current + is);
    branchSiemens = 1.0 / (series + dynamic);
    return std::isfinite(branchSiemens) && branchSiemens > 0.0;
}

// When an analogue row's selected bus approaches 0 V, the *unselected*
// T51-T54 cells and the other two M/PANEL banks are not mathematically open:
// a Shockley junction near zero bias has Is/(nVt) positive differential
// conductance. DC leakage (<nA per diode) was already negligible under the
// frozen 2 mV bus gate, but dropping this small-signal conductance moves the
// local Thevenin resistance by tens of ohms. The offline full Newton model
// retains these physical diodes and is the independent acceptance oracle.
inline bool reverseDiodeTangent(double busVolts, double upstreamOhms,
                                const sf::DiodeModel& diode,
                                double& siemens) noexcept
{
    const double nvt = diode.emission * diode.thermalVolts;
    const double is = diode.saturationAmps;
    const double series = upstreamOhms + diode.seriesOhms;
    if (!std::isfinite(busVolts) || busVolts < 0.0 || !(series >= 0.0)
        || !(nvt > 0.0) || !(is > 0.0)) return false;
    // Analytic bound, never a fitted cutoff: -Is < I <= 0 implies
    // Vdiode = -Vbus - I*R <= -Vbus + Is*R, hence
    // dI/dV <= (Is/nVt)*exp((-Vbus + Is*R)/nVt).
    // One 10k assembled shunt guarantees |dRsource/dG| <= (10k)^2.
    // With eleven inactive factory columns and two inactive memory/PANEL
    // branches, omit one reverse tangent only if its WORST contribution
    // cannot exceed 0.002/13 ohms. This caps the total omission at 0.002Ω,
    // far below the separately frozen 2Ω + 0.2% truth allowance. For the
    // nominal diode and printed resistors it activates near 0.5 V reverse;
    // alternate diode parameters automatically recalculate the bound.
    constexpr double maximumIgnoredSourceOhms = 0.002;
    constexpr int maximumInactiveBranches = 13;
    const double allowedSiemens = maximumIgnoredSourceOhms /
        (maximumInactiveBranches * sf::assembledShuntOhms * sf::assembledShuntOhms);
    const double upperSiemens = is / nvt * std::exp((-busVolts + is * series) / nvt);
    if (upperSiemens <= allowedSiemens) { siemens = 0.0; return true; }

    double lo = -is * (1.0 - 1e-14), hi = 0.0;
    double current = std::clamp(is * std::expm1(-busVolts / nvt), lo, hi);
    bool converged = false;
    for (int iteration = 0; iteration < 120; ++iteration)
    {
        const double residual = current * series + nvt * std::log1p(current / is) + busVolts;
        if (std::abs(residual) <= 1e-12) { converged = true; break; }
        if (residual > 0.0) hi = current; else lo = current;
        const double slope = series + nvt / (current + is);
        double next = current - residual / slope;
        if (!(next > lo && next < hi)) next = lo + 0.5 * (hi - lo);
        current = next;
    }
    if (!converged || !(current > -is) || !(current <= 0.0)) return false;
    siemens = 1.0 / (series + nvt / (current + is));
    return std::isfinite(siemens) && siemens >= 0.0;
}

inline bool inactiveBranchLoad(int rank, int output, double busVolts,
                               const Electrical& e, double& siemens) noexcept
{
    double total = 0.0;
    // All eleven factory columns are unselected when M1..M4/PANEL is active.
    // Include classified printed cells, plus each versioned hypothesis only
    // for the original 25 unresolved cells; open branches contribute no diode.
    for (int selector = 0; selector < 11; ++selector)
    {
        const auto* address = sf::address(rank, selector);
        if (address == nullptr) return false;
        auto branch = hardware::findBranch(address->board, address->column, output);
        if (branch.kind == hardware::BranchKind::unknown)
        {
            const auto* h = factory_hypotheses::find(address->board, address->column, output);
            if (h == nullptr) return false;
            branch = sf::hypothesisBranch(h->nominalExcitation);
        }
        if (branch.kind == hardware::BranchKind::open) continue;
        if (branch.kind != hardware::BranchKind::direct
            && branch.kind != hardware::BranchKind::resistor) return false;
        const double rowOhms = branch.kind == hardware::BranchKind::resistor
            ? branch.resistanceOhms() : 0.0;
        double g = 0.0;
        if (!reverseDiodeTangent(busVolts, rowOhms + e.boundary.driver.outputOhms,
                                 e.boundary.diode, g)) return false;
        total += g;
    }
    // The two inactive M/PANEL banks also put one reverse diode each on every
    // analogue row. Their individual unselected pot positions are not supplied
    // to this selected-bank-only ControlFrame; the printed topology bounds
    // their series wiper impedance between 1 ohm and (50k/4+1) ohm. At zero
    // bias the diode's ~18 megaohm differential resistance dominates, so the
    // midpoint surrogate changes a 10k source resistance <0.1 ohm between
    // either pot endpoint. This bounded choice is a DECLARED INFERENCE.
    const double inactivePotWiper = isBufferedRow(output) ? e.followerOhms
        : 0.25 * e.potOhms + e.endOhms;
    for (int bank = 0; bank < 2; ++bank)
    {
        double g = 0.0;
        if (!reverseDiodeTangent(busVolts, inactivePotWiper,
                                 e.boundary.diode, g)) return false;
        total += g;
    }
    siemens = total;
    return std::isfinite(total) && total >= 0.0;
}
} // namespace detail

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
    std::array<double, 26> alpha {}, branchG {}, inactiveG {}, totalLoadG {}, receiverG {};
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
        if (!detail::inactiveBranchLoad(rank, output, dc.busVolts[i], e, inactiveG[i])) return {};
        totalLoadG[i] = 1.0 / sf::assembledShuntOhms + receiverG[i] + inactiveG[i];
        if (!stiffRail) railStiffness += 1.0 / sum;

        const double wiper = dc.railVolts * alpha[i];
        if (wiper > 0.0)
        {
            sf::BoundaryModel local = e.boundary;
            local.driver = {wiper, isBufferedRow(output) ? e.followerOhms : top * bottom / sum};
            sf::BranchSolution branch;
            if (sf::solveBranch(0.0, local, receiverG[i], 0.0, branch))
            {
                const double series = local.driver.outputOhms + e.boundary.diode.seriesOhms + branch.dynamicOhms;
                if (!(series > 0.0) || !std::isfinite(series)) return {};
                branchG[i] = 1.0 / series;
            }
            else if (!detail::solveShockleyTangentFallback(
                         wiper, local.driver.outputOhms, receiverG[i], e.boundary.diode, branchG[i]))
                return {};
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
        port.inactiveBranchSiemens = inactiveG[i];
        port.analog = true;
        if (!sf::valid(port.source) || port.source.ohms > sf::assembledShuntOhms * (1.0 + 1e-8))
            return {};
    }
    out.ok = true;
    return out;
}
} // namespace cs80::memory_panels::source_equivalent
