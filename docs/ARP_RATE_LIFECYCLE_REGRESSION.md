# Held-chord ARP rate-edit lifecycle

Status: independently source-reviewed, then built and run once with scoped
Release/Ninja CMake and CTest on Linux, 2026-11-13. **1/1 test and 15/15
assertions passed**, exit 0; all three complete transition lists matched with
maximum frame error 0. See [full output](experiments/arp-rate-lifecycle/test.log),
[identities and commands](experiments/arp-rate-lifecycle/result.json), and
[hash-frozen pre-run plan](experiments/arp-rate-lifecycle/pre-run-plan.md).
Base `c0b17dea57b14436418337a93f55df8176993475`. No production changes.

## Existing policy, not a musical timing redesign

`Engine::updateArpeggiator` accumulates elapsed seconds in a float clock. Rate
edits change the interval on the next internal sample without clearing elapsed
time or resetting the chord index. An open gate retains its previous remaining
duration until it expires or a subsequent note replaces it. Thus an overdue
increase may legitimately trigger on the next internal sample: there is no
blanket promise of no retrigger at an edit. Mode/octave changes are different
structural edits and are not exercised here.

This bounded regression uses only valid direct Engine parameter/note calls:
Legacy policy explicitly selected (`sourcePerformance=0`, `parityMode=0`),
48 kHz host rate, Eco fixed 2x/96 kHz internal rate, scanner disabled, ascending
one-octave held triad 60/64/67 on channel 1 at velocity 0.7. Dispersion, drift and
analog noise are disabled. Each of three fresh instances renders 28,800 host
frames (0.6 seconds), one frame at a time for public voice inspection:

| Case | Rate edit | Edit before host frame |
| --- | --- | --- |
| Decrease | 16 to 8 Hz | 1000 |
| Early increase | 8 to 16 Hz | 1000 |
| Overdue increase | 8 to 16 Hz | 4000 |

All rates are supported values. The early edits precede both the existing gate
expiry and the new step deadline. At frame 4000, the accumulated old interval
already exceeds the new 1/16-second interval, so the next note is due immediately.
After edits, the next note must continue chord order rather than restart at 60.

## Numerical oracle and fixed acceptance

A small separate float-clock scheduler generates expected held-note transitions
without calling Engine scheduling helpers or reading private state. It includes
two internal ticks per host frame, the retained elapsed phase, existing 0.58 gate
fraction and the skipped-whole-interval rule. This deliberately models existing
source-clock arithmetic; it is not an external MIDI-clock measurement, an
independent mathematical accuracy standard, or validation of a new musical policy.

The fixture samples only `getVoiceInfo()` after each host frame and compares the
complete ordered transition list, including gaps (`note=-1`). Timing tolerance
is predeclared at **at most two host frames**; list lengths and note values must
match exactly. Comparing all transitions checks both old gate lifetime and new
step timing, not just the first note. Float clocks avoid mistaking accumulated
roundoff for lifecycle errors. There is no adaptive tolerance or alignment.

Five assertions per case, 15 total:
- Public oversampling/rate getters confirm 2x/96 kHz.
- All rendered audio is finite and at most one generated voice is held.
- Complete transition list matches the oracle within two host frames.
- Only the overdue edit produces note 64 within two frames of the edit.
- The initial note is 60 within two host frames of startup.

No scanner, malformed data, state parser, event queue, plugin host, GUI, broad
TuningMidiTests/CoreTests, CPU benchmark or external clock is exercised. This
covers three fixed Legacy histories, not all rates, models, policies, channels,
mode changes, MIDI clock synchronization or arbitrary parameter automation.

## Bounded integration after review

The new CMake/CTest target is `cs80_arp_rate_lifecycle_tests`, timeout 60 seconds.
It links `cs80_core`, so building this target also compiles its three production
translation units; it does not run the core test suite. In a fresh build tree,
configure with plugin OFF and tests ON, build only this target, confirm the JSON
dry list contains exactly this test using regex
`^cs80_arp_rate_lifecycle_tests$`, then run only that anchored selection once.
The four recorded stages exited zero and all frozen source identities remained
unchanged. The JSON dry list contained exactly this test; CTest added no fixture
prerequisites. Only the dedicated executable and core library were built.
The retained build log records actual compilation; other targets present in
CMake configuration were not built or run. Historical evidence is untouched.
