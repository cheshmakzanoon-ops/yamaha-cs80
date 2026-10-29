# Original CS-80 preset circuit ledger

**Status:** verified schematic transcription and conditional calculations, not measured factory patches. No physical instrument was measured. These values have **not** been implemented as circuit-derived software presets.

## Primary sources

Original Yamaha documents, hosted by independent archives:

- [CS-80 service manual index](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/index.html).
- [Complete foldout schematic](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/cs80_foldout_schematic.jpg): includes tone-name/column correspondence and TS → BA → preset matrices.
- [p31 T51](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/31%20-%20T51%20Circuit%20Diagram.jpg), [p33 T52](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/33%20-%20T52%20Circuit%20Diagram.jpg), [p35 T53](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/35%20-%20T53%20Circuit%20Diagram.jpg), [p37 T54](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/37%20-%20T54%20Circuit%20Diagram.jpg).
- [p27 BA](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/27%20-%20BA%20Circuit%20Diagram.jpg).
- [p62 Panel 1](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/62%20-%20Panel%201%20Circuit%20Diagram.jpg): control-name/output-number cross-check.
- [Yamaha CS Series Servicing Guide](https://www.goltsynth.com/resources/downloads/CS/cs_series_servicing_guide.pdf), printed p63: resistor/diode voltage-divider principle. Its example does not justify substituting CS-50 selector behavior or resistor values for CS-80 circuitry.

## Column identities

| Board | A | B | C | D | E | F |
|---|---|---|---|---|---|---|
| T51 | String 1 | String 3 | Brass 1 | Flute | Electric Piano | Clavichord 1 |
| T52 | Harpsichord 1 | Organ 1 | Guitar 1 | Funky 1 | Funky 3 | unused |
| T53 | String 2 | String 4 | Brass 2 | Brass 3 | Bass | Clavichord 2 |
| T54 | Harpsichord 2 | Organ 2 | Guitar 2 | Funky 2 | Funky 4 | unused |

The foldout explicitly connects these columns to the corresponding tone-selector positions.

## Network topology and reference

A selected analog branch consists of a series resistor and **1S1555 isolation diode**, feeding a common output bus.

T51/T53 contain **10 kΩ shunts to E** on the analog outputs tabulated below. Companion T52/T54 outputs share those buses; do not add another parallel 10 kΩ. Waveform-switch rows require separate treatment. Primed terminals are diode-isolated alternate inputs whose Panel/Memory routing must be traced separately.

BA has a labeled **+10.6 V supply rail**, distinct from the TS selector logic’s +15 V supply. The selected matrix-column voltage also depends on the BA transistor drop and loading. It is not guaranteed to equal 10.6 V.

The matrix schematic does not assign individual R-number designators. Use **board/column/output** as the reproducible locator; for example, **T51/A/7** identifies String 1’s HPF branch resistor. Do not invent resistor reference numbers.

## Original 66-entry sample transcription

Values are in **kΩ**, except where explicitly marked Ω.

- **direct:** diode branch without a drawn series resistor.
- **open:** no branch drawn from this column to this output; the output shunt remains.
- Control names are cross-checked against Panel 1’s T51/T53 wiring annotations.

| Output | Control | T51 A String 1 | T51 C Brass 1 | T51 D Flute | T53 A String 2 | T53 C Brass 2 | T53 D Brass 3 |
|---|---|---:|---:|---:|---:|---:|---:|
| 7 | HPF | 6.8 | open | 51 | 6.2 | open | 27 |
| 8 | RESH | 5.6 | direct | 15 | direct | direct | 18 |
| 9 | LPF | 1 | 5.1 | 7.5 | **750 Ω** | 4.3 | 13 |
| 10 | RESL | direct | 6.8 | 30 | direct | 3.6 | 15 |
| 11 | IL | 16 | 43 | 9.1 | open | 9.1 | 27 |
| 12 | AL | 3.6 | 13 | 130 | open | 20 | 1.6 |
| 13 | VCF attack | 30 | 62 | 18 | direct | 120 | 15 |
| 14 | VCF decay | 12 | 51 | 51 | direct | 13 | 12 |
| 15 | VCF release | 1.2 | direct | 3.6 | direct | direct | 3.9 |
| 18 | VCA attack | 62 | 8.2 | 22 | 2.2 | 9.1 | 8.2 |
| 19 | VCA decay | 82 | 39 | open | 47 | 36 | 13 |

These values establish genuine rank-I/rank-II voicing differences. They do **not** by themselves establish normalized software-slider positions or envelope durations.

Later service revisions may alter marked components. Preserve source revision when extending the transcription.

**Native-source correction, 2026-10-29:** T51/C/12 (Brass 1 AL) is printed
**13K**, not the previously transcribed 15K. Two independent visual readers
traced column C to output12 in native tiles `t51-native-r0-c1` and
`t51-native-r0-c2`, [inspection job123175135239](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40790769942/job/123175135239).
Original source SHA256:
`009384403e4c4b8e855daf6153219b3a94e6904d1f605393f3eda27968b9e014`.
Only this source-data entry and its conditional regression fixture are corrected;
no production audio or provisional preset parameter changes.
See [native review](PRESET_NATIVE_MATRIX_REVIEW.md) for crop hashes and remaining
unverified/revision-marked entries.

### Independently reviewed source-domain extension (current)

The source table now represents all22 factory tone columns ×26 outputs:
**547 independently reviewed classifications and25 explicit unknown cells**.
The original150 classified cells are joined by199 rank-I and198 rank-II
classifications. No marked revision or uncertain glyph has been promoted.
The complete25-cell exclusion list, board counts, review provenance and remaining
receiver gates are in [the full matrix audit](PRESET_FULL_MATRIX_AUDIT.md).

Canonical companion-board columns are T52/A–E and T54/A–E, not G–K.
No factory sound is authenticated merely by completing its branch inventory.

Local shunt metadata is now board-aware: T51/T53 analog rows each draw one10k
shunt; their waveform rows4/5 do not. T52/T54 draw **no local shunts on any row**.
This says nothing by itself about total connected-bus/receiver loading.
T52/T54 primed-terminal routing is crossed/revision-marked and remains unresolved.

Use `solveMatrixBus` for a source-table address. Unknown cells remain unknown.
Any row with no drawn local shunt requires an explicit positive finite ground
load, otherwise the result is `unresolvedLoading` with no voltage. Only that
caller load is used on T52/T54 or waveform rows; no companion10k is invented.
For a local10k analog row, a caller load is placed in parallel with that one
local shunt. The generic `solveBus` remains only an explicitly assumed10k
illustrative divider, not a complete hardware receiver model.

Tests preserve printed750Ω,510Ω and390Ω source units, classify all572
addresses, check all25 unknowns, and distinguish local from assumed connected
loading. These are software/data consistency tests, not hardware measurements.
No production audio or factory parameter values change.

### Output-number map

Panel 1 establishes:

| Output | Control |
|---|---|
| 1 | Speed |
| 2 | PWM |
| 3 | PW |
| 4 | Square switch |
| 5 | Saw switch |
| 6 | Noise |
| 7 | HPF |
| 8 | RESH |
| 9 | LPF |
| 10 | RESL |
| 11 | IL |
| 12 | AL |
| 13–15 | VCF attack, decay, release |
| 16 | VCF level |
| 17 | Sine |
| 18–21 | VCA attack, decay, sustain, release |
| 22 | VCA level |
| 23–26 | Initial brilliance, initial level, after brilliance, after level |

## Conditional voltage calculations—not measured CVs

For one selected branch, with negligible receiver loading, ideal selected-column voltage Vs, constant diode drop Vd, series resistance Rs, and shunt Rp = 10,000 Ω:

`Vbus = (Vs − Vd) × Rp / (Rs + Rp)`

**Illustrative assumptions only:**

- Vs = 10.6 V, ignoring BA transistor drop/source impedance.
- Vd = 0.6 V, ignoring current and temperature dependence.
- No additional receiver loading.

Then:

`Vbus = 10 V × 10 kΩ / (Rs + 10 kΩ)`

| Tone | HPF bus 7, V | LPF bus 9, V |
|---|---:|---:|
| String 1 | 5.952 | 9.091 |
| Brass 1 | 0, open | 6.623 |
| Flute | 1.639 | 5.714 |
| String 2 | 6.173 | 9.302 |
| Brass 2 | 0, open | 6.993 |
| Brass 3 | 2.703 | 4.348 |

Displayed precision describes arithmetic, not hardware accuracy.

A direct branch approaches Vs−Vd. An open branch approaches ground only if no alternate source is active and leakage is negligible.

For an additional linear load to ground, replace Rp with Rp||Rload. A receiver tied to another reference requires full node analysis. Rs||Rp is a first-order source resistance only after idealizing the diode and BA stage.

## Why these numbers cannot feed directly into the current DSP

The current DSP parameters describe software quantities such as normalized slider position, cutoff, resonance, or envelope time. The resistor networks instead establish **electrical control-bus voltages**.

Conversion still requires:

1. Selected BA output voltage and impedance.
2. Diode operating point.
3. Receiving amplifier input impedance, reference voltage, gain and polarity.
4. Routing and interaction with Panel/Memory selection.
5. Conversion from the receiving voltage to the actual M-board IC control input.
6. The applicable IC transfer function.

In particular:

- “Open” and “direct” cannot automatically mean normalized zero and one.
- Envelope branch resistances are not envelope time constants.
- Matrix HPF/LPF bus voltages are not the Vfc appearing in the documented IC cutoff equation until the intervening circuitry is accounted for.
- Resonance bus voltage cannot be equated with a linear Q parameter.

**Implementation gate:** preserve branch resistances as source data; solve the loaded control network in volts; apply documented IC laws; compare with service calibration; only then derive replacement factory patches.

## Related dependency: keyboard-control brilliance and level

Sources:

- [Owner manual part 1](https://www.therogoffs.com/cs80/CS80E_1.pdf), printed p9.
- [Owner manual part 2](https://www.therogoffs.com/cs80/CS80E_2.pdf), printed p38.
- [Service p15 KBC1/2](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/15%20-%20KBC%201-2%20Circuit%20Diagram.jpg).
- Full foldout KEY BOARD CONTROL and KBC1/KBC2 blocks.

### Verified user-facing behavior

Four independent LOW/HIGH BRILLIANCE/LEVEL sliders are normal/neutral at their centers.

Moving down increases brightness or level; moving up decreases it. The effect varies gradually and becomes strongest toward the corresponding keyboard end, rather than creating a hard split. Brilliance concerns both filter cutoffs; level controls the corresponding level contour.

The owner documentation does not establish an exact MIDI pivot, maximum cutoff displacement in octaves, or maximum level change in dB.

### Verified circuit quantities

| Slider | Function | Value |
|---|---|---|
| VR19 | LOW brilliance | B10k |
| VR20 | HIGH brilliance | B10k |
| VR21 | LOW level | B10k |
| VR22 | HIGH level | B10k |

Terminal 1 is −15 V and terminal 3 is +15 V; grounded center taps are drawn.

At KBC IC14:

- Both **LI** and **HI** summers have 56 kΩ control input and 15 kΩ feedback, plus a separate 56 kΩ input from −15 V.
- Their ideal reference is therefore 225/56 − (15/56)×Vw volts, where Vw is the loaded control wiper voltage. Centered controls give approximately +4.017857 V; ideal endpoint references span 0 to 8.035714 V.
- The earlier LOW gain −1 versus HIGH gain −15/56 reading was incorrect and is withdrawn. The independently inspected native KBC15 and Panel2p64 drawings establish the same input gain for both controls.

These are **nominal internal references, not final VCF cutoff or VCA gain bounds**. Potentiometer loading, the switched DAC and receiver gains must be included. See [the keyboard-contour evidence](KEYBOARD_CONTOUR_EVIDENCE.md).

The subsequent network contains switched resistors, YM26700 key-address D/A conversion, VR1 B5k adjustment, and per-voice detection amplifiers. Equal 1 kΩ note resistors coexist with nonuniform octave resistors, and the switched HIGH/LOW weighting must be derived separately from the identical IC14 input gains.

A symmetric linear curve around an arbitrary MIDI pivot therefore cannot be described as an exact circuit reconstruction. A center-neutral, monotonic, end-weighted interpolation may be used only as an explicitly approximate fallback until that network is solved. Keyboard-control position must not be replaced by VCO footage or pitch-bend CV.

## Remaining source-analysis boundary

The verified transcription is complete for the sample above. Receiver conversion remains unfinished: the downstream BA/control-amplifier/M-board nodes must be visually traced before assigning numerical software parameters.

Local command execution remains blocked by filesystem ENOSPC, but bounded GitHub Actions source inspection now provides native diagram pixels and hashes. The active receiver and contour audits record newly verified connections; this ledger still does not claim a complete receiver conversion or authentic circuit-derived factory bank.
