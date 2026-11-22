# Assignment 59 — published candidate documentation and links verified

**PUBLISHED_CANDIDATE_DOCS_AND_LINKS_VERIFIED_RELEASE_UNQUALIFIED.**

Verification completed at **2026-11-22 19:47:15 UTC** against documentation
commit `814c9014b65b5ac5e48aa6ead8d5c2b4fec568f4` and the observed public `main`.
The binary source remains `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`, build
`45044040694`, attempt 1, conclusion **failure**. No qualified release is claimed.

## Published content

All **17 credential-free public document GETs returned HTTP 200** and matched
the reviewed or committed byte hashes:

- README, refinement report and distribution status on both the exact
  documentation commit and `main`: six GETs.
- The five-download proof, Linux gate proof, actual audition proof, four-archive
  proof, independent failure review, final-CI index, current team report and
  team ledger at the documentation commit: eight GETs.
- README, refinement report and installation guide at the original binary
  source: three GETs.

The earlier local structure check passed **16 checks** covering **123 relative
links and heading anchors**. The older 2026.11 distribution history remains
byte-identical. The README's existing pressure, KAS, Circuit/Pure/Studio,
factory-reload, saved-state, queue-limitation and 67-sample/1.40-ms guidance
remains unchanged. The current team report is a dated earlier completion
snapshot; later coordinator completion records must remain separate.

## Bounded live candidate availability

Each GET requested only `bytes=0-3`. Every response returned **HTTP 206**, the
ZIP signature `504b0304` and the exact expected total in `Content-Range`:

| Candidate | Artifact | Expected total bytes |
| --- | ---: | ---: |
| Windows x64, VST3 / Standalone | 14763872941 | 6,261,285 |
| macOS Apple Silicon, VST3 / AU / Standalone | 14763761497 | 14,900,540 |
| macOS Intel, VST3 / AU / Standalone | 14763644216 | 16,057,766 |
| Linux x86-64, VST3 / Standalone | 14768807632 | 13,225,022 |
| 24 factory audition WAVs | 14769036590 | 41,014,591 |

These post-push probes verify availability only. Assignment 56's independent
full-GET proof remains the complete-byte SHA-256 verification for all five ZIPs;
its public copy was fetched and matched SHA-256
`87ad68a563e9e81bd6cdf3cd307dc6f0fe27ad75ddeca653f4a04cb4d6b1f7ec`.

## Qualification remains blocked

The public `presets-2026.11.1` release-tag endpoint returned **HTTP 404** at
19:47:11 UTC. The required Linux randomized GUI invocation's original
SIGSEGV/raw −11/wrapper 139 remains a failure. The complete build failed and the
publisher was skipped. Native/archive passes, audition checks, beta3 comparison
passes and these live document/link checks do not reverse that result.

The current links identify unsigned, expiring development candidates. No
signing/notarization, crash repair, arbitrary-host compatibility, acoustic or
hardware equivalence, listening verdict or seven-asset release is certified.

## Reusable records

- `published-candidate-verification.json`: authoritative post-push observation;
  SHA-256 `87bae0eefaa906e8697378cb0da76c0b659c253b51a321e48b1437d8ec5e0c7e`.
- `verify-published-candidate-docs.py`: bounded verifier used for that observation.
- `candidate-documents.json` and `candidate-structure-audit.json`: preserved
  local preparation hashes and structure checks, captured before publication.

Only the three parent-authorized public documentation files were edited during
preparation. This post-push check changed no public file, Git ref, CI run,
validation gate or release. The parent performed the Git commit and push.
