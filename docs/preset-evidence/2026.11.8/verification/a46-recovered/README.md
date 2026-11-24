# A46 recovery — actual-release verification preparation

## Scope and fixed identity

Recovered after the transient workspace was lost. This packet reconstructs
published helper bytes, source contracts and verification interfaces. It is
**not new native evidence or release qualification**. No product changes,
builds, native tests, audio captures, retries or public artifact downloads were
performed by this recovery. Earlier local measurements and synthetic-check
results are not recreated or represented as fresh results here.

- Source: `ef1c472b161c37c490945f7c469086508aae6aef`
- Tree: `f4e00753cde135a36172d9ea549223dfdce5cc82`
- Revision: `2026.11.8`
- Original build: `45439302351`, attempt `1`
- Repository: `cheshmakzanoon-ops/yamaha-cs80`, ID `1436742091`

`run-api-at-recovery.json` is a fresh owner-connector response from the exact
attempt endpoint. Its source/run/attempt/repository were checked. The run was
`in_progress`, with no conclusion, when that receipt was read. A run context
does not bind an artifact to the original upload attempt or qualify its jobs.

## Recovered inputs and exact rosters

All six helpers in `helpers/` were fetched from the immutable source above,
base64-decoded, hashed locally and compared with both the returned Git blob SHA
and the exact Git objects in the restored clone. `helper-recovery.json` records
the newly computed byte counts, SHA-256 and Git blob IDs. **Their bytes are
unchanged.** Existing published blob IDs can be reused at the intended recovery
paths without changing those files.

`recover_contract.py` freshly rederived complete native names from the published
`.7` contract's source-pinned `.4` roster history, then added the six reviewed
common registrations present in the final CMake source. The result is in
`source-contract.json`; `cmake-registration-delta.patch` records the exact delta.

| Target | Required native executions | Validator lanes |
|---|---:|---:|
| Linux x86_64 | 619 | 3 |
| Windows x86_64 | 261 | 2 |
| macOS arm64 | 283 | 5 |
| macOS x86_64 | 283 | 5 |
| Total | 1,446 | 15 |

The six added tests are instrument damper, electric pickup, piano hammer
contact, piano treble contact, crystal string stability and preset repeatability.
The CMake scheduling include changes `RUN_SERIAL` policy, not registrations.
Platform configure conditions and actual original commands must still match.

The final source exporter was independently read and its request list derived
again: **256 WAVs, 95 selected programs, 60 folders, 1,487.2 seconds**, with all
238 previous requests preserved and 18 appended. Canonical request SHA-256:
`09e60ba3a5756d2403d56109413a37b51744d0809dfc2a9e7153e99f00627714`.
Expected regular archive files: 321; complete inventory: **523 × 204**.
This establishes the expected contract, not actual archive contents.

The independent inventory and its execution receipt are deliberately **unset**.
A53 owns the reviewed audition successor and the interface
`--expected-inventory PATH --independent-capture-receipt PATH`. Its reference
must be freshly regenerated from the exact source and a genuinely executed
capture; a remembered inventory hash or lost capture measurement is insufficient.

## Minimal recovered interfaces

Run these scripts from this directory and use fresh output filenames. Supply
artifact IDs and metadata from authenticated acquisition, not guessed IDs.

```bash
python inspect_receipt.py inspect --archive "$CS80_ZIP" \
  --artifact-api "$CS80_API" --artifact-id "$CS80_ARTIFACT_ID" \
  --name "$CS80_ARTIFACT_NAME" --out "$CS80_NEW_RECEIPT"

python check_native.py --target "$CS80_TARGET" --archive "$CS80_NATIVE_ZIP" \
  --artifact-api "$CS80_NATIVE_API" --artifact-id "$CS80_NATIVE_ARTIFACT_ID" \
  --out "$CS80_NEW_NATIVE_REPORT"
```

The receipt inspector binds complete immutable outer ZIP bytes to API size/SHA,
source/run/repository/name/ID/expiry. It preflights the directory before allocation,
bounds expansion, refuses ambiguous paths/nonregular members and reads every
member to EOF for CRC and SHA-256. It never extracts or executes products.

The native checker parses a private copy of those frozen ZIP bytes using the
unchanged published parser, binds every parsed input back to the same ZIP,
checks JUCE/workflow provenance, every expected test name, complete numbered
LastTest framing, zero failed/unknown/disabled cases and all six new tests passed.
It rejects formal skips outside the two historical capability gates. **A zero
checker exit still requires actual capability/skip review, an original successful
Test step, unsuppressed normal CTest exit 0 and original upload-attempt binding.**
Nested unittest skips remain separate from CTest counts. Return 1 means content
failure/incompleteness; return 2 means malformed or unbound input. No actual
current native ZIP was parsed while reconstructing these interfaces.

For each of the four installation ZIPs use the unchanged
`helpers/a41-verify-install-artifact.py` with explicit `--repo`, `--source`,
`--revision 2026.11.8`, `--run-id 45439302351`, `--attempt 1`, target/platform,
archive/API/run-API and fresh output. It loads package checks from that exact
source and verifies full inner inventory/checksums, SOURCE.txt, revision/run,
JUCE, and every ELF/PE/Mach-O native binary's architecture and role. Compare
source-specific packaged guides/notices too. Its legacy “13 lanes” prose is
obsolete: this contract requires **15**.

For Linux/Windows, `helpers/a36-verify-validator-payload.py` takes the install
archive/API and **VST3 input archive/API**, plus the source/revision/run/attempt,
run API, platform, repository and fresh output. `--consumer-log` is required for
Windows compiler-sidecar provenance. A validator report ZIP is not a VST3 input.

For each Mac, `helpers/verify-macos-binding.py` takes explicit expected source,
run, attempt, architecture and revision plus install/report ZIPs and APIs.
Original unsuppressed whole-AU `diff -qr` before and after validation and matching
executable hashes remain required, as do the same VST3 path and ordered
validation/packaging with no intervening mutation or rebuild.

`helpers/parse-native-evidence.py` already implements all 15 pluginval lanes;
`helpers/bind-parser-archives.py` binds those observations to actual API/ZIP/member
bytes. Their published parser hash was freshly confirmed, not assumed.

## Mandatory remaining actual-output gates

Retain original job/step API, commands, logs, upload IDs/digests/timestamps and
attempt identity for four producers and two validator jobs. Artifact API data
does not normally identify an upload attempt. Preserve every earlier failed or
superseded attempt; no diagnostic replay replaces the required original lane.

Require pluginval 1.0.4, strictness 10, 523 programs, one validation start and
terminal SUCCESS, matching console/report seeds, required editor coverage and
original **normal zero exit**. Linux lanes are headless, GUI and GUI seed
`0x2da72db`; Windows lanes are headless and seed `0x5037a0c`; each Mac has VST3,
AU headless, AU GUI and separate GUI seeds `0x2b4eef5` / `0x3c99748`. Review all
warnings, skips and eligible preserved retries. SUCCESS followed by a crash fails.

A53's actual audition acceptance must scan all 256 WAVs, compare exact settings,
frames/format/finite/headroom/metrics and full checksums, bind capture/source/run
provenance and compare the 523×204 vectors against the independent final-source
inventory. The `.7` 238-clip checker is not valid for `.8` without its reviewed
successor. No acoustic realism or DAW compatibility follows from those checks.

For each advertised install and audition route, the optional `public-get` mode
performs a complete unauthenticated, non-range HTTPS GET to its exact nightly.link
artifact-ID route and requires full API size/SHA and HTTP 200. Redirect queries
are omitted from receipts. Run it separately for all five advertised artifacts:

```bash
python inspect_receipt.py public-get --artifact-api "$CS80_API" \
  --artifact-id "$CS80_ARTIFACT_ID" --name "$CS80_ARTIFACT_NAME" \
  --out "$CS80_NEW_PUBLIC_RECEIPT"
```

No public GET was performed during recovery. After all actual gates pass, retain
the exact five links, same-run Artifacts fallback and earliest expiry. Immutable
blob creation of this recovery packet does not move a ref or publish downloads.
