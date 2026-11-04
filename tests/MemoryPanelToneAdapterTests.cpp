// Original M1..M4/PANEL source-to-ToneControls adapter: 26-role, source-only
// truth gate. No Engine audio route, no factory V1 compiler modification.
// Acceptance registered first in memory-panel-tone-adapter-v1.json.
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
namespace ta = cs80::memory_panels::tone_adapter;
namespace eq = cs80::memory_panels::source_equivalent;
namespace sf = cs80::source_factory;
namespace truth = cs80::reference::memory_panel;

namespace {
int failures = 0;
void check(bool good, const char* description)
{
    if (!good) { ++failures; if (failures < 32) std::printf("FAIL: %s\n", description); }
}
double gate(const std::string& text, const char* key)
{
    const auto stage = text.find("\"memory_panel_tone_adapter_v1\"");
    const auto at = stage == std::string::npos ? stage : text.find("\"" + std::string(key) + "\"", stage);
    if (at == std::string::npos) { std::printf("Missing preregistered %s\n", key); std::exit(2); }
    return std::strtod(text.c_str() + text.find(':', at) + 1, nullptr);
}
double u(double v) { return std::clamp(v / 10.0, 0.0, 1.0); }
double seconds(double volts) { return std::pow(10.0, -3.0 * u(volts)); }
mp::Positions fixture(int rank, int selector)
{
    mp::Positions p {};
    for (int output = 1; output <= 26; ++output)
    {
        const auto i = static_cast<std::size_t>(output - 1);
        if (mp::isSwitchRow(output)) p[i] = ((rank + selector + output) & 1) ? 1.0 : 0.0;
        else p[i] = 0.10 + 0.80 * ((output * 5 + rank * 7 + selector * 3) % 19) / 18.0;
    }
    return p;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) { std::printf("usage: %s tolerances.json\n", argv[0]); return 2; }
    std::ifstream input(argv[1]);
    check(input.good(), "preregistered 26-port ToneControls acceptance exists");
    std::stringstream stream; stream << input.rdbuf();
    const auto text = stream.str();
    const double tolSourceV = gate(text, "source_voltage_absolute_volts");
    const double tolSourceR = gate(text, "source_impedance_absolute_ohms");
    const double tolLaw = gate(text, "control_law_absolute");
    const double tolBus = gate(text, "zero_drive_bus_vs_truth_volts");
    const double tolRoundTrip = gate(text, "source_receiver_roundtrip_volts");
    const double minSwap = gate(text, "mutant_swapped_hp_lp_cutoff_bus_min_volts");
    const double minZeroR = gate(text, "mutant_zero_cutoff_impedance_bus_min_volts");
    const double minGateMutation = gate(text, "mutant_reversed_gate_count_min");

    sf::ToneControls guard;
    guard.source = {0, 1234, -1, '?', "sentinel", "sentinel"};
    check(!ta::compile(0, 10, {}, guard) && guard.source.selector == 1234,
          "no factory/invalid selector can alter a previously valid tone transaction");
    check(!ta::compile(-1, 11, {}, guard) && !ta::compile(2, 11, {}, guard)
          && !ta::compile(0, 14, {}, guard),
          "memory adapter rejects invalid ranks/selectors without changing V1 factory sources");

    double maxVoltage = 0.0, maxResistance = 0.0, maxLaw = 0.0;
    double maxBus = 0.0, maxRoundTrip = 0.0, swapDelta = 0.0, zeroRDelta = 0.0;
    int analogPorts = 0, roleChecks = 0, mutations = 0;
    for (int rank = 0; rank < 2; ++rank)
        for (int selector : {11, 12, 13})
        {
            const auto selected = fixture(rank, selector);
            const auto source = eq::compile(rank, selector, selected);
            check(source.ok, "selected M1/M3/M2/M4/PANEL source network compiles");
            if (!source.ok) continue;
            sf::ToneControls tone;
            check(ta::compileResolved(rank, selector, source, tone) && tone.compiled,
                  "all 26 selected source rows compile transactionally to ToneControls");
            if (!tone.compiled) continue;
            check(tone.source.rank == rank && tone.source.selector == selector
                  && tone.source.board == -1 && tone.source.name == ta::selectedName(rank, selector),
                  "memory/PANEL uses named non-factory provenance (no fake T51-T54 address)");
            check(tone.provenance.knownCells == 0 && tone.provenance.unknownCells == 0
                  && tone.provenance.inferredCells == 0 && !tone.provenance.hardwareMeasured,
                  "factory cell masks and physical measurement claims cannot be fabricated");
            check(tone.provenance.boundaryVersion == eq::modelVersion
                  && (tone.provenance.bridgeMask & sf::idealSourceAndDiode) == 0,
                  "electrical source/difference law has explicit non-V1 provenance");
            check(tone.wsc.pulseEnabled == source.switches.pulse
                  && tone.wsc.sawEnabled == source.switches.saw,
                  "rows 4 and 5 preserve the physical selected waveform gate logic");

            truth::Setup setup;
            setup.rank = rank;
            setup.selector = selector;
            setup.banks[static_cast<std::size_t>(selector - 11)] = selected;
            const auto reference = truth::solve(setup);
            check(reference.converged, "independent full memory/PANEL Newton source reference converges");

            for (int output = 1; output <= 26; ++output)
            {
                const auto i = static_cast<std::size_t>(output - 1);
                const double ev = source.ports[i].source.volts;
                const double er = source.ports[i].source.ohms;
                maxVoltage = std::max(maxVoltage, std::abs(tone.sources[i].volts - ev));
                maxResistance = std::max(maxResistance, std::abs(tone.sources[i].ohms - er));
                check(std::abs(tone.sources[i].volts - ev) <= tolSourceV
                      && std::abs(tone.sources[i].ohms - er) <= tolSourceR,
                      "every source port retains its exact Thevenin voltage and impedance");
                if (mp::isSwitchRow(output)) continue;
                ++analogPorts;
                if (reference.converged)
                {
                    const double delta = std::abs(tone.busVolts[i] - reference.busVolts[i]);
                    maxBus = std::max(maxBus, delta);
                    check(delta <= tolBus, "all 24 zero-drive loaded bus voltages match independent Newton truth");
                }
                const double deltaBus = std::abs(tone.busVolts[i] - source.ports[i].loadedBusVolts);
                maxRoundTrip = std::max(maxRoundTrip, deltaBus);
                check(deltaBus <= tolRoundTrip,
                      "receiver source/input coupling reconstructs the resolved loaded bus");
            }

            const auto v = [&](int output) { return source.ports[static_cast<std::size_t>(output - 1)].source.volts; };
            const auto law = [&](double actual, double expected)
            {
                maxLaw = std::max(maxLaw, std::abs(actual - expected));
                check(std::isfinite(actual) && std::abs(actual - expected) <= tolLaw,
                      "named ToneControls downstream law matches its independently computed role");
                ++roleChecks;
            };
            const double duty = 0.5 + 0.4 * u(v(3));
            law(tone.dutyProxy, duty);
            law(tone.wsc.pulseThresholdVolts, 1.525 * (1.0 - 2.0 * duty));
            law(tone.pwmRateHz, 0.05 * std::pow(400.0, u(v(1))));
            law(tone.pwmDepth, u(v(2)));
            law(tone.noiseGain, u(v(6)));
            law(tone.filterEnvelope.initialVolts, -5.0 * u(v(11)));
            law(tone.filterEnvelope.attackVolts, 5.0 * u(v(12)));
            law(tone.filterEnvelope.attackSeconds, seconds(v(13)));
            law(tone.filterEnvelope.decaySeconds, 10.0 * seconds(v(14)));
            law(tone.filterEnvelope.releaseSeconds, 10.0 * seconds(v(15)));
            law(tone.filterPathGain, u(v(16)));
            law(tone.sinePathGain, u(v(17)));
            law(tone.amplitudeEnvelope.attackSeconds, seconds(v(18)));
            law(tone.amplitudeEnvelope.decaySeconds, 10.0 * seconds(v(19)));
            law(tone.amplitudeEnvelope.sustainRatio, u(v(20)));
            law(tone.amplitudeEnvelope.releaseSeconds, 10.0 * seconds(v(21)));
            law(tone.boundary.qHp.volts, v(8));
            law(tone.boundary.qLp.volts, v(10));
            law(tone.boundary.level.volts, v(22));
            law(tone.initialBrillianceVolts, v(23));
            law(tone.boundary.initialLevelVolts, v(24));
            law(tone.afterBrillianceVolts, v(25));
            law(tone.boundary.afterLevelVolts, v(26));

            // Exercise the actual source-driven receiver consumers that the
            // future Engine route uses. Not a hardware authenticity test.
            sf::RankDrives drives {};
            drives[0] = {0.42, 0.78, 0.31};
            sf::CutoffReadout cutoff;
            check(sf::evaluateCutoff(tone, drives, cutoff),
                  "new memory tone drives named 8-voice HP/LP source receiver readout");
            sf::BothRankDrives bothDrives {};
            bothDrives[static_cast<std::size_t>(rank)] = drives;
            sf::BothRankDrives otherDrives = bothDrives;
            std::array<sf::RankBoundary, 2> rankBoundary {tone.boundary, tone.boundary};
            sf::SharedReadout shared;
            check(sf::evaluateShared(rankBoundary, 0.4, otherDrives, shared),
                  "new memory tone drives both shared-rank Q/LC receiver ports");

            if (rank == 0 && selector == 11)
            {
                // Three deliberate model mutations, only in this standalone
                // test. Each must be caught by the separately frozen gates.
                auto swapped = source;
                std::swap(swapped.ports[6], swapped.ports[8]);
                sf::ToneControls wrongCutoff;
                if (ta::compileResolved(rank, selector, swapped, wrongCutoff))
                    swapDelta = std::abs(wrongCutoff.busVolts[6] - reference.busVolts[6]);
                const bool swapDetected = swapDelta >= minSwap;
                check(swapDetected, "mutation: swapped HP/LP sources fails original HP bus truth");
                mutations += static_cast<int>(swapDetected);

                auto zeroR = source;
                zeroR.ports[6].source.ohms = 0.0;
                sf::ToneControls stiffCutoff;
                if (ta::compileResolved(rank, selector, zeroR, stiffCutoff))
                    zeroRDelta = std::abs(stiffCutoff.busVolts[6] - reference.busVolts[6]);
                const bool zeroRDetected = zeroRDelta >= minZeroR;
                check(zeroRDetected, "mutation: discarding finite HP source impedance fails DC bus truth");
                mutations += static_cast<int>(zeroRDetected);

                auto inverted = source;
                inverted.switches.pulse = !inverted.switches.pulse;
                inverted.switches.saw = !inverted.switches.saw;
                sf::ToneControls rejected;
                rejected.source.selector = 1234;
                const bool gateDetected = !ta::compileResolved(rank, selector, inverted, rejected)
                    && rejected.source.selector == 1234;
                check(gateDetected, "mutation: reversed PULSE/SAW logic is rejected transactionally");
                mutations += static_cast<int>(gateDetected);
            }
        }
    check(analogPorts == 2 * 3 * mp::analogRowCount,
          "all 144 memory/rank/analog sources are compared with the independent truth network");
    check(roleChecks == 2 * 3 * 23,
          "all 23 named control-law projections per memory/PANEL selection checked");
    check(mutations >= 2 + static_cast<int>(minGateMutation),
          "all separately preregistered swapped-cutoff, zero-impedance and gate mutants are killed");
    std::printf("memory/PANEL ToneControls: %d analog ports, %d control roles, source max %.6g V %.6g ohm, laws %.6g, bus %.6g V roundtrip %.6g V; mutant HP/LP %.6g V zero-R %.6g V; %d/3 killed\n",
                analogPorts, roleChecks, maxVoltage, maxResistance, maxLaw, maxBus,
                maxRoundTrip, swapDelta, zeroRDelta, mutations);
    return failures == 0 ? 0 : 1;
}
