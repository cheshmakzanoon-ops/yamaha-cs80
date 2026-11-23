# A18 — complete Polysynth recipe review

Source base: `59d489d1a1f0065ae2a0f42a08c74edc43e94159`. Reviewed all24 curated Polysynth recipes, host indexes312–335, against final inventory and Engine/helper routes. The separate Modern-family Phased Polysynth at index520 is outside this file's24 recipes.

## Outcome

No additional definite model, footage, controller-routing or named-articulation defect found. Every reviewed program uses circuit excitation (both body models Off), so the tine/bowed body corrections do not change their excitation. Differences in waveform, rank envelope, modulation, pressure and registration are active rather than dormant body-blend settings. This is a source/behavior review, not a subjective quality or acoustic-match score.

## Complete checklist

| Host index | Existing program | Sustain I / II | Release I / II (s) | Verified behavior |
|---:|---|---:|---:|---|
| 312 | Poly Stab | 0 / 0 | 0.23 / 0.085 | Dry saw body/short bright accent; both sustain values zero; initial-touch level and brilliance active. |
| 313 | Poly Brass Stab | 0 / 0 | 0.25 / 0.14 | Staggered47/69ms filter attacks and18/32ms amp attacks; both sustain values zero. |
| 314 | PWM Poly | 0.8 / 0.52 | 0.46 / 0.29 | Pulse-only ranks with independent0.293/0.467Hz PWM and0.18/0.27 depth; both sustain. |
| 315 | Resonant Poly | 0.72 / 0.18 | 0.36 / 0.14 | Shorter high-resonance upper accent over warmer saw body; both rank envelopes remain active. |
| 316 | Detuned Poly | 0.82 / 0.56 | 0.48 / 0.35 | 17-cent inter-rank detune, saws, chorus off; unequal entrance and sustain. |
| 317 | Poly Sweep | 0.87 / 0.54 | 0.75 / 0.52 | Filter crests0.29/0.56s follow faster amp onset; aftertouch brilliance active on both. |
| 318 | Velocity Poly | 0.83 / 0.21 | 0.4 / 0.17 | Low static levels0.13/0.07 with initial-touch levels0.94/1; measurable full-patch dynamics. |
| 319 | Poly Pluck | 0 / 0 | 0.56 / 0.05 | Zero sustain both ranks; upper decay0.19s and lower4.2s; dry patch. |
| 320 | Octave Poly | 0.84 / 0.23 | 0.46 / 0.18 | 8-foot root/4-foot upper rank produces2:1; upper mix0.24, sustain0.23 vs root0.84. |
| 321 | Fifth Poly | 0.83 / 0.37 | 0.46 / 0.33 | 8-foot/5⅓-foot produces3:2; zero detune, upper mix0.23 and lighter sustain. |
| 322 | Bright Poly | 0.8 / 0.15 | 0.28 / 0.115 | Open LP4300/7000Hz, restrained resonance and short narrow-pulse upper accent. |
| 323 | Dark Poly | 0.9 / 0.42 | 1.35 / 0.65 | LP780/1340Hz, saw+sine foundation, quieter pulse; root release1.35s vs upper0.65s. |
| 324 | Hollow Poly | 0.81 / 0.34 | 0.38 / 0.19 | Near-square pulses0.50/0.535 with shallow PWM; chorus off and slight0.65cent detune. |
| 325 | Wide Poly | 0.81 / 0.52 | 0.63 / 0.4 | Explicit pans−0.28/+0.35, upper PWM, active broad chorus and unequal releases. |
| 326 | Poly Swell | 0.98 / 0.82 | 0.91 / 0.67 | Quiet nonzero bed; aftertouch level0.84/0.94 and brilliance0.24/0.38; not pressure-only silence. |
| 327 | Saw Pulse Poly | 0.84 / 0.2 | 0.43 / 0.16 | Sustaining saw and shorter pulse accent; distinct filter/amp decay and modest stereo pan. |
| 328 | Synthwave Chords | 0.87 / 0.62 | 1.85 / 1.24 | 9.4cent detuned saws, active chorus, pans−0.24/+0.32 and releases1.85/1.24s. |
| 329 | Square Poly | 0.88 / 0.35 | 0.44 / 0.19 | Both pulse widths0.5, PWM depths zero, chorus off; light detune retains square identity. |
| 330 | Sine Poly | 0.98 / 0.04 | 0.85 / 0.14 | Main sine0.91 with saw/pulse/VCF output zero; quieter saw accent retains0.04 sustain. |
| 331 | Contour Poly | 0.82 / 0.4 | 0.44 / 0.26 | Keyboard brilliance−0.34/+0.22 and level−0.06/+0.18 shape bass/treble differently. |
| 332 | Ring Poly | 0.82 / 0.26 | 0.49 / 0.21 | 137Hz ring carrier and amount0.14 active; zero depth disables sweep, not ring effect. |
| 333 | Glide Poly | 0.92 / 0.7 | 0.86 / 0.79 | Portamento0.16s with ordinary glide mode; both held ranks sustain and have close releases. |
| 334 | Tremolo Poly | 0.88 / 0.64 | 0.49 / 0.36 | True5.4Hz sine SUB VCA depth0.20 plus BBD tremolo; no SUB pitch/filter modulation. |
| 335 | Poly Sub Pulse | 0.96 / 0.66 | 0.57 / 0.41 | True4Hz square SUB filter depth0.24; pitch/amplitude depths zero; steady held VCA. |

## Targeted rendered checks

Used a pinned copy of the baseline `cs80_preset_capture` at the source above (SHA256 recorded in `a18/provenance.json`), avoiding any mid-build binary ambiguity. Each fresh complete-preset capture used MIDI60,48kHz,3-second hold and1-second release. No preset changes or normalization.

| Case | First condition | Second condition | Measured difference |
|---|---:|---:|---:|
| Velocity Poly, velocity0.2→1 | Body RMS−38.946dBFS | Body RMS−27.754dBFS | +11.192dB |
| Poly Swell, pressure0→1 | Body RMS−34.571dBFS | Body RMS−21.700dBFS | +12.871dB |
| Poly Stab, early→late-held body | Early RMS−31.437dBFS | Body RMS−106.049dBFS | −74.612dB |
| Poly Pluck, early→late-held body | Early RMS−26.358dBFS | Body RMS−65.181dBFS | −38.823dB |

All six captures were finite with zero full-scale samples. Poly Swell intentionally sounds quietly without pressure; it must not be confused with a pressure-only lead. Poly Stab and Poly Pluck decay under a held key instead of leaving an unintended sustained bed. These single-note fixtures do not replace the full-bank chord/headroom gate.

## Implementation references

- `src/core/curated/Polysynth.inc`: all24 recipes.
- `src/core/CuratedLibrary.inc`: wave, feet, touch, chorus/tremolo, ring, glide and contour helpers.
- `src/core/Params.h`: footage ratios `{0.5,1,1.5,2,3,4}` and chorus speed range0.05–8Hz.
- `src/core/Engine.cpp`: SUB VCF enters filter brilliance, circuit SUB VCA enters exponential gain, ring amount remains independent of sweep depth, sine bypasses HP/LP, portamento acts through the glide gate.

No production files, tests, commits or remote refs changed. Complete capture metrics are in `a18/measurements.json`.

Reviewed source SHA256: `0bb56e1752400a4ba5d896b6e6ee801b7cf7913969ce4789143a905c42b4e356`.
