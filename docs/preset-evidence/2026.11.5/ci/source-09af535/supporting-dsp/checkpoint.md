# A46 — supporting DSP checkpoint

**Partial checkpoint: three original jobs and their three actual archives are qualified. The sanitizer and numerical jobs remain pending; overall supporting DSP acceptance is not granted.**

Execution source: `09af5358d34a7ea431de7e8a42e35dae66e52159`, tree `d18ddfb7766b622af346f55fe78a59d041adffc5`.
Original run: [45344888536, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45344888536).
Inspection record timestamp: `2026-11-24T07:42:28.849181+00:00`; the retained job snapshot was acquired at approximately 07:42 UTC on 2026-11-24.

## Observed original jobs

| Job | ID | Observed result |
|---|---:|---|
| Default-off raw audio versus shipped baseline | 138277054372 | success |
| Source circuit V1 and audio V2 compatibility | 138277054685 | success |
| Synthetic reference comparison (not hardware evidence) | 138277054786 | success |
| measure | 138277054911 | queued |
| sanitizers | 138277054625 | in_progress |

No terminal sanitizer result or numerical artifact result is inferred from a queued or in-progress state. The final completed run API and remaining original logs are still required.

## Inspected actual ZIP archives

| Kind | Artifact ID | Bytes | SHA-256 |
|---|---:|---:|---|
| legacy | 14899105668 | 29,435 | `de30012857354461a326adfabe53e3296e0483c28c12094e366d337ea1be743c` |
| source | 14898618245 | 983 | `16e261131426f43e9300472d661d8a36e0a96dc2bc8c030542950cf73231ba3a` |
| synthetic | 14900155841 | 198,609 | `0fdf197c380036e71bc2cddef8630bc87cb00496b6359c49b739cbcecc4bcae1` |

The archive bytes match authenticated API sizes and SHA-256 digests. Each archive is bound to its original successful upload by source/run/attempt identity, exact artifact ID, upload byte count and digest, and the original timestamped finalization records. Every ZIP member was inspected and inventoried; no downloaded code was executed.

The legacy and source-compatibility archives retain reports and matching comparison digests, plus the immutable legacy corpus. Raw compared binaries are not uploaded. The successful original unsuppressed cmp steps establish those comparisons; no local raw-audio re-comparison is claimed.

The synthetic artifact has eight matching trace digests, one matching declared source digest, a matching manifest digest and 66,040 finite numeric CSV cells. Its original command log records 22 Python tests followed by OK. This is explicitly synthetic evidence with analog_evidence false; it is not a physical instrument comparison.

## Source relationship and remaining scope

The later native source is `db6b5a76b3c5e38dd9444d0cf6065a14f6302814`. Exactly the installation and preset guides differ from the executed source. All 9,962 other tracked entries, including 912 selected source/test/script/build/installer/asset entries, are identical.

Whole supporting test-input equivalence is false. The packaging notice fixtures consume both changed guides, and the macOS installer architecture fixtures consume the installation guide. Final-source native runs must qualify those document-dependent fixtures. The 09af execution is not relabeled as the later source, and independently built binaries are not claimed byte-identical.

Sanitizer CTest counts, failures, formal skips and diagnostics remain unqualified until the complete terminal log is available. Nested Python skip counts cannot be inferred as zero from successful stdout suppressed by ctest --output-on-failure. All five supporting jobs are core-only/Python scope; they do not supply JUCE plugin provenance or qualify native packages.

## Retained records

- [Three-job partial inspection](partial-synthetic-inspection.json)
- [Exact source-equivalence record](release-source-equivalence.json)
- [Checkpoint retained-file hashes](checkpoint-retained-files.json)

The two earlier partial observations, exact API snapshots, decoded original logs, artifact metadata receipts, source contract and reproducible acquisition/inspection helpers are retained. Scratch ZIP files remain available separately for the eventual final inspection.

The parent paused this worker after this checkpoint to free a concurrency slot. Resume by acquiring the original remaining results from run 45344888536 attempt 1, without rerunning CI or changing production gates. The existing final-review generator is deliberately gated on all five successful jobs and all six inspected archives.
