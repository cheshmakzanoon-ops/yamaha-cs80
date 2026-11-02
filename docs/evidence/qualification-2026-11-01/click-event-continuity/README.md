# Click event continuation: exact gate and causal audit

2026-11-01. Software-model diagnostics only. The original physical-PANEL Bass
48/96 kHz and paired Funky note-off <= 1 score criteria remain **OPEN**.
See [the complete report](../../../CLICK_EVENT_CONTINUITY.md) for the numerical
results, source pages, declared inferences and remaining uncertainty.

## Provenance and acceptance chronology

- Audio/source baseline: `d06648c386486cf2630de6e41195ddd5d4360ff0`, whose
  circuit audio pin is `18400c29a54ac2204564b355c0e52ffd3b0dcb8e`.
- Exact gate implementation: `3459978757fc755a25e57e62b63a5810e502ef17`.
  It adds an offline test, an explicit scalar comparator, a CTest target and
  a friend declaration with no production implementation or layout change.
- Independent initial registration:
  `98c8b738b9fb6f96f7006b4f2cb032176c285c66`.
- Independent nonzero-charge fixture amendment:
  `d06648c386486cf2630de6e41195ddd5d4360ff0`.
- Evaluated contract SHA-256:
  `736b49076a592a026b1a05be3a0c3daac277a0bebde29023914faeb0c46030a8`.
  The runner compares its bytes with the registered Git blob before execution.

The implementation compares 72 explicitly named exposed fields per rank,
including all 48 antialias residual entries. Every permitted mismatch count
is zero. Both policies, the original 1.40-second release, the independently
registered 0.22-second release and both active/cold Bass assignments remain
present. The original phrase score definitions and limits are unchanged.

## Full qualification and targeted single-entry control

[`mutations/qualification.json`](mutations/qualification.json) records the
authoritative full source-mutation batch. Its unmutated and restored full
runs each pass **7,438 assertions, zero failures**, using GCC 13.3, C++20,
`-O2 -DNDEBUG -Wall -Wextra -Wpedantic`. All 14 source mutations in that batch
fail their intended assertions. The two idle defects each also fail
**2,112 / 2,121** assertions in the unchanged original cold-clock test.

An additional narrow mutation clears **exactly one occupied residual entry
across both ranks**, supplementing the first batch's one-entry-per-populated-
oscillator mutation. Its
[`single-residual-mutation/qualification.json`](single-residual-mutation/qualification.json)
records the same C++ driver, comparator and production-source hashes. The
unmutated/restored sounding-tail controls each pass 224 assertions; the
single-field defect fails six assertions, one for every rate/policy fixture.
No gate, threshold or C++ test changed between batches. In total, **15 actual
source defects** are rejected. Complete compiler outputs, assertion outputs
and patches are retained beside each qualification record.

The main batch executed the archived
[`mutations/runner-initial.py`](mutations/runner-initial.py). The production
`tests/click/run-continuation-mutations.py` adds the single-entry control and
an optional named-case selector; its default still executes every mutation,
all full baseline/restored fixtures and both test-strength controls. A
selected case executes its matching unmutated and restored fixtures. The
second qualification record retains that final runner's SHA-256.

Two deliberate defects in test strength are also audited:

1. Raising a zero-bit contract limit causes the executable to reject the
   contract even though unmutated source continues exactly.
2. Deleting only the rank-II release comparison lets the actual rank-II
   coupling reset survive: the deliberately defective test reports 1,502
   checks and zero failures. The auditor therefore marks its registration
   **rejected**. This green negative control is evidence of an inadequate
   test, not an accepted DSP result. The proper full test is subsequently
   restored and passes 7,438 checks.

All production files retain their original hashes after qualification.
Mutation builds use a full consistent source/header snapshot, never a new
probe linked against an Engine object with a different layout. A failed
compile/link cannot count as a mutation rejection. Wall-clock test durations
were collected on a shared executor and are not CPU benchmark results.

## Causal audit and retained historical records

[`click-causality.csv`](click-causality.csv) is the fresh three-rate audit of
the original named SOURCE PERFORMANCE fixtures, with the original phrase,
64-frame rounding and 8 ms windows. Every omitted-event control has an exactly
equal pre-event PCM prefix. The electrical-programmer Bass rows remain
separate from original physical PANEL; the latter still fails at 48/96 kHz.
[`click-causality-run.log`](click-causality-run.log) preserves the command and
successful exit. The report compares those rows with the earlier committed
`db24a47` baseline, without describing the new regression as an audio fix.

[`preregistration-original-red.log`](preregistration-original-red.log) and
[`PREREGISTRATION_CORRECTION.md`](PREREGISTRATION_CORRECTION.md) are copied
verbatim from the earlier source-only investigation. The explanation retains
its historical draft label; the amendment was subsequently committed as
`d06648c` before the current evaluation. The original red log has 5,817
checks, nine failures, all the impossible nonzero-filter-charge precondition
at the 1.40-second event. Its SHA-256 is independently checked against the
committed contract's recorded hash:
`67b5f67d2f59840816a828c36a5edb37798274d8a243ea1e9337e7dff6eaa959`.
It has not been relabelled as passing or replaced with the additional event.

[`initial-release-before-suite-selector.log`](initial-release-before-suite-selector.log)
retains the first CMake Release run, 7,438 checks and zero failures. It
predates only the optional CLI fixture selector used by the isolated mutation
runner. The full later qualification records with exact source hashes are
the authority for implementation commit `3459978`; the early construction
run is retained for provenance, not represented as a separate native CI run.

`manifest.json` records the SHA-256 and byte length of every retained evidence
file except the manifest itself. Native CI and whole-repository compatibility
qualification are separate from these focused local runs.
