# Bowed body response during pitch motion

Implemented the root-approved bowed-only correction in `src/core/InstrumentBody.h`, preserving the prior tine normalization fix. Added `tests/BowedBodyFormantTests.cpp`; root owns CMake registration.

A bowed instrument's body resonances do not transpose with a player's vibrato or slide. The old code captured formant weights once at note onset, then moved every oscillator while retaining those weights. The correction evaluates the same authored broad response at each harmonic's live frequency. Excitation, attack and size still latch at note onset. A common gain preserves the previous absolute-mode-sum ceiling; the existing control-rate gain ramps smooth changes. This introduces no allocation and no operations in the per-sample modal rotation loop.

The correction applies only to the bowed model. Wind and brass behavior is unchanged. The primary qualitative reference is UNSW's [Articulation and vibrato on the violin](https://www.phys.unsw.edu.au/jw/violinarticulation.html). The profile's actual numerical formants remain authored values, not fitted instrument measurements.

## Verification

- New regression: 492 checks passed, zero failed, across 44.1/48/96 kHz, body sizes 0.75/1/1.8, upward/downward octave slides and +/-35-cent targets, plus wide continuous pitch safety.
- The same regression on the pre-correction header fails 174 checks, establishing a useful negative control.
- Existing direct body suite: 2,077 checks passed, zero failed, 41,062,350 samples generated.
- Exact comparison: 36,115,200 samples with zero differences. This includes bowed fixed pitch and every other model with +7-semitone slides, across three registers/rates.
- In the focused bowed comparison, maximum harmonic-ratio error versus playing the target directly fell from 4.10–6.19 dB to 0.003–0.025 dB. This verifies the stationary-body behavior, not a real violin match.

## Cost

Two 24-element arrays store the bowed response; no extra fields were added to every 48-element Mode record. Body storage rises from 5,688 to 6,088 bytes and Engine storage from 478,632 to 491,432 bytes on this Linux build.

Eleven pinned, interleaved thread-CPU pairs measured eight isolated bodies with 5 Hz +/-40-cent vibrato. Median paired candidate/baseline ratio was 1.1243 for moving bowed bodies and 1.0113 for fixed bowed bodies. Organ controls were 0.9879 fixed and 0.9825 moving. These are noisy shared-machine body-only estimates; full Engine/DAW cost requires separate qualification.

Code, baseline/prototype snapshots, negative controls, measurements and SHA256 hashes are retained with `summary.json`. No commit or push was made by this agent.
