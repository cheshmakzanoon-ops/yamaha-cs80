# A28: 2026.11.7 audition archive acceptance

## Prepared, pending the actual CI artifact

These are reviewed successors of the unchanged `.6/verification/a50` helpers.
`prepared-contract.json` and `preparation-receipt.json` are preparation evidence:
`accepted_archive_content`, `audition_qualified` where present, and
`release_qualified` are **false**. No actual CI archive was supplied to A28.
The synthetic test fixtures are never retained as accepted audition artifacts.

The pinned source is **c5549f7d095a5c5f49a5ad22ec0ccc819245d201**, tree
**469244af34233cfa145537266efd29024b66abba**, revision **2026.11.7**.
The CI binder expects native run **45385716988**, attempt **1**, and Linux producer
job **138415961141** in `cheshmakzanoon-ops/yamaha-cs80`. Later screenshot/evidence
commits do not replace this source identity.

| Required content | Exact contract |
| --- | ---: |
| Original WAVs | 238 |
| Selected existing programs | 92 |
| Listening folders / metrics CSVs | 54 |
| Total audio seconds | 1,361.2 |
| Stereo frames at 48 kHz | 65,337,600 |
| Raw float32 stereo audio bytes | 522,700,800 |
| Regular files | 297 |
| Inner SHA-256 checksum records | 296 |
| Full ordered inventory | 523 programs × 204 parameters = 106,692 values |

All 225 earlier requests remain in the first 46 folders, with 13 requests appended
in eight low-piano/choir folders. The verifier executes the actual capture roster
loaded through `git show SOURCE:scripts/capture-factory-auditions.py`. It also
checks the reviewed canonical JSON digests of the complete ordered roster, its
225-request prefix, and its 13-request suffix. Equal aggregate counts cannot hide
changed notes, programs, settings, or order.

The independent reference is the source-tracked A12 inventory:

```text
docs/preset-evidence/2026.11.7/verification/a12/inventory.json
SHA-256 55c08568074a6560ef10c8c525733a40aec42d9b8c6d20644bf4984cdd0d3e0d
```

Preparation verifies those reference bytes against that path in the frozen source.
Actual content acceptance compares every ordered identity, description, tag,
parameter ID, and float32 parameter bit against the reference. The archive's own
inventory must not be substituted for this independent input.

## Actual archive procedure

1. Retain the authenticated GitHub retrieval receipts, raw artifact API object,
   raw run API object, full producer-job API object, and original completed Linux
   producer log. The job and its checkout, Build, Test, capture, and upload steps
   must complete successfully. The binder uses their ordered step timestamps.
2. Download the original source-specific artifact named
   `cs80-curated-preset-audition-c5549f7d095a5c5f49a5ad22ec0ccc819245d201`.
   Obtain its ZIP SHA-256 and byte size from the authenticated API record. Its
   artifact ID is learned from that original upload; it is not inferred from the
   run number or borrowed from an older artifact.
3. Run the content verifier with the actual downloaded ZIP and a fresh report
   destination. Substitute only the actual archive path, API digest, API size,
   and intended fresh report path in this example:

```bash
python3 docs/preset-evidence/2026.11.7/verification/a28/verify-audition-archive.py \
  --repo . --source c5549f7d095a5c5f49a5ad22ec0ccc819245d201 \
  --run-id 45385716988 --attempt 1 \
  --expected-inventory docs/preset-evidence/2026.11.7/verification/a12/inventory.json \
  --inventory-source-path docs/preset-evidence/2026.11.7/verification/a12/inventory.json \
  --expected-inventory-sha256 55c08568074a6560ef10c8c525733a40aec42d9b8c6d20644bf4984cdd0d3e0d \
  --archive /absolute/original-source-c5549f7d-audition.zip \
  --expected-zip-sha256 API_SHA256 --expected-zip-bytes API_BYTES \
  --out docs/preset-evidence/2026.11.7/ci/source-c5549f7d/audition/content-acceptance.json
```

The content verifier reads one bounded, immutable ZIP snapshot. The hash and all
inspection describe those same bytes even if the path is subsequently replaced.
It validates all paths, file types, exact filenames, checksums, complete provenance,
commands, README, inventory identities and values, and WAV/CSV settings. It scans
every PCM sample for finiteness and full-scale safety, independently recomputes all
12 reported PCM metrics, and checks exact duration and byte budgets. All 19 active
body models, Standard plus four variants, and Standard plus nine materials must
remain represented.

The unchanged 512 MiB bounds apply both to the ZIP snapshot and expanded member
sizes; 522,700,800 raw audio bytes plus the 710,464-byte reference inventory and
other small metadata fit within them. Each member must remain below 16 MiB. The
archive is inspected without extraction or a duplicate full audio fixture.

4. Bind the accepted content report to the retained raw GitHub evidence, again
   using a fresh destination:

```bash
python3 docs/preset-evidence/2026.11.7/verification/a28/bind-audition-ci.py \
  --content-report docs/preset-evidence/2026.11.7/ci/source-c5549f7d/audition/content-acceptance.json \
  --artifact-api /absolute/original-artifact-api.json \
  --run-api /absolute/native-run-api.json \
  --job-api /absolute/completed-linux-producer-api.json \
  --job-log /absolute/original-linux-producer.log \
  --out docs/preset-evidence/2026.11.7/ci/source-c5549f7d/audition/ci-binding.json
```

The binder checks this verifier's SHA-256; exact source/tree/revision, independent
inventory digest and comparison flags; full declared content contract; run/attempt
and Linux job; successful ordered steps; artifact repository/name/time/digest/size;
and the unique original upload success and digest records inside that upload
step's timestamp interval. The artifact ID, size, hash, and download URL must agree
with the original log. Every retained binder input is snapshotted and hashed.
The caller must retain the authenticated retrieval receipts; agreement between
locally supplied JSON/log files does not itself prove their transport authenticity.

Both acceptance helpers create output files exclusively and refuse replacement.
Keep the helper files unchanged between creating the content report and binding
it, because the binder requires the current verifier's exact SHA-256.

## Preparation and focused controls

To reproduce preparation, use the content command with `--prepare`, omit the ZIP
arguments, and choose a new `--out` path. Preparation does not inspect audio or
accept a CI artifact.

```bash
PYTHONDONTWRITEBYTECODE=1 python3 docs/preset-evidence/2026.11.7/verification/a28/test-audition-archive.py
PYTHONDONTWRITEBYTECODE=1 python3 docs/preset-evidence/2026.11.7/verification/a28/test-audition-ci.py
```

The retained logs record **16 content tests and 7 CI binding tests passing**.
The focused additions reject each of the 13 missing or renamed new WAVs, changed
settings in each appended folder, wrong identity/settings/parameter digest for
each appended request, an obsolete 225-request source roster with a current
revision label, and an internally consistent 225-WAV archive with recalculated
checksums. They also reject equal-count source rosters with altered added settings
and stale `.6` content counts or CI identifiers. All 12 PCM metrics have direct
mismatch and nonfinite controls. Historical format/path/checksum/inventory,
immutable-snapshot, non-overwrite, source/cleanliness, and upload controls remain.

The manifest fixtures contain deliberately invalid small WAV payloads. Their
unchanged control must reach the RIFF rejection; none can pass full content
acceptance. The small PCM fixtures exercise individual metrics only. The CI
fixtures exercise binder logic without publishing an actual acceptance report.

## Scope of a future successful result

Successful content verification and CI binding establish integrity and provenance
of these fixed-note audition excerpts. They do not establish human listening
approval, acoustic equivalence, played arpeggiator patterns, repeated strikes,
pedal gestures, live bends, or quality-mode consistency. The authenticated manifest
records the capture executable's hash and its pre/post stability, but its raw
executable bytes are not included in this artifact. Overall release qualification
and each platform's native host-validation gates remain separate and false here.
