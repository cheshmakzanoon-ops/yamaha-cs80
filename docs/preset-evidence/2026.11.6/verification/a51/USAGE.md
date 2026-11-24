# Current-source validator payload binding

This is an offline inspection helper, not a native qualification result. It requires **five macOS console lanes** and verifies both fixed-seed records: `0x2b4eef5` and `0x3c99748`. The latter console is `pluginval-au-seed-c76-regression.log`. A successful report deliberately keeps `release_qualified`, `validated_bundle_identity_established` and `run_attempt_bound` false.

## macOS commands

Run from the repository root. Download the actual installation and validator ZIPs through the authenticated GitHub connector, and save each artifact's full API object independently. Do not substitute the list wrapper for the single artifact object. The output path must not exist. Replace the four input paths and output path below with those actual files.

```bash
python3 docs/preset-evidence/2026.11.6/verification/a51/verify-macos-binding.py \
  --repo /workspace/scratch/b9c11dd92abc/yamaha-cs80 \
  --expected-source fcc3c8e055223fe4f786ca162ff46076c33471c8 \
  --expected-run 45366603800 --expected-attempt 1 \
  --expected-architecture arm64 --expected-revision 2026.11.6 \
  --install-zip /absolute/downloads/macos-arm64-install.zip \
  --install-api /absolute/downloads/macos-arm64-install-api.json \
  --pluginval-zip /absolute/downloads/macos-arm64-pluginval.zip \
  --pluginval-api /absolute/downloads/macos-arm64-pluginval-api.json \
  --out /absolute/evidence/macos-arm64-archive-binding.json
```

For Intel use `--expected-architecture x86_64` and the Intel files. Required names are `cs80-macos-15-install-bundle` / `cs80-pluginval-macos-15` for arm64 and `cs80-macos-15-intel-install-bundle` / `cs80-pluginval-macos-15-intel` for x86_64. These names are checked inside authenticated metadata; filenames chosen for local downloads do not establish identity.

Expected source/run/attempt/revision remain explicit arguments rather than hidden defaults. If the candidate or successful attempt changes, obtain its actual receipts and supply the correct values; do not relabel old artifacts. The helper loads the package and inventory verifier from the requested Git commit and checks its revision. It hashes and inspects each ZIP from one immutable byte snapshot, then checks the complete install inventory, executable architecture, AU before/after executable equality, five input paths and both fixed-seed lines.

## Windows and Linux commands

The existing parameterized [A36 VST3 payload helper](../../../2026.11.5/verification/a36-verify-validator-payload.py) remains usable with explicit `.6` inputs; no change to that historical file is needed. Its validator ZIP input is the **product payload actually downloaded by the validator job**, not the archive of text reports. The Windows consumer log is required when the known compiler sidecars occur.

```bash
python3 docs/preset-evidence/2026.11.5/verification/a36-verify-validator-payload.py \
  --repo /workspace/scratch/b9c11dd92abc/yamaha-cs80 \
  --source fcc3c8e055223fe4f786ca162ff46076c33471c8 \
  --revision 2026.11.6 --run-id 45366603800 --attempt 1 \
  --run-api /absolute/evidence/native-run-api.json --platform windows \
  --install-archive /absolute/downloads/windows-install.zip \
  --install-api /absolute/downloads/windows-install-api.json \
  --validator-archive /absolute/downloads/windows-validator-input.zip \
  --validator-api /absolute/downloads/windows-validator-input-api.json \
  --consumer-log /absolute/evidence/windows-pluginval-original-job.log \
  --out /absolute/evidence/windows-payload-binding.json
```

For Linux use `--platform linux`, its matching installation/validator-input archives and API objects, and omit the Windows consumer-log argument.

## Required evidence outside these helpers

- Authenticate the run, job and step records; require the correct source, attempt, platform, original completion and success. A caller-supplied consistent JSON object is not authentication.
- Bind original upload artifact IDs, archive SHA-256/byte counts and timestamps to the exact successful upload steps. Preserve the same actual ZIP identity in downstream native/validator parsing results.
- Bind checkout, native architecture, clean pinned JUCE, configure/build and the complete native CTest/JUnit case inventory to that producer and test-evidence archive.
- Independently parse all **15 required pluginval lanes**: five per Mac, two Windows and three Linux. Require official pluginval 1.0.4, strictness 10, 523 programs, zero failures, explicit success, expected headless/GUI flags, exact original command/report/console identity and a successful propagated process exit. A seed/path check does not establish these outcomes.
- On macOS bind the original successful complete-directory `diff -qr` comparisons before and after validation, signing-before-copy order, exact source/copy paths, original executable hashes and absence of intervening mutation. The hash files alone bind only the AU executable, not resources/signatures.
- Bind each VST3 validated path to the packager input and original ordered successful validation/package/upload steps, without an intervening rebuild or mutation.
- Independently verify current public download availability, actual audition archive, all remaining native platforms and any broader release claims.

A passing fixed-seed lane cannot clear a failed randomized lane. A fixture, re-run or diagnostic cannot silently replace an original failed process. Native GUI work and perceptual similarity remain outside this offline byte-consistency check.

## Local verification

```bash
python3 docs/preset-evidence/2026.11.6/verification/a51/test-macos-binding.py
```

The 22 test methods use inert Mach-O headers, fabricated API objects and path/seed-only console text without `SUCCESS`. They exercise both architectures, exact-source loading, complete inventory and architecture rejection, immutable snapshot use, API hash/source/run consistency, AU payload matching, output preservation, and nine missing/wrong/duplicated new-lane or fixed-seed mutations. Their success does not qualify a native product.
