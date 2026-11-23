# A56 — Windows validator qualification; Linux pending

## Explicitly partial checkpoint

**Windows is verified within this assignment: both original pluginval lanes and the complete distributed-to-validated VST3 input binding pass. Linux remains pending.** This is **2 of 5 required original lanes** and **1 of 2 required payload bindings** for worker56. `a56-review.json` retains `release_qualified: false` and `complete_assigned_windows_linux_scope: false`.

- Source: `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`.
- Original native workflow: `45246863788`, attempt **1**, `.github/workflows/build.yml`, push event.
- Windows producer: `137945805240`, **success**, completed `2026-11-23T19:54:22Z`.
- Windows validator consumer: `137969986229`, **success**, completed `2026-11-23T20:17:21Z`.
- Linux producer `137945804596` was still `in_progress` in the saved fresh jobs collection. No Linux validator consumer job or original validator artifact was available in that observation. No hang, failure, completion, or rerun is inferred.

This work read original evidence and ran evidence parsers only. It made no repository source/workflow edits, external mutations, CI reruns, plug-in executions, diagnostic validator reruns, or synthetic product evidence. Windows installation/native CTest qualification remains the separate A52 review; those counts were not recomputed here.

## Original Windows lanes

| Lane | Original successful step (UTC) | Actual seed | Strictness | Programs | Original invocations | Validation retries |
|---|---|---|---:|---:|---:|---:|
| Randomized headless | `Validate VST3 headless`, `20:17:07Z`–`20:17:12Z` | `0xdcb650` | 10 | 523 | 1 | 0 |
| Fixed-seed headless | `Validate state regression seed 0x5037a0c`, `20:17:12Z`–`20:17:17Z` | `0x5037a0c` | 10 | 523 | 1 | 0 |

Each artifact console exactly matches its original timestamp-stripped job-output segment. Its report copy is identical after removing the console's one `Started validating:` path header and normalizing line endings. The console and report therefore count as **one invocation per lane**. Both have exactly one terminal `SUCCESS`, no validator failure markers, and no validator warnings. The consumer contains exactly two original validation starts and no validation retry logs.

**Original normal exit zero is bound through the successful original step and exact source exit propagation.** Both PowerShell commands immediately execute `if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }` after the native pluginval pipeline, then require the explicit success marker. There is no `continue-on-error`. A crash or other nonzero native exit after printing `SUCCESS` would fail the original step and cannot pass this binding. Windows does not separately print a raw numeric child termination record; the JSON states that evidence limit. The A45 parser's unknown process-exit fields remain unchanged; the independent binding is recorded separately.

The complete supplied consumer log was fetched again through `github_fetch_workflow_job_logs` for the completed original job and matched **all 46,107 bytes**, SHA-256 `53887c28ada6182438a1cff060e2a547576b51ce702d3e9e3a5b559131ee2baa`. Fresh run/jobs/artifact collection responses also exactly match the saved Windows job and artifact API records.

### Version, download attempts, warnings, and coverage

The original fetch step and artifact version file both report **pluginval 1.0.4**. The download evidence records official `Tracktion/pluginval` release **188928656**, Windows asset **210999198**, **2,408,590 bytes**, SHA-256 `c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab`. The source-pinned downloader constants match these values. There was **one successful transfer attempt, zero download retries**. Download attempts are not added to validation invocations or retries.

Both Windows lanes are headless by contract; neither starts an Editor test. Each explicitly reports `INFO: Skipping vst3 validator as validator path hasn't been set`. These two notices remain recorded as coverage limits. They do not establish independent VST3 SDK validator coverage. Four Node/action deprecation warnings are retained separately from the empty pluginval-warning lists. The workflow's `Report pluginval failure` step was skipped because no failure occurred; that administrative step is not a validator-test skip.

## Authentic artifact and upload binding

The following actual downloaded ZIPs match the full API byte count and SHA-256. Each API record has the expected source/run/repository, `expired: false`, and expiry **`2027-02-21T19:14:35Z`**. Creation/update times fall inside the corresponding original successful upload step. The producing job/attempt and logged upload name, artifact ID, bytes, digest, and completion timestamp are bound in the JSON.

| Artifact | ID | ZIP bytes | SHA-256 | Logged upload completion UTC |
|---|---:|---:|---|---|
| `cs80-windows-install-bundle` | 14856804857 | 6361027 | `22f5037782789b061dc48a658c5ab08400a85019e050c2a26c4f3042eccd6b4b` | `2026-11-23T19:54:18.3587697Z` |
| `cs80-windows-vst3` | 14857530802 | 3186363 | `0d69558a9b2e67d45922cd078c0b82d3c1720f1562c8ed18c76b40bfc72e99f5` | `2026-11-23T19:54:13.7001358Z` |
| `cs80-pluginval-windows` | 14857949768 | 5082 | `2d562aed67ff29bc4bfc14d0bafb2d67b3cba69fe02dd50e79605578de0ce239` | `2026-11-23T20:17:18.0806446Z` |

The successful original consumer download selects `cs80-windows-vst3`, artifact **14857530802**, and prints the matching **3,186,363-byte** expected identity and computed download digest. Both validation starts select only `vst3\CS-80 Reference.vst3`.

`a56/windows-zip-inventories.json` records every member of all three outer ZIPs and the complete inner Windows installation ZIP. All entries have canonical unique names, supported unencrypted compression/object types, bounded sizes, no parent-file/path collisions, and no Windows case-folding collisions. Every member was read completely, its CRC checked by the ZIP reader, and its SHA-256 recorded. The pluginval report ZIP has exactly the expected six files; no retry or extra log member is silently ignored.

## Complete VST3 payload equality

The prescribed source-pinned `verify-validator-payload.py` exited **0**, status **`distributed_vst3_matches_validator_input`**. The helper itself, `.github/workflows/build.yml`, `verify-ci-bundle.py`, and `package-ci-artifacts.py` were compared byte-for-byte with the exact source commit. The helper SHA-256 is `7afd80e3a3aabd9b49d1573987f0a27408ea1a4e09a25b7543ffc6518a2a3dcd`.

- Inner distributable: `cs80-reference-windows-x86_64-078c0981ffa1.zip`, **6,355,711 bytes**, SHA-256 `2fd754face192645e53a26f0568d26ff6ff14207a7c3579515b50d7de20030c2`.
- Complete selected VST3 tree: **6 entries**, **2 files**, **7,349,879 file bytes**, **0 symlinks**. Omitted parent directory records are inferred consistently.
- Canonical payload tree SHA-256: **`c6f50b1b11b79b1c61317235eba185c961220bdc9a078a459dd64f3698180196`**.
- Native VST3 binary: **7,348,736 bytes**, SHA-256 `9ecd532fa1b59d33e32ada5010acd246b6b0e5905b1b7035d4d78a53350d3aa3`.
- `Contents/Resources/moduleinfo.json`: **1,143 bytes**, SHA-256 `04cdb82881820c40fec098f17ffd857821b6f38ee4dbf5c0290e30f9247f9cd6`.

The raw validator-input container also has the exact top-level compiler sidecars `CS-80 Reference.exp` (**968 bytes**, a valid AMD64 COFF export object) and `CS-80 Reference.lib` (**2,266 bytes**, a valid COFF import archive). Their hashes and format checks are retained separately. Their exclusion is permitted only because the source uploads the whole VST3 output directory, while both authentic consumer invocations select its VST3 bundle subdirectory. No bundle member is excluded from equality. Archive ownership, timestamps, and normalized permissions are outside this payload comparison.

## Linux continuation contract

`a56/linux-source-contract.json` and the exact source snapshots under `a56/` prepare the remaining checks. The required original Linux lanes are randomized headless, randomized GUI under Xvfb, and GUI fixed seed **`0x2da72db`**, all pluginval **1.0.4**, strictness **10**, and **523 programs**. Original job/step outcomes, report/console equality, API/upload/download identities, selected path, final original child termination, and complete distributed payload equality still need the authentic final Linux inputs.

The source Linux wrapper permits at most **three full attempts** per lane only when the original normalized exit is **139**, a test phase was started and the last observed phase completed, and the failed log contains **neither `SUCCESS` nor `FAILED`**. Each failed console must be preserved as contiguous `attemptN` logs. The core helper records the original child PID and raw termination, preserving the child's result; final acceptance requires **raw return code 0, kind `exit`**. Status 139 alone does not establish SIGSEGV, crash ownership, or completion of the whole suite. A `SUCCESS` followed by a crash is a failure and is not eligible for this retry policy. Diagnostic reruns cannot replace any original lane. The curl download retry policy must be counted separately from actual validator retries.

No original Linux lane, retry count, payload binding, or process outcome is asserted at this checkpoint. Worker56 should resume when the original Linux producer artifacts and validator consumer job finish.

## Review records

- Main machine-readable checkpoint: `/workspace/scratch/dde4ad85ed5b/a56-review.json`.
- Full payload result: `/workspace/scratch/dde4ad85ed5b/a56/windows-vst3-payload.json`.
- Unmodified A45 observations: `/workspace/scratch/dde4ad85ed5b/a56/windows-validator-observations.json`.
- Complete ZIP inventory: `/workspace/scratch/dde4ad85ed5b/a56/windows-zip-inventories.json`.
- Fresh source/run/job/artifact binding records: `a56/fresh-native-run-api.json` and `a56/fresh-windows-api-bindings.json`.
- Read-only reproducible binding script: `/workspace/scratch/dde4ad85ed5b/a56/qualify_windows_checkpoint.py`.
- Linux contract: `/workspace/scratch/dde4ad85ed5b/a56/linux-source-contract.json`.

This checkpoint does not qualify the overall release, acoustic equivalence, signing/notarization, independent VST3 SDK validation, or DAW hosting.
