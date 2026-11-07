# Poly MSEG retrigger rate isolation

## Defect and scope

At baseline `a53c1a8b196d494dedd9f0438be173c1b0ecd28d`, a poly MSEG restarts
when a circuit receives a new note, but its first tick still uses the rate
modulation calculated for the previous note on that circuit. Matrix rate
routes intentionally act one sample later, so the old value remains in
`VoiceState::rate` until the new note's first tick has already happened.

This can change more than one sample. A large old rate can skip a segment,
cross a loop boundary or finish a short one-shot. At 96 kHz internal rate,
three full-depth velocity-to-rate routes sum to nine octaves, clamped to
eight: 256 times the base rate. A new soft note then advances a 2 ms ramp
by 2.667 ms on its very first tick and holds its final value, rather than
starting around 0.0052083 of the ramp as a fresh circuit does.

The same retained rate can survive a poly-to-mono-to-poly trigger-mode
edit made before another rendered sample. The existing explicit restart
then restarts the runner without clearing its delayed rate.

## Change

Clear only the restarted poly envelope's rate slot beside each actual
`trigger()` call. Its first tick now uses the base rate, matching a fresh
voice. Rate routes calculated from the new note take effect on the next
sample, preserving the existing one-sample feedback convention, including
self-modulation. This does not move matrix evaluation ahead of the runners.

Shared mono/free runners and their global rates are unchanged. A selected
trigger-mode restart leaves other MSEGs on the same voice alone. Ordinary
held notes retain rate modulation; other voices retain their own history.
No parameter, state format, allocation, lock or per-sample transcendental
calculation is added.

This is a modern modulation correctness change, not a circuit or measured
hardware-fidelity claim. Affected routed performances intentionally change
in both Legacy and Source Performance policies. Default/no-route playback
and PURE CS-80 remain unaffected. It is independent of the dormant mono
onset proposal in PR #71 and of the open effect-cache proposals.

## Verification

`tests/MsegRetriggerRateTests.cpp` contains 1,442 checks:

- The explicit 2 ms ramp reproduction.
- Recycled versus fresh trajectories for all three MSEGs at six internal
  rates, 16 kHz through 1.536 MHz, with all loop modes, free/synced time,
  positive/negative direct/via rate routes and base-rate endpoints.
- Held-rate arithmetic, release, same-boundary amount/routing edits,
  explicit trigger-mode restarts, cross-voice and same-voice sibling
  isolation, and positive/negative self-feedback.
- Public `Engine` cyclic reuse of release tails at 44.1, 48, 96 and 192 kHz
  host rates, in Legacy, Source Performance and PURE policies. The real
  allocator refuses a ninth held key; no all-held voice-stealing behavior
  is assumed.

The final suite fails **1,338 of 1,442 checks** against the unmodified main
source and passes **1,442 of 1,442** after this change. GCC 14.2 Release and
AddressSanitizer/UndefinedBehaviorSanitizer with standard-library assertions
pass; leak detection is disabled for the sanitizer run. An independent
review also rebuilt and passed the final suite with warnings enabled,
repeated existing MSEG/transport tests, and found no blocking issue.

The full JUCE-free target build passes. Strict compatibility remains
byte-identical for pinned Legacy default/tail audio (`d56911a`), V1 circuit
numerics (`22d6a32`) and V2 circuit audio (`00ceccf`). These pinned fixtures
do not cover every historical routed MIDI performance.

No plugin, GUI, DAW, native ARM or physical-hardware qualification is
claimed. No validator source/configuration or workflows were changed, and
no existing validation discrepancy is resolved or waived by these tests.

## Reproduction

```sh
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --parallel 2
ctest --test-dir build-core --output-on-failure \
  -R '^cs80_(mseg(_transport_rate|_retrigger_rate)?|mod_matrix)_tests$'
scripts/check-compat-gates.sh --strict-v2

c++ -std=c++20 -O1 -g1 -fno-omit-frame-pointer \
  -fsanitize=address,undefined -D_GLIBCXX_ASSERTIONS -Isrc \
  tests/MsegRetriggerRateTests.cpp src/core/Engine.cpp -o /tmp/mseg-retrigger
ASAN_OPTIONS=detect_leaks=0 /tmp/mseg-retrigger
```

To confirm the pre-fix failure using the current regression source:

```sh
mkdir -p build-before
git archive a53c1a8b196d494dedd9f0438be173c1b0ecd28d \
  src/core src/reference src/experimental | tar -x -C build-before
c++ -std=c++20 -O2 -Ibuild-before/src tests/MsegRetriggerRateTests.cpp \
  build-before/src/core/Engine.cpp -o build-before/mseg-retrigger
./build-before/mseg-retrigger
```

Keep the proposal in draft until the required checks pass for its exact
published revision. Local core evidence is not a release-ready claim.
