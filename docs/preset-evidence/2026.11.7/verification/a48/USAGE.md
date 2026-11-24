# A48 — fresh-source macOS qualification

**Apple Silicon is platform-qualified; Intel still awaits its own actual artifact review. Cross-platform and release qualification remain false.** The ARM wrapper now pins the actual acquired and reviewed inputs. The Intel wrapper still refuses acceptance. No c554 source receipt is reused as ee0dabca platform acceptance.

The actual [ARM receipt](../../ci/source-ee0dabca/macos-arm64/platform-qualification.json) was generated at **2026-11-24 13:37:47.395340 UTC**. Its [qualification command](macos-arm64/qualification-command.json), [content commands](macos-arm64/actual-content-command-receipts.json), [skip review](macos-arm64/skip-review.json), [actual pin record](macos-arm64/reviewed-input-pins.json), [preflight](macos-arm64/preflight-summary.json) and [actual refusal guards](macos-arm64/actual-input-guards.json) are retained. The full [review](../../reviews/a48.md) explains the original process-exit and payload-binding evidence.

To replay ARM qualification, use its fixed wrapper with `--repo . --archives /tmp/cs80-ci-1cee5a/ee0dabca --documents-receipt docs/preset-evidence/2026.11.7/verification/a48/macos-arm64/docs-and-notices.json --out-dir` followed by a **new empty destination**. Existing accepted destinations are refused. The commands below document the actual content workflow; their ARM outputs now exist and must not be reused as overwrite destinations.

| Context | Required identity |
| --- | --- |
| Source | `ee0dabcacb4cdff93f8803844f60348e0ceda6ed` |
| Tree | `17115913cbb29157bd00539485bcc5f38b57219c` |
| Revision | `2026.11.7` |
| Native run / attempt | `45397189410` / `1` |
| Apple Silicon producer | `138455976227`, `macos-15`, `arm64` |
| Intel producer | `138455976307`, `macos-15-intel`, `x86_64` |
| Native names | 277 exact names per Mac; 275 passes, two individually reviewed formal capability skips |
| Required validators | Five original lanes per Mac; 15 lanes across all four platforms |

`macos-qualification.py` derives A44's successful original-execution checker. Each `qualify-macos-*.py` wrapper pins that engine's bytes and, when actual review completes, its own acquisition, input receipts, artifacts, elapsed time and document supplement. Direct engine execution refuses generic qualification. A wrapper cannot accept the other Mac's original job, architecture or artifact names. Both source/run/platform and independent upload-attempt gates remain mandatory.

The actual original job must complete successfully with unsuppressed native and validator commands. Five terminal `SUCCESS` lines alone do not qualify. Complete command groups must equal the frozen workflow; every archived console must equal its original timestamp-stripped segment, and every report must equal its console after the path line. Pinned A52 summaries are reparsed from actual members. Both complete AU comparisons, signing before copy, and the subsequent package sequence remain required. VST3 binding uses the original source path and sequence, without inventing an independent pre/post tree hash.

The successor adds explicit acceptance of the actual repaired `cs80_library_audit_tests`: passing original CTest/JUnit/LastTest results and the full 301-curated-plus-222-circuit terminal output. It also directly reads each packaged thin 64-bit Mach-O header to check target CPU, bundle/executable file type and `0755` mode. These header checks do not establish a separate Standalone launch or complete load-command validation.

## Actual input sequence

Root supplies original complete API objects, original full job logs and acquisition receipts in `../../ci/source-ee0dabca/<target>/`, and the three matching ZIPs under `/tmp/cs80-ci-1cee5a/ee0dabca/`. The prepared `<target>/inputs.json` files only name those future inputs; they do not assert artifact IDs, digests or outcomes. No downloaded product or validator is executed on this inspection host.

For each available target, use the unchanged `.6` A52 parser on its input manifest, omitting `--require-complete` for this explicitly single-platform scope. Bind the exact native and validator ZIP/API bytes with unchanged A52. Review actual formal and nested skip output against frozen source, then write `<target>/skip-review.json`. Run A40's new source-specific native contract with `--target <target>`. Use new output paths; every retained result must remain immutable after acceptance.

Example Apple Silicon content commands, from the repository root:

```bash
python3 -B docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py \
  --inputs docs/preset-evidence/2026.11.7/verification/a48/macos-arm64/inputs.json \
  --out docs/preset-evidence/2026.11.7/verification/a48/macos-arm64/observations.json

python3 -B docs/preset-evidence/2026.11.6/verification/a52/bind-parser-archives.py \
  --repo . --source ee0dabcacb4cdff93f8803844f60348e0ceda6ed \
  --run-id 45397189410 --attempt 1 --target macos-arm64 \
  --observations docs/preset-evidence/2026.11.7/verification/a48/macos-arm64/observations.json \
  --native-zip /tmp/cs80-ci-1cee5a/ee0dabca/macos-arm64-native.zip \
  --native-api docs/preset-evidence/2026.11.7/ci/source-ee0dabca/macos-arm64/native-api.json \
  --validator-zip /tmp/cs80-ci-1cee5a/ee0dabca/macos-arm64-validator.zip \
  --validator-api docs/preset-evidence/2026.11.7/ci/source-ee0dabca/macos-arm64/validator-api.json \
  --out docs/preset-evidence/2026.11.7/verification/a48/macos-arm64/parser-archive-binding.json

python3 -B docs/preset-evidence/2026.11.7/verification/a40/source-ee0dabca/verify-native-contract.py \
  --repo . --target macos-arm64 \
  --inputs docs/preset-evidence/2026.11.7/verification/a48/macos-arm64/inputs.json \
  --out docs/preset-evidence/2026.11.7/verification/a48/macos-arm64/native-contract.json
```

Use unchanged `.5` `a41-verify-install-artifact.py` with explicit source, revision, run, attempt, original attempt API, platform `macos` and target architecture. Write its new `install-acceptance.json` into the assigned fresh CI directory. Use unchanged `.6` A51 `verify-macos-binding.py` with the same exact context and actual install/validator ZIPs/APIs; write `archive-payload-binding.json` beside it.

Run this directory's `inspect-packaged-docs.py`, the separately retained A30 successor, on that actual install archive and A41 receipt with `--source ee0dabcacb4cdff93f8803844f60348e0ceda6ed`. Its exact content gates remain unchanged: 13 required metadata files, three guides rewritten by the frozen packager, seven repository notices equal to frozen source bytes, and explicit JUCE-license provenance scope. Only module/temp names and source/reviewer prose differ from A30. Its output belongs in `<target>/docs-and-notices.json`.

Only after those actual inputs and original execution are reviewed may the corresponding wrapper receive exact fixed pins and `reviewed_actual_inputs: True`. The final invocation produces new `original-execution-binding.json` and `platform-qualification.json` in the target's assigned CI directory. Existing destinations and symlinks are refused. Final platform acceptance leaves cross-platform and release qualification false for root's later aggregate.

## Preserved limits and history

`predecessor-files.json` records 100 historical/helper files before this work. Neither the original c554 ARM success nor the original c554 Intel failure is changed. The six frozen skip-source files remain byte-identical, recorded by `skip-source-preparation.json`; this establishes source guards only and cannot replace actual runtime skip review.

`preparation-guards.json` confirms both prepared wrappers refuse acceptance with exit 2. `preparation-receipt.json` preserves the preparation status and engine identity. `helper-differences.patch` makes the successor changes reviewable. No native binary was built or rerun for these preparation checks.

AU `diff -qr` proves ordinary-file membership/content equality, without asserting owner, permissions, ACLs, xattrs or signature validity. Packaged modes are separately checked. The four AU warnings, optional unconfigured Steinberg validator, downloader transport attempts and original validation retry counts must be retained from actual output. The generic A41 receipt's historical 13-lane prose is preserved; the explicit current requirement is 15. Publisher signing, notarization, broad DAW compatibility, acoustical equivalence, listening approval and final public delivery remain outside these platform receipts.
