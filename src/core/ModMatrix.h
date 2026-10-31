#pragma once

#include "Dsp.h"

#include <algorithm>
#include <array>
#include <cmath>

// 16-slot modulation matrix of the modern layer (IMPROVEMENT_PLAN Phase 6). A software
// extension with no CS-80 counterpart: the instrument routes its controls through fixed
// circuits. Several sources ARE original controls (ribbon, pressure, keyboard contour,
// SUB, ring envelope, the EGs); the matrix only taps them. Inert under PURE CS-80.
//
// Each slot is  offset[dest] += amount * shaped(source) * shaped(via) * scale(dest),
// with a bipolar amount (a host parameter) and an optional via source (1 when none).
// Enum values are saved in session state: append only, never renumber.
namespace cs80::modern
{

inline constexpr int kModSlots = 16;

enum class ModSource : int
{
    none = 0,
    velocity,         // note-on velocity, 0..1 (velocity curve)
    polyPressure,     // this voice's pressure (poly or channel aftertouch), 0..1 (pressure curve)
    channelPressure,  // latest channel-pressure message, 0..1 (pressure curve)
    sharedPressure,   // TRG5 shared bus: highest pressure of the held keys, 0..1 (pressure curve)
    modWheel,         // CC1, 0..1 (mod wheel curve)
    ribbon,           // ribbon position, -1..1
    keyFollow,        // note across the 61-key playing surface, -1..1 (key curve)
    keyboardContour,  // KBC octave-tap ladder: HIGH weight minus LOW weight, -1..1
    sub,              // SUB oscillator output, -1..1
    ringEnvelope,     // ring modulator AD envelope, 0..1
    lfo1, lfo2, lfo3, // modern LFOs, -1..1
    mseg1, mseg2, mseg3, // 0..1, or -1..1 for a bipolar shape
    filterEnvelope,   // this rank's filter EG, -1..1 (IL below, AL above the steady cutoff)
    ampEnvelope,      // this rank's VCA EG, 0..1
    expression,       // expression pedal / CC11, 0..1
    pitchBend,        // pitch wheel, -1..1
    timbre,           // CC74 of this voice, 0..1
    noteRandom,       // a random value per note, -1..1
    constant,         // 1
    count
};
inline constexpr int kModSourceCount = (int) ModSource::count;

enum class ModDestination : int
{
    none = 0,
    pitch,        // both oscillators of the rank, semitones
    pulseWidth,   // pulse duty
    cutoff,       // HPF and LPF together, octaves (like BRILLIANCE)
    hpCutoff,     // octaves
    lpCutoff,     // octaves
    hpResonance,  // octaves of Q, clamped to the documented Q range
    lpResonance,
    level,        // VCA level control sum
    pan,
    subSpeed,     // octaves (global)
    subDepth,     // SUB modulation depth, x(1 + offset) clamped 0..2 (global)
    fx1Mix, fx2Mix, fx3Mix, // (global)
    mseg1Rate, mseg2Rate, mseg3Rate, // octaves (per voice for a poly MSEG, else global)
    count
};
inline constexpr int kModDestinationCount = (int) ModDestination::count;

enum class ModTarget : int { both = 0, rankI = 1, rankII = 2 };

struct ModSlot
{
    ModSource source = ModSource::none;
    ModSource via = ModSource::none;
    ModDestination destination = ModDestination::none;
    ModTarget target = ModTarget::both;
    bool operator==(const ModSlot&) const = default;
};

// Full-scale offset for amount = 1 and a source at 1. Engineering ranges for a modern
// control (declared design choices, not CS-80 evidence): one octave of pitch, most of the
// usable pulse duty, five octaves of cutoff (the panel cutoff sliders span about ten),
// two octaves of Q, the whole panel LEVEL range and full pan.
inline constexpr float kPitchSemitones = 12.0f;
inline constexpr float kPulseWidthRange = 0.4f;
inline constexpr float kCutoffOctaves = 5.0f;
inline constexpr float kResonanceOctaves = 2.0f;
inline constexpr float kLevelRange = 1.0f;
inline constexpr float kPanRange = 1.0f;
inline constexpr float kSubSpeedOctaves = 3.0f;
inline constexpr float kSubDepthRange = 1.0f;
inline constexpr float kFxMixRange = 1.0f;
inline constexpr float kMsegRateOctaves = 3.0f;
// Documented IG00156 Q endpoints: 5 at 0 V and 0.5 at 10 V (Yamaha CS servicing guide p34,
// CS-80 IC sheet p42; provisionalFilterQ in Dsp.h). Modulation never leaves that range.
inline constexpr float kMinModulatedQ = 0.5f;
inline constexpr float kMaxModulatedQ = 5.0f;

inline float destinationScale(ModDestination d) noexcept
{
    switch (d)
    {
        case ModDestination::pitch: return kPitchSemitones;
        case ModDestination::pulseWidth: return kPulseWidthRange;
        case ModDestination::cutoff:
        case ModDestination::hpCutoff:
        case ModDestination::lpCutoff: return kCutoffOctaves;
        case ModDestination::hpResonance:
        case ModDestination::lpResonance: return kResonanceOctaves;
        case ModDestination::level: return kLevelRange;
        case ModDestination::pan: return kPanRange;
        case ModDestination::subSpeed: return kSubSpeedOctaves;
        case ModDestination::subDepth: return kSubDepthRange;
        case ModDestination::fx1Mix:
        case ModDestination::fx2Mix:
        case ModDestination::fx3Mix: return kFxMixRange;
        case ModDestination::mseg1Rate:
        case ModDestination::mseg2Rate:
        case ModDestination::mseg3Rate: return kMsegRateOctaves;
        default: return 0.0f;
    }
}

inline bool isGlobalDestination(ModDestination d) noexcept
{
    return d >= ModDestination::subSpeed && d <= ModDestination::fx3Mix;
}
inline bool isRankDestination(ModDestination d) noexcept
{
    return d >= ModDestination::pitch && d <= ModDestination::pan;
}
inline bool isMsegRateDestination(ModDestination d) noexcept
{
    return d >= ModDestination::mseg1Rate && d <= ModDestination::mseg3Rate;
}
inline bool isRankSource(ModSource s) noexcept
{
    return s == ModSource::filterEnvelope || s == ModSource::ampEnvelope;
}

inline const char* sourceName(ModSource s) noexcept
{
    static constexpr const char* names[kModSourceCount] = {
        "None", "Velocity", "Poly pressure", "Channel pressure", "Shared pressure (TRG5)", "Mod wheel",
        "Ribbon", "Key follow", "Keyboard contour", "SUB", "Ring envelope", "LFO 1", "LFO 2", "LFO 3",
        "MSEG 1", "MSEG 2", "MSEG 3", "Filter EG", "VCA EG", "Expression", "Pitch bend", "Timbre (CC74)",
        "Note random", "Constant" };
    const int i = (int) s;
    return i >= 0 && i < kModSourceCount ? names[i] : "None";
}

inline const char* destinationName(ModDestination d) noexcept
{
    static constexpr const char* names[kModDestinationCount] = {
        "None", "Pitch", "Pulse width", "Cutoff (HP+LP)", "HP cutoff", "LP cutoff", "HP resonance",
        "LP resonance", "VCA level", "Pan", "SUB speed", "SUB depth", "FX 1 mix", "FX 2 mix", "FX 3 mix",
        "MSEG 1 rate", "MSEG 2 rate", "MSEG 3 rate" };
    const int i = (int) d;
    return i >= 0 && i < kModDestinationCount ? names[i] : "None";
}

inline const char* targetName(ModTarget t) noexcept
{
    return t == ModTarget::rankI ? "I" : t == ModTarget::rankII ? "II" : "I+II";
}

inline ModSlot sanitizeSlot(ModSlot slot) noexcept
{
    auto inRange = [](int v, int count) { return v >= 0 && v < count; };
    if (! inRange((int) slot.source, kModSourceCount)) slot.source = ModSource::none;
    if (! inRange((int) slot.via, kModSourceCount)) slot.via = ModSource::none;
    if (! inRange((int) slot.destination, kModDestinationCount)) slot.destination = ModDestination::none;
    if (! inRange((int) slot.target, 3)) slot.target = ModTarget::both;
    return slot;
}

// Keyboard modulation page: response curves for the controller-derived matrix sources.
// shape 0 is linear (exactly the input); +1 is soft (x^(1/4), small motions do more), -1
// is hard (x^4). An interface-layer adapter for different MIDI controllers, labelled as
// such; it shapes matrix sources only and never the original touch circuits.
inline float responseCurve(float x, float shape) noexcept
{
    x = clampf(x, 0.0f, 1.0f);
    if (shape == 0.0f)
        return x;
    return std::pow(x, std::exp2(-2.0f * clampf(shape, -1.0f, 1.0f)));
}
inline float bipolarResponseCurve(float x, float shape) noexcept
{
    const float y = responseCurve(std::abs(x), shape);
    return x < 0.0f ? -y : y;
}

// Offsets consumed by the rank chain; identity when nothing is routed.
struct RankOffsets
{
    float pitch = 0.0f;       // semitones
    float pulseWidth = 0.0f;  // duty
    float hpCutoff = 0.0f;    // octaves
    float lpCutoff = 0.0f;    // octaves
    float hpQ = 1.0f;         // Q ratio
    float lpQ = 1.0f;         // Q ratio
    float level = 0.0f;       // level-control units
    float pan = 0.0f;
};

struct GlobalOffsets
{
    float subSpeedRatio = 1.0f;
    float subDepthGain = 1.0f;
    std::array<float, 3> fxMix {};
};

// Apply a Q ratio. Unmodulated (ratio exactly 1) returns q itself, so the default render
// does not move; a modulated Q stays inside the documented 0.5..5 range.
inline float modulatedQ(float q, float ratio) noexcept
{
    return ratio == 1.0f ? q : clampf(q * ratio, kMinModulatedQ, kMaxModulatedQ);
}

} // namespace cs80::modern
