#pragma once

// KBC keyboard contour driven from the settled-DC network reference
// (IMPROVEMENT_PLAN 2.3, circuit engine). The Legacy policy keeps its frozen
// per-key weights (PerformanceControls.h).
//
// Network: src/reference/KeyboardContour.h (KBC1-2 p15 decoder and ladders,
// Panel2 p64 pots, IC14 summers), evaluated exactly per key and lever setting.
//
// DECLARED physical-key map (the source documents do not give one; see
// docs/workstreams/ws3-keyboard.md 2.3 for the alternatives and the conflict):
//  - Keys C1..C6 (foldout labels, 61 keys) form six octave groups:
//    G0 = C1, G1 = C#1..C2, G2 = C#2..C3, G3 = C#3..C4, G4 = C#4..C5,
//    G5 = C#5..C6. Group k selects octave tap OCk (L, L, (2L+R)/3, R,
//    (R+2H)/3, H). Note tap m runs C# = 0 .. C = 11 within a group; C1 is a C.
//  - Electrical Q1Q2Q3 per group: G2..G5 take the KAS6 printed codes for the
//    same physical labels (C#2..C3 010, C#3..C4 110, C#4..C5 001, C#5..C6 101,
//    after KAS6's negative-logic labels and the polarity-preserving B->Q path).
//    G1 (which contains C2) takes 110, the code the CS-80 adjustment procedure
//    requires at C2 to enable VR1. G0 (C1, the lowest single key) takes the
//    KAS6 lowest-single-key code 100 (flat at OC0).
//  - This reads the KAS6 table as the 49-key (C2..C6) layout: in that layout
//    C2 is the lowest single key with code 100, but on the 61-key CS-80 C2 is
//    the top key of the added C#1..C2 group. The literal KAS6 label "C2 -> 100"
//    therefore conflicts with the C2/VR1 trim anchor; it is reported, not hidden.
//  - Engine convention: physical key = MIDI note - 24 (MIDI 24..84), the same
//    61-key span the editor keyboard uses; clamped at the ends.
//  - Trims: VR1 = 2136.29 ohm and VR2 = 4136.29 ohm, the positions at which the
//    nominal network meets the adjustment targets (C2 LOW min -2.76 V, C5# HIGH
//    max +2.76 V) exactly; a declared "aligned unit".
// Receiving networks (R51-52 p39, as compiled in SourceFactoryTones.h): KC
// enters FH through 120k and FL through 39k, KB enters LC through 270k, like
// the other local branches; effective cutoff CV per KC volt is gt/gc
// (HP 39/120, LP 1), level gain per KB volt is levelPinPerTouch/levelUnitVolts.

#include "reference/KeyboardContour.h"
#include "SourceFactoryTones.h"

#include <algorithm>

namespace cs80::kbc
{
inline constexpr int kLowestMidiNote = 24;
inline constexpr int kKeyCount = 61;
inline constexpr double kVr1Ohms = 2136.2862;
inline constexpr double kVr2Ohms = 4136.2862;

struct KeyCode { bool q1 = false, q2 = false, q3 = false; int octaveTap = 0, noteTap = 0; };

inline KeyCode keyCode(int physicalKey) noexcept
{
    const int key = std::clamp(physicalKey, 0, kKeyCount - 1);
    const int group = key == 0 ? 0 : (key - 1) / 12 + 1;
    const int tap = key == 0 ? 11 : (key - 1) % 12;
    // Q1Q2Q3 per group (see header comment).
    static constexpr bool codes[6][3] = {
        {true, false, false},  // G0 C1         100
        {true, true, false},   // G1 C#1..C2    110 (C2 / VR1 anchor)
        {false, true, false},  // G2 C#2..C3    010 (KAS6)
        {true, true, false},   // G3 C#3..C4    110 (KAS6)
        {false, false, true},  // G4 C#4..C5    001 (KAS6)
        {true, false, true},   // G5 C#5..C6    101 (KAS6, C5# / VR2 anchor)
    };
    return { codes[group][0], codes[group][1], codes[group][2], group, tap };
}

inline int physicalKey(int midiNote) noexcept
{
    return std::clamp(midiNote - kLowestMidiNote, 0, kKeyCount - 1);
}

// KBC O1 output volts for one key and the board's LOW/HIGH levers (electrical
// travel -1..1; -1 = MINIMUM). Inputs are clamped so the reference never throws.
inline double outputVolts(int physical, double lowTravel, double highTravel) noexcept
{
    namespace kc = cs80::reference::keyboard_contour;
    const auto code = keyCode(physical);
    kc::Inputs in;
    in.q1 = code.q1; in.q2 = code.q2; in.q3 = code.q3;
    in.selectedOc = code.octaveTap;
    in.noteTap = code.noteTap;
    in.lowTravel = std::clamp(std::isfinite(lowTravel) ? lowTravel : 0.0, -1.0, 1.0);
    in.highTravel = std::clamp(std::isfinite(highTravel) ? highTravel : 0.0, -1.0, 1.0);
    in.vr1Ohms = kVr1Ohms;
    in.vr2Ohms = kVr2Ohms;
    return kc::evaluate(in).outputVolts;
}

// Unloaded receiver maps (PANEL/MEMORY ranks, no source bus model).
inline constexpr double kKcHpCvPerVolt = 39000.0 / 120000.0;
inline constexpr double kKcLpCvPerVolt = 39000.0 / 39000.0;
inline constexpr double kKbLevelPerVolt = source_factory::levelPinPerTouch / source_factory::levelUnitVolts;
} // namespace cs80::kbc
