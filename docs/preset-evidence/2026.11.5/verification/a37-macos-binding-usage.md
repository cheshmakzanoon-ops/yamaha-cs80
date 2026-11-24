# Parameterized macOS archive binding helper

This is preparation for revision **2026.11.5**, not a macOS build result. No .5 native artifact was supplied or qualified in this work. The historical `.4/ci/source-078c098` scripts and results remain unchanged.

## Run against actual downloaded artifacts

Use the full committed **sound source** SHA, its workflow run ID and attempt, and fresh artifact API JSON obtained through an authenticated GitHub read. The local Git object database must contain that exact source. Run once for each native architecture. A new output pathname is required; existing reports are never overwritten.

```bash
python docs/preset-evidence/2026.11.5/verification/a37-verify-macos-binding.py \
  --repo /absolute/path/to/yamaha-cs80 \
  --expected-source FULL_40_CHARACTER_SOURCE_SHA \
  --expected-run WORKFLOW_RUN_ID \
  --expected-attempt WORKFLOW_RUN_ATTEMPT \
  --expected-architecture arm64 \
  --expected-revision 2026.11.5 \
  --install-zip /absolute/downloads/macos-arm64-install.zip \
  --install-api /absolute/downloads/macos-arm64-install-api.json \
  --pluginval-zip /absolute/downloads/macos-arm64-pluginval.zip \
  --pluginval-api /absolute/downloads/macos-arm64-pluginval-api.json \
  --out /absolute/evidence/macos-arm64-archive-binding.json
```

For Intel use `--expected-architecture x86_64` and the corresponding Intel artifact files. Runner/artifact mapping is explicit: `arm64 → macos-15`; `x86_64 → macos-15-intel`. The default repository is `cheshmakzanoon-ops/yamaha-cs80`; a deliberate alternative can be supplied with `--repository-full-name`.

The helper reads each downloaded ZIP exactly once, then hashes and inspects that immutable snapshot. It checks the supplied API receipt's artifact name, ID, repository URL/IDs, full source, run, byte length and SHA-256. A supplied receipt is not authenticated merely by parsing it. Artifact expiry is recorded, not treated as evidence of current download-link availability.

From the exact expected Git commit, it loads the existing package/inventory verifiers into a temporary directory, verifies that commit's preset revision, and records hashes of the workflow, packager, bundle verifier, macOS pluginval downloader and revision header. It uses no old hardcoded run/SHA/path or previous successful observation. Loading these files means executing trusted repository verification code; downloaded plugin binaries are never executed.

## What a successful report establishes

- Both downloaded archives match the supplied API digest and size, and their receipt metadata agrees with the requested source/run/platform.
- The install ZIP contains the expected tar archive, sidecar manifest and checksum list. The source-pinned verifier checks the complete embedded/sidecar manifest identity, every inventory entry, byte hash, file size, Unix mode and supported link target, rejecting missing or extra archive content.
- Packaged source/revision/run and JUCE source declarations are consistent. Those declarations still need their original execution evidence.
- Complete AU, VST3 and Standalone inventories and reproducible inventory digests identify the packaged bundles, including their resources. Each regular executable has executable mode and the advertised Mach-O architecture and image role. This is header inspection, not native execution or a proof that a universal binary has no additional slices.
- Both AU source/copy executable hash records are identical before and after validation and match the distributed AU executable. Their canonical absolute paths fit the expected source/copy roles.
- Each of the four validator consoles contains exactly one `Started validating` input path matching the packaged-source VST3 path or installed AU path derived from the AU evidence. Validator success is deliberately parsed and bound separately.

## Gates deliberately left open

Every output sets `release_qualified`, `validated_bundle_identity_established` and `run_attempt_bound` to **false**. A successful archive check is useful evidence, not a replacement for these gates:

1. **Original run/job and attempt:** authenticated successful native run/job records must match the full source, run, explicit attempt and architecture. The current artifact receipt and package manifest do not contain an attempt binding.
2. **Original artifact upload:** the exact artifact IDs, byte counts, SHA-256 values and creation times must bind to the successful original install/report upload steps and timestamped upload logs.
3. **Native source and tests:** checkout, native architecture, clean JUCE source verification, build and complete CTest/JUnit case inventory must bind to that same producer/attempt and test-evidence archive.
4. **All four pluginval lanes:** version 1.0.4, strictness 10, 523 programs, zero failures, explicit SUCCESS, correct headless/GUI flags, AU regression seed `0x2b4eef5`, original command/report/console identity and zero propagated exit must be checked. The source-pinned pluginval download receipt is also required.
5. **Whole installed AU bundle:** successful original `diff -qr "$AU_SOURCE" "$AU_DEST"` before and after validation binds resources and signatures. The pre/post `.sha256` files alone bind only the executable. Confirm signing before copying, the exact original hash-output lines, matching source/copy paths and no intervening mutation or rebuild.
6. **VST3 validation input to packaging input:** the exact original VST3 path must match the packager's source in the same checkout, with successful ordered validation and packaging and no intervening mutation/rebuild. No independent validated VST3 bundle is uploaded by the macOS job.
7. **Workflow exit propagation/order:** inspect the exact workflow and original command groups for `set -euo pipefail`, unsuppressed grep/diff/validation failures, no `continue-on-error`, no retries or diagnostic substitutions, and register/validate/package/upload ordering. Source hashes identify review inputs; they do not prove execution.
8. **Remaining platform/release/audio gates:** other native CI jobs, current download availability, signing/notarization, DAW testing and perceptual quality require their own evidence.

The helper intentionally cannot turn self-consistent fabricated receipts, manifests and log text into a release qualification. Its fixtures demonstrate that input-path consistency can succeed without a validator SUCCESS line while every actual validator/job gate remains open.

## Bounded synthetic verification

```bash
PYTHONDONTWRITEBYTECODE=1 python \
  docs/preset-evidence/2026.11.5/verification/a37-test-macos-binding.py
```

The fixtures contain inert Mach-O headers, synthetic plist/resource bytes and fake API/run values. They exercise both architectures and rejection of wrong API identity, missing complete-bundle resources, an otherwise self-consistent wrong-CPU payload, wrong preset/run metadata, changed AU hashes, wrong/repeated validator input paths, unsafe ZIP paths and duplicate JSON keys. They also check immutable snapshot use, exact committed-source loading/revision rejection, no output overwrite, and explicit unbound attempt/release status. Their successful result is **not a native .5 build or validation result**.
