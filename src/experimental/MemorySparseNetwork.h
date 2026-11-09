#pragma once

#include "experimental/MemorySparseCertificate.h"

namespace cs80::experimental::memory_sparse
{
// OFFLINE candidate only. Publication means writing this caller-owned State,
// not an Engine/audio-thread handoff. Any rejection leaves destination intact.
inline Report update(const Prepared& p, const Inputs& input, const State* warmStart,
                     State& destination, const Options& options = {}) noexcept
{
    Report report;
    if (!verified::supportedArithmetic()) { report.status = Status::arithmeticUnsupported; return report; }
    if (!detail::preparedValid(p)) { report.status = Status::invalidConfiguration; return report; }
    if (!detail::inputsValid(input) || !(detail::approximateMargin(p, input) > 0.0))
        { report.status = Status::invalidInput; return report; }
    if (options.maxIterations < 0 || options.maxIterations > 64
        || !std::isfinite(options.nodeErrorVolts) || options.nodeErrorVolts <= 0.0)
        { report.status = Status::invalidOptions; return report; }
    State candidate; candidate.key = p.key;
    if (warmStart != nullptr)
    {
        if (warmStart->key != p.key) { report.status = Status::staleState; return report; }
        for (double v : warmStart->volts) if (!std::isfinite(v)) { report.status = Status::invalidState; return report; }
        candidate.volts = warmStart->volts;
    }
    else
    {
        for (int s = 0; s < railCount; ++s)
        {
            const auto bank = static_cast<std::size_t>(s);
            candidate.volts[bank] = s == p.activeBank ? p.selectedVolts : 0.0;
            for (int w = 0; w < wipersPerBank; ++w)
                candidate.volts[static_cast<std::size_t>(wiperNode(s, w))] = candidate.volts[bank]
                    * p.followerRatio[bank][static_cast<std::size_t>(wiperBuses[static_cast<std::size_t>(w)])];
        }
        for (int r = 0; r < busCount; ++r) candidate.volts[static_cast<std::size_t>(busBase + r)] = 1.0;
    }
    const double threshold = options.nodeErrorVolts * detail::approximateMargin(p, input) * 0.5;
    for (int iteration = 0; iteration <= options.maxIterations; ++iteration)
    {
        report.iterations = iteration;
        detail::Linearization linear;
        if (!detail::linearize(p, input, candidate.volts, linear))
            { report.status = Status::diodeEnclosureFailed; return report; }
        double worst = 0.0;
        for (double v : linear.residual) worst = std::max(worst, std::abs(v));
        if (worst <= threshold || iteration == options.maxIterations)
        {
            report.certificate = certify(p, input, candidate, options.nodeErrorVolts);
            if (report.certificate.accepted())
            {
                destination = candidate; report.status = Status::success; return report;
            }
            if (report.certificate.status != Status::residualTooLarge)
                { report.status = report.certificate.status; return report; }
        }
        if (iteration == options.maxIterations) break;
        Nodes dx;
        if (!detail::solveNewton(p, input, linear, dx)) { report.status = Status::linearSolveFailed; return report; }
        double largest = 0.0;
        for (double v : dx) largest = std::max(largest, std::abs(v));
        const double scale = largest > 0.5 ? 0.5 / largest : 1.0;
        for (std::size_t i = 0; i < candidate.volts.size(); ++i)
        {
            candidate.volts[i] += scale * dx[i];
            if (!std::isfinite(candidate.volts[i])) { report.status = Status::invalidState; return report; }
        }
    }
    report.status = Status::iterationLimit; return report;
}
} // namespace cs80::experimental::memory_sparse
