#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <string_view>

namespace cs80::hardware
{
// Source snapshot: docs/PRESET_CIRCUIT_LEDGER.md, verified sample transcription.
// These are electrical branches, NOT slider positions, envelope times, IC input
// voltages, or calibrated factory patches. No connection to the audio engine.
inline constexpr std::string_view t51Source =
    "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/31%20-%20T51%20Circuit%20Diagram.jpg";
inline constexpr std::string_view t53Source =
    "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/35%20-%20T53%20Circuit%20Diagram.jpg";
inline constexpr std::string_view columnSource =
    "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/cs80_foldout_schematic.jpg";
inline constexpr std::string_view controlNameSource =
    "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/62%20-%20Panel%201%20Circuit%20Diagram.jpg";

enum class BranchKind { unknown, open, direct, resistor };
enum class ResistanceUnit { ohms, kiloOhms };

// sourceValue/sourceUnit preserve the actual printed notation (notably 750 ohms).
// Non-resistor branches have no resistance value; direct still includes a diode.
struct Branch
{
    BranchKind kind = BranchKind::unknown;
    double sourceValue = 0.0;
    ResistanceUnit sourceUnit = ResistanceUnit::ohms;

    constexpr double resistanceOhms() const noexcept
    {
        return sourceValue * (sourceUnit == ResistanceUnit::kiloOhms ? 1000.0 : 1.0);
    }
};

constexpr Branch kiloOhms(double value) noexcept
{
    return { BranchKind::resistor, value, ResistanceUnit::kiloOhms };
}
constexpr Branch ohms(double value) noexcept
{
    return { BranchKind::resistor, value, ResistanceUnit::ohms };
}
inline constexpr Branch unknownBranch {};
inline constexpr Branch openBranch { BranchKind::open };
inline constexpr Branch directBranch { BranchKind::direct };

struct Column
{
    int board;
    char column;
    std::string_view tone;
    std::string_view source;
};

inline constexpr std::array<Column, 6> verifiedColumns {{
    { 51, 'A', "String 1", t51Source },
    { 51, 'C', "Brass 1", t51Source },
    { 51, 'D', "Flute", t51Source },
    { 53, 'A', "String 2", t53Source },
    { 53, 'C', "Brass 2", t53Source },
    { 53, 'D', "Brass 3", t53Source }
}};

struct OutputRow
{
    int output;
    std::string_view control;
    std::array<Branch, 6> branches; // verifiedColumns order
};

inline constexpr std::array<OutputRow, 11> verifiedOutputs {{
    { 7, "HPF", { kiloOhms(6.8), openBranch, kiloOhms(51), kiloOhms(6.2), openBranch, kiloOhms(27) } },
    { 8, "RESH", { kiloOhms(5.6), directBranch, kiloOhms(15), directBranch, directBranch, kiloOhms(18) } },
    { 9, "LPF", { kiloOhms(1), kiloOhms(5.1), kiloOhms(7.5), ohms(750), kiloOhms(4.3), kiloOhms(13) } },
    { 10, "RESL", { directBranch, kiloOhms(6.8), kiloOhms(30), directBranch, kiloOhms(3.6), kiloOhms(15) } },
    { 11, "IL", { kiloOhms(16), kiloOhms(43), kiloOhms(9.1), openBranch, kiloOhms(9.1), kiloOhms(27) } },
    { 12, "AL", { kiloOhms(3.6), kiloOhms(15), kiloOhms(130), openBranch, kiloOhms(20), kiloOhms(1.6) } },
    { 13, "VCF attack", { kiloOhms(30), kiloOhms(62), kiloOhms(18), directBranch, kiloOhms(120), kiloOhms(15) } },
    { 14, "VCF decay", { kiloOhms(12), kiloOhms(51), kiloOhms(51), directBranch, kiloOhms(13), kiloOhms(12) } },
    { 15, "VCF release", { kiloOhms(1.2), directBranch, kiloOhms(3.6), directBranch, directBranch, kiloOhms(3.9) } },
    { 18, "VCA attack", { kiloOhms(62), kiloOhms(8.2), kiloOhms(22), kiloOhms(2.2), kiloOhms(9.1), kiloOhms(8.2) } },
    { 19, "VCA decay", { kiloOhms(82), kiloOhms(39), openBranch, kiloOhms(47), kiloOhms(36), kiloOhms(13) } }
}};

// No interpolation, invented component designators, or assumed zero for missing
// rows/columns. T52/T54 and all untranscribed outputs remain unknown.
constexpr Branch findBranch(int board, char column, int output) noexcept
{
    for (std::size_t i = 0; i < verifiedColumns.size(); ++i)
        if (verifiedColumns[i].board == board && verifiedColumns[i].column == column)
            for (const auto& row : verifiedOutputs)
                if (row.output == output)
                    return row.branches[i];
    return unknownBranch;
}

inline constexpr double busShuntOhms = 10000.0;

struct BusConditions
{
    double selectedColumnVolts; // Caller must supply actual/assumed BA output.
    double diodeDropVolts;      // Caller assumption, NOT a modeled 1S1555 I-V.
    std::optional<double> groundLoadOhms = std::nullopt;
};

enum class SolveStatus { solved, unknownBranch, invalidInput };

struct BusResult
{
    SolveStatus status;
    std::optional<double> volts; // Unresolved/invalid results have no voltage.
};

// Deliberately narrow model:
// - one selected ideal voltage source and one constant-drop isolation diode;
// - ONE 10k shunt to E, not duplicate shunts from companion boards;
// - optional positive finite linear load to E (ground);
// - no alternate/primed inputs, leakage, BA source impedance or diode I-V;
// - no receiver reference/gain/polarity or M-board IC conversion.
// A diode below its supplied constant-drop threshold is treated as nonconducting.
// The absence of alternate sources is essential to an open branch's zero result.
inline BusResult solveBus(Branch branch, const BusConditions& conditions) noexcept
{
    const auto invalid = BusResult { SolveStatus::invalidInput, std::nullopt };
    if (!std::isfinite(conditions.selectedColumnVolts) || conditions.selectedColumnVolts < 0.0
        || !std::isfinite(conditions.diodeDropVolts) || conditions.diodeDropVolts < 0.0)
        return invalid;
    if (conditions.groundLoadOhms
        && (!std::isfinite(*conditions.groundLoadOhms) || *conditions.groundLoadOhms <= 0.0))
        return invalid;
    if (branch.sourceUnit != ResistanceUnit::ohms && branch.sourceUnit != ResistanceUnit::kiloOhms)
        return invalid;
    if (!std::isfinite(branch.sourceValue) || branch.sourceValue < 0.0)
        return invalid;

    double seriesOhms = 0.0;
    switch (branch.kind)
    {
        case BranchKind::unknown:
            if (branch.sourceValue != 0.0) return invalid;
            return { SolveStatus::unknownBranch, std::nullopt };
        case BranchKind::open:
            if (branch.sourceValue != 0.0) return invalid;
            return { SolveStatus::solved, 0.0 };
        case BranchKind::direct:
            if (branch.sourceValue != 0.0) return invalid;
            break;
        case BranchKind::resistor:
            seriesOhms = branch.resistanceOhms();
            if (!std::isfinite(seriesOhms) || seriesOhms <= 0.0) return invalid;
            break;
        default:
            return invalid;
    }

    const double drive = conditions.selectedColumnVolts > conditions.diodeDropVolts
                       ? conditions.selectedColumnVolts - conditions.diodeDropVolts : 0.0;
    double shunt = busShuntOhms;
    if (conditions.groundLoadOhms)
    {
        const double load = *conditions.groundLoadOhms;
        // Avoid multiplying resistances, which can overflow for finite loads.
        shunt = load / (1.0 + load / busShuntOhms);
    }
    return { SolveStatus::solved, drive * (shunt / (seriesOhms + shunt)) };
}
} // namespace cs80::hardware
