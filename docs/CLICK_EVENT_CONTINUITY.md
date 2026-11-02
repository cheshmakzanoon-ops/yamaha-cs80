# Exact event continuation and the remaining click scores

2026-11-01. These are deterministic software-state and waveform diagnostics,
not hardware measurements. The original phrase retrigger and note-off
requirements remain **score <= 1 at every required rate**. The new regression
does not replace either requirement or change any audio equation, preset,
window, denominator, attack time, oscillator seed, or output gain.

## What the new regression protects

The independent acceptance commits are
`98c8b738b9fb6f96f7006b4f2cb032176c285c66` and the non-vacuity amendment
`d06648c386486cf2630de6e41195ddd5d4360ff0`. Their
[registered contract](../tests/parity/tolerances-v2/click-residual-continuity.json)
precedes this implementation. Every exposed-state and PCM mismatch limit is
exactly zero; the executable also rejects a contract that raises a zero-bit
limit. `ClickEventContinuationTests.cpp` compares named scalar fields with
IEEE-754 bit casts, including signed zero, rather than comparing object padding.

| Exposed state per rank | Fields | Acceptance at zero elapsed time |
| --- | ---: | --- |
| WSC charge, reset time, two baselines, held controls, elapsed time, waveform gates, residual cursor/count, and all 3 x 16 antialias residuals | 59 | Identical bits |
| HP and LP input, feedback, BP and LP nodal histories | 8 | Identical bits |
| Amplitude and filter envelope charge proxies | 2 | Identical bits |
| VCA linear-control node and output-coupling state | 2 | Identical bits |
| Engine oscillator phase proxy | 1 | Identical bits |
| **Total** | **72** | **Zero mismatches** |

Gate enums, selected key/touch controls and new attack/release coefficients
may change at a note event. They are the purpose of that event. Derived
oscillator search caches and unexposed internal variables are outside this
field manifest; the test makes no claim that it observes every private state.

The fixtures retain the source phrase and independently exercise its difficult
boundaries, under both SOURCE PERFORMANCE and PURE CS-80:

- **A, original Funky release:** the actual rank-I Funky 1 and rank-II Funky 2
  source cards, onset at 0.20 seconds and note-off at 1.40 seconds, quantized
  upward to the original 64-frame boundaries. Both ranks retain all 72 fields
  at note-off at 44.1/48/96 kHz and Eco/Standard/HQ. The amplitude and filter
  gates must enter Release. The omitted-event replay has exactly identical
  pre-event PCM, and the rank-II VCA coupling state must already be nonzero.
- **A2, additional early release:** the same cards and onset, with a separate
  note-off at 0.22 seconds. All four envelope charge proxies must be nonzero
  before release, making individual amplitude/filter reset mutations
  observable. The original 1.40-second fixture remains present.
- **B, sounding Bass tail:** physical-unit PANEL Bass Stack fills all eight
  circuits. The allocator reassigns an active, released circuit before
  retirement, with no elapsed rendering or pending idle time between snapshots.
  A genuine nonzero oscillator residual must be present before reassignment.
- **C, original inactive Bass assignment:** the original physical-PANEL phrase
  through the 6.25-second note-on at 48/96 kHz. The live allocator must select
  the historical inactive voice 6. A separately replayed reference engine
  completes the pending idle time; its post-wake state is compared with the
  ordinarily assigned engine at the same time. Both are checked against
  independently counted rendered samples, using the unchanged `1e-8 s`
  [cold-time bound](../tests/parity/tolerances-v2/cold-idle-timing.json).
  A third, unobserved event engine must produce identical post-event PCM.
- **D, WSC control-change cache oracle:** at internal 96/192 kHz, an oscillator
  must have an actual cached future branch and nonzero antialias residual.
  Its exposed state is restored into a separately prepared oscillator, which
  invalidates the derived cache. The next 128 outputs and continuation states
  must agree exactly after a frequency change, a threshold change, or both.
  This independently compares cache strategies under the same software law;
  it is not an independent analog oscillator model.

The test also compares each engine's compiled Funky bus voltages with an
independently named source-card lookup. This detects accidentally compiling
rank-I Funky 1 for rank II even though both hardware selectors have index 9.

### Why inactive wake is a separate observation

At the current qualified 1.5 kHz idle cadence, voice 6 has **47 pending internal
samples at 48 kHz** and **95 at 96 kHz** in this fixture. The test reads these
counts from the engine; it does not require historical 7/15-sample values from
an earlier cadence. A stale pre-wake state and the post-assignment state refer
to different elapsed times, so their difference is not evidence of a reset.

`EngineEventContinuityProbe` is a friend declaration with its implementation
entirely in the test. It only reads allocator/counter state and invokes the
existing wake on the independent reference engine. The event engine receives
its ordinary `noteOn`; the third engine establishes that observations do not
change a single output PCM bit. No production wake, allocator, render path,
object layout, or timing coefficient is changed.

### The preregistration correction remains visible

The original contract assumed that both Funky filter-envelope proxies would
still be nonzero at the 1.40-second release. Its first run retained **5,817
checks and nine failures**. All nine were that fixture precondition: after a
1.2-second hold, the registered source model has already settled the filter
envelope to zero. The exact continuity comparisons did not fail.

The separately committed amendment retains that complete original event and
adds the 0.22-second event for nonzero-charge mutation sensitivity. No numeric
threshold was relaxed. The original red log and its source-law explanation
are retained with the qualification evidence; its SHA-256 remains
`67b5f67d2f59840816a828c36a5edb37798274d8a243ea1e9337e7dff6eaa959`.

## Qualification result

The published implementation is `6b01a8b8b142e2a66424aefa131d373504f99762`.
Its gate source is identical to local qualification commit
`3459978757fc755a25e57e62b63a5810e502ef17`, whose identity and source hashes
remain unmodified in the archived raw evidence. That implementation passes
**7,438 assertions with zero failures** in both the unmutated and restored
full GCC 13.3 / C++20 / `-O2` runs. Both source policies, every registered
event fixture, the 72-field comparisons and exact PCM controls are present.
The complete source hashes, command lines, full failure logs and actual-source
patches are in the [qualification bundle](evidence/qualification-2026-11-01/click-event-continuity/README.md).

All **15 source mutations** are rejected at their intended assertion:

| Deliberate source defect | Failing assertions |
| --- | ---: |
| Reset active-tail WSC | 24 |
| Keep stale branch cache after frequency change | 345 |
| Keep stale branch cache after threshold change | 61 |
| Clear one occupied residual per populated oscillator | 10 |
| Clear exactly one occupied residual across both ranks | 6 |
| Reset rank-II HP history / LP history | 6 / 6 |
| Reset rank-I amplitude / filter envelope | 72 / 54 |
| Reset rank-II amplitude / filter envelope | 72 / 54 |
| Clear only rank-II VCA coupling | 36 |
| Skip pending idle wake / double pending idle time | 32 / 16 |
| Compile rank-I Funky 1 for rank II | 72 |

The skip/double-time mutants independently fail **2,112 of the existing 2,121
cold-clock assertions**, with its original time and phase limits. The extra
single-entry residual mutant has unmutated/restored sounding-tail controls of
224 assertions, zero failures, around its six failing cases.

Two test-strength controls also pass their audit. Raising a zero-bit contract
limit is rejected even with unmutated production code. Removing the rank-II
release comparison lets the already-proven rank-II VCA reset survive
(1,502 assertions, zero failures in that deliberately defective test), so
the mutation auditor explicitly rejects that test registration. That green
negative control is retained as evidence of an inadequate test, never as a
successful production qualification. The correct full test is restored and
again passes 7,438 assertions.

## Current score evidence, with original controls retained

The new causal audit is at source baseline `d06648c`; its audio is still the
previously published `18400c2` checkpoint. The following compares the named
fixtures with the archived original-main `db24a47` audit. These are effects of
the intervening already-published DSP changes, not an audio change made by
this regression. These score rows use the audit's SOURCE PERFORMANCE policy;
PURE CS-80 is additionally covered by the exact-state gate. They are named
fixture comparisons, not a new full-bank worst-case scorecard.

| Host rate | Original Bass retrigger | Current physical-PANEL Bass | Original Funky note-off | Current Funky note-off |
| ---: | ---: | ---: | ---: | ---: |
| 44,100 | 1.510515993 | **0.567655911** | 1.441274461 | **1.626889135** |
| 48,000 | 1.308167504 | **1.440924029** | 1.647965028 | **1.291310019** |
| 96,000 | 1.246673988 | **1.365308570** | 1.290938909 | **1.220958165** |

Original-main provenance is the committed
[baseline CSV](evidence/continuation-2026-10-31/click-causality-baseline.csv)
and [investigation record](evidence/continuation-2026-10-31/DSP_INVESTIGATION.md).
The original 64-frame event quantization, 8 ms post-event windows and reference
windows are unchanged in the [fresh causal audit](evidence/qualification-2026-11-01/click-event-continuity/click-causality.csv).

| Host rate | Bass first strike | Bass second note omitted | Funky note-off omitted | Pre-event PCM difference |
| ---: | ---: | ---: | ---: | ---: |
| 44,100 | 1.050416978 | 0.027021849 | 1.626887639 | Exactly 0 |
| 48,000 | 1.123291366 | 0.031435311 | 1.380503068 | Exactly 0 |
| 96,000 | 1.106078572 | 0.073042998 | 1.238903552 | Exactly 0 |

The original Bass peak remains at host sample 300406 at 48 kHz, 374 samples
after assignment, and at sample 600750 at 96 kHz, 750 samples after assignment:
about 7.79/7.81 ms later. Its first strike already exceeds the same chord
reference. Omitting the second note removes almost all its attack response;
this establishes dependence on the new note, without identifying a reset.

All Funky omitted-note-off scores also exceed one. At 48/96 kHz, omitting the
release increases the score. The release still changes subsequent audio:
the largest derivative of the event-minus-omitted output is approximately
0.010980/0.010769/0.008694 at the three rates. An identical prefix is not a
claim that the later release waveform is identical.

Electrical-programmer Bass is a separate control coordinate, with current
scores 0.294052189/0.269982960/0.385795007. Those passing values do not replace
the original physical-PANEL fixture or close its 48/96 kHz requirement.

## What is proved, and what remains open

The earlier cold filter-envelope replacement and fictional idle elapsed time
were actual software defects, already repaired in `c54c476` and `46a7a5a`.
Their regression and compatibility records remain in
[COLD_FILTER_ENVELOPE.md](COLD_FILTER_ENVELOPE.md) and
[COLD_IDLE_TIMING.md](COLD_IDLE_TIMING.md). The new gate protects exposed WSC
residual/history and both-rank continuation against additional regressions;
it does not identify another production reset in the tested fixtures.

The archived [rank-resolved Bass trace](evidence/continuation-2026-10-31/bass-continuity-3226011/README.md)
attributes the remaining peak primarily to a later filtered oscillator edge,
with much smaller envelope/control/coupling terms and a direct inferred VCA
pedestal contribution around `1e-8` engine units. That decomposition is the
earlier named trace, not a new hardware measurement or a fresh decomposition
of every current program. The fresh audit and exact event snapshots are
consistent with it.

**Still open:** the original physical-PANEL Bass score at 48/96 kHz and the
Funky note-off scores at all three rates. These results do not justify a fade,
pedestal retuning, envelope-time fit, waveform-phase selection, or a relaxed
score threshold. They also do not establish the instrument's exact touch,
envelope-current, stage-comparator, or VCA transient laws. A further tonal
change needs new source evidence or a directly demonstrated numerical defect,
with its own preregistered acceptance and V2 publication sequence.

The source basis is the owner's manual p43 cyclic assignment and M41 p41
trigger/capacitor paths; R39 p39 and the IG00151 guide p52 identify separate
level-control connections. The IG00152 guide PDF p33/printed 2-3 and IG00159
PDF p41/printed 2-7 distinguish selected drive/time paths from capacitor nodes.
They do not specify all internal device laws. The numerical RC, VCA pedestal,
coupling, cold-state and comparator models remain declared inferences with
alternatives in the linked model documents and the registered contract.

## Reproduction

```sh
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --target cs80_click_event_continuation_tests cs80_engine_audit -j 2
ctest --test-dir build-core -R '^cs80_click_event_continuation_tests$' --output-on-failure
python3 tests/click/run-continuation-mutations.py --source . --evidence /tmp/cs80-click-mutations-new
build-core/cs80_engine_audit --suite click-causality --rates 44100,48000,96000 \
  --out /tmp/cs80-click-causality-new
```

The mutation runner compiles a consistent temporary source tree. A compilation
failure is never counted as a rejected DSP mutation. The registered original
zero-bit limits, previous cold-clock bounds and existing phrase criteria are
kept intact.
