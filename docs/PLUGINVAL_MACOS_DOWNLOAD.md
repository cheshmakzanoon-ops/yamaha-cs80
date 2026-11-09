# Pinned macOS validator transfer

This change addresses archive transport, not validation. Both macOS runners
still use official pluginval **1.0.4** and the same strictness-10 VST3, AU and
AU-GUI commands. Registration, extraction, quarantine handling, validator
commands, SUCCESS checks, packaging, and the Windows/Linux jobs are unchanged.
There are no validator rerolls, workflow reruns, new credentials or mirrors.

## Observed failure

Exact source `6c560f130be649dd4a63cbe146cd5cc54dd057a3`, native run
[`42579617910`, attempt 1, Intel job `129035005752`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42579617910/job/129035005752):

- Build and **154/154 tests passed**. The test step completed at 08:09:55 UTC
  on 2026-11-09.
- The canonical `pluginval_macOS.zip` release URL returned HTTP **504** on the
  initial curl transfer and all three retries, at 08:10:06.624, 08:10:08.836,
  08:10:10.898 and 08:10:13.085 UTC. Curl exited **22**.
- AU registration, all three validators and Intel packaging were skipped.
  This is a confirmed pre-validation transfer failure. It remains a failed
  attempt and says nothing about whether those skipped gates would pass.

The same canonical macOS URL returned 504 in the local investigation. The
corresponding official asset API succeeded, supporting a bounded alternate
route rather than repeatedly fetching only the failing browser URL. This does
not repair or reclassify the separate Linux GUI validator SIGSEGV, nor change
the separate upstream-comparison workflow.

## Official pin and Windows comparison

Official [v1.0.4 release metadata](https://api.github.com/repos/Tracktion/pluginval/releases/tags/v1.0.4)
was checked on 2026-11-09. The release ID is `188928656`.

| Field | macOS pin |
| --- | --- |
| Asset | `pluginval_macOS.zip` |
| Asset API ID | `210999197` |
| Archive bytes | `9202795` |
| Archive SHA256 | `3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f` |
| API route | `https://api.github.com/repos/Tracktion/pluginval/releases/assets/210999197` |
| Browser route | `https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_macOS.zip` |

Two API transfers matched these bytes, including execution of the exact new
helper. GitHub redirected them to `release-assets.githubusercontent.com`.
The local browser-route request failed with 504, so **no successful browser/API
cross-comparison is claimed**. The ZIP passed integrity inspection and contains
12 safe relative entries. Its 26,249,328-byte executable is a universal Mach-O
with x86_64 and arm64 slices; its bundle version is 1.0.4. It was inspected,
not executed on the local Linux machine.

Upstream metadata has `digest: null`. This is an **observed official-source
content pin**, not a publisher-signed checksum. A replaced asset must fail
verification; changing the pin requires renewed source verification and review.

The [Windows helper](PLUGINVAL_WINDOWS_DOWNLOAD.md) already uses this transfer
policy for a different asset (`210999198`, 2,408,590 bytes). The macOS helper
initially copied that reviewed implementation with only its platform/pin fields
changed, including the explicit total-hop counter. The macOS-only API-403
follow-up below adds one route-fallback exception; the Windows helper remains
unchanged during this bounded correction.

## Bounded, fail-closed transfer

- At most **three** attempts: asset API, canonical browser URL, then asset API.
- Each child process has a **60-second wall timeout**, with 2/4-second backoff;
  the complete workflow fetch step has a **five-minute cap**.
- Transport failures, HTTP 408/429/5xx and truncation permit a bounded retry.
  Separately, HTTP 403 on the API route permits advancing through the same
  fixed route sequence; it is not classified as transient or proven rate
  limiting. Browser-route 403, other permanent HTTP errors, TLS verification
  errors, invalid redirects/responses, oversize, integrity errors and local
  failures stop the step.
- Redirects require HTTPS, explicit official GitHub hosts, no URL credentials,
  and at most five total hops. TLS verification is not bypassed.
- Reads are bounded to expected size plus one byte. Each attempt has its own
  temporary file. Exact size and SHA256 must match before atomic publication.
- Required `pluginval-download.json` contains stable source URLs, pin,
  attempt outcomes, final hostname and verified digest. Signed redirect URLs,
  raw exceptions and credentials are not recorded. Failure to write required
  evidence also fails the helper, even if verified archive bytes were published.
- The existing `set -euo pipefail` stops extraction/execution after a failed
  helper. The existing always-upload step now includes its metadata file.

## Verification and limits

Source/test checkpoint: `0128a34f83a0ae3fe8ed0ea012e7cfc1a944774f`, based on
exact `6c560f1`. [Frozen evidence and hashes](evidence/pluginval-macos-download-2026-11-09/manifest.json)
record the official metadata, failed job/transfer excerpts, live API transfer,
archive inspection and local tests.

- **17/17 offline tests pass**, including real urllib redirect plumbing,
  same-URL loops, bounded reads, child deadlines, partial-file isolation,
  required provenance failure, malformed worker metadata and fatal errors.
- **4/4 registered CTests pass**: new macOS transfer tests, unchanged Windows
  transfer tests, validator retry policy and original-process exit classification.
- **14 deliberate helper/workflow faults are detected**, with normal unittest
  completion, not import/compile failures. The reproduction script edits only
  private temporary copies and does not download or execute a validator.
- Exact hashes protect the existing AU registration/three-validator block and
  unrelated Linux/Windows job text. When integrating separately approved Linux
  diagnostic changes, refresh that Linux-prefix expectation against the new
  base deliberately, retain the guard, and rerun the helper tests.

Local tests/live transfer ran on Linux. Native macOS Python/TLS, extraction and
validator execution remain CI qualification. The universal binary inspection
is not a runtime or code-signing attestation. Those original local results did not qualify native execution. The subsequent
published ARM transfer failure and bounded correction are recorded below; no
claim is made that all network outages are recoverable.

```sh
python3 scripts/test-pluginval-macos-download.py -v
python3 scripts/test-pluginval-macos-download-negative.py --output /tmp/macos-transfer-faults
cmake -S . -B build-download -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
ctest --test-dir build-download --output-on-failure \
  -R '^cs80_(pluginval_(macos|windows)_download|validator_(exit_classification|retry))_tests$'
```

## Integration after the diagnostic correction

The reviewed transfer changes were applied without conflicts onto
`bfac9a40afa5d28c695a7952dfa23298e600b9b5`, the screenshot-only child of
`fee93c4ec6147220276c823b40c21680bd865ed5`. The only test adjustment is the
expected Linux-prefix hash, deliberately refreshed to
`4a6fd4498d4c7910f8db71e600b2f950e4b5a8fac0e9d3a26170071672e07bbe`
for that separately approved diagnostic base. The exact Linux prefix and
Windows job text are unchanged from this integration base, as are all existing
validator commands and the bounded original-artifact replay workflow.

The earlier manifest remains evidence for the original `0128a34` source/test
checkpoint, not hashes of subsequently integrated workflow/CMake/test files.
At that integration checkpoint the macOS helper itself was unchanged. Fresh
integrated checks passed 17/17 offline tests, rejected all 14 deliberate faults,
and passed 5/5 registered transfer and
validator-helper CTests, including the new diagnostic selector. Production source
and the complete screenshot tree were unchanged. Native macOS qualification
remained pending at that checkpoint; no earlier failed invocation is reclassified.


## Published ARM failure and API-403 correction

Published source `25b0b9eb44abf789ae9fc2342bd0deaf35051260`, native run
[`42588096429`, ARM job `129062691940`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42588096429/job/129062691940)
passed **156/156 CTests** (979.79 seconds), then failed its fetch step at
09:20:05 UTC on 2026-11-09. Artifact `13661687665` contains provenance for
exactly one API-route request: HTTP **403**, internal transfer status **11**,
`verified: false`. The all-403-is-fatal policy prevented a browser-route attempt.
Extraction and validators did not run. The original run remains failed.

The response body and headers were not retained, so the cause of GitHub's 403
is **not known**. Rate limiting is possible but is not established by this
record. Older `fee93c4` macOS success used the legacy curl path and does not
qualify this helper.

Source/test correction `0459f99619e518af822f206046278f1e298ce782` changes only
three macOS-helper scripts. An HTTP 403 encountered on the public asset's API
route now returns a distinct `API_FORBIDDEN` status **12** and
`reason: api_forbidden`. The parent permits that status only while on the API
route, advancing to the next route if one remains. The canonical public URL is
not a new recipient or mirror; the same exact asset pin is mandatory. This
adds no credential, authentication retry, TLS bypass, or validator reroll.

- API-403 then browser success still requires exact size and SHA256 before
  publication. Browser denial or an integrity mismatch in a complete browser
  download is terminal, without trying the third transfer or replacing an
  existing destination.
- API-403, browser-504, API-403 stops after exactly three transfers. The 60-second
  child limits, 2/4-second backoff, five-minute workflow cap and redirect guard
  are unchanged.
- Browser-route 403 remains fatal. An API-fallback status unexpectedly returned
  on the browser route is also fatal. Other permanent HTTP, TLS, integrity,
  evidence and local-write failures retain their previous behavior.
- Workflow/CMake, all validator commands, Windows/Linux helpers and product
  source are byte-identical to the published base.

[Follow-up evidence and hashes](evidence/pluginval-macos-api-403-2026-11-09/manifest.json)
record the original failure, exact source checkpoint and fresh checks. The
finalized **22-test** regression against the published helper produces four
assertion failures plus one expected unhandled `RuntimeError` in the newly
required fallback-success case, not an import or syntax failure. The corrected
helper passes **22/22** tests; **17/17** deliberate faults are detected and
**5/5** focused CTests pass, including unchanged Windows and validator helpers.

These are offline policy tests on Linux, not a successful live reproduction of
the ARM API-403/browser-success sequence. Native macOS qualification of this
correction is still pending. Earlier manifests remain historical records for
their named checkpoints, not hashes of the newly changed helper/test files.

```sh
# Run from the corrected checkout; the first command intentionally tests old code.
git show 25b0b9e:scripts/download-pluginval-macos.py > /tmp/macos-helper-before-403.py
python3 scripts/test-pluginval-macos-download.py --helper /tmp/macos-helper-before-403.py -v
python3 scripts/test-pluginval-macos-download.py -v
python3 scripts/test-pluginval-macos-download-negative.py --output /tmp/macos-403-faults
ctest --test-dir build-download --output-on-failure \
  -R '^cs80_(pluginval_(macos|windows)_download|validator_(exit_classification|retry|diagnostic_selection))_tests$'
```
