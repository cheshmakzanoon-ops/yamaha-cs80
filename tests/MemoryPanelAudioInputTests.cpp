// Test-first memory/PANEL circuit-audio input contract. The audio engine does
// not yet consume this bridge: this gate fixes the 26 physical parameter-to-
// wiper fractions against the independent source-network truth model before
// wiring selected memory/PANEL rails to the circuit-engine rank controls.
// Not an analog CS-80 measurement. See memory-panel-audio-input-v1.json.
#include "core/MemoryPanelControlBridge.h"
#include "reference/MemoryPanelNetwork.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace mp = cs80::memory_panels;
namespace bridge = cs80::memory_panels::control_bridge;
namespace truth = cs80::reference::memory_panel;

namespace {
int failures = 0;
void check(bool ok, const char* what)
{
    if (!ok) { ++failures; if (failures < 30) std::printf("FAIL: %s\n", what); }
}
double tolerance(const std::string& json, const char* key)
{
    const auto stage = json.find("\"memory_panel_audio_input_v1\"");
    const auto pos = stage == std::string::npos ? stage : json.find("\"" + std::string(key) + "\"", stage);
    if (pos == std::string::npos) { std::printf("missing pre-registered tolerance %s\n", key); std::exit(2); }
    const auto colon = json.find(':', pos);
    return std::strtod(json.c_str() + colon + 1, nullptr);
}
// Independent inverse of the APVTS float conversion under test: generate a
// physical control position using JUCE NormalisableRange's specified skew.
float physicalAt(cs80::ChannelParam param, double fraction)
{
    const auto& d = cs80::kChannelDefs[param];
    return static_cast<float>(d.min + (d.max - d.min) * std::pow(fraction, d.skew));
}
double targetFraction(int rank, int selector, int row, int fixture)
{
    if (row == 4 || row == 5)
        return ((rank + selector + fixture + row) & 1) != 0 ? 1.0 : 0.0;
    if (fixture == 0) return 0.5;
    if (fixture == 1)
    {
        if (row == 7) return 0.10; // forces an HP/LP swap mutation to be audible
        if (row == 9) return 0.90;
        return 0.10 + 0.80 * ((row * 11 + rank * 7 + selector * 3) % 19) / 18.0;
    }
    return ((row + rank + selector) & 1) != 0 ? 0.05 : 0.95;
}
double worstBusDifference(const mp::BusResult& actual, const truth::Result& expected)
{
    double worst = 0.0;
    for (int output = 1; output <= 26; ++output)
        if (!mp::isSwitchRow(output))
            worst = std::max(worst, std::abs(actual.busVolts[(size_t) (output - 1)]
                                               - expected.busVolts[(size_t) (output - 1)]));
    return worst;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) { std::printf("usage: %s preregistered-tolerances.json\n", argv[0]); return 2; }
    std::ifstream in(argv[1]);
    check(in.good(), "pre-registered memory audio input gate file exists");
    std::stringstream text; text << in.rdbuf();
    const auto json = text.str();
    const double tolPot = tolerance(json, "control_frame_pot_fraction");
    const double tolBus = tolerance(json, "selected_analog_bus_volts");
    const double tolInactive = tolerance(json, "unselected_source_current_amps");
    const double tolResidual = tolerance(json, "truth_newton_residual_amps");
    const double tolIsolation = tolerance(json, "unselected_bank_isolation_bus_volts");

    check(mp::memoryBank(0, mp::Source::memoryA) == 0
          && mp::memoryBank(1, mp::Source::memoryA) == 1
          && mp::memoryBank(0, mp::Source::memoryB) == 2
          && mp::memoryBank(1, mp::Source::memoryB) == 3,
          "M1/M3 rank I and M2/M4 rank II mapping is exact");
    cs80::ParamValues invalid;
    check(bridge::positions(invalid, -1) == mp::Positions{}
          && bridge::positions(invalid, 2) == mp::Positions{},
          "invalid rank cannot activate a memory/PANEL pot bank");

    double worstFraction = 0.0, worstBus = 0.0, worstLeakage = 0.0, worstResidual = 0.0;
    bool mutationCaught = false;
    for (int rank = 0; rank < 2; ++rank)
        for (int selector : {11, 12, 13})
            for (int fixture = 0; fixture < 3; ++fixture)
            {
                // Effective values already contain the selected memory bank
                // (or the PANEL) when PluginProcessor::readParameters returns.
                cs80::ParamValues effective;
                mp::Positions expected {};
                for (int output = 1; output <= 26; ++output)
                {
                    const auto i = (size_t) (output - 1);
                    expected[i] = targetFraction(rank, selector, output, fixture);
                    const auto param = bridge::rowParameter[i];
                    effective[cs80::chParam(rank, param)] = physicalAt(param, expected[i]);
                }
                const auto actual = bridge::positions(effective, rank);
                for (int output = 1; output <= 26; ++output)
                {
                    const auto i = (size_t) (output - 1);
                    const double error = std::abs(actual[i] - expected[i]);
                    worstFraction = std::max(worstFraction, error);
                    check(error <= tolPot, "all 26 physical controls invert their JUCE skew into the intended wiper fractions");
                }
                const auto switches = mp::switchStates(rank, selector, actual);
                check(switches.pulse == (expected[3] >= 0.5)
                      && switches.saw == (expected[4] >= 0.5),
                      "selected bank owns both original waveform switch rows exactly");

                truth::Setup setup;
                setup.rank = rank;
                setup.selector = selector;
                const int active = selector == 11 ? 0 : selector == 12 ? 1 : 2;
                setup.banks[(size_t) active] = expected; // independent expected physical positions
                const auto t = truth::solve(setup);
                check(t.converged, "offline full-circuit truth model converges");
                check(t.residualAmps <= tolResidual, "offline truth Newton residual stays in preregistered bound");
                check(t.worstInactiveAmps <= tolInactive, "unselected original rails/columns leak only through reverse diodes");
                worstResidual = std::max(worstResidual, t.residualAmps);
                worstLeakage = std::max(worstLeakage, t.worstInactiveAmps);

                const auto compiled = mp::compileBuses(rank, selector, actual);
                check(compiled.ok, "selected memory/PANEL source bus compiles at control-frame boundary");
                const double error = worstBusDifference(compiled, t);
                worstBus = std::max(worstBus, error);
                check(error <= tolBus, "selected memory/PANEL buses agree with the offline truth model on all 24 analog rows");

                // Deliberate wrong source-row mapping: swapped HP/LP slider
                // rows MUST break the source/truth gate, not pass invisibly.
                if (rank == 0 && selector == 11 && fixture == 1)
                {
                    auto wrong = actual;
                    std::swap(wrong[6], wrong[8]);
                    const auto bad = mp::compileBuses(rank, selector, wrong);
                    mutationCaught = bad.ok && worstBusDifference(bad, t) > tolBus;
                }

                // Other memory sources are physically still there, but their
                // rails are off: changing their stored knob positions must not
                // affect the selected bus above leakage-level voltage.
                auto unselected = setup;
                for (int bank = 0; bank < 3; ++bank)
                    if (bank != active)
                        unselected.banks[(size_t) bank].fill((bank & 1) == 0 ? 0.1 : 0.9);
                const auto isolated = truth::solve(unselected);
                check(isolated.converged, "truth also converges with unrelated memory knobs moved");
                double difference = 0.0;
                for (int output = 1; output <= 26; ++output)
                    if (!mp::isSwitchRow(output))
                        difference = std::max(difference, std::abs(t.busVolts[(size_t) (output - 1)]
                                                                        - isolated.busVolts[(size_t) (output - 1)]));
                check(difference <= tolIsolation, "inactive banks cannot influence selected memory/PANEL buses");
            }
    check(mutationCaught, "mutation check: swapped HP/LP source row must be rejected by truth gate");
    std::printf("memory audio input truth: max pot %.6g, bus %.6g V, inactive %.6g A, residual %.6g A; row-swap mutation %s\n",
                worstFraction, worstBus, worstLeakage, worstResidual, mutationCaught ? "caught" : "missed");
    return failures == 0 ? 0 : 1;
}
