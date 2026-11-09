#pragma once

#include "experimental/MemorySparseNewton.h"
#include "experimental/MemoryVerifiedDiode.h"

namespace cs80::experimental::memory_sparse
{
// Conservative enclosure for the *prepared coefficient graph*. Coefficients
// are exact binary64 constants, not intervals for Yamaha component uncertainty.
// The verifier is independently callable on any complete candidate state.
inline Certificate certify(const Prepared& p, const Inputs& input, const State& candidate,
                           double requestedNodeErrorVolts) noexcept
{
    namespace iv = verified;
    Certificate report;
    if (!iv::supportedArithmetic()) { report.status = Status::arithmeticUnsupported; return report; }
    if (!detail::preparedValid(p)) { report.status = Status::invalidConfiguration; return report; }
    if (!detail::inputsValid(input)) { report.status = Status::invalidInput; return report; }
    if (candidate.key != p.key) { report.status = Status::staleState; return report; }
    if (!std::isfinite(requestedNodeErrorVolts) || requestedNodeErrorVolts <= 0.0)
        { report.status = Status::invalidOptions; return report; }
    for (double v : candidate.volts) if (!std::isfinite(v)) { report.status = Status::invalidState; return report; }
    double margin = std::min(p.driverG, 1000.0);
    for (int s = 0; s < railCount; ++s) for (int r : wiperBuses)
        margin = std::min(margin, p.bottomG[static_cast<std::size_t>(s)][static_cast<std::size_t>(r)]);
    std::array<iv::Interval, busCount> load;
    for (std::size_t r = 0; r < load.size(); ++r) load[r] = iv::point(p.load[r]);
    for (std::size_t i = 0; i < input.feedbackG.size(); ++i)
    {
        const auto target = static_cast<std::size_t>(feedbackTargets[i]);
        load[target] = iv::sub(load[target], iv::point(input.feedbackG[i]));
    }
    for (const auto& g : load)
    {
        if (!iv::valid(g)) { report.status = Status::invalidInput; return report; }
        margin = std::min(margin, g.lo);
    }
    if (!(margin > 0.0) || !std::isfinite(margin)) { report.status = Status::invalidInput; return report; }
    report.dominanceLowerSiemens = margin;
    // A per-branch work/precision request, not an acceptance proof. The final
    // summed full-node residual alone decides whether the requested bound fits.
    const auto allocation = iv::div(iv::mul(iv::point(requestedNodeErrorVolts), iv::point(margin)), iv::point(64.0));
    const double currentWidth = allocation.lo;
    if (!std::isfinite(currentWidth) || !(currentWidth > 0.0))
        { report.status = Status::diodeEnclosureFailed; return report; }
    // Rebuild a sufficient reverse-current condition locally for this exact
    // configuration and precision request. No cached state crosses calls. For
    // negative i_hi and R <= Rmax, R*i_hi >= Rmax*i_hi; a verified lower bound
    // on V_Rmax(i_hi) proves every branch with across.hi <= threshold has its
    // exact current in [-Is,i_hi]. This is not a reverse-current approximation.
    bool reverseReady = false;
    iv::Interval reverseCurrent, reverseThreshold;
    double maximumR = std::max(p.diodeR,p.followerR);
    for (const auto& column : p.factoryR) for (double resistance : column) maximumR = std::max(maximumR,resistance);
    const double upperCurrent = -p.diodeIs + std::min(currentWidth * 0.5,p.diodeIs * 0.5);
    if (upperCurrent > -p.diodeIs && upperCurrent < 0.0)
    {
        const auto width = iv::sub(iv::point(upperCurrent),iv::point(-p.diodeIs));
        const auto argument = iv::div(iv::add(iv::point(p.diodeIs),iv::point(upperCurrent)),iv::point(p.diodeIs));
        iv::Interval logLower, logUpper;
        if (iv::valid(width) && width.hi <= currentWidth && iv::valid(argument) && argument.lo > 0.0
            && iv::encloseLog(argument.lo,logLower) && iv::encloseLog(argument.hi,logUpper))
        {
            reverseThreshold = iv::add(iv::mul(iv::point(maximumR),iv::point(upperCurrent)),
                                      iv::mul(iv::point(p.diodeNvt),{logLower.lo,logUpper.hi}));
            reverseCurrent = {-p.diodeIs,upperCurrent};
            reverseReady = iv::valid(reverseThreshold) && iv::valid(reverseCurrent);
        }
    }
    std::array<iv::Interval, nodeCount> f;
    for (auto& v : f) v = iv::point(0.0);
    const auto& x = candidate.volts;
    const auto add = [&](std::size_t node, iv::Interval value) { f[node] = iv::add(f[node], value); };
    const auto subtract = [&](std::size_t node, iv::Interval value) { f[node] = iv::sub(f[node], value); };
    const auto diode = [&](iv::Interval across, double resistance, iv::Interval& result)
    {
        if (!iv::valid(across)) return false;
        if (reverseReady && across.hi <= reverseThreshold.lo) { result = reverseCurrent; return true; }
        const double middle = 0.5 * across.lo + 0.5 * across.hi;
        const auto guess = detail::diodeCandidate(middle, resistance, p.diodeIs, p.diodeNvt);
        // The guess is only a proposal. Its validity/accuracy never substitutes
        // for the independently verified enclosure's sign/width contract.
        return iv::encloseDiode(across, resistance, p.diodeIs, p.diodeNvt,
                                guess.valid ? guess.amps : 0.0, result, currentWidth);
    };
    for (int s = 0; s < railCount; ++s)
    {
        const auto bank = static_cast<std::size_t>(s);
        add(bank, iv::mul(iv::point(p.driverG), iv::sub(iv::point(s == p.activeBank ? p.selectedVolts : 0.0), iv::point(x[bank]))));
        for (int r : {4, 6})
        {
            const auto row = static_cast<std::size_t>(r), bi = static_cast<std::size_t>(busBase + r);
            subtract(bank, iv::mul(iv::point(p.dividerG[bank][row]), iv::point(x[bank])));
            iv::Interval current;
            if (!diode(iv::sub(iv::mul(iv::point(p.followerRatio[bank][row]), iv::point(x[bank])), iv::point(x[bi])), p.followerR, current))
                { report.status = Status::diodeEnclosureFailed; return report; }
            add(bi, current);
        }
        for (int w = 0; w < wipersPerBank; ++w)
        {
            const auto row = static_cast<std::size_t>(wiperBuses[static_cast<std::size_t>(w)]);
            const auto wi = static_cast<std::size_t>(wiperNode(s, w)), bi = static_cast<std::size_t>(busBase) + row;
            const auto top = iv::mul(iv::point(p.topG[bank][row]), iv::sub(iv::point(x[bank]), iv::point(x[wi])));
            subtract(bank, top); add(wi, top);
            subtract(wi, iv::mul(iv::point(p.bottomG[bank][row]), iv::point(x[wi])));
            iv::Interval current;
            if (!diode(iv::sub(iv::point(x[wi]), iv::point(x[bi])), p.diodeR, current))
                { report.status = Status::diodeEnclosureFailed; return report; }
            subtract(wi, current); add(bi, current);
        }
    }
    f[emitterNode] = iv::mul(iv::point(-1000.0), iv::point(x[emitterNode]));
    for (int r = 0; r < busCount; ++r)
    {
        const auto row = static_cast<std::size_t>(r), bi = static_cast<std::size_t>(busBase + r);
        for (std::size_t column = 0; column < p.factoryR.size(); ++column)
        {
            if (!p.factoryPresent[column][row]) continue;
            iv::Interval current;
            if (!diode(iv::sub(iv::point(0.0), iv::point(x[bi])), p.factoryR[column][row], current))
                { report.status = Status::diodeEnclosureFailed; return report; }
            add(bi, current);
        }
        subtract(bi, iv::mul(iv::point(p.load[row]), iv::point(x[bi])));
        add(bi, iv::point(input.constantAmps[static_cast<std::size_t>(outputs[row])]));
    }
    for (std::size_t i = 0; i < input.feedbackG.size(); ++i)
        add(static_cast<std::size_t>(busBase + feedbackTargets[i]),
            iv::mul(iv::point(input.feedbackG[i]), iv::point(x[static_cast<std::size_t>(busBase + feedbackSources[i])])));
    double worst = 0.0;
    for (std::size_t i = 0; i < f.size(); ++i)
    {
        if (!iv::valid(f[i])) { report.status = Status::invalidState; return report; }
        const double magnitude = std::max(std::abs(f[i].lo), std::abs(f[i].hi));
        if (!std::isfinite(magnitude)) { report.status = Status::invalidState; return report; }
        report.residualNodeUpperAmps[i] = magnitude;
        worst = std::max(worst, magnitude);
    }
    report.residualUpperAmps = worst;
    const auto bound = iv::div(iv::point(worst), iv::point(margin));
    if (!iv::valid(bound)) { report.status = Status::invalidState; return report; }
    report.nodeErrorUpperVolts = bound.hi;
    report.status = bound.hi <= requestedNodeErrorVolts ? Status::success : Status::residualTooLarge;
    return report;
}
} // namespace cs80::experimental::memory_sparse
