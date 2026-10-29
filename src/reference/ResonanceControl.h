#pragma once
// Offline conditional DC network only. No VQ-to-Q law or production dependency.
// See docs/RESONANCE_CONTROL_REFERENCE_DESIGN.md.
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cs80::reference::resonance {
inline constexpr double unknown = std::numeric_limits<double>::quiet_NaN();
inline constexpr int nodeCount = 32;
inline constexpr int busCount = 5;
inline constexpr double branchOhms = 220000.0;
// Nodes: rank I HP[0..7], LP[8..15], rank II HP[16..23], LP[24..31].
// Buses: those four rank/side groups, followed by the shared RES bus at index4.
struct Source { double volts = unknown, ohms = unknown; };
struct Receiver { double siemens = unknown, amps = unknown; };
struct Inputs {
    std::array<Source, busCount> sources;
    std::array<Receiver, nodeCount> receivers;
};
struct Result {
    std::array<double, nodeCount> nodeVolts {}, receiverAmps {};
    std::array<double, busCount> busVolts {}, sourceAmps {};
};
inline void require(bool valid) {
    if (!valid) throw std::invalid_argument("Resonance: explicit valid source/receiver boundaries required");
}
inline void numerical(bool valid) {
    if (!valid) throw std::overflow_error("Resonance: ill-conditioned/unrepresentable network");
}
inline bool attached(int node, int bus) { return bus == 4 || node / 8 == bus; }
inline Result solve(const Inputs& in) {
    for (auto s : in.sources)
        require(std::isfinite(s.volts) && std::isfinite(s.ohms) && s.ohms >= 0);
    for (auto r : in.receivers)
        require(std::isfinite(r.siemens) && r.siemens >= 0 && std::isfinite(r.amps));
    constexpr double g = 1.0 / branchOhms;
    using Matrix = std::array<std::array<double, nodeCount>, nodeCount>;
    Matrix a {};
    std::array<double, nodeCount> rhs {};
    std::array<double, busCount> inverseDenominator {}, coupling {};
    for (int i = 0; i < nodeCount; ++i) {
        a[i][i] = 2.0 * g + in.receivers[i].siemens;
        rhs[i] = -in.receivers[i].amps;
    }
    for (int b = 0; b < busCount; ++b) {
        const double count = b == 4 ? 32.0 : 8.0;
        const double rg = in.sources[b].ohms * g;
        const double denominator = 1.0 + rg * count;
        numerical(std::isfinite(denominator));
        inverseDenominator[b] = 1.0 / denominator;
        coupling[b] = g * (rg / denominator);
        const double drive = g * (in.sources[b].volts / denominator);
        numerical(std::isfinite(drive));
        for (int i = 0; i < nodeCount; ++i) if (attached(i, b)) {
            rhs[i] += drive;
            for (int j = 0; j < nodeCount; ++j) if (attached(j, b))
                a[i][j] -= coupling[b];
        }
    }
    const Matrix original = a;
    const auto originalRhs = rhs;
    // Scale rows before elimination. Reject loss of resolvable pivots rather
    // than silently accepting a nearly floating network. This is a numerical
    // validity bound, not an electrical tolerance or physical resistance limit.
    double maximumScaledRowNorm = 0;
    double minimumScaledMargin = std::numeric_limits<double>::infinity();
    for (int i = 0; i < nodeCount; ++i) {
        double scale = 0;
        for (double v : a[i]) { numerical(std::isfinite(v)); scale = std::max(scale, std::abs(v)); }
        numerical(scale > 0 && std::isfinite(rhs[i]));
        double rowNorm = 0;
        for (double& v : a[i]) {
            v /= scale;
            rowNorm += std::abs(v);
        }
        // Exact circuit row-dominance margin, evaluated without subtracting
        // nearly equal diagonal/off-diagonal totals. Varah's bound applies to
        // this strictly row-diagonally-dominant, row-scaled passive matrix.
        const double margin = (in.receivers[i].siemens
            + g * inverseDenominator[i / 8] + g * inverseDenominator[4]) / scale;
        numerical(std::isfinite(margin) && margin > 0 && std::isfinite(rowNorm));
        maximumScaledRowNorm = std::max(maximumScaledRowNorm, rowNorm);
        minimumScaledMargin = std::min(minimumScaledMargin, margin);
        rhs[i] /= scale;
        numerical(std::isfinite(rhs[i]));
    }
    // Conservative infinity-norm condition upper bound for the scaled system.
    // Rejection is allowed even when a particular RHS happens to be solvable;
    // this is a numerical domain bound, not an analog component restriction.
    constexpr double conditionLimit = 1e8;
    numerical(maximumScaledRowNorm / minimumScaledMargin <= conditionLimit);
    constexpr double pivotFloor = 1e-12;
    for (int k = 0; k < nodeCount; ++k) {
        int pivot = k;
        for (int i = k + 1; i < nodeCount; ++i)
            if (std::abs(a[i][k]) > std::abs(a[pivot][k])) pivot = i;
        numerical(std::isfinite(a[pivot][k]) && std::abs(a[pivot][k]) > pivotFloor);
        std::swap(a[k], a[pivot]);
        std::swap(rhs[k], rhs[pivot]);
        for (int i = k + 1; i < nodeCount; ++i) {
            const double factor = a[i][k] / a[k][k];
            a[i][k] = 0;
            for (int j = k + 1; j < nodeCount; ++j) a[i][j] -= factor * a[k][j];
            rhs[i] -= factor * rhs[k];
        }
    }
    Result out;
    for (int i = nodeCount - 1; i >= 0; --i) {
        double value = rhs[i];
        for (int j = i + 1; j < nodeCount; ++j) value -= a[i][j] * out.nodeVolts[j];
        out.nodeVolts[i] = value / a[i][i];
        numerical(std::isfinite(out.nodeVolts[i]));
    }
    for (int b = 0; b < busCount; ++b) {
        // Weight each node individually: do not sum huge voltages before the
        // bounded source-elimination weight is applied.
        double voltage = in.sources[b].volts * inverseDenominator[b];
        const double weight = coupling[b] / g;
        for (int i = 0; i < nodeCount; ++i) if (attached(i, b))
            voltage += weight * out.nodeVolts[i];
        out.busVolts[b] = voltage;
        numerical(std::isfinite(voltage));
        // Eliminate the bus before computing source current. Subtracting the
        // reconstructed bus from each node loses tiny current when a high-Z
        // source is nearly disconnected from receiver-driven node voltages.
        const double currentWeight = g * inverseDenominator[b];
        long double sourceCurrent = 0;
        for (int i = 0; i < nodeCount; ++i) if (attached(i, b)) {
            const double difference = in.sources[b].volts - out.nodeVolts[i];
            const double contribution = std::isfinite(difference)
                ? currentWeight * difference
                : currentWeight * in.sources[b].volts - currentWeight * out.nodeVolts[i];
            sourceCurrent += contribution;
        }
        out.sourceAmps[b] = static_cast<double>(sourceCurrent);
        numerical(std::isfinite(out.sourceAmps[b]));
    }
    // Verify reduced-system residual with long-double accumulation. Component
    // KCL below independently checks reconstructed bus/node consistency.
    for (int i = 0; i < nodeCount; ++i) {
        long double residual = -static_cast<long double>(originalRhs[i]);
        long double magnitude = std::abs(static_cast<long double>(originalRhs[i]));
        for (int j = 0; j < nodeCount; ++j) {
            const long double term = static_cast<long double>(original[i][j]) * out.nodeVolts[j];
            residual += term;
            magnitude += std::abs(term);
        }
        numerical(std::abs(residual) <= 1e-10L * magnitude + 1e-300L);
        out.receiverAmps[i] = in.receivers[i].siemens * out.nodeVolts[i] + in.receivers[i].amps;
        numerical(std::isfinite(out.receiverAmps[i]));
        const double first = g * (out.nodeVolts[i] - out.busVolts[i / 8]);
        const double second = g * (out.nodeVolts[i] - out.busVolts[4]);
        const double balance = first + second + out.receiverAmps[i];
        numerical(std::isfinite(balance));
        numerical(std::abs(balance) <= 1e-9 * (std::abs(first) + std::abs(second)
                                             + std::abs(out.receiverAmps[i])) + 1e-18);
    }
    return out;
}
} // namespace cs80::reference::resonance
