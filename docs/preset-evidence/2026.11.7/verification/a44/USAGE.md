# A44 — actual `.7` Apple Silicon qualification

The [platform receipt](../../ci/source-c5549f7d/macos-arm64/platform-qualification.json) accepts only frozen source `c5549f7d095a5c5f49a5ad22ec0ccc819245d201`, tree `469244af34233cfa145537266efd29024b66abba`, run `45385716988`, attempt 1, original ARM job `138415961176`. It leaves `cross_platform_qualified` and `release_qualified` false. See [A44's review](../../reviews/a44.md) for the acceptance reasoning, original process-exit basis, AU/VST3 payload scope and remaining work.

## Unchanged helpers exercised on actual archives

Both commands succeeded. Their actual retained output paths below now exist and must not be reused as overwrite destinations. To reproduce, choose new output paths.

```bash
python3 -B docs/preset-evidence/2026.11.5/verification/a41-verify-install-artifact.py \
  --repo /workspace/scratch/1cee5a9391a9/yamaha-cs80 \
  --source c5549f7d095a5c5f49a5ad22ec0ccc819245d201 \
  --revision 2026.11.7 --run-id 45385716988 --attempt 1 \
  --run-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-arm64/attempt1-api-at-platform-completion.json \
  --platform macos --architecture arm64 \
  --archive /tmp/cs80-ci-1cee5a/c5549f7d/macos-arm64-install.zip \
  --artifact-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-arm64/install-api.json \
  --out docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-arm64/install-acceptance.json

python3 -B docs/preset-evidence/2026.11.6/verification/a51/verify-macos-binding.py \
  --repo /workspace/scratch/1cee5a9391a9/yamaha-cs80 \
  --expected-source c5549f7d095a5c5f49a5ad22ec0ccc819245d201 \
  --expected-run 45385716988 --expected-attempt 1 \
  --expected-architecture arm64 --expected-revision 2026.11.7 \
  --install-zip /tmp/cs80-ci-1cee5a/c5549f7d/macos-arm64-install.zip \
  --install-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-arm64/install-api.json \
  --pluginval-zip /tmp/cs80-ci-1cee5a/c5549f7d/macos-arm64-validator.zip \
  --pluginval-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-arm64/validator-api.json \
  --out docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-arm64/archive-payload-binding.json
```

A41 retains its generic false qualification/attempt/native flags. Its inherited thirteen-lane wording is historical; the current requirement is **15**. A51 retains its limited false run-attempt/validated-bundle/release fields. The platform receipt closes those particular ARM external gates through the original successful job, original command/output binding, full directory comparisons and installation/document evidence. It does not change the helpers' meaning or edit their outputs.

## Replay the fixed original acceptance

`qualify-macos-arm64.py` is deliberately bound to the actual reviewed acquisition and dependent receipts. It is not a generic target/revision switch. It rejects changed retained inputs, helper bytes, source tree, archive bytes, missing/failed original steps, command or output drift, inconsistent upload identities/times, native names/results, validator outcomes, payload bindings, or the independent A30 document receipt. Offline hashes preserve the root's authenticated acquisition; they do not independently authenticate a caller-provided API object.

The three downloads must be named `macos-arm64-native.zip`, `macos-arm64-validator.zip` and `macos-arm64-install.zip` beneath `--archives`. A newly downloaded archive is usable only if its bytes match the exact accepted artifact. The retained input/API/A29/A30 paths resolve from `--repo` or the explicit document argument. No downloaded product or validator executes.

Use a fresh output directory for replay:

```bash
python3 -B docs/preset-evidence/2026.11.7/verification/a44/qualify-macos-arm64.py \
  --repo /workspace/scratch/1cee5a9391a9/yamaha-cs80 \
  --archives /tmp/cs80-ci-1cee5a/c5549f7d \
  --documents-receipt docs/preset-evidence/2026.11.7/verification/a30/macos-arm64-docs-and-notices.json \
  --out-dir /tmp/cs80-a44-1cee5a/replay-new
```

Success produces `original-execution-binding.json` followed by `platform-qualification.json`, with the latter binding the exact proof bytes. Existing output files or symlinks are refused. The final retained copies are in the ARM CI evidence directory; preserve them. Source-qualification changes require a separately reviewed successor, not relabeling these inputs or weakening their pins.

The original-execution proof deliberately has `platform_qualified: false`: it establishes the execution side only. The final platform receipt additionally joins A30's exact source-specific document/notice acceptance and is the concrete `platform_qualified: true` decision. Both retain false cross-platform and release fields.

## Verification retained here

The actual preflight checked all 13 executed source workflow command bodies, all three original artifact uploads, all 277 ordered native console rows, five full console/report pairs and their pinned-parser summaries, both full AU comparisons, absence of intervening bundle mutation, all three ARM64 binaries, and the independent documentation/notice supplement. A31 separately reviewed the acceptance boundary.

`guard-checks.json` preserves the bounded guards on the preflight qualifier. The only subsequent qualifier change added the explicit `diff -qr` metadata/signature limitation; `final-guard-checks.json` repeats the same three guards against the final qualifier. These checks mutate temporary copies only. A changed ZIP byte, changed independent source receipt and existing-output replacement all fail without publishing a replacement result. They are local integrity guards, not new native execution or synthetic product qualification.

The complete-directory comparisons prove ordinary-file content equality; they do not prove permissions/ownership/ACL/xattr equality or signature validity. Packaged modes are checked separately. The VST3 proof is the original ordered source path through validation and the read-only packager, with no intervening mutation; there is no separately recorded VST3 tree hash before and after validation.
