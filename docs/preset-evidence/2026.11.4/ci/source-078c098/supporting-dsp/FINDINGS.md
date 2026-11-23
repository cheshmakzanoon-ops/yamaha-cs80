# A48 — supporting DSP qualification

**Status: complete; all five original jobs passed.**

Source: `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`.
Run: [45246863831, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863831).
This is a read-only supporting-workflow review. No source changes, CI reruns, native package qualification or new audio renders were performed.

## Original job outcomes

| Job | Job ID | Observed outcome |
|---|---:|---|
| Synthetic reference comparison (not hardware evidence) | 137945816716 | success |
| Default-off raw audio versus shipped baseline | 137945817147 | success |
| measure | 137945817176 | success |
| sanitizers | 137945817253 | success |
| Source circuit V1 and audio V2 compatibility | 137945817476 | success |

All retained job metadata belongs to the same full source SHA, run and original attempt. Completed-job logs show the exact checkout, original successful commands and upload steps. Earlier or superseded runs are not relabeled as these results.

## Actual supporting archives

All six downloaded ZIPs matched the authentic API byte counts and SHA256 digests. Each was bound to its successful producer job and attempt, exact source, upload line, and creation/upload timestamps inside the producing job. Numeric artifact-ID ordering was not used as provenance. Every actual ZIP entry was inspected for canonical unique paths and regular-file/directory types; these inspections did not extract or execute archive contents.

| Artifact | ID | ZIP bytes | SHA256 |
|---|---:|---:|---|
| synthetic | 14853109005 | 198,609 | `4fba882c577dbf86e6987914712b15fb88aa9c4fb06b9d3632a8f901098223ff` |
| legacy | 14855770877 | 29,436 | `a59ac27b9a0ca0ec3bb215a9b88560e3c99e8c1e588a2d060acb678ec6018e5d` |
| source | 14856000575 | 985 | `d4a9d78f9d9ac5114e2b9bb2f654c143654bb279772fa39aa92d1820e9b6e9eb` |
| candidate | 14856066121 | 1,437,988 | `da5ad87657fe494fd5a7562bdf853777fe1553d37875cf61e984ecf3263ec726` |
| live-adr | 14856840329 | 1,454,416 | `2fe175f05d2aab2f5071a8a368d35a669e24e5c8bb6f399b4f67e9fa6870c4e0` |
| dsp | 14854758765 | 8,505,873 | `07c0aebfa13a3cd0bed34c9b978e4456f9387b55bd09e75e811fb5510874fffa` |

- **Synthetic:** all eight trace hashes and the source/manifest hashes agree; 22 comparison/provenance Python tests and the synthetic plumbing step passed. The report explicitly says `analog_evidence: false`.
- **Engine candidate and live envelope:** all 7 and 8 member-checksum entries respectively agree with the actual files, and all 15 source hashes in each archive match the pinned source. The producer passed 13 capture-boundary Python tests.
- **Numerical measurements:** the actual report binds the pinned source, five matching analysis-time source hashes and a clean working tree. It reports zero probe failures and a successful comparative regression against software numerical references.
- **Compatibility:** the baseline corpus hash and recorded baseline/current digest pairs agree. The original successful producer `cmp` commands establish the comparisons. These two archives distribute reports/digest receipts and the legacy corpus, **not the raw comparison binaries**; no local raw-audio re-comparison is claimed.

## Sanitizer scope and counts

The retained terminal log records **199 passed CTest entries, 0 failed and 1 formally skipped**, out of **200 registered entries**. These are core-build CTest executions, including tool/Python fixtures as well as instrumented C++ tests; they are not native host or pluginval results.

Formal skips: `cs80_stereo_pra_exact_tests`.

The workflow ran ASan and UBSan with leak detection and halt-on-error enabled. No matching AddressSanitizer, LeakSanitizer or UBSan diagnostic was found in the complete retained log.

Nested Python skips are **unknown in this sanitizer capture**: `ctest --output-on-failure` suppresses successful Python stdout. They are not silently counted as zero or borrowed from native runs.

## Evidence and limits

Machine-readable record: `a48/supporting-ci.json`; authentic run/jobs/artifact API objects, complete decoded producer logs, archive inspection results and reproducible inspection scripts are retained alongside it. API metadata and producer timestamps explicitly resolve nonmonotonic artifact-ID ordering.

Synthetic fixtures, Fourier/numerical references and historical byte-compatibility checkpoints are software evidence. They do not compare the finished presets with physical instruments, establish a realism score, qualify native installation bundles, or replace the separate 13-lane pluginval contract.
