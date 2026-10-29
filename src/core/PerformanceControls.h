#pragma once

#include <algorithm>
#include <cmath>

namespace cs80
{

// ---------------------------------------------------------------------------
// Original performance controls that earlier builds left unimplemented:
//   - keyboard contour (owner controls 9 and 10, LOW/HIGH BRILLIANCE and LEVEL)
//   - footswitch jack/contact and the sustain / portamento assigners
//   - EXP / EXP-WAH foot selector
//   - GENERAL output HIGH / LOW switch
//
// The *topology* of each route follows the evidence in docs/KEYBOARD_CONTOUR_EVIDENCE.md
// and docs/FOOT_CONTROL_EVIDENCE.md. Every numerical gain below is a named
// provisional constant: none of them is a measured transfer function, and none
// may be promoted to circuit truth without reference measurements.
// ---------------------------------------------------------------------------

// ---- Keyboard contour ------------------------------------------------------

// Software key-position convention: the 61-key playing surface used by this
// instrument's own keyboard (MIDI 24..84). It is NOT a claim about the original
// KAS/KBC physical-key code map, which the evidence ledger still lists as open.
inline constexpr int kKeyboardLowestNote = 24;
inline constexpr int kKeyboardKeyCount = 61;

// Full-scale effect of a fully deflected LOW or HIGH lever at its own end of
// the keyboard. Brilliance is added to both filters' effective control voltage;
// level is added to the VCA control sum, like the original separate KB branch.
inline constexpr float kProvisionalKeyboardBrillianceOctaves = 2.0f;
inline constexpr float kProvisionalKeyboardLevelRange = 0.4f;

struct KeyboardContourWeights
{
    float low = 0.0f;  // 1 at the bottom of the keyboard, falling to 0 at the centre
    float high = 0.0f; // 0 at the centre, rising to 1 at the top of the keyboard
};

// KBC octave-tap ladder (evidence ledger): taps OC0..OC5 are L, L, (2L+R)/3, R,
// (R+2H)/3, H relative to the neutral reference R. Expressed as a fraction of
// each lever's deflection from neutral, LOW contributes {1,1,2/3,0,0,0} and
// HIGH {0,0,0,0,2/3,1}. The six taps cover one key plus five octaves of the
// 61-key keyboard. The real ladder is stepped per octave group; this model
// linearly interpolates between group centres instead. That smoothing is a
// labelled software adaptation, not an observed property of the instrument.
inline KeyboardContourWeights keyboardContourWeights(int midiNote) noexcept
{
    constexpr float knots[6] = { 0.0f, 6.5f, 18.5f, 30.5f, 42.5f, 54.5f };
    constexpr float lowTaps[6] = { 1.0f, 1.0f, 2.0f / 3.0f, 0.0f, 0.0f, 0.0f };
    constexpr float highTaps[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 2.0f / 3.0f, 1.0f };

    const float key = (float) std::clamp(midiNote - kKeyboardLowestNote, 0, kKeyboardKeyCount - 1);
    auto sample = [&](const float* taps)
    {
        if (key <= knots[0])
            return taps[0];
        for (int i = 1; i < 6; ++i)
            if (key <= knots[i])
            {
                const float t = (key - knots[i - 1]) / (knots[i] - knots[i - 1]);
                return taps[i - 1] + (taps[i] - taps[i - 1]) * t;
            }
        return taps[5];
    };
    return { sample(lowTaps), sample(highTaps) };
}

// Cutoff displacement in octaves for both filters at this key (positive = brighter).
inline float keyboardBrillianceOctaves(float leverLow, float leverHigh,
                                       const KeyboardContourWeights& w) noexcept
{
    return (leverLow * w.low + leverHigh * w.high) * kProvisionalKeyboardBrillianceOctaves;
}

// Additive VCA control offset at this key (positive = louder).
inline float keyboardLevelOffset(float leverLow, float leverHigh,
                                 const KeyboardContourWeights& w) noexcept
{
    return (leverLow * w.low + leverHigh * w.high) * kProvisionalKeyboardLevelRange;
}

// ---- Footswitch assigners --------------------------------------------------

// Owner manual printed pp11-12,39: a switched jack behaves as an active
// footswitch when nothing is plugged in; an assigner that is OFF bypasses the
// gate. A plugged but open (unpressed) switch disables an assigned effect.
//     effectEnabled = !assigned || !plugged || pressed
// This is the discrete control-routing truth table, not a transistor transfer.
inline constexpr bool footswitchGate(bool assigned, bool plugged, bool pressed) noexcept
{
    return ! assigned || ! plugged || pressed;
}

// ---- EXP / EXP-WAH (PRA board) ----------------------------------------------

// PRA53 shows a dedicated filter/bypass audio network switched by two FETs.
// EXP passes only the dry branch; EXP-WAH passes only the processed branch.
// The processed branch is modelled as a swept resonant band-pass. Sweep range,
// Q and make-up gain stand in for unmeasured optical-resistance laws.
inline constexpr float kProvisionalWahLowHz = 350.0f;
inline constexpr float kProvisionalWahOctaves = 2.9f;
inline constexpr float kProvisionalWahQ = 4.5f;
inline constexpr float kProvisionalWahMakeup = 1.6f;

inline float wahCentreHz(float pedal) noexcept
{
    return kProvisionalWahLowHz * std::exp2(kProvisionalWahOctaves * std::clamp(pedal, 0.0f, 1.0f));
}

// ---- GENERAL output HIGH / LOW ---------------------------------------------

// No numerical level difference is documented in this repository's evidence
// set. HIGH is the pre-existing unity output; LOW is an explicit provisional
// -20 dB trim.
inline constexpr float kProvisionalLowOutputGain = 0.1f;

inline float outputLevelGain(bool high) noexcept
{
    return high ? 1.0f : kProvisionalLowOutputGain;
}

} // namespace cs80
