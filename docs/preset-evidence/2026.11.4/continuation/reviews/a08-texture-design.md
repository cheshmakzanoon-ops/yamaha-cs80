# Agent 08 — acoustic texture audit

Scope: `src/core/InstrumentBody.h` at continuation baseline `59d489d`.
No production source changed.

## Decision

Source-specific residual filtering is a defensible architecture, but the inspected
primary sources do not establish universal noise cutoffs for each instrument.
Do not publish a hand-picked cutoff table as a measured realism improvement.
For the current release, prioritize the demonstrated tine normalization problem.
For a next acoustic improvement, fixed-frequency bowed-body filtering through
vibrato has a stronger direct perceptual rationale than increasing noise.

The present texture implementation has one white-noise stream through a
650 Hz high-pass and 5200 Hz low-pass for every body, a 19 Hz flutter pole,
and model-specific amplitude/decay. The per-note coefficients are inexpensive
to specialize without adding audio-rate work. This can be prototyped safely,
but listening/reference comparison is still needed to justify shipping it.

## Primary evidence

- Laurenti and De Poli, DAFx 2000, *A Method for Spectrum Separation and Envelope
  Estimation of the Residual in Spectrum Modeling of Musical Sound*:
  https://www.dafx.de/paper-archive/2000/pdf/Laurenti_DePoli_se.pdf
  Separates quiet stochastic residual from harmonic content, fits its spectrum
  and time envelope, and keeps piano percussive noise untransposed when the
  sinusoidal component is pitch-shifted. This supports independent texture
  spectra; it supplies no universal HP/LP settings.
- Ystad, DAFx 1999, *Identification and Modeling of a Flute Source Signal*:
  https://www.dafx.de/paper-archive/1999/ystad.pdf
  Separates source from resonator and models the stochastic source through
  linear filtering. Supports measured residual fitting, rather than indiscriminate
  white noise added to every wind.
- UNSW Music Acoustics, *Violin acoustics: an introduction*:
  https://www.phys.unsw.edu.au/jw/violintro.html
  Describes measured body response and the timbral change as vibrato sweeps
  harmonics across fixed body resonances; it identifies onset and this timbre
  vibrato as important violin recognition cues. Current onset-frozen body weights
  miss this moving-harmonic/fixed-body interaction.
- Bank and Sujbert, DAFx 2002, *On the Nonlinear Commuted Synthesis of the Piano*:
  https://www.dafx.de/papers/DAFX02_Bank_Sujbert_piano_synthesis.pdf
  Separates hammer force, string and soundboard contributions. Its method is
  richer than a short colored-noise burst; the current signal model should not
  be described as that measured physical model.
- Pfeifle, DAFx 2017, *Real-time Physical Model of a Wurlitzer and Rhodes
  Electronic Piano*:
  https://www.dafx.de/paper-archive/details/W0D0fMDJBUxiacAi_398wg
  Models mechanical generator and pickup independently and compares measurements.
  Do not equate direct acoustic action clack with the pickup's electrical output.

## Optional prototype parameters — authored estimates only

These are proposed audition starting points, not measurements or accepted
production settings. Keep current noise amounts/envelopes initially; do not
raise them to force an audible difference. Leave organ silent and retain current
harpsichord/bar textures until separately justified. Keep flutter unchanged so
the experiment isolates spectral texture.

| Model | HP Hz | LP Hz | Filtered-noise power change vs current at 96 kHz | Maximum noise-gain multiplier |
|---|---:|---:|---:|---:|
| Piano | 120 | 3000 | -1.86 dB | 1.000 |
| Tine EP | 250 | 2400 | -3.12 dB | 1.000 |
| Reed EP | 300 | 3200 | -1.86 dB | 1.000 |
| Nylon | 180 | 2800 | -2.28 dB | 1.000 |
| Steel | 700 | 6000 | +0.62 dB | 0.931 |
| Bass | 100 | 1800 | -4.13 dB | 1.000 |
| Bowed | 850 | 4800 | -0.60 dB | 1.000 |
| Flute | 1800 | 8000 | +1.09 dB | 0.882 |
| Clarinet | 500 | 3600 | -1.58 dB | 1.000 |
| Oboe | 800 | 5200 | -0.15 dB | 1.000 |
| Bassoon | 300 | 2800 | -2.49 dB | 1.000 |
| Brass | 700 | 4000 | -1.32 dB | 1.000 |

Power changes are calculated from the exact two-pole discrete transfer function
using 65537 equally spaced frequency points at internal 96 kHz. Multipliers
only attenuate wider-band proposals so stationary filtered noise energy does
not exceed the baseline. These numbers are arithmetic checks of the proposal,
not perceptual or acoustic validation. Recompute for every supported rate.

Implementation: compute coefficients at note onset after the model is latched,
using the prepared internal sample rate; reset existing filter histories as
already done. No extra filter states, RNG calls, trigonometry or branches in
the sample loop, no new preset IDs/parameters, and no change to harmonic modes.
For parameterized size, initially keep this residual spectrum independent of
pitch/size so a ribbon bend cannot transpose mechanical contact noise.

## Acceptance before shipping

1. Extract isolated texture via test-only matched render with noise disabled,
   keeping RNG advancement identical; compare 20 ms attack and held residual
   spectra independently from pitched harmonics.
2. At 44.1, 48 and 96 kHz, require no sustained residual RMS increase over
   current settings; keep same seeded block-partition invariance and finite
   output. The broader bands need rate-correct attenuation.
3. Verify fixed body-model noise spectrum across pitch bends (only musical
   envelope/rank filter behavior may change), and absence of persistent noise
   for struck models once their hit envelope decays.
4. Compare complete existing presets at low/middle/high registers, soft/firm
   strikes, repeated notes and held wind/string notes. Preserve current phrase
   RMS and stress peak gates, but do not treat them as a realism judgment.
5. Perform level-matched blinded auditions/reference comparison before claiming
   improvement. Reject family cutoff choices that merely add an audible hiss,
   thud, or unrelated electronic transient.

A stronger longer-term route is to fit residual spectra/envelopes from licensed
reference recordings and apply fixed-frequency resonator responses. That work
needs real reference evidence and should not be replaced by arbitrary coefficients.
