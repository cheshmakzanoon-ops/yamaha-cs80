# Preset revision evidence

This compact bundle supports [the engineering report](../../PRESET_REVISION_2026_11.md).
It contains numeric summaries, inventory data and logs, without audio or binaries.

**Local integration verified; platform publication pending.** `final/` contains
the integrated bank including the Stereo Delay Pluck correction and its completed
library audit. The nominal CSV represents all 301 final parameter vectors:
300 unchanged measured rows plus a new Stereo Delay Pluck capture. Every row's
parameter digest matches the final inventory; [collection provenance](final/nominal-provenance.json)
records the two binaries and commands. Five refreshed native recall logs, behavior
checks, capture tests and panel comparisons are included. The release report will
identify the actual source commit, CI run and artifacts when published.

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

## Provenance and reproduction

[provenance.json](provenance.json) maps every copied/extracted data file to its
review origin and original SHA-256. `stored_sha256` describes the included bytes.
Absolute scratch prefixes were removed from JSON provenance strings; remaining
capture-origin paths are historical identifiers, not links to files in this
repository. Numerical fields are unchanged. Waveform hashes identify reviewed
recordings but do not imply those WAVs are bundled here.

The complete bank can be reproduced with the repository's CMake core build,
[`cs80_preset_capture`](../../../src/tools/preset_capture.cpp),
[`LibraryAuditTests.cpp`](../../../tests/LibraryAuditTests.cpp) and
[`PresetVoicingTests.cpp`](../../../tests/PresetVoicingTests.cpp).
Use the [preset-guide commands](../../PRESET_GUIDE.md#reproduce-an-audition-from-source)
for inventory and unnormalized float WAV export. To compare the original bank,
build the documented baseline commit separately. Calibration must rebuild after
each generated level-table change. Reproducing historical family diagnostics
also requires their stated bank snapshot and metric windows; final master gain
must not be substituted into old absolute-level comparisons.

These files document measured software behavior. They do not record a completed
subjective listening study, acoustic measurements, a physical CS-80 measurement,
or native Windows/macOS execution.
