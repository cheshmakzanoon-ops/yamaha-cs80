# Source-model compatibility: unchanged circuits, versioned audio

The original `Seven complete source tones versus V1` job compared the **whole
engine's audio and numerical history** against
`22d6a32c48c85f9b813506edcd856f73f4293c2d`. It failed after two documented,
intentional source-policy changes:

- `2516f4bf8b4e5ac9a56b2011b44e1504ab517fc4` replaced the generic chorus with
  the [OE2 BBD path](BBD_CHORUS_MODEL.md). The fixture enables chorus. Even before
  the delay fills, the former `lerp(dry, wet, mix)` attenuates the dry signal,
  whereas the new path sums the unchanged dry signal with its wet signal.
- `6e5892f5ca6e8a5bfae7ceea3c67d03a74738773` keeps the VCA envelope's current
  level when a source policy reuses a release tail. The fixture exercises
  release/reassignment. See [active-tail continuity](ACTIVE_TAIL_CONTINUITY.md).

Neither is an editor-only change, and neither can honestly satisfy a claim that
all source-policy audio is still byte-identical to the earlier engine. The
source-converter equations and the full audio behavior now have separate,
explicitly versioned gates. **The old failures remain failures; this explains
their cause rather than relabeling them as passing runs.**

## Mandatory comparisons

| Contract | Immutable baseline | Coverage |
| --- | --- | --- |
| Original seven complete source circuits | `22d6a32c48c85f9b813506edcd856f73f4293c2d` (unchanged V1) | All seven columns and nine HP/LP grade combinations; 16,632 unrounded doubles for source equivalents, loaded buses, WSC controls, envelope settings, boundaries, cutoff maps and dynamic cutoff/Q/level readouts |
| Source audio after the documented BBD/tail changes | `6e5892f5ca6e8a5bfae7ceea3c67d03a74738773` (source-audio V2) | The original, unchanged whole-engine fixture: seven selections, four sample rates, eight voices, controller automation, panel/memory transitions, release/reassignment, panic and reset, audio plus numerical history |
| Default-off legacy audio | `d56911a7c5f6953cd244457226b126cc567a538b` (unchanged) | Existing 222-program default and noisy-reassignment corpora at three rates |

The V1 circuit probe is
`src/tools/source_factory_circuit_compatibility_probe.cpp`. It compiles against
each revision's own headers, writes fields individually (never struct padding),
rejects nonfinite values and write failures, and requires exactly 16,632 values.
The portable quantized V1 hash already in `SourceFactoryToneTests` is retained;
the same-compiler CI comparison additionally permits **no rounding**.

`src/tools/source_factory_compatibility_probe.cpp` is unchanged: no chorus, note,
tail, audio sample or state field was removed to make the audio gate pass. Its
checkpoint is a fixed pre-existing commit, not `main`, `HEAD` or a moving branch.
The checkpoint and the inspected integration source (`dc05f7c`) have identical
`src/core` trees: `7029c45645add1c46ffccfc9c559d21813486827`.

Both comparisons run in `.github/workflows/dsp-measurements.yml`, using one
compiler for each pair. Reports record both baseline commits, the candidate
commit, corpus hashes and compiler identity. Independent `BbdChorusTests`,
`ActiveTailContinuityTests`, source integration tests, output/wah oracles and the
full core suite remain required; baseline equality is not a substitute for them.

## Causal verification

The following was executed on Linux x86-64 with GCC 13.3.0 and `-O2 -std=c++20`.
Production engine files were not modified. An isolated copy of `dc05f7c` changed
only these two historical policies for diagnosis:

1. Source-policy `bbdChorus.process` dispatch became the old unconditional
   `chorus.process` call.
2. `startVoice`'s conditional VCA envelope reset became the old unconditional
   `r.aenv.reset()`.

The original full audio/state fixture then matched V1 **byte for byte**. The
unmodified current engine matched the frozen source-audio V2 checkpoint, also
byte for byte. This confines the observed mismatch to the two documented policy
changes for this fixture; it is not merely an unexplained baseline update.

| Corpus | SHA-256 in this local toolchain |
| --- | --- |
| V1 full audio/state, and current with only the two historical policies restored | `5cdff050db69383303b82229d4c71fa7094da6b4d483ff97d91db995732e19ce` |
| Unmodified current full audio/state, and frozen source-audio V2 | `3485b7e034821911cebe0a6aa843b7406857bb62f66da135a276dcf8c470aaa9` |
| V1 and current exact circuit corpus (133,056 bytes each) | `77a312249f671afde87ee673aae6d9cf73f4c41e28bc092c1ceba6a50e0a3beb` |

The unmodified V1/current audio comparison still differs at byte 1, as expected.
An isolated mutation of `nominalWscHalfSpanVolts` from 1.525 to 1.526 was rejected
by the new circuit comparison at byte 38,657; the mutation was then removed.
Hashes here describe this compiler and fixture, not universal cross-platform
golden hashes. CI rebuilds and compares both sides on the same runner.

## Reproduce the circuit check

With a checkout of the immutable V1 commit at `source-v1/`:

```sh
c++ -O2 -std=c++20 -Isource-v1/src src/tools/source_factory_circuit_compatibility_probe.cpp -o circuit-v1
c++ -O2 -std=c++20 -Isrc src/tools/source_factory_circuit_compatibility_probe.cpp -o circuit-current
./circuit-v1 circuit-v1.bin
./circuit-current circuit-current.bin
cmp circuit-v1.bin circuit-current.bin
```

The workflow contains the complete frozen-audio and legacy comparison commands.
Do not move either immutable baseline merely because a future comparison fails.
A deliberate audible change needs a documented scope, causal evidence and
independent behavior tests before establishing another versioned checkpoint.

These checks establish software compatibility with specified versions. They do
**not** establish measured CS-80 equivalence, calibration, host qualification,
new model activation or a release-quality instrument.
