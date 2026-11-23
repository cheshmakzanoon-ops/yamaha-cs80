# Agent 22 — existing FX recipe review

Reviewed all 24 curated FX programs, host indexes 480–503, in
`src/core/curated/Fx.inc`, the exported final 2026.11.4 inventory,
`CuratedLibrary.inc`, `ModulationLfo.h`, and the Engine modulation/VCA routes.
All 24 select **Off** for both instrument bodies. The tine and bowed-body
corrections therefore do not alter their excitation.

## Finding requiring correction

**Wind's description reverses the order of its two entrances.** It says the
upper airy gust arrives later, but rank II has a 0.71-second amplitude attack
and 1.05-second filter attack; rank I has 1.65/2.3-second attacks. Six isolated
rank captures at C4, velocity 0.8, 48 kHz and eight-second holds confirm:

| Program | Low-rank RMS peak | Upper-rank RMS peak | Interpretation |
|---|---:|---:|---|
| Wind | 1.95 s | 0.75 s | Upper gust enters first, then the low bed blooms |
| Surf | 1.45 s | 1.85 s | Low surge precedes spray, as described |
| Thunder | 0.25 s | 0.05 s | Fast crack precedes rumble, as described |

RMS uses consecutive 100 ms windows, with their center times reported. Wind
first crosses half of each rank's own peak RMS at 0.75 seconds for rank I and
0.35 seconds for rank II. This is a measured description/gesture mismatch,
not a missing signal route or a recommendation to change every noise recipe.
The safest correction is to describe the existing faster upper gust settling
into the slower low bed. If a deliberately delayed gust is required instead,
reprogram its envelopes and remeasure the complete preset.

## Complete recipe checklist

| Program | Checked behavior and result |
|---|---|
| Random Filter | Modern waveform 5 is S&H; 7.5 Hz filter steps, sustained saw body, short octave pulse. Coherent. |
| Random Pitch Blips | 6.5 Hz S&H pitch and square modern amplitude routes are active. Both VCA base levels permit the negative amplitude phase to close the notes; the SUB exponential path is not mistakenly used as a gate. |
| Siren | Sine main and octave support; original SUB pitch scale gives the stated three-semitone peak-to-peak excursion at depth 1. Pressure speed route is active. |
| Laser Zap | No bypass sine; resonant pulse filter descent plus separately short high noise. Both ranks have zero sustain. |
| Wind | Distinct low/air noise bands and restrained 0.7 Hz motion; pressure favors upper air. **Entrance description is reversed**, measured above. |
| Surf | Separate zero-sustain surge/spray ranks, long unequal decays, weak periodic modulation. Measured low peak precedes spray. No repeating amplitude-envelope loop is configured. |
| Sci-Fi Drone | 16′ versus 5⅓′ is a 3:1 upper relation; late upper pulse, slow 3.1 Hz ring motion and sustained lower body match the description. |
| Robot Voice | Two different resonant bands, short upper consonant layer, active fixed 73 Hz ring amount and pressure-brilliance routes. Robotic character is intentional. |
| Alarm | 4′ call above quieter 8′ accent; square SUB pitch depth 1 alternates tones three semitones apart. Level motion accents rather than falsely promising complete silence between phases. |
| Space Sweep | 16′ foundation and 4′ upper pulse; independent delayed envelopes and an active 31 Hz ring-carrier envelope. |
| Bubbles | 11 Hz S&H filter motion plus 2.3 Hz smooth-random amplitude motion; resonant pulse with subordinate noise. |
| Thunder | Zero sustained gain, broadband fast crack and slow low rumble, reduced resonance. Measured onset order matches description. |
| Rain | 16 Hz S&H level modulation combined with 0.23 Hz smooth-random filter and 3.7 Hz smooth-random level movement; two distinct noise bands. No single periodic amplitude LFO defines the wash. |
| Helicopter | Intentional 11.5 Hz square SUB rotor pulses, 16′ pulse motor and separate noise band; aftertouch speed route active. Underlying motor remains audible as described. |
| UFO Landing | Repeating downward SUB pitch glide and phrase-triggered ring sweep are separate mechanisms; description correctly explains releasing all held/sustained notes before retriggering the shared sweep. |
| Telemetry | 4′/2′ pulse layers, true 7 Hz square pitch alternation, short upper edge and releases. Synthesized signal is intentional. |
| Clockwork | Modern square amplitude LFO at 6 Hz can close the low-base-level VCAs; 3 Hz original SUB filter alternation adds color. Not the incorrect exponential-SUB-as-gate construction. |
| Deep Space | Slow 16′ sine bloom followed by clouded noise, sub-hertz ring beating and long independent releases. |
| Metal Scrape | Short broadband upper scrape and slower resonant saw/noise body; active falling ring carrier. Small main sustain is intentional in its current envelope. |
| Machine Hum | Steady 16′ pulse and quiet 8′ buzz with fixed 50 Hz ring sidebands; gradual start and shorter upper release. |
| Sweep Riser | Five-second main filter attack with IL/AL extremes, later 5.5-second noise-filter rise and slower amplitude blooms. Held rise is distinct from pitch rise. |
| Downer | Bright held signal followed by release toward negative filter IL; VCA release exceeds filter release, keeping the descent audible. |
| Glitch Sub | Intentional 30 Hz square filter steps and shallow SUB level modulation; short saw accent above low pulse. |
| Breath FX | Distinct low-breath/high-air bands, faster upper layer, stronger upper pressure-level amount and independent release. |

## Limits relevant to the user's complaint

- Nature FX remain authored filtered-noise gestures. They do not model measured
  turbulent gusts, individual droplet impacts, surf propagation or thunder
  acoustics. A routing audit cannot establish acoustic realism.
- Wind, Surf and Thunder retain shallow 0.7 Hz periodic SUB modulation, but it
  is subordinate to separate envelopes and continuously generated noise. A
  deterministic pseudorandom stream is reproducible; that alone does not make
  it a short repeating recorded loop.
- Rain's random values advance at fixed clock intervals, including the 16 Hz
  droplet-level path. This is controlled irregular modulation, not a physical
  stochastic arrival-time model. Its two additional smooth-random paths help
  avoid a single obvious repeating intensity cycle.
- Circuit filter keyboard tracking still changes noise-band color across the
  keyboard. These are playable synthesizer FX rather than invariant recordings
  of environmental sounds.

No production files, tests, presets, commits or refs were changed. No subjective
listening or reference-audio-equivalence claim is made.

## Evidence

`a22/provenance.json` binds the copied capture executable and reviewed FX source.
`a22/envelope-comparison.json` retains all 100 ms RMS windows and extracted times.
The six `a22/*-rank*/` directories contain unnormalized WAVs and capture metrics.
