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
    constexpr std::array<Column, 6> sampleColumns {{
        verifiedColumns[0],verifiedColumns[1],verifiedColumns[2],
        verifiedColumns[3],verifiedColumns[4],verifiedColumns[5]
    }};
    check(verifiedColumns.size() == 22, "all22 factory tone columns");
    const BusConditions illustration { 10.6, 0.6, std::nullopt };
    check(sampleColumns.size() == 6 && verifiedOutputs.size() == 26, "sample dimensions");
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
        for (std::size_t col = 0; col < sampleColumns.size(); ++col)
        {
            const auto& column = sampleColumns[col];
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


    // Independently reviewed supplemental source cells. -2 means unresolved
    // revision, -1 means absent branch, 0 means direct diode, others are ohms.
    constexpr std::array<int, 15> addedOutputs { 1,2,3,4,5,6,16,17,20,21,22,23,24,25,26 };
    constexpr double added[15][6] {
        {-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1},
        {-1,-1,-1,-1,-1,-1}, {0,0,0,0,0,0}, {-1,-1,-1,-1,-1,-1},
        {0,0,0,0,0,0}, {220000,220000,-1,-1,-1,-1},
        {-2,6200,9100,-2,-2,11000},
        {1300,0,3900,4700,0,5100}, {0,0,0,0,0,1800},
        {12000,18000,62000,22000,22000,22000},
        {0,0,0,0,0,0}, {-2,-2,62000,-2,10000,10000}, {0,0,0,0,0,0}
    };
    int addedKnown = 0, addedUnknown = 0;
    for (std::size_t row = 0; row < addedOutputs.size(); ++row)
        for (std::size_t col = 0; col < sampleColumns.size(); ++col)
        {
            const auto& c = sampleColumns[col];
            const int output = addedOutputs[row];
            const double expectedValue = added[row][col];
            const auto branch = findBranch(c.board, c.column, output);
            const auto kind = expectedValue == -2 ? BranchKind::unknown
                            : expectedValue == -1 ? BranchKind::open
                            : expectedValue == 0 ? BranchKind::direct : BranchKind::resistor;
            check(branch.kind == kind, "supplemental source branch classification");
            if (expectedValue > 0)
                check(branch.resistanceOhms() == expectedValue
                      && branch.sourceUnit == ResistanceUnit::kiloOhms,
                      "supplemental source printed kilo-ohms");
            const bool noShunt = output == 4 || output == 5;
            check(findLocalShunt(c.board, c.column, output)
                      == (noShunt ? LocalShunt::none : LocalShunt::tenK),
                  "independent row-local shunt fixture");
            const auto result = solveMatrixBus(c.board, c.column, output, illustration);
            if (expectedValue == -2)
            {
                ++addedUnknown;
                check(result.status == SolveStatus::unknownBranch && !result.volts,
                      "revision uncertainty remains unresolved");
                const auto loaded = solveMatrixBus(c.board, c.column, output, {10.6,0.6,5000.0});
                check(loaded.status == SolveStatus::unknownBranch && !loaded.volts,
                      "explicit load cannot resolve revision");
            }
            else
            {
                ++addedKnown;
                if (noShunt)
                {
                    check(result.status == SolveStatus::unresolvedLoading && !result.volts,
                          "no-shunt switch requires explicit receiver load");
                    near(solveMatrixBus(c.board, c.column, output, {10.6,0.6,5000.0}),
                         expectedValue == -1 ? 0.0 : 10.0,
                         "switch uses only explicit conditional ground load");
                    near(solveMatrixBus(c.board, c.column, output, {0.5,0.6,5000.0}),
                         0.0, "switch below diode threshold with explicit load");
                }
                else
                {
                    near(result, expectedValue == -1 ? 0.0 : 100000.0/(expectedValue+10000.0),
                         "supplemental analog conditional divider");
                    near(solveMatrixBus(c.board, c.column, output, {10.6,0.6,10000.0}),
                         expectedValue == -1 ? 0.0 : 50000.0/(expectedValue+5000.0),
                         "supplemental analog explicit parallel load");
                }
            }
        }
    check(addedKnown == 84 && addedUnknown == 6, "84 promoted cells and six held revisions");
    int totalKnown = 0, totalUnknown = 0;
    for (const auto& row : verifiedOutputs)
        for (const auto& branch : row.branches)
            branch.kind == BranchKind::unknown ? ++totalUnknown : ++totalKnown;
    check(totalKnown == 547 && totalUnknown == 25, "547 known /25 unresolved of572 addresses");
    // Unknown addresses do not inherit another board's shunt or source branch.
    for (int board : {0,50,55})
    {
        check(findLocalShunt(board,'A',4) == LocalShunt::unknown, "no cross-board shunt inference");
        const auto result = solveMatrixBus(board,'A',4,{10.6,0.6,10000.0});
        check(result.status == SolveStatus::unknownBranch && !result.volts, "unknown board not solved");
    }
    check(findLocalShunt(52,'F',4) == LocalShunt::unknown, "unknown column metadata");
    check(findLocalShunt(51,'A',27) == LocalShunt::unknown, "unknown output metadata");
    for (const auto& c : sampleColumns)
        for (int output : outputs)
        {
            check(findLocalShunt(c.board,c.column,output) == LocalShunt::tenK,
                  "original rows retain one local10k");
            const auto generic = solveBus(findBranch(c.board,c.column,output), illustration);
            near(solveMatrixBus(c.board,c.column,output,illustration), *generic.volts,
                 "matrix-aware original-row compatibility");
        }
    for (int output : {4,5,7,20})
    {
        for (double bad : {0.0,-1.0,std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()})
        {
            const auto result = solveMatrixBus(51,'A',output,{10.6,0.6,bad});
            check(result.status == SolveStatus::invalidInput && !result.volts,
                  "matrix load validation precedes topology or revision status");
        }
        const auto badSource = solveMatrixBus(51,'A',output,{-1.0,0.6});
        check(badSource.status == SolveStatus::invalidInput && !badSource.volts,
              "matrix invalid source validation");
        const auto badDiode = solveMatrixBus(51,'A',output,{10.6,-1.0});
        check(badDiode.status == SolveStatus::invalidInput && !badDiode.volts,
              "matrix invalid diode validation");
    }


    // Frozen fixture from separately visually reviewed rank-I/rank-II ledgers,
    // not computed from HardwareControl's branch objects. -2=excluded,
    // -1=absent branch,0=direct diode, positive values are ohms.
    constexpr std::array<Column,16> remainingColumns {{
        {51,'B',"String 3",t51Source},
        {51,'E',"Electric Piano",t51Source},
        {51,'F',"Clavichord 1",t51Source},
        {52,'A',"Harpsichord 1",t52Source},
        {52,'B',"Organ 1",t52Source},
        {52,'C',"Guitar 1",t52Source},
        {52,'D',"Funky 1",t52Source},
        {52,'E',"Funky 3",t52Source},
        {53,'B',"String 4",t53Source},
        {53,'E',"Bass",t53Source},
        {53,'F',"Clavichord 2",t53Source},
        {54,'A',"Harpsichord 2",t54Source},
        {54,'B',"Organ 2",t54Source},
        {54,'C',"Guitar 2",t54Source},
        {54,'D',"Funky 2",t54Source},
        {54,'E',"Funky 4",t54Source}
    }};
    constexpr double remaining[26][16] {
        {-1,-1,-1,-1,-1,-1,18000,7500,-1,-1,-1,-1,-1,-1,-1,-1},
        {-1,150000,-1,-1,-1,-1,110000,120000,-1,-1,110000,-1,-1,-1,-1,-1},
        {-1,20000,4300,7500,-1,13000,6200,8200,-1,7500,0,7500,-1,-1,-1,-1},
        {-1,0,0,0,-1,0,0,0,-1,0,0,0,0,-1,-1,-1},
        {0,-1,0,-1,0,-1,0,-1,0,-1,0,-1,-1,0,0,0},
        {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
        {-1,47000,6200,3600,-1,11000,8200,7500,27000,-1,-1,2700,-1,22000,7500,6800},
        {0,4700,6200,0,0,6800,13000,-1,0,0,0,0,0,-1,5600,-1},
        {0,33000,4300,1800,-2,4300,30000,-1,1600,12000,-1,1800,6200,18000,4300,6200},
        {0,12000,10000,1500,-1,27000,11000,-1,0,-1,0,0,-1,-1,18000,51000},
        {-1,-1,-1,-1,-1,220000,0,0,4700,130000,-1,-1,-1,-1,3900,-1},
        {5100,8200,10000,-1,-1,20000,1600,0,30000,3300,0,-1,-1,2400,4700,0},
        {75000,0,0,0,0,0,510,-1,8200,0,0,0,0,0,16000,11000},
        {39000,18000,33000,0,0,10000,3000,-1,-2,8200,22000,0,0,36000,8200,15000},
        {11000,0,3300,0,0,0,13000,-1,3300,1600,0,0,0,3900,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {-1,10000,-1,-1,-1,-1,9100,3600,-1,-1,-1,-1,-1,6800,-1,0},
        {150000,3300,0,0,0,3300,0,0,13000,4300,0,0,0,0,3600,0},
        {-1,36000,20000,36000,-1,16000,11000,-1,0,33000,11000,16000,5600,82000,13000,11000},
        {7500,160000,68000,-1,2700,51000,10000,-2,-2,-1,43000,-1,39000,330000,33000,12000},
        {9100,5600,0,3600,0,5600,3000,4700,2700,0,2000,0,0,2700,0,0},
        {0,-2,-2,0,0,-2,0,0,1800,0,-2,0,0,-2,4700,0},
        {18000,15000,82000,-1,-1,22000,6800,0,22000,-2,16000,-1,-1,22000,8200,12000},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {-1,-1,-2,-1,-1,-1,390,16000,13000,-1,-1,-1,-1,-2,-1,5600},
        {0,-2,-2,-2,0,0,0,0,0,-2,-2,-2,0,-2,0,0}
    };
    int rankIKnown = 0, rankIIKnown = 0, remainingUnknown = 0;
    for (std::size_t row = 0; row < 26; ++row)
        for (std::size_t col = 0; col < remainingColumns.size(); ++col)
        {
            const auto& c = remainingColumns[col];
            const int output = static_cast<int>(row + 1);
            const double value = remaining[row][col];
            const auto b = findBranch(c.board,c.column,output);
            const auto kind = value == -2 ? BranchKind::unknown
                            : value == -1 ? BranchKind::open
                            : value == 0 ? BranchKind::direct : BranchKind::resistor;
            const std::string address = "T" + std::to_string(c.board) + "/"
                                      + c.column + "/" + std::to_string(output);
            check(b.kind == kind, address + " independent remaining-cell kind");
            if (value > 0)
            {
                check(std::abs(b.resistanceOhms() - value) < 1.0e-8,
                      address + " independent resistance");
                const bool printedOhms = c.board == 52 && c.column == 'D'
                                      && (output == 13 || output == 25);
                check(b.sourceUnit == (printedOhms ? ResistanceUnit::ohms : ResistanceUnit::kiloOhms),
                      address + " printed units");
                check(std::abs(b.sourceValue - (printedOhms ? value : value / 1000.0)) < 1.0e-10,
                      address + " printed numeric value");
            }
            const bool noLocalShunt = c.board == 52 || c.board == 54 || output == 4 || output == 5;
            check(findLocalShunt(c.board,c.column,output)
                    == (noLocalShunt ? LocalShunt::none : LocalShunt::tenK),
                  address + " board-aware LOCAL loading only");
            const auto unloaded = solveMatrixBus(c.board,c.column,output,illustration);
            const auto loaded = solveMatrixBus(c.board,c.column,output,{10.6,0.6,5000.0});
            if (value == -2)
            {
                ++remainingUnknown;
                check(unloaded.status == SolveStatus::unknownBranch && !unloaded.volts
                      && loaded.status == SolveStatus::unknownBranch && !loaded.volts,
                      address + " excluded despite explicit load");
                continue;
            }
            if (col < 8) ++rankIKnown; else ++rankIIKnown;
            if (noLocalShunt)
                check(unloaded.status == SolveStatus::unresolvedLoading && !unloaded.volts,
                      address + " no invented companion shunt");
            else
                near(unloaded,value == -1 ? 0.0 : 100000.0/(value+10000.0),
                     address + " local10k conditional result");
            const double rp = noLocalShunt ? 5000.0 : 10000.0/3.0;
            near(loaded,value == -1 ? 0.0 : 10.0*rp/(value+rp),
                 address + " explicit ground load only plus LOCAL shunt");
            near(solveMatrixBus(c.board,c.column,output,{0.5,0.6,5000.0}),0.0,
                 address + " conditional nonconducting diode");
        }
    check(rankIKnown == 199 && rankIIKnown == 198 && remainingUnknown == 19,
          "397 newly classified and19 additional exclusions");
    // Distinguish no-local-shunt analog resistor paths from an invented10k.
    near(solveMatrixBus(52,'D',13,{10.6,0.6,10000.0}),100000.0/10510.0,
         "T52/D/13:510ohms with caller10k only");
    near(solveMatrixBus(52,'D',25,{10.6,0.6,10000.0}),100000.0/10390.0,
         "T52/D/25:390ohms with caller10k only");
    near(solveMatrixBus(54,'C',20,{10.6,0.6,10000.0}),100000.0/340000.0,
         "T54/C/20:330k no hidden companion10k");
    for (const auto& c : verifiedColumns)
    {
        check(!c.tone.empty() && !c.source.empty(), "all tones retain source attribution");
        for (int output = 1; output <= 26; ++output)
        {
            const auto local = findLocalShunt(c.board,c.column,output);
            check(local == ((c.board == 52 || c.board == 54 || output == 4 || output == 5)
                          ? LocalShunt::none : LocalShunt::tenK),
                  "exhaustive572 board-aware local-shunt check");
            const auto invalidLoad = solveMatrixBus(c.board,c.column,output,{10.6,0.6,0.0});
            check(invalidLoad.status == SolveStatus::invalidInput && !invalidLoad.volts,
                  "invalid load cannot bypass any topology or exclusion");
        }
    }
    for (std::size_t i = 0; i < verifiedColumns.size(); ++i)
        for (std::size_t j = i + 1; j < verifiedColumns.size(); ++j)
            check(verifiedColumns[i].board != verifiedColumns[j].board
                  || verifiedColumns[i].column != verifiedColumns[j].column,
                  "unique canonical board/local-column address");

    // Independent decimal calculations for the two filter rows.
    constexpr std::array<double, 6> hpf {
        5.952380952380952, 0.0, 1.639344262295082, 6.172839506172839, 0.0, 2.702702702702703
    };
    constexpr std::array<double, 6> lpf {
        9.090909090909091, 6.622516556291391, 5.714285714285714,
        9.302325581395349, 6.993006993006993, 4.347826086956522
    };
    for (std::size_t i = 0; i < sampleColumns.size(); ++i)
    {
        const auto& c = sampleColumns[i];
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

    for (const auto branch : { findBranch(52, 'F', 7), findBranch(54, 'F', 7),
                               findBranch(51, 'G', 7), findBranch(53, 'B', 14),
                               findBranch(51, 'A', 25), findBranch(51, 'A', 20),
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
