# A53 — Linux installation and original native CTest qualification

**Accepted within this review’s scope:** the actual Linux x86_64 installation archive and original native CTest execution for source `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`, preset library revision **2026.11.4**. This report does not assert overall release qualification; A56 owns Linux validator lanes and payload equality, while A58 owns audition audio qualification.

[Original Linux producer job](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788/job/137945804596), run **45246863788**, **attempt 1**, completed **successfully at 22:52:41 UTC on 2026-11-23**. Its checkout, configured JUCE source, build, native Test, capture, render, packaging, and uploads all succeeded. The original checkout log explicitly prints the full source SHA after `git log -1 --format=%H`.

## Actual installation archive

| Field | Verified value |
|---|---|
| API artifact | **14870428441** — `cs80-linux-install-bundle` |
| Outer ZIP bytes | **13,322,687** |
| Outer SHA256 | `611de438dee91810aaf27b2ac57498b3abbaae9ebdda8fcc57342b0b883ede0e` |
| Inner archive | `cs80-reference-linux-x86_64-078c0981ffa1.tar.gz` |
| Inner archive bytes | **13,317,366** |
| Inner SHA256 | `ed8b2cf399b94813454002c7753be2b23002ecc653b906f82a59cd92dad0993f` |
| Complete payload inventory | **20 entries** |
| Native formats | **VST3 and Standalone**, both verified **ELF64 x86_64**, machine 62 |
| JUCE | **8.0.11**, commit `c352e2489000ffeb9e397051377d75eef2efe641`, clean/PASS provenance |
| API expiry | **2027-02-21 19:14:35 UTC** |

The existing `verify-install-artifact.py` exited **0**. It verified actual outer bytes against the authentic API, complete inner checksums/inventory, embedded and sidecar manifests, exact source/revision/run metadata, JUCE provenance, and native executable headers. The production packager and inner verifier were compared byte-for-byte against the tested source commit. No product was executed during this inspection.

The distributed VST3 executable is **17,482,080 bytes**, SHA256 `fdd84087d5b580b036ff88608a0faccdbe3c3e76b9dcac9d05808e20839617d0`. The Standalone executable is **16,819,576 bytes**, SHA256 `94b125e0ca60b4e75a4061d1667a0268b04803e0823b938dfdf0fa8e430564cf`.

## Original native CTest execution

Actual test-evidence artifact **14871037472** is **142,351 bytes**, SHA256 `d90334c62f869abf52a438dc2e1babca68b4751e49aeb948975a17e0ebe5efe5`. Its exact members are `ctest-results.xml`, `Testing/Temporary/LastTest.log`, and `ci-juce-provenance.txt`.

The A45 parser exited **0**, reconciled all JUnit cases with the complete LastTest log, and reported:

- **587 registered CTest executions**;
- **586 passed, zero failed, one formally skipped**;
- **zero disabled or unknown outcomes**;
- **three nested Python unittest skips**, counted separately from CTest executions and internal assertions.

An independent direct XML count agrees. The single formal skip is **`cs80_stereo_pra_exact_tests`**, with `SKIP_RETURN_CODE=77`. Its LastTest entry says `Test Passed`, but the authoritative JUnit skip remains a skip. The three nested Python skips are package binary identity (**1**) and Windows installer preflight (**2**).

The original Test step ran **19:42:31–22:48:21 UTC** and succeeded. JUnit records **11,148 seconds**; the original CTest summary records **11,148.14 seconds**, with `0 tests failed out of 587`. The source-pinned workflow invokes CTest directly, without a masking pipeline, continue-on-error or retry, so the successful original step establishes the process outcome separately from the parser’s text observations.

Both new tine and bowed-body regressions passed, along with direct instrument-body tests, engine integration, the full-library audit, panel golden tests, and UI smoke. Some JUnit stdout sections are truncated; the full **1,120,497-byte LastTest log** supplies the complete corresponding outputs and reconciles every execution. Native CI success is not a claim of exact acoustic realism.

## Original producer and upload binding

`a53/producer-binding.json` binds each exact artifact name, ID, byte count, SHA256, expiry, run and source to the original successful upload step and timestamped log lines. Cumulative `Uploaded bytes` records are permitted, must increase monotonically, and must end at the exact API/ZIP size. Artifact IDs are never assumed to follow upload-time order.

The complete original job log is **457,776 UTF-8 bytes**, SHA256 `a9251e66b8effa0936f9dbeb01f228ee604330117d53992a34e8c1de4bba13a6`. The producer binding, strict installation verifier and native parser all exited **0**. Authentic run/job/artifact APIs and exact command results are preserved.

## Shared evidence handed to other reviewers

The original raw VST3 transport artifact is **14869179841**, `cs80-linux-vst3`, **6,937,487 bytes**, SHA256 `6654e44ec8dc91e81ed4193c0ed1a3c0c111c7b66af891f474116d823496d44e`. Its actual ZIP/API and the verified install ZIP/API are ready for A56. The original dependent Linux validator job is **138074711564**; A53 did not poll or execute that consumer.

The audition artifact is **14870917591**, `cs80-curated-preset-audition-078c0981ffa1b5472342e0fb5ab8702bc3bd2889`, **300,067,470 bytes**, SHA256 `ae8cd5c31e03f0db317852f49a9169ad7ff455e2f385e8a510e8da4569f500ad`. A53 preserved its authentic API and bound the successful capture/upload; A58 independently handled the full audition ZIP/WAV qualification.

## Retention paths

Primary summary: `/workspace/scratch/dde4ad85ed5b/a53-summary.json` (identical copy at `a53/summary.json`). Review: `/workspace/scratch/dde4ad85ed5b/a53-review.md`.

Actual ZIPs and individual API records are under `/workspace/scratch/dde4ad85ed5b/release-078c098/input/` with `linux-x86_64` prefixes. Canonical shared producer files are `linux-x86_64-native-job-api.json`, `linux-x86_64-native-job.log`, and `linux-x86_64-native-run-api.json`.

Strict reports are `/workspace/scratch/dde4ad85ed5b/release-078c098/verified/linux-x86_64-install.json` and `linux-x86_64-native-observations.json`. Full source/API/upload binding, independent native cross-check, acquisition helper, source-tool hashes and command records are under `/workspace/scratch/dde4ad85ed5b/a53/`. `a53/retention-manifest.json` gives exact file paths, sizes and hashes.

No repository source, workflows, tests, or existing verifiers were edited; no CI job was rerun or cancelled. Original package/native scope is accepted; overall release qualification remains a separate aggregate decision.
