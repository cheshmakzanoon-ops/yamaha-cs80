# A56 — Original Windows/Linux validator and VST3 payload qualification

## Result and scope

**Worker56's assigned scope is complete: all five original Windows/Linux pluginval lanes and both complete distributed-to-validator-input VST3 bindings are verified.** There were **five original validator invocations, zero validation retries, and zero validator warnings**. The machine-readable review sets `complete_assigned_windows_linux_scope: true` and retains **`release_qualified: false`** because broader release qualification is outside this worker's scope.

- Exact source: `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`.
- Original build run: **45246863788**, attempt **1**, `.github/workflows/build.yml`, push event. The final authentic run API reports **completed / success**.
- Windows producer **137945805240**, consumer **137969986229**: both completed **success**.
- Linux producer **137945804596**, consumer **138074711564**: both completed **success**. The Linux consumer finished at **2026-11-23T22:57:25Z**.
- The accepted Windows findings were retained unchanged from the earlier checkpoint and were not rerun on resumption.

The work read original uploaded evidence and ran read-only evidence parsers/binding helpers. It made no product/source/workflow edits, external mutations, CI reruns, plug-in executions, or diagnostic validator reruns. Native CTest case counts and installation qualification remain the separate A52 Windows and A53 Linux reviews. A58 owns the audition evidence.

## Five original validator lanes

All five lanes used **pluginval 1.0.4**, **strictness 10**, and reported **523 programs**. Each console has exactly one terminal `SUCCESS`, and each original workflow step succeeded.

| Platform / original lane | Actual seed | Original successful step interval, UTC | Editor coverage | Original invocations | Validation retries |
|---|---|---|---|---:|---:|
| Windows randomized headless | `0xdcb650` | `20:17:07Z`–`20:17:12Z` | Headless; no Editor test started | 1 | 0 |
| Windows fixed-seed headless | `0x5037a0c` | `20:17:12Z`–`20:17:17Z` | Headless; no Editor test started | 1 | 0 |
| Linux randomized headless | `0x588d540` | `22:56:02Z`–`22:56:05Z` | Headless; no Editor or Editor Automation test | 1 | 0 |
| Linux randomized GUI under Xvfb | `0x38562f0` | `22:56:05Z`–`22:56:23Z` | Editor and Editor Automation started and completed | 1 | 0 |
| Linux fixed-seed GUI under Xvfb | `0x2da72db` | `22:56:23Z`–`22:56:39Z` | Editor and Editor Automation started and completed | 1 | 0 |

Dates for every interval are **2026-11-23**. The exact original step names and metadata are retained in JSON. The fixed-seed lanes supplement their randomized lanes; neither replaces an independent randomized result.

Each artifact console exactly matches its original timestamp-stripped runtime segment. Each report copy is identical after removing the console's one `Started validating:` selected-path header and normalizing line endings. Console/report copies count as **one invocation per lane**. All five selected the exact platform artifact's `vst3/CS-80 Reference.vst3` bundle, using the Windows path separator in that consumer.

### Original process exit binding

**Windows:** both original PowerShell steps immediately check `if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }` after the native pipeline, then require the explicit `SUCCESS` marker. The exact source contains no `continue-on-error`. The successful original steps therefore bind exit zero without suppressing a nonzero native exit. Windows does **not** separately print a raw numeric child termination record; this evidence limit remains explicit in JSON. A crash or nonzero exit after printing `SUCCESS` would fail the original step and cannot pass this binding.

**Linux:** every original core log is byte-for-byte represented in the original consumer's core-display output. The raw termination and normalized status records agree, and the final original child exits are direct observations:

| Original Linux lane | Original child PID | Raw return code | Termination kind | Recorded exit code |
|---|---:|---:|---|---:|
| Headless | 2685 | 0 | `exit` | 0 |
| GUI | 2716 | 0 | `exit` | 0 |
| Fixed-seed GUI | 2759 | 0 | `exit` | 0 |

The exact source workflow, `run-pluginval-linux.sh`, and `run-pluginval-with-core.py` preserve the original result. The wrapper captures both pipeline statuses; log-capture failure also fails the gate. Final acceptance requires original normal exit zero and explicit success. The Linux raw records independently rule out accepting `SUCCESS` followed by a crash.

### Retry and diagnostic accounting

The original Linux report ZIP has **no `attemptN` files**, and the original consumer has no retry warning or extra validation start. There are exactly three original Linux termination records, one per lane, all normal zero. No original status-139 validation failure occurred.

The reviewed source allows at most three full attempts per Linux lane only for normalized original status **139**, after a started test phase whose last observed phase completed, with **neither `SUCCESS` nor `FAILED`** in the failed log. Each eligible failed console must be preserved before retry. This policy was not exercised in the observed run. Status 139 alone does not establish SIGSEGV, crash ownership, or completion of the entire suite.

Both failure-only Linux validator diagnostic rerun steps—GDB and fault-thread trace—were **skipped** in the original job. The successful diagnostic exit-propagation and fault-thread-shim checks are supporting helper checks, not extra plug-in validation lanes. In particular, the synthetic `/bin/sh` exits recorded in `pluginval-core-fixture.log`, including 7 and 139, are **not original pluginval failures**. The normal GDB fixture log also is not counted as a validator invocation. All twelve original Linux report files are retained unchanged under `a56/linux-original-validator-files/`.

## Version, downloads, warnings, and explicit skips

**Windows version/download:** the original version output and report file both say `pluginval - 1.0.4`. The source-pinned downloader and verified download record agree on official release **188928656**, Windows asset **210999198**, **2,408,590 bytes**, SHA-256 `c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab`. There was **one successful transfer attempt and zero download retries**.

**Linux version/download:** the exact source fetch uses the official `v${PLUGINVAL_VERSION}/pluginval_Linux.zip` URL with original environment value **1.0.4**, and the executed binary's original version output and uploaded file both say `pluginval - 1.0.4`. The original fetch contains **one curl command, one observed transfer attempt, and zero transfer retries**. The configured `curl --retry 3` is a maximum transfer policy; it is not three actual retries and is unrelated to validation retries. The Linux workflow does not retain or hash the downloaded pluginval ZIP, so this review makes **no retained release-asset SHA-256 claim for Linux**.

**Validator warnings:** none in any of the five original validator consoles. Windows retains four Node/action deprecation notices separately. Linux retains four Node deprecation notices and one GitHub Node-runtime warning separately. These are not pluginval warnings or failed tests.

**Explicit skips:** every lane prints exactly one `INFO: Skipping vst3 validator as validator path hasn't been set` notice. Thus this review preserves **five external-VST3-validator skip notices**, one per original invocation. It does not establish independent Steinberg VST3 SDK validator coverage. The three headless lanes do not provide editor-test coverage; the two Linux GUI lanes do. Skipped failure-report/diagnostic workflow steps are administrative outcomes, not additional validator-test skips.

## Authentic artifact, producer, and consumer binding

All six used outer ZIPs match the full authentic artifact API byte count and SHA-256. API source/run/repository, unexpired state, creation/update timestamps, original producer job and run attempt, successful upload step, and original logged name/ID/bytes/digest/upload timestamp are bound. All six expiry timestamps are **2027-02-21T19:14:35Z**.

| Platform / artifact | Artifact ID | ZIP bytes | SHA-256 |
|---|---:|---:|---|
| Windows install bundle | 14856804857 | 6361027 | `22f5037782789b061dc48a658c5ab08400a85019e050c2a26c4f3042eccd6b4b` |
| Windows raw VST3 input | 14857530802 | 3186363 | `0d69558a9b2e67d45922cd078c0b82d3c1720f1562c8ed18c76b40bfc72e99f5` |
| Windows pluginval reports | 14857949768 | 5082 | `2d562aed67ff29bc4bfc14d0bafb2d67b3cba69fe02dd50e79605578de0ce239` |
| Linux install bundle | 14870428441 | 13322687 | `611de438dee91810aaf27b2ac57498b3abbaae9ebdda8fcc57342b0b883ede0e` |
| Linux raw VST3 input | 14869179841 | 6937487 | `6654e44ec8dc91e81ed4193c0ed1a3c0c111c7b66af891f474116d823496d44e` |
| Linux pluginval reports | 14871685246 | 8901 | `25ae00479c490f90d006bfa2ecec4a8153dde0ffe92efe49d13716b6e87deb1f` |

The original consumer download logs bind the exact platform raw-VST3 artifact ID, expected size/digest, and computed downloaded ZIP digest. Both complete selected bundle comparisons use these very artifacts, not a rebuilt or substitute binary.

The Linux original upload completion timestamps are **22:50:55.6223850Z** for raw VST3, **22:52:38.7360736Z** for the install bundle, and **22:57:23.8975230Z** for pluginval reports. Their cumulative uploaded byte totals also match the authentic APIs. Exact Windows upload timestamps remain in the accepted Windows object and historical checkpoint.

The freshly fetched completed original consumer logs are preserved with byte identities:

| Consumer | Log bytes | SHA-256 |
|---|---:|---|
| Windows job 137969986229 | 46107 | `53887c28ada6182438a1cff060e2a547576b51ce702d3e9e3a5b559131ee2baa` |
| Linux job 138074711564 | 81561 | `21fcc603a56e45680d2c2ffd60877e6ce83d8a5212695fefe072faa3869bfbac` |

Full ZIP inventories verify canonical unique member names, supported unencrypted compression and object types, bounded sizes, and non-directory-parent/path collisions. Windows scans also check case-folding collisions. Every member was read completely and its CRC and SHA-256 recorded. The Windows report ZIP contains exactly six expected files; the Linux report ZIP contains exactly twelve. No attempt or diagnostic file is silently ignored.

## Complete tested-to-distributed VST3 equality

The prescribed **source-pinned `verify-validator-payload.py` exited 0 for both platforms**, with status `distributed_vst3_matches_validator_input`. The helper, source workflow, and imported package/inner-verification scripts were matched byte-for-byte to source **078c0981ffa1b5472342e0fb5ab8702bc3bd2889**. The helper SHA-256 is `7afd80e3a3aabd9b49d1573987f0a27408ea1a4e09a25b7543ffc6518a2a3dcd`.

| Selected bundle | Tree entries | Regular files | File bytes | Symlinks | Canonical payload tree SHA-256 |
|---|---:|---:|---:|---:|---|
| Windows x86_64 VST3 | 6 | 2 | 7349879 | 0 | `c6f50b1b11b79b1c61317235eba185c961220bdc9a078a459dd64f3698180196` |
| Linux x86_64 VST3 | 6 | 2 | 17483175 | 0 | `ba1533590073d218e015a33334c416a1cb8b8274e2282c6f2540c3d02b11a491` |

Every native binary, resource file, path, and directory relationship in the selected bundle matches. Parent directory records omitted by Actions are inferred consistently. Archive ownership, timestamps, and normalized permissions are not compared.

| Platform payload file | Bytes | SHA-256 |
|---|---:|---|
| Windows native VST3 binary | 7348736 | `9ecd532fa1b59d33e32ada5010acd246b6b0e5905b1b7035d4d78a53350d3aa3` |
| Windows `moduleinfo.json` | 1143 | `04cdb82881820c40fec098f17ffd857821b6f38ee4dbf5c0290e30f9247f9cd6` |
| Linux native `.so` | 17482080 | `fdd84087d5b580b036ff88608a0faccdbe3c3e76b9dcac9d05808e20839617d0` |
| Linux `moduleinfo.json` | 1095 | `01f8d197948fffeec78d876a71bccad19a5319b33f7f3069d098bb8fcad4a1f4` |

The inner Windows distributable is **6,355,711 bytes**, SHA-256 `2fd754face192645e53a26f0568d26ff6ff14207a7c3579515b50d7de20030c2`. The inner Linux `.tar.gz` is **13,317,366 bytes**, SHA-256 `ed8b2cf399b94813454002c7753be2b23002ecc653b906f82a59cd92dad0993f`.

**Only the known Windows container exception applies:** the raw artifact includes top-level `CS-80 Reference.exp` and `CS-80 Reference.lib`, separately recorded and validated as an AMD64 COFF export object and COFF import archive. Their exclusion is bound to the exact source upload-directory contract and both authentic consumer selections of the bundle subdirectory. No member inside the Windows bundle is excluded. Linux has no excluded container sidecars or other payload exceptions.

## Verification records and handoff

The A45 parser independently observed **two Windows lanes / two invocations / zero retries** in the accepted checkpoint and **three Linux lanes / three invocations / zero retries** on resumption, exiting 0 in both cases. Its reports remain observations and retain their own `release_qualified: false` and unknown process-exit fields; the source/original-step/raw-child qualification is a separate binding in this review. Both prescribed full-payload helper commands and the final Linux binding command exited 0. None executed a product or validator.

Authoritative final records under `/workspace/scratch/dde4ad85ed5b/`:

- `a56-review.json` and `a56-review.md`: complete assigned scope and all five lanes.
- `a56/linux-validator-binding.json`: detailed original Linux job, artifact, lane, raw termination, retry, version, download, warning, and payload bindings.
- `a56/windows-vst3-payload.json` and `a56/linux-vst3-payload.json`: both complete bundle comparisons.
- `a56/windows-validator-observations.json` and `a56/linux-validator-observations.json`: unmodified A45 observation outputs.
- `a56/windows-zip-inventories.json` and `a56/linux-zip-inventories.json`: all inspected ZIP members and byte records.
- `a56/linux-final-api-bindings.json`: final authentic run/job/artifact API snapshot.
- `a56/linux-original-validator-files/`: all twelve exact original Linux report files, including all three raw core logs.
- `a56/qualify_linux_final.py`: reproducible read-only Linux binding and accepted-Windows merge.

The earlier partial review is preserved unchanged as `a56/windows-only-a56-review.json` and `.md`. The earlier `a56/linux-source-contract.json` remains a historical preparation snapshot; `linux-validator-binding.json` supersedes it for actual outcomes. No waiting or partial status from those historical records should be substituted for the authoritative final status.

This review qualifies only the five assigned original Windows/Linux lanes and both complete VST3 payload bindings. It does not independently qualify the overall release, macOS lanes, acoustic equivalence, signing/notarization, independent VST3 SDK validation, or DAW hosting.
