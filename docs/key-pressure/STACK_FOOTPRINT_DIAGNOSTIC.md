# Pressure ownership storage: bounded eight-slot footprint

## Failure and evidence limits

Windows CI for pressure commit `16735ee` exited the UI smoke test with a
segmentation-fault classification after 0.01 s, without assertion text:
[run 41020919700, job 123947798988](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41020919700/job/123947798988).
The other 38 tests passed. The original test-log artifact is
[12962320286](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41020919700/artifacts/12962320286).

The enlarged processor's stack footprint is a supported hypothesis, **not a
proven Windows crash diagnosis**: there is no Windows debugger stack trace or
MSVC frame report here. The separate Linux/macOS pointer assertions had a
different cause: a peerless test editor could not pass JUCE's native hit test.

## Isolated compiler comparison

An untouched `git archive 16735ee src tests` was extracted to a fresh temporary
directory. GCC 14.2.0 (Debian 14.2.0-19), x86-64 Linux, official JUCE 8.0.11,
compiled `ui_snapshot.cpp` with `-std=c++20 -O2 -fstack-usage -c`. The second
compile replaced **only** `KeyboardPressureOwnership.h` with the eight-slot
implementation. No continuity code, test removal, stack-size flag or compiler
optimization change entered the comparison.

| Observation (bytes) | 16 × 128 current-owner table | Eight current-owner slots |
|---|---:|---:|
| `sizeof(KeyboardPressureOwnership)` | 65,792 | 512 |
| `sizeof(Cs80AudioProcessor)` | 113,952 | 48,672 |
| GCC frame: `main` (dynamic, bounded) | 459,248 | 198,128 |
| GCC frame: `testPerformanceControlsHost` (static) | 911,968 | 389,728 |
| GCC frame: `testExternalInputHost` (static) | 570,432 | 244,032 |
| GCC frame: `testSourceFactoryHost` (static) | 344,096 | 148,256 |

`main` calls `testPerformanceControlsHost`; their reported caller-plus-callee
frame estimates are 1,371,216 versus 587,856 bytes, before deeper calls. The
former exceeds 1 MiB. These Linux compiler observations substantiate excessive
stack demand but must not be presented as Windows/MSVC measurements or a
confirmed runtime repair. Native Windows retry remains the acceptance check.

Baseline header SHA-256:
`6583c32f8c52e918e981406fece17cc27fcdcc3a97e837ffd050f654dce63c0f`.
Eight-slot header SHA-256:
`5d38bfbac44283164ca84d97bf4e2a3bf33a525c950d7624df9b9a75cb7e6bed`.
Original local `.su` reports, objects and compiler logs were retained in
`/tmp/cs80-pressure-stack-NXZjbZ` for this diagnostic.

## Reproduction

Set `JUCE_MODULES` to the official JUCE 8.0.11 modules directory, run from the
repository root, and keep the generated temporary fixture until inspected:

```sh
fixture=$(mktemp -d /tmp/cs80-pressure-stack-XXXXXX)
git archive 16735ee src tests | tar -x -C "$fixture"
flags='-std=c++20 -O2 -fstack-usage -c
 -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1
 -DJUCE_WEB_BROWSER=0 -DJUCE_USE_CURL=0 -DJUCE_MODAL_LOOPS_PERMITTED=1'
g++ $flags -I"$JUCE_MODULES" -I"$fixture/src" \
  "$fixture/src/tools/ui_snapshot.cpp" -o "$fixture/ui-before.o"
cp src/plugin/KeyboardPressureOwnership.h "$fixture/src/plugin/KeyboardPressureOwnership.h"
g++ $flags -I"$JUCE_MODULES" -I"$fixture/src" \
  "$fixture/src/tools/ui_snapshot.cpp" -o "$fixture/ui-after.o"
```

`sizeof` was independently read from ELF symbol sizes of arrays declared as
`char processor_size[sizeof(Cs80AudioProcessor)]` and
`char pressure_owner_size[sizeof(cs80::KeyboardPressureOwnership)]`, compiled
with the same headers/defines and inspected using `nm -S`.

## Invariants retained

At most eight physical circuits can own pressure. The current gesture and its
origin are now parallel fixed eight-slot arrays; release/restrike and host-pitch
invalidation scan at most eight entries. Host slot reuse clears the old current
token even when the incoming token is invalid. Exact token matching, refused
onset behavior and active host-alias exclusion remain unchanged. A compile-time
1,024-byte maximum prevents silently reintroducing a full MIDI-key ownership
matrix. No audio allocation, lock, retry loop or stack-limit increase is added.
