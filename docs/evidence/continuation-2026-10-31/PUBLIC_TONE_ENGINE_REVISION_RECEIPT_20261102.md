# Opt-in public-tone Engine revision: bounded implementation receipt

2026-11-02. **Implemented and locally tested; not a production-default switch,
full-instrument qualification, CPU result or physical measurement.** The original
25-cell uncertainty, alternative policies and Yamaha acceptance limits remain.

## Exact source and integration boundary

- Tested local implementation: `5c1ba2b8577bbcacc32e23f1c52267708887c0ec`.
- Tested tree: `b8c0c26aea0818546ab1843fc5cd9b3e2b30402f`.
- Tested `src` tree: `4ffb2fe36447fe835d0556102d42cd11cbfe6e5e`.
- Prerequisite tip / pre-Engine-increment baseline:
  `ce307860fc64a0c151991983f6157463d2fd707c`, tree
  `561f6bf37b8dcc46c54b5a35e328c08f0cbec399`.
- Earlier base: `058224fad844b8f51e211e3b820c0ae2d8f25762`.

These are **local historical Git identities**, not claims that those commit IDs
are published GitHub refs. Publication may reauthor metadata; root must record
the local-to-remote mapping and verify equal trees. The [source identity file](public-tone-engine-v2-20261102/source-identity.json)
pins every changed file, all 19 prerequisite commits and all 11 subsequent
implementation/test commits in chronological order. This receipt only adds
documentation/evidence after the tested source; its own Git commit is separate.

The complete prerequisite range is `058224fa..ce307860`; the bounded Engine
increment is `ce307860..5c1ba2b`. The separate filter-research range
`ce307860..50db90a1298f742fc9f4be69291a07119286fdc4` is **not** an Engine dependency.
The source base predates PR6's click-bank causal tests and WSC oracle fixtures.
Integrate incremental commits onto the PR6-preserving head: do not replace that
head with this older branch's whole tree. A clean local bounded test does not
establish the integrated head's compatibility.

## Implemented behavior

`ControlFrame::factoryToneRevision` defaults to `frozenV1`. The explicit
`publicAmendedV2` revision routes active factory ranks through the already
registered `compileTonePublicEstimate`; it does not change
`source_factory::modelVersion`, any factory ID, parameter default/range,
plugin state, programmer controls, Legacy synthesis or oscillator/filter/VCA law.
Unknown revisions reject before mutation. Per-rank compiled revision is part
of cache identity, including inactive ranks and Legacy excursions.

`cs80_engine_audit --factory-tone-revision public-amended-v2` exercises the real
Engine path and writes `candidate-policy.json` with revision 2, policy ID
`public-amended-marks-v2`, hash `832eee6f44f5cfcc`, and explicit false values for
default adoption, qualification and physical measurement. Buffered write and
close failures stop the suite. Candidate CPU runs reject before output creation;
the original CPU workload cannot silently change tone policy. The absent flag
or explicit `frozen-v1` retains the existing path.

## Test-first observations, including failures

Raw observations are preserved in [the evidence directory](public-tone-engine-v2-20261102/)
with a [SHA-256 inventory](public-tone-engine-v2-20261102/SHA256SUMS). Failed logs
are not rewritten to later source IDs or represented as successful runs.

| Observation | Outcome |
| --- | --- |
| New Engine contract against real V1-only execution | 14,931 checks, **232 ordinary failures** |
| Engine routing/cache/validation implementation | 14,931 checks, zero failures |
| First audit-test compilation | Test-helper name collided with `cs80::reference`; **build failure**, not semantic RED |
| Audit helper repaired, old CLI still tested | 397 checks, **12 ordinary failures** |
| Actual CLI propagation implemented | Both focused CTests pass |
| Signed-zero oracle, expanded programmer returns and failing stream sync added first | Engine 15,116 checks / **1 failure**; audit 400 checks / **2 failures** |
| Bitwise comparisons and explicit flush/close implemented | Both focused CTests pass; the final frozen run passes them again |

PCM equality now compares each float's `uint32_t` representation, with +0/-0
negative controls. Dynamic snapshots compare the listed **public semantic
readouts** in all 16 lanes, not every private authoritative filter memory or
precise ADSR field. Tests cover all 22 factory selections, three rates and both
source policies; all 222 Legacy programs; the seven complete tones; an affected
Brass audio difference; V1/V2, Legacy, inactive-rank, MEMORY1/MEMORY2/PANEL and
return-to-factory transitions; reset/prepare/setParams; and invalid transactions.
An intentional affected-tone audio difference is not an audio-quality PASS.

### Actual-source mutation controls

All listed mutants compile and fail ordinary semantic assertions; no compiler
failure counts as a killed mutant. Positive/restored runs pass and original
source hashes remain unchanged.

| Actual mutation | Ordinary failures |
| --- | ---: |
| Always compile V1, ignoring explicit V2 | 208 |
| Remove per-rank revision cache key | 15 |
| Remove revision validation | 18 |
| Publish requested revision globally to inactive caches | 8 |
| Remove actual audit render propagation | 2 |
| Remove actual sidecar flush | 1 |

The first five controls used the earlier 14,931/397-check suites. The Engine
production source is unchanged since that batch. After review hardening, the
400-check audit suite independently reran propagation and flush mutants and a
restored positive. Both batches retain exact source/compiler/binary/log hashes.

### Separate pristine-relative-to-increment replay

The exact revision-free fixture was compiled against both the pinned ce307860
tree and candidate source. This baseline already contains the opt-in estimate
API; **it is pristine only relative to this Engine revision increment, not
pristine original PR6**. There are 798 cases: 22 Source selections × 2 source
policies × 3 rates at 256 frames, plus 222 Legacy programs × 3 rates at 64 frames.
Each complete serialized result is 627,288 bytes. Baseline and candidate are
byte-identical:

`43df8f5cfd923da8d8b470a03db7a0a170b1025eb435aff2f66111c5e113a9f2`

A real candidate-header default-to-V2 mutant still renders successfully but
changes the bytes to:

`d49ae5dbf5fa8361c2e7c182c6827118aedcbbae9568456892657971b5b235c0`

The [replay receipt](public-tone-engine-v2-20261102/pristine-replay/receipt.json)
pins source, fixture, compiler, binaries and outputs. Raw PCM and staged
binaries remain in the local evidence workspace; they are not packaged as
source files here. The fixture and complete hashes permit exact reproduction.

Metadata nuance: the outer replay log starts at `22d06754`; the runner captures
HEAD at completion, after the audit-only mutation runner/documentation commit
`5c1ba2b`. No `src` or replay-fixture byte changed during the run; all staged
source and final original-file checks pass. Preserve both observed IDs rather
than silently assigning a different run identity.

## Frozen original-path regressions

The seven selected CTests passed at clean `5c1ba2b`, unchanged before/after:
source-factory integration, source-factory tones, source boundary, public-tone
estimates, audit revision, Engine revision and MEMORY/PANEL. Total 441.20 s;
integration alone 419.88 s. The original core executable was built in that
command, but its registered CTest name is `cs80_core_tests`, not `cs80_tests`,
so it was run explicitly afterward: **1/1 passed, 19.75 s**.

The first explicit-core harness attempt lacked the shared official-tool PATH
and exited 127 before CTest. Its failed log is retained alongside the corrected
successful run. This infrastructure failure is not counted as a semantic RED.
All multi-second runs held the shared heavy-check lock; these timings are test
durations, not a CPU benchmark or speedup claim.

Reproduction uses the existing configured core-only build and official tools:

```sh
export PYTHONPATH=/workspace/scratch/aa102d573ba3/cs80-audio-tools
export PATH=/workspace/scratch/aa102d573ba3/cs80-audio-tools/bin:$PATH
# Hold cs80-heavy-check.lock around builds, tests and each Python runner.
ctest --test-dir build-v2 -R \
 '^(cs80_core_tests|cs80_source_factory_(integration|tone|boundary)_tests|cs80_public_tone_estimate_tests|cs80_memory_panel_tests|cs80_public_tone_(audit|engine)_revision_tests)$' \
 --output-on-failure
python tests/circuits/check_public_tone_engine_mutations.py --out NEW_MUTANT_DIR
python tests/circuits/check_public_tone_default_replay.py \
 --baseline-revision ce307860fc64a0c151991983f6157463d2fd707c --out NEW_REPLAY_DIR
```

## Still required before any adoption

Independent final review, PR6-preserving incremental integration and its normal
regressions remain separate actions. Then register a distinct immutable
candidate source/command for the complete original 885-case inventory, rates,
reporter and numerical limits. Keep original V1 qualification and the original
registered CPU fixture unchanged. The existing original audit gate cannot be
repinned to make this candidate masquerade as that baseline. Plugin exposure,
saved-state policy and any production-default switch require later explicit
review. HP CP3 remains a public-constraint miss; this tone revision does not
claim to solve it or to reproduce measured hardware.
