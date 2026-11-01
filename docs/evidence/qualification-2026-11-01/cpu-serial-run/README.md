# Serial GitHub CPU qualification

2026-11-01. This is software callback timing, not hardware measurement or a
real-DAW qualification. [Run 41257213038](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41257213038)
completed all seven immutable checkpoints and all three rounds per checkpoint
on one Ubuntu 24.04 GitHub runner. Builds completed before timing; round-major
timing ran serially. The checked-in request and runner/report hashes predate
evaluation. The request seals two benchmark source blobs: original main uses
`b73298678acb6684654ed1b3771fb442352853c0`; the six subsequent checkpoints
use `92ea7e0b1801bf98fccaed21c13e676890039c94`. Their one-line difference is
returning `ControlFrame::programmer(frame.effectiveValues, frame.hardwareSources)`
instead of the old aggregate frame, so the later driver reaches the completed
electrical programmer. The 27 cases, 51 rows and timing/scoring loop are
unchanged. The original artifact `SUMMARY.md` already records this distinction;
this paragraph corrects the first retrieval report
`d43174a6e1f4268024ce7fdd8bb057831b4682c6`, which incorrectly described the
source blobs as identical. No raw evidence or measured number was changed.

The original target remains **0.15–0.20 callback realtime factor for eight
voices, 48 kHz, 256-frame blocks; quiet idle <=0.02**. Neither target is met.
RTF divides measured wall-clock callback duration by the audio time budget.
An idle RTF of 0.077991 is 7.7991% of realtime, not an operating-system CPU
utilization measurement.

| Checkpoint | Held median RTF range | Held p99 RTF range | Held deadline misses | Quiet idle RTF | Moving-controls median RTF |
| --- | ---: | ---: | ---: | ---: | ---: |
| main-before `db24a47170` | 0.589659–0.695422 | 0.601415–0.808634 | 0 | 0.144685 | 0.670410 |
| inherited-before `2ee33dcc81` | 0.613537–0.893367 | 0.627465–1.048145 | 57 | 0.102848 | 0.854164 |
| lazy-filter-histories `476c2f1fed` | 0.608570–0.893615 | 0.630247–1.054142 | 64 | 0.100299 | 0.851390 |
| idle-exact `f79b6f7fe6` | 0.608242–0.888042 | 0.623219–1.056947 | 55 | 0.089710 | 0.856346 |
| stationary-controls `8dac795558` | 0.605332–0.889552 | 0.619235–1.058104 | 60 | 0.088383 | 0.849535 |
| unused-lfo `193f1a31a6` | 0.603646–0.889993 | 0.622794–1.054589 | 55 | 0.087235 | 0.852503 |
| rank-idle-1500 `18400c29a5` | 0.603543–0.890282 | 0.618008–1.054760 | 56 | 0.077991 | 0.849753 |

For each fixture, the reporter selects the minimum callback median of the
three complete rounds, then retains p99 and deadline counts from that same
round. Held ranges cover the eleven original circuit paired-tone fixtures.
Every measured phase retains 1,024 callbacks where specified by the stock
driver. Release-tail phases retain their original varying lengths. All 51
result rows in all 21 rounds are present; every output-finiteness count is zero.

This run supports the direction of the individual idle reductions more
clearly than the earlier shared-container series: paired Legacy held ranges
remain approximately 0.269–0.314 RTF. It does not isolate an individual cost
within the larger original-main to inherited-completion change, which also
introduced the qualified full-band filter and electrical programmer. The held
target remains substantially above budget. No timer threshold, fixture,
quality setting, tail, or numerical requirement was changed to improve the
reported result.

The last measured source is the published 1.5 kHz idle checkpoint
`18400c29a54ac2204564b355c0e52ffd3b0dcb8e`. This report does not relabel it as
the later native Intel predicate repair or the pending FIR candidate. Those
changes have separate qualification and timing checkpoints.

## Durable raw evidence

The complete original GitHub artifact is retained as `original-ci-artifact.zip`.
It includes the unchanged benchmark source, compiler/build metadata, complete
host/process snapshots (executable names only), all raw rounds, reporter and
runner source, and the original 206-file SHA-256 manifest. The important raw
CSV/metadata/completion records and per-checkpoint summaries are also expanded
beside this report for review. `sha256.json` names paths inside the original
archive; it is not a claim that all those paths are duplicated outside it.

[Artifact 13064294170](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41257213038/artifacts/13064294170)
has SHA-256 `a43cef3608e1066c1820172b03c6c76ffbf25b2da206d81a88892bc2bd612435`.
That matches GitHub's artifact digest. Every original internal manifest entry
was independently rehashed after download and matched. The compact summary
is derived only from the original per-checkpoint summaries; the archived
original artifact bytes remain unchanged.
