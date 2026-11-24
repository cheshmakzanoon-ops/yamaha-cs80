# A31 — macOS validator and payload mapping for 2026.11.7

These commands reuse the frozen `.6` A51/A52 helpers unchanged. They perform **offline observation and input consistency checks**. Their outputs do not qualify a platform or release, authenticate supplied API JSON, prove an original producing attempt, or supply original process exits. A44 owns the actual Apple Silicon decision; A45 owns Intel. The [A31 review](../../reviews/a31.md) explains the required original-execution and complete-bundle gates.

## Fixed context and exact platform mapping

- Source: `c5549f7d095a5c5f49a5ad22ec0ccc819245d201`.
- Revision: `2026.11.7`; native run `45385716988`; attempt `1`.
- Compiled source is this commit. Later documentation/screenshot-only main commits are not replacement binary identities.
- Five lanes are required on **each** Mac: VST3 headless, AU headless, AU GUI random, AU GUI `0x2b4eef5`, AU GUI `0x3c99748`.
- The complete cross-platform matrix is **15 lanes**: Linux 3, Windows 2, Apple Silicon 5, Intel 5.

| Argument or authenticated name | Apple Silicon | Intel |
| --- | --- | --- |
| A51 `--expected-architecture` | `arm64` | `x86_64` |
| A52 `--target` | `macos-arm64` | `macos-x86_64` |
| Original runner | `macos-15` | `macos-15-intel` |
| Native artifact | `cs80-macos-15-test-evidence` | `cs80-macos-15-intel-test-evidence` |
| Validator report artifact | `cs80-pluginval-macos-15` | `cs80-pluginval-macos-15-intel` |
| Install artifact | `cs80-macos-15-install-bundle` | `cs80-macos-15-intel-install-bundle` |
| Required Mach-O CPU | `0x0100000c` | `0x01000007` |

The authenticated names and actual bytes establish the lane context. Renaming a local ZIP does not. Supply each artifact's complete single API object from the authenticated collection, with its original acquisition record; do not pass the collection wrapper. The artifacts and package manifest do not themselves establish the producing attempt.

## Actual ARM archive consistency command

Run from the repository root. The retained [A31 inspection](macos-arm64-a51-inspection.json) was produced with the following inputs and the same arguments; use a **new output path** when repeating it. The example output's parent directory must already exist.

```bash
python3 -B docs/preset-evidence/2026.11.6/verification/a51/verify-macos-binding.py \
  --repo . \
  --expected-source c5549f7d095a5c5f49a5ad22ec0ccc819245d201 \
  --expected-run 45385716988 --expected-attempt 1 \
  --expected-architecture arm64 --expected-revision 2026.11.7 \
  --install-zip /tmp/cs80-ci-1cee5a/c5549f7d/macos-arm64-install.zip \
  --install-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-arm64/install-api.json \
  --pluginval-zip /tmp/cs80-ci-1cee5a/c5549f7d/macos-arm64-validator.zip \
  --pluginval-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-arm64/validator-api.json \
  --out /absolute/new-evidence/macos-arm64-payload-inspection.json
```

A51 loads package/inventory verifiers from the explicitly requested source commit. It checks complete packaged inventories, declared source/revision/run, advertised native architecture, the AU executable against both original pre/post hash records, all five console input paths and both fixed seeds. It deliberately leaves `validated_bundle_identity_established`, `run_attempt_bound` and `release_qualified` false. Its path/seed-only fixtures contain no successful validator result.

## Parse and bind the actual native/validator ZIPs

The existing [A29 ARM inputs](../a29/macos-arm64-inputs.json) contain the actual native and validator ZIP paths. This is a one-target observation: omit `--require-complete`. For a final cross-platform observation, supply all four native targets and all four validator targets and require completeness. A one-target zero exit cannot establish the other ten validator lanes.

```bash
python3 -B docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py \
  --inputs docs/preset-evidence/2026.11.7/verification/a29/macos-arm64-inputs.json \
  --out /absolute/new-evidence/macos-arm64-observations.json

python3 -B docs/preset-evidence/2026.11.6/verification/a52/bind-parser-archives.py \
  --repo . \
  --source c5549f7d095a5c5f49a5ad22ec0ccc819245d201 \
  --run-id 45385716988 --attempt 1 --target macos-arm64 \
  --observations /absolute/new-evidence/macos-arm64-observations.json \
  --native-zip /tmp/cs80-ci-1cee5a/c5549f7d/macos-arm64-native.zip \
  --native-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-arm64/native-api.json \
  --validator-zip /tmp/cs80-ci-1cee5a/c5549f7d/macos-arm64-validator.zip \
  --validator-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-arm64/validator-api.json \
  --out /absolute/new-evidence/macos-arm64-parser-archive-binding.json
```

The binder pins the A52 parser and binds each recorded member to the actual ZIP/API bytes. It does not regenerate caller-written summary fields or enforce result acceptance. Generate observations with the pinned parser, preserve their provenance, and independently compare the original command/console/report and process outcome. A44 also reparses the actual console/report summaries when joining its original-execution receipt.

For Intel, use `x86_64`/`macos-x86_64` and **all three Intel archives and their own original API/log/step records**, using the mapping above. No Intel archive or result was accepted in this A31 review. The same five console/report basenames apply, including `pluginval-au-seed-c76-regression.log` and `pluginval-reports/cs80-au-seed-c76-regression`.

## Mandatory original-execution join

Require the original job/step/source/attempt identities and successful upload IDs, sizes, digests and timestamps. On each Mac, match all five original source-pinned commands and full console/report content, official pluginval `1.0.4`, strictness `10`, `523` programs, expected GUI flags, exactly one invocation per lane, the two fixed seeds, explicit terminal success and normal-zero process termination. `set -euo pipefail`, `--validate-in-process`, the unsuppressed `tee` pipeline and subsequent success check are material: text `SUCCESS` alone cannot establish normal termination.

For AU, join both successful original `diff -qr "$AU_SOURCE" "$AU_DEST"` steps, source signing before copying, the exact pre/post hash output and absence of intervening rebuild/signing/product mutation. The two `.sha256` files cover only the executable. The recursive comparisons and reviewed source/order supply the ordinary resource-content connection to the complete packaged inventory. They do not attest permissions, ownership, ACLs, extended attributes or signature validity.

For VST3, join its validated build-tree path to the source-pinned packager's same input and ordered successful steps with no intervening product mutation. There is no independent Mac VST3 product archive or before/after VST3 tree hash. Standalone has its own inventory/native evidence; it is not a sixth pluginval lane.

## Existing fixture verification

```bash
python3 -B docs/preset-evidence/2026.11.6/verification/a51/test-macos-binding.py
python3 -B docs/preset-evidence/2026.11.6/verification/a52/test-evidence.py
```

All **22 A51** and **7 A52** methods passed in this review, recorded in [a51-fixtures.log](a51-fixtures.log) and [a52-fixtures.log](a52-fixtures.log). They include both architecture mappings, altered resources/architecture/API/source/run identities, absent/wrong/duplicate fifth lanes, both fixed seeds, GUI/start/strictness/program/success failures, preserved failures/retries and output preservation. These are inert fixtures, separate from native CTest totals and actual validator invocations. No additional synthetic tests or production changes were needed.

The [inspection record](inspection.json) retains exact helper/source/input hashes, lane mappings, original-log locations and the scope limits. A51/A52 and their test bytes match those in the frozen `.7` source commit; no historical helper was edited.
