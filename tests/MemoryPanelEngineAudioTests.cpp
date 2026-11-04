// RED, TEST-FIRST engine contract for original M1/M3 (rank I), M2/M4 (rank II)
// and PANEL selected source circuitry. Preregistered before Engine audio work
// at tests/parity/tolerances-v2/memory-panel-engine-audio-v1.json.
// Expected to FAIL until the separate circuit-only Engine route is connected.
#include "core/Engine.h"
#include "core/MemoryPanelControlBridge.h"
#include "core/MemoryPanelToneAdapter.h"
#include "reference/MemoryPanelNetwork.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace cs80;
namespace mp = cs80::memory_panels;
namespace cb = cs80::memory_panels::control_bridge;
namespace ta = cs80::memory_panels::tone_adapter;
namespace sf = cs80::source_factory;
namespace truth = cs80::reference::memory_panel;

namespace {
int failures = 0;
void check(bool good, const char* label)
{
    if (!good) { ++failures; if (failures < 38) std::printf("FAIL: %s\n", label); }
}
double gate(const std::string& text, const char* key)
{
    const auto stage = text.find("\"memory_panel_engine_audio_v1\"");
    const auto at = stage == std::string::npos ? stage : text.find("\"" + std::string(key) + "\"", stage);
    if (at == std::string::npos) { std::printf("Missing preregistered %s\n", key); std::exit(2); }
    return std::strtod(text.c_str() + text.find(':', at) + 1, nullptr);
}
void near(double actual, double expected, double limit, const char* what)
{ check(std::isfinite(actual) && std::abs(actual - expected) <= limit, what); }
ControlFrame frame(bool originalCircuit)
{
    ControlFrame f;
    f.effectiveValues[P_parityMode] = originalCircuit ? 1.0f : 0.0f;
    f.effectiveValues[P_sourcePerformance] = 0.0f;
    f.effectiveValues[P_oversampling] = 0.0f;
    f.effectiveValues[P_resonance] = 0.37f;
    f.effectiveValues[P_subVcf] = f.effectiveValues[P_subVca] = 0.0f;
    f.effectiveValues[P_subAtVcf] = f.effectiveValues[P_subAtVco] = 0.0f;
    f.effectiveValues[P_ringAmount] = f.effectiveValues[P_chorusOn] = 0.0f;
    f.hardwareSources = {11, 12};
    for (int rank = 0; rank < 2; ++rank)
        for (int output = 1; output <= 26; ++output)
        {
            const auto param = cb::rowParameter[static_cast<std::size_t>(output - 1)];
            const auto& def = kChannelDefs[param];
            // Distinct rank I vs II banks. The test, not the production
            // converter, constructs host-space NormalisableRange positions.
            const double position = mp::isSwitchRow(output)
                ? ((output + rank) & 1 ? 1.0 : 0.0)
                : 0.10 + 0.80 * ((output * 5 + rank * 11 + f.hardwareSources[rank] * 3) % 19) / 18.0;
            const double physical = def.min + (def.max - def.min) * std::pow(position, def.skew);
            f.effectiveValues[chParam(rank, param)] = static_cast<float>(physical);
        }
    return f;
}
std::vector<float> render(Engine& e, int n)
{
    std::vector<float> pcm(static_cast<std::size_t>(2 * n));
    e.render(pcm.data(), n);
    return pcm;
}
bool same(Engine::RankAudioState a, Engine::RankAudioState b)
{
    return a.phase == b.phase && a.hpf.ic1 == b.hpf.ic1 && a.hpf.ic2 == b.hpf.ic2
        && a.lpf.ic1 == b.lpf.ic1 && a.lpf.ic2 == b.lpf.ic2
        && a.hpCircuit.input == b.hpCircuit.input
        && a.hpCircuit.feedback == b.hpCircuit.feedback
        && a.hpCircuit.bp == b.hpCircuit.bp && a.hpCircuit.lp == b.hpCircuit.lp
        && a.lpCircuit.input == b.lpCircuit.input
        && a.lpCircuit.feedback == b.lpCircuit.feedback
        && a.lpCircuit.bp == b.lpCircuit.bp && a.lpCircuit.lp == b.lpCircuit.lp;
}
bool same(ReconstructedOscillator::State a, ReconstructedOscillator::State b)
{
    return a.charge == b.charge && a.resetRemainingSeconds == b.resetRemainingSeconds
        && a.inputBaselineVolts == b.inputBaselineVolts
        && a.triangleBaselineVolts == b.triangleBaselineVolts
        && a.heldFrequencyHz == b.heldFrequencyHz
        && a.heldThresholdVolts == b.heldThresholdVolts
        && a.elapsedSeconds == b.elapsedSeconds
        && a.sawEnabled == b.sawEnabled && a.pulseEnabled == b.pulseEnabled
        && a.residual == b.residual && a.residualCursor == b.residualCursor
        && a.resetCount == b.resetCount;
}
bool same(Engine::RankEnvelopeState a, Engine::RankEnvelopeState b)
{
    return a.filter.stage == b.filter.stage && a.filter.value == b.filter.value
        && a.filter.elapsed == b.filter.elapsed
        && a.amplitude.stage == b.amplitude.stage
        && a.amplitude.value == b.amplitude.value
        && a.amplitude.elapsed == b.amplitude.elapsed;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) { std::printf("usage: %s preregistered-tolerances.json\n", argv[0]); return 2; }
    std::ifstream input(argv[1]);
    check(input.good(), "preregistered memory Engine audio truth file exists");
    std::stringstream buffer; buffer << input.rdbuf();
    const auto json = buffer.str();
    const double tolBus = gate(json, "zero_drive_bus_vs_truth_volts");
    const double tolCutoff = gate(json, "cutoff_pin_and_effective_cv_volts");
    const double tolShared = gate(json, "shared_q_and_level_readout_absolute");
    const double tolLegacy = gate(json, "legacy_pcm_max_absolute");
    const double tolZeroFrame = gate(json, "zero_frame_state_max_absolute");
    const double minAudio = gate(json, "memory_audio_pcm_peak_min");
    const double minStiff = gate(json, "mutant_stiff_hp_cv_delta_min_volts");
    const double minSwapped = gate(json, "mutant_swapped_memory_hp_cv_delta_min_volts");
    const double minDisabled = gate(json, "mutant_disabled_source_route_count_min");
    const double minSwitch = gate(json, "mutant_reversed_waveform_gate_count_min");

    // Never change the original legacy programmer model: the new route is
    // sound-active ONLY for circuit/PURE policies. Byte-for-byte PCM is the
    // exact gate, not a floating-point epsilon comparison.
    const auto legacyFrame = frame(false);
    Engine legacy, framedLegacy;
    legacy.prepare(48000.0); framedLegacy.prepare(48000.0);
    legacy.setParams(legacyFrame.effectiveValues);
    check(framedLegacy.setControlFrame(legacyFrame), "legacy frame with memory selectors accepted");
    check(framedLegacy.getSourceToneInfo(0).route == sf::Route::panel_or_memory
          && framedLegacy.getSourceToneInfo(1).route == sf::Route::panel_or_memory,
          "Legacy memory/PANEL remain exact old panel_or_memory route");
    legacy.noteOn(36, .82f); framedLegacy.noteOn(36, .82f);
    const auto legacyPcm = render(legacy, 1024);
    const auto framedPcm = render(framedLegacy, 1024);
    double maxLegacy = 0.0;
    for (std::size_t i = 0; i < legacyPcm.size(); ++i)
    {
        maxLegacy = std::max(maxLegacy, std::abs(static_cast<double>(legacyPcm[i]) - framedPcm[i]));
        check(std::bit_cast<std::uint32_t>(legacyPcm[i]) == std::bit_cast<std::uint32_t>(framedPcm[i]),
              "Legacy programmer frames remain byte-identical to setParams path");
    }
    check(maxLegacy <= tolLegacy, "legacy PCM tolerance is exactly zero");

    const auto f = frame(true);
    Engine circuit;
    circuit.prepare(48000.0);
    check(circuit.setControlFrame(f), "circuit/PURE M1+M4 source frame accepted");
    int disabledRouteMutants = 0;
    for (int rank = 0; rank < 2; ++rank)
    {
        const auto info = circuit.getSourceToneInfo(rank);
        const bool active = sf::isSourceDriven(info.route)
            && info.route != sf::Route::panel_or_memory
            && info.selector == f.hardwareSources[rank];
        check(active, "circuit/PURE memory source must be explicitly source-driven");
        // A deliberate no-source-route mutant (the previous Engine behavior)
        // would leave this readout and provenance path inactive.
        disabledRouteMutants += static_cast<int>(active);
        check(!info.provenance.hardwareMeasured && info.provenance.knownCells == 0
              && info.provenance.unknownCells == 0,
              "memory/PANEL provenance cannot claim verified factory cells");
    }
    check(disabledRouteMutants >= minDisabled,
          "mutation: disabled original-memory source route is rejected");
    const auto sourceCompiles = circuit.getSourceCompileCount();
    check(sourceCompiles >= 2, "both memory source ranks compiled exactly once at initial frame");
    check(circuit.setControlFrame(f) && circuit.getSourceCompileCount() == sourceCompiles,
          "unchanged 26 memory controls must not rerun the circuit compiler");

    std::array<sf::ToneControls, 2> expectedTones;
    std::array<sf::RankBoundary, 2> expectedBoundaries;
    double maxZeroBus = 0.0;
    for (int rank = 0; rank < 2; ++rank)
    {
        const auto positions = cb::positions(f.effectiveValues, rank);
        check(ta::compile(rank, f.hardwareSources[rank], positions, expectedTones[rank]),
              "independent nonaudio 26-port ToneControls memory oracle compiled");
        expectedBoundaries[rank] = expectedTones[rank].boundary;
        truth::Setup setup;
        setup.rank = rank;
        setup.selector = f.hardwareSources[rank];
        setup.banks[static_cast<std::size_t>(f.hardwareSources[rank] - 11)] = positions;
        const auto fullNetwork = truth::solve(setup);
        check(fullNetwork.converged, "independent 94-node reference converges at exact selected memory bank");
        for (int output = 1; output <= 26; ++output)
        {
            if (mp::isSwitchRow(output) || !fullNetwork.converged) continue;
            const auto i = static_cast<std::size_t>(output - 1);
            const double diff = std::abs(expectedTones[rank].busVolts[i] - fullNetwork.busVolts[i]);
            maxZeroBus = std::max(maxZeroBus, diff);
            check(diff <= tolBus, "selected zero-drive source bus retains prereg 2mV Newton truth");
        }
    }

    circuit.noteOn(36, .8f);
    circuit.noteOn(43, .7f);
    auto pcm = render(circuit, 4096);
    double peakAudio = 0.0;
    for (float sample : pcm) peakAudio = std::max(peakAudio, std::abs(static_cast<double>(sample)));
    check(peakAudio >= minAudio, "new source-driven memory/PANEL audio is not a vacuous silence test");

    sf::BothRankDrives drives {};
    for (int rank = 0; rank < 2; ++rank)
        for (int voice = 0; voice < sf::voiceCount; ++voice)
        {
            const auto info = circuit.getVoiceInfo(voice);
            drives[rank][voice] = {circuit.getRankEnvelopeState(voice, rank).filter.value,
                                   info.touchB, info.touchA};
        }
    double maxPin = 0.0, maxShared = 0.0;
    std::array<sf::CutoffReadout, 2> expectedCutoff;
    for (int rank = 0; rank < 2; ++rank)
    {
        check(sf::evaluateCutoff(expectedTones[rank], drives[rank], expectedCutoff[rank]),
              "independent memory rank HP/LP dynamic receiver network compiled");
        const auto actual = circuit.getSourceCutoffReadout(rank);
        for (int voice = 0; voice < sf::voiceCount; ++voice)
            for (int side = 0; side < 2; ++side)
            {
                const auto v = static_cast<std::size_t>(voice), s = static_cast<std::size_t>(side);
                const double pin = std::abs(actual.pinVolts[v][s] - expectedCutoff[rank].pinVolts[v][s]);
                const double cv = std::abs(actual.effectiveCv[v][s] - expectedCutoff[rank].effectiveCv[v][s]);
                maxPin = std::max({maxPin, pin, cv});
                near(actual.pinVolts[v][s], expectedCutoff[rank].pinVolts[v][s], tolCutoff,
                     "both memory ranks use exact same-sample 8-voice HP/LP pin voltage");
                near(actual.effectiveCv[v][s], expectedCutoff[rank].effectiveCv[v][s], tolCutoff,
                     "both memory ranks use exact same-sample effective CV");
            }
    }
    sf::SharedReadout expectedShared;
    check(sf::evaluateShared(expectedBoundaries, f.effectiveValues[P_resonance], drives, expectedShared),
          "independent memory shared 32-Q/16-LC network compiled");
    const auto actualShared = circuit.getSourceSharedReadout();
    for (int rank = 0; rank < 2; ++rank)
        for (int voice = 0; voice < sf::voiceCount; ++voice)
        {
            for (int side = 0; side < 2; ++side)
            {
                const auto d = std::abs(actualShared.q[rank][voice][side] - expectedShared.q[rank][voice][side]);
                maxShared = std::max(maxShared, d);
                near(actualShared.q[rank][voice][side], expectedShared.q[rank][voice][side], tolShared,
                     "both memory ranks retain shared Q receiver ownership");
            }
            const auto d = std::abs(actualShared.levelGain[rank][voice] - expectedShared.levelGain[rank][voice]);
            maxShared = std::max(maxShared, d);
            near(actualShared.levelGain[rank][voice], expectedShared.levelGain[rank][voice], tolShared,
                 "both memory ranks retain shared LC level receiver ownership");
        }

    // Deliberately wrong static and dynamic source-control mutations. They
    // do not change the production path or preregistered truth thresholds.
    sf::ToneControls stiff = expectedTones[0];
    stiff.sources[6].ohms = 0.0;
    stiff.cutoff[0] = sf::compileCutoffSide(stiff.sources[6], sf::Grade::B, true);
    sf::CutoffReadout stiffCutoff;
    check(sf::evaluateCutoff(stiff, drives[0], stiffCutoff), "zero-impedance mutant evaluated");
    const double stiffDelta = std::abs(stiffCutoff.effectiveCv[0][0] - expectedCutoff[0].effectiveCv[0][0]);
    check(stiffDelta >= minStiff, "mutation: stiff digital cutoff source violates dynamic circuit CV truth");
    sf::CutoffReadout swappedCutoff;
    check(sf::evaluateCutoff(expectedTones[1], drives[0], swappedCutoff), "wrong-rank-bank mutant evaluated");
    const double swappedDelta = std::abs(swappedCutoff.effectiveCv[0][0] - expectedCutoff[0].effectiveCv[0][0]);
    check(swappedDelta >= minSwapped, "mutation: swapping rank-I and rank-II memory bank violates HP truth");
    auto reversedFrame = f;
    for (int output : {4, 5})
    {
        const auto param = cb::rowParameter[static_cast<std::size_t>(output - 1)];
        auto& physical = reversedFrame.effectiveValues[chParam(0, param)];
        physical = physical >= 0.5f ? 0.0f : 1.0f;
    }
    const auto correctGates = mp::switchStates(0, f.hardwareSources[0],
                                               cb::positions(f.effectiveValues, 0));
    const auto wrongGates = mp::switchStates(0, reversedFrame.hardwareSources[0],
                                             cb::positions(reversedFrame.effectiveValues, 0));
    const int reversedGates = static_cast<int>(correctGates.pulse != wrongGates.pulse)
                            + static_cast<int>(correctGates.saw != wrongGates.saw);
    check(reversedGates >= minSwitch, "mutation: independent selected waveform source gates invert exactly");
    Engine wrongSwitches;
    wrongSwitches.prepare(48000.0);
    check(wrongSwitches.setControlFrame(reversedFrame), "inverted live waveform source fixture accepted");
    wrongSwitches.noteOn(36, .8f); wrongSwitches.noteOn(43, .7f);
    const auto wrongPcm = render(wrongSwitches, 4096);
    double waveformChange = 0.0;
    for (std::size_t i = 0; i < pcm.size(); ++i)
        waveformChange = std::max(waveformChange,
                                  std::abs(static_cast<double>(pcm[i]) - wrongPcm[i]));
    check(waveformChange >= minAudio,
          "mutation: inverting only original pulse/saw contacts must audibly change rendered tone");

    // A memory change under the same selector is an electrical recompilation;
    // repeated unchanged host buffers are not. Selector changes must preserve
    // the existing oscillator, VCF and EG state at zero rendered frames.
    auto modified = f;
    modified.effectiveValues[chParam(0, CP_hpfCutoff)] =
        modified.effectiveValues[chParam(0, CP_hpfCutoff)] >= 2000.0f ? 200.0f : 4200.0f;
    const auto compiledBeforeChange = circuit.getSourceCompileCount();
    check(circuit.setControlFrame(modified) && circuit.getSourceCompileCount() == compiledBeforeChange + 1,
          "changed selected memory wiper recompiles only its own rank at block boundary");
    const auto previousAudio = circuit.getRankAudioState(0, 0);
    const auto previousEnvelope = circuit.getRankEnvelopeState(0, 0);
    const auto previousOscillator = circuit.getRankOscillatorState(0, 0);
    auto newSelector = modified;
    newSelector.hardwareSources[0] = 12;
    check(circuit.setControlFrame(newSelector), "same-parameter memory-source selector switch accepted");
    check(same(previousAudio, circuit.getRankAudioState(0, 0))
          && same(previousEnvelope, circuit.getRankEnvelopeState(0, 0))
          && same(previousOscillator, circuit.getRankOscillatorState(0, 0))
          && tolZeroFrame == 0.0,
          "zero-frame memory selector change preserves exact oscillator/VCF/envelope state");

    std::printf("memory/PANEL RED Engine audio: legacy max %.6g, zero DC %.6g V, cutoff %.6g V, Q/LC %.6g, audio peak %.6g; mutant stiff %.6g V swap %.6g V gate=%d audio delta %.6g route=%d\n",
                maxLegacy, maxZeroBus, maxPin, maxShared, peakAudio,
                stiffDelta, swappedDelta, reversedGates, waveformChange, disabledRouteMutants);
    return failures == 0 ? 0 : 1;
}
