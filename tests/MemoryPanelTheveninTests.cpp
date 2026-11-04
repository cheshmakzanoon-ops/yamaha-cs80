// Future circuit-engine memory/PANEL source adapter, TEST-FIRST. A diagonal
// local Thevenin model (including source pot, diode, shared rail and 10k bus
// shunt) is independently checked by finite-current probes on the existing
// 94-node offline Newton reference. Source-only: it is not connected to Engine.
#include "core/MemoryPanelSourceEquivalent.h"
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
namespace eq = cs80::memory_panels::source_equivalent;
namespace truth = cs80::reference::memory_panel;
namespace sf = cs80::source_factory;

namespace {
int failures = 0;
void check(bool okay, const char* what)
{
    if (!okay) { ++failures; if (failures < 30) std::printf("FAIL: %s\n", what); }
}
double tolerance(const std::string& json, const char* key)
{
    const auto stage = json.find("\"memory_panel_thevenin_v1\"");
    const auto i = stage == std::string::npos ? stage : json.find("\"" + std::string(key) + "\"", stage);
    if (i == std::string::npos) { std::printf("Missing preregistered %s\n", key); std::exit(2); }
    return std::strtod(json.c_str() + json.find(':', i) + 1, nullptr);
}
double loadedBusFromSource(sf::SourceEquivalent s, double receiverG)
{
    return s.volts / (1.0 + s.ohms * receiverG);
}
mp::Positions fixture(int rank, int selector)
{
    mp::Positions p {};
    for (int row = 1; row <= 26; ++row)
    {
        const auto i = (size_t) (row - 1);
        if (mp::isSwitchRow(row)) p[i] = ((rank + selector + row) & 1) != 0 ? 1.0 : 0.0;
        else p[i] = 0.10 + 0.80 * ((row * 5 + rank * 7 + selector * 3) % 19) / 18.0;
    }
    return p;
}
bool withinTangentGate(double proposed, double truthOhms, double abs, double relative)
{
    return std::isfinite(proposed) && std::isfinite(truthOhms)
        && std::abs(proposed - truthOhms) <= abs + relative * truthOhms;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) { std::printf("usage: %s preregistered-tolerances.json\n", argv[0]); return 2; }
    std::ifstream in(argv[1]);
    check(in.good(), "preregistered Thevenin tolerance JSON exists");
    std::stringstream text; text << in.rdbuf();
    const auto json = text.str();
    const double tolBus = tolerance(json, "selected_analog_bus_volts");
    const double tolReconstruct = tolerance(json, "reconstructed_dc_bus_volts");
    const double tolAbsR = tolerance(json, "local_tangent_resistance_absolute_ohms");
    const double tolRelR = tolerance(json, "local_tangent_resistance_relative");
    const double tolResidual = tolerance(json, "truth_newton_residual_amps");
    const double deltaAmps = tolerance(json, "small_signal_current_injection_amps");
    const double tolRailTerm = tolerance(json, "shared_rail_backreaction_ohms_min");
    check(deltaAmps > 0.0 && deltaAmps <= 1e-6, "bounded Newton probe current");

    check(!eq::compile(-1, 11, {}).ok && !eq::compile(2, 13, {}).ok
          && !eq::compile(0, 10, {}).ok && !eq::compile(0, 14, {}).ok,
          "memory Thevenin adapter cannot mutate factory-tab or invalid selectors");
    double maxDc = 0.0, maxReconstruct = 0.0, maxAbsR = 0.0, maxRelativeR = 0.0;
    bool caughtMissingShunt = false, caughtMissingDeembed = false, caughtMissingRail = false;
    int ports = 0;
    for (int rank = 0; rank < 2; ++rank)
        for (int selector : {11, 12, 13})
        {
            const auto positions = fixture(rank, selector);
            const auto adapter = eq::compile(rank, selector, positions);
            check(adapter.ok, "memory/PANEL local Thevenin adapter compiles");
            if (!adapter.ok) continue;
            const auto switches = mp::switchStates(rank, selector, positions);
            check(adapter.switches.pulse == switches.pulse && adapter.switches.saw == switches.saw
                  && !adapter.ports[3].analog && !adapter.ports[4].analog,
                  "logical PULSE/SAW rows do not become analog sources");

            truth::Setup base;
            base.rank = rank;
            base.selector = selector;
            base.banks[(size_t) (selector - 11)] = positions;
            const auto direct = truth::solve(base);
            check(direct.converged && direct.residualAmps <= tolResidual,
                  "independent full-network reference converges at selected memory/PANEL DC point");
            for (int row = 1; row <= 26; ++row)
            {
                if (mp::isSwitchRow(row)) continue;
                const auto i = (size_t) (row - 1);
                const double receiver = sf::compiledReceiverSiemens(row, sf::Grade::B, sf::Grade::B);
                const auto& port = adapter.ports[i];
                check(port.analog && sf::valid(port.source) && port.source.ohms <= sf::assembledShuntOhms,
                      "every analog output gets a valid Thevenin voltage and impedance with 10k shunt retained");
                const double dc = std::abs(port.loadedBusVolts - direct.busVolts[i]);
                maxDc = std::max(maxDc, dc);
                check(dc <= tolBus, "compiled loaded DC bus stays within the original 2mV truth limit");
                const double reconstructed = std::abs(port.loadedBusVolts
                                                      - loadedBusFromSource(port.source, receiver));
                maxReconstruct = std::max(maxReconstruct, reconstructed);
                check(reconstructed <= tolReconstruct, "de-embedded source reconstructs its loaded DC bus exactly");

                // Independent local one-port tangent, all other receiver loads
                // held constant, from central-difference injected currents.
                auto plus = base, minus = base;
                plus.injectedBusCurrentAmps[i] = deltaAmps;
                minus.injectedBusCurrentAmps[i] = -deltaAmps;
                const auto high = truth::solve(plus), low = truth::solve(minus);
                check(high.converged && low.converged
                      && high.residualAmps <= tolResidual && low.residualAmps <= tolResidual,
                      "symmetric Newton current-injection probes converge");
                if (!high.converged || !low.converged) continue;
                const double rLoaded = (high.busVolts[i] - low.busVolts[i]) / (2.0 * deltaAmps);
                check(rLoaded > 0.0 && receiver * rLoaded < 1.0,
                      "independent loaded tangent can be de-embedded without a singularity");
                const double truthR = 1.0 / (1.0 / rLoaded - receiver);
                const double absoluteError = std::abs(port.source.ohms - truthR);
                maxAbsR = std::max(maxAbsR, absoluteError);
                maxRelativeR = std::max(maxRelativeR, absoluteError / std::max(1.0, truthR));
                check(withinTangentGate(port.source.ohms, truthR, tolAbsR, tolRelR),
                      "24 local source impedances per selector match independently perturbed Newton tangent");
                ++ports;

                // Three deliberate wrong-source mutations, restricted to the
                // test: no production model or pre-registered gate is altered.
                if (rank == 0 && selector == 11)
                {
                    if (row == 12)
                    {
                        // Omitting the assembled 10k shunt from the source
                        // increases effective R catastrophically.
                        const double mutatedWithoutShunt = 1.0 /
                            (1.0 / port.loadedPortOhms - receiver - 1.0 / sf::assembledShuntOhms);
                        caughtMissingShunt = !withinTangentGate(mutatedWithoutShunt, truthR, tolAbsR, tolRelR);
                        // Explicit mutant: delete the rail term from the loaded
                        // one-port resistance. The independent topology gate
                        // demands a detectable rail contribution even when its
                        // size is below the wider Newton-tangent model budget.
                        const double mutantStiffRail = port.loadedPortOhms - port.railBackreactionOhms;
                        caughtMissingRail = !mp::isBufferedRow(row)
                            && std::abs(mutantStiffRail - port.loadedPortOhms) > tolRailTerm;
                    }
                    if (row == 8)
                        caughtMissingDeembed = !withinTangentGate(port.loadedPortOhms, truthR, tolAbsR, tolRelR);
                }
            }
        }
    check(ports == 2 * 3 * mp::analogRowCount, "all 144 rank/selector/analog ports were independently probed");
    check(caughtMissingShunt, "mutation check: removing the 10k assembled shunt must fail tangent truth");
    check(caughtMissingDeembed, "mutation check: leaving receivers in the Thevenin source must fail tangent truth");
    check(caughtMissingRail, "mutation check: zeroing shared rail backreaction must fail the preregistered topology gate");
    std::printf("memory/PANEL Thevenin: %d ports, worst bus %.6g V, reconstruction %.6g V, source impedance %.6g ohm (%.3g rel); mutations shunt=%d deembed=%d rail=%d\n",
                ports, maxDc, maxReconstruct, maxAbsR, maxRelativeR,
                caughtMissingShunt, caughtMissingDeembed, caughtMissingRail);
    return failures == 0 ? 0 : 1;
}
