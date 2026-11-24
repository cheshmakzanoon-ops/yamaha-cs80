# A55 public download acceptance

This checkpoint verifies one of the five required public routes. The actual Apple Silicon full GET succeeded with exactly the API size and digest. The other three installation bundles and actual audition archive remain pending. `acceptance-contract.json` is the authoritative scope and checkpoint state for this review.

## Real observation

The 2026-11-24 10:35 UTC metadata snapshot came from the authenticated GitHub connector. The single-artifact API endpoint was unavailable through the repository fetch wrapper; the dedicated workflow-artifacts connector returned the original artifact entries instead. `artifacts-api.json` retains that list and `14910025415-api.json` its ARM entry. `run-api.json` retains original run JSON from the repository fetch connector. No wrapper error is counted as an artifact failure.

The parameterized probe was reused unchanged:

```bash
python3 docs/preset-evidence/2026.11.5/verification/a57-probe-public-artifact.py \
  --artifact-api docs/preset-evidence/2026.11.6/ci/source-fcc3c8e/public-downloads/a55/14910025415-api.json \
  --run-api docs/preset-evidence/2026.11.6/ci/source-fcc3c8e/public-downloads/a55/run-api.json \
  --source fcc3c8e055223fe4f786ca162ff46076c33471c8 \
  --run-id 45366603800 --attempt 1 \
  --artifact-name cs80-macos-15-install-bundle \
  --out /tmp/a55-arm-new-observation.json
```

The output must be a fresh path. A rerun creates a new observation and should not overwrite the retained original. It requires complete HTTP 200 retrieval and API-matching bytes/digest. Its 512 MiB compressed-body bound also covers the expected audition's 490,060,800 raw WAV-data bytes; the actual archive's API size must still be checked. The probe does not save ZIP bytes or qualify ZIP contents.

## Remaining five-route gate

The required names are fixed in the contract. For each missing route, obtain its real ID and authentic metadata from the same full source and native run. Fetch fresh run metadata, ensure the intended attempt, then pass that artifact's exact name and metadata file into the unchanged probe. Record the original upload/producer-attempt proof separately; the artifact API has no producing-attempt field.

After all five routes pass, update each contract entry from its real receipt. Before marking aggregate public availability true, independently confirm:

1. Exactly five expected artifact names and five distinct actual IDs.
2. Full source/run/repository match in all metadata and receipts.
3. Each receipt's metadata-file hashes match retained inputs; no expiry has passed.
4. Every actual HTTP 200 body exactly equals its authenticated API byte count and SHA256.
5. The audition entry points to the actual archive accepted by the separate 225-WAV content review.
6. The minimum expiry includes all five artifact entries and is converted using `ZoneInfo("America/Halifax")`, with UTC retained.

Do not substitute the minimum from the single available ARM artifact for the all-five minimum. Keep global `release_qualified` false in this public-route record; the release publisher must also satisfy the independent native, package, source, audition and final publication gates. A public-route pass neither proves acoustic realism nor replaces listening.

No signed redirect query, credentials or cookies may be retained. The current receipt records only endpoint host/path and a `query_redacted` flag.

## Focused negative exercise

```bash
python3 docs/preset-evidence/2026.11.6/ci/source-fcc3c8e/public-downloads/a55/exercise-probe-failures.py
```

This runs 19 synthetic mock checks and writes `probe-self-test.json`. It performs no network request. Its synthetic fixture, even on a success case, is never counted as a public artifact or release qualification.
