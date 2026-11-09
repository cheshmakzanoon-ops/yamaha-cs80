#pragma once

#include "reference/MemoryPanelNetwork.h"

namespace cs80::reference::memory_panel
{
// Physical boundary units, independently assembled from the resistor graph.
// No experimental prepared map or source-equivalent receiver helper is used.
struct ReceiverDrive
{
    double envelopeVolts = 0.0, velocity = 0.0, pressure = 0.0;
    double keyboardCutoffVolts = 0.0, keyboardLevelVolts = 0.0;
};
struct ReceiverDrives
{
    std::array<ReceiverDrive, 8> voices {};
    double masterResVolts = 0.0;
};
struct ReceiverInjection
{
    std::array<double, 26> constantAmps {};
    BusCurrentFeedback feedback;
    bool valid = false;
};
inline ReceiverInjection receiverInjection(const ReceiverDrives& input)
{
    ReceiverInjection out;
    if (!std::isfinite(input.masterResVolts) || input.masterResVolts < 0.0 || input.masterResVolts > 10.0) return out;
    double env = 0.0, velocity = 0.0, pressure = 0.0, kc = 0.0, kb = 0.0;
    for (const auto& d : input.voices)
    {
        if (!std::isfinite(d.envelopeVolts) || !std::isfinite(d.velocity) || !std::isfinite(d.pressure)
            || !std::isfinite(d.keyboardCutoffVolts) || !std::isfinite(d.keyboardLevelVolts)
            || d.envelopeVolts < -5.0 || d.envelopeVolts > 5.0 || d.velocity < 0.0 || d.velocity > 1.0
            || d.pressure < 0.0 || d.pressure > 1.0) return out;
        env += d.envelopeVolts; velocity += d.velocity; pressure += d.pressure;
        kc += d.keyboardCutoffVolts; kb += d.keyboardLevelVolts;
    }
    for (std::size_t side = 0; side < 2; ++side)
    {
        const bool hp = side == 0;
        const double gc = 1.0 / 39000.0, gt = 1.0 / (hp ? 120000.0 : 39000.0), ge = 1.0 / (hp ? 100000.0 : 47000.0);
        const double diagonal = 1.0 / (hp ? 2200.0 : 2700.0) + 5.0 * gt
            + 1.0 / (hp ? 680000.0 : 220000.0) + ge + gc + 1.0 / 175000.0;
        const double t = gt / diagonal, e = ge / diagonal;
        const auto row = 6 + 2 * side;
        out.constantAmps[row] = gc * (t * kc + e * env);
        out.feedback.siemens[row][22] = gc * t * velocity;
        out.feedback.siemens[row][24] = gc * t * pressure;
        out.constantAmps[row + 1] = 8.0 * input.masterResVolts / 440000.0;
    }
    const double d = 1.0 / 180000.0 + 1.0 / 560000.0 + 3.0 / 270000.0;
    const double t = (1.0 / 270000.0) / d;
    out.constantAmps[21] = (1.0 / 560000.0) * t * kb;
    out.feedback.siemens[21][23] = (1.0 / 560000.0) * t * velocity;
    out.feedback.siemens[21][25] = (1.0 / 560000.0) * t * pressure;
    for (double value : out.constantAmps) if (!std::isfinite(value)) return {};
    out.valid = true;
    return out;
}
struct ReceiverNetworkResult
{
    Result network;
    std::array<double, 26> injectionAmps {};
    bool converged = false;
};
// Add the physical receiver currents to any explicitly supplied background
// injection. Do NOT pass a numerical map's reference-current bias as background.
inline ReceiverNetworkResult solveReceivers(const Setup& source, const ReceiverDrives& drives)
{
    ReceiverNetworkResult out;
    const auto injection = receiverInjection(drives);
    const auto& e = source.electrical;
    if (!injection.valid || source.rank < 0 || source.rank > 1 || source.selector < 11 || source.selector > 13
        || !sf::valid(e.boundary) || !std::isfinite(e.potOhms) || e.potOhms <= 0.0
        || !std::isfinite(e.endOhms) || e.endOhms <= 0.0
        || !std::isfinite(e.followerOhms) || e.followerOhms < 0.0) return out;
    for (const auto& bank : source.banks)
        for (double v : bank) if (!std::isfinite(v) || v < 0.0 || v > 1.0) return out;
    auto setup = source;
    for (std::size_t row = 0; row < 26; ++row) setup.injectedAmps[row] += injection.constantAmps[row];
    out.network = solve(setup, nullptr, &injection.feedback);
    if (!out.network.converged) return out;
    out.injectionAmps = setup.injectedAmps;
    for (std::size_t row = 0; row < 26; ++row)
    {
        for (std::size_t col = 0; col < 26; ++col)
            out.injectionAmps[row] += injection.feedback.siemens[row][col] * out.network.busVolts[col];
        if (!std::isfinite(out.injectionAmps[row])) return out;
    }
    out.converged = true;
    return out;
}
} // namespace cs80::reference::memory_panel
