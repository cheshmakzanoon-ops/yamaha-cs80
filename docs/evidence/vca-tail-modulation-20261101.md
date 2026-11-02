# Current-sample modern modulation of VCA coupling tails

2026-11-01. Baseline audio: `b75b4d653b6ed6f0aa5ef8ee99b670d83e975250`.
This is a modern software integration question, not a Yamaha circuit law.
The sections through the registration amendments retain the before-evaluation
record; completed qualification follows below.

Independent review found that the newly connected inactive VCA tail consumes
`modern.rankOffsets()` before the old decimated `processVoice` call. A live
matrix-to-pan route can therefore use an earlier sample's offset; a poly MSEG
still advances only at the former silent-voice cadence. Pure and the neutral
default have no active matrix and are unaffected by that code path.

The preregistered contract is
`tests/parity/tolerances-v2/vca-tail-modulation.json`. No candidate behavior or
new witness has been evaluated at this registration. The existing original
phrase score criteria, capacitor recurrence, idle-clock bounds and Legacy/V1
compatibility requirements remain unchanged.

The proposed narrow repair computes current tail pan before mixing, only when
a source-policy inactive voice has nonzero stored coupling and a live pan
destination. The original idle deadline still owns note identity, per-note RNG,
mono triggers and global newest-voice publication. Additional tail samples may
advance used poly MSEGs and evaluate offsets, without repeating those ownership
operations. The later scheduled call must not tick a second time.

One MSEG continues to serve all its routes. If a poly MSEG drives pan and another
destination, its corrected elapsed-time phase also affects that other route at
its scheduled evaluation. A separate cloned pan runner would create two
inconsistent versions of one modulation source. No-pan configurations retain
their existing path; Pure and neutral defaults must remain exact.

The registered witnesses cover current raw rank mix, zero observer effect,
full-rate poly elapsed time, both ranks/policies/rates/qualities, global selection
timing, late matrix activation before note identity was latched, and subsequent
note-RNG ordering. At registration, production changes remained pending qualification of the prior
VCA audio checkpoint and its separate published V2 pin. That prerequisite was
completed before the candidate was applied.

## Registration amendment: removal of the last pan route

Before applying or evaluating the candidate, source review identified a second
boundary: removing, zeroing or rerouting pan while another matrix slot stays
active leaves the previous voice pan offset cached. A predicate that checks only
current live pan destinations would miss clearing that offset before the next
inactive tail sample. Register both-rank witnesses for those three edits using
the existing exact zero/current-mix bounds. Tail eligibility may include a
nonzero cached pan offset, so one final scheduled-safe evaluation clears it.
This applies only to charged Source tails; existing Legacy and neutral paths
remain unchanged. A separate mutation will remove that cached-offset condition.

## Registration amendment: independent ownership and expression boundaries

Read-only review also identified three controls missing from the initial witness.
Register direct `ModernLayer` comparisons for mono triggers and held-gate
publication on extra unscheduled updates; nonzero pressure/timbre-to-pan across
the first inactive deadline; and unchanged decimated poly-MSEG tick counts when
a live matrix has neither present nor cached pan. The same exact/elapsed limits
apply. Source control smoothing restores event-set pressure/timbre after each
sample, so the tail must keep using the scheduled released values after its
first deadline rather than reviving old expression between deadlines. This
affects the input to the tail update and leaves stored controller targets and
the original scheduled paths unchanged. The one update clearing a removed pan
route intentionally advances remaining used poly runners/rate feedback once;
steady no-pan configurations keep their previous cadence. Independent mutation
cases cover each boundary. All registrations precede the production repair.

## Qualified implementation

The published source change is
`83b7f7670f000a95c6b44f96ffb0df3894755d14`; its separate V2 pin is
`14ef653ed60be67ff98656966f9181c5e9b6b4fc`. The locally evaluated audio
commit `b96c3cfd52f42eccc8cd37bbdbaa7edb84b5d94f` has the identical `src`
subtree `7dd159d491012fd47b46c0aa651e86ea959ca289`. The preceding audio
sequence (`b75b4d6` → `fb43c04` → `ac931915`) was completed and published
before this candidate was applied.

Canonical preregistrations are `4e5dc03560ef8e3c65f5a131fa2137a5cdf04b42`
(current sample), `9bb6ee02a5b824439e6c21cad0182dd8fcb55a8d` (route removal),
and `f31bbb4120fa4b8d3827a920b0b2e8bb9ff666b7` (ownership, expression, and
steady no-pan cadence). The new direct API test had a compile-only namespace
correction before any execution; no compile failure is counted as a rejected
mutation. Numeric tolerances were not changed during qualification.

Only `Engine.cpp` and `ModernLayer.h` change production behavior. A charged
inactive Source voice with a live pan destination, or a remaining cached pan
offset, evaluates modulation before consuming its signed VCA discharge. The
original idle deadline is predicted without advancing its counter. Additional
evaluations do not latch note identity, draw note randomness, trigger mono/poly
identity transitions, publish held gates, or replace the newest global-source
owner. The original scheduled call is suppressed when that frame already
evaluated the tail, preventing a second poly tick.

Pressure/timbre inputs are zero from the original first idle deadline onward.
This leaves event targets and their smoothers intact while preventing those
restored targets from reappearing in intervening tail samples. Removed pan is
cleared on the next sample even when another route keeps the matrix active.
That final clearing evaluation can advance another used poly runner once;
steady configurations with neither current nor cached pan retain the original
decimated cadence. Pure, Legacy, and neutral defaults retain their original
paths. Exact nonzero coupling selects the work, independently of rank gain,
so muting a rank does not freeze its modulation time.

A shared poly MSEG remains one source. If it drives pan and other destinations,
its corrected phase also reaches those other destinations at their scheduled
evaluations. The guarantee is unchanged note/owner timing, not equality of
mixed-route values to an incorrectly decimated runner. This is an explicit
modern software rule with no claim of Yamaha hardware equivalence.

## Measured regression results

| Check | Before (`b75b4d6`) | After (`83b7f767`) | Registered requirement |
| --- | ---: | ---: | --- |
| Expanded engine witness | 618 checks, 92 failures | 618 checks, 0 failures | No failures |
| Direct mono/held-gate scheduling controls | Extra-update API absent | 6 checks, 0 failures | Exact equality |
| Largest current-pan error | 0.000119537115097 | 0 | Exactly 0 |
| Largest poly elapsed-time error | 0.0228344671202 s | 5.03251953998e-14 s | At most 1e-12 s |
| Existing VCA primitive suite | Previously qualified | 59 checks, 0 failures | Unchanged |
| Existing VCA idle/panic/reset grid | Previously qualified | 361 checks, 0 failures | Unchanged |

The raw rank-mix witness covers Source and Pure, three rates, three quality
settings, both rank endpoints, and free/poly MSEGs. It checks 2,048 consecutive
inactive internal samples, including the first within a host frame. The
observed production stages must produce exactly the same internal output bits
as an unobserved `processInternalSample` twin. Current MSEG readout supplies
the pan expectation; a separate elapsed-time oracle prevents a frozen source
and frozen consumer from passing together.

Additional fixtures preserve velocity/key/random global ownership, activate a
route before an old note identity was latched, assign a new note before the old
idle deadline, remove/zero/reroute pan while another slot remains active, cross
the original controller-clear deadline, and retain the original no-pan poly
tick count. The direct layer controls independently observe mono triggering
and held-gate publication with no older held voice masking their effects.

## Qualification evidence

All **14 actual-source mutations** were rejected by their intended assertion.
Both unmodified and restored controls pass 618 engine checks plus 6 direct
scheduling checks. Every engine mutation runs the complete 618-check fixture;
the two direct mono/held-gate mutations run the complete 6-check layer fixture.
No production tree is edited by the mutation runner, and no compile failure
counts as a rejection.

| Fault injected into isolated source | Failing checks |
| --- | ---: |
| `omit-early-pan` | 92 |
| `consume-pan-before-update` | 44 |
| `double-tick-idle-deadline` | 36 |
| `publish-unscheduled-owner` | 6 |
| `latch-unscheduled-note` | 1 |
| `run-tail-pan-in-legacy` | 1 |
| `enable-pure-matrix` | 127 |
| `weaken-elapsed-time-bound` | 1 |
| `ignore-removed-pan-cache` | 12 |
| `publish-unscheduled-gate` | 1 |
| `trigger-unscheduled-mono` | 2 |
| `omit-controller-clearing` | 4 |
| `revive-expression-between-deadlines` | 4 |
| `advance-active-no-pan-tail` | 1 |

Non-strict compatibility against the prior `b75b4d6` pin and strict
compatibility against `83b7f767` both exit 0: Legacy default, Legacy tails,
V1 numerics, and V2 default audio are byte-identical. The additional direct
file comparisons retain sizes and SHA-256 values in `strict-receipt.json`.
The V2 default bytes also equal the prior checkpoint: the intentional change
requires an activated modern pan route or its just-removed cached value.
Its registered nondefault tests cover that behavior. The source/repin/STATUS
sequence is complete with third commit
`af943c27b9c4012cf128a0e0063bdfcdcb3edf33`.


The [complete qualification bundle](qualification-2026-11-01/vca-tail-modulation/README.md)
contains raw logs, source mutations, baseline/current summaries, benchmark
CSVs, byte-capture hashes, and source/binary identities. No test, window,
score definition, or tolerance was weakened. No output fade was introduced.

## CPU scope and original click limits

Three after rounds repeat the same 48 kHz / 256-frame / 128-callback baseline
benchmark as the three `b75b4d6` after rounds reused here as the before control.
Each completes all 27 cases / 51 rows. This default benchmark covers Legacy
and source-model isolation (Pure); it does not measure an activated modern pan
route. Other qualification workloads share the host, so these timings are
diagnostic and cannot establish a CPU speedup or the 0.15–0.20 target. The
serialized CI measurements remain the performance qualification.

| Capture | Round | Held median RTF range | Quiet idle median RTF |
| --- | ---: | ---: | ---: |
| before | 1 | 0.639069187–2.357763938 | 0.061994437 |
| before | 2 | 0.959161125–4.164548250 | 0.138642188 |
| before | 3 | 0.977183062–4.722354562 | 0.122823750 |
| after | 1 | 0.680097938–1.102997250 | 0.064788000 |
| after | 2 | 0.677283188–1.499974875 | 0.062041875 |
| after | 3 | 0.842014313–1.438364812 | 0.128268375 |


All six rounds have zero nonfinite samples and identical recorded checksum,
peak/RMS, callback and voice-count fields for corresponding cases. Those CSV
fields supplement the raw-byte compatibility gates and do not replace them.

This follow-up does not claim to close the original physical-PANEL Bass or
Funky note-off score requirements. Their unchanged <= 1 limits and the
remaining failures at the preceding checkpoint are recorded in
[the click investigation](click-20261101.md). Fresh final full-suite results
are owned by integration; a different programmer coordinate or an omitted-
event score cannot be substituted for the original requirement.
