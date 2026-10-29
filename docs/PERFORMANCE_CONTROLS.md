# Performance controls: keyboard contour, footswitch assigners, EXP/WAH, output switch, key pressure

Implemented 2026-10-29 on top of the 233-parameter baseline. These close the functional omissions that [HARDWARE_COVERAGE.md](HARDWARE_COVERAGE.md) listed as MISSING. Each route now exists and is wired to the right destination; **none of the numerical gains is a measured or circuit-derived law.** Every constant is a named `kProvisional…` value in `src/core/PerformanceControls.h` so it cannot be mistaken for evidence.

The default of every new control reproduces the previous behavior exactly. A fixed performance rendered through 96 factory-preset/gesture combinations (glide, both sustain modes, pressure, expression) produced a bit-identical float stream before and after this change.

## Parameters (append-only)

| Core index | Host index | ID | Type | Default | AU hint |
|---|---|---|---|---|---|
| 127 | 233 | `kbd_brilliance_low` | float -1..1 | 0 | 5 |
| 128 | 234 | `kbd_brilliance_high` | float -1..1 | 0 | 5 |
| 129 | 235 | `kbd_level_low` | float -1..1 | 0 | 5 |
| 130 | 236 | `kbd_level_high` | float -1..1 | 0 | 5 |
| 131 | 237 | `footswitch_plugged` | bool | off | 5 |
| 132 | 238 | `footswitch_pressed` | bool | off | 5 |
| 133 | 239 | `sustain_assign` | bool | off | 5 |
| 134 | 240 | `glide_assign` | bool | off | 5 |
| 135 | 241 | `foot_mode` | choice EXP / EXP-WAH | EXP | 5 |
| 136 | 242 | `output_level` | bool, on = HIGH | HIGH | 5 |

All earlier IDs, order, ranges, defaults and AU hints are untouched (the smoke test pins literal snapshots of the first 228 IDs, the AT VCF hint 3 and the original-SUB hint 4 block). A session saved before these controls restores neutral values, never the live ones.

**Preset recall.** Loading a factory preset recalls the sound, not the wiring: `footswitch_plugged`, `footswitch_pressed`, `foot_mode` and `output_level` are left where they are. The four contour levers and both assigners are part of the patch and return to their neutral values with it. User presets and host state restore all ten.

## Keyboard contour (owner controls 9 and 10)

Source chain: [KEYBOARD_CONTOUR_EVIDENCE.md](KEYBOARD_CONTOUR_EVIDENCE.md). Established there: four center-neutral bipolar levers; brilliance moves both filters, level feeds the VCA control sum through its own branch; the effect grows toward the keyboard ends.

**Implemented topology.** Each voice stores two key-position weights when its key goes down. `brilliance = (lowLever*wLow + highLever*wHigh) * 2.0` octaves is added to the effective cutoff control of both the HP and LP filters of both ranks; `level = (lowLever*wLow + highLever*wHigh) * 0.4` is added to the VCA control sum before its clamp. At centre both terms are exactly `+0.0f`.

**Weights.** The KBC octave-tap ladder gives taps OC0..OC5 of L, L, (2L+R)/3, R, (R+2H)/3, H. As a fraction of each lever, LOW is `{1, 1, 2/3, 0, 0, 0}` and HIGH `{0, 0, 0, 0, 2/3, 1}`, one key plus five octaves across 61 keys. The weights interpolate linearly between octave-group centres.

**Not established, and deliberately labelled as such:**

- The real ladder is stepped per octave group; the smooth interpolation is a software adaptation. Whether the instrument steps audibly at octave boundaries is unmeasured.
- The key map is this instrument's own 61-key span (MIDI 24..84). The original KAS/KBC B-code to physical-key mapping is still the open item in the evidence ledger, so `src/reference/KeyboardContour.h` (the settled-DC network experiment) is intentionally **not** driving audio.
- The 2.0-octave and 0.4 full-scale values, the equal HP/LP split (the 120k/39k branch resistors are not dimensionless gains) and lever polarity (positive = brighter/louder) are provisional.

## Footswitch and assigners (owner pp11-12, 39)

`effectEnabled = !assigned || !plugged || pressed`, evaluated independently for SUSTAIN and for PORTA/GLISS (`footswitchGate`). With no plug the switched jack acts as a pressed switch; a plugged but open switch disables an assigned effect. The virtual switch latches (it is a toggle, so it can be automated); the original is momentary.

- **Sustain gate closed.** Only the `masterSustain * 10 s` release term is removed. Per-rank release and KEY RELEASE survive. Sustain II's tail privileges (a new note cutting earlier tails, tails following the ribbon) are tied to the gate, so with it closed there is no tail to privilege.
- **Gate falls while a tail sounds.** `Engine::cancelSustainTails()` shortens the release of already-released voices through `setRelease`, which keeps stage, value and elapsed stage time. The envelope value does not jump, nothing retriggers and no note is held. Re-opening the gate neither resurrects a finished voice nor lengthens a tail that began without SUSTAIN. The edge acts when parameters are applied, not by per-sample polling, so it does not depend on render partitioning.
- **Glide gate closed.** New notes start on their own pitch, a running glide snaps to its target on the next sample, and glissando quantization is suppressed. Re-opening does not resurrect the discarded transition.
- MIDI CC64 and its per-channel/MPE bookkeeping are an unrelated software convention and are untouched. A MIDI controller cannot say whether a jack is plugged in, so there is no MIDI adapter.

The truth table is a discrete routing law, not a transistor transfer model. The cancellation policy (shorten, don't restart) is an explicit provisional choice; the real switch transient is unmeasured.

## EXP / EXP-WAH (PRA board)

[FOOT_CONTROL_EVIDENCE.md](FOOT_CONTROL_EVIDENCE.md) establishes from PRA53 a dedicated filter/bypass audio network switched by two FETs: EXP passes the dry branch, EXP-WAH the processed branch. It is not a per-voice cutoff offset.

**Implemented.** A stereo post-mix stage after the ring modulator and before the chorus. EXP leaves the signal untouched. EXP-WAH replaces it with a resonant band-pass (Q 4.5, make-up 1.6) whose centre runs 350 Hz upward by 2.9 octaves as the pedal goes down (CC11 or the on-screen PEDAL wheel). A 12 ms crossfade hides the switch, and the filters are reset whenever the stage is fully off. Pedal position still scales volume in both modes.

**Not established:** the sweep range, Q, make-up gain, the band-pass shape itself, the stage's position in the signal chain (the PRA harness block diagram is not in the evidence set) and the optical-resistance and lamp dynamics.

## GENERAL output HIGH / LOW

HIGH is the pre-existing unity output. LOW is an explicit **-20 dB** trim (`kProvisionalLowOutputGain = 0.1`), smoothed over 5 ms. No numerical level difference is documented in this repository's evidence set; the value is a placeholder until the original specification or a measurement is added. Headphone amplifier and load behavior remain unmodelled.

## On-screen key pressure

The original senses pressure per key. The on-screen keyboard now supports a pointer gesture: after the strike, dragging down the key presses harder (full pressure after 40% of the key height) and drawing back up eases off; sliding sideways onto another key is a new strike. The pressure reaches the engine as polyphonic aftertouch on channel 1 through a lock-free per-note mailbox that the audio callback drains **after** the block's note-ons, so a gesture that begins in the same block as its strike is not lost. This is a pointer stand-in, not a model of the pressure sensor.

## UI

A KEYBOARD / FOOT group sits left of the keyboard (which is correspondingly narrower): four contour faders under BRILLIANCE and LEVEL, an on-screen PEDAL wheel, and six switches (PLUG, PRESS, WAH, SUSTAIN and PORTA assigners, HIGH). Every control is a host-automatable parameter with a title for assistive technology; double-click re-centres a lever.

## Validation actually run

`cs80_performance_control_tests` (113 checks): parameter layout and defaults; contour weight table and monotonic, non-overlapping shape; exact `2^octaves` filter ratios at the lever's own end and exact neutrality elsewhere; level leaves the filters alone and vice versa; both ranks; the full 8-row truth table and all 16 assigner/plug/press combinations against observed release length; gate edges (no envelope jump, no resurrection, held notes untouched); Sustain II privilege; glide suppression, mid-glide snap and glissando rounding; EXP versus EXP-WAH spectra, click-free switching and pedal travel; output trim and its smoothing; extreme/NaN inputs and four sample rates. Three deliberate mutations (gate ignored at release, tail cancel removed, contour levers swapped) were each caught.

The UI smoke test adds the host contract (append order, hint 5, defaults, types, state round trip, old-session fallback, preset-recall policy, automation reaching the core, audible EXP-WAH/LOW/pedal), the main-panel controls (host automation moves them, clicks and keys drive the host with balanced gestures) and the key-pressure gesture and mailbox, including the same-block strike case.

These are software-consistency checks. They do not show that any gain matches an original CS-80, and the exact-revision Actions matrix (macOS/Windows builds, pluginval) has to be consulted rather than inferred from a local run.

## Remaining measurement requirements

Lever polarity and taper on the physical panel; the KAS/KBC key map and octave-boundary behavior; real contour voltages into FH/FL/LC; footswitch edge transients; PRA wah transfer and placement; output switch levels. All stay **measurement required**.
