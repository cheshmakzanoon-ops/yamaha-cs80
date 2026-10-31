#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>

namespace cs80
{

// ---------------------------------------------------------------------------
// Parameter model
//
// Global parameters are indexed 0..P_numGlobals-1. Each of the two synthesis
// channels has its own copy of the channel parameter block. Keeping the table
// in one place lets the plugin, the standalone renderer, the UI, and the
// preset bank agree on names, ranges and defaults.
// ---------------------------------------------------------------------------

enum GlobalParam : int
{
    P_masterTune = 0,
    P_detune,
    P_mix,
    P_brilliance,
    P_resonance,
    P_masterSustain,
    P_volume,
    P_initBend,
    P_portamento,
    P_glideMode,
    P_sustainMode,
    P_keyRelease,
    P_ribbon,
    P_subWave,
    P_subSpeed,
    P_subVco,
    P_subVcf,
    P_subVca,
    P_subAtSpeed,
    P_subAtVco,
    P_ringSpeed,
    P_ringDepth,
    P_ringAttack,
    P_ringDecay,
    P_ringAmount,
    P_chorusOn,
    P_chorusMode,
    P_chorusSpeed,
    P_chorusDepth,
    P_outputMode,
    P_voiceDispersion,
    P_drift,
    P_analogNoise,
    P_mpeOn,
    P_pitchBendRange,
    P_mpeBendRange,
    P_tuningRef,
    P_modWheelSub,
    P_modWheelVcf,
    P_lfo1Rate,
    P_lfo1Depth,
    P_lfo1Dest,
    P_lfo2Rate,
    P_lfo2Depth,
    P_lfo2Dest,
    P_lfo3Rate,
    P_lfo3Depth,
    P_lfo3Dest,
    P_fx1Type,
    P_fx1Mix,
    P_fx2Type,
    P_fx2Mix,
    P_fx3Type,
    P_fx3Mix,
    P_arpOn,
    P_arpMode,
    P_arpRate,
    P_arpOctaves,
    P_macro1,
    P_macro2,
    P_macro3,
    P_macro4,
    P_numGlobals
};

enum ChannelParam : int
{
    CP_saw = 0,
    CP_pulse,
    CP_sine,
    CP_noise,
    CP_pw,
    CP_pwmSpeed,
    CP_pwmDepth,
    CP_feet,
    CP_hpfCutoff,
    CP_hpfRes,
    CP_hpfOn,
    CP_lpfCutoff,
    CP_lpfRes,
    CP_lpfOn,
    CP_fIl,
    CP_fAl,
    CP_fA,
    CP_fD,
    CP_fR,
    CP_vcfLevel,
    CP_vA,
    CP_vD,
    CP_vS,
    CP_vR,
    CP_vLevel,
    CP_initBrill,
    CP_initLevel,
    CP_afterBrill,
    CP_afterLevel,
    CP_pan,
    CP_count
};

enum : int
{
    P_ch1Start = P_numGlobals,
    P_ch2Start = P_ch1Start + CP_count,
    P_legacyNumParams = P_ch2Start + CP_count,
    // Extensions must follow both legacy channel blocks; never shift their IDs.
    P_subAtVcf = P_legacyNumParams,
    P_subOriginalMode,
    P_subOriginalFunction,
    P_subOriginalSpeed,
    P_subExtLevel,
    // Owner manual controls 9/10: keyboard contours (bipolar, 0 = neutral).
    P_kbdBrillianceLow,
    P_kbdBrillianceHigh,
    P_kbdLevelLow,
    P_kbdLevelHigh,
    // Footswitch jack/contact and the two independent assigners (owner pp11-12,39).
    P_footswitchPlugged,
    P_footswitchPressed,
    P_sustainAssign,
    P_glideAssign,
    // Foot controller selector: 0 = EXP, 1 = EXP-WAH. Output switch: 0 = LOW, 1 = HIGH.
    P_footMode,
    P_outputLevel,
    // W0 feature isolation; not a claim of calibrated analog parity.
    P_parityMode,
    // Opt-in source-model audio with the existing modern performance overlays.
    // Append only: parity keeps its index and its independent isolation meaning.
    P_sourcePerformance,

    // [ws1-voice] append this workstream's core parameters below this line.

    // [ws2-engine] append this workstream's core parameters below this line.

    // [ws3-keyboard] append this workstream's core parameters below this line.
    // KAS scan timing (2.2): opt-in, quantises key events to scan instants.
    P_kasScanTiming,
    // Controller response curves (2.4). INTERFACE LAYER, identity at default.
    P_velocityCurve,
    P_pressureCurve,
    P_pressureThreshold,
    // Ribbon from a MIDI CC (interface layer): off by default, CC 16 when on.
    P_ribbonCcEnable,
    P_ribbonCc,
    // Tuning source (Phase 6, modern layer): 0 12-TET, 1 Scala/KBM, 2 MTS.
    P_tuningMode,
    // Voice mode (Phase 6): 0 eight voices (authentic), 1 sixteen, 2/3/4 unison x2/x4/x8.
    P_voiceMode,
    P_unisonDetune,

    // [ws4-circuits] append this workstream's core parameters below this line.

    // [ws5-modern] append this workstream's core parameters below this line.
    // Modern modulation layer (IMPROVEMENT_PLAN Phase 6); inert under PURE CS-80.
    // LFO 1..3 waveform: 0 sine (historical), 1 triangle, 2 saw up, 3 saw down,
    // 4 square, 5 sample & hold, 6 smooth random (src/core/ModulationLfo.h).
    P_lfo1Wave,
    P_lfo2Wave,
    P_lfo3Wave,
    // MSEG 1..3 speed in octaves (-3..+3 = 0.125x..8x, linear so the default round-trips
    // exactly through host state); shapes and routing live in session state.
    P_mseg1Rate,
    P_mseg2Rate,
    P_mseg3Rate,
    // Modulation matrix slot 1..16 bipolar amount (src/core/ModMatrix.h).
    P_mod1Amount,
    P_mod2Amount,
    P_mod3Amount,
    P_mod4Amount,
    P_mod5Amount,
    P_mod6Amount,
    P_mod7Amount,
    P_mod8Amount,
    P_mod9Amount,
    P_mod10Amount,
    P_mod11Amount,
    P_mod12Amount,
    P_mod13Amount,
    P_mod14Amount,
    P_mod15Amount,
    P_mod16Amount,
    // Keyboard modulation page response curves (-1 hard .. 0 linear .. +1 soft).
    P_modVelocityCurve,
    P_modPressureCurve,
    P_modWheelCurve,
    P_modKeyCurve,
    // Modern FX per slot: model (0 classic = use fxN_type, 1 BBD ensemble, 2 spring,
    // 3 plate, 4 tape echo, 5 drive), two model controls and the drive oversampling
    // (0 = 1x .. 3 = 8x of the internal rate). Then one switch for in/out loudness
    // matching of every slot (src/core/FxModels.h, FxLoudness.h).
    P_fx1Model,
    P_fx1ParamA,
    P_fx1ParamB,
    P_fx1Oversampling,
    P_fx2Model,
    P_fx2ParamA,
    P_fx2ParamB,
    P_fx2Oversampling,
    P_fx3Model,
    P_fx3ParamA,
    P_fx3ParamB,
    P_fx3Oversampling,
    P_fxGainCompensation,

    // [ws6-library] append this workstream's core parameters below this line.

    // [ws7-panel] append this workstream's core parameters below this line.

    // [ws8-studio] append this workstream's core parameters below this line.

    P_numParams
};

// Yamaha owner manual, printed p37: 16', 8', 5 1/3', 4', 2 2/3', 2'.
// Nominal harmonic footage ratios. Actual hardware includes resistor/trim tolerances.
inline constexpr float kFeetRatios[6] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f };
// Legacy equal-tempered approximation, retained for source compatibility only.
// The oscillator no longer uses these offsets: fifth stops differ by ~1.955 cents.
inline constexpr float kFeetSemitones[6] = { -12.0f, 0.0f, 7.0f, 12.0f, 19.0f, 24.0f };

inline constexpr int channelStart(int channel) noexcept
{
    return channel == 0 ? P_ch1Start : P_ch2Start;
}

inline constexpr int chParam(int channel, int index) noexcept
{
    return channelStart(channel) + index;
}

struct ParamDef
{
    const char* id;
    const char* name;
    float min;
    float max;
    float def;
    float skew; // 1 = linear, <1 = more resolution at the low end
    const char* unit;
    const char* group;
};

inline constexpr ParamDef kGlobalDefs[] = {
    { "master_tune", "MASTER TUNE", -2.0f, 2.0f, 0.0f, 1.0f, "st", "MASTER" },
    { "detune", "DETUNE II", 0.0f, 50.0f, 7.0f, 1.0f, "ct", "MASTER" },
    { "mix", "MIX I/II", 0.0f, 1.0f, 0.5f, 1.0f, "", "MASTER" },
    { "brilliance", "BRILLIANCE", -2.0f, 2.0f, 0.0f, 1.0f, "oct", "MASTER" },
    { "resonance", "RESONANCE", 0.0f, 1.0f, 0.22f, 1.0f, "", "MASTER" },
    { "master_sustain", "MASTER SUSTAIN", 0.0f, 1.0f, 0.0f, 1.0f, "", "MASTER" },
    { "volume", "VOLUME", 0.0f, 1.0f, 0.62f, 1.0f, "", "MASTER" },
    { "init_bend", "INITIAL PITCH BEND", 0.0f, 1.0f, 0.0f, 1.0f, "", "PERFORMANCE" },
    { "portamento", "PORTA / GLISS", 0.0f, 5.0f, 0.0f, 0.5f, "s", "PERFORMANCE" },
    { "glide_mode", "GLIDE MODE", 0.0f, 2.0f, 0.0f, 1.0f, "", "PERFORMANCE" },
    { "sustain_mode", "SUSTAIN MODE", 0.0f, 1.0f, 0.0f, 1.0f, "", "PERFORMANCE" },
    { "key_release", "KEY RELEASE", 0.0f, 5.0f, 0.18f, 0.5f, "s", "PERFORMANCE" },
    { "ribbon", "RIBBON", -1.0f, 1.0f, 0.0f, 1.0f, "", "PERFORMANCE" },

    { "sub_wave", "SUB OSC WAVE", 0.0f, 5.0f, 0.0f, 1.0f, "", "SUB OSC" },
    { "sub_speed", "SUB OSC SPEED", 0.05f, 30.0f, 4.2f, 0.35f, "Hz", "SUB OSC" },
    { "sub_vco", "SUB VCO", 0.0f, 1.0f, 0.0f, 1.0f, "", "SUB OSC" },
    { "sub_vcf", "SUB VCF", 0.0f, 1.0f, 0.0f, 1.0f, "", "SUB OSC" },
    { "sub_vca", "SUB VCA", 0.0f, 1.0f, 0.0f, 1.0f, "", "SUB OSC" },
    { "sub_at_speed", "AT SPEED", 0.0f, 1.0f, 0.0f, 1.0f, "", "SUB OSC" },
    { "sub_at_vco", "AT VCO", 0.0f, 1.0f, 0.0f, 1.0f, "", "SUB OSC" },
    { "ring_speed", "RING SPEED", 0.1f, 500.0f, 2.0f, 0.3f, "Hz", "RING MOD" },
    { "ring_depth", "RING DEPTH", 0.0f, 1.0f, 0.0f, 1.0f, "", "RING MOD" },
    { "ring_attack", "RING ATTACK", 0.001f, 10.0f, 0.08f, 0.4f, "s", "RING MOD" },
    { "ring_decay", "RING DECAY", 0.001f, 10.0f, 0.45f, 0.4f, "s", "RING MOD" },
    { "ring_amount", "RING AMOUNT", 0.0f, 1.0f, 0.0f, 1.0f, "", "RING MOD" },
    { "chorus_on", "CHORUS ON", 0.0f, 1.0f, 0.0f, 1.0f, "", "CHORUS" },
    { "chorus_mode", "CHORUS / TREM", 0.0f, 1.0f, 0.0f, 1.0f, "", "CHORUS" },
    { "chorus_speed", "CHORUS SPEED", 0.05f, 8.0f, 0.55f, 0.4f, "Hz", "CHORUS" },
    { "chorus_depth", "CHORUS DEPTH", 0.0f, 1.0f, 0.55f, 1.0f, "", "CHORUS" },
    { "output_mode", "OUTPUT", 0.0f, 1.0f, 1.0f, 1.0f, "", "MASTER" },
    { "voice_dispersion", "VOICE DISPERSION", 0.0f, 1.0f, 0.35f, 1.0f, "", "VOICE" },
    { "drift", "ANALOG DRIFT", 0.0f, 1.0f, 0.25f, 1.0f, "", "VOICE" },
    { "analog_noise", "ANALOG NOISE", 0.0f, 1.0f, 0.12f, 1.0f, "", "VOICE" },
    { "mpe_on", "MPE", 0.0f, 1.0f, 0.0f, 1.0f, "", "VOICE" },
    { "pitch_bend_range", "PITCH BEND RANGE", 1.0f, 48.0f, 2.0f, 1.0f, "st", "VOICE" },
    { "mpe_bend_range", "MPE BEND RANGE", 1.0f, 96.0f, 48.0f, 1.0f, "st", "VOICE" },
    { "tuning_ref", "TUNING REF", 415.0f, 465.0f, 440.0f, 1.0f, "Hz", "VOICE" },
    { "modwheel_sub", "MOD WHEEL > SUB", 0.0f, 1.0f, 0.25f, 1.0f, "", "PERFORMANCE" },
    { "modwheel_vcf", "MOD WHEEL > VCF", 0.0f, 1.0f, 0.0f, 1.0f, "", "PERFORMANCE" },

    { "lfo1_rate", "LFO 1 RATE", 0.05f, 20.0f, 0.28f, 0.35f, "Hz", "MODULATION" },
    { "lfo1_depth", "LFO 1 DEPTH", 0.0f, 1.0f, 0.0f, 1.0f, "", "MODULATION" },
    { "lfo1_dest", "LFO 1 DEST", 0.0f, 5.0f, 0.0f, 1.0f, "", "MODULATION" },
    { "lfo2_rate", "LFO 2 RATE", 0.05f, 20.0f, 0.55f, 0.35f, "Hz", "MODULATION" },
    { "lfo2_depth", "LFO 2 DEPTH", 0.0f, 1.0f, 0.0f, 1.0f, "", "MODULATION" },
    { "lfo2_dest", "LFO 2 DEST", 0.0f, 5.0f, 0.0f, 1.0f, "", "MODULATION" },
    { "lfo3_rate", "LFO 3 RATE", 0.05f, 20.0f, 1.1f, 0.35f, "Hz", "MODULATION" },
    { "lfo3_depth", "LFO 3 DEPTH", 0.0f, 1.0f, 0.0f, 1.0f, "", "MODULATION" },
    { "lfo3_dest", "LFO 3 DEST", 0.0f, 5.0f, 0.0f, 1.0f, "", "MODULATION" },

    { "fx1_type", "FX 1", 0.0f, 5.0f, 0.0f, 1.0f, "", "FX" },
    { "fx1_mix", "FX 1 MIX", 0.0f, 1.0f, 0.0f, 1.0f, "", "FX" },
    { "fx2_type", "FX 2", 0.0f, 5.0f, 0.0f, 1.0f, "", "FX" },
    { "fx2_mix", "FX 2 MIX", 0.0f, 1.0f, 0.0f, 1.0f, "", "FX" },
    { "fx3_type", "FX 3", 0.0f, 5.0f, 0.0f, 1.0f, "", "FX" },
    { "fx3_mix", "FX 3 MIX", 0.0f, 1.0f, 0.0f, 1.0f, "", "FX" },

    { "arp_on", "ARP", 0.0f, 1.0f, 0.0f, 1.0f, "", "ARP" },
    { "arp_mode", "ARP MODE", 0.0f, 4.0f, 0.0f, 1.0f, "", "ARP" },
    { "arp_rate", "ARP RATE", 0.25f, 16.0f, 4.0f, 0.5f, "Hz", "ARP" },
    { "arp_octaves", "ARP OCTAVES", 1.0f, 4.0f, 1.0f, 1.0f, "", "ARP" },
    { "macro1", "MACRO 1", 0.0f, 1.0f, 0.5f, 1.0f, "", "MACROS" },
    { "macro2", "MACRO 2", 0.0f, 1.0f, 0.5f, 1.0f, "", "MACROS" },
    { "macro3", "MACRO 3", 0.0f, 1.0f, 0.5f, 1.0f, "", "MACROS" },
    { "macro4", "MACRO 4", 0.0f, 1.0f, 0.5f, 1.0f, "", "MACROS" }
};

inline constexpr ParamDef kChannelDefs[] = {
    { "saw", "SAW", 0.0f, 1.0f, 1.0f, 1.0f, "", "VCO" },
    { "pulse", "PULSE", 0.0f, 1.0f, 0.0f, 1.0f, "", "VCO" },
    { "sine", "SINE", 0.0f, 1.0f, 0.0f, 1.0f, "", "VCA" },
    { "noise", "NOISE", 0.0f, 1.0f, 0.0f, 1.0f, "", "VCO" },
    { "pw", "PULSE WIDTH", 0.5f, 0.9f, 0.5f, 1.0f, "", "VCO" },
    { "pwm_speed", "PWM SPEED", 0.05f, 20.0f, 0.45f, 0.35f, "Hz", "VCO" },
    { "pwm_depth", "PWM DEPTH", 0.0f, 1.0f, 0.0f, 1.0f, "", "VCO" },
    { "feet", "FEET", 0.0f, 5.0f, 1.0f, 1.0f, "", "VCO" },
    { "hpf_cutoff", "HPF CUTOFF", 20.0f, 18000.0f, 25.0f, 0.28f, "Hz", "VCF" },
    { "hpf_res", "HPF RES", 0.0f, 1.0f, 0.0f, 1.0f, "", "VCF" },
    { "hpf_on", "HPF ON", 0.0f, 1.0f, 1.0f, 1.0f, "", "VCF" },
    { "lpf_cutoff", "LPF CUTOFF", 20.0f, 18000.0f, 7200.0f, 0.28f, "Hz", "VCF" },
    { "lpf_res", "LPF RES", 0.0f, 1.0f, 0.12f, 1.0f, "", "VCF" },
    { "lpf_on", "LPF ON", 0.0f, 1.0f, 1.0f, 1.0f, "", "VCF" },
    { "f_il", "IL", 0.0f, 1.0f, 0.5f, 1.0f, "", "FILTER EG" },
    { "f_al", "AL", 0.0f, 1.0f, 0.55f, 1.0f, "", "FILTER EG" },
    { "f_a", "A", 0.001f, 20.0f, 0.035f, 0.4f, "s", "FILTER EG" },
    { "f_d", "D", 0.001f, 20.0f, 0.42f, 0.4f, "s", "FILTER EG" },
    { "f_r", "R", 0.001f, 20.0f, 0.30f, 0.4f, "s", "FILTER EG" },
    { "vcf_level", "VCF LEVEL", 0.0f, 1.0f, 0.78f, 1.0f, "", "VCA" },
    { "v_a", "A", 0.001f, 20.0f, 0.008f, 0.4f, "s", "VCA EG" },
    { "v_d", "D", 0.001f, 20.0f, 0.32f, 0.4f, "s", "VCA EG" },
    { "v_s", "S", 0.0f, 1.0f, 0.78f, 1.0f, "", "VCA EG" },
    { "v_r", "R", 0.001f, 20.0f, 0.28f, 0.4f, "s", "VCA EG" },
    { "v_level", "LEVEL", 0.0f, 1.0f, 0.72f, 1.0f, "", "VCA" },
    { "init_brill", "INIT BRILL", 0.0f, 1.0f, 0.0f, 1.0f, "", "TOUCH" },
    { "init_level", "INIT LEVEL", 0.0f, 1.0f, 0.0f, 1.0f, "", "TOUCH" },
    { "after_brill", "AFTER BRILL", 0.0f, 1.0f, 0.0f, 1.0f, "", "TOUCH" },
    { "after_level", "AFTER LEVEL", 0.0f, 1.0f, 0.0f, 1.0f, "", "TOUCH" },
    { "pan", "PAN", -1.0f, 1.0f, 0.0f, 1.0f, "", "VCA" }
};

static_assert(std::size(kGlobalDefs) == P_numGlobals, "global parameter table size mismatch");
static_assert(std::size(kChannelDefs) == CP_count, "channel parameter table size mismatch");

inline constexpr ParamDef kExtensionDefs[] = {
    { "sub_at_vcf", "AT VCF", 0.0f, 1.0f, 0.0f, 1.0f, "", "SUB OSC" },
    { "sub_original_mode", "ORIGINAL SUB MODE", 0.0f, 1.0f, 1.0f, 1.0f, "", "SUB OSC" },
    { "sub_original_function", "ORIGINAL SUB FUNCTION", 0.0f, 5.0f, 0.0f, 1.0f, "", "SUB OSC" },
    { "sub_original_speed", "ORIGINAL SUB SPEED", 0.7f, 60.0f, 4.2f, 0.35f, "Hz", "SUB OSC" },
    { "sub_ext_level", "EXT INPUT LEVEL", 0.0f, 1.0f, 0.0f, 1.0f, "", "SUB OSC" },
    { "kbd_brilliance_low", "KBD BRILLIANCE LOW", -1.0f, 1.0f, 0.0f, 1.0f, "", "KEYBOARD" },
    { "kbd_brilliance_high", "KBD BRILLIANCE HIGH", -1.0f, 1.0f, 0.0f, 1.0f, "", "KEYBOARD" },
    { "kbd_level_low", "KBD LEVEL LOW", -1.0f, 1.0f, 0.0f, 1.0f, "", "KEYBOARD" },
    { "kbd_level_high", "KBD LEVEL HIGH", -1.0f, 1.0f, 0.0f, 1.0f, "", "KEYBOARD" },
    { "footswitch_plugged", "FOOTSWITCH PLUGGED", 0.0f, 1.0f, 0.0f, 1.0f, "", "FOOT" },
    { "footswitch_pressed", "FOOTSWITCH PRESSED", 0.0f, 1.0f, 0.0f, 1.0f, "", "FOOT" },
    { "sustain_assign", "SUSTAIN ASSIGN", 0.0f, 1.0f, 0.0f, 1.0f, "", "FOOT" },
    { "glide_assign", "PORTA / GLISS ASSIGN", 0.0f, 1.0f, 0.0f, 1.0f, "", "FOOT" },
    { "foot_mode", "PEDAL EXP / WAH", 0.0f, 1.0f, 0.0f, 1.0f, "", "FOOT" },
    { "output_level", "OUTPUT HIGH / LOW", 0.0f, 1.0f, 1.0f, 1.0f, "", "MASTER" },
    { "parity_mode", "PARITY MODE (UNCALIBRATED)", 0.0f, 1.0f, 0.0f, 1.0f, "", "PERFORMANCE" },
    { "source_performance", "SOURCE PERFORMANCE (UNCALIBRATED)", 0.0f, 1.0f, 0.0f, 1.0f, "", "PERFORMANCE" },
    // [ws1-voice] append this workstream's ParamDefs below this line (same order as the enum).

    // [ws2-engine] append this workstream's ParamDefs below this line (same order as the enum).

    // [ws3-keyboard] append this workstream's ParamDefs below this line (same order as the enum).
    { "kas_scan_timing", "KAS SCAN TIMING", 0.0f, 1.0f, 0.0f, 1.0f, "", "KEYBOARD" },
    { "velocity_curve", "VELOCITY CURVE", -1.0f, 1.0f, 0.0f, 1.0f, "", "MIDI" },
    { "pressure_curve", "PRESSURE CURVE", -1.0f, 1.0f, 0.0f, 1.0f, "", "MIDI" },
    { "pressure_threshold", "PRESSURE THRESHOLD", 0.0f, 0.5f, 0.0f, 1.0f, "", "MIDI" },
    { "ribbon_cc_enable", "RIBBON MIDI CC", 0.0f, 1.0f, 0.0f, 1.0f, "", "MIDI" },
    { "ribbon_cc", "RIBBON CC NUMBER", 1.0f, 119.0f, 16.0f, 1.0f, "", "MIDI" },
    { "tuning_mode", "TUNING SOURCE", 0.0f, 2.0f, 0.0f, 1.0f, "", "TUNING" },
    { "voice_mode", "VOICE MODE", 0.0f, 4.0f, 0.0f, 1.0f, "", "VOICE" },
    { "unison_detune", "UNISON DETUNE", 0.0f, 50.0f, 10.0f, 1.0f, "ct", "VOICE" },

    // [ws4-circuits] append this workstream's ParamDefs below this line (same order as the enum).

    // [ws5-modern] append this workstream's ParamDefs below this line (same order as the enum).
    { "lfo1_wave", "LFO 1 WAVE", 0.0f, 6.0f, 0.0f, 1.0f, "", "MODULATION" },
    { "lfo2_wave", "LFO 2 WAVE", 0.0f, 6.0f, 0.0f, 1.0f, "", "MODULATION" },
    { "lfo3_wave", "LFO 3 WAVE", 0.0f, 6.0f, 0.0f, 1.0f, "", "MODULATION" },
    { "mseg1_rate", "MSEG 1 RATE", -3.0f, 3.0f, 0.0f, 1.0f, "oct", "MODULATION" },
    { "mseg2_rate", "MSEG 2 RATE", -3.0f, 3.0f, 0.0f, 1.0f, "oct", "MODULATION" },
    { "mseg3_rate", "MSEG 3 RATE", -3.0f, 3.0f, 0.0f, 1.0f, "oct", "MODULATION" },
    { "mod1_amount", "MOD 1 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod2_amount", "MOD 2 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod3_amount", "MOD 3 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod4_amount", "MOD 4 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod5_amount", "MOD 5 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod6_amount", "MOD 6 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod7_amount", "MOD 7 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod8_amount", "MOD 8 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod9_amount", "MOD 9 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod10_amount", "MOD 10 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod11_amount", "MOD 11 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod12_amount", "MOD 12 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod13_amount", "MOD 13 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod14_amount", "MOD 14 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod15_amount", "MOD 15 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod16_amount", "MOD 16 AMOUNT", -1.0f, 1.0f, 0.0f, 1.0f, "", "MOD MATRIX" },
    { "mod_velocity_curve", "VELOCITY CURVE", -1.0f, 1.0f, 0.0f, 1.0f, "", "KEYBOARD MOD" },
    { "mod_pressure_curve", "AFTERTOUCH CURVE", -1.0f, 1.0f, 0.0f, 1.0f, "", "KEYBOARD MOD" },
    { "mod_wheel_curve", "MOD WHEEL CURVE", -1.0f, 1.0f, 0.0f, 1.0f, "", "KEYBOARD MOD" },
    { "mod_key_curve", "KEY FOLLOW CURVE", -1.0f, 1.0f, 0.0f, 1.0f, "", "KEYBOARD MOD" },
    { "fx1_model", "FX 1 MODEL", 0.0f, 5.0f, 0.0f, 1.0f, "", "FX" },
    { "fx1_param_a", "FX 1 PARAM A", 0.0f, 1.0f, 0.5f, 1.0f, "", "FX" },
    { "fx1_param_b", "FX 1 PARAM B", 0.0f, 1.0f, 0.5f, 1.0f, "", "FX" },
    { "fx1_oversampling", "FX 1 OVERSAMPLING", 0.0f, 3.0f, 2.0f, 1.0f, "", "FX" },
    { "fx2_model", "FX 2 MODEL", 0.0f, 5.0f, 0.0f, 1.0f, "", "FX" },
    { "fx2_param_a", "FX 2 PARAM A", 0.0f, 1.0f, 0.5f, 1.0f, "", "FX" },
    { "fx2_param_b", "FX 2 PARAM B", 0.0f, 1.0f, 0.5f, 1.0f, "", "FX" },
    { "fx2_oversampling", "FX 2 OVERSAMPLING", 0.0f, 3.0f, 2.0f, 1.0f, "", "FX" },
    { "fx3_model", "FX 3 MODEL", 0.0f, 5.0f, 0.0f, 1.0f, "", "FX" },
    { "fx3_param_a", "FX 3 PARAM A", 0.0f, 1.0f, 0.5f, 1.0f, "", "FX" },
    { "fx3_param_b", "FX 3 PARAM B", 0.0f, 1.0f, 0.5f, 1.0f, "", "FX" },
    { "fx3_oversampling", "FX 3 OVERSAMPLING", 0.0f, 3.0f, 2.0f, 1.0f, "", "FX" },
    { "fx_gain_comp", "FX LOUDNESS MATCH", 0.0f, 1.0f, 0.0f, 1.0f, "", "FX" },

    // [ws6-library] append this workstream's ParamDefs below this line (same order as the enum).

    // [ws7-panel] append this workstream's ParamDefs below this line (same order as the enum).

    // [ws8-studio] append this workstream's ParamDefs below this line (same order as the enum).

};
static_assert(P_legacyNumParams == 122, "legacy core indexes must not change");
static_assert(std::size(kExtensionDefs) == P_numParams - P_legacyNumParams);

inline const ParamDef& paramDef(int index) noexcept
{
    if (index < P_numGlobals)
        return kGlobalDefs[index];
    if (index >= P_legacyNumParams)
        return kExtensionDefs[index - P_legacyNumParams];
    return kChannelDefs[(index - P_ch1Start) % CP_count];
}

inline std::string paramId(int index)
{
    if (index < P_numGlobals)
        return kGlobalDefs[index].id;
    if (index >= P_legacyNumParams)
        return kExtensionDefs[index - P_legacyNumParams].id;
    const int channel = index < P_ch2Start ? 1 : 2;
    const int local = index < P_ch2Start ? index - P_ch1Start : index - P_ch2Start;
    return "ch" + std::to_string(channel) + "_" + kChannelDefs[local].id;
}

inline std::string channelParamId(int channel, int local)
{
    return "ch" + std::to_string(channel + 1) + "_" + kChannelDefs[local].id;
}

struct ParamValues
{
    std::array<float, P_numParams> v {};

    ParamValues()
    {
        for (int i = 0; i < P_numParams; ++i)
            v[(size_t) i] = paramDef(i).def;
    }

    float& operator[](int index) noexcept { return v[(size_t) index]; }
    float operator[](int index) const noexcept { return v[(size_t) index]; }
};

inline float clampValue(float x, float lo, float hi) noexcept
{
    return x < lo ? lo : (x > hi ? hi : x);
}

inline float normaliseParam(int index, float value) noexcept
{
    const auto& d = paramDef(index);
    const float t = (value - d.min) / (d.max - d.min);
    return clampValue(t, 0.0f, 1.0f);
}

inline float denormaliseParam(int index, float normalised) noexcept
{
    const auto& d = paramDef(index);
    return d.min + clampValue(normalised, 0.0f, 1.0f) * (d.max - d.min);
}

inline ParamValues defaultValues()
{
    return ParamValues {};
}

} // namespace cs80
