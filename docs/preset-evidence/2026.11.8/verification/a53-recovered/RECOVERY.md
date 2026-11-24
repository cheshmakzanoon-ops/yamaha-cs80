# A53 recovered audition qualification tools

## Provenance and current status

The local execution environment was reset after the original A53 preparation.
Its local `.8` checker, test outputs and receipts were no longer available.
The four published `.7` predecessor files were fetched again from immutable
source `ef1c472b161c37c490945f7c469086508aae6aef`; their bytes match the previously
recorded predecessor SHA-256 values in `predecessor-identities.json`.

The current files are a **newly reviewed successor**, identified as
`a53-independent-capture-successor-1`. They are not claimed to reproduce the
lost `.8` file hashes. The original `.8` logic was reconstructed from retained
task/tool history and the published predecessors; the independent-input
interface was changed to require newly executed recovery evidence.

Source remains **ef1c472b161c37c490945f7c469086508aae6aef**, tree
**f4e00753cde135a36172d9ea549223dfdce5cc82**. The **original native build** is
**45439302351, attempt 1**, Linux producer **138605634881**. A local recovery
execution has no invented GitHub Actions run ID.

**Fresh recovery completed:** the separately rebuilt capture tool actually
generated a 710,475-byte inventory with SHA-256
`399d4c1e8fc1d638f11a22dccb9643a77026acb40ea7fe2a96c66b940938b621` and inventory
process exit **0**. The source was clean and pinned before/after; the capture
binary's independently observed pre/post SHA-256 is
`3b6f0cc6d25cd5ce98c07f226db246be2d505ff83479688237cbfc4b62c3e0a1`.
Its receipt records `execution.kind = local_recovery`, observed Python 3.12.14,
CMake 3.31.6 and GNU C++ 14.2.0, and hashes of root's fresh configure/build logs.
The producer helper performed the inventory command; the preceding
configure/build commands and their original exits require root's separate logs.

**20 freshly executed content test methods and 7 CI-binding methods pass.**
The regenerated source/roster contract independently agrees with A46's recovered
contract. A missing actual archive returns 1 and creates no acceptance receipt.
The original audition ZIP and its original completed capture/upload are still
pending actual acceptance; these recovery facts do not qualify the release.

## Reproduction requirements

Use a clean checkout at the exact frozen source and separately build its
`cs80_preset_capture` target. Do not modify the frozen checkout while capturing.
Keep the current three helpers together in one directory. Python 3 is sufficient;
the content/CI checkers use only its standard library. All output paths must be
fresh. The compressed independent inventory is an exact retained representation;
its decompressed bytes must have the SHA-256 above.

To generate fresh independent input and an honest execution receipt:

```bash
python3 produce-independent-inventory.py \
  --repo "$CS80_FROZEN_REPO" --capture "$CS80_REBUILT_CAPTURE" \
  --out "$CS80_NEW_INVENTORY" --receipt "$CS80_NEW_INVENTORY_RECEIPT" \
  --cmake "$CS80_CMAKE" --compiler "$CS80_COMPILER" \
  --configure-log "$CS80_CONFIGURE_LOG" --build-log "$CS80_BUILD_LOG"
```

The last four options retain observed version/log evidence. They do not assert
that this helper performed the build. Actual inventory execution must exit 0;
the output must match the exact final inventory hash. The helper records the
real GitHub Actions environment only if one actually exists; otherwise it
records `local_recovery` with no qualification run ID. Original native build
identity is stored separately in `original_build`.

For actual audition content acceptance, first obtain the complete original ZIP,
the authentic artifact API digest and byte count, and retain A46's independent
outer-ZIP receipt. Then run:

```bash
python3 verify-audition-archive.py \
  --repo "$CS80_FROZEN_REPO" \
  --source ef1c472b161c37c490945f7c469086508aae6aef \
  --run-id 45439302351 --attempt 1 \
  --expected-inventory "$CS80_NEW_INVENTORY" \
  --independent-capture-receipt "$CS80_NEW_INVENTORY_RECEIPT" \
  --archive "$CS80_ORIGINAL_AUDITION_ZIP" \
  --expected-zip-sha256 "$CS80_API_SHA256_WITHOUT_PREFIX" \
  --expected-zip-bytes "$CS80_API_BYTES" --out "$CS80_NEW_CONTENT_REPORT"

python3 bind-audition-ci.py \
  --content-report "$CS80_NEW_CONTENT_REPORT" \
  --artifact-api "$CS80_ORIGINAL_ARTIFACT_API_OBJECT" \
  --run-api "$CS80_ORIGINAL_RUN_API_OBJECT" \
  --job-api "$CS80_ORIGINAL_LINUX_JOB_API_OBJECT" \
  --job-log "$CS80_COMPLETE_ORIGINAL_LINUX_JOB_LOG" \
  --out "$CS80_NEW_AUDITION_CI_REPORT"
```

The three API inputs are individual authenticated objects, not collections.
The required artifact name is
`cs80-curated-preset-audition-ef1c472b161c37c490945f7c469086508aae6aef`.
Do not substitute a later rerun or local result for an original native result.

## Preserved acceptance gates

The exact contract is **256 WAVs / 95 existing programs / 60 folders / 321
regular files / 320 checksums / 1,487.2 seconds / 71,385,600 stereo frames**.
All **523 × 204** ordered identities and binary32 vectors compare independently,
including both Init entries. The 238 previous requests retain their exact
ordered digest; the appended 18 requests also have a fixed digest. No presets
are added by this exporter or checker.

Every checksum, README byte, selection, command, parameter digest and settings
row is checked. Every WAV sample is scanned, with exact 48 kHz stereo float32
format and frame count. Nonfinite samples and samples at or above absolute full
scale are rejected. Exact silence is restricted to the existing Pressure Lead
at pressure 0 example; the other clips must contain signal. All 12 reported
audio metrics keep the predecessor's independent computation and tolerances.

The 571,084,800-byte PCM payload exceeds the old 512 MiB expanded cap. The new
bounded limits account explicitly for that payload plus metadata, while the
exact file/frame checks remain unchanged. Inspection streams one immutable
compressed snapshot, then one bounded WAV member at a time; it does not extract
the expanded audio. Allow space for an additional compressed ZIP during reading.

The CI binder requires original successful and ordered checkout, Configure,
JUCE verification, Build, Test, Capture and Upload steps; original source/run/
attempt/job/repository/workflow identity; exact upload ID/bytes/digest/time;
and the source's unsuppressed Bash command in the actual capture step.
The native capture executable is not uploaded: its hash remains authenticated
producer-manifest evidence and is not expected to equal a recovery binary hash.

Parameter ranges are not inventory fields and remain a source review. Content
acceptance, audition CI binding, public delivery, native host qualification,
acoustic equivalence and human listening approval remain distinct. Every helper
keeps overall `release_qualified` false. A public full GET is still required.

## Bounded verification commands

```bash
CS80_TEST_REPO="$CS80_FROZEN_REPO" \
CS80_TEST_INVENTORY="$CS80_NEW_INVENTORY" \
CS80_TEST_RECEIPT="$CS80_NEW_INVENTORY_RECEIPT" \
  python3 test-audition-archive.py
python3 test-audition-ci.py
```

Synthetic negative controls reject obsolete 238-clip content, all 18 missing or
misnamed additions, corrupt settings/values/identities, unsafe ZIP metadata,
malformed WAVs, metric drift, wrong uploads/attempts/jobs and suppressed or
out-of-step commands. They are not delivered-audio acceptance. The new local
execution-context test rejects invented Actions identities.
