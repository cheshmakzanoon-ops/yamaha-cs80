#pragma once

#include "MemoryPanelSourceEquivalent.h"

#include <cmath>
#include <cstdint>
#include <string_view>

// AUDIO-INERT source/receiver adapter for selected original memory/PANEL
// controls. It projects their 26 electrical row outputs into the existing
// circuit-engine ToneControls laws, without changing the frozen factory
// compileTone() path or registering an Engine route. The selected potentiometer
// network remains a declared inference (BA27/BA28, MBK63, S31-S38; see
// docs/MEMORY_PANEL_THEVENIN_EVIDENCE.md). No hardware voltage was measured.
namespace cs80::memory_panels::tone_adapter
{
inline constexpr std::uint32_t modelVersion = 1;
inline constexpr std::string_view modelId = "memory-panel-source-to-tonecontrols-v1";

inline constexpr std::string_view selectedName(int rank, int selector) noexcept
{
    if (rank < 0 || rank > 1) return {};
    if (selector == 11) return rank == 0 ? "Memory 1" : "Memory 2";
    if (selector == 12) return rank == 0 ? "Memory 3" : "Memory 4";
    if (selector == 13) return rank == 0 ? "Panel I" : "Panel II";
    return {};
}

// Transactional: a failed input never modifies `result`. `source` has already
// eliminated the diode/pot network at zero voice drive, retaining the actual
// assembled 10k bus shunt and removing ONLY downstream receiver loading.
inline bool compileResolved(int rank, int selector,
                            const source_equivalent::Result& source,
                            sf::ToneControls& result,
                            sf::Grade hpGrade = sf::Grade::B,
                            sf::Grade lpGrade = sf::Grade::B) noexcept
{
    if (rank < 0 || rank > 1 || selector < 11 || selector > 13
        || !source.ok || !sf::valid(hpGrade) || !sf::valid(lpGrade))
        return false;
    // A source compiler must not silently turn the two original digital
    // waveform contacts into analog voltages or contradict its gate states.
    for (int output : {4, 5})
    {
        const auto& port = source.ports[static_cast<std::size_t>(output - 1)];
        const bool enabled = output == 4 ? source.switches.pulse : source.switches.saw;
        if (port.analog || port.source.ohms != 0.0
            || port.source.volts != (enabled ? 10.0 : 0.0))
            return false;
    }

    sf::ToneControls next;
    const auto name = selectedName(rank, selector);
    // A synthetic, explicitly non-factory address; consumers must inspect
    // sourceRoutes, not board 51-54, to identify a memory/PANEL source.
    next.source = {rank, selector, -1, selector == 13 ? 'P' : 'M', name, name};
    next.provenance.knownCells = 0;
    next.provenance.unknownCells = 0;
    next.provenance.inferredCells = 0; // factory cell masks are inapplicable
    // Existing source-factory consumer checksum is a model compatibility tag;
    // it does NOT classify these source banks as measured factory columns.
    next.provenance.bridgeMask &= ~static_cast<std::uint32_t>(sf::idealSourceAndDiode);
    next.provenance.boundaryVersion = source_equivalent::modelVersion;
    next.hpGrade = hpGrade;
    next.lpGrade = lpGrade;
    for (std::size_t i = 0; i < next.sources.size(); ++i)
    {
        if (i != 3 && i != 4 && !source.ports[i].analog) return false;
        if (!sf::valid(source.ports[i].source)) return false;
        next.sources[i] = source.ports[i].source;
        next.busVolts[i] = source.ports[i].loadedBusVolts;
    }
    next.wsc = {source.switches.saw, source.switches.pulse};
    const auto volts = [&](int output) noexcept -> double
    {
        return next.sources[static_cast<std::size_t>(output - 1)].volts;
    };

    // These are the currently declared source-factory panel/receiver laws,
    // reused by name and without changing their order or the V1 compiler.
    next.dutyProxy = 0.5 + 0.4 * sf::unit(volts(3));
    next.wsc.pulseThresholdVolts = sf::nominalWscHalfSpanVolts * (1.0 - 2.0 * next.dutyProxy);
    next.pwmRateHz = 0.05 * std::pow(400.0, sf::unit(volts(1)));
    next.pwmDepth = sf::unit(volts(2));
    next.noiseGain = sf::unit(volts(6));
    next.filterEnvelope = {-5.0 * sf::unit(volts(11)), 5.0 * sf::unit(volts(12)), 0.0,
                           sf::attackSeconds(volts(13)), sf::decayReleaseSeconds(volts(14)),
                           sf::decayReleaseSeconds(volts(15))};
    next.filterPathGain = sf::unit(volts(16));
    next.sinePathGain = sf::unit(volts(17));
    next.amplitudeEnvelope = {0.0, 1.0, sf::unit(volts(20)),
                              sf::attackSeconds(volts(18)), sf::decayReleaseSeconds(volts(19)),
                              sf::decayReleaseSeconds(volts(21))};
    next.initialBrillianceVolts = volts(23);
    next.afterBrillianceVolts = volts(25);
    next.boundary = {next.sources[7], next.sources[9], next.sources[21],
                     volts(24), volts(26)};
    if (!sf::valid(next.boundary)) return false;
    next.cutoff = {sf::compileCutoffSide(next.sources[6], hpGrade, true),
                   sf::compileCutoffSide(next.sources[8], lpGrade, false)};
    next.busVolts[6] = next.cutoff[0].busBaseVolts;
    next.busVolts[8] = next.cutoff[1].busBaseVolts;
    next.busVolts[7] = sf::loadedQBus(next.boundary.qHp, 0.0);
    next.busVolts[9] = sf::loadedQBus(next.boundary.qLp, 0.0);
    next.busVolts[21] = sf::loadedLevelBus(next.boundary.level, 0.0);
    next.compiled = true;
    result = next;
    return true;
}

inline bool compile(int rank, int selector, const Positions& positions,
                    sf::ToneControls& result,
                    const Electrical& electrical = nominalElectrical,
                    sf::Grade hpGrade = sf::Grade::B,
                    sf::Grade lpGrade = sf::Grade::B) noexcept
{
    const auto source = source_equivalent::compile(rank, selector, positions, electrical);
    return compileResolved(rank, selector, source, result, hpGrade, lpGrade);
}
} // namespace cs80::memory_panels::tone_adapter
