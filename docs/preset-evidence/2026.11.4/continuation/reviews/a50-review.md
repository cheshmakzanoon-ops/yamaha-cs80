# A50 — final screenshot qualification

**Passed for source `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`.** The original screenshot workflow [45246863473](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863473), attempt 1, and job `137945802978` completed successfully. No rerun, golden refresh or relaxed comparison was used.

## Actual artifact and producer binding

- Artifact: `14856641950`, `cs80-panel-078c0981ffa1b5472342e0fb5ab8702bc3bd2889`.
- Actual downloaded ZIP: **62,117,315 bytes**.
- SHA-256: `310ce4aecec7f84130ea95c35443b62950a5f9d1a473ba47f5cb8800e68e36e1`.
- API size/digest match the actual ZIP and the original producer's upload lines. API run/head SHA, job/run attempt, checkout, embedded provenance and artifact timestamps agree. Artifact creation at `2026-11-23T19:37:41Z` falls within the successful upload step and producer job.
- All archive entries have canonical unique paths and supported regular-file/directory types. The archive contains **73 files**, including **66 top-level PNGs**, four golden candidate/diff PNGs, the smoke log, provenance and checksums. Every top-level PNG matches its declared checksum; the outer API digest binds all members.

## Original UI result and independent pixel check

The full retained smoke log reports **`UI SMOKE TEST PASSED`** and contains no failed assertions. The formerly failing contour assertion now observes the exact canonical restored value `0.0249999761581`, matching its canonical expectation while retaining the authored value `0.0250000003725` in the diagnostic.

| Fixed panel fixture | Reported changed pixels | Independent changed pixels | Worst 8×8 block mean | Actual PNG equals source golden |
| --- | ---: | ---: | ---: | --- |
| 1600 × 1000 | 0.0000% | 0.0000% | 0.00 | Yes, byte-for-byte and all decoded pixels |
| 1120 × 700 | 0.0000% | 0.0000% | 0.00 | Yes, byte-for-byte and all decoded pixels |

The complete comparator and workflow remain byte-identical to `bcc86aa`; both golden PNGs are unchanged. Original Linux thresholds remain channel delta 40, changed fraction at most 0.003 and maximum 8×8 block mean 8.0. Independent calculations from the downloaded candidate pixels reproduce both logged metrics. The original live-readout mask is unchanged, and the images also match inside that mask.

Both actual candidate images were visually inspected against the source goldens at their complete 1600×1000 and 1120×700 sizes. Control positions, labels, boundaries and keyboard layout match; no visible displacement, clipping or missing elements was found. This visual review covers these two fixed layout fixtures, not every screenshot page.

## Source history and limits

The preceding `bcc86aa` screenshot run `45244531816` remains a failed historical result with three failed assertions. Its excerpts and original-log hash are retained separately; this later success does not relabel it.

The successful workflow published a documentation-only screenshot update beginning `4beaa66` with `[skip ci]`. The screenshot source remains `078c0981`; later documentation commits do not change the qualified renderer input. Root independently checked production-source equivalence after fetching that update.

This is supporting UI/artifact qualification. It does not qualify native installation packages, pluginval lanes, all DAWs or acoustic realism. No repository code, tests, workflows, PNG goldens or tolerances were changed by this worker.

## Retained evidence

`a50/acceptance-summary.json` contains the actual acceptance result, complete artifact API identity, native pixel metrics and visual-review scope. `actual-artifact-members.json` records all 73 member sizes and SHA-256 digests. Raw `final-run.json`, `final-jobs.json`, `artifact-api.json`, `job.log`, `source-contract.json` and historical-failure records preserve provenance. `verify_actual_artifact.py` records the read-only reproducible check. The full ZIP and extracted images remain scratch evidence and need not be added to Git.
