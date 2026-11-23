# A59 — download mapping and publication review

## Decision: partial checkpoint; final publication review remains open

The three available installation mappings pass this review. **This is not a
release qualification or approval to replace the current downloads.** The
original Linux installation/native evidence, its three validator lanes, actual
audition acceptance and completed final ledger/candidate are still required.
Resume this same worker once the exact final candidate is available.

The pinned binary source is `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`, source
tree `d84661c6c465c53ff41361e8aba0cba5e4a53397`, native run `45246863788`,
attempt `1`. The publication plan identifies documentation checkpoint
`6426abaa2578a72d9c4df489828037fc9db8eece`. A57's retained source audit records
no runtime, tests, scripts, resources or workflow changes after the binary
source. This worker did not run Git or independently reproduce binary builds.

## Actual installation mappings

For each available target, I independently recomputed the downloaded outer ZIP
SHA-256 and byte count, read the actual outer manifest and inner `SOURCE.txt`,
and compared them with the saved authentic API object, strict installation
report and proposed URL. All those comparisons passed. Architecture and format
claims agree with both manifests and the previously verified executable-header
records. `SOURCE.txt` identifies the source and library revision; the manifest
and API records carry the run identity.

| Target | Actual artifact ID and name | Formats | Outer ZIP bytes |
| --- | --- | --- | ---: |
| Windows x86-64 | `14856804857` — `cs80-windows-install-bundle` | VST3, Standalone | 6,361,027 |
| macOS arm64 | `14855214289` — `cs80-macos-15-install-bundle` | VST3, AU, Standalone | 14,949,170 |
| macOS x86-64 | `14858878730` — `cs80-macos-15-intel-install-bundle` | VST3, AU, Standalone | 16,228,346 |
| Linux x86-64 | **Pending actual artifact** | Expected VST3, Standalone; verify actual manifest | Pending |

The three corresponding nightly.link URLs in the publication plan follow the
correct immutable artifact-ID pattern. Their actual source is `078c098`, not
the later documentation commit. The Mac archives contain separate arm64 and
x86-64 executables; the publication must tell users to choose the architecture
of their **host process**.

| Target | Independently recomputed outer SHA-256 |
| --- | --- |
| Windows x86-64 | `22f5037782789b061dc48a658c5ab08400a85019e050c2a26c4f3042eccd6b4b` |
| macOS arm64 | `a185194b05d68f74c08bdb98f084f826f86f251e842890fe6e07183284dddcc8` |
| macOS x86-64 | `84e0b4f7bd7ea480ccee2bca0eb116b93250df37bd44aa1101d9abf9bd032d0a` |

The detailed observations and exact API/report paths are in
[mapping-checkpoint.json](mapping-checkpoint.json). Producer/upload/attempt and
native evidence remain supported by A51, A52 and A54's separate scoped reviews;
this is a mapping audit, not a replacement for those reviews.

## Publication wording and scope

The current README correctly keeps the qualified .3 downloads while .4 is
pending. Do not replace only three links, fill the Linux row with an older
artifact, infer a Linux ID, or carry forward the .3 native/audition counts.
The eventual four-platform table must point exclusively to the actual accepted
installation bundles, not test-evidence or validator-input artifacts.

The three completed native suites currently total **725 passed / 731 registered**,
zero failures, six formal skips and 21 nested Python skips. These are partial
counts. A55's eight Mac validator lanes and A56's two Windows lanes are accepted;
the three original Linux lanes remain pending. The final native totals must be
assembled from actual per-platform results, with formal skips and nested Python
skips kept distinct.

The supporting DSP review is complete: all five exact-source jobs passed and
the sanitizer suite recorded **199 passed / 200 registered**, zero failures and
one formal skip. Nested Python skips are unknown in that successful sanitizer
log, where successful stdout was suppressed. Do not count them as zero or merge
the supporting suite into native installation totals.

The actual audition artifact is still pending. **138 WAVs, 74 existing programs,
870 seconds and a 523-by-200 inventory are producer expectations**, not accepted
download contents. A58 must finish its authentic artifact, complete-entry,
checksum, sample, metric and delivered-inventory checks before those become
publication facts. A representative 74-program audition would not establish
that all 523 programs were auditioned or listened to by a person.

The revision report correctly isolates the all-521 matched spectral/envelope
comparison at historical source `59d489d`. Its current structural coverage claim
is supported separately: the coverage record reports 523 preserved ordered
identities/tags, 521 musical programs with changed existing sound controls, two
unchanged initialization vectors, the original 198 core IDs and two appended
size controls. Preserve that distinction; parameter changes alone do not prove
realism or perceptual improvement.

## Expiry, acquisition and upgrade instructions

All three currently available installation API records have expiry
`2027-02-21T19:14:35Z`, which converts to **21 February 2027 at 15:14:35 AST
(19:14:35 UTC)** for Halifax. Recompute the earliest advertised-archive expiry
after the actual Linux and audition API records arrive. It is a retention
timestamp, not a permanence guarantee.

Keep the original run's
[Artifacts list](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788#artifacts)
as a fallback while retained. State that these are **CI installation bundles
without publisher release signing or Apple notarization**, with possible Mac
ad-hoc signatures and compatible-library requirements on Linux. The archives
were acquired through the GitHub connector and hashed; no independently tested
unauthenticated nightly.link download route is claimed.

Use this exact upgrade instruction prominently in the final Downloads section:

> After installing, use **Library → FACTORY LIBRARY → LOAD / RELOAD**, then play
> fresh notes. Older projects restore their stored settings. Clicking an
> already-loaded row with Audition enabled may retain those settings.

The installation guide's `#install-only-the-formats-you-need` anchor exists.
Keep the outer-ZIP versus inner-archive distinction and preserve intact
plug-in/application bundles. The outer hashes belong in the final qualification
record; included `SHA256SUMS` verifies the packaged manifest/inner archive.

## History preservation and synchronized records

Insert the new .4 current section before the existing .3 distribution section.
Change the .3 heading from **Current** to **Historical** and preserve its entire
body, including development corrections, failed/cancelled outcomes, source IDs,
counts and links. The .2 and older history also remain unchanged.

[historical-preservation.json](historical-preservation.json) records the exact
current bytes for the final comparison:

- Existing .3 body: **7,204 bytes**, SHA-256
  `5002fcd0648888d66c35ec9e347abe58aa7bcbca585f3e2808b8c6173bd02a27`.
- All text after the old .3 heading through EOF: **25,173 bytes**, SHA-256
  `43a4c2827550c104a5570df9ac7381713854d8b906ed79928ccb3979ea1e720f`.

During this review, the parent refreshed the CI README and qualification status
to include all three available installation/native suites, ten validator lanes,
the complete supporting DSP result and 60 actual spawned workers. Overall
qualification remains correctly false. Earlier planning-state pending labels
and A57's historical 58-worker snapshot are not final current evidence.

The current draft ledger has **60 distinct numbered rows and 60 unique task
paths**, with 54 marked completed and six incomplete or partial. This checks
the shape of the parent's actual roster; it does not replace A57's final ledger
audit. The final `.4` team review, release-qualification and completed ledger
are not yet present at their proposed repository paths. Final publication
must create those records before linking to them.

## Deliverables and next review

- [a59-review.json](a59-review.json): scoped decision, checks, inputs and pending
  requirements.
- [README-Downloads.pending.md](README-Downloads.pending.md): minimal current
  Downloads template with visible pending fields.
- [distribution-current.pending.md](distribution-current.pending.md): minimal
  new current-distribution excerpt and preservation instructions.

The drafts are scratch templates, not publication-ready documents. They leave
Linux, final aggregate counts, actual audition acceptance and final ledger links
explicitly pending. The final follow-up must inspect the exact candidate, all
four actual installation records, actual audition record, all qualification
references, expiry minimum, historical-body hashes and all 60 worker outcomes.

No repository files, Git state or external services were modified. No product,
native suite, validator or new audio capture was executed.
