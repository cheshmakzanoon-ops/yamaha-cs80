# Assignment 55 — actual CI audition artifact verified

**PASS: the actual CI audition ZIP was downloaded and verified.** The verifier
exited **0** after 15.917 seconds. No cache entry existed
before invocation: the credential-free public GET returned the exact API size
and SHA-256. No product binary was executed and no WAV copy was extracted.

| Identity | Observed value |
| --- | --- |
| Binary source | `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c` |
| Build / attempt | `45044040694` / **1** |
| Linux producing job | `137274597635` |
| Artifact | `14769036590` |
| ZIP bytes | **41,014,591** |
| ZIP SHA-256 | `9b164379e214d8a15e6907d94dd48842628c09d5970579386818596a0488e16b` |
| Verified UTC | `2026-11-22T19:23:11.617984+00:00` |
| CI artifact expiry | `2027-02-20T16:28:38Z` |

[Public pinned artifact download](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14769036590.zip) ·
[Exact proof and all 24 measurements](artifact-proof.json) ·
[Process invocation, output and exit](verification-execution.json)

The outer ZIP is retained once at `build-coordination/artifact-cache/cs80-curated-preset-audition-6d3cc220edef5a4a2c7cd968ea8a786c7c83198c-14769036590.zip` relative to the
repository root. Its public CI artifact URL expires according to the API field
above; subsequent release publication is a separate operation.

## Completed checks

| Check | Result |
| --- | ---: |
| Complete checksum entries verified | **32** |
| Final inventory programs / core values per program | **301 / 190** |
| Independently matched factory parameter digests | **24** |
| Actual WAV files | **24** |
| Audio length / stereo frames | **117 seconds / 5,616,000** |
| Audio encoding | **48 kHz stereo IEEE float32** |
| Nonfinite samples / samples at or above full scale | **0 / 0** |
| Highest sample peak | **-14.575833239 dBFS**, Arp Poly |
| Largest WAV-to-CSV peak difference | **4.90284080001e-09 dB** |
| Largest WAV-to-CSV RMS difference | **4.77789541264e-09 dB** |

The inventory has exact SHA-256
`fe3ddc6ecdb9fa25e77170f1cd9f81e6f2fe0ef5aabd105506a0fa487eb286fa`. All four source-defined selections, capture
arguments, category-specific MIDI notes, velocity, pressure, rank selection and
hold/release excerpt lengths match. Independent RIFF/sample-array inspection
and float32 FNV-1a parameter digests complement the trusted publisher's complete
ZIP and checksum verification. No normalization is applied.

The required source-verification, Build, Test, capture and upload steps all
report success on attempt 1 in [the raw producing job](linux-job-api.json).
The [raw run](run-api.json) was still **in progress** at this verification, so
this result does not certify the remaining native, pluginval or publication
work. [Raw artifact metadata](artifact-api.json) records its exact API identity.

## Reproduction

With the saved metadata objects and the reviewed source checkout:

```sh
python3 build-w12-ci-auditions/verify-ci-auditions.py \
  --artifact-json build-w12-ci-auditions/artifact-api.json \
  --run-json build-w12-ci-auditions/run-api.json \
  --job-json build-w12-ci-auditions/linux-job-api.json
```

The verifier pins the reviewed capture workflow and publisher SHA-256
`72ad916f9843a9019d17833c6722c64493e8875a3e7818e58dee5ed0c8d7a343`. It uses only the publisher's pure offline
function, not its mutation entry point. It reserves 64 MiB of disk space beyond
the artifact's download size, retains only the outer ZIP in the shared cache,
and verifies an existing cached ZIP before using it.

The capture executable hash `e6a2845a5186572472c8fcbb1e86675e13b7b5f0f81521ee610269aa6839ccd7` is **recorded by
the producing workflow**. The executable itself is not in this ZIP, so this
check does not independently rehash that binary. The ZIP identity, all included
checksums, exact factory inventory and raw samples were verified directly.

## Preserved stages and limits

`initial-preparation/` preserves the original ten-file preparation snapshot.
`publisher-pin-review.json` and `publisher-note-change.diff` document the later
body-only publisher pin update. `pre-verification-README.md` and
`pre-verification-summary.json` preserve the last pending stage. The earlier
local Flute parser check remains identified as source `99f5ebd`, run
`local-assignment-32`; it was never relabeled as this CI artifact.

`verification.log` contains the actual successful invocation output. Pending
producer refusals are retained separately. `SHA256SUMS` covers the current
top-level evidence files; the original snapshot keeps its own checksum list.
The monitor stopped after this assignment's successful artifact verification,
so `monitor-latest-*.json` are historical observations rather than a continuing
heartbeat for later pluginval jobs.

These are bounded single-note core captures, including a one-key arpeggiator
example. Release excerpts may end before a long voice release becomes silent.
The result is not a listening-panel verdict, acoustic-instrument or physical
CS-80 equivalence, all-format playback proof, or arbitrary-DAW qualification.

## Subsequent required gate failure

After the scoped artifact verification, the required **Validate VST3 GUI under
Xvfb** step failed at `2026-11-22T19:24:06Z`. The subsequent raw jobs snapshot is
`post-verification-jobs.json`. This blocks full-build/release qualification. The
audition-byte result remains scoped to the successful producing steps and exact
artifact; it is not a substitute for that failed validator gate. The original
verification-time run/job snapshots and artifact proof are unchanged.
