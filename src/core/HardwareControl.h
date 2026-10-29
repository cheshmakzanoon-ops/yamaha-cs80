#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <string_view>

namespace cs80::hardware
{
// Source snapshots: docs/PRESET_CIRCUIT_LEDGER.md and PRESET_FULL_MATRIX_AUDIT.md.
// All22 tone columns; only independently verified, unmarked cells are classified.
// These are electrical branches, NOT slider positions, envelope times, IC input
// voltages, or calibrated factory patches. No connection to the audio engine.
inline constexpr std::string_view t51Source =
    "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/31%20-%20T51%20Circuit%20Diagram.jpg";
inline constexpr std::string_view t53Source =
    "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/35%20-%20T53%20Circuit%20Diagram.jpg";
inline constexpr std::string_view t52Source =
    "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/33%20-%20T52%20Circuit%20Diagram.jpg";
inline constexpr std::string_view t54Source =
    "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/37%20-%20T54%20Circuit%20Diagram.jpg";
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

inline constexpr std::array<Column, 22> verifiedColumns {{
    { 51, 'A', "String 1", t51Source },
    { 51, 'C', "Brass 1", t51Source },
    { 51, 'D', "Flute", t51Source },
    { 53, 'A', "String 2", t53Source },
    { 53, 'C', "Brass 2", t53Source },
    { 53, 'D', "Brass 3", t53Source },
    { 51, 'B', "String 3", t51Source },
    { 51, 'E', "Electric Piano", t51Source },
    { 51, 'F', "Clavichord 1", t51Source },
    { 52, 'A', "Harpsichord 1", t52Source },
    { 52, 'B', "Organ 1", t52Source },
    { 52, 'C', "Guitar 1", t52Source },
    { 52, 'D', "Funky 1", t52Source },
    { 52, 'E', "Funky 3", t52Source },
    { 53, 'B', "String 4", t53Source },
    { 53, 'E', "Bass", t53Source },
    { 53, 'F', "Clavichord 2", t53Source },
    { 54, 'A', "Harpsichord 2", t54Source },
    { 54, 'B', "Organ 2", t54Source },
    { 54, 'C', "Guitar 2", t54Source },
    { 54, 'D', "Funky 2", t54Source },
    { 54, 'E', "Funky 4", t54Source }
}};

// Metadata describes the drawn local shunt only, not total receiver loading.
enum class LocalShunt { unknown, none, tenK };

struct OutputRow
{
    int output;
    std::string_view control;
    std::array<Branch, 22> branches; // verifiedColumns order
    std::array<LocalShunt, 4> localShunts; // board51,52,53,54; not shared-bus loading
};

// 547 classified cells;25 revision/readability exclusions remain unknown.
// T52/T54 primed-terminal routing is revision-uncertain, not solved here.
inline constexpr std::array<OutputRow, 26> verifiedOutputs {{
    { 1, "Speed", { openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, kiloOhms(18), kiloOhms(7.5), openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 2, "PWM", { openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, kiloOhms(150), openBranch, openBranch, openBranch, openBranch, kiloOhms(110), kiloOhms(120), openBranch, openBranch, kiloOhms(110), openBranch, openBranch, openBranch, openBranch, openBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 3, "PW", { openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, kiloOhms(20), kiloOhms(4.3), kiloOhms(7.5), openBranch, kiloOhms(13), kiloOhms(6.2), kiloOhms(8.2), openBranch, kiloOhms(7.5), directBranch, kiloOhms(7.5), openBranch, openBranch, openBranch, openBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 4, "Square switch", { openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, directBranch, directBranch, directBranch, openBranch, directBranch, directBranch, directBranch, openBranch, directBranch, directBranch, directBranch, directBranch, openBranch, openBranch, openBranch },
      { LocalShunt::none, LocalShunt::none, LocalShunt::none, LocalShunt::none } },
    { 5, "Saw switch", { directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, openBranch, directBranch, openBranch, directBranch, openBranch, directBranch, openBranch, directBranch, openBranch, directBranch, openBranch, openBranch, directBranch, directBranch, directBranch },
      { LocalShunt::none, LocalShunt::none, LocalShunt::none, LocalShunt::none } },
    { 6, "Noise", { openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch, openBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 7, "HPF", { kiloOhms(6.8), openBranch, kiloOhms(51), kiloOhms(6.2), openBranch, kiloOhms(27), openBranch, kiloOhms(47), kiloOhms(6.2), kiloOhms(3.6), openBranch, kiloOhms(11), kiloOhms(8.2), kiloOhms(7.5), kiloOhms(27), openBranch, openBranch, kiloOhms(2.7), openBranch, kiloOhms(22), kiloOhms(7.5), kiloOhms(6.8) },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 8, "RESH", { kiloOhms(5.6), directBranch, kiloOhms(15), directBranch, directBranch, kiloOhms(18), directBranch, kiloOhms(4.7), kiloOhms(6.2), directBranch, directBranch, kiloOhms(6.8), kiloOhms(13), openBranch, directBranch, directBranch, directBranch, directBranch, directBranch, openBranch, kiloOhms(5.6), openBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 9, "LPF", { kiloOhms(1), kiloOhms(5.1), kiloOhms(7.5), ohms(750), kiloOhms(4.3), kiloOhms(13), directBranch, kiloOhms(33), kiloOhms(4.3), kiloOhms(1.8), unknownBranch, kiloOhms(4.3), kiloOhms(30), openBranch, kiloOhms(1.6), kiloOhms(12), openBranch, kiloOhms(1.8), kiloOhms(6.2), kiloOhms(18), kiloOhms(4.3), kiloOhms(6.2) },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 10, "RESL", { directBranch, kiloOhms(6.8), kiloOhms(30), directBranch, kiloOhms(3.6), kiloOhms(15), directBranch, kiloOhms(12), kiloOhms(10), kiloOhms(1.5), openBranch, kiloOhms(27), kiloOhms(11), openBranch, directBranch, openBranch, directBranch, directBranch, openBranch, openBranch, kiloOhms(18), kiloOhms(51) },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 11, "IL", { kiloOhms(16), kiloOhms(43), kiloOhms(9.1), openBranch, kiloOhms(9.1), kiloOhms(27), openBranch, openBranch, openBranch, openBranch, openBranch, kiloOhms(220), directBranch, directBranch, kiloOhms(4.7), kiloOhms(130), openBranch, openBranch, openBranch, openBranch, kiloOhms(3.9), openBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 12, "AL", { kiloOhms(3.6), kiloOhms(13), kiloOhms(130), openBranch, kiloOhms(20), kiloOhms(1.6), kiloOhms(5.1), kiloOhms(8.2), kiloOhms(10), openBranch, openBranch, kiloOhms(20), kiloOhms(1.6), directBranch, kiloOhms(30), kiloOhms(3.3), directBranch, openBranch, openBranch, kiloOhms(2.4), kiloOhms(4.7), directBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 13, "VCF attack", { kiloOhms(30), kiloOhms(62), kiloOhms(18), directBranch, kiloOhms(120), kiloOhms(15), kiloOhms(75), directBranch, directBranch, directBranch, directBranch, directBranch, ohms(510), openBranch, kiloOhms(8.2), directBranch, directBranch, directBranch, directBranch, directBranch, kiloOhms(16), kiloOhms(11) },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 14, "VCF decay", { kiloOhms(12), kiloOhms(51), kiloOhms(51), directBranch, kiloOhms(13), kiloOhms(12), kiloOhms(39), kiloOhms(18), kiloOhms(33), directBranch, directBranch, kiloOhms(10), kiloOhms(3), openBranch, unknownBranch, kiloOhms(8.2), kiloOhms(22), directBranch, directBranch, kiloOhms(36), kiloOhms(8.2), kiloOhms(15) },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 15, "VCF release", { kiloOhms(1.2), directBranch, kiloOhms(3.6), directBranch, directBranch, kiloOhms(3.9), kiloOhms(11), directBranch, kiloOhms(3.3), directBranch, directBranch, directBranch, kiloOhms(13), openBranch, kiloOhms(3.3), kiloOhms(1.6), directBranch, directBranch, directBranch, kiloOhms(3.9), directBranch, directBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 16, "VCF level", { directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 17, "Sine", { kiloOhms(220), kiloOhms(220), openBranch, openBranch, openBranch, openBranch, openBranch, kiloOhms(10), openBranch, openBranch, openBranch, openBranch, kiloOhms(9.1), kiloOhms(3.6), openBranch, openBranch, openBranch, openBranch, openBranch, kiloOhms(6.8), openBranch, directBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 18, "VCA attack", { kiloOhms(62), kiloOhms(8.2), kiloOhms(22), kiloOhms(2.2), kiloOhms(9.1), kiloOhms(8.2), kiloOhms(150), kiloOhms(3.3), directBranch, directBranch, directBranch, kiloOhms(3.3), directBranch, directBranch, kiloOhms(13), kiloOhms(4.3), directBranch, directBranch, directBranch, directBranch, kiloOhms(3.6), directBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 19, "VCA decay", { kiloOhms(82), kiloOhms(39), openBranch, kiloOhms(47), kiloOhms(36), kiloOhms(13), openBranch, kiloOhms(36), kiloOhms(20), kiloOhms(36), openBranch, kiloOhms(16), kiloOhms(11), openBranch, directBranch, kiloOhms(33), kiloOhms(11), kiloOhms(16), kiloOhms(5.6), kiloOhms(82), kiloOhms(13), kiloOhms(11) },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 20, "VCA sustain", { unknownBranch, kiloOhms(6.2), kiloOhms(9.1), unknownBranch, unknownBranch, kiloOhms(11), kiloOhms(7.5), kiloOhms(160), kiloOhms(68), openBranch, kiloOhms(2.7), kiloOhms(51), kiloOhms(10), unknownBranch, unknownBranch, openBranch, kiloOhms(43), openBranch, kiloOhms(39), kiloOhms(330), kiloOhms(33), kiloOhms(12) },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 21, "VCA release", { kiloOhms(1.3), directBranch, kiloOhms(3.9), kiloOhms(4.7), directBranch, kiloOhms(5.1), kiloOhms(9.1), kiloOhms(5.6), directBranch, kiloOhms(3.6), directBranch, kiloOhms(5.6), kiloOhms(3), kiloOhms(4.7), kiloOhms(2.7), directBranch, kiloOhms(2), directBranch, directBranch, kiloOhms(2.7), directBranch, directBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 22, "VCA level", { directBranch, directBranch, directBranch, directBranch, directBranch, kiloOhms(1.8), directBranch, unknownBranch, unknownBranch, directBranch, directBranch, unknownBranch, directBranch, directBranch, kiloOhms(1.8), directBranch, unknownBranch, directBranch, directBranch, unknownBranch, kiloOhms(4.7), directBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 23, "Initial brilliance", { kiloOhms(12), kiloOhms(18), kiloOhms(62), kiloOhms(22), kiloOhms(22), kiloOhms(22), kiloOhms(18), kiloOhms(15), kiloOhms(82), openBranch, openBranch, kiloOhms(22), kiloOhms(6.8), directBranch, kiloOhms(22), unknownBranch, kiloOhms(16), openBranch, openBranch, kiloOhms(22), kiloOhms(8.2), kiloOhms(12) },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 24, "Initial level", { directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 25, "After brilliance", { unknownBranch, unknownBranch, kiloOhms(62), unknownBranch, kiloOhms(10), kiloOhms(10), openBranch, openBranch, unknownBranch, openBranch, openBranch, openBranch, ohms(390), kiloOhms(16), kiloOhms(13), openBranch, openBranch, openBranch, openBranch, unknownBranch, openBranch, kiloOhms(5.6) },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } },
    { 26, "After level", { directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, directBranch, unknownBranch, unknownBranch, unknownBranch, directBranch, directBranch, directBranch, directBranch, directBranch, unknownBranch, unknownBranch, unknownBranch, directBranch, unknownBranch, directBranch, directBranch },
      { LocalShunt::tenK, LocalShunt::none, LocalShunt::tenK, LocalShunt::none } }
}};

// No interpolation, invented component designators, or assumed zero for missing
// cells. All25 revision/readability exclusions and invalid addresses remain unknown.
constexpr Branch findBranch(int board, char column, int output) noexcept
{
    for (std::size_t i = 0; i < verifiedColumns.size(); ++i)
        if (verifiedColumns[i].board == board && verifiedColumns[i].column == column)
            for (const auto& row : verifiedOutputs)
                if (row.output == output)
                    return row.branches[i];
    return unknownBranch;
}

// Known columns only. T52/T54 have NO drawn local shunts on any row;
// do not infer their companion-board load. Board lookup is bounded by the
// verifiedColumns match before indexing the four-element localShunts array.
constexpr LocalShunt findLocalShunt(int board, char column, int output) noexcept
{
    for (const auto& c : verifiedColumns)
        if (c.board == board && c.column == column)
            for (const auto& row : verifiedOutputs)
                if (row.output == output)
                    return row.localShunts[static_cast<std::size_t>(board - 51)];
    return LocalShunt::unknown;
}

inline constexpr double busShuntOhms = 10000.0;

struct BusConditions
{
    double selectedColumnVolts; // Caller must supply actual/assumed BA output.
    double diodeDropVolts;      // Caller assumption, NOT a modeled 1S1555 I-V.
    std::optional<double> groundLoadOhms = std::nullopt;
};

enum class SolveStatus { solved, unknownBranch, invalidInput, unresolvedLoading };

struct BusResult
{
    SolveStatus status;
    std::optional<double> volts; // Unresolved/invalid results have no voltage.
};

// GENERIC ILLUSTRATIVE HELPER, not matrix-aware: explicitly assumes 10k.
// Use solveMatrixBus for source-table addresses (local loading is board-aware).
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
// Matrix-aware conditional DC calculation, NOT a solved physical receiver.
// Retains all solveBus ideal-source/constant-diode/no-alternate-source assumptions.
// Local shunts are not evidence of total loaded impedance. Unknown selection,
// reference-tied loads, IC conversion and nonlinear loading remain outside scope.
inline BusResult solveMatrixBus(int board, char column, int output,
                                const BusConditions& conditions) noexcept
{
    const auto branch = findBranch(board, column, output);
    // Reuse generic input validation, but never use its 10k result on a no-shunt row.
    const auto checked = solveBus(branch, conditions);
    if (checked.status != SolveStatus::solved)
        return checked;
    const auto shunt = findLocalShunt(board, column, output);
    if (shunt == LocalShunt::tenK)
        return checked;
    if (shunt != LocalShunt::none)
        return { SolveStatus::unknownBranch, std::nullopt };
    // Conservatively require explicit loading even for an ideal direct drive:
    // actual receiver/leakage, shared-bus loading and source strength are not established.
    if (!conditions.groundLoadOhms)
        return { SolveStatus::unresolvedLoading, std::nullopt };
    if (branch.kind == BranchKind::open)
        return { SolveStatus::solved, 0.0 };
    const double drive = conditions.selectedColumnVolts > conditions.diodeDropVolts
                       ? conditions.selectedColumnVolts - conditions.diodeDropVolts : 0.0;
    const double series = branch.kind == BranchKind::resistor ? branch.resistanceOhms() : 0.0;
    // Only the caller's ground load: no invented 10k on T52/T54 or switch rows.
    return { SolveStatus::solved, drive / (1.0 + series / *conditions.groundLoadOhms) };
}
} // namespace cs80::hardware
