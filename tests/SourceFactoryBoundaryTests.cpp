// Versioned BA-driver / 1S1555 boundary refinement (SourceFactoryTones.h,
// compileToneRefined). Truth: exact solve of the selected column (one shared
// emitter, every row's diode branch and loaded bus) at each drive. Compiled:
// the per-row Thevenin equivalents, linearised at the reference operating
// point, that compileToneRefined feeds into the existing loaded graphs.
// Tolerances are read from the pre-registered
// tests/parity/tolerances-v2/ws6-library.json (stage ba_diode_boundary_v1).
// Model-consistency checks, not hardware evidence.
#include "core/SourceFactoryTones.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace sf = cs80::source_factory;

namespace
{
int failures = 0;
void check(bool ok, const std::string& message)
{
    if (!ok)
    {
        ++failures;
        if (failures < 40) std::printf("FAIL: %s\n", message.c_str());
    }
}

double tolerance(const std::string& json, const std::string& stage, const std::string& key)
{
    const auto at = json.find("\"" + stage + "\"");
    const auto k = at == std::string::npos ? at : json.find("\"" + key + "\"", at);
    if (k == std::string::npos) { std::printf("missing tolerance %s.%s\n", stage.c_str(), key.c_str()); std::exit(2); }
    return std::strtod(json.c_str() + json.find(':', k) + 1, nullptr);
}

// The column rows of a compiled tone, as the truth model sees them.
std::array<sf::ColumnRow, 26> columnOf(const sf::ToneControls& tone)
{
    std::array<sf::ColumnRow, 26> rows {};
    for (int output = 1; output <= 26; ++output)
    {
        if (output == 4 || output == 5) continue;
        const auto i = (size_t) (output - 1);
        const auto b = tone.hypotheses[i] != nullptr ? sf::hypothesisBranch(tone.hypothesisExcitations[i])
                                                     : cs80::hardware::findBranch(tone.source.board, tone.source.column, output);
        if (b.kind == cs80::hardware::BranchKind::open) continue;
        rows[i] = { true, b.kind == cs80::hardware::BranchKind::resistor ? b.resistanceOhms() : 0.0,
                    sf::compiledReceiverSiemens(output, tone.hpGrade, tone.lpGrade), 0.0 };
    }
    return rows;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) { std::printf("usage: %s tolerances.json\n", argv[0]); return 2; }
    std::ifstream in(argv[1]);
    std::stringstream text;
    text << in.rdbuf();
    const auto json = text.str();
    const double tolReference = tolerance(json, "ba_diode_boundary_v1", "reference_point_bus_volts");
    const double tolDrive = tolerance(json, "ba_diode_boundary_v1", "drive_range_bus_volts");
    const double tolResidual = tolerance(json, "ba_diode_boundary_v1", "newton_residual_amps");
    const bool verbose = std::getenv("CS80_BOUNDARY_VERBOSE") != nullptr;
    const auto& m = sf::nominalBoundaryV1;
    double worstDrive = 0.0, worstReference = 0.0, worstResidual = 0.0, maxSag = 0.0;

    for (int rank = 0; rank < 2; ++rank)
        for (int selector = 0; selector < 11; ++selector)
        {
            const std::string name = std::string(sf::address(rank, selector)->name);
            sf::ToneControls ideal, refined;
            check(sf::compileTone(rank, selector, ideal) && sf::compileToneRefined(rank, selector, refined),
                  "ideal and refined compile: " + name);
            check(ideal.provenance.boundaryVersion == 0
                      && (ideal.provenance.bridgeMask & sf::idealSourceAndDiode) != 0,
                  "production compileTone keeps the ideal V1 boundary: " + name);
            check(refined.provenance.boundaryVersion == m.version
                      && (refined.provenance.bridgeMask & sf::idealSourceAndDiode) == 0
                      && refined.provenance.version == sf::modelVersion,
                  "refined provenance records the boundary version: " + name);

            const auto rows = columnOf(refined);
            sf::ColumnSolution reference;
            check(sf::solveColumn(rows, m, reference), "column solves: " + name);
            worstResidual = std::max(worstResidual, std::abs(reference.residualAmps));
            maxSag = std::max(maxSag, m.driver.openCircuitVolts - reference.emitterVolts);
            check(reference.emitterVolts < m.driver.openCircuitVolts && reference.emitterVolts > 0.0,
                  "shared emitter sags under the column's load: " + name);
            sf::ToneControls forward;
            sf::compileToneRefined(rank, selector, forward, sf::forwardActiveBoundaryV1);
            for (int output = 1; output <= 26; ++output)
            {
                const auto i = (size_t) (output - 1);
                if (!rows[i].conducts)
                {
                    if (output != 4 && output != 5)
                        check(refined.sources[i].volts == 0.0 && refined.sources[i].ohms == sf::assembledShuntOhms,
                              "open branch keeps only the shunt: " + name);
                    continue;
                }
                const auto& s = reference.rows[i];
                worstResidual = std::max(worstResidual, std::abs(s.residualAmps));
                check(std::abs(s.residualAmps) <= tolResidual, "Newton residual: " + name + " output " + std::to_string(output));
                check(s.diodeVolts > 0.4 && s.diodeVolts < 0.9, "diode forward-biased at a silicon drop: " + name);
                // Against an ideal source of the same driver voltage, the diode
                // drop and finite impedances can only lower the source.
                check(refined.sources[i].volts < ideal.sources[i].volts * m.driver.openCircuitVolts / 10.0,
                      "diode and impedances lower the open-circuit source: " + name + " output " + std::to_string(output));
                check(forward.sources[i].volts < refined.sources[i].volts,
                      "forward-active alternative sits below the saturated nominal: " + name);
                // Reference point: the compiled Thevenin reproduces the truth bus.
                const auto& src = refined.sources[i];
                const double compiled = src.volts / (1.0 + src.ohms * rows[i].loadSiemens);
                const double error = std::abs(compiled - s.busVolts);
                worstReference = std::max(worstReference, error);
                check(error <= tolReference, "reference point exact: " + name + " output " + std::to_string(output));
                // The linearisation is the truth's tangent: d(bus)/d(current
                // injected into this bus) = Rth/(1 + Rth*gL), the shared emitter
                // included. Identity, 1e-6 relative (central difference); added
                // after the mutation check showed the 1 mV drive gate cannot
                // resolve the diode's dynamic resistance.
                auto up = rows, down = rows;
                const double delta = 1.0e-7;
                up[i].injectedAmps = delta;
                down[i].injectedAmps = -delta;
                sf::ColumnSolution a, b;
                sf::solveColumn(up, m, a);
                sf::solveColumn(down, m, b);
                const double truthSlope = (a.rows[i].busVolts - b.rows[i].busVolts) / (2.0 * delta);
                const double compiledSlope = src.ohms / (1.0 + src.ohms * rows[i].loadSiemens);
                check(std::abs(compiledSlope - truthSlope) <= 1.0e-6 * std::abs(truthSlope),
                      "linearised source impedance is the truth tangent: " + name + " output " + std::to_string(output));
            }

            // Drive range: every voice's filter-EG state and touch move the
            // cutoff buses; the truth re-solves the whole column each time.
            std::uint32_t lcg = 0x9e3779b9u ^ (std::uint32_t) (rank * 11 + selector);
            const auto next = [&] { lcg = lcg * 1664525u + 1013904223u; return (double) (lcg >> 8) / 16777216.0; };
            for (int trial = 0; trial < 64; ++trial)
            {
                sf::RankDrives drives {};
                for (int v = 0; v < sf::voiceCount; ++v)
                {
                    const double z = trial < 3 ? -3.0 + 3.0 * trial : -3.0 + 6.0 * next();
                    drives[(size_t) v] = { z, trial < 3 ? (double) (trial % 2) : next(), trial < 3 ? 1.0 : next() };
                }
                sf::CutoffReadout readout;
                check(sf::evaluateCutoff(refined, drives, readout), "refined tone evaluates: " + name);
                auto driven = rows;
                for (int side = 0; side < 2; ++side)
                {
                    const auto& map = refined.cutoff[(size_t) side];
                    double injected = 0.0;
                    for (const auto& d : drives)
                        injected += (1.0 / 39000.0) * (map.pinPerTouch * (refined.initialBrillianceVolts * d.velocity
                                                                          + refined.afterBrillianceVolts * d.pressure)
                                                       + map.pinPerEnvelopeVolt * sf::envelopeVoltsFromState(d.filterEnvelopeZ));
                    driven[side == 0 ? 6 : 8].injectedAmps = injected;
                }
                sf::ColumnSolution truth;
                check(sf::solveColumn(driven, m, truth), "drive solve: " + name);
                for (int side = 0; side < 2; ++side)
                {
                    const auto i = (size_t) (side == 0 ? 6 : 8);
                    if (!rows[i].conducts) continue;
                    const double error = std::abs(readout.loadedBusVolts[(size_t) side] - truth.rows[i].busVolts);
                    worstDrive = std::max(worstDrive, error);
                    check(error <= tolDrive, "cutoff bus within the linearisation tolerance: " + name + " "
                                                 + std::to_string(error));
                }
            }

            if (verbose)
            {
                sf::ToneControls lo, hi;
                sf::compileToneRefined(rank, selector, lo, sf::lowDropDiodeBoundaryV1);
                sf::compileToneRefined(rank, selector, hi, sf::highDropDiodeBoundaryV1);
                std::printf("%-14s emitter %.3f V | LP bus ideal %.3f nominal %.3f (%+.3f oct) fwd %.3f low %.3f high %.3f"
                            " | HP bus %.3f -> %.3f | AL %.2f -> %.2f V | SL %.2f -> %.2f V | LV bus %.3f -> %.3f\n",
                            name.c_str(), reference.emitterVolts, ideal.cutoff[1].busBaseVolts, refined.cutoff[1].busBaseVolts,
                            refined.cutoff[1].busBaseVolts - ideal.cutoff[1].busBaseVolts,
                            forward.cutoff[1].busBaseVolts, lo.cutoff[1].busBaseVolts, hi.cutoff[1].busBaseVolts,
                            ideal.cutoff[0].busBaseVolts, refined.cutoff[0].busBaseVolts,
                            ideal.sources[11].volts, refined.sources[11].volts, ideal.sources[19].volts, refined.sources[19].volts,
                            ideal.busVolts[21], refined.busVolts[21]);
            }
        }

    // The receiver conductance used for linearisation is the one the existing
    // compiled graphs attach (independent of compiledReceiverSiemens): a probe
    // source (5 V, 1 k) loaded by each graph at its reference point.
    {
        const sf::SourceEquivalent probe { 5.0, 1000.0 };
        const auto implied = [&](double bus) { return (probe.volts / bus - 1.0) / probe.ohms; };
        const auto close = [](double a, double b) { return std::abs(a - b) <= 1e-12 * std::max(1.0, std::abs(b)) + 1e-15; };
        check(close(sf::compiledReceiverSiemens(7, sf::Grade::B, sf::Grade::B),
                    implied(sf::compileCutoffSide(probe, sf::Grade::B, true).busBaseVolts))
                  && close(sf::compiledReceiverSiemens(9, sf::Grade::B, sf::Grade::B),
                           implied(sf::compileCutoffSide(probe, sf::Grade::B, false).busBaseVolts))
                  && close(sf::compiledReceiverSiemens(8, sf::Grade::B, sf::Grade::B), implied(sf::loadedQBus(probe, 0.0)))
                  && close(sf::compiledReceiverSiemens(10, sf::Grade::B, sf::Grade::B), implied(sf::loadedQBus(probe, 0.0)))
                  && close(sf::compiledReceiverSiemens(22, sf::Grade::B, sf::Grade::B), implied(sf::loadedLevelBus(probe, 0.0)))
                  && sf::compiledReceiverSiemens(12, sf::Grade::B, sf::Grade::B) == 0.0,
              "linearisation load equals the existing graphs' receiver load");
    }

    // Cell re-examination record: every unresolved cell is still an unknown
    // source cell, carries a labelled hypothesis, a source page and a candidate.
    {
        namespace fh = cs80::factory_hypotheses;
        int boardCandidates = 0;
        for (const auto& r : fh::review)
        {
            check(cs80::hardware::findBranch(r.address.board, r.address.column, r.address.output).kind
                      == cs80::hardware::BranchKind::unknown,
                  "re-examined cell stays unknown in the immutable source table");
            check(fh::find(r.address) != nullptr && !r.schematicPage.empty() && !r.boardPage.empty()
                      && !r.candidate.label.empty(),
                  "re-examined cell keeps its hypothesis, pages and candidate");
            boardCandidates += r.outcome == fh::ReviewOutcome::boardCandidate ? 1 : 0;
        }
        check(boardCandidates == 1 && fh::findReview({53, 'B', 14})->outcome == fh::ReviewOutcome::boardCandidate,
              "only T53/B/14 has a board-artwork candidate");
        // The paired-artwork inference behind it is consistent with three classified pairs.
        using cs80::hardware::findBranch;
        check(findBranch(53, 'C', 12).resistanceOhms() == 20000.0 && findBranch(51, 'C', 12).resistanceOhms() == 13000.0
                  && findBranch(53, 'A', 9).resistanceOhms() == 750.0 && findBranch(51, 'A', 9).resistanceOhms() == 1000.0
                  && findBranch(51, 'B', 14).resistanceOhms() == 39000.0,
              "board artwork outside/parenthesised values match T53/T51 classified cells");
        for (int rank = 0; rank < 2; ++rank)
            for (int selector = 0; selector < 11; ++selector)
            {
                const auto scenario = sf::visibleCandidateScenario(rank, selector);
                sf::ToneControls median, candidate;
                sf::compileTone(rank, selector, median);
                check(sf::compileToneForSensitivity(rank, selector, scenario, candidate)
                          && scenario.count == (std::size_t) std::popcount(median.provenance.unknownCells)
                          && candidate.provenance.hypothesisOverrideMask == median.provenance.unknownCells,
                      "visible-candidate scenario overrides exactly the tone's unknown cells");
                if (verbose && scenario.count > 0)
                {
                    std::printf("%-14s", std::string(sf::address(rank, selector)->name).c_str());
                    for (std::size_t k = 0; k < scenario.count; ++k)
                    {
                        const int output = scenario.overrides[k].output;
                        const auto i = (size_t) (output - 1);
                        std::printf("  out%-2d x %.3f->%.3f (%.2f->%.2f V)", output, median.hypothesisExcitations[i],
                                    candidate.hypothesisExcitations[i], median.sources[i].volts, candidate.sources[i].volts);
                    }
                    std::printf("\n");
                }
            }
    }

    // Invalid models are rejected without touching the result.
    sf::BoundaryModel broken = m;
    broken.diode.saturationAmps = 0.0;
    sf::ToneControls untouched;
    check(!sf::compileToneRefined(0, 3, untouched, broken) && !untouched.compiled, "invalid boundary model rejected");

    std::printf("worst reference-point error %.3g V, drive-range error %.3g V, Newton residual %.3g A, "
                "largest emitter sag %.3f V\n", worstReference, worstDrive, worstResidual, maxSag);
    if (failures != 0)
    {
        std::printf("%d boundary refinement failures\n", failures);
        return 1;
    }
    std::printf("BA/diode boundary refinement passed for 22 tones\n");
    return 0;
}
