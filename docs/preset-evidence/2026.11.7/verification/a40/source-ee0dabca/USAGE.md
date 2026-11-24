# A40 — ee0dabca source continuity and prepared acceptance successors

**Prepared only. No ee0dabca native or audition archive was accepted by this assignment.** All preparation qualification flags remain false. These helpers use revision `2026.11.7`, source `ee0dabcacb4cdff93f8803844f60348e0ceda6ed`, tree `17115913cbb29157bd00539485bcc5f38b57219c`, native run `45397189410`, attempt `1`.

| Original producer | Job ID | Required native names |
| --- | ---: | ---: |
| Linux x86_64 | 138455975798 | 613 |
| Windows x86_64 | 138455976256 | 255 |
| Apple Silicon | 138455976227 | 277 |
| Intel Mac | 138455976307 | 277 |

The source-specific outputs belong under `docs/preset-evidence/2026.11.7/ci/source-ee0dabca/`. Root's download directory is `/tmp/cs80-ci-1cee5a/ee0dabca/`. The paths in a future input manifest must name the original downloaded files and retained API records actually acquired by root. No artifact ID, size, digest, or successful outcome is inferred from these prepared paths.

## Frozen helper identities

Paths below are relative to this directory. [helper-hashes.json](helper-hashes.json) records complete repository paths and all dependency/test hashes.

| Helper | SHA-256 |
| --- | --- |
| `verify-native-contract.py` | `cb9597061fe16b590f85cfc75ddbc98e5868c80d1a9d6763f77edfc71b27e32f` |
| `bind-audition-ci.py` | `65cf483ee4397533ea5ca4bf8d88eb4c5f21353059b99ba761833f7f288e2554` |
| `verify-audition-archive.py` | `bae2c82d291bee1240a3c49673a82381114f9bf4c0260275ea1b4494fda59df5` |

The native helper changes only A29's fixed source, tree and run values. The audition binder changes only A28's fixed source, tree, run and Linux producer job values. Their function ASTs are identical to their predecessors. The generic audition content verifier and CI fixture suite are byte-exact copies. [helper-differences.patch](helper-differences.patch) and [helper-differences.json](helper-differences.json) enumerate every difference, including companion tests' source/location/temp-directory context.

**Always pass `--repo .` to the content verifier from the repository root.** Its byte-exact copy deliberately retains the predecessor's default path-depth assumption; the deeper A40 location requires the explicit argument. The binder imports the adjacent byte-exact verifier through its unchanged `HERE` reference.

## Source continuity and bank identity

[source-continuity.json](source-continuity.json) compares every protected input against both frozen Git objects. All 945 records in root's c554 manifest match c554 Git blobs; ee0dabca has the identical path/mode set, 943 identical files, and exactly two ownership-only harness changes. [source-inputs-ee0dabca.json](source-inputs-ee0dabca.json) retains all 945 new records. The normalization proof reconstructs each old changed file byte for byte after undoing exactly the added `<memory>`, scoped owner/reference declarations, and one audit comment.

The entire core, plugin, reference, vendor, experimental, asset, installer, scripts, workflow and CMake helper trees retain their identities. CMake itself, the curated audition producer and `preset_capture.cpp` retain exact bytes. The complete tree comparison has 373 changed paths: the two harness files plus 371 evidence/screenshot paths. Additional documentation hashed by offline tools and the packaged guides/licenses also remain unchanged.

A12's independent inventory is identical in both frozen sources and the retained reference: **523 ordered programs × 204 parameters = 106,692 values**, including identities, descriptions, tags, ordered IDs and float32 bits. Its 710,464 bytes have SHA-256 `55c08568074a6560ef10c8c525733a40aec42d9b8c6d20644bf4984cdd0d3e0d`. This source/data result supports retaining audio reviews with their original scope. It does not establish ee0dabca compiled-binary or newly rendered PCM byte identity, and does not transfer any c554 platform/artifact receipt to the new source.

To reproduce the audit, choose an empty output directory:

```bash
python3 -B docs/preset-evidence/2026.11.7/verification/a40/source-ee0dabca/verify-source-continuity.py \
  --repo . --out-dir /tmp/cs80-a40-1cee5a/fresh-source-continuity
```

## Derive and check the complete native contract

```bash
python3 -B docs/preset-evidence/2026.11.7/verification/a40/source-ee0dabca/verify-native-contract.py \
  --repo . --describe --out /tmp/cs80-a40-1cee5a/fresh-native-contract.json
```

[contract.json](contract.json) is that prepared derivation. It preserves every expected name, every platform condition, all 22 common additions through `.6`, and all four `.7` additions: `cs80_piano_partial_decay_tests`, `cs80_body_noise_rate_tests`, `cs80_string_body_formant_tests`, and `cs80_low_choir_spectrum_tests`. The full expected name sets are identical to A29's c554 contract. Matching totals cannot substitute for names.

Copy [native-inputs-template.json](native-inputs-template.json) into the new CI evidence directory and fill the actual original ZIP/API/review paths. Keep the context exactly as prepared. For a complete cross-platform observation, run the unchanged `.6` A52 parser with all four native and all four validator archives:

```bash
python3 -B docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py \
  --inputs docs/preset-evidence/2026.11.7/ci/source-ee0dabca/native-inputs.json \
  --out docs/preset-evidence/2026.11.7/ci/source-ee0dabca/native-observations.json \
  --require-complete
```

Use the unchanged A52 archive binder separately for each target, supplying the actual original native and validator archives/API objects, the observations, and `--source ee0dabcacb4cdff93f8803844f60348e0ceda6ed --run-id 45397189410 --attempt 1`. Its [existing usage](../../../../2026.11.6/verification/a52/USAGE.md) remains authoritative for its CLI. All **15** original validator lanes remain required; binder agreement and the attempt argument do not prove authenticated acquisition, original producer attempt, or unsuppressed process outcomes.

Use [skip-review-template.json](skip-review-template.json) for each target. Review the actual formal and nested Python skips against frozen source and original output before setting `reviewed: true`. Only the two existing capability CTest cases can be formally skipped. All four new tests must pass. Preserve separate formal/nested counts and any limitation from nonverbose output.

```bash
python3 -B docs/preset-evidence/2026.11.7/verification/a40/source-ee0dabca/verify-native-contract.py \
  --repo . --inputs docs/preset-evidence/2026.11.7/ci/source-ee0dabca/native-inputs.json \
  --out docs/preset-evidence/2026.11.7/ci/source-ee0dabca/native-content-contract.json
```

Omitting `--target` requires exactly all four native targets. An explicitly preliminary single-target check can use a one-target manifest and the corresponding `--target`, with a fresh target-specific output path. The helper requires the full exact roster, complete numbered LastTest session/footer, zero failed/unknown/disabled cases, reviewed formal/nested skips, exact JUCE/source text, and the completed UI `instrument body host identity and recall` phase followed by UI success. It leaves native/platform/release acceptance false. Original jobs, uploads, process exits, validators, packages and payloads remain independent gates.

## Prepare and later accept the original audition archive

The unchanged contract is **238 WAVs, 92 selected existing programs, 54 folders, 1,361.2 seconds, 65,337,600 stereo frames, 522,700,800 raw float32 bytes, 297 files, and 296 checksums**. Every complete/preserved/appended roster digest, inventory bit/metadata check, PCM metric tolerance, safety check and archive bound remains unchanged.

Preparation uses the independent A12 inventory, not an inventory extracted from the future archive:

```bash
python3 -B docs/preset-evidence/2026.11.7/verification/a40/source-ee0dabca/verify-audition-archive.py \
  --repo . --source ee0dabcacb4cdff93f8803844f60348e0ceda6ed \
  --run-id 45397189410 --attempt 1 \
  --expected-inventory docs/preset-evidence/2026.11.7/verification/a12/inventory.json \
  --inventory-source-path docs/preset-evidence/2026.11.7/verification/a12/inventory.json \
  --expected-inventory-sha256 55c08568074a6560ef10c8c525733a40aec42d9b8c6d20644bf4984cdd0d3e0d \
  --prepare --out /tmp/cs80-a40-1cee5a/fresh-audition-contract.json
```

[prepared-contract.json](prepared-contract.json) is a preparation result with archive/release acceptance false. After authenticated retrieval of the original source-specific artifact `cs80-curated-preset-audition-ee0dabcacb4cdff93f8803844f60348e0ceda6ed`, run the same command with `--prepare` removed and these actual inputs added:

```text
--archive /tmp/cs80-ci-1cee5a/ee0dabca/ACTUAL_DOWNLOADED_AUDITION.zip
--expected-zip-sha256 ACTUAL_AUTHENTICATED_API_SHA256
--expected-zip-bytes ACTUAL_AUTHENTICATED_API_SIZE
--out docs/preset-evidence/2026.11.7/ci/source-ee0dabca/audition/content-acceptance.json
```

Replace the preparation `--out`; do not supply it twice. Then bind the fresh accepted content report to retained raw API records and the original completed Linux producer log:

```bash
python3 -B docs/preset-evidence/2026.11.7/verification/a40/source-ee0dabca/bind-audition-ci.py \
  --content-report docs/preset-evidence/2026.11.7/ci/source-ee0dabca/audition/content-acceptance.json \
  --artifact-api ACTUAL_ORIGINAL_ARTIFACT_API.json \
  --run-api ACTUAL_NATIVE_RUN_API.json \
  --job-api ACTUAL_COMPLETED_LINUX_PRODUCER_API.json \
  --job-log ACTUAL_ORIGINAL_LINUX_PRODUCER.log \
  --out docs/preset-evidence/2026.11.7/ci/source-ee0dabca/audition/ci-binding.json
```

The binder still requires successful original checkout/build/test/capture/upload steps in order, exact source/tree/run/attempt/job, all content counts and independent inventory agreement, original artifact API identity, unique upload digest/success records, and matching ID/size/hash/time/download URL. Retain authenticated acquisition receipts separately. Both helpers refuse existing output destinations; keep all helper hashes fixed between content checking and CI binding.

## Existing companion fixtures and preserved history

[fixture-results.json](fixture-results.json) records actual exit zero and log hashes for **40 existing methods**: 10 native, 16 audition content, 7 audition CI, and 7 unchanged A52 methods. All fixtures are synthetic and execute no native product binary. The companion tests only change source/location context; no assertion is deleted or relaxed.

[predecessor-files.json](predecessor-files.json) and [preparation-receipt.json](preparation-receipt.json) verify that all 67 snapshotted historical files remain byte-identical, including A28/A29 originals, their c554 accepted ARM receipts, the old CI source directory, A52 parser/binder/tests, and the `.5` A41 install helper and fixtures. Historical success and failure remain attached to c554. A future accepted ee0dabca archive still does not by itself establish listening approval, acoustic equivalence, live gestures, independent possession of the capture executable, or overall release qualification.
