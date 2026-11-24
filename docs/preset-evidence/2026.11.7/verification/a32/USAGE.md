# A32 — exact `.7` Windows/Linux consumer-to-install binding

This guide prepares checks for the actual Windows/Linux artifacts. It does not accept an archive or qualify a platform. [The review](../../reviews/a32.md), [source pins](helper-pins.json), and [pending contract](prepared-contract.json) define its scope.

## Fixed identity and required input files

| Field | Exact value |
| --- | --- |
| Repository | `cheshmakzanoon-ops/yamaha-cs80` |
| Repository path | `/workspace/scratch/1cee5a9391a9/yamaha-cs80` |
| Compiled source | `c5549f7d095a5c5f49a5ad22ec0ccc819245d201` |
| Source tree | `469244af34233cfa145537266efd29024b66abba` |
| Revision | `2026.11.7` |
| Native run / attempt | `45385716988` / `1` |
| Windows original producer | `138415960660` |
| Linux original producer | `138415961141` |
| Consumer job IDs | Pending original allocation; obtain from this run's attempt-1 jobs collection |
| JUCE | `8.0.11`, clean commit `c352e2489000ffeb9e397051377d75eef2efe641` |

Use the compiled source even though the later documentation checkout is `e8e29133a14c492a98033871cfe3912ef8978f5d`. Original run/attempt/job/step API JSON and job logs are needed in addition to artifact metadata. A single artifact object must be extracted unchanged from the authenticated run artifact collection; passing the list wrapper is incorrect. Preserve the collection and acquisition receipt separately. Artifact metadata does not identify a producing attempt.

The commands below use these **planned local filenames**, consistent with the coordinator's download directory. They are not assertions that the files already exist. If acquisition uses different local names, change only the input paths and manifest; retain all source/run/attempt/artifact identities and choose fresh output paths.

| Role | Windows local name | Linux local name | Exact artifact name |
| --- | --- | --- | --- |
| Installation outer ZIP | `windows-install.zip` | `linux-install.zip` | `cs80-windows-install-bundle` / `cs80-linux-install-bundle` |
| **Product VST3 ZIP** | `windows-vst3.zip` | `linux-vst3.zip` | `cs80-windows-vst3` / `cs80-linux-vst3` |
| **Text-report ZIP** | `windows-validator.zip` | `linux-validator.zip` | `cs80-pluginval-windows` / `cs80-pluginval-reports` |
| Native test ZIP | `windows-native.zip` | `linux-native.zip` | `cs80-windows-test-evidence` / `cs80-linux-test-evidence` |

All ZIP filenames above are under `/tmp/cs80-ci-1cee5a/c5549f7d/`. Metadata/log paths below are under `docs/preset-evidence/2026.11.7/ci/source-c5549f7d/<platform>-x86_64/`, with `install-api.json`, `vst3-api.json`, `validator-api.json`, `native-api.json`, `producer-job-api.json`, `producer-job.log`, `validator-job-api.json`, `validator-job.log` and `run-api-at-platform-completion.json`. These are input naming conventions; A32 does not write the platform evidence directories.

## Source steps and selected paths

Both producers build at `build/CS80Reference_artefacts/Release`. Both product-upload steps publish its **entire `VST3` directory**. The installation packager independently reads the same Release root and selects the VST3 bundle and Standalone. The original successful producer sequence must connect checkout, configure, clean JUCE verification, build, native Test, product upload, package and install upload, without an intervening rebuild or mutation of the relevant product. Linux has audition capture and rendering between these steps; review their actual successful sequence as well. The byte comparison closes the product-to-package identity gap.

| Platform | Producer product upload | Consumer download | Selected product | Package step / install upload |
| --- | --- | --- | --- | --- |
| Windows | `Upload Windows VST3`; `cs80-windows-vst3`; `build/CS80Reference_artefacts/Release/VST3` | `Download Windows VST3`; `path: vst3` | `vst3\CS-80 Reference.vst3` in both original commands | `Package Windows VST3 and Standalone`; `Upload Windows install bundle` |
| Linux | `Upload Linux VST3`; `cs80-linux-vst3`; `build/CS80Reference_artefacts/Release/VST3` | `Download Linux VST3`; `path: vst3` | `$PWD/vst3/CS-80 Reference.vst3` in all original commands/retries | `Package Linux VST3 and Standalone`; `Upload Linux install bundle` |

The product ZIP root contains `CS-80 Reference.vst3/`. The distributed roots are:

- Windows: outer artifact → `cs80-reference-windows-x86_64-c5549f7d095a.zip` → `cs80-reference-windows-x86_64-c5549f7d095a/VST3/CS-80 Reference.vst3/`.
- Linux: outer artifact → `cs80-reference-linux-x86_64-c5549f7d095a.tar.gz` → `cs80-reference-linux-x86_64-c5549f7d095a/VST3/CS-80 Reference.vst3/`.

The native binaries inside that selected bundle are `Contents/x86_64-win/CS-80 Reference.vst3` and `Contents/x86_64-linux/CS-80 Reference.so`. A36 compares the complete selected trees: regular-file bytes/sizes/digests, resources, inferred directory topology and supported safe symlink targets. It does not compare archive timestamps, ownership or Actions-normalized permissions. A41 separately checks installation structure, declared inventory, native image architecture/role, source text, provenance and checksums.

### Windows compiler sidecars

Only the exact regular top-level pair `CS-80 Reference.exp` and `CS-80 Reference.lib` may sit outside the selected bundle. The `.exp` must be an AMD64 COFF export object with `.edata`; the `.lib` must be a valid COFF archive containing AMD64 short-import members. The helper records both hashes and sizes separately. Missing partners, malformed formats, other basenames, `.pdb`, arbitrary outside files, or sidecars placed inside the selected VST3 bundle receive no exemption. Linux has no sidecar exception.

The exception also requires the exact source workflow's upload/download/path contract and the original Windows consumer log's artifact identity plus exactly two selected-bundle validation starts. It does not exclude any member *inside* `CS-80 Reference.vst3` from equality.

## Required original upload and download binding

For each installation, product, native-evidence and report artifact, require the exact repository API route/name/ID, source/run, `expired: false`, future `expires_at`, byte count and `sha256:` digest to agree with the downloaded ZIP. Resolve one unambiguous artifact record from the authenticated collection. Preserve raw metadata bytes and their hash.

Bind each artifact ID to its original successful upload step in the original producer or consumer job of attempt 1. The log must connect the action's name, uploaded ZIP SHA-256, cumulative bytes and terminal artifact ID/size to that step. The upload completion and API creation/update timestamps must lie within the reviewed step's time interval, allowing only the timestamp precision tolerance used by the parent qualification. An `always()` evidence upload is not proof that earlier tests/validators passed.

Within the original **product download step**, match exactly one line with these actual metadata values:

```text
- cs80-PLATFORM-vst3 (ID: ACTUAL_ID, Size: ACTUAL_BYTES, Expected Digest: sha256:ACTUAL_SHA256)
SHA256 digest of downloaded artifact is ACTUAL_SHA256
```

Keep that step boundary; an unrelated artifact download elsewhere in the log is insufficient. Require the downloaded digest to equal the independently hashed `windows-vst3.zip` or `linux-vst3.zip`, and bind the corresponding producer upload ID. After extraction into `vst3`, require every relevant `Started validating:` path to resolve to the selected bundle under the original consumer workspace. Do not infer exact workspace prefixes from the historical `.4` runner path; read them from the new original checkout and commands.

The same original commands must select the pinned pluginval executable, preserve the source's strictness/GUI/seed flags and appear in successful original step metadata. Match each artifact console to its original runtime segment after removing only the job-log timestamp wrapper and documented text encoding/line-ending representation. Match its report to the same console apart from the selected-path header. Preserve all original raw files and their hashes. A console/report pair describes one invocation, not two.

**A36 consumer-log limitation:** the helper performs the Windows log check only for a ZIP containing the sidecar pair. It does not inspect a Linux consumer log and does not inspect a Windows log when there are no sidecars. The independent proof above is required in all cases. Supplying a log argument alone must never be described as a completed consumption check.

## Five original Windows/Linux validator lanes

Every lane is pluginval **1.0.4**, strictness **10**, `--validate-in-process`, timeout **120000 ms**, exactly **523 programs**, one validation start, exactly one terminal clean `SUCCESS`, no failure markers and an original normal successful process exit.

| Platform / lane | Exact workflow step | Console member | Report member under `pluginval-reports/` | GUI / seed |
| --- | --- | --- | --- | --- |
| Windows headless | `Validate VST3 headless` | `pluginval-windows.log` | `cs80-windows` | `--skip-gui-tests`; random seed recorded |
| Windows state regression | `Validate state regression seed 0x5037a0c` | `pluginval-windows-seed-regression.log` | `cs80-windows-seed-regression` | `--skip-gui-tests`; exactly `0x5037a0c` |
| Linux headless | `Validate VST3 headless` | `pluginval-headless.log` | `cs80-headless` | `--skip-gui-tests`; random seed recorded |
| Linux GUI | `Validate VST3 GUI under Xvfb` | `pluginval-gui.log` | `cs80-gui` | Xvfb; editor/automation phases; random seed recorded |
| Linux GUI regression | `Validate GUI state regression seed 0x2da72db` | `pluginval-gui-seed-regression.log` | `cs80-gui-seed-regression` | Xvfb; editor/automation phases; exactly `0x2da72db` |

Each console and report needs one matching seed record. Randomized lanes must use the original source command without a supplied fixed seed; a chance numerical coincidence with a fixed seed is not itself a failure. Check actual GUI phase completion as well as A52's editor-start observation. Record warnings and explicit skipped subtests for review rather than silently promoting them to extra executions.

Windows' source-pinned downloader binds official release **188928656**, asset **210999198**, `pluginval_Windows.zip`, **2,408,590 bytes**, SHA-256 `c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab`. Require the actual receipt's verified/actual size/digest fields, original successful fetch step and `pluginval - 1.0.4` version output. Download-transfer retries in that receipt are not validation retries. A52's minimal version/verified fields do not independently enforce all downloader constants.

Linux fetches the source-pinned official `v1.0.4/pluginval_Linux.zip` release URL and retains `pluginval-version.txt` equal to `pluginval - 1.0.4`. The current Linux workflow does not retain the Windows-style pinned download-digest receipt. Require its original successful fetch command and version output; do not invent an independently pinned Linux asset digest.

### Normal process exit and Linux retry history

Windows propagates `$LASTEXITCODE` immediately after each pluginval/Tee-Object pipeline, then requires `SUCCESS`; neither original validation step suppresses failure. Require successful original step conclusions and exact unsuppressed commands. A `SUCCESS` line followed by a crash cannot be accepted. Windows has no validation retry wrapper and must have exactly the two original lane executions.

Linux runs the pinned `run-pluginval-linux.sh` around `run-pluginval-with-core.py`. Require the original successful `Verify diagnostic exit propagation` step and retained diagnostic fixture records. Each lane's `*-core.log` must record all original child terminations in order; the final termination must be `raw returncode=0; kind=exit`. A status-139 child or signal after `SUCCESS` is a failed lane, regardless of the text parser.

The source permits at most **three full invocations per lane**, hence at most two retries, only when all these conditions are demonstrated for each preserved earlier invocation:

1. The wrapper's normalized original exit is **139**. Preserve the distinction between raw `139` with `kind=exit` and raw `-11` with `kind=signal`; the former does not prove SIGSEGV. Normalization is `code` for nonnegative raw code and `128 - code` for a negative raw code.
2. The exact preserved console contains neither `SUCCESS` nor `FAILED`. It has started a test phase and its last started/completed phase marker is completed. This establishes only the source's retry eligibility, not whole-suite completion or crash ownership.
3. The failed console is retained as the contiguous sequence `*-attempt1.log`, then `*-attempt2.log` if applicable, and exactly matches its original job-output segment. A missing, overwritten or discontinuously numbered history is a blocker.
4. Original retry warnings, runtime starts, raw/normalized PID termination records, preserved failed consoles and final console all agree in number and order. Final invocations must succeed normally under the same full source-defined command. Require the original lane step to be successful.

Core-log filename mapping is the console basename with `-core.log`: `pluginval-headless-core.log`, `pluginval-gui-core.log`, and `pluginval-gui-seed-regression-core.log`. The source appends each invocation's child record to that lane's core log; inspect the complete history. A52 checks final normal-zero termination, contiguous saved retry names, no more than two saved retries, and matching record counts. It explicitly leaves retry eligibility and original step/process binding to the parent reviewer.

GDB and fault-thread diagnostic executions, if present, remain separate failed-lane investigations. They cannot count as required lane passes. A passing fixed-seed lane likewise cannot repair a failed random lane. Keep **required lanes**, **actual invocations including eligible retries**, and **diagnostic invocations** as separate counts. The full `.7` release requires **15 lanes**, not the old 13-lane historical prose.

## Exact commands after acquisition

Run from `/workspace/scratch/1cee5a9391a9/yamaha-cs80`. Every `--out` path must be new. These commands are prepared, not recorded actual-archive executions. First complete the A41 installation checks and source documentation/notices review from [A30](../a30/usage-and-mapping.md); a payload-only result does not replace installation acceptance.

### Windows product-to-install comparison

```sh
TMPDIR=/tmp/cs80-a32-1cee5a PYTHONDONTWRITEBYTECODE=1 python3 -B \
  docs/preset-evidence/2026.11.5/verification/a36-verify-validator-payload.py \
  --repo /workspace/scratch/1cee5a9391a9/yamaha-cs80 \
  --source c5549f7d095a5c5f49a5ad22ec0ccc819245d201 \
  --revision 2026.11.7 --run-id 45385716988 --attempt 1 \
  --run-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/windows-x86_64/run-api-at-platform-completion.json \
  --platform windows \
  --install-archive /tmp/cs80-ci-1cee5a/c5549f7d/windows-install.zip \
  --install-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/windows-x86_64/install-api.json \
  --validator-archive /tmp/cs80-ci-1cee5a/c5549f7d/windows-vst3.zip \
  --validator-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/windows-x86_64/vst3-api.json \
  --consumer-log docs/preset-evidence/2026.11.7/ci/source-c5549f7d/windows-x86_64/validator-job.log \
  --out docs/preset-evidence/2026.11.7/verification/a32/windows-payload-binding.json
```

### Linux product-to-install comparison

```sh
TMPDIR=/tmp/cs80-a32-1cee5a PYTHONDONTWRITEBYTECODE=1 python3 -B \
  docs/preset-evidence/2026.11.5/verification/a36-verify-validator-payload.py \
  --repo /workspace/scratch/1cee5a9391a9/yamaha-cs80 \
  --source c5549f7d095a5c5f49a5ad22ec0ccc819245d201 \
  --revision 2026.11.7 --run-id 45385716988 --attempt 1 \
  --run-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/linux-x86_64/run-api-at-platform-completion.json \
  --platform linux \
  --install-archive /tmp/cs80-ci-1cee5a/c5549f7d/linux-install.zip \
  --install-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/linux-x86_64/install-api.json \
  --validator-archive /tmp/cs80-ci-1cee5a/c5549f7d/linux-vst3.zip \
  --validator-api docs/preset-evidence/2026.11.7/ci/source-c5549f7d/linux-x86_64/vst3-api.json \
  --out docs/preset-evidence/2026.11.7/verification/a32/linux-payload-binding.json
```

A36 success must retain `release_qualified: false`, `native_validation_passed: null`, and `artifact_attempt_binding_verified: false`. Its status `distributed_vst3_matches_validator_input` is a byte-equality observation. Even a failed run context can produce this observation. Do not change these fields in the raw receipt.

### Parse reports and bind their ZIP/API/member identities

[The two-target manifest](consumer-inputs-template.json) uses the planned native/test-report filenames above. Use it only after those actual ZIPs exist. It contains no product VST3 ZIP. This is a preliminary Windows/Linux observation; the final aggregate must use A29's full four-target manifest and A52 `--require-complete`.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -B \
  docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py \
  --inputs docs/preset-evidence/2026.11.7/verification/a32/consumer-inputs-template.json \
  --out docs/preset-evidence/2026.11.7/verification/a32/windows-linux-observations.json

for cs80_a32_platform in windows linux; do
  PYTHONDONTWRITEBYTECODE=1 python3 -B \
    docs/preset-evidence/2026.11.6/verification/a52/bind-parser-archives.py \
    --repo /workspace/scratch/1cee5a9391a9/yamaha-cs80 \
    --source c5549f7d095a5c5f49a5ad22ec0ccc819245d201 \
    --run-id 45385716988 --attempt 1 --target "${cs80_a32_platform}-x86_64" \
    --observations docs/preset-evidence/2026.11.7/verification/a32/windows-linux-observations.json \
    --native-zip "/tmp/cs80-ci-1cee5a/c5549f7d/${cs80_a32_platform}-native.zip" \
    --native-api "docs/preset-evidence/2026.11.7/ci/source-c5549f7d/${cs80_a32_platform}-x86_64/native-api.json" \
    --validator-zip "/tmp/cs80-ci-1cee5a/c5549f7d/${cs80_a32_platform}-validator.zip" \
    --validator-api "docs/preset-evidence/2026.11.7/ci/source-c5549f7d/${cs80_a32_platform}-x86_64/validator-api.json" \
    --out "docs/preset-evidence/2026.11.7/verification/a32/${cs80_a32_platform}-parser-archive-binding.json" || exit "$?"
done
```

A52 parser and binder hashes are pinned in `helper-pins.json`. The parser observes 15 supported lanes, but this two-target input contains only five; neither its success nor the binder's success qualifies a platform. Retain original run/attempt/step review, consumer proof, A29 native roster/skip acceptance and A41/A30 installation acceptance independently.

## Windows notices: bounded source checkout comparison

`scripts/package-ci-artifacts.py` copies seven repository notice payloads with `read_bytes()`. The source's only `.gitattributes` entry prevents text conversion for the immutable `.5` baseline JSON; it does not cover these notices. An isolated exact-source checkout demonstrated the following potential Windows transformation. The raw/source and transformed SHA-256 values, all four queried attributes per file, and command are in [the retained receipt](windows-notice-checkout-transform.json).

| Packaged notice | Source bytes | LF count | Complete CRLF checkout bytes |
| --- | ---: | ---: | ---: |
| `LICENSE` | 34,523 | 661 | 35,184 |
| `licenses/BarlowCondensed-OFL.txt` | 4,377 | 93 | 4,470 |
| `licenses/BarlowCondensed-SOURCE.txt` | 994 | 16 | 1,010 |
| `licenses/MTS-ESP-LICENSE.txt` | 661 | 5 | 666 |
| `licenses/libxml2-Copyright.txt` | 1,314 | 24 | 1,338 |
| `licenses/libxml2-SOURCE.txt` | 667 | 15 | 682 |
| `licenses/libxml2-ADDITIONAL-NOTICES.txt` | 1,406 | 38 | 1,444 |

No actual Windows archive or original Git newline setting was available to A32. The permitted future proof is **one consistent mode for the entire seven-file set**: every raw payload equals its exact source blob, or every raw payload equals the demonstrated Git checkout result. For this source the latter is exactly `source_bytes.replace(b'\n', b'\r\n')`, because each original has zero CR/NUL bytes. Store each raw payload hash/length and the selected comparison mode; preserve the original ZIP/API and inventoried hashes unchanged.

Require a passing same-archive A41 inspection, Windows/x86_64/source/revision/run identity, all 13 required regular nonempty metadata files and their inventory bindings, source-derived guide rewrites, and original clean checkout/package-step proof. Keep the source `.gitattributes` and notice hashes pinned. A different byte, missing/final extra newline, partial EOL conversion, BOM, changed whitespace, mixed seven-file modes or unrelated path is outside this exception. The binary/resource payload comparison remains strict byte equality.

A30's existing ARM report and helper remain unchanged. If its raw notice equality refuses the actual Windows bytes for this precise reason, create a distinct Windows supplemental receipt; do not call the A30 command's failure a pass. If a new executable supplement is needed, focus its negative cases on the identified exception: one non-newline edit, partial conversion, extra/missing final newline, mixed modes, wrong target/source, archive/receipt mismatch and unexpected notice path. No production modification is implied by an exact documented checkout transform.

## Verification already run

```sh
TMPDIR=/tmp/cs80-a32-1cee5a PYTHONDONTWRITEBYTECODE=1 python3 -B \
  docs/preset-evidence/2026.11.5/verification/a36-payload-probe.py \
  --out docs/preset-evidence/2026.11.7/verification/a32/existing-probe-results.json
```

The retained invocation passed 13 unchanged methods with zero failures/errors, 24 inherited plus 34 additional rejected mutations, and no product execution. The output now exists; choose a fresh filename for a deliberate rerun. A29 already reran the unchanged A52 tests for this source; this task did not duplicate them. No extra test suite was needed for the byte-comparison contract.
