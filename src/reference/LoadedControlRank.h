#pragma once
#include "LoadedControl.h"

namespace cs80::reference::loaded_control {
// One eight-voice board's passive cutoff network. No inter-rank wiring assumed.
// DC only; all sources/chip currents remain explicit conditional boundaries.
struct RankVoice {
    std::array<Source, 5> local; // KC, TF, AB, IB, local TP1 envelope
    Source hpTrim, lpTrim;
    Receiver hpReceiver, lpReceiver;
    Grade hpGrade = Grade::unknown, lpGrade = Grade::unknown;
};
struct RankInputs {
    std::array<RankVoice, 8> voices;
    std::array<Source, 4> buses; // SUB, BR, master FH, master FL: one source each
};
struct RankResult {
    std::array<double, 16> pinVolts; // HP0, LP0, HP1, LP1, ...
    std::array<std::array<double, 5>, 8> localTerminalVolts;
    std::array<double, 4> busTerminalVolts;
};
namespace rank_detail {
using Vector = std::array<double, 16>;
using Matrix = std::array<Vector, 16>;

inline Vector localConductances(std::size_t voice, std::size_t local)
{
    Vector conductance{};
    conductance[2 * voice] = 1 / (local == 4 ? 100e3 : 120e3);
    conductance[2 * voice + 1] = 1 / (local == 4 ? 47e3 : 39e3);
    return conductance;
}

inline Vector busConductances(std::size_t bus)
{
    Vector conductance{};
    for (std::size_t voice = 0; voice < 8; ++voice) {
        if (bus == 0) {
            conductance[2 * voice] = 1 / 120e3;
            conductance[2 * voice + 1] = 1 / 39e3;
        }
        if (bus == 1) {
            conductance[2 * voice] = 1 / 680e3;
            conductance[2 * voice + 1] = 1 / 220e3;
        }
        if (bus == 2)
            conductance[2 * voice] = 1 / 39e3;
        if (bus == 3)
            conductance[2 * voice + 1] = 1 / 39e3;
    }
    return conductance;
}

inline void sourceStamp(Source source, const Vector& conductance, Matrix& matrix, Vector& rhs)
{
    validate(source);
    double totalConductance = 0;
    for (double value : conductance)
        totalConductance += value;
    const double denominator = 1 + source.ohms * totalConductance;
    require(std::isfinite(denominator));
    for (std::size_t row = 0; row < 16; ++row) {
        rhs[row] += conductance[row] * source.volts / denominator;
        matrix[row][row] += conductance[row]
            * (1 + source.ohms * (totalConductance - conductance[row])) / denominator;
        for (std::size_t column = 0; column < row; ++column) {
            const double coupling = -source.ohms * conductance[row] * conductance[column] / denominator;
            matrix[row][column] += coupling;
            matrix[column][row] += coupling;
        }
    }
}

inline double terminal(Source source, const Vector& conductance, const Vector& volts)
{
    double totalConductance = 0, weightedVolts = 0;
    for (std::size_t pin = 0; pin < 16; ++pin) {
        totalConductance += conductance[pin];
        weightedVolts += conductance[pin] * volts[pin];
    }
    const double result = (source.volts + source.ohms * weightedVolts)
        / (1 + source.ohms * totalConductance);
    require(std::isfinite(result));
    return result;
}

// Positive grounded shunts make this conditional passive matrix SPD. Cholesky
// is a numerical method only, not a claim about analog dynamics.
inline Vector solvePositive(const Matrix& matrix, const Vector& rhs)
{
    Matrix lower{};
    for (std::size_t row = 0; row < 16; ++row) {
        for (std::size_t column = 0; column <= row; ++column) {
            double residual = matrix[row][column];
            for (std::size_t k = 0; k < column; ++k)
                residual -= lower[row][k] * lower[column][k];
            if (row == column) {
                require(std::isfinite(residual) && residual > 0);
                lower[row][column] = std::sqrt(residual);
            } else {
                lower[row][column] = residual / lower[column][column];
            }
        }
    }
    Vector intermediate{}, result{};
    for (std::size_t row = 0; row < 16; ++row) {
        double residual = rhs[row];
        for (std::size_t column = 0; column < row; ++column)
            residual -= lower[row][column] * intermediate[column];
        intermediate[row] = residual / lower[row][row];
    }
    for (int row = 15; row >= 0; --row) {
        double residual = intermediate[row];
        for (int column = row + 1; column < 16; ++column)
            residual -= lower[column][row] * result[column];
        result[row] = residual / lower[row][row];
        require(std::isfinite(result[row]));
    }
    return result;
}
} // namespace rank_detail

inline RankResult solveRank(const RankInputs& inputs)
{
    using namespace rank_detail;
    Matrix matrix{};
    Vector rhs{};
    for (std::size_t voice = 0; voice < 8; ++voice) {
        const auto& parameters = inputs.voices[voice];
        validate(parameters.hpReceiver);
        validate(parameters.lpReceiver);
        matrix[2 * voice][2 * voice] = 1 / shuntOhms(parameters.hpGrade, true)
            + parameters.hpReceiver.siemens;
        matrix[2 * voice + 1][2 * voice + 1] = 1 / shuntOhms(parameters.lpGrade, false)
            + parameters.lpReceiver.siemens;
        rhs[2 * voice] = -parameters.hpReceiver.amps;
        rhs[2 * voice + 1] = -parameters.lpReceiver.amps;
        for (std::size_t local = 0; local < 5; ++local)
            sourceStamp(parameters.local[local], localConductances(voice, local), matrix, rhs);
        Vector hpTrimConductance{}, lpTrimConductance{};
        hpTrimConductance[2 * voice] = 1 / 150e3;
        lpTrimConductance[2 * voice + 1] = 1 / 150e3;
        sourceStamp(parameters.hpTrim, hpTrimConductance, matrix, rhs);
        sourceStamp(parameters.lpTrim, lpTrimConductance, matrix, rhs);
    }
    for (std::size_t bus = 0; bus < 4; ++bus)
        sourceStamp(inputs.buses[bus], busConductances(bus), matrix, rhs);

    RankResult result{};
    result.pinVolts = solvePositive(matrix, rhs);
    for (std::size_t voice = 0; voice < 8; ++voice) {
        for (std::size_t local = 0; local < 5; ++local) {
            result.localTerminalVolts[voice][local] = terminal(inputs.voices[voice].local[local],
                localConductances(voice, local), result.pinVolts);
        }
    }
    for (std::size_t bus = 0; bus < 4; ++bus)
        result.busTerminalVolts[bus] = terminal(inputs.buses[bus], busConductances(bus), result.pinVolts);
    return result;
}
} // namespace cs80::reference::loaded_control
