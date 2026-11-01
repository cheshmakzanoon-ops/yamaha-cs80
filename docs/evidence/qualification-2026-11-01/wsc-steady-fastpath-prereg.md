# WSC one-segment fast-path experiment — preregistered contract

**Scope:** Experimental branch only, based on source `7c864f266a939aa2a42422440dd4c1c0cabe3814`.
Its frozen production `src` tree is `7dd159d491012fd47b46c0aa651e86ea959ca289`;
the untouched oscillator header has SHA-256
`c9bb458f238ea6cb0bf74444a1a9459441da846bbf3ab98598848b45b147034c`.
Production PR #6, `main`, CI source pins and the legacy/V1/V2 sound gates are immutable.
This is a prospective scheduler-work optimization, **not a changed CS-80 circuit law**,
hardware measurement, or claim that the 0.15–0.20 RTF budget is achievable.

## Admission and exact continuation

The original `ReconstructedOscillator::tick()` in
`src/core/ReconstructedOscillator.h` updates controls and records any initial
control event before its `for` scheduler. An alternative single-sample path
may run only after those unchanged operations and only if **all** hold:

1. `endNodesValid && branchEventCacheValid`; no `gates`, `event` or `refresh`;
   the effective frequency and threshold are unchanged and finite.
2. `state.resetRemainingSeconds == 0.0`, `state.charge > 0.0`, and `f > 0.0`.
3. The physical boundary `state.charge / f` is strictly **greater than**
   `dt + 2e-15`, preserving the old `physicalReached` endpoint rule.
4. Either no cached branch event exists or `cachedBranchSeconds > dt`. An
   exactly-on-endpoint branch event takes the **original** scheduler.
5. Every nonfinite, stopped, reset, changed-control, invalidated-cache,
   zero-progress, ambiguous-boundary and restored-state case takes the original
   scheduler. There must be no approximate math, skipped capacitor or residual
   update, altered FP environment, or changed event-capacity handling.

The only admitted work omission is the scheduler loop's redundant branch
classification. The path must call the **same** `advance(dt, after)`, update
elapsed time and remaining branch countdown in the **same expression order**,
then compute `after = nodes(1)` and enter the unchanged shared audio/diagnostic
epilogue. `diagnostics.branchSearchCount` must not increase for an already
cached branch. If the exact-state gate cannot prove this, abandon the
candidate; do not tune a tolerance or change the source law to make it pass.

## Independent truth and protected fixtures

`tests/reference/WscLoopOriginal.h` freezes the exact pre-change implementation
in a test-only namespace; it is mechanically copied from the above source and
must not be modified when optimizing the production scheduler. The independent
oracle checks **all** output pin bits, all capacitor, held-control, elapsed,
reset and 3×16 residual bits, both gates, counters, residual cursor, all
diagnostic flags/readouts, cumulative branch searches, and all 64 event slots
(including inactive slots) after every sample. It also checks idle advances,
state restores and invalid preparations. No `memcmp` of uninitialized C++
padding is allowed. The rates 88.2, 96, 192 and 384 kHz include the mandatory
44.1/48/96-kHz host cases at Standard/HQ internal rate.

Fixed fixtures: stable-control multi-cycle runs, PWM and Hz movement, frequent
gate toggles, held comparator changes, zero/negative/over-limit/NaN/infinite
control requests, signed zero, subnormal state, physical reset and charge
boundaries at `dt` and adjacent representable values, threshold corners,
reset plateau, complete oscillator copy/assignment, idle advance and
restoreState/restoreIdleState, and invalid prepare rollback. These exercise
both the fast and original fallbacks. The numeric acceptance file
`tests/cpu/wsc-steady-fastpath.json` fixes a **zero-bit-mismatch** limit before
any production edit. The new gate must meet its minimum tick, event, no-event
and search coverage. Existing alias/pin anchors remain independent.

The fixed fixture is 14,144 compared ticks per rate, **56,576** ticks total:
8,192 steady ticks, two 256-tick cloned-continuation runs, 4,096 changing-
control ticks and 192 threshold-edge ticks, plus 10 physical boundaries ×
two reset/charge modes × 32 ticks and 512 one-sample-reset-plateau ticks per
rate. The untouched standalone GCC build recorded 12,038 events, 45,311
no-event ticks and 19,630 scheduler searches. These three work counts are
observations, *not* cross-compiler exact-count requirements; the admission gate
requires 5,000 / 40,000 / 10,000 respectively. All 44,294,052 scalar fields
in that baseline were bit-identical. The gate requires **exactly** 56,576 ticks
and all four rates, so deleting a fixture silently is a failing mutation.

Two required negative controls are source-compiled test mutations:
`CS80_WSC_ORACLE_MUTATE_STATE` alters one ULP of charge in the checked snapshot,
and `CS80_WSC_ORACLE_MUTATE_EVENT` alters one ULP of an observed event timestamp.
Each must make the exact-bit oracle exit nonzero on the *unchanged* production
oscillator; a mere failure to compile or inability to reach the event is not a
valid mutation result. These are gate-sensitivity witnesses, not proof that a
future implementation is sound. After implementation, additionally mutate the
new eligibility boundary and branch-countdown update, and require both to fail.
The two prereg witness builds must compile successfully, run the fixture and
exit nonzero with both `BIT MISMATCH` and `bit mismatch budget exceeded`:
`state.charge` and `events.absoluteSeconds`, respectively, each one ULP away.

## Timing and compatibility policy

Before implementing, capture a stock benchmark baseline from this exact parent
at 48 kHz / 256 frames and **at least three** rounds. After implementation,
compile both sides identically and remeasure at least three rounds **on the same
host**, retaining per-fixture medians, p99, deadline misses, complete logs,
source/build identity and host metadata; compare sound rows and all
`scripts/check-compat-gates.sh` byte gates including `--strict-v2`. Also run
the full core tests, original three-rate audit and relevant alias tests. Never
report a speedup from different hosts or Callgrind instruction shares.

If CMake/benchmark provisioning, a genuinely independent comparator or full
byte identity cannot be established, publish **only this preregistration and
negative controls** on the isolated branch and do not touch production audio
or the PR #6 workflow pins. The current 12-checkpoint worst held RTF 0.888572
would still need 4.44× to reach 0.20; a scalar scheduling shortcut is not
expected to close that architecture-level gap.
