#pragma once

// The one compiled copy of the evidence that the editor shows to players: the model register
// (which implementation makes the sound of each subsystem, and on what evidence) and the
// per-control help (owner's-manual wording, manual page and the evidence class of each law).
//
// Sources, and where to update them:
//   * kModelRegister rows marked Origin::status mirror the "Model register" table in
//     docs/STATUS.md. Rows marked Origin::coverage are taken from docs/HARDWARE_COVERAGE.md.
//     When either document changes, change this table in the same commit.
//   * kControlHelp follows the owner-index rows of docs/HARDWARE_COVERAGE.md: owner index,
//     printed owner-manual pages ("O" in that ledger) or service-manual sheets ("S"), and the
//     ledger status codes (IB, A, M, MR, PC).
//
// Nothing here is a measurement. "Source-derived" means the behaviour or law is taken from a
// schematic or manual page; "inferred" means a declared engineering model where the public
// sources leave the quantity open (docs/ANALOG_FIDELITY_REQUIREMENT.md). No row claims
// hardware-measured parity.
//
// Plain data with no JUCE dependency, so the core test suite can check it.

#include <cstring>
#include <string_view>

namespace cs80::evidence
{

// The four evidence classes of docs/ANALOG_FIDELITY_REQUIREMENT.md, reduced to what a law in
// this build actually rests on, plus the modern layer that has no original counterpart.
enum class Basis
{
    sourceDerived,        // the law itself is read from a schematic, manual or adjustment page
    sourceTopologyInferredLaw, // topology/route from the sources; quantitative law declared inference
    engineeringInferred,  // declared engineering model; the sources leave the behaviour open
    modernExtension       // not on the original instrument; inert in PURE CS-80
};

inline constexpr const char* basisName(Basis b) noexcept
{
    switch (b)
    {
        case Basis::sourceDerived: return "Source-derived";
        case Basis::sourceTopologyInferredLaw: return "Source-derived route, inferred law";
        case Basis::engineeringInferred: return "Engineering inference";
        case Basis::modernExtension: return "Modern extension (not on the original)";
    }
    return "";
}

// One-line explanation of a basis, in the player's words.
inline constexpr const char* basisExplanation(Basis b) noexcept
{
    switch (b)
    {
        case Basis::sourceDerived:
            return "Taken from the Yamaha schematics or manuals.";
        case Basis::sourceTopologyInferredLaw:
            return "The circuit path is taken from the Yamaha sources; how strongly it acts is a declared estimate.";
        case Basis::engineeringInferred:
            return "A declared engineering estimate: the public sources do not give this behaviour.";
        case Basis::modernExtension:
            return "A software addition that the original instrument does not have. PURE CS-80 switches it off.";
    }
    return "";
}

// HARDWARE_COVERAGE.md status codes, expanded for display ("IB/A" -> two phrases).
inline constexpr const char* coverageCodeName(std::string_view code) noexcept
{
    if (code == "IB") return "implemented behaviour";
    if (code == "A") return "approximate law";
    if (code == "M") return "missing";
    if (code == "MR") return "measurement required";
    if (code == "PC") return "physical controller required";
    if (code == "EXT") return "software extension";
    return "";
}

enum class Origin { status, coverage };

struct ModelEntry
{
    const char* subsystem;
    const char* legacy;      // what the Legacy (compatibility) engine runs
    const char* circuit;     // what the circuit engine (Source Performance / PURE CS-80) runs
    const char* reference;   // offline truth or experimental model, not in audio ("-" if none)
    Basis basis;             // what the circuit engine's law rests on
    const char* coverage;    // HARDWARE_COVERAGE status codes
    const char* doc;         // evidence document under docs/
    Origin origin;
};

inline constexpr ModelEntry kModelRegister[] = {
    { "Oscillator", "PolyBLEP oscillator", "same PolyBLEP oscillator",
      "ReconstructedOscillator (experimental)", Basis::engineeringInferred, "A",
      "OSCILLATOR_CIRCUIT_EVIDENCE.md", Origin::status },
    { "HP / LP filters", "TPT state-variable filter, input soft clip",
      "IG00156 stage: in-loop OTA saturation, 22.6 Hz feedforward floor",
      "IG00156Linear reference", Basis::sourceTopologyInferredLaw, "A",
      "IG00156_REALTIME_MODEL.md", Origin::status },
    { "Envelopes", "float ADSR and filter envelope", "double-precision single-RC law",
      "-", Basis::engineeringInferred, "A", "ENVELOPE_RC_MODEL.md", Origin::status },
    { "VCA", "linear multiply", "loaded LC network gain", "LevelControl reference",
      Basis::sourceTopologyInferredLaw, "A", "LEVEL_CONTROL_REFERENCE_DESIGN.md", Origin::status },
    { "Tone tabs", "legacy bridge voicings", "source-factory tones: 7 complete, 15 inferred",
      "HardwareControl resistor matrix", Basis::sourceTopologyInferredLaw, "IB/A",
      "PRESET_CIRCUIT_LEDGER.md", Origin::status },
    { "Sub oscillator", "legacy LFO or original six functions", "same",
      "-", Basis::sourceTopologyInferredLaw, "IB/A", "ORIGINAL_SUB_OSCILLATOR.md", Origin::status },
    { "Chorus", "generic modulated delay", "OE2 BBD chorus (tremolo generic)",
      "Mn3001Packet, Oe2Filters references", Basis::sourceTopologyInferredLaw, "A",
      "BBD_CHORUS_MODEL.md", Origin::status },
    { "Wah / EXP", "band-pass stand-in", "PRA wah circuit", "PraWah reference",
      Basis::sourceTopologyInferredLaw, "A/MR/PC", "REALTIME_PRA_WAH_MODEL.md", Origin::status },
    { "Output", "LOW trim and soft clip", "-6 dB reference, inferred driver ceiling, output taps",
      "OutputTaps reference", Basis::sourceTopologyInferredLaw, "A/MR",
      "OUTPUT_REALTIME_PROPOSAL.md", Origin::status },
    { "Idle voice evolution", "frozen while idle", "active-tail continuity only",
      "ContinuousVoiceCircuit (experimental)", Basis::engineeringInferred, "A",
      "VOICE_CIRCUIT_CONTINUITY.md", Origin::status },
    { "Keyboard scan and assignment", "MIDI events, cyclic assignment", "same",
      "-", Basis::engineeringInferred, "IB/M/MR", "KEYBOARD_CODE_PATH_EVIDENCE.md", Origin::coverage },
    { "Initial touch and aftertouch", "velocity and pressure proxies",
      "held TWS b path for brilliance and level, separate pitch transient", "-",
      Basis::sourceTopologyInferredLaw, "A/PC", "INITIAL_TOUCH_CIRCUIT_EVIDENCE.md", Origin::coverage },
    { "Ring modulator", "ideal multiplier, provisional sweep", "same", "-",
      Basis::engineeringInferred, "A", "HARDWARE_COVERAGE.md", Origin::coverage },
    { "Ribbon", "relative first contact", "same", "-",
      Basis::sourceTopologyInferredLaw, "IB/A/PC", "HARDWARE_COVERAGE.md", Origin::coverage },
    { "Modern layer", "LFOs, insert FX, macros, arpeggiator, MPE, MIDI learn",
      "same; all inert in PURE CS-80", "-", Basis::modernExtension, "EXT",
      "IMPROVEMENT_PLAN.md", Origin::coverage },
};

inline constexpr int kModelRegisterSize = (int) (sizeof(kModelRegister) / sizeof(kModelRegister[0]));

// Per-control help. `id` is the core parameter id for globals and extensions, and the
// channel-local id (kChannelDefs) for the two synthesis rows, so one entry serves both ranks.
struct ControlHelp
{
    const char* id;
    int ownerIndex;          // owner's manual index 1-46; 0 when the control is not numbered there
    const char* pages;       // "O18, O32" printed owner-manual pages, "S17" service sheets, "-" none
    const char* coverage;    // HARDWARE_COVERAGE status codes
    Basis basis;
    const char* plain;       // owner's-manual style, plain words
    const char* circuit;     // engineering notes for the "Circuit notes" section
};

inline constexpr ControlHelp kControlHelp[] = {
    // ---- Global controls (owner index 2-19) ---------------------------------------------------
    { "volume", 2, "O6, O38", "A/MR", Basis::sourceTopologyInferredLaw,
      "Sets the overall output level of the instrument.",
      "Final gain stage. The potentiometer taper and output amplifier headroom are not calibrated; the circuit engine "
      "uses a -6 dB output reference with an inferred driver ceiling." },
    { "mix", 4, "O7, O37", "A", Basis::sourceTopologyInferredLaw,
      "Balances channel I against channel II. Fully up is channel I only, fully down is channel II only.",
      "Linear rank gains. The physical pot taper, loading and end points of the passive mixer are not verified." },
    { "feet", 5, "O8, O37", "IB/MR", Basis::sourceDerived,
      "Selects the octave or harmonic footage of a channel: 16', 8', 5 1/3', 4', 2 2/3' or 2'.",
      "Nominal harmonic ratios from the printed owner's manual (p37). The fifth stops are true ratios, about "
      "1.955 cents from equal temperament; switch and trim tolerances are not measured." },
    { "detune", 6, "O8, O37", "A", Basis::engineeringInferred,
      "Detunes channel II against channel I for a thicker, chorused sound.",
      "Positive cents offset. The real detune voltage injection, centre, direction and range need circuit reconciliation." },
    { "brilliance", 7, "O8, O38", "A", Basis::sourceTopologyInferredLaw,
      "Brightens or darkens both filters of both channels together.",
      "Adds a shared offset to the effective filter cutoff. The HP and LP summing paths are loaded differently on "
      "the original; equal scaling is unverified." },
    { "resonance", 8, "O8, O38", "A", Basis::engineeringInferred,
      "Adds resonance to every filter at once, on top of each filter's own RES control.",
      "Global and per-filter amounts are combined, then mapped to Q by a provisional law. Gains and interaction "
      "are uncalibrated." },
    { "kbd_brilliance_low", 9, "O9, O38", "A/MR", Basis::sourceTopologyInferredLaw,
      "Keyboard brilliance, low end: makes the low keys brighter or darker.",
      "Per-key octave-tap weights add to both filters' cutoff. Weights, the 2-octave full scale and the equal HP/LP "
      "split are provisional; the key map is software, not the KAS/KBC code." },
    { "kbd_brilliance_high", 9, "O9, O38", "A/MR", Basis::sourceTopologyInferredLaw,
      "Keyboard brilliance, high end: makes the high keys brighter or darker. Centre is neutral.",
      "Same route as the LOW lever. Lever polarity is not verified against the terminals." },
    { "kbd_level_low", 10, "O9, O38", "A/MR", Basis::sourceTopologyInferredLaw,
      "Keyboard level, low end: makes the low keys louder or softer.",
      "Additive VCA control offset on the KB branch (KBC2 via 270k to the level-control node); 0.4 full scale is provisional." },
    { "kbd_level_high", 10, "O9, O38", "A/MR", Basis::sourceTopologyInferredLaw,
      "Keyboard level, high end: makes the high keys louder or softer.",
      "Same route as the LOW lever; symmetry with LOW is an assumption." },
    { "sub_wave", 11, "O8, O37, O49", "IB/A", Basis::sourceTopologyInferredLaw,
      "Chooses the sub oscillator waveform that modulates pitch, filter or level.",
      "Legacy waveform set. The original six functions are selected with ORIGINAL WAVEFORMS in the Studio view." },
    { "sub_original_function", 11, "O8, O37, O49", "IB/A", Basis::sourceTopologyInferredLaw,
      "Chooses the sub oscillator function: sine, falling saw, rising saw, square, noise or the external input.",
      "SUB29/30 and PN2 switch routing. Ideal waveforms, not a nonlinear IG00150 model; EXT voltage is not calibrated." },
    { "sub_speed", 11, "O49", "IB/A", Basis::engineeringInferred,
      "Sets how fast the sub oscillator runs.",
      "Legacy rate range 0.05-30 Hz, kept separate from the original range." },
    { "sub_original_speed", 11, "O49", "IB/A", Basis::sourceTopologyInferredLaw,
      "Sets how fast the sub oscillator runs, from 0.7 to 60 Hz.",
      "End points from the owner's manual (p49). The intermediate taper and the 10 ms rate smoothing are provisional." },
    { "sub_vco", 11, "O8, O37", "A", Basis::sourceTopologyInferredLaw,
      "How far the sub oscillator moves the pitch (vibrato).",
      "Semitone modulation. The real pitch-CV injection may not be exponential semitone FM." },
    { "sub_vcf", 11, "O8, O37", "A", Basis::sourceTopologyInferredLaw,
      "How far the sub oscillator moves both filters (wah-like movement).",
      "Equal shared offset; the separate HP/LP path gains and loading are unverified." },
    { "sub_vca", 11, "O8, O37", "A", Basis::sourceTopologyInferredLaw,
      "How far the sub oscillator moves the level (tremolo).",
      "Linear level addition with a clamp; the actual VCA control law is unverified." },
    { "init_bend", 12, "S17, S23", "A", Basis::sourceTopologyInferredLaw,
      "Initial pitch bend: each note starts slightly sharp and settles, more so when you strike harder.",
      "Separately AC-coupled TWS C path: conditional 1 uF x (100k || 100k) pole. FET acquisition, clamp and "
      "retrigger behaviour are approximate." },
    { "sub_at_speed", 12, "O10, O38", "IB/A", Basis::sourceTopologyInferredLaw,
      "Aftertouch speeds up the sub oscillator while you press harder.",
      "TRG5 diode-OR touch bus into the IG00150 exponential converter; the bounded multiplier is provisional." },
    { "sub_at_vco", 12, "O10, O38", "A", Basis::sourceTopologyInferredLaw,
      "Aftertouch deepens the sub oscillator's pitch modulation.",
      "Per-voice pressure multiplier; the destinations are right, the gain law is not proven." },
    { "sub_at_vcf", 12, "O10, O38", "IB/A", Basis::sourceTopologyInferredLaw,
      "Aftertouch deepens the sub oscillator's filter modulation.",
      "Independent green control; numerical depth is provisional (AFTERTOUCH_SUB_VCF.md)." },
    { "master_sustain", 13, "O11, O39", "IB/A", Basis::sourceTopologyInferredLaw,
      "Lengthens the release of every note after the key is let go.",
      "Extends both filter and amplitude release; the footswitch removes only this sustain term." },
    { "sustain_mode", 13, "O11, O39", "IB/A", Basis::sourceTopologyInferredLaw,
      "Sustain I or II. In II a new key press cuts the tails of earlier notes.",
      "Held-tail mode handling; edge cases and release-cancellation timing are unmeasured." },
    { "footswitch_plugged", 13, "O11, O39", "IB/A/PC", Basis::sourceDerived,
      "Tells the instrument a footswitch is plugged into the jack.",
      "Switched jack: with no plug the jack acts as a pressed switch (FOOT_CONTROL_EVIDENCE.md)." },
    { "footswitch_pressed", 13, "O11, O39", "IB/A/PC", Basis::sourceDerived,
      "The footswitch contact. Latches; press again to release.",
      "Truth table: not assigned, or not plugged, or pressed. A latching virtual switch stands in for the pedal." },
    { "sustain_assign", 13, "O11, O39", "IB/A", Basis::sourceDerived,
      "Assigns the footswitch to sustain.",
      "Plugged and open removes only the SUSTAIN release term and the Sustain II tail privilege." },
    { "portamento", 14, "O12, O39", "A", Basis::engineeringInferred,
      "Sets the glide time between notes.",
      "Exponential glide with pitch rounding for glissando; the original stepped control and smoothing are not reconstructed." },
    { "glide_mode", 14, "O12, O39", "A", Basis::engineeringInferred,
      "Chooses portamento (smooth glide) or glissando (semitone steps).",
      "Three-state digital choice; the gliss/porta relationship needs hardware confirmation." },
    { "glide_assign", 14, "O12, O39", "IB/A", Basis::sourceDerived,
      "Assigns the footswitch to portamento or glissando.",
      "A closed gate suppresses new glides, snaps running ones and disables glissando rounding." },
    { "chorus_on", 15, "O13, O39", "IB/A", Basis::sourceTopologyInferredLaw,
      "Switches the chorus or tremolo section on or off.",
      "Bypass switch; switching transient and level change are uncalibrated." },
    { "chorus_mode", 15, "O13, O39", "A", Basis::sourceTopologyInferredLaw,
      "Chooses chorus (a moving ensemble delay) or tremolo (a moving level).",
      "Circuit engine: OE2 BBD delay with 256 clock periods and printed filters; clock range, wet level and "
      "polarity are inferred. Tremolo is a generic amplitude modulator." },
    { "chorus_speed", 15, "O13, O39", "A", Basis::engineeringInferred,
      "Sets how fast the chorus or tremolo moves.",
      "Free range and gain; mode-related rates and tapers are uncalibrated." },
    { "chorus_depth", 15, "O13, O39", "A", Basis::engineeringInferred,
      "Sets how deep the chorus or tremolo moves.",
      "Free range and gain; stereo phase and amplitude against depth are unmeasured." },
    { "ring_speed", 16, "O13-14, O37, O45", "A", Basis::engineeringInferred,
      "Sets the ring modulator's carrier frequency.",
      "Sine carrier; the range keeps a 500 Hz software extension. Service calibration anchors exist (ADJ)." },
    { "ring_depth", 16, "O45", "A", Basis::engineeringInferred,
      "How far the ring modulator's envelope sweeps the carrier frequency.",
      "Provisional base-frequency-dependent excursion; no calibrated law." },
    { "ring_attack", 16, "O13-14, O45", "A", Basis::engineeringInferred,
      "Attack time of the ring modulator's sweep envelope.",
      "Envelope with gate handling; an ADJ attack target exists, the full taper is unknown." },
    { "ring_decay", 16, "O13-14, O45", "A", Basis::engineeringInferred,
      "Decay time of the ring modulator's sweep envelope.",
      "Envelope with gate handling; taper unknown." },
    { "ring_amount", 16, "O13-14, O45", "A", Basis::engineeringInferred,
      "How much ring-modulated sound is mixed in (MOD).",
      "Ideal multiplication and dry interpolation; carrier bleed, distortion and mix law are unmeasured." },
    { "foot_mode", 17, "O14, O39", "A/MR/PC", Basis::sourceTopologyInferredLaw,
      "Foot controller selector: EXP controls volume only, EXP-WAH also sweeps a wah filter.",
      "PRA53 filter and bypass network; the circuit engine runs the PRA wah circuit. Optical pedal resistance and "
      "sweep are unmeasured." },
    { "master_tune", 18, "O14, O37", "A", Basis::engineeringInferred,
      "Fine tunes the whole instrument.",
      "Digital offset on the A4 reference; not the original knob circuitry." },
    { "ribbon", 19, "O14, O44-45", "IB/A/PC", Basis::sourceTopologyInferredLaw,
      "Touch the ribbon anywhere, then slide left or right to bend the pitch; lift off to return.",
      "First contact sets zero (relative), as on the original. Touch mechanics, voltage curve and contact noise are unverified." },
    { "output_level", 0, "-", "A/MR", Basis::engineeringInferred,
      "Rear-panel output level switch: HIGH or LOW.",
      "HIGH is unity; LOW is a provisional -20 dB trim. The evidence set documents no level difference." },
    { "sub_ext_level", 0, "-", "IB/A", Basis::sourceTopologyInferredLaw,
      "Level of the external signal that the sub oscillator's EXT function uses.",
      "Optional mono input, disabled by default. Software normalisation, not physical voltage calibration "
      "(EXTERNAL_MODULATION.md); 10k schematic vs 50k manual conflict preserved." },
    { "sub_original_mode", 0, "-", "EXT", Basis::modernExtension,
      "Switches the sub oscillator between the original six functions and the legacy set.",
      "Stored with the patch. PURE CS-80 forces the original functions." },

    // ---- Both programmable synthesis rows (owner index 20-45) ---------------------------------
    { "pwm_speed", 20, "O15, O32", "A", Basis::engineeringInferred,
      "Sets how fast the pulse width moves.",
      "Per-rank PWM phase; frequency law, waveform and analog independence are uncalibrated." },
    { "pwm_depth", 21, "O15, O32", "A", Basis::engineeringInferred,
      "Sets how far the pulse width moves.",
      "Fixed 0.35 modulation with clamps; gain and clipping are not circuit-derived." },
    { "pw", 22, "O14, O32", "IB/A", Basis::sourceDerived,
      "Sets the width of the pulse wave from 50 to 90 percent.",
      "50-90 % setting into the pulse generator; duty convention and pulse polarity need verification." },
    { "pulse", 23, "O14, O32", "IB/A", Basis::sourceDerived,
      "Switches the square (pulse) wave on or off.",
      "Waveform switch; switch loading and bleed are absent." },
    { "saw", 24, "O14, O32", "IB/A", Basis::sourceDerived,
      "Switches the sawtooth wave on or off.",
      "Band-limited falling saw, not a full IG00153 ramp and reset model." },
    { "noise", 25, "O15, O32", "A", Basis::engineeringInferred,
      "Mixes white noise into the channel.",
      "Pseudorandom broadband source; spectrum, source fan-out and level taper are unknown." },
    { "hpf_cutoff", 26, "O18, O32", "IB/A", Basis::sourceTopologyInferredLaw,
      "Sets the high-pass filter cutoff. Raise it to thin the sound by removing lows.",
      "IG00156 high-pass stage on the nominal key-voltage law. Circuit engine: in-loop OTA saturation (inferred "
      "level) and the source-derived 4.7 Mohm feedforward floor (22.6 Hz)." },
    { "hpf_res", 27, "O18, O32", "A", Basis::engineeringInferred,
      "Adds resonance at the high-pass cutoff.",
      "Provisional Q mapping; global/local summing and internal feedback are unknown." },
    { "lpf_cutoff", 28, "O18, O32", "IB/A", Basis::sourceTopologyInferredLaw,
      "Sets the low-pass filter cutoff. Lower it to darken the sound.",
      "IG00156 low-pass stage, separate instance of the same nominal law; LP-specific source impedances are not validated." },
    { "lpf_res", 29, "O18, O32", "A", Basis::engineeringInferred,
      "Adds resonance at the low-pass cutoff.",
      "Provisional Q mapping, independent of the HP resonance." },
    { "f_il", 30, "O19, O33-34", "IB/A", Basis::sourceTopologyInferredLaw,
      "Filter envelope initial level: where the cutoff starts when a key is struck.",
      "Negative initial offset of the IG00152 envelope; voltage excursion and loading provisional." },
    { "f_al", 31, "O19, O33-34", "IB/A", Basis::sourceTopologyInferredLaw,
      "Filter envelope attack level: how far the cutoff rises at the end of the attack.",
      "Positive attack peak; same voltage and loading boundary as IL." },
    { "f_a", 32, "O19, O33-34", "A", Basis::engineeringInferred,
      "Filter envelope attack time.",
      "Circuit engine: inferred single-RC law that peaks at exactly the set time (ENVELOPE_RC_MODEL.md)." },
    { "f_d", 33, "O19, O33-34", "A", Basis::engineeringInferred,
      "Filter envelope decay time.",
      "Circuit engine: single-RC decay to -60 dB at the set time; not a recovered IG00152 law." },
    { "f_r", 34, "O19, O33-34", "IB/A", Basis::engineeringInferred,
      "Filter envelope release time, after the key is let go.",
      "Release toward IL with global sustain priority; the contour law is unresolved." },
    { "vcf_level", 35, "O22, O35", "A", Basis::sourceTopologyInferredLaw,
      "Level of the filtered sound in the channel's mix.",
      "Scales the filtered signal before the sine mix; gain, polarity and headroom are uncalibrated." },
    { "sine", 36, "O22, O35", "A", Basis::engineeringInferred,
      "Level of the pure sine wave, mixed after the filters.",
      "Bypass mix; the fixed third harmonic is a placeholder, not a measured IG00158 characterisation." },
    { "v_a", 37, "O21, O34", "A", Basis::engineeringInferred,
      "Amplitude envelope attack time.",
      "Circuit engine: inferred single-RC attack; legacy: ADSR heuristic." },
    { "v_d", 38, "O21, O34", "A", Basis::engineeringInferred,
      "Amplitude envelope decay time.",
      "Circuit engine: single-RC decay with no x5 time-window bend; not a recovered IG00159 law." },
    { "v_s", 39, "O21, O34", "A", Basis::engineeringInferred,
      "Amplitude envelope sustain level, held while the key is down.",
      "Sustain fraction; the control-voltage to gain mapping is unverified." },
    { "v_r", 40, "O21, O34", "IB/A", Basis::engineeringInferred,
      "Amplitude envelope release time.",
      "Release and sustain priority implemented; exact timing and shape unmeasured." },
    { "v_level", 41, "O22, O35", "A", Basis::sourceTopologyInferredLaw,
      "Overall level of the channel.",
      "Circuit engine: loaded LC network gain; the IG00151 linear and exponential inputs are not separately reconstructed." },
    { "init_brill", 42, "S17, S19", "A", Basis::sourceTopologyInferredLaw,
      "Initial touch brilliance: strike harder for a brighter note.",
      "Held TWS b signal feeding the IBI/IBII-controlled banks; the sample/hold transfer is not derived." },
    { "init_level", 43, "S17, S19, S23", "A", Basis::sourceTopologyInferredLaw,
      "Initial touch level: strike harder for a louder note.",
      "Held TWS b signal feeding the ILI/ILII-controlled banks; zero-touch invariance holds, the level transfer is uncalibrated." },
    { "after_brill", 44, "O23, O36, O44", "A", Basis::sourceTopologyInferredLaw,
      "Aftertouch brilliance: press a held key harder to brighten it.",
      "Per-voice pressure offset; sensor law, HP/LP ratio and aggregation unverified." },
    { "after_level", 45, "O23, O36, O44", "A", Basis::sourceTopologyInferredLaw,
      "Aftertouch level: press a held key harder to make it louder.",
      "Per-voice pressure level addition; sensor and VCA law uncalibrated." },
    { "hpf_on", 0, "-", "EXT", Basis::modernExtension,
      "Bypasses the channel's high-pass filter when off.",
      "Software extension; not a switch on the original." },
    { "lpf_on", 0, "-", "EXT", Basis::modernExtension,
      "Bypasses the channel's low-pass filter when off.",
      "Software extension; not a switch on the original." },
    { "pan", 0, "-", "EXT", Basis::modernExtension,
      "Places the channel in the stereo field.",
      "Software extension; the original's left/right routing is not reconstructed." },

    // ---- Software layer ------------------------------------------------------------------------
    { "key_release", 0, "-", "EXT", Basis::modernExtension,
      "A minimum release for every note: the longer of this and the channel's own release wins.",
      "Software floor, scaled by the MOTION macro." },
    { "output_mode", 0, "-", "EXT", Basis::modernExtension,
      "Stereo keeps left and right; mono averages them after the effects.",
      "Digital fold-down; the original GENERAL mono summing is not reconstructed." },
    { "voice_dispersion", 0, "-", "A", Basis::engineeringInferred,
      "Spreads the eight voice cards in tuning and pan, as no two analog voices match.",
      "Random trims; not a calibrated tolerance distribution." },
    { "drift", 0, "-", "A", Basis::engineeringInferred,
      "Slowly moves each voice card's tuning, like warming analog circuits.",
      "Slow drift model; thermal time constants are not measured." },
    { "analog_noise", 0, "-", "A", Basis::engineeringInferred,
      "Adds a quiet analog background noise floor.",
      "Provisional noise floor; not a measured spectrum." },
    { "mpe_on", 0, "-", "EXT", Basis::modernExtension,
      "Lets an MPE controller bend and shape each note separately.",
      "Channel 1 master, 2-16 members; bend ranges matched by hand." },
    { "pitch_bend_range", 0, "-", "EXT", Basis::modernExtension,
      "Range of the pitch wheel and MIDI pitch bend.",
      "MIDI convenience; the original has a ribbon, not a pitch wheel." },
    { "mpe_bend_range", 0, "-", "EXT", Basis::modernExtension,
      "Per-note pitch bend range in MPE mode.",
      "MIDI convenience." },
    { "tuning_ref", 0, "-", "EXT", Basis::modernExtension,
      "Reference frequency of A4 for equal temperament.",
      "Digital reference; not the original tuning circuitry." },
    { "modwheel_sub", 0, "-", "EXT", Basis::modernExtension,
      "How much the mod wheel adds sub oscillator vibrato.",
      "MIDI convenience." },
    { "modwheel_vcf", 0, "-", "EXT", Basis::modernExtension,
      "How much the mod wheel moves the filters.",
      "MIDI convenience." },
    { "parity_mode", 0, "-", "EXT", Basis::modernExtension,
      "PURE CS-80: strips every software addition and runs the circuit models only.",
      "Comparison isolation. Not calibrated; no hardware match established." },
    { "source_performance", 0, "-", "EXT", Basis::modernExtension,
      "ENGINE: CIRCUIT runs the circuit-informed models; LEGACY keeps the older voicing for old sessions.",
      "Source performance policy. Not calibrated; no hardware match established." },
};

inline constexpr int kControlHelpSize = (int) (sizeof(kControlHelp) / sizeof(kControlHelp[0]));

inline const ControlHelp* findControlHelp(const char* id) noexcept
{
    if (id == nullptr)
        return nullptr;
    for (const auto& entry : kControlHelp)
        if (std::strcmp(entry.id, id) == 0)
            return &entry;
    return nullptr;
}

// Families of modern parameters that share one entry (lfo1_rate, fx2_mix, macro3, arp_mode...).
inline constexpr ControlHelp kModernFamilyHelp[] = {
    { "lfo", 0, "-", "EXT", Basis::modernExtension,
      "A free-running global LFO. Route it to one destination with its chips.",
      "Software modulation source; masked in PURE CS-80." },
    { "fx", 0, "-", "EXT", Basis::modernExtension,
      "One of three insert effects after the voices and chorus.",
      "Software effects; masked in PURE CS-80." },
    { "arp", 0, "-", "EXT", Basis::modernExtension,
      "The arpeggiator: holds a chord and plays it as a pattern.",
      "Software arpeggiator; masked in PURE CS-80." },
    { "macro", 0, "-", "EXT", Basis::modernExtension,
      "A performance macro: a patch-level offset that is neutral at centre.",
      "Software macro; masked in PURE CS-80." },
    // Phase 6 (ws5-modern): mseg1_rate..mseg3_rate.
    { "mseg", 0, "-", "EXT", Basis::modernExtension,
      "A drawable multi-segment envelope; this sets how fast it runs or its tempo division.",
      "Software modulation source routed through the matrix; masked in PURE CS-80." },
    // Phase 6 (ws5-modern): mod1_amount..mod16_amount and the keyboard curves
    // (mod_velocity_curve, mod_pressure_curve, mod_wheel_curve, mod_key_curve).
    { "mod", 0, "-", "EXT", Basis::modernExtension,
      "The modulation matrix: how strongly a slot's source moves its destination, or the "
      "response curve a keyboard source passes through before it reaches the matrix.",
      "Software routing; matrix curves shape matrix sources only, never the original "
      "touch circuits. Masked in PURE CS-80." },
};

// Help for a core parameter id, falling back to its modern family ("lfo2_depth" -> lfo).
inline const ControlHelp* findHelpForId(const char* id) noexcept
{
    if (auto* exact = findControlHelp(id))
        return exact;
    if (id == nullptr)
        return nullptr;
    for (const auto& family : kModernFamilyHelp)
        if (std::strncmp(id, family.id, std::strlen(family.id)) == 0)
            return &family;
    return nullptr;
}

} // namespace cs80::evidence
