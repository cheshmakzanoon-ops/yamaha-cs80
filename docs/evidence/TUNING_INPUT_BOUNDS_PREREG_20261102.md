# KBM input narrowing — preregistration, 2026-11-02

Before implementing the guard, test the current PR6 production parser with
signed 64-bit header values whose low 32 bits equal ordinary valid values.
The external integers must be checked against the already-supported header
ranges **before** narrowing to `int`. Likewise, a mapping degree larger than
`INT_MAX` cannot be silently stored as another degree or an unmapped sentinel.
Any rejection must preserve every member of the caller's existing mapping.

The fixture uses positive and negative `2^32` shifts independently in all five
integer header fields, five overflowing mapping degrees following an otherwise
valid entry, and exact controls at ordinary values and `INT_MAX`. Existing
header bounds and explicit `x` entries remain unchanged. This is an input
representation correction, not a new DSP tolerance or a change to normal tuning.

Scala's [public keyboard mapping specification](https://www.huygens-fokker.org/scala/help.htm#mappings)
defines MIDI-note header fields and scale-degree entries. Its mathematical
degree domain is broader than this implementation's `int` representation;
silently wrapping a large degree is not a defensible approximation. This
focused correction does not claim full unrestricted Scala-domain support.
Existing negative-degree/formal-octave limitations and downstream extreme-Hz
validation are separate follow-up scopes, not hidden by this test.

Required before/after evidence: the focused parser executable, then the original
tuning/MIDI suite; ASan/UBSan coverage where available; unchanged valid fixtures;
exact source and command records. Native cross-platform CI must still run on
the composed reviewed source. Original source-locked audit requests stay frozen.

## Observed before/after, 2026-11-02

The test-only commit `e6b2fc8721db1f1c80f6c2b50aa5300ac0d8eafe` ran against
unchanged PR6 production source: **61 checks, 30 failures**. All ten wrapped
headers and five overflowing degrees were accepted and changed the caller's
mapping. Each case violated both rejection and atomic-state assertions.

Implementation `f1b90724f37f97a645975ab8c43f012d7fddd708`, source tree
`2029aa3bca615f855e00e68247c57d3a8735a661`, validates the original wide header
values against the existing bounds before storing them, and rejects a degree
above `(std::numeric_limits<int>::max)()`. It changes no valid parameter range,
file format, numerical tolerance, audio algorithm or existing fixture.

Under the shared lock with GCC 14.2.0, the focused Release test passed all 61
checks; the AddressSanitizer + UndefinedBehaviorSanitizer replay also passed all
61 with leak detection disabled. LeakSanitizer could not inspect this container
because of its reported ptrace restriction: that failed attempt is retained,
and **no leak qualification is claimed**. The unchanged original tuning/MIDI/
voice-mode suite passed **81 checks**. Its initial manual compile omitted the
CMake-defined fixture directory; that setup failure is retained, then corrected
to the exact existing definition. The suite reused the exact PR6 Engine and
FactoryPresets objects; this is focused parser/behavior evidence, not a fresh
whole-core sanitizer build or native-platform qualification.

Commands (executed under `cs80-heavy-check.lock`):

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Isrc tests/TuningInputBoundaryTests.cpp -o input-test
./input-test
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
  -Isrc tests/TuningInputBoundaryTests.cpp -o input-asan-ubsan
ASAN_OPTIONS=detect_leaks=0 ./input-asan-ubsan
g++ -std=c++20 -O2 -Isrc \
  -DCS80_TUNING_DATA_DIR='"/workspace/scratch/aa102d573ba3/cs80-tuning/tests/data/tuning"' \
  tests/TuningMidiTests.cpp ../cs80-hosts/build-host-core/Engine.o \
  ../cs80-hosts/build-host-core/FactoryPresets.o -o original-tuning-midi
./original-tuning-midi
```

Raw successes and unsuccessful setup attempts are preserved in
`tuning-input-bounds-20261102/`, with SHA-256 checksums. Independent review,
clean published-head checks and native CI remain required before integration.
