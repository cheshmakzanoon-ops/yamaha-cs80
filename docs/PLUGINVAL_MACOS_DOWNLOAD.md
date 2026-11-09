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
copies that reviewed implementation with only its platform/pin fields changed,
including the explicit total-hop counter. It is separate to avoid changing the
already-qualified Windows path during this bounded fix.

## Bounded, fail-closed transfer

- At most **three** attempts: asset API, canonical browser URL, then asset API.
- Each child process has a **60-second wall timeout**, with 2/4-second backoff;
  the complete workflow fetch step has a **five-minute cap**.
- Only transport failures, HTTP 408/429/5xx and truncation permit another
  transfer. TLS verification errors, invalid redirects/responses, oversize,
  permanent HTTP errors, integrity errors and local failures stop the step.
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
is not a runtime or code-signing attestation. There is no new native CI result
for this unpushed candidate, and no claim that all network outages are recoverable.

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
The macOS helper itself is unchanged. Fresh integrated checks pass 17/17 offline
tests, reject all 14 deliberate faults, and pass 5/5 registered transfer and
validator-helper CTests, including the new diagnostic selector. Production source
and the complete screenshot tree are unchanged. Native macOS qualification of
the new transfer remains pending; no earlier failed invocation is reclassified.
