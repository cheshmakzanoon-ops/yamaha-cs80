# All-22 source-model listening audition

`cs80_source_factory_audition` renders the **existing production Engine**, not the
experimental oscillator prototype. It is a convenient listening fixture for the
nominal factory-source model, **not a hardware recording, calibration, parity
comparison, live-selector test, or performance benchmark**. Seven columns have
complete classified source cells; fifteen use the explicit inference overlay.
Even complete cells retain inferred device/receiver laws.

## Build and render (core only)

From the repository root, using the existing C++20 compiler; no JUCE, audio
hardware, extra packages, or paid tools are needed:

```sh
CORE_COMMIT=df08d1e3c6a1df59cb083419a2fcbf6b1c6dbaee
# Required provenance check: do not label changed core code as this revision.
git diff --exit-code "$CORE_COMMIT" -- src/core
c++ -O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -Isrc \
  src/tools/source_factory_audition.cpp src/core/Engine.cpp \
  -o /tmp/cs80-source-factory-audition
/tmp/cs80-source-factory-audition \
  --out /tmp/cs80-source-factory-audition-df08d1e \
  --source-commit "$CORE_COMMIT"
```

With its CMake target available, build `cs80_source_factory_audition` from a
`CS80_BUILD_PLUGIN=OFF` configuration instead. The commit argument describes the
verified **core tree**, not necessarily the later commit adding this fixture.
It is caller-supplied provenance, not a runtime Git attestation. Use a fresh output
directory: existing WAV/JSON/temporary outputs are refused, not overwritten.

Outputs are `source-factory-all22.wav` (48 kHz, two-channel, little-endian PCM16)
and `source-factory-all22.json`. Keep generated audio outside Git.

## Controlled listening setup

- Order: rank I's eleven hardware tabs, then rank II's eleven, in panel order.
- A fresh `Engine` per segment accepts one source `ControlFrame` with isolation
  enabled; `reset()` initializes the fixture after applying the frame. This is
  deliberately **not** a demonstration of switching a sounding source live.
- The selected rank is audible through MIX 0 (I) or 1 (II). The other rank stays
  at PANEL 13 with fixed common parameters, mixer-muted but with its modeled
  shared receiver ports retained. Both use 8-foot footage; no per-tone transpose.
- Identical phrase: MIDI 48, 52, 55, 60 (C3/E3/G3/C4), velocity 0.75, note-ons at
  0.15/0.45/0.75/1.05 s; all four release at 2.10 s. Expression 1, pressure/ribbon
  zero, no pedal. Slowly attacking sounds get time to develop into the chord.
- One quiet master setting, **0.12 for every source**. No per-tone normalization,
  limiter, clamp, fade, resampling, or added dither. Float samples are checked
  before PCM conversion (`round(sample * 32767)`). If peak is at least 0.9, the
  entire 22-source capture is rerendered at one lower uniform master setting.
- Chorus, ring amount, global SUB modulation, external input, modern FX,
  dispersion/drift/background noise, and detune are off. Source-defined PWM and
  noise remain part of each tone. Existing inferred filter saturation, dry/output
  coupling, output HIGH circuit, and production decimator remain in the Engine.
- Each segment lasts at least 3.5 s. All voices must be inactive, selected-rank
  amplitude **and filter** envelopes idle, and the final 100 ms below half a
  PCM16 LSB before proceeding. Longer releases extend the segment, never truncate
  it. A 25-second segment safety bound and 24 MiB WAV budget fail explicitly.

The manifest records every common parameter and segment MIX override, selectors,
source names/addresses, model/source/hypothesis hashes, original known/unknown and
inferred masks, retained-bridge mask, UTC render times, exact frame/second offsets,
events, release observations, and per-segment/channel/whole-capture float peak,
RMS, nonfinite and out-of-range counts. Bit 0 is source output 1; `unknown` preserves
the original evidence exclusion and intentionally overlaps `inferred`.

## Verified initial capture

Core `df08d1e3c6a1df59cb083419a2fcbf6b1c6dbaee`; model 2
`nominal-factory-v2-all22-median-overlay-v1`; source hash `0xa95212770767733c`;
hypothesis v1 hash `0xe5f877a025fc52c6`.

- 22 segments; **90.312 s**, **17,339,948 bytes** (16.54 MiB).
- Uniform master 0.12, first attempt; float peak **0.2510508597** (about −12 dBFS),
  combined RMS **0.0210293913**; zero nonfinite/out-of-range samples.
- Rank-I Funky 3 occupies 35.000–51.812 s: its long filter-only release is allowed
  to finish, explaining the longer quiet gap. Every other segment is 3.5 s.
- Independent Python `wave`/JSON checks confirmed format/size, all 22 contiguous
  named segments, seven complete/fifteen inferred masks, nonzero audio in every
  segment, final 100 ms quantizing to zero, and PCM peak/RMS within half a LSB of
  the pre-conversion float statistics. Usage/overwrite rejection also passed.
- WAV SHA-256:
  `56a4575df6049aeb36297fca736a8f9c0d9cd23abe493f4d89331daed05a4013`.

RMS includes silence, so the manifest also includes phrase-window RMS. Tone levels
are intentionally not equalized; perceived loudness differences are model output,
not evidence that the corresponding physical instrument has those differences.
