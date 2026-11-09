# Expanded library phrase acceptance

This is a separate, opt-in audit. It does not modify `engine_audit.cpp`, its
historical captures, registered release contracts, or any production audio code.

## Why this exists

The historical phrase audit intentionally calls `factoryPresets()`, which is the
frozen 222-program Legacy Library. It renders that bank at the first requested
rate and every sixteenth program at later rates. Its 283 Source rows therefore
do not cover the separate 301-program curated library or every program at all
three rates. The curated library's level tests do not test retrigger scores.

`src/tools/library_phrase_audit.cpp` adds the distinct
`expanded-library-phrase-v1` namespace. It renders all 523 entries returned by
`programLibrary()`, forced onto the circuit engine, plus eleven paired physical
tone selectors, at 44.1, 48 and 96 kHz: **1,602 rows**. It reuses the original
reporter's render, phrase and metric functions by including that translation
unit with its entry point renamed. This deliberate coupling avoids copying or
silently changing the original analysis equations. The original executable and
source file remain untouched. The new CSV uses 17 significant digits so a small
threshold excess cannot be rounded down to an apparent pass.

The inventory is fixed in `tests/parity/tolerances-v2/library-phrase-v1.json`.
The click limits remain the original `ws1-voice.json` limits: retrigger <=1 and
steal <=1. Peak must remain <=0 dBFS, with no samples above full scale and no
nonfinite audio frames. Every numeric CSV cell must be present and finite.
Schema validity additionally requires non-dB magnitudes and counts to be
nonnegative, the softclip fraction to lie in [0,1], active voices to be an exact
integer in [0,8] for this single Engine, and frame counts not to exceed the
12-second render length. These are reporter domains, not new sound-quality
limits. In particular, finite positive note-off scores above one remain
reported diagnostics and do not introduce a new acceptance threshold.
Unknown, missing, duplicated or substituted fixtures are rejected. A change to
the library inventory requires a separately reviewed contract update.

## Build and run

With an existing core build, use the same compiler and flags used to build that
core archive:

```sh
c++ -std=c++20 -O2 -Isrc src/tools/library_phrase_audit.cpp \
    build-core/libcs80_core.a -o build-core/cs80_library_phrase_audit
python3 tests/test_library_phrase_audit.py \
    --capture-tool build-core/cs80_library_phrase_audit -v
build-core/cs80_library_phrase_audit --out artifacts/library-phrase-v1
python3 scripts/check_library_phrase_audit.py artifacts/library-phrase-v1
```

A standalone build, when CMake is unavailable:

```sh
mkdir -p build-library-audit
c++ -std=c++20 -O2 -Isrc src/tools/library_phrase_audit.cpp \
    src/core/Engine.cpp src/core/FactoryPresets.cpp \
    -o build-library-audit/cs80_library_phrase_audit
```

The complete sweep renders over five hours of audio and can take substantial
wall time. It is deliberately not added to ordinary CTest or default CI.
Use a fresh output directory. Completion of the capture is not a passing
numerical result: always run the checker afterward. Checker exit codes are 0
for complete coverage meeting these targets, 1 for a numerical miss, and 2 for
missing, malformed or incomplete evidence.

For a quick inventory check or diagnostic render:

```sh
build-core/cs80_library_phrase_audit --out artifacts/library-catalog --catalog-only
build-core/cs80_library_phrase_audit --out artifacts/library-bass \
    --fixture program:35 --rate 44100
```

These explicitly record `complete: false` and `diagnostic_subset: true`. The
checker rejects them. Interrupted captures also retain an incomplete receipt.
The full sweep has no quick or rate-sampling mode.

## What a result does and does not establish

Positive synthetic fixture tests establish checker behavior only. Negative
controls cover missing curated programs, stride-16 sampling, easy-row
substitution, duplicate rows, bad schemas, nonfinite/blank fields, changed
policy/rates, malformed counts, each original threshold, and a 1.0000001 score.
Domain controls cover impossible diagnostic values, exact integer parsing,
fraction/voice/frame-count boundaries, and valid diagnostic ratios above one.
The optional compiled test verifies the real source inventory and CLI boundaries
without pretending to measure the complete library.

This audit checks software phrase coverage and numerical CSV results only. It
does not authenticate the source or binary, replace the frozen release gate,
establish hardware parity, prove audible click causality, or qualify release.
Note-off metrics remain unfiltered diagnostics, not a new fitted target.
CPU, aliasing, PURE CS-80 isolation, physical-PANEL causal controls, DAW behavior
and host-state qualification remain separate. On branches with the electrical
programmer, the included reporter retains that branch's programmer coordinate;
those results must not be relabelled as the original physical-PANEL control.
Record the exact source and binary hashes with any real capture, and never use
this additive result to waive a failure in the original registered scope.
