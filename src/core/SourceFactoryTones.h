#pragma once

#include "HardwareControl.h"
#include "FactoryToneHypotheses.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

// Public-knowledge reconstruction, NOT measured hardware parity. The matrix
// remains immutable evidence. Every device/adapter law below is a named nominal
// engineering inference; see docs/FACTORY_TONE_INTEGRATION_PROPOSAL.md.
// No allocations, lazy initialization, locks, exceptions, or offline solvers.
namespace cs80::source_factory {
inline constexpr std::uint32_t modelVersion = 2;
inline constexpr std::string_view modelId = "nominal-factory-v2-all22-median-overlay-v1";
inline constexpr std::uint32_t allOutputsMask = (1u << 26) - 1u;
inline constexpr int voiceCount = 8;

struct Address {
    int rank, selector, board;
    char column;
    std::string_view name, sourceName;
};
// HardwareControl::verifiedColumns is historical fixture order, not tab order.
inline constexpr std::array<std::array<Address, 11>, 2> addresses {{
    {{{0,0,51,'A',"String 1","String 1"}, {0,1,51,'B',"String 3","String 3"},
      {0,2,51,'C',"Brass 1","Brass 1"}, {0,3,51,'D',"Flute","Flute"},
      {0,4,51,'E',"E. Piano","Electric Piano"}, {0,5,51,'F',"Clavichord 1","Clavichord 1"},
      {0,6,52,'A',"Harpsichord 1","Harpsichord 1"}, {0,7,52,'B',"Organ 1","Organ 1"},
      {0,8,52,'C',"Guitar 1","Guitar 1"}, {0,9,52,'D',"Funky 1","Funky 1"},
      {0,10,52,'E',"Funky 3","Funky 3"}}},
    {{{1,0,53,'A',"String 2","String 2"}, {1,1,53,'B',"String 4","String 4"},
      {1,2,53,'C',"Brass 2","Brass 2"}, {1,3,53,'D',"Brass 3","Brass 3"},
      {1,4,53,'E',"Bass","Bass"}, {1,5,53,'F',"Clavichord 2","Clavichord 2"},
      {1,6,54,'A',"Harpsichord 2","Harpsichord 2"}, {1,7,54,'B',"Organ 2","Organ 2"},
      {1,8,54,'C',"Guitar 2","Guitar 2"}, {1,9,54,'D',"Funky 2","Funky 2"},
      {1,10,54,'E',"Funky 4","Funky 4"}}}
}};
constexpr const Address* address(int rank, int selector) noexcept {
    return rank >= 0 && rank < 2 && selector >= 0 && selector < 11
        ? &addresses[static_cast<std::size_t>(rank)][static_cast<std::size_t>(selector)] : nullptr;
}
constexpr const Address* addressByName(int rank, std::string_view name) noexcept {
    if (rank < 0 || rank > 1) return nullptr;
    for (const auto& a : addresses[static_cast<std::size_t>(rank)])
        if (a.name == name || a.sourceName == name) return &a;
    return nullptr;
}
constexpr std::uint32_t unknownMask(const Address& a) noexcept {
    std::uint32_t mask = 0;
    for (int output = 1; output <= 26; ++output)
        if (hardware::findBranch(a.board, a.column, output).kind == hardware::BranchKind::unknown)
            mask |= 1u << (output - 1);
    return mask;
}
enum class Route { invalid, panel_or_memory, legacy_bridge, source_complete, source_inferred };
constexpr bool isSourceDriven(Route value) noexcept {
    return value == Route::source_complete || value == Route::source_inferred;
}
constexpr Route route(int rank, int selector, bool isolation) noexcept {
    if (rank < 0 || rank > 1 || selector < 0 || selector > 13) return Route::invalid;
    if (selector >= 11) return Route::panel_or_memory;
    if (!isolation) return Route::legacy_bridge;
    return unknownMask(*address(rank, selector)) == 0 ? Route::source_complete : Route::source_inferred;
}

// Canonical-address FNV-1a digest, independent of fixture row/column order.
// Include raw printed units and 1e-6 source-value coordinates, not only ohms.
constexpr std::uint64_t sourceTableFingerprint() noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    const auto add = [&hash](std::uint64_t n) constexpr {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= n & 255u;
            hash *= 1099511628211ull;
            n >>= 8;
        }
    };
    for (const auto& rank : addresses) for (const auto& a : rank) {
        add(static_cast<std::uint64_t>(a.board)); add(static_cast<std::uint64_t>(a.column));
        for (int output = 1; output <= 26; ++output) {
            const auto b = hardware::findBranch(a.board, a.column, output);
            add(static_cast<std::uint64_t>(output)); add(static_cast<std::uint64_t>(b.kind));
            add(static_cast<std::uint64_t>(b.sourceUnit));
            add(static_cast<std::uint64_t>(b.sourceValue * 1000000.0 + 0.5));
        }
    }
    return hash;
}
inline constexpr std::uint64_t sourceFingerprint = sourceTableFingerprint();
static_assert(sourceFingerprint == 0xa95212770767733cull,
              "Source facts changed: review/version the source model before compiling");

// One role per source output. Names preserve receiver identity, not an asserted
// voltage transfer; rank-II ILI/ALI are NOT the rank-I TRG1/TRG2 output ports.
enum class Role {
    pwmRate, pwmDepth, pulseWidth, pulseGate, sawGate, noise,
    highpassCutoff, highpassQ, lowpassCutoff, lowpassQ,
    filterInitial, filterAttackTarget, filterAttackTime, filterDecayTime, filterReleaseTime,
    filterGain, sineGain, amplitudeAttackTime, amplitudeDecayTime, amplitudeSustain,
    amplitudeReleaseTime, level, initialBrilliance, initialLevel, afterBrilliance, afterLevel
};
inline constexpr std::array<Role, 26> roles {{
    Role::pwmRate,Role::pwmDepth,Role::pulseWidth,Role::pulseGate,Role::sawGate,Role::noise,
    Role::highpassCutoff,Role::highpassQ,Role::lowpassCutoff,Role::lowpassQ,
    Role::filterInitial,Role::filterAttackTarget,Role::filterAttackTime,Role::filterDecayTime,
    Role::filterReleaseTime,Role::filterGain,Role::sineGain,Role::amplitudeAttackTime,
    Role::amplitudeDecayTime,Role::amplitudeSustain,Role::amplitudeReleaseTime,Role::level,
    Role::initialBrilliance,Role::initialLevel,Role::afterBrilliance,Role::afterLevel
}};
inline constexpr std::array<std::array<std::string_view, 4>, 2> touchReceiverIds {{
    {{"TRG1:IBI", "TRG1:ILI", "TRG2:ABI", "TRG2:ALI"}},
    {{"TRG1:IBII", "TRG4:ILII", "TRG2:ABII", "TRG5:ALII"}}
}};
inline constexpr std::array<std::string_view, 26> boundaryLawIds {{
    "inferred-pwm-exp-0.05-to-20Hz", "inferred-SUB-PWM-linear-depth",
    "inferred-panel-duty-to-threshold-A1.525V", "ideal-M-G1-switch", "ideal-M-G2-switch",
    "inferred-noise-linear", "loaded-R39-M41-affine-HP", "inferred-linear-damping-Q",
    "loaded-R39-M41-affine-LP", "inferred-linear-damping-Q",
    "inferred-IG00152-IL-minus5u", "inferred-IG00152-AL-plus5u",
    "inferred-IG00152-AT-exp", "inferred-IG00152-1DT-exp", "inferred-IG00152-2DT-selector-exp",
    "inferred-IC4-LI-linear", "inferred-IC3-LI-linear", "inferred-IG00159-AT-exp",
    "inferred-IG00159-1DT-exp-source-1A-conflict-retained", "inferred-IG00159-SL-linear",
    "inferred-IG00159-2DT-selector-exp", "loaded-LC-IC5-normalized-linear",
    "inferred-TRG-initial-brilliance-linear", "inferred-TRG-initial-level-linear",
    "inferred-TRG-after-brilliance-linear", "inferred-TRG-after-level-linear"
}};
enum Bridge : std::uint32_t {
    idealSourceAndDiode = 1u << 0, zeroChipInputCurrents = 1u << 1,
    pulseWidthPanelProxy = 1u << 2, pwmAndNoiseLinear = 1u << 3,
    timingExponential = 1u << 4, touchLinearStiff = 1u << 5,
    cutoffAffineAlignment = 1u << 6, globalEffectiveTransports = 1u << 7,
    settledDcNoCapacitorState = 1u << 8, linearLiUnitScale = 1u << 9,
    resonanceLinearDamping = 1u << 10, idealMasterRes = 1u << 11,
    mixedSourceProxies = 1u << 12, behavioralSustain = 1u << 13
};
inline constexpr std::uint32_t retainedBridges = (1u << 14) - 1u;
struct Provenance {
    std::uint32_t version = modelVersion;
    std::uint64_t sourceHash = sourceFingerprint;
    // unknownCells is the ORIGINAL evidence exclusion mask, not absence of a
    // computable nominal hypothesis. It intentionally overlaps inferredCells.
    std::uint32_t knownCells = 0, inferredCells = 0, unknownCells = 0;
    std::uint32_t hypothesisVersion = 0, hypothesisOverrideMask = 0;
    std::uint64_t hypothesisHash = 0;
    std::uint32_t bridgeMask = retainedBridges;
    // 0 = the ideal 10 V / 0 ohm source and ideal diode of compileTone() (V1/V2).
    // >0 = BoundaryModel::version of compileToneRefined(); see below.
    std::uint32_t boundaryVersion = 0;
    // Complete source cells still use inferred device laws. Never conflate the
    // classified-cell mask with a measured/recovered control-voltage mask.
    bool hardwareMeasured = false;
    // Nonzero only for explicit electrical Memory/PANEL controls; no matrix-cell claim.
    std::uint32_t programmerMappingVersion = 0;
};

struct SourceEquivalent { double volts = 0.0, ohms = 0.0; };
inline bool valid(SourceEquivalent s) noexcept {
    return std::isfinite(s.volts) && std::isfinite(s.ohms) && s.ohms >= 0.0;
}
// Assembled companion bus: exactly ONE 10k, even when selected cell is T52/54.
// Output is untouched on failure. Switch rows MUST use the separate logic below.
inline bool analogEquivalent(hardware::Branch b, SourceEquivalent& result) noexcept {
    if (!std::isfinite(b.sourceValue) || b.sourceValue < 0.0
        || (b.sourceUnit != hardware::ResistanceUnit::ohms
            && b.sourceUnit != hardware::ResistanceUnit::kiloOhms)) return false;
    SourceEquivalent next;
    switch (b.kind) {
        case hardware::BranchKind::open:
            if (b.sourceValue != 0.0) return false;
            next = {0.0, 10000.0}; break;
        case hardware::BranchKind::direct:
            if (b.sourceValue != 0.0) return false;
            next = {10.0, 0.0}; break;
        case hardware::BranchKind::resistor: {
            const double r = b.resistanceOhms();
            if (!std::isfinite(r) || r <= 0.0) return false;
            const double x = 1.0 / (1.0 + r / 10000.0);
            next = {10.0 * x, 10000.0 * (1.0 - x)};
            break;
        }
        default: return false;
    }
    result = next;
    return true;
}
inline bool switchEnabled(hardware::Branch b, bool& result) noexcept {
    if (b.sourceValue != 0.0
        || (b.sourceUnit != hardware::ResistanceUnit::ohms
            && b.sourceUnit != hardware::ResistanceUnit::kiloOhms)) return false;
    if (b.kind == hardware::BranchKind::direct) { result = true; return true; }
    if (b.kind == hardware::BranchKind::open) { result = false; return true; }
    return false;
}
// ---------------------------------------------------------------------------
// Versioned refinement of Bridge::idealSourceAndDiode (ws6-library, 2026-10-31).
// compileTone() and the engine keep the ideal V1/V2 boundary, so the seven
// complete tones' V1 numerics are unchanged; compileToneRefined() below is an
// explicit, versioned opt-in path. Evidence (docs/PRESET_BA_CUTOFF_BRANCHES.md,
// service pages BA27/BA28, T51/T53 notes): each selected column is a 2SC458 NPN
// emitter follower whose collector sits on the printed +10.6 V rail, base fed
// through 10 k from the tone-selector line with 100 k to reference; every
// non-open matrix branch has a 1S1555 isolation diode. DECLARED INFERENCE:
// the selector-line voltage is not traced, so the nominal driver is the
// follower saturated against its rail (alternative: forward-active below it),
// and the 1S1555 is a Shockley diode with 1N4148-class surrogate parameters
// (no 1S1555 curve was obtained). Alternatives are listed with the models.
struct DriverModel {
    double openCircuitVolts = 10.0; // selected-column emitter, unloaded
    double outputOhms = 0.0;        // small-signal emitter output resistance
};
struct DiodeModel {
    double saturationAmps = 0.0, emission = 1.0, thermalVolts = 0.025852, seriesOhms = 0.0;
};
struct BoundaryModel {
    std::uint32_t version = 1;
    DriverModel driver;
    DiodeModel diode;
    std::string_view id;
};
// Nominal: 10.6 V rail minus a 0.15 V saturation drop, 20 ohm; 1N4148-class
// Shockley parameters Is 2.52 nA, n 1.752, Rs 0.568 ohm at 300 K.
inline constexpr BoundaryModel nominalBoundaryV1 {
    1, {10.45, 20.0}, {2.52e-9, 1.752, 0.025852, 0.568}, "ba-2sc458-saturated-1s1555-shockley-v1"};
// Alternative driver: selector line at the +10.6 V rail, follower forward-active
// (9.64 V base Thevenin minus Vbe and base-resistor drop; 9.09 k/(beta+1)+re).
inline constexpr BoundaryModel forwardActiveBoundaryV1 {
    1, {8.95, 50.0}, {2.52e-9, 1.752, 0.025852, 0.568}, "ba-2sc458-forward-active-1s1555-shockley-v1"};
// Alternative diode corners (forward drop about 60 mV lower / higher at 1 mA).
inline constexpr BoundaryModel lowDropDiodeBoundaryV1 {
    1, {10.45, 20.0}, {1.0e-8, 1.6, 0.025852, 0.5}, "ba-2sc458-saturated-diode-low-drop-v1"};
inline constexpr BoundaryModel highDropDiodeBoundaryV1 {
    1, {10.45, 20.0}, {1.0e-9, 2.0, 0.025852, 0.7}, "ba-2sc458-saturated-diode-high-drop-v1"};
inline bool valid(const BoundaryModel& m) noexcept {
    return m.version > 0 && std::isfinite(m.driver.openCircuitVolts) && m.driver.openCircuitVolts > 0.0
        && std::isfinite(m.driver.outputOhms) && m.driver.outputOhms >= 0.0
        && std::isfinite(m.diode.saturationAmps) && m.diode.saturationAmps > 0.0
        && std::isfinite(m.diode.emission) && m.diode.emission > 0.0
        && std::isfinite(m.diode.thermalVolts) && m.diode.thermalVolts > 0.0
        && std::isfinite(m.diode.seriesOhms) && m.diode.seriesOhms >= 0.0;
}
inline constexpr double assembledShuntOhms = 10000.0;
struct BranchSolution {
    double busVolts = 0.0, amps = 0.0, diodeVolts = 0.0, dynamicOhms = 0.0, residualAmps = 0.0;
    int iterations = 0;
    bool converged = false;
};
// Offline-grade solve of one selected branch: driver (E, Rout) -> 1S1555 ->
// matrix resistor -> bus, the bus loaded by the one assembled 10 k shunt and a
// linear receiver conductance to reference, with `injectedAmps` flowing into
// the bus from the receivers. Bracketed Newton on the diode current (the KVL
// residual is strictly decreasing), bisection when a step leaves the bracket.
inline bool solveBranch(double seriesOhms, const BoundaryModel& m, double loadSiemens,
                        double injectedAmps, BranchSolution& out) noexcept {
    if (!valid(m) || !std::isfinite(seriesOhms) || seriesOhms < 0.0 || !std::isfinite(loadSiemens)
        || loadSiemens < 0.0 || !std::isfinite(injectedAmps)) return false;
    const double is = m.diode.saturationAmps, nvt = m.diode.emission * m.diode.thermalVolts;
    const double rt = m.driver.outputOhms + seriesOhms + m.diode.seriesOhms;
    const double g = 1.0 / assembledShuntOhms + loadSiemens;
    // f(I) = E - I*Rt - Vd(I) - (I + Iinj)/G, strictly decreasing for I > -Is.
    const auto f = [&](double i) { return m.driver.openCircuitVolts - i * rt - nvt * std::log1p(i / is) - (i + injectedAmps) / g; };
    double lo = -is * (1.0 - 1e-12), hi = (m.driver.openCircuitVolts + std::abs(injectedAmps) / g) / (rt + 1.0 / g) + 1e-3;
    if (!(f(hi) < 0.0)) return false;
    double i = std::clamp((m.driver.openCircuitVolts - 0.6 - injectedAmps / g) / (rt + 1.0 / g), 0.0, hi);
    BranchSolution next;
    for (int k = 0; k < 200; ++k) {
        const double v = f(i);
        if (v > 0.0) lo = i; else hi = i;
        const double slope = -rt - nvt / (i + is) - 1.0 / g;
        double step = i - v / slope;
        if (!(step > lo && step < hi)) step = 0.5 * (lo + hi);
        next.iterations = k + 1;
        if (std::abs(step - i) <= 1e-18 + 1e-15 * std::abs(i)) { i = step; break; }
        i = step;
    }
    next.amps = i;
    next.diodeVolts = nvt * std::log1p(i / is);
    next.busVolts = (i + injectedAmps) / g;
    next.dynamicOhms = nvt / (i + is);
    // KVL mismatch expressed as a branch current.
    next.residualAmps = (m.driver.openCircuitVolts - next.diodeVolts - next.busVolts) / std::max(rt, 1e-12) - i;
    if (rt <= 1e-12) next.residualAmps = f(i) * g;
    next.converged = std::isfinite(next.busVolts) && std::abs(next.residualAmps) <= 1e-12;
    if (!next.converged) return false;
    out = next;
    return true;
}
// Thevenin equivalent of the refined source branch, linearised at the bus's
// reference operating point under `loadSiemens` (zero envelope/touch drive), in
// the same open-circuit SourceEquivalent convention as analogEquivalent().
inline bool refinedEquivalent(hardware::Branch b, const BoundaryModel& m, double loadSiemens,
                              SourceEquivalent& result) noexcept {
    if (!std::isfinite(b.sourceValue) || b.sourceValue < 0.0
        || (b.sourceUnit != hardware::ResistanceUnit::ohms
            && b.sourceUnit != hardware::ResistanceUnit::kiloOhms)) return false;
    double series = 0.0;
    switch (b.kind) {
        case hardware::BranchKind::open:
            if (b.sourceValue != 0.0) return false;
            result = {0.0, assembledShuntOhms}; // no branch: only the shunt
            return true;
        case hardware::BranchKind::direct:
            if (b.sourceValue != 0.0) return false;
            break;
        case hardware::BranchKind::resistor:
            series = b.resistanceOhms();
            if (!std::isfinite(series) || series <= 0.0) return false;
            break;
        default: return false;
    }
    BranchSolution s;
    if (!solveBranch(series, m, loadSiemens, 0.0, s)) return false;
    const double source = m.driver.outputOhms + series + m.diode.seriesOhms + s.dynamicOhms;
    const double rth = source * assembledShuntOhms / (source + assembledShuntOhms);
    result = {s.busVolts + rth * loadSiemens * s.busVolts, rth};
    return true;
}
// An inferred cell's excitation x as the equivalent branch: open, direct, or
// the resistor that gives x = 10k/(10k+R) (the manifest's own coordinate).
inline hardware::Branch hypothesisBranch(double x) noexcept {
    if (x <= 0.0) return hardware::openBranch;
    if (x >= 1.0) return hardware::directBranch;
    return hardware::ohms(assembledShuntOhms * (1.0 - x) / x);
}
// One selected column: every conducting row draws from the SAME emitter (BA27),
// so the emitter node sags with the column's total current. solveColumn finds
// that node exactly (the KCL residual is strictly decreasing in its voltage) and
// each row's branch at it. Rows: open/switch rows have conducts = false.
struct ColumnRow { bool conducts = false; double seriesOhms = 0.0, loadSiemens = 0.0, injectedAmps = 0.0; };
struct ColumnSolution {
    double emitterVolts = 0.0, residualAmps = 0.0;
    std::array<BranchSolution, 26> rows {};
    bool converged = false;
};
inline bool solveColumn(const std::array<ColumnRow, 26>& rows, const BoundaryModel& m,
                        ColumnSolution& out) noexcept {
    if (!valid(m)) return false;
    ColumnSolution next;
    const double e0 = m.driver.openCircuitVolts, rout = m.driver.outputOhms;
    const auto at = [&](double emitter, double& total, double& slope) {
        BoundaryModel row = m;
        row.driver = {emitter, 0.0};
        total = 0.0;
        slope = 0.0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (!rows[i].conducts) continue;
            if (!solveBranch(rows[i].seriesOhms, row, rows[i].loadSiemens, rows[i].injectedAmps, next.rows[i])) return false;
            total += next.rows[i].amps;
            slope += 1.0 / (rows[i].seriesOhms + m.diode.seriesOhms + next.rows[i].dynamicOhms
                            + 1.0 / (1.0 / assembledShuntOhms + rows[i].loadSiemens));
        }
        return true;
    };
    double total = 0.0, slope = 0.0;
    double emitter = e0;
    if (rout > 0.0) {
        double lo = 0.0, hi = e0;
        for (int k = 0; k < 200; ++k) {
            if (!at(emitter, total, slope)) return false;
            const double f = (e0 - emitter) / rout - total;
            if (f > 0.0) lo = emitter; else hi = emitter;
            double step = emitter + f / (1.0 / rout + slope);
            if (!(step > lo && step < hi)) step = 0.5 * (lo + hi);
            if (std::abs(step - emitter) <= 1e-15 * std::max(1.0, e0)) { emitter = step; break; }
            emitter = step;
        }
    }
    if (!at(emitter, total, slope)) return false;
    next.emitterVolts = emitter;
    next.residualAmps = rout > 0.0 ? (e0 - emitter) / rout - total : 0.0;
    next.converged = std::abs(next.residualAmps) <= 1e-12;
    if (!next.converged) return false;
    out = next;
    return true;
}
// Thevenin of row r of a solved column, linearised at its operating point: the
// emitter node's small-signal impedance is Rout in parallel with every OTHER
// conducting row's path to reference, then the row's own series path.
inline SourceEquivalent columnEquivalent(const std::array<ColumnRow, 26>& rows, const BoundaryModel& m,
                                         const ColumnSolution& column, std::size_t r) noexcept {
    if (!rows[r].conducts) return {0.0, assembledShuntOhms};
    double node = 0.0;
    if (m.driver.outputOhms > 0.0) {
        double g = 1.0 / m.driver.outputOhms;
        for (std::size_t k = 0; k < rows.size(); ++k)
            if (k != r && rows[k].conducts)
                g += 1.0 / (rows[k].seriesOhms + m.diode.seriesOhms + column.rows[k].dynamicOhms
                            + 1.0 / (1.0 / assembledShuntOhms + rows[k].loadSiemens));
        node = 1.0 / g;
    }
    const double source = node + rows[r].seriesOhms + m.diode.seriesOhms + column.rows[r].dynamicOhms;
    const double rth = source * assembledShuntOhms / (source + assembledShuntOhms);
    const double v = column.rows[r].busVolts;
    return {v + rth * (rows[r].loadSiemens * v - rows[r].injectedAmps), rth};
}

inline double unit(double volts) noexcept { return std::clamp(volts * 0.1, 0.0, 1.0); }
// Input must be finite; compile/evaluate validate before these scalar laws.
inline double attackSeconds(double volts) noexcept { return std::pow(10.0, -3.0 * unit(volts)); }
inline double decayReleaseSeconds(double volts) noexcept { return 10.0 * attackSeconds(volts); }
inline double qFromVolts(double volts) noexcept {
    return 1.0 / (0.2 + 0.18 * std::clamp(volts, 0.0, 10.0));
}
constexpr double envelopeVoltsFromState(double z) noexcept { return (5.0 / 3.0) * z; }
constexpr double envelopeStateFromVolts(double volts) noexcept { return (3.0 / 5.0) * volts; }
inline double releaseSeconds(double individualVolts, double sustainVolts, bool enabled) noexcept {
    return decayReleaseSeconds(enabled ? std::min(individualVolts, sustainVolts) : individualVolts);
}
inline constexpr double nominalWscHalfSpanVolts = 1.525;
struct WscControls {
    bool sawEnabled = false, pulseEnabled = false;
    double pulseThresholdVolts = 0.0; // A_nominal=1.525 V; shared oscillator duty-proxy bridge
};
struct FilterEnvelope {
    double initialVolts = 0.0, attackVolts = 0.0, sustainVolts = 0.0;
    double attackSeconds = 1.0, decaySeconds = 10.0, releaseSeconds = 10.0;
};
struct AmplitudeEnvelope {
    double initialRatio = 0.0, attackRatio = 1.0, sustainRatio = 0.0;
    double attackSeconds = 1.0, decaySeconds = 10.0, releaseSeconds = 10.0;
};
enum class Grade { A, B, C };
constexpr bool valid(Grade grade) noexcept { return grade == Grade::A || grade == Grade::B || grade == Grade::C; }
constexpr double shuntOhms(Grade grade, bool hp) noexcept {
    return grade == Grade::A ? (hp ? 2700.0 : 3300.0)
         : grade == Grade::B ? (hp ? 2200.0 : 2700.0) : (hp ? 1800.0 : 2200.0);
}

// Fixed source-eliminated maps of the declared G=I0=0 reference graph. Local
// ports are explicitly 0-ohm; the finite common source preserves all 8 loads.
struct CutoffSideMap {
    double pinPerLoadedBus = 0.0, pinPerTouch = 0.0, pinPerEnvelopeVolt = 0.0;
    double busBaseVolts = 0.0, busPerTouchSum = 0.0, busPerEnvelopeVoltSum = 0.0;
    double effectiveCvPerPinVolt = 0.0, referencePinVolts = 0.0, effectiveCvOffset = 0.0;
};
// Alignment metadata and offsets are model hypotheses, not physical calibration.
// C1 is explicitly interpreted as MIDI36; 2' raises the excitation by x4 while
// key CV stays at MIDI36. The resonance-specific service alignment is frozen
// by a shared model exercise, never a per-preset fit.
struct Alignment {
    int midiNote = 36;
    double feetRatio = 4.0, referenceKeyVolts = 0.125;
    double hpBusVolts = 4.0, lpBusVolts = 3.3;
    // Shared frozen model alignment: full-square settled peak-to-peak maximized
    // through an ideal continuous SVF HP+LP pair. Service HP setup uses Q=5
    // both sides, LP HIGH=10V. LP setup uses HP LOW=0V, Qhp=10/11, Qlp=5.
    // Master RES is HIGH (0V inverse bridge). No CP3 gain fitting or per-tone fit.
    // Fundamental ratios .9936652495 / 1.0062787580; independently checked
    // Fourier-series convergence and exact periodic four-state integration.
    double hpEffectiveCv = 4.05640725, lpEffectiveCv = 4.07460544;
};
inline constexpr Alignment alignment {};
// Optional grade fixtures are individually recalibrated (slope/reference track
// grade). To study uncompensated grade sensitivity, freeze the B calibration
// separately; these9 fixtures are not claimed to perform that sensitivity sweep.
inline CutoffSideMap compileCutoffSide(SourceEquivalent source, Grade grade, bool hp) noexcept {
    constexpr double gc = 1.0 / 39000.0;
    const double gt = 1.0 / (hp ? 120000.0 : 39000.0);
    const double ge = 1.0 / (hp ? 100000.0 : 47000.0);
    // KC, TF, AB, IB and SUB all retain their branches. BR and centered trim
    // are explicit stiff-zero / 0V+25k Thevenin endpoints, respectively.
    const double d = 1.0 / shuntOhms(grade, hp) + 5.0 * gt
        + 1.0 / (hp ? 680000.0 : 220000.0) + ge + gc + 1.0 / 175000.0;
    const double a = gc / d, t = gt / d, e = ge / d;
    const double denominator = 1.0 + voiceCount * source.ohms * gc * (1.0 - a);
    return {a, t, e, source.volts / denominator,
        source.ohms * gc * t / denominator, source.ohms * gc * e / denominator,
        1.0 / a, a * (hp ? alignment.hpBusVolts : alignment.lpBusVolts),
        hp ? alignment.hpEffectiveCv : alignment.lpEffectiveCv};
}
struct RankBoundary {
    SourceEquivalent qHp, qLp, level;
    double initialLevelVolts = 0.0, afterLevelVolts = 0.0;
};
struct InferenceOverride { int output = 0; double excitation = 0.0; };
// Research-only constructor input. The ordinary production compileTone uses the
// frozen nominal manifest; this is not an Engine/host parameter or second enable.
struct InferenceScenario {
    std::array<InferenceOverride, 26> overrides {};
    std::size_t count = 0;
};
struct ToneControls {
    Address source {-1,-1,0,'?',{}, {}};
    Provenance provenance;
    std::array<SourceEquivalent, 26> sources {}; // open-circuit equivalents, NOT loaded buses
    // Null for classified cells. Inferred entries point into the immutable
    // versioned manifest with every accepted basis cell and original exclusion.
    std::array<const factory_hypotheses::InferredCell*, 26> hypotheses {};
    std::array<double, 26> hypothesisExcitations {};
    std::array<double, 26> busVolts {}; // zero-envelope/touch, master RES=0V operating point
    std::uint32_t analogShuntMask = allOutputsMask & ~((1u << 3) | (1u << 4));
    WscControls wsc;
    double dutyProxy = 0.5, pwmRateHz = 0.05, pwmDepth = 0.0, noiseGain = 0.0;
    double filterPathGain = 0.0, sinePathGain = 0.0;
    FilterEnvelope filterEnvelope;
    AmplitudeEnvelope amplitudeEnvelope;
    double initialBrillianceVolts = 0.0, afterBrillianceVolts = 0.0;
    RankBoundary boundary;
    std::array<CutoffSideMap, 2> cutoff; // HP, LP
    Grade hpGrade = Grade::B, lpGrade = Grade::B;
    bool compiled = false;
};
inline double loadedQBus(SourceEquivalent source, double masterVolts) noexcept {
    const double a = source.ohms * (voiceCount / 440000.0);
    return (source.volts + a * masterVolts) / (1.0 + a);
}
inline constexpr double levelDiagonal = 1.0 / 180000.0 + 1.0 / 560000.0 + 3.0 / 270000.0;
inline constexpr double levelPinPerLoadedBus = (1.0 / 560000.0) / levelDiagonal;
inline constexpr double levelPinPerTouch = (1.0 / 270000.0) / levelDiagonal;
inline constexpr double levelUnitVolts = 10.0 * levelPinPerLoadedBus; // exactly 30/31 V
inline double loadedLevelBus(SourceEquivalent source, double touchSumVolts) noexcept {
    constexpr double g = 1.0 / 560000.0;
    return (source.volts + source.ohms * g * levelPinPerTouch * touchSumVolts)
        / (1.0 + voiceCount * source.ohms * g * (1.0 - levelPinPerLoadedBus));
}
// Compile is cheap bounded scalar work suitable for a block-boundary control
// transaction. It never invokes a throwing solver. Failed compile preserves the
// caller's previous valid result. Unknown source cells can ONLY enter through
// the separately frozen, address-matched inference manifest (never analogEquivalent).
// Receiver conductance each compiled graph attaches to a source bus at its
// reference operating point (zero envelope/touch, master RES source at 0 V):
// eight cutoff receivers for rows 7/9, eight Q ports for 8/10, eight LC ports
// for 22. Rows without a compiled downstream graph see none (gL = 0).
inline double compiledReceiverSiemens(int output, Grade hpGrade, Grade lpGrade) noexcept {
    if (output == 7 || output == 9) {
        const auto m = compileCutoffSide({0.0, 0.0}, output == 7 ? hpGrade : lpGrade, output == 7);
        return voiceCount * (1.0 / 39000.0) * (1.0 - m.pinPerLoadedBus);
    }
    if (output == 8 || output == 10) return voiceCount / 440000.0;
    if (output == 22) return voiceCount * (1.0 / 560000.0) * (1.0 - levelPinPerLoadedBus);
    return 0.0;
}
namespace detail {
inline bool compileToneWithScenario(int rank, int selector, ToneControls& result,
                                   const InferenceScenario& scenario,
                                   Grade hpGrade, Grade lpGrade,
                                   const BoundaryModel* boundary = nullptr) noexcept {
    const auto* a = address(rank, selector);
    if (!a || !valid(hpGrade) || !valid(lpGrade) || scenario.count > scenario.overrides.size()) return false;
    if (boundary != nullptr && !valid(*boundary)) return false;
    const std::uint32_t unknown = unknownMask(*a);
    std::uint32_t overrideMask = 0;
    for (std::size_t i=0;i<scenario.count;++i) {
        const auto& entry = scenario.overrides[i];
        if (entry.output < 1 || entry.output > 26 || !std::isfinite(entry.excitation)
            || entry.excitation < 0.0 || entry.excitation > 1.0) return false;
        const std::uint32_t bit = 1u << (entry.output-1);
        if ((unknown & bit) == 0 || (overrideMask & bit) != 0) return false;
        overrideMask |= bit;
    }
    ToneControls next;
    next.source = *a;
    next.provenance.knownCells = allOutputsMask & ~unknown;
    next.provenance.unknownCells = unknown;
    next.provenance.inferredCells = unknown;
    next.provenance.hypothesisOverrideMask = overrideMask;
    if (unknown != 0) {
        next.provenance.hypothesisVersion = factory_hypotheses::version;
        next.provenance.hypothesisHash = factory_hypotheses::fingerprint;
    }
    next.hpGrade = hpGrade; next.lpGrade = lpGrade;
    std::array<ColumnRow, 26> columnRows {};
    // Refined path only: record the branch; sources follow the joint column solve.
    const auto refinedRow = [&](hardware::Branch branch, int output, std::size_t i) {
        if (!std::isfinite(branch.sourceValue) || branch.sourceValue < 0.0) return false;
        if (branch.kind == hardware::BranchKind::open) { columnRows[i] = {}; return true; }
        if (branch.kind != hardware::BranchKind::direct && branch.kind != hardware::BranchKind::resistor) return false;
        const double series = branch.kind == hardware::BranchKind::resistor ? branch.resistanceOhms() : 0.0;
        if (!std::isfinite(series) || (branch.kind == hardware::BranchKind::resistor && series <= 0.0)) return false;
        columnRows[i] = {true, series, compiledReceiverSiemens(output, hpGrade, lpGrade), 0.0};
        return true;
    };
    for (int output = 1; output <= 26; ++output) {
        const auto b = hardware::findBranch(a->board, a->column, output);
        const auto i = static_cast<std::size_t>(output - 1);
        if (b.kind == hardware::BranchKind::unknown) {
            const auto* hypothesis = factory_hypotheses::find(a->board,a->column,output);
            if (!hypothesis || output == 4 || output == 5) return false;
            double x = hypothesis->nominalExcitation;
            for (std::size_t j=0;j<scenario.count;++j)
                if (scenario.overrides[j].output == output) x = scenario.overrides[j].excitation;
            next.hypotheses[i] = hypothesis;
            next.hypothesisExcitations[i] = x;
            if (boundary == nullptr)
                next.sources[i] = {10.0*x, 10000.0*(1.0-x)};
            else if (!refinedRow(hypothesisBranch(x), output, i)) return false;
        } else if (output == 4 || output == 5) {
            bool enabled = false;
            if (!switchEnabled(b, enabled)) return false;
            next.sources[i] = {enabled ? 10.0 : 0.0, 0.0}; // logical, no 10k shunt
            if (output == 4) next.wsc.pulseEnabled = enabled;
            else next.wsc.sawEnabled = enabled;
        } else if (boundary == nullptr ? !analogEquivalent(b, next.sources[i])
                   : !refinedRow(b, output, i)) return false;
        next.busVolts[i] = next.sources[i].volts;
    }
    if (boundary != nullptr) {
        // All rows of the selected column share its emitter: solve it jointly.
        ColumnSolution column;
        if (!solveColumn(columnRows, *boundary, column)) return false;
        for (std::size_t i = 0; i < columnRows.size(); ++i) {
            if (i == 3 || i == 4) continue;
            next.sources[i] = columnEquivalent(columnRows, *boundary, column, i);
            next.busVolts[i] = next.sources[i].volts;
        }
        next.provenance.bridgeMask &= ~static_cast<std::uint32_t>(idealSourceAndDiode);
        next.provenance.boundaryVersion = boundary->version;
    }
    const auto v = [&next](int output) { return next.sources[static_cast<std::size_t>(output - 1)].volts; };
    next.dutyProxy = 0.5 + 0.4 * unit(v(3));
    next.wsc.pulseThresholdVolts = nominalWscHalfSpanVolts * (1.0 - 2.0 * next.dutyProxy);
    next.pwmRateHz = 0.05 * std::pow(400.0, unit(v(1)));
    next.pwmDepth = unit(v(2)); next.noiseGain = unit(v(6));
    next.filterEnvelope = {-5.0 * unit(v(11)), 5.0 * unit(v(12)), 0.0,
        attackSeconds(v(13)), decayReleaseSeconds(v(14)), decayReleaseSeconds(v(15))};
    next.filterPathGain = unit(v(16)); next.sinePathGain = unit(v(17));
    next.amplitudeEnvelope = {0.0, 1.0, unit(v(20)),
        attackSeconds(v(18)), decayReleaseSeconds(v(19)), decayReleaseSeconds(v(21))};
    next.initialBrillianceVolts = v(23); next.afterBrillianceVolts = v(25);
    next.boundary = {next.sources[7], next.sources[9], next.sources[21], v(24), v(26)};
    next.cutoff = {compileCutoffSide(next.sources[6], hpGrade, true),
                   compileCutoffSide(next.sources[8], lpGrade, false)};
    next.busVolts[6] = next.cutoff[0].busBaseVolts;
    next.busVolts[8] = next.cutoff[1].busBaseVolts;
    next.busVolts[7] = loadedQBus(next.boundary.qHp, 0.0);
    next.busVolts[9] = loadedQBus(next.boundary.qLp, 0.0);
    next.busVolts[21] = loadedLevelBus(next.boundary.level, 0.0);
    next.compiled = true;
    result = next;
    return true;
}
} // namespace detail
inline bool compileTone(int rank, int selector, ToneControls& result,
                        Grade hpGrade = Grade::B, Grade lpGrade = Grade::B) noexcept {
    return detail::compileToneWithScenario(rank,selector,result,{},hpGrade,lpGrade);
}
inline bool compileToneForSensitivity(int rank, int selector, const InferenceScenario& scenario,
                                      ToneControls& result, Grade hpGrade = Grade::B,
                                      Grade lpGrade = Grade::B) noexcept {
    return detail::compileToneWithScenario(rank,selector,result,scenario,hpGrade,lpGrade);
}
// Versioned opt-in path: every analog source (classified or inferred cell) uses
// the refined BA driver + 1S1555 boundary, linearised at the compiled graph's
// reference operating point. Provenance drops idealSourceAndDiode and records
// boundaryVersion. NOT used by the engine; compileTone() is unchanged.
inline bool compileToneRefined(int rank, int selector, ToneControls& result,
                               const BoundaryModel& boundary = nominalBoundaryV1,
                               Grade hpGrade = Grade::B, Grade lpGrade = Grade::B) noexcept {
    return detail::compileToneWithScenario(rank,selector,result,{},hpGrade,lpGrade,&boundary);
}
// "Visible candidates" sensitivity scenario (factory_hypotheses::review): every
// inferred cell of the tone at its re-examined candidate reading instead of the
// same-output median. For compileToneForSensitivity; never a production path.
inline InferenceScenario visibleCandidateScenario(int rank, int selector) noexcept {
    InferenceScenario scenario;
    const auto* a = address(rank, selector);
    if (a == nullptr) return scenario;
    for (int output = 1; output <= 26; ++output)
        if (const auto* r = factory_hypotheses::findReview({a->board, a->column, output}))
            scenario.overrides[scenario.count++] = {output, r->candidate.excitation};
    return scenario;
}

// PANEL/memory remain on their legacy audio adapter. These proxies merely retain
// the other rank's represented ports in a mixed source receiver network.
inline bool bridgeBoundary(double hpfRes, double lpfRes, double level,
                           double initialLevel, double afterLevel, RankBoundary& result) noexcept {
    if (!std::isfinite(hpfRes) || !std::isfinite(lpfRes) || !std::isfinite(level)
        || !std::isfinite(initialLevel) || !std::isfinite(afterLevel)) return false;
    const auto u = [](double x) { return std::clamp(x, 0.0, 1.0); };
    result = {{10.0 * (1.0 - u(hpfRes)), 0.0}, {10.0 * (1.0 - u(lpfRes)), 0.0},
              {10.0 * u(level), 0.0}, 10.0 * u(initialLevel), 10.0 * u(afterLevel)};
    return true;
}
struct VoiceDrives { double filterEnvelopeZ = 0.0, velocity = 0.0, pressure = 0.0; };
using RankDrives = std::array<VoiceDrives, voiceCount>;
using BothRankDrives = std::array<RankDrives, 2>;
// Bounds apply to nominal READOUT only: the caller's stored z is never modified.
// Saturation is reported, not represented as a sourced rail or force limit.
inline bool sanitizeDrive(VoiceDrives input, VoiceDrives& output, bool& saturated) noexcept {
    if (!std::isfinite(input.filterEnvelopeZ) || !std::isfinite(input.velocity)
        || !std::isfinite(input.pressure)) return false;
    output = {std::clamp(input.filterEnvelopeZ, -3.0, 3.0),
              std::clamp(input.velocity, 0.0, 1.0), std::clamp(input.pressure, 0.0, 1.0)};
    saturated = saturated || output.filterEnvelopeZ != input.filterEnvelopeZ
        || output.velocity != input.velocity || output.pressure != input.pressure;
    return true;
}
struct CutoffReadout {
    std::array<std::array<double, 2>, voiceCount> pinVolts {}, effectiveCv {};
    std::array<double, 2> loadedBusVolts {};
    std::uint32_t saturatedVoices = 0;
};
inline bool evaluateCutoff(const ToneControls& tone, const RankDrives& drives,
                           CutoffReadout& result) noexcept {
    if (!tone.compiled || tone.provenance.version != modelVersion
        || tone.provenance.sourceHash != sourceFingerprint
        || (tone.provenance.inferredCells != 0
            && (tone.provenance.hypothesisVersion != factory_hypotheses::version
                || tone.provenance.hypothesisHash != factory_hypotheses::fingerprint))) return false;
    std::array<double, voiceCount> touch {}, envelope {};
    double touchSum = 0.0, envelopeSum = 0.0;
    CutoffReadout next;
    for (int voice = 0; voice < voiceCount; ++voice) {
        const auto i = static_cast<std::size_t>(voice);
        VoiceDrives d; bool saturated = false;
        if (!sanitizeDrive(drives[i], d, saturated)) return false;
        if (saturated) next.saturatedVoices |= 1u << voice;
        touch[i] = tone.initialBrillianceVolts * d.velocity + tone.afterBrillianceVolts * d.pressure;
        envelope[i] = envelopeVoltsFromState(d.filterEnvelopeZ);
        touchSum += touch[i]; envelopeSum += envelope[i];
    }
    for (std::size_t side = 0; side < 2; ++side) {
        const auto& m = tone.cutoff[side];
        const double bus = m.busBaseVolts + m.busPerTouchSum * touchSum
            + m.busPerEnvelopeVoltSum * envelopeSum;
        if (!std::isfinite(bus)) return false;
        next.loadedBusVolts[side] = bus;
        for (std::size_t voice = 0; voice < voiceCount; ++voice) {
            const double pin = m.pinPerLoadedBus * bus + m.pinPerTouch * touch[voice]
                + m.pinPerEnvelopeVolt * envelope[voice];
            const double cv = m.effectiveCvOffset + m.effectiveCvPerPinVolt * (pin - m.referencePinVolts);
            if (!std::isfinite(pin) || !std::isfinite(cv)) return false;
            next.pinVolts[voice][side] = pin; next.effectiveCv[voice][side] = cv;
        }
    }
    result = next;
    return true;
}
struct SharedReadout {
    std::array<std::array<std::array<double, 2>, voiceCount>, 2> qVolts {}, q {};
    std::array<std::array<double, voiceCount>, 2> levelPinVolts {}, levelGain {};
    std::array<std::array<double, 2>, 2> qBusVolts {};
    std::array<double, 2> levelBusVolts {};
    double masterResVolts = 0.0;
    std::uint32_t saturatedVoices = 0;
    bool masterResSaturated = false;
};
inline bool valid(const RankBoundary& b) noexcept {
    return valid(b.qHp) && valid(b.qLp) && valid(b.level)
        && std::isfinite(b.initialLevelVolts) && std::isfinite(b.afterLevelVolts)
        && b.qHp.volts >= 0.0 && b.qHp.volts <= 10.0 && b.qHp.ohms <= 10000.0
        && b.qLp.volts >= 0.0 && b.qLp.volts <= 10.0 && b.qLp.ohms <= 10000.0
        && b.level.volts >= 0.0 && b.level.volts <= 10.0 && b.level.ohms <= 10000.0
        && b.initialLevelVolts >= 0.0 && b.initialLevelVolts <= 10.0
        && b.afterLevelVolts >= 0.0 && b.afterLevelVolts <= 10.0;
}
// Exact closed form of all32 Q ports and16 LC ports at nominal stiff RES/AM/KB
// and zero chip-current boundaries. The shared sources have NOT been duplicated:
// their explicit zero impedances make cross-rank loading vanish in this version.
inline bool evaluateShared(const std::array<RankBoundary, 2>& boundaries, double masterRes,
                           const BothRankDrives& drives, SharedReadout& result) noexcept {
    if (!std::isfinite(masterRes) || !valid(boundaries[0]) || !valid(boundaries[1])) return false;
    SharedReadout next;
    const double r = std::clamp(masterRes, 0.0, 1.0);
    next.masterResSaturated = r != masterRes;
    next.masterResVolts = 10.0 * (1.0 - r);
    for (std::size_t rank = 0; rank < 2; ++rank) {
        const auto& b = boundaries[rank];
        std::array<double, voiceCount> touch {};
        double sum = 0.0;
        for (int voice = 0; voice < voiceCount; ++voice) {
            const auto i = static_cast<std::size_t>(voice);
            VoiceDrives d; bool saturated = false;
            if (!sanitizeDrive(drives[rank][i], d, saturated)) return false;
            if (saturated) next.saturatedVoices |= 1u << (static_cast<int>(rank) * voiceCount + voice);
            touch[i] = b.initialLevelVolts * d.velocity + b.afterLevelVolts * d.pressure;
            sum += touch[i];
        }
        next.qBusVolts[rank] = {loadedQBus(b.qHp, next.masterResVolts),
                                loadedQBus(b.qLp, next.masterResVolts)};
        next.levelBusVolts[rank] = loadedLevelBus(b.level, sum);
        for (std::size_t voice = 0; voice < voiceCount; ++voice) {
            for (std::size_t side = 0; side < 2; ++side) {
                const double vq = 0.5 * (next.qBusVolts[rank][side] + next.masterResVolts);
                next.qVolts[rank][voice][side] = vq;
                next.q[rank][voice][side] = qFromVolts(vq);
            }
            const double pin = levelPinPerLoadedBus * next.levelBusVolts[rank] + levelPinPerTouch * touch[voice];
            next.levelPinVolts[rank][voice] = pin;
            next.levelGain[rank][voice] = pin / levelUnitVolts;
        }
    }
    result = next;
    return true;
}
} // namespace cs80::source_factory
