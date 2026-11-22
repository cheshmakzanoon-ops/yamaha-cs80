# Assignment 57 — installation bundle review

**Actual four-archive content review: PASS. Publication remains unqualified.**
The exact Windows x64, Apple Silicon, Intel Mac and Linux x86-64 candidate ZIPs
from source `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`, run `45044040694`,
attempt 1, were inspected from the shared cache. Their contents pass the bounded
checks below. The required Linux GUI pluginval step failed while diagnostics
were still running; this archive-content result does not override that failure
or certify a complete build/release. No product was extracted or executed.

## Actual candidate archive results

The four archives total **50,444,613 bytes** and contain **114 inventory entries**.
All three guides match the exact fixed-source packaging transformation in every
archive. All **172 source-pinned project links** resolve at that source, and all
**eight local guide links** and anchors resolve. Eight external documentation
links are recorded separately; their source location guidance was checked during
preparation.

The **32 license/notice comparisons** contain 24 byte-exact Mac/Linux matches and
eight Windows CRLF/LF-only checkout differences. Every original archive hash is
preserved; no notice or archive bytes were rewritten. All **eight Unix executable
paths** retain their inventoried executable modes. All **six Mac bundle plists**
name the expected executable, bundle type and application version **1.0.0**.
There are no payload symlinks in these four actual archives.

`artifact-proof.json` records each exact outer/inner digest, layout, per-notice
comparison and guide target. `artifacts-api.json`, `run-api.json` and
`jobs-api.json` preserve the contemporaneous raw API metadata, including the
Linux GUI failure. `summary.json` labels publication as unqualified.
The original preparation records and their checksum manifest remain byte-exact
under `preparation/`. The three-archive interim proof is also retained.


## Source findings

The expected outer download contains exactly an inner archive, its matching
sidecar manifest and `SHA256SUMS`. The inner Windows archive is ZIP; both Mac
architectures and Linux use tar.gz to preserve executable modes and bundle
links. The product roots are VST3 and Standalone, plus AU on macOS. All three
project guides, project and third-party license/notice files, JUCE evidence and
`SOURCE.txt` are explicit package extras.

The guide distinguishes preset revision **2026.11.1** from application version
**1.0.0**, advises closing the host before replacing a complete bundle, explains
Mac host-process architecture and Linux system-library limits, and tells the
user to reselect the Circuit factory program because saved sessions retain
their serialized values. Pressure Lead's pressure requirement and Pure's
masking of modern behavior are explicit.

The three generated source-pinned guides have **47 checked links**: 43 project
links resolve at the exact source, two links resolve to the other bundled guide
(including its heading), and two are external primary documentation links.
These are source-transformation checks, not observations of a downloaded
archive.

The user VST3 locations and Windows system-location alternative agree with
[Steinberg's official locations documentation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BLocations.html).
The AU user location agrees with
[Apple's component-location documentation](https://support.apple.com/en-us/102239)
and [Audio Components API documentation](https://developer.apple.com/documentation/audiotoolbox/audio-components).
The guide's regional Apple link returned a transient HTTP 429 during direct
retrieval; the same article at the US address and Apple Developer documentation
provided the matching location. No installation action was performed.

One nonblocking clarification was reported to root: the frozen packaged guides
predate the VST3 queue-only Program diagnosis, so their broad host-number recall
wording should be read with the current public README/report's qualification.
Root has prepared release-note wording directing users to Library and linking
that qualification. This does not relabel the packaged guides with later text,
and no binary rebuild is proposed for the release-note clarification.

## Inspector and resumption

`inspect-installation-bundles.py` uses only the existing shared outer-ZIP cache.
It never downloads, extracts or executes artifact products. It opens nested
archives through streams and keeps only small metadata in memory. Temporary
files contain three trusted source guides solely to reproduce their packaging
transformation; these temporary source files are removed immediately.

The inspector requires all four exact artifact metadata objects and the raw
first-attempt run record. It checks the outer bytes against API size/SHA-256,
the two inner checksum lines, exact sidecar/embedded manifest bytes, every
inventoried member/hash/type, Unix modes, safe manifest link structure, expected
product roots and format list, Mac plist/executable relationships and
application version, notice/license content, exact source-generated guides and
their links, and the source/revision/JUCE/run identities.

Third-party notices are compared with the fixed source without rewriting their
archived bytes. An otherwise exact CRLF/LF-only difference is recorded explicitly
as a checkout line-ending difference; actual archive hashes remain unchanged.
The JUCE license is compared with the pinned JUCE commit. This is notice
presence/content verification, not a legal-compliance determination.

To review the source contract:

```sh
python3 build-w12-installation-review/inspect-installation-bundles.py --source-only
```

To reproduce this inspection with the recorded candidate archives, supply raw API records as `artifacts-api.json` and `run-api.json`, then:

```sh
python3 build-w12-installation-review/inspect-installation-bundles.py \
  --artifacts-json build-w12-installation-review/artifacts-api.json \
  --run-json build-w12-installation-review/run-api.json
```

The successful actual invocation wrote `artifact-proof.json`. This is an independent installation-content
review; the dedicated native validators, binary-header reviewers and public
release byte verifier provide separate evidence. It does not assert signed or
notarized builds, working installation on a physical computer, every-DAW support,
subjective listening quality or calibrated hardware/acoustic equivalence.

## Preparation evidence

Syntax checking and the source-only invocation pass. An invocation against the
real saved API snapshot containing zero artifacts is correctly refused before
any cached product is opened. `pending-refusal.log` preserves that refusal.
`source-review.json` contains the generated-document hashes, all 47 links and
trusted helper hashes. Raw preparation API snapshots remain separately named;
they must not be substituted for completed build or artifact metadata.
