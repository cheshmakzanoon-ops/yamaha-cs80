# Performance controls: keyboard contour, footswitch assigners, EXP/WAH, output switch, key pressure

> **Policy precedence (2026-10-29):** Under [the current requirement](ANALOG_FIDELITY_REQUIREMENT.md) and [roadmap](PARITY_ROADMAP.md), missing physical measurements do not block development or reviewed integration. Use public evidence plus explicitly labeled engineering inference, with assumptions, uncertainty and tests. Earlier measurement/promotion gates below remain limits on claims of physical verification, not a prohibition on inferred models; recorded offline status and source conflicts remain unchanged.

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

**Preset recall.** Loading a factory preset recalls the sound, not the wiring: `footswitch_plugged`, `footswitch_pressed`, `foot_mode` and `output_level` are left where they are. The four contour levers and both assigners are part of the patch and return to their neutral values with it. User presets and host state restore all ten. Parity isolation is a separate session policy: both factory and user presets preserve its current setting, whereas host state restores its saved setting.

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

**Placement verified (2026-10-29):** current PRA53/54, Panel2 and OE55/56 scans, corroborated by the overall foldout, place the wah after the ring MODULATION blend and before the OE1/OE2 send-return/mixing network; the separate PRA optical expression paths follow OE1 and precede stereo VOLUME. See [signal-routing evidence](OE1_SIGNAL_ROUTING_EVIDENCE.md) for pins, hashes, crops and the unresolved OE2 input-number discrepancy. This validates coarse stage order, not the stereo stand-in or chorus implementation.

**Not established:** the sweep range, Q, make-up gain, the band-pass shape itself, optical-resistance and lamp dynamics, and quantitative OE1/OE2 transfer.

## GENERAL output HIGH / LOW

HIGH is the pre-existing unity output. LOW is an explicit **-20 dB** trim (`kProvisionalLowOutputGain = 0.1`), smoothed over 5 ms. No numerical level difference is documented in this repository's evidence set; the value is a placeholder until the original specification or a measurement is added. Headphone amplifier and load behavior remain unmodelled.

## On-screen key pressure

The original senses pressure per key. The on-screen keyboard supports a one-pointer gesture: after the strike, dragging down the key presses harder (full pressure after 40% of the key height) and drawing back up eases off; sliding sideways onto another key is a new strike. This is a pointer stand-in, not a model of the pressure sensor.

**Pointer-pressure safety limits:** it is disabled while the effective arpeggiator runs, and suppressed when any active host/unattributed voice shares the pitch—even on another MIDI channel or in its release tail. This conservative rule prevents the engine's pitch-group aftertouch from modifying a host note. A refused ninth held/pedal-held key never acquires pressure ownership. Ordinary MIDI polyphonic/channel aftertouch keeps its existing semantics and does not require a UI token.

Simultaneous UI/host notes on the same channel and pitch still share the engine's newest-note-off identity; protecting a newer host strike can leave an older UI voice requiring PANIC. The pressure fix is not full per-voice source-targeted note-off support.

The actual JUCE note-on publication supplies a processor-specific epoch/onset token to the gesture. Pressure shares the bounded keyboard FIFO with note-on/off, in timestamp order; the audio consumer takes no locks, allocates nothing and uses no retry loop. This intentionally replaces the old untagged, block-end, last-value mailbox. First pressure in the onset's block still works; pressure before a release cannot leak into a later same-pitch strike, and stale pointer movement after prepare/release, PANIC or an effective mode change cannot adopt a fresh host note. A same-key drag retains its original token; lift/restrike or a cross-key slide supplies a genuinely new one. Mode changes also discard any already-captured old UI batch. Queue overflow follows bounded UI-key recovery, not a channel-wide host panic. See [the detailed handoff contract](KEY_PRESSURE_HANDOFF.md).

## Pedal position is shared state

The processor owns the foot-controller position. The on-screen wheel and MIDI CC11 both update it; `prepareToPlay` (transport restart, sample-rate change) re-applies it because the engine resets expression to full travel there; and the editor reads it back, so a reopened window and a CC11 move are reflected by the wheel. Without this the wheel and the sound could disagree by a factor of three in level. The position is a performance control and is not saved in session state.

## UI

A KEYBOARD / FOOT group sits left of the keyboard (which is correspondingly narrower): four contour faders under BRILLIANCE and LEVEL, an on-screen PEDAL wheel, and six switches (PLUG, PRESS, WAH, SUSTAIN and PORTA assigners, HIGH). Every control is a host-automatable parameter with a title for assistive technology; double-click re-centres a lever.

## Validation actually run

`cs80_performance_control_tests` (113 checks): parameter layout and defaults; contour weight table and monotonic, non-overlapping shape; exact `2^octaves` filter ratios at the lever's own end and exact neutrality elsewhere; level leaves the filters alone and vice versa; both ranks; the full 8-row truth table and all 16 assigner/plug/press combinations against observed release length; gate edges (no envelope jump, no resurrection, held notes untouched); Sustain II privilege; glide suppression, mid-glide snap and glissando rounding; EXP versus EXP-WAH spectra, click-free switching and pedal travel; output trim and its smoothing; extreme/NaN inputs and four sample rates. Three deliberate mutations (gate ignored at release, tail cancel removed, contour levers swapped) were each caught.

The UI smoke test adds the host contract (append order, hint 5, defaults, types, state round trip, old-session fallback, preset-recall policy, automation reaching the core, audible EXP-WAH/LOW/pedal), the main-panel controls (host automation moves them, clicks and keys drive the host with balanced gestures), the token-owned key-pressure gesture (including the same-block strike case), and pedal-position persistence across `prepareToPlay`, CC11 and a reopened editor (mutation-checked).

These are software-consistency checks. They do not show that any gain matches an original CS-80, and the exact-revision Actions matrix (macOS/Windows builds, pluginval) has to be consulted rather than inferred from a local run.

## Remaining measurement requirements

Lever polarity and taper on the physical panel; the KAS/KBC key map and octave-boundary behavior; real contour voltages into FH/FL/LC; footswitch edge transients; PRA wah transfer and optical dynamics; output switch levels. Coarse PRA stage placement is source-verified above. All stay **measurement required**.
