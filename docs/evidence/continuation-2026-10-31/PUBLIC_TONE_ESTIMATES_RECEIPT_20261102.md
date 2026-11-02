# Explicit public tone estimates — bounded implementation receipt

2026-11-02. **Non-default engineering estimates, not installed-part facts
or measured hardware fidelity.** The 25 excluded source cells remain
excluded. The ordinary engine path, immutable V1 policy, Legacy IDs and
manufacturer/audio acceptance limits are unchanged.

## Frozen selection and independent challenge

The [preregistration](PUBLIC_TONE_ESTIMATES_PREREG_20261102.md) and literal
tests were committed before evaluation (`48d939f`); reviewer-requested
bit-exact complete-tone controls and correlated topology arms followed
in `95f13b3`, before their evaluation. Implementation is `c6e969e` and the
isolated mutation runner `6e86208`. These are **local commit identities**;
publication via the GitHub connector can recreate metadata and assign
different commit IDs. Do not assume they are remotely addressable.

The initial old-median red arm retained
[20,806 checks / 2,610 expected failures](public-tone-estimates-20261102/before.log).
It predates the exact-control/correlated extension, so it is not claimed
to be the same test revision as the larger green suite.
The extended candidate passed
[42,877 checks](public-tone-estimates-20261102/after.log), reporting all
individual alternatives and **66 correlated arms**: all 22 tones under
old median, full selected policy, and all disputed direct branches open.
No arm was selected from its audio or amplitude result.

At clean local `fc559e6f075dcd3c8b3daab0c7de1afca357e7f0`, tree
`7355e704f86e0aec03c32dd6f8d3c429038d763c`, GCC 14.2.0 successfully compiled
all **25 one-cell mutants** and the **old-median API mutant**. Every mutant
failed independent literal-value assertions (nine per one-cell mutant;
153 for the old-median replacement). Compile failures were not accepted
as detection. Self-fingerprint metadata was pinned to prevent a
self-hash assertion from substituting for the independent literal oracle.
Positive-before and restored-after both passed 42,877 checks and produced
the exact same complete-stdout hash as the retained extended green log.

The [mutation manifest](public-tone-estimates-20261102/mutations/manifest.json)
records actual source/test hashes, exit codes and per-mutant output
identities. Per-mutant logs retain stderr and terminal summary, **not
complete stdout**; the complete stdout hash/byte count is recorded.

## Preserved source/V1 regressions

The same clean source was built Release with CMake 4.4.4 / Ninja under
the shared heavy-check lock. [Raw locked run](public-tone-estimates-20261102/frozen-regression.log):

| Suite | Checks | Result |
| --- | ---: | --- |
| source factory integration | 135,739 | 0 failures, 345.24 s |
| source factory nominal controls | 373,977 | 0 failures |
| explicit public estimate | 42,877 | 0 failures |

All three CTest suites passed in 345.43 s. The compilation and mutation
snapshot preceded subsequent test-only metadata/CP2 preregistrations;
the raw log's original source identity is preserved. This receipt does
not assert those later negative tests passed yet.

## Remaining adoption conditions

- The independently requested **policy-2 exact override-mask** negative
  cases subsequently passed red/green/guard-deletion/restored checks;
  [their separate receipt](READOUT_GUARDS_RECEIPT_20261102.md) must accompany
  any adoption of this API. They do not alter the historical run above.
- The **original 885-row audio inventory** and unchanged click thresholds,
  alias/FX/latency, state/host and CPU contracts still govern any default
  switch. No default switch is included here.
- Twelve conducting-direct choices remain low-confidence topology
  estimates with their all-open alternative retained. Public scans do
  not establish installed serial-specific parts.
- The HP CP3 discrepancy remains a separate public-constraint miss;
  changing tone assumptions does not close it.
