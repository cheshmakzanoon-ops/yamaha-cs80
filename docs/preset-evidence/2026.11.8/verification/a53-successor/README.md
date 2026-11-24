# A53 final-source audition verification packet

**Prepared; actual audition ZIP acceptance remains pending.** This packet is
bound to source **1b5b3c70471e369481b1f813880a6bcc762a2f57**, tree
**b235b67a801d33e9572076b8dfcfbb89e71fac29**, native build **45457535944**,
attempt **1**, original Linux producer **138670916222**. Edition:
`a53-final-source-successor-2`.

The earlier `a53-recovered` packet remains unchanged and source-bound to ef1c472.
All 16 files in its immutable manifest were rehashed before and after this work.
This successor changes only the content checker's source/tree/run/producer/edition
constants; the other four qualification/test helper files are byte-identical.
`reviewed-pin-update.patch` records the complete change.

## Source continuity and actual clean build

The complete `src` Git tree is unchanged:
`5462494f03daa9841be51000c6b5708570f5b8e8`. CMake, the native build workflow,
the audition exporter and selected capture/factory/body inputs were separately
compared byte-for-byte. The non-documentation changes since ef1c472 are the
supporting sanitizer workflow, its two scripts and the offline
`SixteenVoiceReleaseTests.cpp` fixture. This comparison does not give their
native executions credit for an earlier source.

A genuine clean **Release core/capture rebuild** was performed with one compiler
worker. `clean-build-receipt.json` records the original commands, UTC timestamps,
return codes, log byte counts and hashes. The successful sequence was:

1. CMake `--fresh`, **Unix Makefiles**, `CMAKE_BUILD_TYPE=Release`,
   `CS80_BUILD_PLUGIN=OFF`, `CS80_BUILD_TESTS=OFF`: original exit **0**.
2. Build target `clean`: original exit **0**. An explicit filesystem check found
   **zero** remaining core/capture objects, static library or capture executable.
3. Build target `cs80_preset_capture --parallel 1`: original exit **0**.

The source/tree was clean and exact before and after. Version commands also
exited 0: CMake **3.31.6**, GNU C++ **14.2.0**. This is a capture-only local build;
the original native suites and validators remain separate acceptance gates.

Two earlier local configure attempts are retained under `history/` with their
original **exit 1** records. The first found Ninja unavailable. The second used
Makefiles but found three committed reference documents absent in the sparse
checkout. `IG00156_NODAL_MODEL.md`, `IG00151_VCA_MODEL.md` and `VCA_REFERENCE.md`
were materialized from the exact pinned Git objects; their source hashes and
clean tracked status are retained. No checksum, source content or build gate
was changed. Clean/build did not run in either failed setup attempt.

## Independently executed inventory and tests

The newly built capture tool actually executed `--library all --inventory`,
with normal exit **0**, producing **710,475 bytes**, SHA-256:

`399d4c1e8fc1d638f11a22dccb9643a77026acb40ea7fe2a96c66b940938b621`

Those actual bytes equal the preserved ef1 recovery inventory. The independently
observed executable pre/post SHA-256 is unchanged:

`3b6f0cc6d25cd5ce98c07f226db246be2d505ff83479688237cbfc4b62c3e0a1`

The new receipt identifies source 1b5b3c7, its original native run/job and the
real local execution separately (`execution.kind = local_recovery`). It invents
no Actions qualification run ID. `independent-inventory.json.gz` decompresses
exactly to the raw inventory above.

The existing **20 content test methods and 7 CI-binding methods** freshly passed
after the pin update. Each test command's original exit **0** and exact log hash
are retained. Missing actual archive input returns **1** and creates no receipt.
The source-only prepared contract keeps `accepted_archive_content: false`.

## Actual archive invocation

Use the source checkout pinned above and keep the three helper scripts together.
The current source-specific reference is `independent-inventory.json`, with
`independent-inventory-receipt.json`. For another execution location, regenerate
fresh independent input with the same producer; the receipt binds actual paths
and code hashes. Its optional build logs retain evidence but do not themselves
claim that the inventory producer performed the preceding build.

```bash
python3 produce-independent-inventory.py \
  --repo "$CS80_FROZEN_REPO" --capture "$CS80_REBUILT_CAPTURE" \
  --out "$CS80_NEW_INVENTORY" --receipt "$CS80_NEW_INVENTORY_RECEIPT" \
  --cmake "$CS80_CMAKE" --compiler "$CS80_COMPILER" \
  --configure-log "$CS80_CONFIGURE_LOG" --build-log "$CS80_BUILD_LOG"

python3 verify-audition-archive.py \
  --repo "$CS80_FROZEN_REPO" \
  --source 1b5b3c70471e369481b1f813880a6bcc762a2f57 \
  --run-id 45457535944 --attempt 1 \
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

The required artifact name is
`cs80-curated-preset-audition-1b5b3c70471e369481b1f813880a6bcc762a2f57`.
Use individual authenticated API objects and the original complete job log.
A46's complete outer-ZIP inspection and the separate public full GET remain
required. No existing report is overwritten.

## Unchanged content and CI gates

The exact contract remains **256 WAVs / 95 existing programs / 60 folders /
321 regular files / 320 checksums / 1,487.2 seconds / 71,385,600 stereo frames**.
The full **523 × 204** ordered identities and binary32 values compare against
the independent inventory, including both Init entries. The first 238 requests
and last 18 have separately pinned ordered digests.

Every PCM sample, exact frame/format count, settings row, parameter digest,
12 independently computed metrics, member checksum, README byte, source tree,
clean pre/post provenance and command remains required. Samples at or above
absolute full scale are rejected. Exact zero is restricted to the existing
Pressure Lead at pressure 0 example. The complete PCM budget is **571,084,800
bytes**; the checker streams a bounded immutable compressed snapshot and one
WAV member at a time, without extracting all audio.

CI binding requires the original successful ordered checkout/Configure/JUCE/
Build/Test/Capture/Upload steps, exact repository/workflow/source/run/attempt/job,
original upload ID/bytes/hash/time and the unsuppressed capture command in the
correct original step. Native capture binary bytes are not uploaded; their hash
remains producer-manifest evidence, separate from the independently rebuilt
local binary.

Parameter ranges remain a source review. This packet does not establish actual
audition acceptance, native host acceptance, public delivery, acoustic
equivalence or listening approval. Overall `release_qualified` remains false.
