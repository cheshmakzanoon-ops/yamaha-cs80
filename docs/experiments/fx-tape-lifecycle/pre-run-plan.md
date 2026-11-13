# Tape-slot stale-history lifecycle regression

Status: source only, awaiting independent review before any compilation/run.
Base: `c0b17dea57b14436418337a93f55df8176993475`.

## Contract and scope

`FxSlot::process` clears the departing live effect when bypassed or when its
identity changes. Classic tape clearing resets both delay lines and damping;
modern tape clearing resets its used-model state, filters, line and modulation.
This must prevent replay of old audio on reactivation. Existing Engine core
coverage checks classic delay off/on, while this fixture directly checks both
tape identities and the effective-mix boundary without Engine or plugin hosts.
No production code or historical evidence changes.

One fixed internal rate, 48 kHz; classic type 3/model 0 and modern type 0/model 4;
active mix 0.75, A/B 0.5, outer gain matching off, classic wow phase fixed zero.
Modern modulation follows its ordinary implementation. The classic nominal delay
is 0.34 s and modern A=0.5 delay is `0.06*sqrt(20)` (approximately 0.2683 s),
with ordinary wow/flutter. Each warm history is a 100 ms stereo tone burst
(L: 0.1 at 733 Hz, R: 0.07 at 997 Hz) followed by 100 ms silence.

For each identity a copied uninterrupted reference runs 700 ms of silence and
must produce a finite delayed tail above `1e-5` peak absolute amplitude. Three
separate copies of the same warmed state then take distinct 50 ms transitions:

1. Mix exactly zero.
2. Mix exactly `0.001f`, the inclusive bypass boundary.
3. Switch to the other tape identity, retaining active mix, then switch back.

All transitions receive ordinary tone input. The first two must return that
input exactly dry, with finite samples. The third excites the alternate effect;
it is not expected to be dry. Reactivated original identity is compared sample
for sample against a freshly prepared slot: 100 ms silence, 100 ms new tone
burst, 800 ms silence. Every stereo float must compare exactly equal and remain
finite. The fresh reference must have delayed output above `1e-5` after the new
burst, which also prevents an always-cleared or permanently bypassed slot from
passing. The initial silence detects stale output from the pre-transition burst;
the remainder checks reset state and a genuinely working reactivated effect.

## Fixed acceptance and limits

24 assertions total: two uninterrupted-tail witnesses plus 11 per identity
(three finite-transition checks, two exact-bypass checks, three exact fresh-state
comparisons, three new-tail witnesses). Tail thresholds and all durations are
predeclared. Exact equality is appropriate for the same effect and controls
starting from cleared state in the same binary. No threshold relaxation or
silent retry after execution is permitted. Public value-copy creates independent
reference histories; there is no private-state access or malformed input.

This does not qualify all modern models, all rates/controls, Engine policy,
host automation, click-free switching, oversampling transitions, gain-matching
switches, hardware fidelity or CPU performance. The other tape is a transition
witness, not a general all-pairs switching matrix.

## Bounded reproduction after source approval

```sh
mkdir build-fx-tape-lifecycle
c++ -std=c++20 -O2 -Isrc tests/FxTapeLifecycleTests.cpp -o build-fx-tape-lifecycle/fx-tape-lifecycle
build-fx-tape-lifecycle/fx-tape-lifecycle
```

CMake/CTest target `cs80_fx_tape_lifecycle_tests` includes only `src`, has no
Engine linkage, and a 60-second timeout. The planned qualification uses a fresh
Release/Ninja CMake directory with CS80_BUILD_PLUGIN=OFF and CS80_BUILD_TESTS=ON,
builds only this target, verifies a JSON dry list containing exactly this test,
and runs CTest with anchored regex `^cs80_fx_tape_lifecycle_tests$`. No default
build or unfiltered CTest is allowed. No execution is claimed yet. Freeze source
hashes and preserve one-shot logs before reporting results.
