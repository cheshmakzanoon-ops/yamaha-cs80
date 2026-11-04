# Completed integration before the ring continuation repair

This archive records the completed local integration built from
`1e5893854c3d0d9b0aa1ab1c725fd2e8084717f6`, whose last production change was
`23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162`. Production and CMake files remained
identical through the published registration at
`97147ed03ee325d6d6cd1277cf47efcdf121e412`.

It is the **pre-repair baseline**, not qualification of the later ring phase
and precise envelope charge correction. The original logs, all five audit
CSVs and their unfiltered summary are retained. Successful process completion
does not mean every scorecard target passed.

## Completed checks

- Core: 102/102 tests passed in a fresh full run after restoring execute bits
  on two local ELF outputs. The first run and unchanged-mode retry reported
  `BAD_COMMAND` for those two files. Their bytes matched the executable copies
  passing in the plugin build. Only permissions were repaired; the origin of
  the permission loss is not established.
- Linux plugin: 110/110 tests passed, including the actual VST3 host and
  windowed UI smoke test.
- Strict compatibility: Legacy default, Legacy tails, V1 circuit numerics and
  V2 audio/state were byte-identical to their recorded pins. `qualification.json`
  includes the actual sizes and SHA-256 digests of both sides of each pair.
- Full audit: all 885 expected rows completed: 566 phrase, 102 FX, 168 alias,
  40 latency and nine event-causality rows. `audit-summary.md` describes the
  exact factory/rate coverage and every unchanged criterion.

The Source phrase maxima were 1.6192 retrigger (Laser Sweep), 1.03039
reassignment (Clavichord 1 + 2), 1.62689 single note-off (Funky 1 + 2) and
1.6135 chord note-off (Brass 1 + 2). The original physical-PANEL Bass criterion
still exceeded one at 48 and 96 kHz. The ring review arose from these results;
it must be judged using a separate post-repair archive.

## Reproduction and scope

`qualification.json` records the exact commands, compiler, source and pin
identities. The local JUCE build used an explicit checkout of the pinned
8.0.11 commit. The CI-only provenance guard correctly rejects such an
override; its retained rejection is not represented as a local CI-guard
pass. Native CI fetches and verifies JUCE independently.

Run the preserved summarizer from this checkout with
`python3 docs/evidence/qualification-2026-11-01/pre-ring-local-integration/summarize-audit.py --root docs/evidence/qualification-2026-11-01/pre-ring-local-integration --repo .`.
Its output uses the recorded build identity, original source contracts and
raw emitted numeric precision. These are software renders and automated
host tests, not hardware measurements or real-DAW sessions.

`sha256.json` covers every archived file except itself. XML and large CTest
logs use lossless gzip; the manifest also records their uncompressed bytes
and digests. No original failure log is replaced by a successful rerun.
