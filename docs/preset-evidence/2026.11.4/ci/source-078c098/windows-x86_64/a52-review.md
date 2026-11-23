# A52 — Windows installation and native evidence

## Result

**Windows installation bytes and original native Test evidence verified and bound. Overall release qualification remains open.** No source or workflow changes, external mutations, CI reruns, replacement tests, plug-in execution, or synthetic product evidence were performed.

- Source: `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`.
- Native workflow run: `45246863788`, attempt `1`.
- Producer job: `137945805240`, Windows / VST3 + Standalone, completed **success** at `2026-11-23T19:54:22Z`.
- Original Test step: **success**, `19:34:17Z` through `19:54:11Z`.
- Authentic run and job API records, checkout source in producer log, and source-pinned workflow agree. The Test step executes CTest directly without `continue-on-error`. No raw numeric CTest exit line is separately printed; the successful original step and exact workflow are retained as exit evidence.

## Authentic uploaded artifacts

All three actual downloaded ZIPs match their complete GitHub API size and SHA-256. Their API source/run, unexpired state, creation and expiry, producer job/attempt, successful upload step, and logged upload name/ID/size/digest/timestamps are bound in `a52-review.json`. Expiry for each is `2027-02-21T19:14:35Z`.

| Artifact | ID | Bytes | SHA-256 | Upload completion UTC |
|---|---:|---:|---|---|
| `cs80-windows-install-bundle` | 14856804857 | 6361027 | `22f5037782789b061dc48a658c5ab08400a85019e050c2a26c4f3042eccd6b4b` | `2026-11-23T19:54:18.3587697Z` |
| `cs80-windows-test-evidence` | 14856849959 | 63288 | `118b9c3ff9988ae8474540c96492d2171c438a84ed6fe2fdadb62beedc1cd8e7` | `2026-11-23T19:54:19.5722425Z` |
| `cs80-windows-vst3` | 14857530802 | 3186363 | `0d69558a9b2e67d45922cd078c0b82d3c1720f1562c8ed18c76b40bfc72e99f5` | `2026-11-23T19:54:13.7001358Z` |

## Installation verification

The prescribed `verify-install-artifact.py` command exited **0**. Its report status is `artifact_bytes_verified`: all 20 inventory entries, complete inner checksums, exact source/revision/run, JUCE `8.0.11` at `c352e2489000ffeb9e397051377d75eef2efe641`, clean source evidence, and both native PE32+ x86_64 headers passed. The package/inner-verification source scripts were compared byte-for-byte to the exact source commit.

- Inner distributable: `cs80-reference-windows-x86_64-078c0981ffa1.zip`, 6,355,711 bytes, SHA-256 `2fd754face192645e53a26f0568d26ff6ff14207a7c3579515b50d7de20030c2`.
- VST3 binary: 7,348,736 bytes, SHA-256 `9ecd532fa1b59d33e32ada5010acd246b6b0e5905b1b7035d4d78a53350d3aa3`, PE32+ machine `0x8664`, DLL characteristics `0x2022`.
- Standalone executable: 7,549,440 bytes, SHA-256 `a5a299c21ff27f5bd34e00852842c5f02d528efaf6d50f13081752af11b929fa`, PE32+ machine `0x8664`, characteristics `0x22`.
- This helper did not inspect signing or perform release signing.

## Native CTest evidence

The prescribed A45 parser exited **0** and reconciled actual JUnit testcases with complete `LastTest.log`. There are **229 registered CTest cases: 227 passed, 0 failed, 2 formal skips, 0 disabled, 0 unknown**. The original producer console reports 1,193.52 seconds total real test time. The parser's report remains scoped to observations and does not claim release qualification.

The two formal skipped cases are `cs80_filter_batch_cap_tests` and `cs80_stereo_pra_exact_tests`, each with JUnit message `SKIP_RETURN_CODE=77`. CTest's console headline “100% tests passed” includes these formal skips; this review does not count them as passed.

**Eleven nested Python skips** are separate from CTest cases and are counted once from terminal unittest summaries, not duplicated from JUnit output or individual reason lines:

| Enclosing CTest case | Nested Python skips |
|---|---:|
| `cs80_packaging_notice_tests` | 1 |
| `cs80_windows_installer_preflight_tests` | 1 |
| `cs80_validator_diagnostic_selection_tests` | 1 |
| `cs80_preset_capture_tests` | 1 |
| `cs80_render_cli_tests` | 3 |
| `cs80_truth_render_tests` | 2 |
| `cs80_vca_reference_tests` | 1 |
| `cs80_float_wav_tests` | 1 |

No assertion counts are inferred from these execution counts.

## Exact local records and handoff

- Bound review JSON: `/workspace/scratch/dde4ad85ed5b/a52-review.json`.
- Installation report: `/workspace/scratch/dde4ad85ed5b/release-078c098/verified/windows-x86_64-install.json`.
- Native observation report: `/workspace/scratch/dde4ad85ed5b/release-078c098/verified/windows-x86_64-native-observations.json`.
- Original job API/log: `input/windows-x86_64-native-job-api.json`, `input/windows-x86_64-native-job.log` under `/workspace/scratch/dde4ad85ed5b/release-078c098`.
- Original run API/source workflow: `input/windows-x86_64-native-run-api.json`, `input/windows-x86_64-native-workflow.yml`.
- Native parser inputs: `input/windows-x86_64-native-inputs.json`.
- Install ZIP/API: `input/windows-x86_64-install.zip`, `input/windows-x86_64-install-api.json`.
- Test ZIP/API: `input/windows-x86_64-test-evidence.zip`, `input/windows-x86_64-test-evidence-api.json`.
- **Worker55 handoff:** `input/windows-x86_64-vst3-validator-input.zip`, `input/windows-x86_64-vst3-validator-input-api.json`; consumer job `137969986229` was queued in the observed jobs API. The validator input has four actual members: `.lib`, `.exp`, VST3 `moduleinfo.json`, and VST3 PE payload. This worker verifies its authentic outer bytes and upload provenance only.

Worker55 must qualify original Windows randomized/fixed-seed pluginval exits and distributed VST3 identity. All original native platform results, all 13 validator lanes, and supporting qualification gates remain mandatory. Nothing here qualifies the overall release or substitutes a fixture/replay for the original native gate.
