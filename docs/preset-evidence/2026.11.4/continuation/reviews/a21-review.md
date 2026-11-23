# A21 — Lead recipe review

Scope: all 30 recipes in `src/core/curated/Leads.inc`, host indexes 428–457; source starting commit `59d489d1a1f0065ae2a0f42a08c74edc43e94159`. The two later Modern-family Lead entries are outside this file and this checklist.

## Finding

No additional definite named-articulation, octave, controller-routing or unintended-noise defect was found. All 30 use circuit excitation with both body models Off. All retain the default 8-voice polyphonic mode. “Lead” and “Solo” describe a musical role here; none of these descriptions promises a monophonic allocation mode. Portamento works through the Engine's per-voice glide path and does not require mono allocation.

All rank noise and global analog-noise values are zero. Sustains are intentional for held melodic tones and releases are finite. No recipe suggests a permanent noise bed or an unexplained struck sound with an organ-like held tail.

The inspection uses final exported parameter vectors, because fluent source chains can overwrite earlier literals. Definitions were checked against `CuratedLibrary.inc`, `Params.h`, `PerformanceControls.h` and `Engine.cpp` (pitch/ribbon, glide gate, semitone quantization, SUB scaling and level/touch paths).

## All 30 programs

| Host index | Existing program | Source line | Verified role / controls |
|---:|---|---:|---|
| 428 | Touch Lead | 23 | Saw/pulse; pressure controls brilliance, level and pitch depth; small initial scoop and wheel route active. |
| 429 | Aftertouch Vibrato Lead | 30 | No free-running pitch depth; pressure depth0.16 gives approximately48cent peak-to-peak at full pressure; wheel route active. |
| 430 | Ribbon Glide Lead | 38 | 0.056s portamento, small initial scoop; direct ribbon pitch transport is active globally, not dependent on a special recipe switch. |
| 431 | Noir Lead | 46 | Dark main and delayed upper brightness, chorus, 0.64/0.44s releases and restrained pressure vibrato. |
| 432 | Neo-Noir Brass Lead | 54 | Distinct slower filter bloom, chorus and5.6cent detune; both ranks sustained with separate releases. |
| 433 | Glide Lead | 62 | 0.22s portamento; no initial bend; main sustain1.0 retains clear slides. |
| 434 | Sine Lead | 69 | Single audible8-foot sine; second rank excluded; brilliance has no sine-bypass effect, as disclosed; pressure level/pitch active. |
| 435 | Whistle Lead | 75 | Single4-foot sine; no lower rank;0.085 initial scoop, restrained pressure vibrato and quick release. |
| 436 | Square Lead | 82 | Two50% pulse ranks with PWM off, zero detune and no initial scoop. |
| 437 | Narrow Pulse Lead | 90 | Different narrow pulse widths0.765/0.665 with slight PWM; upper rank shorter and brighter. |
| 438 | Resonant Lead | 98 | Higher resonance on upper rank, low-cut main foundation, active pressure brilliance. |
| 439 | Screaming Lead | 105 | Strong upper resonance and short accent above saw/sine foundation; pressure destinations active. |
| 440 | Fifth Lead | 114 | 5 1/3-foot upper rank gives exact3:2 interval; later upper onset0.042s; no detune. |
| 441 | Octave Lead | 122 | 4-foot upper rank gives2:1; higher upper cutoff, quieter mix and nonzero0.28 upper sustain. |
| 442 | Soft Lead | 130 | Saw/sine body, low cutoffs, staggered attacks and modest pressure brilliance. |
| 443 | Init Bend Lead | 138 | Initial bend0.42; no portamento or free-running pitch modulation; pressure/wheel vibrato remains available deliberately. |
| 444 | Pressure Lead | 145 | Both static and initial-touch levels zero, both keyboard-level contours zero; aftertouch-only gain with full sustain. Verified exact silence at pressure 0. |
| 445 | Velocity Lead | 153 | Low static levels and full initial-touch levels on both ranks; measured11.67dB body gain between velocities0.2 and1. |
| 446 | Detuned Lead | 161 | Two saw ranks at12cent offset; no chorus, small pressure vibrato and unequal filter/articulation roles. |
| 447 | Sub Vibrato Lead | 170 | Continuous sine SUB at5.2Hz, depth0.08; pressure/wheel add modest depth. |
| 448 | Trill Lead | 178 | Square SUB6Hz and depth2/3×1.5semitone=±1semitone; pressure/wheel pitch, glide, drift and bend disabled. |
| 449 | Glissando Lead | 187 | 0.19s glide with mode2 quantization; actual Engine rounds pitch to semitones when glide gate active. |
| 450 | Brass Lead | 195 | Main filter/body enters before brighter saw crown; short releases and active pressure bloom. |
| 451 | Reed Lead | 203 | Dry centered pulse pair, near-square main and narrower upper edge; pressure brilliance and modest vibrato. |
| 452 | Wah Lead | 211 | 3.8Hz SUB sent to filter only through pressure; static, pressure and wheel pitch depths all zero. |
| 453 | Ring Lead | 219 | 280Hz ring carrier with wet amount0.19; zero carrier-sweep depth does not disable the ring sidebands. |
| 454 | Hollow Lead | 227 | Near-square main and slightly stronger moving upper pulse; distinct slow PWM and no initial scoop. |
| 455 | Ether Lead | 235 | Single8-foot sine, no second beating rank;0.32s glide,0.05s onset,0.52s release and pressure level/vibrato. |
| 456 | Solo Synth | 242 | Fast saw/pulse onset with0.015s glide and modest pressure pitch depth; no claim of forced mono mode. |
| 457 | Contour Lead | 249 | Actual low/high keyboard brilliance and level controls differ; main/upper attacks and active pressure routes retain solo articulation. |

## Four targeted complete-preset captures

Pinned executable: `evidence/baseline/cs80_preset_capture`, built from the starting source. All 30 Lead models are Off, so the proposed tine/bowed corrections do not change their excitation. The executable identity and exact source hashes are recorded in `a21/provenance.json`.

Fixture: MIDI 60,48 kHz,1-second hold,0.3-second tail. These are software behavior checks, not acoustic realism or broad long-tail qualification.

| Existing program / control | Peak dBFS | Body RMS dBFS |
|---|---:|---:|
| Pressure Lead, pressure 0 | exact silence (reported floor−240) | exact silence (reported floor−240) |
| Pressure Lead, pressure 1 | −15.737971 | −24.826295 |
| Velocity Lead, velocity 0.2 | −28.765410 | −37.346021 |
| Velocity Lead, velocity 1 | −16.277620 | −25.678555 |

Velocity Lead's body RMS rises **11.667466 dB** and peak rises **12.487789 dB**. Pressure Lead's aftertouch requirement is intentional and explicitly stated in the preset description. All four captures contain zero non-finite values and zero samples above full scale. The pressure-zero file reports zero spectral power as well as the numerical dB floor.

## Limits and action

No production edit is recommended from this focused review. The preserved library-wide headroom/phrase tests and native qualification still apply. This review makes no listening-based quality or exact-instrument-equivalence claim. No presets, tests, commits or remote refs were changed by this reviewer.
