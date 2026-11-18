# Paired stock-clock precision study

**Result: FAIL / no precision admission.** All 20 planned processes completed,
and all 20 passed the frozen integrity checks. Numerical precision failed
**36 threshold checks**. This completed stock-only control trial does not support
using this measurement setup to admit a candidate speedup.

Both A and B contain the same shipping stock core. **This is not C, S or V timing,
not a plugin/DAW benchmark, and not a reinterpretation of the earlier 216-slot
experiment.** No observations were removed, retried, replaced or collected again.

## Main findings

- **7 of 12 sham processes** exceeded the allowed -1.5% to +1.5% signed CPU effect.
  Sham estimates ranged from **-4.9626% to +5.8200%**.
- All eight deliberate extra-work processes had the correct direction and passed
  their per-process 1.03–1.10 extra-to-ordinary CPU ratio gate. Their observed
  extra CPU ranged from **4.9868% to 9.8746%**.
- Across all **144 sham reversal cycles**, the nearest-rank 90th percentile of
  apparent effect magnitude was **11.0857%**, versus a 4% limit. The peak was
  **26.3200%**, versus a 10% limit.
- All overhead gates passed. The largest empty-clock max/span ratio was 0.78884%
  (limit strictly below 1%); the largest corresponding mean/span ratio was
  0.10182% (limit strictly below 0.25%). Zero-boundary probes were smaller.
- Failure accounting: 7 process effect, 12 process cycle-p90, 9 process cycle-max,
  4 cell sham-range, 1 cell mean-sham, 1 cell positive-direction-gap, and 2 pooled
  cycle checks. That is **28 process + 8 cohort = 36 failures**.

Detecting added work did not establish the precision needed for a smaller genuine
optimization. Passing integrity and overhead controls did not rescue the sham
or cross-process stability failures.

## All 20 observed process effects

Numbers are percentages for **B over A**, `100 * (exp(mean cycle log(B/A)) - 1)`.
Positive means B costs more. In A-extra rows, the intended response is negative;
in B-extra rows it is positive. The reported positive-control range above uses
the correctly inverted extra/ordinary ratio, not the absolute signed percentage.
Wall values use the identical pairing estimator but are **descriptive only**.
They do not enter any admission gate or claim realtime capacity.

| Slot | Scene | Chunk | Control | Initial order | CPU B/A % | Wall B/A % | Failed process checks |
|---|---|---:|---|---|---:|---:|---:|
| 00 | factory-held | 16 | sham0 | AB | +5.8200 | +5.8058 | 3 |
| 01 | panel-moving | 32 | sham0 | BA | +0.5822 | -0.3545 | 2 |
| 02 | factory-held | 32 | sham0 | AB | -2.4522 | -2.4290 | 3 |
| 03 | panel-moving | 16 | sham0 | BA | -1.3311 | -1.2998 | 2 |
| 04 | panel-moving | 32 | A-extra | BA | -8.9343 | -9.6773 | 0 |
| 05 | factory-held | 32 | A-extra | AB | -5.5419 | -5.5311 | 0 |
| 06 | panel-moving | 16 | A-extra | BA | -6.0868 | -6.0924 | 0 |
| 07 | factory-held | 16 | A-extra | AB | -8.9871 | -8.9883 | 0 |
| 08 | factory-held | 32 | sham1 | BA | +1.5478 | +0.3957 | 2 |
| 09 | panel-moving | 16 | sham1 | AB | -0.6240 | -0.6018 | 2 |
| 10 | factory-held | 16 | sham1 | BA | +1.2088 | +2.2502 | 2 |
| 11 | panel-moving | 32 | sham1 | AB | -1.8140 | -1.8042 | 2 |
| 12 | panel-moving | 16 | B-extra | AB | +4.9868 | +4.9655 | 0 |
| 13 | factory-held | 16 | B-extra | BA | +8.9543 | +7.8489 | 0 |
| 14 | panel-moving | 32 | B-extra | AB | +6.2205 | +7.1127 | 0 |
| 15 | factory-held | 32 | B-extra | BA | +7.6323 | +6.3595 | 0 |
| 16 | factory-held | 16 | sham2 | AB | -4.9626 | -4.9364 | 3 |
| 17 | panel-moving | 32 | sham2 | BA | +0.4670 | -0.4286 | 2 |
| 18 | factory-held | 32 | sham2 | BA | -2.0950 | -2.0708 | 2 |
| 19 | panel-moving | 16 | sham2 | BA | +2.0460 | +2.0406 | 3 |

## Frozen design and arithmetic

The four cells are Factory-held and Panel-moving, each with 16- and 32-callback
chunks. Five fixed rounds rotate the cell order: sham0, A-extra, sham1, B-extra,
sham2. There are ten AB and ten BA initial orientations. The complete schedule and
all original thresholds are in [protocol.json](protocol.json).

Each fresh process owns two isolated stock images and independent main/shadow
contexts. Each side has 1,024 warm callbacks, 512 measured main callbacks and one
unclocked continuation. A positive control performs exactly 32 additional real
stock callbacks in the already-warmed shadow context, one per 16 main callbacks,
on the nominated side only. This is +6.25% callback work, not an assumption of an
exact +6.25% CPU response. Main-output identities and both work counters are
verified independently of the timing.

Each measured chunk pair alternates AB then BA, or BA then AB. Two adjacent
opposite-order log ratios form a reversal cycle; each process estimate gives all
its cycles equal weight. The raw thread-CPU bracket encloses the monotonic-wall
bracket and work. Empty-clock and zero-boundary probes are retained from both
before and after the workload. No overhead is subtracted. The overhead denominator
is that process's smallest complete ordinary measured span.

Gates use the frozen exact integer/rational comparisons, with 80-digit Decimal
arithmetic for the declared cell range and direction checks. Descriptive values
use the original floating-point estimator. There is no bootstrap interval,
post-hoc trimmed average, pooled replacement endpoint or wall-based rescue rule.
Full cell results, every cycle and all numerical pass/fail records are retained in
`numerical-result.json.gz`.

## What this compact export preserves

`spans.csv` contains **all 3,400 ordered rows**:

- 960 measured main-work spans, paired into 480 chunk pairs and 240 reversal
  cycles. Of these, 144 cycles are sham cycles and 96 are positive-control cycles.
- 160 empty-clock and 320 zero-boundary observations, all clocked.
- 1,920 unclocked warm spans and 40 unclocked continuation spans.

Thus **1,440 rows have both raw CPU and wall brackets**; the other 1,960 retain
blank clock columns. No sampled span or overhead observation is dropped.

The CSV uses integer nanoseconds. Each slot's thread-clock origin and one common
trial wall-clock origin are subtracted, with the first origin represented as 1 ns.
All durations, ordering and within-domain gaps remain exact; wall spacing between
processes is retained. Absolute origins are deliberately not exported, so the CSV
is lossless **after this explicitly stated normalization**, not a byte-for-byte
copy of the original JSONL. Per-slot original JSONL SHA-256 values remain in
[provenance.json](provenance.json).

Each CSV `receipt` index refers into `work-receipts.json.gz`. That dictionary
preserves the full before/after reports, main/shadow/extra counts, endpoint hashes,
caller and image FP states, errno and CPU/affinity checks. Original frozen gold
endpoints used by these rows are included; unused endpoints are omitted. Both gzip
files use deterministic encoding and the Python standard library can read them.

`export.py` retains the normalization code and its completion/decision guards.
It requires the separately held original campaign and explicit expected final
hashes; it is not needed for replay and cannot collect observations.

Private host paths, launch arguments, PIDs, absolute clock origins and runtime
ASLR/base/function/errno addresses are not published. Per-slot hashes of the
original attempt, stdout/stderr, loader and image-proof records remain, together
with work summaries and verified integrity-check counts. Hashes identify omitted
proofs; they are not a portable re-execution of image isolation or a guarantee
about the original host.

## Offline replay

Copy this directory intact and use Python 3.12 with its standard library:

```sh
python3 -B replay.py
python3 -B replay.py --self-test
```

Replay checks the manifest, all 20 complete slots, every row's order/work/FP/gold
identity and relative clocks, then recomputes **every CPU and wall effect and every
numerical gate**. The saved result must match exactly. `contract.py` and
`analyze.py` are copied byte-for-byte from the frozen official implementation;
their authorship is the project's original stock-clock study, not a newly tuned
estimator. Their source hashes and the export's normalization are recorded here.
Last-bit floating-point differences on another Python/libm implementation will
fail the exact result check rather than silently rewrite the evidence.

The **13 offline tests** include incomplete-terminal and unverified-decision
rejection, candidate substitution, weakened thresholds, missing/reordered rows,
work-count/FP/CPU corruption and invalid clocks. Synthetic tests fabricate zero-sham
and exact 6.25% extra-work arithmetic, plus a deliberately invalid sham. No test
renders audio or reads a measurement clock.

The exporter refused data access before verifying the final complete-20 terminal,
the frozen official decision and frozen-source hashes. It then reran the original
read-only validator. All effects, numerical gates, failures and remaining decision
fields matched exactly. Expanded integrity-check arrays matched as exact multisets:
their list order alone is nondeterministic because that validator iterates Python
sets. The original decision was not changed.

The 51,455,683-byte official decision's SHA-256 is
`11f6f6bec370d50fac0348eb154aa31a61ec1cc8838ca24a3ad0da636dbe3ce7`.
Its 295,696 expanded checks are represented by exact numerical outcomes and compact
integrity receipts, rather than a giant duplicate dump. This package reproduces the
failed precision conclusion and makes no candidate, native, DAW or realtime claim.
