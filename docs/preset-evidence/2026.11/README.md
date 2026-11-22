# Preset revision evidence

This is the retained **2026.11** verification record. The later
[2026.11.1 evidence](../2026.11.1/README.md) and
[refinement report](../../PRESET_REFINEMENT_2026_11_1.md) describe a separate
source revision; these 2026.11 results do not qualify its binaries.

This compact bundle supports [the engineering report](../../PRESET_REVISION_2026_11.md).
It contains numeric summaries, inventory data and logs, without audio or binaries.

**All four native development packages and the twelve-WAV audition have verified
public downloads. The latest CI attempt succeeded; the original Linux validator
crash remains unexplained.** The verified source is
`d0c3c23be68ef048aa56969f1c493c221c1562c5`, under
[run 45029235535](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45029235535).
All native build/test/package jobs passed in attempt 1. An externally triggered
attempt 2 reran only the Linux validator, using the same VST3 artifact on a
different runner with changed randomized seeds. Its headless, GUI and fixed-GUI
gates passed with normal exit 0. The original headless seed `0x5ba7253` printed
`SUCCESS` and then terminated with raw −11 / SIGSEGV; that invocation remains
failed. Latest success is not evidence of a clean first pass or a fix.
[Combined verification record](ci/release-qualification.json) ·
[Attempt comparison](ci/final-linux-validator-attempt-2.json)

`final/` contains the integrated bank including the Stereo Delay Pluck correction
and its completed library audit. The nominal CSV represents all 301 final parameter vectors:
300 unchanged measured rows plus a new Stereo Delay Pluck capture. Every row's
parameter digest matches the final inventory; [collection provenance](final/nominal-provenance.json)
records the two binaries and commands. Five refreshed native recall logs, behavior
checks, capture tests and panel comparisons are included.
The preceding `ba3a721` run passed Windows and both Mac targets but failed two
Linux fixtures; `ci/second-attempt.json` preserves that failed result. Its three
platform reports and downloaded-file checks are historical evidence, not proof
that the current run has passed. The two fixture repairs have controlled local
reproductions and independent review under `ci/fixture-repairs/`; the actual
`d0c3c23` Linux CI then passed all 376 controller assertions and four strict panel
checks, with 577 passed, zero failed and one skipped CTest entry out of 578.
[Actual Linux execution](ci/final-linux-x86_64.json)

## Reading the evidence

| Directory | Meaning and conditions |
| --- | --- |
| `baseline/` | Original bank at `61f45feeae1f01038476eb9c99105534950db4de`; complete inventory, nominal capture and library audit. |
| `final/` | Integrated inventory, 301-row nominal data (48 kHz, .8 velocity, zero pressure, at least 3 s hold + 1 s tail) and unchanged library audit. Slow attacks extend the hold; notes and durations are recorded per CSV row. |
| `identity/` | Extracted independent float-bit/metadata/MIDI audit and one change-count row per program. Snapshot precedes the final delay correction; it establishes preserved contracts, not that every later source hash is unchanged. |
| `calibration/` | Two generated level tables and rebuild log between them. Generator stdout is empty on success. |
| `families/` | Selected W04/W05 numeric review data. Mostly 48 kHz captures before final master calibration. The engineering report specifies windows and inputs for its cited comparisons. |
| `controls/` | All-bank static velocity/pressure screens plus bounded actual-engine comparisons. These do not imply every program was rendered under every control value. |
| `behavior/` | Same 136 checks linked to original, quiet-draft and corrected immutable libraries; 44.1 kHz and 45.05 seconds generated per run. |
| `long-tails/` | Six actual factory programs, 84 seconds total, 48 kHz/2×, two full-velocity/full-pressure chords per program with natural release overlap; exact peaks and windowed voice counts. |
| `stereo/` | Eight-program screen and matched two-control Stereo Delay Pluck correction at 48 kHz, MIDI 60, velocity .8, pressure 0. `comparison.json` retains aggregate/critical-window values and omits the long sliding-window arrays. |
| `host/` | Five final rebuilt identity/recall logs (6,800 checks) and four panel comparator results, all passing. |
| `capture/` | Nine CLI tests and six byte-identical edge-note/rate comparisons after portability changes. |
| `ci/` | Source-bound native and package reviews, all five complete public downloads, actual audition verification, retained failed attempts, successful later invocations, bounded diagnostics, causal fixture repairs and the independent downloaded-artifact verifier. Each JSON record names its own commit, run and scope. |

### Current publication records

| Evidence | What it establishes |
| --- | --- |
| [Combined record](ci/release-qualification.json) and [latest API status](ci/run-latest-attempt.json) | Current source/run identities and successful latest attempt, read together with the retained Linux failure. |
| [All installation archives](ci/final-install-artifacts.json) | Four verified native packages, 114 inventory entries and 10 binary headers; source, run, revision and bundled-document checks. |
| [Public downloads](ci/final-download-links.json) | Full unauthenticated HTTP 200 downloads with exact API sizes and SHA-256 for four installation ZIPs and the audition ZIP. |
| [Validator/package identity](ci/final-validator-payload-identity.json) | Complete Linux and Windows VST3 file inventories match the packages offered for installation. |
| [Actual audition](ci/final-audition.json) | Twelve actual 48 kHz stereo float WAVs / 48 seconds, complete checksums, matching 301-program inventory and raw/CSV metric agreement. |
| [Linux first attempt](ci/final-linux-x86_64.json) | Passing native build/tests and the separate failed stable headless invocation: `SUCCESS` followed by SIGSEGV. The two GUI gates were skipped. |
| [Linux second attempt](ci/final-linux-validator-attempt-2.json) | Same input artifact, three successful stable validator invocations, changed runner/seeds, retained native executions and rerun-attribution limits. |
| [Current official beta3 comparison](ci/current-upstream-comparison.json) | The exact Linux VST3 artifact passed 25/25 headless, 31/31 GUI-inclusive and 31/31 GUI-regression tests under a different host and different seeds. |
| [Bounded headless diagnostic](ci/headless-diagnostic.json) | Two instrumented invocations with the original artifact and failed seed exited 0. They did not reproduce the crash or produce a fault stack; they establish no cause, ownership or fix. |
| [Independent final acceptance](ci/acceptance/FINAL-ACCEPTANCE-2026_11.md) | All 83 then-retained evidence hashes and 206 consistency joins pass. Accepts the accurately disclosed original development downloads; the original Linux failure remains unresolved and newer 2026.11.1 binaries remain outside this verdict. |

The verified [Linux installation ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14761556554.zip)
retains that known intermittent validator limitation. The
[twelve-WAV audition ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14761870353.zip)
contains the actual checked recordings and provenance; audio is not duplicated
in this compact repository evidence bundle. All artifact links follow GitHub's
retention period. The beta3 and instrumented diagnostic passes are additional
bounded observations and do not replace the original stable failure.

The [review ledger](review-assignments.md) records the 60 primary authoring and
review assignments plus one nested, read-only controller-contract follow-up.
Final release acceptance is tracked separately from completion of those initial
assignments.

## Provenance and reproduction

[provenance.json](provenance.json) maps every copied/extracted data file to its
review origin and original SHA-256. `stored_sha256` describes the included bytes.
Absolute scratch prefixes were removed from JSON provenance strings; remaining
capture-origin paths are historical identifiers, not links to files in this
repository. Numerical fields are unchanged. Waveform hashes identify reviewed
recordings but do not imply those WAVs are bundled here.

To reproduce **this 2026.11 bank**, first select its immutable source
`d0c3c23be68ef048aa56969f1c493c221c1562c5` in a separate clean checkout. Current
`main` contains later 2026.11.1 data and does not reproduce this historical bank.
For example, from an existing repository clone:

```sh
git worktree add --detach ../cs80-preset-2026.11 d0c3c23be68ef048aa56969f1c493c221c1562c5
cd ../cs80-preset-2026.11
```

Then use that checkout's CMake core build,
[`cs80_preset_capture`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/d0c3c23be68ef048aa56969f1c493c221c1562c5/src/tools/preset_capture.cpp),
[`LibraryAuditTests.cpp`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/d0c3c23be68ef048aa56969f1c493c221c1562c5/tests/LibraryAuditTests.cpp) and
[`PresetVoicingTests.cpp`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/d0c3c23be68ef048aa56969f1c493c221c1562c5/tests/PresetVoicingTests.cpp).
The [source-pinned preset-guide commands](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/d0c3c23be68ef048aa56969f1c493c221c1562c5/docs/PRESET_GUIDE.md#reproduce-an-audition-from-source)
export inventory and unnormalized float WAVs. For the pre-redesign comparison,
build `61f45feeae1f01038476eb9c99105534950db4de` in another separate checkout.
Calibration must rebuild after
each generated level-table change. Reproducing historical family diagnostics
also requires their stated bank snapshot and metric windows; final master gain
must not be substituted into old absolute-level comparisons.

These files document measured software behavior, source compatibility and the
specific native executions identified in the CI reports. They do not record a
completed subjective listening study, acoustic-instrument equivalence, a
physical CS-80 comparison, or qualification of every DAW and playback system.
