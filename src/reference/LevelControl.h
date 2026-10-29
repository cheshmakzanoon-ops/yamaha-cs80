#pragma once
// Offline conditional DC network only. No IC5 LI-to-gain law or production dependency.
// See docs/LEVEL_CONTROL_REFERENCE_DESIGN.md.
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cs80::reference::level {
inline constexpr double unknown = std::numeric_limits<double>::quiet_NaN();
inline constexpr int nodeCount = 16;
inline constexpr int busCount = 43;
// Nodes: rank I LC1..8 [0..7], rank II LC1..8 [8..15].
// Fixture buses: AM[0], LV ranks[1..2], KB voices[3..10],
// TI node-indexed[11..26], TA node-indexed[27..42]. These are conditional
// independent boundary ports, NOT proof of 43 independent installed drivers.
inline double branchOhms(int bus) {
    return bus == 0 ? 180000.0 : bus <= 2 ? 560000.0 : 270000.0;
}
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
    if (!valid) throw std::invalid_argument("Level: explicit valid source/receiver boundaries required");
}
inline void numerical(bool valid) {
    if (!valid) throw std::overflow_error("Level: ill-conditioned/unrepresentable network");
}
inline bool attached(int node, int bus) { return bus == 0 || bus == 1 + node / 8 || bus == 3 + node % 8
        || bus == 11 + node || bus == 27 + node; }
inline Result solve(const Inputs& in) {
    for (auto s : in.sources)
        require(std::isfinite(s.volts) && std::isfinite(s.ohms) && s.ohms >= 0);
    for (auto r : in.receivers)
        require(std::isfinite(r.siemens) && r.siemens >= 0 && std::isfinite(r.amps));
    using Matrix = std::array<std::array<double, nodeCount>, nodeCount>;
    Matrix a {};
    std::array<double, nodeCount> rhs {};
    std::array<double, busCount> inverseDenominator {}, coupling {};
    for (int i = 0; i < nodeCount; ++i) {
        a[i][i] = in.receivers[i].siemens;
        for (int b = 0; b < busCount; ++b) if (attached(i, b))
            a[i][i] += 1.0 / branchOhms(b);
        rhs[i] = -in.receivers[i].amps;
    }
    for (int b = 0; b < busCount; ++b) {
        const double g = 1.0 / branchOhms(b);
        const double count = b == 0 ? 16.0 : b <= 2 ? 8.0 : b <= 10 ? 2.0 : 1.0;
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
        double margin = in.receivers[i].siemens;
        for (int b = 0; b < busCount; ++b) if (attached(i, b))
            margin += inverseDenominator[b] / branchOhms(b);
        margin /= scale;
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
        const double g = 1.0 / branchOhms(b);
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
        long double balance = out.receiverAmps[i];
        long double magnitudeKcl = std::abs(balance);
        for (int b = 0; b < busCount; ++b) if (attached(i, b)) {
            const long double current = (static_cast<long double>(out.nodeVolts[i])
                - out.busVolts[b]) / branchOhms(b);
            balance += current;
            magnitudeKcl += std::abs(current);
        }
        numerical(std::isfinite(balance));
        numerical(std::abs(balance) <= 1e-9L * magnitudeKcl + 1e-18L);
    }
    return out;
}
} // namespace cs80::reference::level
