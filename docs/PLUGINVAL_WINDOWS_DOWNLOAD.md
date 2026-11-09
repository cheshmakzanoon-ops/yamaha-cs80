# Pinned Windows validator transfer

The Windows job still runs official pluginval **1.0.4**, at strictness 10 with
the same randomized and fixed-seed validation commands. Only the download step
changes. Neither validator failures nor workflow jobs are retried by this helper.

## Pin provenance

Verified on 2026-11-09 from the official
[v1.0.4 release](https://github.com/Tracktion/pluginval/releases/tag/v1.0.4):

| Field | Value |
| --- | --- |
| Release ID | `188928656` |
| Windows asset ID | `210999198` |
| Asset | `pluginval_Windows.zip` |
| Archive bytes | `2408590` |
| Archive SHA256 | `c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab` |

Fresh downloads through the release URL and the asset API with
`Accept: application/octet-stream` produced identical archives, both redirected
by GitHub to `release-assets.githubusercontent.com`. The exact new helper also
downloaded and verified those bytes through the API. The archive contains one
`pluginval.exe` (5,909,504 bytes, PE/MZ header); it was inspected, not executed on
the local Linux test machine.

The upstream asset metadata has `digest: null`. This is a locally measured,
official-source-verified content pin, **not an upstream signed checksum**. A
future asset replacement must not silently update it: a new pin requires source
verification and review.

## Transfer contract

- At most three transfers, using the official asset API, release URL, then API.
- Each transfer runs in a separate process with a 60-second wall timeout; the
  only delays are 2 and 4 seconds. The workflow fetch step has a five-minute cap.
- Only transport failures, HTTP 408/429/5xx and truncated transfers qualify for
  retry. TLS verification, unexpected responses, oversized content, checksum
  mismatch, permanent HTTP errors and local failures stop the step.
- HTTPS-only redirects to explicit GitHub release hosts, at most five redirects.
  No authentication token, alternate mirror, TLS bypass or cache is used.
- Transfers use isolated temporary files. Exact size and SHA256 must match
  before publishing `pluginval.zip`; extraction and execution follow only a
  successful helper exit. Existing validator checks are unchanged.
- `pluginval-download.json` records stable source URLs, pin, bounded attempts,
  final hostname and verified digest. Signed redirect paths/queries and raw
  exception messages are not logged. The existing always-upload step includes
  this evidence even when the fetch fails.

The registered offline regression covers transient fallback, deadlines, retry
limits, partial-file isolation, integrity failures, TLS/redirect refusal, output
provenance and the byte-identical Windows validation-step contract. Local
fixtures and live official transfer checks do not replace native Windows CI.

The two download failures on `4278da7` (run `42566983265`, attempts 1 and 2)
were HTTP 504 responses before validator execution. They remain failed attempts.
This transfer change is not a repair or reclassification of the separate
intermittent Linux GUI validator failure.
