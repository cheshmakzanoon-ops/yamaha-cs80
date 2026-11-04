#pragma once

#include "SourceFactoryTones.h"
#include "Dsp.h"
#include "Params.h"

#include <array>
#include <cmath>
#include <cstdint>

// Memory and PANEL programmer panels as electrical sources (ws6-library,
// 2026-10-31). Control-rate compiled model; the offline truth model is
// src/reference/MemoryPanelNetwork.h and tests/MemoryPanelTests.cpp holds the
// two together with the pre-registered tolerances. Engine consumes explicit
// programmer positions through ControlFrame; the physical-unit API is separate.
//
// Source evidence (docs/PRESET_BA_CUTOFF_BRANCHES.md, PRESET_AL_RECEIVER_GRAPH.md,
// RANK_II_RECEIVER_PORTS.md; service pages BA27/BA28, MBK63, PN1):
// - Selection acts on supply endpoints, not on the outputs: BA I12/I13/I14 (rank I)
//   drive the selected-endpoint rail (VR..-3) of memory M1, M3 or PANEL; factory
//   tabs drive their T51/T52 column. Rank II uses M2/M4 and its own PANEL rail.
// - Each memory/PANEL control is a pot between that rail and the reference
//   endpoint; its wiper feeds the bus through an isolation diode (MBK memory
//   diodes; T51 alternate-input diode for PANEL). For the cutoff rows 7/9 the
//   wiper is first buffered by an NJM4558 follower (memory: IC7/IC9/IC8/IC10,
//   diode after the feedback point; PANEL: IC5/IC6 into the primed inputs).
// - The bus is the same assembled row bus with one 10 k shunt and the compiled
//   receiver loads (SourceFactoryTones.h compiledReceiverSiemens).
// DECLARED INFERENCE (alternatives in docs/workstreams/ws6-library.md):
// - pot value 50 k linear (alternatives 10 k, 100 k); 1 ohm end resistance;
// - reference endpoint ideal 0 V; follower ideal unity gain with 1 ohm output;
// - rail drivers are the same 2SC458 followers as the factory columns
//   (source_factory::nominalBoundaryV1), unselected emitters at 0 V;
// - diodes are the 1S1555 Shockley surrogate of nominalBoundaryV1.
namespace cs80::memory_panels
{
namespace sf = source_factory;

// Selector values follow the plugin: 0..10 factory tab, 11 first memory of the
// rank (I: M1, II: M2), 12 second memory (I: M3, II: M4), 13 PANEL.
enum class Source { factoryTab, memoryA, memoryB, panel };
constexpr Source sourceFor(int selector) noexcept
{
    return selector <= 10 ? Source::factoryTab : selector == 11 ? Source::memoryA
         : selector == 12 ? Source::memoryB : Source::panel;
}
// Memory bank index used by the plugin for a rank's memory selector.
constexpr int memoryBank(int rank, Source s) noexcept
{
    return s == Source::memoryA ? rank : s == Source::memoryB ? rank + 2 : -1;
}

struct Electrical
{
    double potOhms = 50000.0;
    double endOhms = 1.0;
    double followerOhms = 1.0;
    sf::BoundaryModel boundary = sf::nominalBoundaryV1;
};
inline constexpr Electrical nominalElectrical {};

constexpr bool isSwitchRow(int output) noexcept { return output == 4 || output == 5; }
constexpr bool isBufferedRow(int output) noexcept { return output == 7 || output == 9; }
inline constexpr int analogRowCount = 24;

// One bank's pot positions as electrical wiper fractions (0 = reference end,
// 1 = rail end), indexed by output-1; switch rows hold 0/1 switch states.
using Positions = std::array<double, 26>;

// Logical waveform gates (rows 4/5): only the selected source decides. An
// unselected memory/PANEL switch sits on an unpowered rail; a factory column's
// direct/open branch is its own switch (SourceFactoryTones switchEnabled).
struct Switches { bool pulse = false, saw = false; };
inline Switches switchStates(int rank, int selector, const Positions& selectedBank) noexcept
{
    Switches out;
    if (sourceFor(selector) == Source::factoryTab)
    {
        const auto* a = sf::address(rank, selector);
        bool pulse = false, saw = false;
        if (a != nullptr && sf::switchEnabled(hardware::findBranch(a->board, a->column, 4), pulse)
            && sf::switchEnabled(hardware::findBranch(a->board, a->column, 5), saw))
            out = { pulse, saw };
        return out;
    }
    return { selectedBank[3] >= 0.5, selectedBank[4] >= 0.5 };
}

struct BusResult
{
    std::array<double, 26> busVolts {};   // switch rows stay 0
    std::array<sf::SourceEquivalent, 26> sources {}; // receiver removed, one 10k shunt retained
    double railVolts = 0.0;
    int passes = 0;
    bool ok = false;
};

inline double clampedFraction(double x) noexcept { return std::isfinite(x) ? std::clamp(x, 0.0, 1.0) : 0.0; }

// Bus voltages of every analog row with the given selector state. A factory
// tab uses compileToneRefined's branch law; a memory/PANEL selection solves the
// shared rail by fixed point (each pass re-applies the rows' solved currents)
// and each row's diode branch exactly. Inactive sources are omitted: in the
// truth model their reverse-biased diodes carry only leakage.
inline BusResult compileBuses(int rank, int selector, const Positions& bank,
                              const Electrical& e = nominalElectrical) noexcept
{
    BusResult out;
    if (rank < 0 || rank > 1 || selector < 0 || selector > 13 || !sf::valid(e.boundary)
        || !(e.potOhms > 0.0) || !(e.endOhms > 0.0) || !(e.followerOhms >= 0.0))
        return out;
    const auto load = [](int output) { return sf::compiledReceiverSiemens(output, sf::Grade::B, sf::Grade::B); };
    if (sourceFor(selector) == Source::factoryTab)
    {
        sf::ToneControls tone;
        if (!sf::compileToneRefined(rank, selector, tone, e.boundary)) return out;
        for (int output = 1; output <= 26; ++output)
        {
            if (isSwitchRow(output)) continue;
            const auto i = (size_t) (output - 1);
            const auto& src = tone.sources[i];
            out.busVolts[i] = src.volts / (1.0 + src.ohms * load(output));
        }
        out.sources = tone.sources;
        out.railVolts = e.boundary.driver.openCircuitVolts;
        out.passes = 1;
        out.ok = true;
        return out;
    }
    // Rail: driver (E, Rout) into the 24 pots; rows draw extra current through
    // their wipers. Start from the unloaded divider and refine.
    double conductance = 0.0;
    for (int output = 1; output <= 26; ++output)
        if (!isSwitchRow(output)) conductance += 1.0 / (e.potOhms + 2.0 * e.endOhms);
    const double rout = e.boundary.driver.outputOhms;
    double rail = e.boundary.driver.openCircuitVolts / (1.0 + rout * conductance);
    std::array<double, 26> fraction {}, branchOhms {};
    for (int pass = 0; pass < 4; ++pass)
    {
        double railCurrent = 0.0;
        for (int output = 1; output <= 26; ++output)
        {
            if (isSwitchRow(output)) continue;
            const auto i = (size_t) (output - 1);
            const double x = clampedFraction(bank[i]);
            const double top = e.potOhms * (1.0 - x) + e.endOhms, bottom = e.potOhms * x + e.endOhms;
            // Wiper Thevenin of the pot fed from the rail node.
            const double vw = rail * bottom / (top + bottom), rw = top * bottom / (top + bottom);
            sf::BoundaryModel m = e.boundary;
            // A buffered row's follower reproduces the unloaded wiper voltage.
            m.driver = { vw, isBufferedRow(output) ? e.followerOhms : rw };
            sf::BranchSolution s;
            if (vw <= 0.0 || !sf::solveBranch(0.0, m, load(output), 0.0, s))
            {
                out.busVolts[i] = 0.0;
                railCurrent += rail / (top + bottom);
                continue;
            }
            out.busVolts[i] = s.busVolts;
            fraction[i] = bottom / (top + bottom);
            branchOhms[i] = m.driver.outputOhms + e.boundary.diode.seriesOhms + s.dynamicOhms;
            // Rail current: the pot's own divider current, plus (unbuffered
            // rows) the branch current, drawn through the top section.
            railCurrent += rail / (top + bottom)
                         + (isBufferedRow(output) ? 0.0 : s.amps * bottom / (top + bottom));
        }
        rail = e.boundary.driver.openCircuitVolts - rout * railCurrent;
        out.passes = pass + 1;
    }
    // Diagonal small-signal source impedance, including shared rail sag.
    // Other rows retain their operating-point loads. Off-diagonal dynamic
    // coupling is not represented by the existing scalar receiver adapter.
    double railConductance = conductance;
    for (int output = 1; output <= 26; ++output)
    {
        if (isSwitchRow(output) || isBufferedRow(output)) continue;
        const auto i = static_cast<size_t>(output - 1);
        const double g = 1.0 / sf::assembledShuntOhms + load(output);
        railConductance += fraction[i] * fraction[i] * g / (1.0 + branchOhms[i] * g);
    }
    const double railOhms = rout / (1.0 + rout * railConductance);
    for (int output = 1; output <= 26; ++output)
    {
        if (isSwitchRow(output)) continue;
        const auto i = static_cast<size_t>(output - 1);
        const double receiver = load(output), g = 1.0 / sf::assembledShuntOhms + receiver;
        const double denominator = 1.0 + branchOhms[i] * g;
        double loadedOhms = branchOhms[i] / denominator;
        if (!isBufferedRow(output))
            loadedOhms += fraction[i] * fraction[i] * railOhms / (denominator * denominator);
        const double sourceOhms = std::clamp(loadedOhms / (1.0 - receiver * loadedOhms),
                                             0.0, sf::assembledShuntOhms);
        out.sources[i] = { out.busVolts[i] * (1.0 + receiver * sourceOhms), sourceOhms };
    }
    out.railVolts = rail;
    out.ok = true;
    return out;
}
// The 26 source rows in service-number order. Parameter IDs and memory-bank
// ownership are unchanged; excluded footage/pan/bypass controls remain live.
inline constexpr std::array<ChannelParam, 26> rowChannels {{
    CP_pwmSpeed, CP_pwmDepth, CP_pw, CP_pulse, CP_saw, CP_noise,
    CP_hpfCutoff, CP_hpfRes, CP_lpfCutoff, CP_lpfRes, CP_fIl, CP_fAl,
    CP_fA, CP_fD, CP_fR, CP_vcfLevel, CP_sine, CP_vA, CP_vD, CP_vS,
    CP_vR, CP_vLevel, CP_initBrill, CP_initLevel, CP_afterBrill, CP_afterLevel
}};
inline constexpr std::uint32_t mappingVersion = 1;

// DECLARED interface inference: invert the existing nominal receiving laws
// into an unloaded 0..10 V control coordinate, then use that fraction of the
// physical pot. No inverse of the loaded model and no per-program fitting.
// Values beyond the declared hardware range saturate at the physical ends.
inline double positionFor(ChannelParam parameter, double value) noexcept
{
    double position = value;
    switch (parameter)
    {
        case CP_pwmSpeed: position = std::log(std::max(value, 0.05) / 0.05) / std::log(400.0); break;
        case CP_pw: position = (value - 0.5) / 0.4; break;
        case CP_hpfRes: case CP_lpfRes: position = 1.0 - value; break;
        case CP_fA: case CP_vA: position = -std::log10(std::max(value, 0.001)) / 3.0; break;
        case CP_fD: case CP_fR: case CP_vD: case CP_vR:
            position = (1.0 - std::log10(std::max(value, 0.001))) / 3.0; break;
        case CP_hpfCutoff: case CP_lpfCutoff:
        {
            const bool hp = parameter == CP_hpfCutoff;
            const double cv = std::log2(std::max(value, 0.001) / (125.0 * kReferenceKeyVolts));
            position = ((hp ? sf::alignment.hpBusVolts : sf::alignment.lpBusVolts) + cv
                      - (hp ? sf::alignment.hpEffectiveCv : sf::alignment.lpEffectiveCv)) / 10.0;
            break;
        }
        default: break;
    }
    return clampedFraction(position);
}
inline Positions positionsFromParameters(const ParamValues& values, int rank) noexcept
{
    Positions positions {};
    if (rank < 0 || rank > 1) return positions;
    for (size_t row = 0; row < rowChannels.size(); ++row)
    {
        const auto parameter = rowChannels[row];
        const auto& definition = kChannelDefs[parameter];
        const double value = std::clamp(double(values[chParam(rank, parameter)]),
                                        double(definition.min), double(definition.max));
        positions[row] = positionFor(parameter, value);
    }
    return positions;
}
inline bool valid(const Positions& positions) noexcept
{
    for (double x : positions) if (!std::isfinite(x) || x < 0.0 || x > 1.0) return false;
    return true;
}

// Compile into the same receiver adapters as source_factory::compileTone.
// Its immutable V1/V2 factory-column compiler and numerical fixtures are not
// changed. Only a frame carrying explicit programmer positions selects this.
inline bool compileTone(int rank, int selector, const Positions& positions,
                        sf::ToneControls& result) noexcept
{
    if (rank < 0 || rank > 1 || selector < 11 || selector > 13 || !valid(positions)) return false;
    const auto buses = compileBuses(rank, selector, positions);
    if (!buses.ok) return false;
    sf::ToneControls tone;
    constexpr std::array<const char*, 4> memoryNames {"M1", "M2", "M3", "M4"};
    const auto selectedSource = sourceFor(selector);
    const auto name = selectedSource == Source::panel ? "PANEL" : memoryNames[static_cast<size_t>(memoryBank(rank, selectedSource))];
    // Factory-column board/letter addresses do not describe PANEL (PN1).
    tone.source = { rank, selector, selectedSource == Source::panel ? 0 : 63, '?', name, "electrical programmer" };
    tone.sources = buses.sources;
    tone.provenance.boundaryVersion = nominalElectrical.boundary.version;
    tone.provenance.programmerMappingVersion = mappingVersion;
    tone.provenance.bridgeMask &= ~static_cast<std::uint32_t>(sf::idealSourceAndDiode);
    const auto switches = switchStates(rank, selector, positions);
    tone.wsc.pulseEnabled = switches.pulse;
    tone.wsc.sawEnabled = switches.saw;
    tone.sources[3] = { switches.pulse ? 10.0 : 0.0, 0.0 };
    tone.sources[4] = { switches.saw ? 10.0 : 0.0, 0.0 };
    const auto v = [&tone](int output) { return tone.sources[static_cast<size_t>(output - 1)].volts; };
    tone.dutyProxy = 0.5 + 0.4 * sf::unit(v(3));
    tone.wsc.pulseThresholdVolts = sf::nominalWscHalfSpanVolts * (1.0 - 2.0 * tone.dutyProxy);
    tone.pwmRateHz = 0.05 * std::pow(400.0, sf::unit(v(1)));
    tone.pwmDepth = sf::unit(v(2)); tone.noiseGain = sf::unit(v(6));
    tone.filterEnvelope = {-5.0 * sf::unit(v(11)), 5.0 * sf::unit(v(12)), 0.0,
        sf::attackSeconds(v(13)), sf::decayReleaseSeconds(v(14)), sf::decayReleaseSeconds(v(15))};
    tone.filterPathGain = sf::unit(v(16)); tone.sinePathGain = sf::unit(v(17));
    tone.amplitudeEnvelope = {0.0, 1.0, sf::unit(v(20)),
        sf::attackSeconds(v(18)), sf::decayReleaseSeconds(v(19)), sf::decayReleaseSeconds(v(21))};
    tone.initialBrillianceVolts = v(23); tone.afterBrillianceVolts = v(25);
    tone.boundary = {tone.sources[7], tone.sources[9], tone.sources[21], v(24), v(26)};
    tone.cutoff = {sf::compileCutoffSide(tone.sources[6], sf::Grade::B, true),
                   sf::compileCutoffSide(tone.sources[8], sf::Grade::B, false)};
    tone.busVolts = buses.busVolts;
    tone.compiled = true;
    result = tone;
    return true;
}
} // namespace cs80::memory_panels
