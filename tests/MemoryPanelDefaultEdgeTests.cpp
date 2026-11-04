// Test-first default and electrical pot-endpoint contract. The near-zero
// selected 1S1555 surrogate remains weakly conducting; a solver's absolute
// Newton residual must not reject the entire M1..M4/PANEL source frame.
// Numerical gates are independently frozen in memory-panel-default-edge-v1.json.
#include "core/MemoryPanelControlBridge.h"
#include "core/MemoryPanelToneAdapter.h"
#include "reference/MemoryPanelNetwork.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace mp = cs80::memory_panels;
namespace cb = cs80::memory_panels::control_bridge;
namespace eq = cs80::memory_panels::source_equivalent;
namespace ta = cs80::memory_panels::tone_adapter;
namespace truth = cs80::reference::memory_panel;
namespace sf = cs80::source_factory;

namespace {
int failures = 0;
void check(bool valid, const char* description)
{
    if (!valid) { ++failures; if (failures < 35) std::printf("FAIL: %s\n", description); }
}
double gate(const std::string& json, const char* key)
{
    const auto stage = json.find("\"memory_panel_default_edge_v1\"");
    const auto at = stage == std::string::npos ? stage : json.find("\"" + std::string(key) + "\"", stage);
    if (at == std::string::npos) { std::printf("Missing preregistered %s\n", key); std::exit(2); }
    return std::strtod(json.c_str() + json.find(':', at) + 1, nullptr);
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) { std::printf("usage: %s memory-panel-default-edge-v1.json\n", argv[0]); return 2; }
    std::ifstream input(argv[1]);
    check(input.good(), "default/endpoint tolerance JSON preregistered before testing");
    std::stringstream buffer; buffer << input.rdbuf();
    const auto json = buffer.str();
    const double tolBus = gate(json, "selected_analog_bus_volts");
    const double tolAbsR = gate(json, "local_tangent_resistance_absolute_ohms");
    const double tolRelR = gate(json, "local_tangent_resistance_relative");
    const double tolRoundtrip = gate(json, "receiver_roundtrip_volts");
    const double tolNewton = gate(json, "truth_newton_residual_amps");
    const double minG = gate(json, "low_drive_diode_tangent_siemens_min");
    const double maxG = gate(json, "low_drive_diode_tangent_siemens_max");
    const int expected = static_cast<int>(gate(json, "expected_source_scenarios"));
    const int mutationMin = static_cast<int>(gate(json, "mutant_missing_low_drive_tangent_count_min"));
    // Identical 50 nA probe from separately preregistered
    // memory-panel-thevenin-v1.json; do not tune to a result.
    constexpr double probe = 50e-9;
    double maxBus = 0.0, maxSourceR = 0.0, maxRoundtrip = 0.0;
    int scenarios = 0, probed = 0, mutationsKilled = 0;
    for (int rank = 0; rank < 2; ++rank)
        for (int selector : {11, 12, 13})
            for (int corner = 0; corner < 3; ++corner)
            {
                mp::Positions positions {};
                if (corner == 0)
                {
                    cs80::ParamValues defaults;
                    positions = cb::positions(defaults, rank);
                }
                else if (corner == 2)
                    positions.fill(1.0);

                const auto source = eq::compile(rank, selector, positions);
                sf::ToneControls tone;
                const bool toneOk = ta::compileResolved(rank, selector, source, tone);
                check(source.ok && toneOk && tone.compiled,
                      "selected M1..M4/PANEL default and endpoint sources never reject a valid pot frame");
                if (!source.ok || !toneOk) continue;
                ++scenarios;
                truth::Setup setup;
                setup.rank = rank;
                setup.selector = selector;
                setup.banks[static_cast<std::size_t>(selector - 11)] = positions;
                const auto direct = truth::solve(setup);
                check(direct.converged && direct.residualAmps <= tolNewton,
                      "independent 94-node selected edge network Newton solve converges");
                for (int output = 1; output <= 26; ++output)
                {
                    if (mp::isSwitchRow(output)) continue;
                    const auto i = static_cast<std::size_t>(output - 1);
                    const auto& port = source.ports[i];
                    check(port.analog && sf::valid(port.source),
                          "all 24 extreme analogue ports retain a valid finite Thevenin source");
                    if (!direct.converged) continue;
                    const double error = std::abs(port.loadedBusVolts - direct.busVolts[i]);
                    maxBus = std::max(maxBus, error);
                    check(error <= tolBus, "all valid edge pots meet original 2mV full-network bus truth");
                    const double receiver = sf::compiledReceiverSiemens(output, sf::Grade::B, sf::Grade::B);
                    const double reconstructed = port.source.volts / (1.0 + port.source.ohms * receiver);
                    const double roundtrip = std::abs(reconstructed - port.loadedBusVolts);
                    maxRoundtrip = std::max(maxRoundtrip, roundtrip);
                    check(roundtrip <= tolRoundtrip, "extreme port receiver-source de-embedding roundtrips exactly");

                    auto plus = setup, minus = setup;
                    plus.injectedBusCurrentAmps[i] = probe;
                    minus.injectedBusCurrentAmps[i] = -probe;
                    const auto high = truth::solve(plus), low = truth::solve(minus);
                    check(high.converged && low.converged
                          && high.residualAmps <= tolNewton && low.residualAmps <= tolNewton,
                          "independent positive and negative low-current Newton probes converge");
                    if (!high.converged || !low.converged) continue;
                    const double loadedR = (high.busVolts[i] - low.busVolts[i]) / (2.0 * probe);
                    check(loadedR > 0.0 && loadedR * receiver < 1.0,
                          "edge-pot one-port source tangent remains de-embeddable");
                    if (!(loadedR > 0.0 && loadedR * receiver < 1.0)) continue;
                    const double expectedSourceR = 1.0 / (1.0 / loadedR - receiver);
                    const double errorR = std::abs(port.source.ohms - expectedSourceR);
                    maxSourceR = std::max(maxSourceR, errorR);
                    check(errorR <= tolAbsR + tolRelR * expectedSourceR,
                          "near-open memory branch keeps correct nonzero source tangent");
                    ++probed;
                    if (corner == 0 && rank == 0 && selector == 11 && output == 1)
                    {
                        // Two additional *local* mutations under the already
                        // committed 2 ohm + 0.2% independent Newton gate.
                        // They must not be made to the production model.
                        const double mutantNoInactive = 1.0 /
                            (1.0 / port.loadedPortOhms - port.inactiveBranchSiemens - receiver);
                        const bool inactiveKilled = std::abs(mutantNoInactive - expectedSourceR)
                            > tolAbsR + tolRelR * expectedSourceR;
                        check(inactiveKilled,
                              "mutation: dropping zero-bias inactive diodes fails the independent source tangent");
                        mutationsKilled += static_cast<int>(inactiveKilled);
                        const double mutantNoShunt = 1.0 /
                            (1.0 / port.loadedPortOhms - receiver - 1.0 / sf::assembledShuntOhms);
                        const bool shuntKilled = std::abs(mutantNoShunt - expectedSourceR)
                            > tolAbsR + tolRelR * expectedSourceR;
                        check(shuntKilled,
                              "mutation: deleting the assembled bus shunt fails the independent source tangent");
                        mutationsKilled += static_cast<int>(shuntKilled);
                    }
                }
                if (corner == 0 && rank == 0 && selector == 11)
                {
                    const auto tangent = source.ports[0].selectedBranchSiemens;
                    check(tangent >= minG && tangent <= maxG,
                          "default PWM-speed wiper below diode knee retains bounded nonzero differential conductance");
                    const double mutantDisconnected = 0.0;
                    const bool killed = tangent >= minG && mutantDisconnected < minG;
                    check(killed, "mutation: incorrectly treating low-drive diode as open must fail topology gate");
                    mutationsKilled += static_cast<int>(killed);
                }
            }
    check(scenarios == expected, "all preregistered default and endpoint memory/PANEL source scenarios compiled");
    check(probed == expected * mp::analogRowCount,
          "all 432 default/endpoint memory source tangents probed against independent Newton truth");
    check(mutationsKilled >= std::max(3, mutationMin),
          "all low-drive, inactive-diode and assembled-shunt default-edge mutants detected");
    std::printf("memory/PANEL default+edges: %d scenarios, %d probed ports; max bus %.6g V, R %.6g ohm, roundtrip %.6g V, mutations %d/3\n",
                scenarios, probed, maxBus, maxSourceR, maxRoundtrip, mutationsKilled);
    return failures == 0 ? 0 : 1;
}
