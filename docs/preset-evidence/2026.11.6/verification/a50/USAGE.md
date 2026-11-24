# A50: current audition archive acceptance

## Prepared, pending the actual artifact

The source contract is read with `git show fcc3c8e055223fe4f786ca162ff46076c33471c8:<path>`.
`prepared-contract.json` is preparation evidence only. Its reference inventory is A15's
retained development inventory, not an independent final CI acceptance. No actual
225-clip archive was available to this preparation task. No archive is accepted yet.

Required actual content: 225 WAVs, 89 existing programs, 46 folders, 1,276.2 seconds,
61,257,600 stereo frames, and 490,060,800 raw audio bytes. Every WAV must be original
48 kHz stereo IEEE float32. The complete inventory is 523 × 204 = 106,692 values.
The exact ZIP has 276 regular files and 275 inner checksum records. Full archive
inspection independently recomputes every WAV's safety and CSV audio metrics, checks
its float32 parameter digest, source-derived filename/settings/command/README, and
checks all 19 active models, Standard plus four variants and Standard plus nine materials.

The verifier's 512 MiB immutable-snapshot and expanded-ZIP bounds accommodate the
490,060,800 raw-byte payload plus metadata, while individual members remain under
16 MiB. A partial failed or started capture cannot pass, even with a valid API ZIP digest.

## Actual artifact procedure

1. Retain authenticated raw API artifact metadata, run API JSON, full Linux producer
   job API JSON, and original completed producer log. Required identity: native run
   **45366603800**, attempt **1**, Linux producer **138349816508**, exact source above.
   The job must complete successfully, including the original capture and upload.
2. Download the new source-specific audition artifact. Verify API `size_in_bytes`
   and `digest` against the entire downloaded ZIP. Do not use a historical archive.
3. Obtain an independently source-qualified full inventory (coordinate A56/root).
   The A15 development reference is suitable for preparing the roster; using an
   archive's own inventory as its supposedly independent reference is circular.
4. Run content acceptance, retaining stdout and a fresh JSON output. Substitute the
   actual new paths, authenticated ZIP hash and size:

```bash
python3 docs/preset-evidence/2026.11.6/verification/a50/verify-audition-archive.py \
  --repo . --source fcc3c8e055223fe4f786ca162ff46076c33471c8 \
  --run-id 45366603800 --attempt 1 \
  --expected-inventory /absolute/qualified-reference-inventory.json \
  --expected-inventory-sha256 REFERENCE_SHA256 \
  --archive /absolute/actual-new-audition.zip \
  --expected-zip-sha256 API_SHA256 --expected-zip-bytes API_BYTES \
  --out docs/preset-evidence/2026.11.6/ci/source-fcc3c8e/audition/content-acceptance.json
```

5. Bind that report to the retained raw API and original upload evidence:

```bash
python3 docs/preset-evidence/2026.11.6/verification/a50/bind-audition-ci.py \
  --content-report docs/preset-evidence/2026.11.6/ci/source-fcc3c8e/audition/content-acceptance.json \
  --artifact-api /absolute/actual-artifact-api.json \
  --run-api /absolute/actual-run-api.json \
  --job-api /absolute/actual-linux-job-api.json \
  --job-log /absolute/original-linux-producer.log \
  --out docs/preset-evidence/2026.11.6/ci/source-fcc3c8e/audition/ci-binding.json
```

The binder checks run/attempt/source/job, successful ordered steps, artifact time,
API ZIP digest/size and the unique actual upload ID/size/hash record within that
upload step's timestamp interval. It hashes all retained input files and requires
the current content-verifier SHA256. Both reports create new files exclusively.
Overall release qualification stays false; all four platforms and public-download
checks remain separate.

## Honest scope

The authenticated complete manifest records the capture executable SHA256, while
pinned producer source checks that executable and tracked source before and after
rendering. The workflow does not upload the capture executable itself, so independent
comparison against its raw bytes is unavailable and is explicitly reported as such.

The clips consist of one note-on, fixed pressure, and note-off. They do not play
arpeggiator patterns, live bends, repeated strikes, or pedal gestures. Numerical and
provenance acceptance does not establish acoustic equivalence or human listening approval.

## Offline negative controls

```bash
python3 docs/preset-evidence/2026.11.6/verification/a50/test-audition-archive.py
python3 docs/preset-evidence/2026.11.6/verification/a50/test-audition-ci.py
```

18 unittest methods passed at preparation, with numerous subtests. They cover
stale 171 clips / 202 parameters; missing material WAV; format, rate, float, lengths,
NaN/infinity and clipping; exact inventory metadata and binary32 controls; paths,
checksums, source/cleanliness/commands, partial status and run/attempt; immutable
snapshot identity; non-overwriting reports; and API/upload/producers mismatches.
Synthetic fixtures are not actual audition evidence.
