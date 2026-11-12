#pragma once
#include "core/Params.h"
#include <array>
namespace cs80 {
inline constexpr std::array kFrozen92Roster {
    int(P_volume), int(P_mix), int(P_brilliance), int(P_resonance), int(P_masterTune), int(P_detune),
    chParam(0, CP_hpfCutoff), chParam(0, CP_hpfRes), chParam(0, CP_lpfCutoff),
    chParam(0, CP_lpfRes), chParam(0, CP_vLevel), chParam(0, CP_pw),
    chParam(1, CP_hpfCutoff), chParam(1, CP_hpfRes), chParam(1, CP_lpfCutoff),
    chParam(1, CP_lpfRes), chParam(1, CP_vLevel), chParam(1, CP_pw),
    // Append capability lanes so the original eighteen keep their indices.
    chParam(0, CP_pan), chParam(1, CP_pan),
    // Original-panel SUB controls: append without moving existing lanes.
    int(P_subOriginalSpeed), int(P_subVco),
    // Complete the three continuous original SUB depth controls.
    int(P_subVcf), int(P_subVca),
    // Existing global touch amounts; keep the complete 24-lane prefix stable.
    int(P_subAtSpeed), int(P_subAtVco), int(P_subAtVcf),
    // Compatibility-mode SUB speed, distinct from original-panel speed.
    int(P_subSpeed),
    // Existing global ring controls; retain every preceding capability lane.
    int(P_ringSpeed), int(P_ringAmount),
    // Existing global envelope depth; preserve the complete 30-lane prefix.
    int(P_ringDepth),
    // Existing trigger-latched times; do not reshape an active ring envelope.
    int(P_ringAttack), int(P_ringDecay),
    // Existing global chorus controls; retain the complete 33-lane prefix.
    int(P_chorusSpeed), int(P_chorusDepth),
    // Existing global glide time; preserve the complete 35-lane prefix.
    int(P_portamento),
    // Existing global performance release; retain the 36-lane prefix.
    int(P_masterSustain),
    // Existing smoothed initial-bend amount; retain all 37 previous lanes.
    int(P_initBend),
    // Existing smoothed LOW level contour; preserve the 38-lane prefix.
    int(P_kbdLevelLow),
    // Existing HIGH level contour; retain the complete 39-lane prefix.
    int(P_kbdLevelHigh),
    // Existing LOW/HIGH brilliance contours; retain the complete 40-lane prefix.
    int(P_kbdBrillianceLow), int(P_kbdBrillianceHigh),
    // Existing rank INITIAL TOUCH amounts; preserve source authority and 42-lane prefix.
    chParam(0, CP_initBrill), chParam(0, CP_initLevel),
    chParam(1, CP_initBrill), chParam(1, CP_initLevel),
    // Existing rank AFTER TOUCH amounts; preserve source authority and 46-lane prefix.
    chParam(0, CP_afterBrill), chParam(0, CP_afterLevel),
    chParam(1, CP_afterBrill), chParam(1, CP_afterLevel),
    // Existing rank PWM controls; preserve source authority and 50-lane prefix.
    chParam(0, CP_pwmSpeed), chParam(0, CP_pwmDepth),
    chParam(1, CP_pwmSpeed), chParam(1, CP_pwmDepth),
    // Existing filtered-path gains; preserve the complete 54-lane prefix.
    chParam(0, CP_vcfLevel), chParam(1, CP_vcfLevel),
    // Existing live envelope targets; preserve the 56-lane prefix.
    chParam(0, CP_vS), chParam(1, CP_vS),
    // Existing filter release targets; preserve the 58-lane prefix.
    chParam(0, CP_fIl), chParam(1, CP_fIl),
    // Existing filter attack targets; preserve the 60-lane prefix.
    chParam(0, CP_fAl), chParam(1, CP_fAl),
    // Existing live attack times; preserve the 62-lane prefix.
    chParam(0, CP_fA), chParam(0, CP_vA),
    chParam(1, CP_fA), chParam(1, CP_vA),
    // Existing live decay times; preserve the 66-lane prefix.
    chParam(0, CP_fD), chParam(0, CP_vD),
    chParam(1, CP_fD), chParam(1, CP_vD),
    // Existing live release times; preserve the 70-lane prefix.
    chParam(0, CP_fR), chParam(0, CP_vR),
    chParam(1, CP_fR), chParam(1, CP_vR),
    // Existing sine/noise levels; preserve the 74-lane prefix.
    chParam(0, CP_sine), chParam(0, CP_noise),
    chParam(1, CP_sine), chParam(1, CP_noise),
    // Existing global ribbon displacement; preserve the 78-lane prefix.
    int(P_ribbon),
    // Existing global release floor; preserve the 79-lane prefix.
    int(P_keyRelease),
    // Existing global tuning reference; preserve the 80-lane prefix.
    int(P_tuningRef),
    // Existing continuous wheel depths; preserve the full 81-lane prefix.
    int(P_modWheelSub), int(P_modWheelVcf),
    // Existing modern LFO depth amounts; preserve the full 83-lane prefix.
    int(P_lfo1Depth), int(P_lfo2Depth), int(P_lfo3Depth),
    // Existing modern LFO rates; preserve the full 86-lane prefix.
    int(P_lfo1Rate), int(P_lfo2Rate), int(P_lfo3Rate),
    // Existing octave MSEG rates; preserve the complete 89-lane prefix.
    int(P_mseg1Rate), int(P_mseg2Rate), int(P_mseg3Rate)
};
static_assert(kFrozen92Roster.size() == 92);
}
