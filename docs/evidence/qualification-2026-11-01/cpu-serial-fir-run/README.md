# Serial GitHub CPU qualification including zero-history FIR

2026-11-01. [Run 41262684915](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41262684915)
completed all nine immutable checkpoints and three rounds per checkpoint on
one Ubuntu 24.04 GitHub runner. The frozen request retains the original seven
checkpoints in their original order and appends the native-BBD pre-FIR source
and exact zero-history FIR change. All nine builds finished before the first
measurement; all 27 measurement intervals ran serially in round-major order.
This is software callback timing. No hardware measurements or real-DAW
qualification are claimed.

**The CPU targets remain open.** The final source measures 0.388281–0.557422
held-note callback realtime factor (RTF), above the required 0.15–0.20 range.
Quiet idle measures 0.0360525 RTF, above 0.02. RTF is callback duration divided
by the block's audio duration; 0.0360525 means 3.60525% of realtime, not an
operating-system CPU-utilization reading. The FIR pair improves idle timing
but does not improve the held-note results in this run.

## All nine checkpoints

The stock driver uses 48 kHz, 256-frame blocks and 1,024 callbacks, retaining
all 27 cases and 51 result rows per round. Release phases retain their stock
varying lengths. The preregistered reporter selects the minimum callback
median per fixture from its three complete rounds, then takes p99 and deadline
misses from that same round. Held ranges cover the eleven original circuit
paired-tone fixtures. Each selected held deadline count is summed across
those eleven fixtures. No raw round is discarded. Values below are rounded
for display; the exact numbers remain in `compact-summary.json` and the
original per-checkpoint summaries.

| Checkpoint | Held median RTF range | Held p99 RTF range | Held deadline misses | Quiet idle RTF | Moving-controls median RTF |
| --- | ---: | ---: | ---: | ---: | ---: |
| main-before `db24a47170` | 0.353231–0.416845 | 0.369217–0.466853 | 0 | 0.083520 | 0.416252 |
| inherited-before `2ee33dcc81` | 0.378000–0.530563 | 0.396183–0.656434 | 0 | 0.057242 | 0.526779 |
| lazy-filter-histories `476c2f1fed` | 0.376517–0.538466 | 0.407882–0.651675 | 1 | 0.058862 | 0.534769 |
| idle-exact `f79b6f7fe6` | 0.378828–0.547914 | 0.399436–0.647711 | 0 | 0.051376 | 0.531004 |
| stationary-controls `8dac795558` | 0.370577–0.559799 | 0.398491–0.662259 | 7 | 0.050491 | 0.522698 |
| unused-lfo `193f1a31a6` | 0.372002–0.540718 | 0.423601–0.661914 | 3 | 0.048241 | 0.514173 |
| rank-idle-1500 `18400c29a5` | 0.369646–0.536324 | 0.395692–0.618616 | 0 | 0.041867 | 0.525081 |
| pre-fir-native-bbd `475063ca4b` | 0.372466–0.545617 | 0.390135–0.653069 | 0 | 0.042133 | 0.530523 |
| zero-fir-history `23a7b8df5c` | 0.388281–0.557422 | 0.397795–0.715900 | 8 | 0.036053 | 0.537154 |

Every output-finiteness count is zero in all 1,377 result rows. The later
checkpoints' paired Legacy held ranges are approximately 0.151–0.194 RTF.
The original-main to inherited-completion comparison also includes the
qualified full-band nodal filter and electrical programmer; it does not
isolate an individual optimization.

This runner reports an AMD EPYC 9V45 CPU and four virtual CPUs. The earlier
[seven-checkpoint run](../cpu-serial-run/README.md) reports an AMD EPYC 7763.
Its absolute timing numbers must not be directly compared with this run to
claim a code speedup. This report uses comparisons within the same nine-source
run. Hypervisor interference is still uncontrolled; the source order was
fixed before evaluation and was not randomized.

## The isolated FIR comparison

Before: `475063ca4b5640c1b17cb45eb89a23706ad18873`.
After: `23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162`.

| Measure | Before | After |
| --- | ---: | ---: |
| Circuit held median RTF range | 0.372466125–0.5456169375 | 0.388281375–0.557422125 |
| Circuit held p99 RTF range | 0.390134625–0.6530685 | 0.3977945625–0.715900125 |
| Selected held deadline misses | 0 | 8 |
| Circuit quiet-idle median RTF | 0.0421329375 | 0.0360525 |
| Circuit moving-controls median RTF | 0.5305228125 | 0.53715375 |
| Circuit moving-controls p99 RTF | 0.7039940625 | 0.935590125 |
| Selected moving-controls deadline misses | 1 | 9 |
| Legacy held median RTF range | 0.1566129375–0.1876535625 | 0.156922875–0.1875639375 |
| Legacy quiet-idle median RTF | 0.01545075 | 0.008660625 |

The selected circuit idle median decreases by **14.4316%**. Each of the three
after-round idle medians is lower than each before-round median: before
0.0431300625 / 0.0421329375 / 0.0438886875, after
0.0366703125 / 0.0366384375 / 0.0360525. All six idle phases have zero deadline
misses. The shared engine oversampling decimator also serves Legacy mode, so
exact zero-history work elimination can reduce Legacy idle time while
preserving its audio.

Held and moving-control timing worsened in this observation, including the
reported p99 values and deadline counts. This artifact supplies no basis for
claiming a held-speed improvement or assigning that difference entirely to
scheduler noise. The intended optimization removes convolution work only
when the complete FIR history is exactly zero; its nonzero-history bookkeeping
can still cost time. Further work is required to meet both circuit targets.
No workload, quality setting, tolerance, or frozen reporter rule was changed.

For the six before/after rounds, all **306 rows** have identical recorded
non-timing fields, including fixture identity, voice/controller activity,
checksums, peak/RMS values and nonfinite counts. The exact compared-field and
excluded-timing-field lists are retained in `fir-comparison.json`. This
comparison checks recorded benchmark observables; the separate
[zero-FIR qualification](../zero-fir/README.md) preserves the state/rounding
mutation tests and the strict Legacy/default/tails, V1 and V2 audio checks.
The current run does not replace those numerical gates.

## Source and compiler provenance

Every manifest source hash was independently checked against its recorded Git
commit: **662 source-file records and 108 unique blobs**. All nine commit trees
and benchmark source blobs match. Original main retains benchmark blob
`b73298678acb6684654ed1b3771fb442352853c0`; the subsequent eight retain
`92ea7e0b1801bf98fccaed21c13e676890039c94`. Their one-line difference returns
`ControlFrame::programmer(frame.effectiveValues, frame.hardwareSources)` in
place of the original aggregate frame. The original sources and timing/scoring
loop are preserved; the two distinct blobs are not normalized away.

Both FIR checkpoints use the same second benchmark blob. The relevant three
compile commands (Engine, FactoryPresets and the stock benchmark) and benchmark
link rule are identical after replacing only their absolute per-checkpoint
source/build path prefix. GCC 13.3.0 builds them with the original Release
`-O3 -DNDEBUG -std=gnu++20` configuration. The normalized commands, link rule
and changed-source hashes are recorded in `fir-source-and-build.json`.

Within the recorded core/reference/build scope, the pair differs in three
files. `Dsp.h` contains the FIR shortcut and bookkeeping. `Engine.h` adds
friend access defined only by the continuity test, with no runtime code or
object-layout change. `CMakeLists.txt` adds test registrations and a strict
rounding option applied only to the rounding-test target. The complete scoped
diff is retained as `fir-source-diff.patch.gz`; no production-wide floating-point
option was changed.

## Artifact and verification

[Artifact 13066662491](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41262684915/artifacts/13066662491)
is preserved byte-for-byte as `original-ci-artifact.zip` (808,261 bytes).
Its SHA-256 is
`168fe21914ebad8eccc718462ec3fdd31ece8c0317478c5ca5defee07bbd0999`.
The downloaded bytes match both the GitHub API digest and the successful
upload log. All **262 original internal manifest entries** were independently
rehashed and matched. The archive has those 262 files plus `sha256.json`.

The original ZIP retains complete host/process snapshots, configure/build
logs, compiler commands, benchmark source, all raw rounds, frozen collector
and reporter source, and original manifests. Important CSV/metadata/completion
records and per-checkpoint source manifests/summaries are also expanded here
for review. `sha256.json` names original paths inside the ZIP; it does not
assert that every such path is duplicated outside it. `report-sha256.json`
separately hashes the preserved archive and expanded/derived report files.

`independent-verification.json` records the source, interval and completion
checks. The unchanged frozen reporter recomputed every stored summary exactly.
`verify-retrieval.py` reproduces those checks against an extracted original ZIP
and a repository containing the immutable source objects and collector/report
scripts. It only reads existing source/evidence and writes the three derived
summary records; it performs no builds or benchmark reruns.

Run head: `b1dbc46109fbffa81b9ea83966a3160933e4ccac`.
The full successful job log is retained in `job-124718989712.log.gz`, alongside
API job/artifact snapshots. CI passed all 27 actual-source runner mutations
before building. The earlier failed nine-checkpoint run and the proven
prefix-length-witness correction remain separately preserved in
[CPU runner prefix fix](../cpu-runner-prefix-fix/README.md). Its correction
changed the witness, with all 27 mutations and immutable checkpoint requirements
retained; this report does not treat a green rerun alone as a mechanism fix.
