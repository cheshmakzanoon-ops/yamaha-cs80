#pragma once

#include "SourceFactoryTones.h"

#include <array>
#include <cmath>
#include <cstdint>

// Memory and PANEL programmer panels as electrical sources (ws6-library,
// 2026-10-31). Control-rate compiled model; the offline truth model is
// src/reference/MemoryPanelNetwork.h and tests/MemoryPanelTests.cpp holds the
// two together with the pre-registered tolerances. Not wired into audio.
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
    double railVolts = 0.0;
    int passes = 0;
    bool ok = false;
};

inline double clampedFraction(double x) noexcept { return std::isfinite(x) ? std::clamp(x, 0.0, 1.0) : 0.0; }

// Bus voltages of every analog row with the given selector state. A factory
// tab uses compileToneRefined's branch law; a memory/PANEL selection solves the
// shared rail by fixed point (each pass re-applies the rows' measured currents)
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
            // Rail current: the pot's own divider current, plus (unbuffered
            // rows) the branch current, drawn through the top section.
            railCurrent += rail / (top + bottom)
                         + (isBufferedRow(output) ? 0.0 : s.amps * bottom / (top + bottom));
        }
        rail = e.boundary.driver.openCircuitVolts - rout * railCurrent;
        out.passes = pass + 1;
    }
    out.railVolts = rail;
    out.ok = true;
    return out;
}
} // namespace cs80::memory_panels
