#pragma once

#include "HardwareControl.h"

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
inline constexpr std::uint32_t modelVersion = 1;
inline constexpr std::string_view modelId = "nominal-factory-v1-seven-complete";
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
enum class Route { invalid, panel_or_memory, legacy_bridge, source_complete };
constexpr Route route(int rank, int selector, bool isolation) noexcept {
    if (rank < 0 || rank > 1 || selector < 0 || selector > 13) return Route::invalid;
    if (selector >= 11) return Route::panel_or_memory;
    return isolation && unknownMask(*address(rank, selector)) == 0
        ? Route::source_complete : Route::legacy_bridge;
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
    std::uint32_t knownCells = 0, inferredCells = 0, unknownCells = 0;
    std::uint32_t bridgeMask = retainedBridges;
    // Complete source cells still use inferred device laws. Never conflate the
    // classified-cell mask with a measured/recovered control-voltage mask.
    bool hardwareMeasured = false;
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
struct ToneControls {
    Address source {-1,-1,0,'?',{}, {}};
    Provenance provenance;
    std::array<SourceEquivalent, 26> sources {}; // open-circuit equivalents, NOT loaded buses
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
// caller's previous valid result, including incomplete/unknown source columns.
inline bool compileTone(int rank, int selector, ToneControls& result,
                        Grade hpGrade = Grade::B, Grade lpGrade = Grade::B) noexcept {
    const auto* a = address(rank, selector);
    if (!a || unknownMask(*a) != 0 || !valid(hpGrade) || !valid(lpGrade)) return false;
    ToneControls next;
    next.source = *a;
    next.provenance.knownCells = allOutputsMask;
    next.hpGrade = hpGrade; next.lpGrade = lpGrade;
    for (int output = 1; output <= 26; ++output) {
        const auto b = hardware::findBranch(a->board, a->column, output);
        const auto i = static_cast<std::size_t>(output - 1);
        if (output == 4 || output == 5) {
            bool enabled = false;
            if (!switchEnabled(b, enabled)) return false;
            next.sources[i] = {enabled ? 10.0 : 0.0, 0.0}; // logical, no 10k shunt
            if (output == 4) next.wsc.pulseEnabled = enabled;
            else next.wsc.sawEnabled = enabled;
        } else if (!analogEquivalent(b, next.sources[i])) return false;
        next.busVolts[i] = next.sources[i].volts;
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
        || tone.provenance.sourceHash != sourceFingerprint) return false;
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
