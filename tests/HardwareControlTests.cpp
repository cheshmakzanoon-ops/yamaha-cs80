#include "core/HardwareControl.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <string>

using namespace cs80::hardware;

namespace
{
int checks = 0;
int failures = 0;

void check(bool condition, const std::string& label)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::printf("FAIL: %s\n", label.c_str());
    }
}

void near(const BusResult& result, double expected, const std::string& label,
          double tolerance = 1.0e-10)
{
    check(result.status == SolveStatus::solved && result.volts.has_value()
          && std::abs(*result.volts - expected) <= tolerance, label);
}

void invalid(Branch branch, BusConditions conditions, const std::string& label)
{
    const auto result = solveBus(branch, conditions);
    check(result.status == SolveStatus::invalidInput && !result.volts, label);
}
}

int main()
{
    // Explicit illustrative assumptions from the ledger, NOT measured hardware.
    const BusConditions illustration { 10.6, 0.6, std::nullopt };
    check(verifiedColumns.size() == 6 && verifiedOutputs.size() == 11, "sample dimensions");
    check(busShuntOhms == 10000.0, "single 10k bus shunt");
    constexpr std::array<int, 11> outputs { 7, 8, 9, 10, 11, 12, 13, 14, 15, 18, 19 };
    // Independent ledger transcription in ohms: -1 = open, 0 = direct.
    constexpr double expected[11][6] {
        { 6800, -1, 51000, 6200, -1, 27000 },
        { 5600, 0, 15000, 0, 0, 18000 },
        { 1000, 5100, 7500, 750, 4300, 13000 },
        { 0, 6800, 30000, 0, 3600, 15000 },
        { 16000, 43000, 9100, -1, 9100, 27000 },
        { 3600, 13000, 130000, -1, 20000, 1600 },
        { 30000, 62000, 18000, 0, 120000, 15000 },
        { 12000, 51000, 51000, 0, 13000, 12000 },
        { 1200, 0, 3600, 0, 0, 3900 },
        { 62000, 8200, 22000, 2200, 9100, 8200 },
        { 82000, 39000, -1, 47000, 36000, 13000 }
    };
    for (std::size_t row = 0; row < outputs.size(); ++row)
        for (std::size_t col = 0; col < verifiedColumns.size(); ++col)
        {
            const auto& column = verifiedColumns[col];
            const auto branch = findBranch(column.board, column.column, outputs[row]);
            const std::string locator = "T" + std::to_string(column.board) + "/"
                                      + column.column + "/" + std::to_string(outputs[row]);
            const double resistance = expected[row][col];
            const auto kind = resistance < 0 ? BranchKind::open
                            : resistance == 0 ? BranchKind::direct : BranchKind::resistor;
            check(branch.kind == kind, locator + " branch kind");
            if (resistance > 0)
                check(std::abs(branch.resistanceOhms() - resistance) < 1.0e-8,
                      locator + " source resistance in ohms");
            near(solveBus(branch, illustration),
                 resistance < 0 ? 0.0 : 100000.0 / (resistance + 10000.0),
                 locator + " conditional divider");
        }

    // Independent decimal calculations for the two filter rows.
    constexpr std::array<double, 6> hpf {
        5.952380952380952, 0.0, 1.639344262295082, 6.172839506172839, 0.0, 2.702702702702703
    };
    constexpr std::array<double, 6> lpf {
        9.090909090909091, 6.622516556291391, 5.714285714285714,
        9.302325581395349, 6.993006993006993, 4.347826086956522
    };
    for (std::size_t i = 0; i < verifiedColumns.size(); ++i)
    {
        const auto& c = verifiedColumns[i];
        near(solveBus(findBranch(c.board, c.column, 7), illustration), hpf[i], "HPF ledger example");
        near(solveBus(findBranch(c.board, c.column, 9), illustration), lpf[i], "LPF ledger example");
        check(!c.tone.empty() && c.source == (c.board == 51 ? t51Source : t53Source),
              "tone source attribution");
    }
    // Native T51 source independently re-read by two reviewers: C/output12
    // is 13K, not the earlier 15K transcription. Still a conditional DC model.
    const auto brass1AL = findBranch(51, 'C', 12);
    check(brass1AL.kind == BranchKind::resistor && brass1AL.sourceValue == 13.0
          && brass1AL.sourceUnit == ResistanceUnit::kiloOhms,
          "T51/C/12 native-source correction preserves printed 13K");
    near(solveBus(brass1AL, illustration), 4.347826086956522,
         "T51/C/12 corrected conditional divider, not an authenticated AL parameter");

    const auto printed750 = findBranch(53, 'A', 9);
    check(printed750.sourceValue == 750.0 && printed750.sourceUnit == ResistanceUnit::ohms,
          "preserve printed 750 ohms, not 750 kilohms");
    const auto printed68 = findBranch(51, 'A', 7);
    check(printed68.sourceValue == 6.8 && printed68.sourceUnit == ResistanceUnit::kiloOhms,
          "preserve printed kilo-ohm notation");
    check(ohms(750).resistanceOhms() == kiloOhms(0.75).resistanceOhms(), "unit equivalence");

    for (const auto branch : { findBranch(52, 'A', 7), findBranch(54, 'A', 7),
                               findBranch(51, 'B', 7), findBranch(53, 'F', 9),
                               findBranch(51, 'A', 1), findBranch(51, 'A', 20),
                               findBranch(0, '?', -1), unknownBranch })
    {
        const auto result = solveBus(branch, illustration);
        check(branch.kind == BranchKind::unknown && result.status == SolveStatus::unknownBranch
              && !result.volts, "untranscribed is unresolved, not open");
    }
    near(solveBus(openBranch, illustration), 0.0, "open bus has shunt but no selected branch");
    near(solveBus(directBranch, illustration), 10.0, "direct still subtracts supplied diode drop");
    near(solveBus(directBranch, { 10.6, 0.6, 10000.0 }), 10.0,
         "ideal direct source unaffected by finite ground load");
    near(solveBus(kiloOhms(10), illustration), 5.0, "unloaded 10k divider");
    near(solveBus(kiloOhms(10), { 10.6, 0.6, 10000.0 }), 10.0 / 3.0, "parallel load");
    near(solveBus(kiloOhms(10), { 10.6, 0.6, 5000.0 }), 2.5, "stronger ground load");
    near(solveBus(kiloOhms(10), { 10.0, 0.0 }), 5.0, "supplied zero diode drop");
    near(solveBus(kiloOhms(10), { 0.5, 0.6 }), 0.0, "below constant-drop threshold");
    near(solveBus(directBranch, { 0.6, 0.6 }), 0.0, "at threshold");
    near(solveBus(openBranch, { 0.0, 0.6 }), 0.0, "open at zero source");

    double previous = 11.0;
    for (double rs : { 1.0, 750.0, 1000.0, 10000.0, 130000.0, 1.0e9 })
    {
        const double voltage = *solveBus(ohms(rs), illustration).volts;
        check(voltage >= 0.0 && voltage < previous, "voltage decreases with series resistance");
        previous = voltage;
    }
    previous = -1.0;
    for (double supply : { 0.0, 0.6, 1.0, 5.0, 10.6, 15.0 })
    {
        const double voltage = *solveBus(kiloOhms(10), { supply, 0.6 }).volts;
        check(voltage >= previous, "voltage nondecreasing with source");
        previous = voltage;
    }
    previous = 11.0;
    for (double drop : { 0.0, 0.2, 0.6, 1.0, 10.6, 12.0 })
    {
        const double voltage = *solveBus(kiloOhms(10), { 10.6, drop }).volts;
        check(voltage <= previous && voltage >= 0.0, "voltage nonincreasing with diode drop");
        previous = voltage;
    }
    previous = -1.0;
    for (double load : { 1.0, 100.0, 1000.0, 10000.0, 1.0e9 })
    {
        const double voltage = *solveBus(kiloOhms(10), { 10.6, 0.6, load }).volts;
        check(voltage > previous && voltage < 5.0, "weaker ground load raises voltage");
        previous = voltage;
    }

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const double max = std::numeric_limits<double>::max();
    for (double bad : { -1.0, nan, inf, -inf })
    {
        invalid(kiloOhms(10), { bad, 0.6 }, "invalid selected-column voltage");
        invalid(kiloOhms(10), { 10.6, bad }, "invalid diode drop");
        invalid(kiloOhms(10), { 10.6, 0.6, bad }, "invalid finite ground load");
        invalid(ohms(bad), illustration, "invalid resistance");
    }
    invalid(kiloOhms(10), { 10.6, 0.6, 0.0 }, "zero load outside finite-positive-load model");
    invalid(ohms(0), illustration, "zero resistor must be explicit direct");
    invalid(kiloOhms(max), illustration, "resistance unit conversion overflow");
    invalid({ static_cast<BranchKind>(99), 0.0 }, illustration, "invalid branch kind");
    invalid({ BranchKind::resistor, 1.0, static_cast<ResistanceUnit>(99) },
            illustration, "invalid resistance unit");
    for (auto kind : { BranchKind::unknown, BranchKind::open, BranchKind::direct })
        invalid({ kind, 1.0 }, illustration, "non-resistor cannot carry resistance");
    invalid(unknownBranch, { nan, 0.6 }, "invalid input precedes unknown status");
    invalid(openBranch, { 10.6, 0.6, inf }, "open does not bypass validation");
    near(solveBus(kiloOhms(10), { 10.6, 0.6, max }), 5.0, "huge finite load avoids overflow");
    near(solveBus(directBranch, { max, 0.0 }), max, "finite extreme direct source");
    near(solveBus(directBranch, { 10.6, 0.6, std::numeric_limits<double>::denorm_min() }),
         10.0, "tiny finite load preserves ideal direct source");

    std::printf("Hardware control: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
