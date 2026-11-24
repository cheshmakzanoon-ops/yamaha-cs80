# A46 — replacement DSP checkpoint

**Four original jobs and all six actual replacement-run archives passed inspection. Overall supporting DSP acceptance remains pending the original sanitizer result. Native qualification is outside this checkpoint.**

Executed source: `c76ad288e02fbeca48f39652bf4fcdd99837f793`, tree `416c1f6bfb4cd9f7dd5dbd9f5cbc211edda2bf00`.
Original run: [45351731746, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45351731746).
The retained latest job snapshot was observed at 08:37:18 UTC on 2026-11-24. This worker is paused at the parent's request to free a concurrency slot for a separate native failure investigation.

## Original job states

| Job | Job ID | Observed state |
|---|---:|---|
| measure | 138299680648 | success |
| sanitizers | 138299680788 | in_progress |
| Source circuit V1 and audio V2 compatibility | 138299680844 | success |
| Synthetic reference comparison (not hardware evidence) | 138299681076 | success |
| Default-off raw audio versus shipped baseline | 138299681079 | success |

Sanitizer counts, formal skips and diagnostics are not qualified yet. Its test step began at 08:10:14 UTC. A final successful run API, complete original terminal log and complete actual CTest result roster are still required. Nested Python skips must remain unknown where successful child stdout is suppressed.

## Actual acquired archives

| Kind | Artifact ID | Bytes | SHA-256 |
|---|---:|---:|---|
| legacy | 14901531465 | 29,436 | `103468a71330dd05313b6dca62f39fe2f993737f9712ed786524deaa7517e336` |
| source | 14901590818 | 985 | `f985dd3923a377d61b9fbb15d05093a3ed1732e7c5b742157643941ee3569b37` |
| synthetic | 14900839011 | 198,613 | `d98b7eeaad3a2f15a5843e3c85ab0b13da2a84b19529e9dc24435f199e385d75` |
| candidate | 14900009973 | 1,438,001 | `12448d7d5bb8129712c7489474e9582a745d557dba976ea8590a0d33bd81166d` |
| live-adr | 14901087142 | 1,454,428 | `77cbc1c527369a2d0e9afc0c756b28010a51db09451866385e1e410689439238` |
| dsp | 14901286103 | 8,505,895 | `6e57f7331d455b4864f6519b1174d606546d3c27c0581699aaf74bfcdcce6cd5` |

Each ZIP matches its authenticated API size and SHA-256 digest and its original successful timestamped upload. Source, repository, run, attempt, artifact ID, byte count and digest binding passed. All members were inspected and individually inventoried from the same bytes.

The candidate and live-envelope exports each match all 202 parameter IDs derived from the committed core tables. Candidate, initial live and final live settings all use Standard (0) for both resonator variants. Four audio channels each contain 96,000 correctly indexed finite samples, expected initial silence and fixture signal. Both archives match fifteen committed source hashes and declare the same producer-executable digest; the executable itself is not distributed.

The numerical report has zero probe failures, a passed software-reference regression, a clean source tree and five matching analysis-time source hashes. All 3,014,646 parseable numeric cells across 26 CSV files are finite. This is not an independent recomputation of every numerical/Fourier result.

The original measurement log records thirteen metadata/capture tests followed by OK. The synthetic log records twenty-two tests followed by OK; its archive has eight matching trace digests, one matching source digest and a matching manifest digest. Synthetic and numerical checks do not establish a physical-instrument match.

The compatibility archives retain matching digest reports and the immutable legacy corpus. Their original successful, unsuppressed cmp steps establish the comparisons; raw compared binaries are not uploaded, and no local raw-audio re-comparison is claimed. These core fixtures are not listening evaluations of the revised factory presets.

## Source and history

All results in this checkpoint are direct c76 executions. No source-equivalence substitution or guide-input exception is needed. The failed 09af run remains preserved history and is not reused as acceptance evidence. All 25 historical checkpoint file hashes were verified unchanged; the actual old metadata failure is linked in prior-run-history.json.

These jobs use core-only targets or Python fixtures. They do not supply JUCE plugin provenance or qualify native bundles, pluginval, installers, physical instruments or perceptual quality.

## Retained evidence and resumption

- [Four-job/six-archive inspection](partial-four-job-inspection.json)
- [Latest original-job snapshot](jobs-checkpoint-api.json)
- [Exact source contract](source-contract.json)
- [Preserved failure-history references](prior-run-history.json)
- [Checkpoint retained-file hashes](checkpoint-retained-files.json)

All six immutable ZIPs remain in /workspace/scratch/c98fb40a9529/ci-c76-dsp-downloads. On reactivation, acquire the original completed sanitizer log and fresh final run/jobs/artifact API snapshots as run.json, jobs.json and artifacts.json. Run inspect_supporting.py without --allow-pending-run to produce supporting-ci.json, then write_review.py to produce reviews/a46-final.md and retained-files.json. Both scripts refuse final acceptance before all original jobs succeed and actual artifacts are verified.
