# Reject impossible oscillator requests before reading FP controls

**Separate, untimed follow-up to the [four-tick WSC interior prototype](../README.md); no shipping change or CPU speed claim.** The [countercensus](../SETUP.md) showed entire Panel/rate cases paying the original experimental wrapper's FP-control reads without skipping a single event search. This variant changes exactly the entry tests: integer-compare the internal rate, endpoint validity, basis, held frequency/threshold bit patterns and gates first. Only surviving requests read MXCSR and the x87 control word. The actual four-step certificate, physical evolution, antialiasing and all fallbacks are unchanged. No extra eligibility or higher tolerance was introduced.

| Fixture | Original prototype control-read pairs | New observed pairs | Avoided stock root searches, unchanged |
|---|---:|---:|---:|
| Factory held (48 kHz, AVX2 filter) | 524,288 | 523,730 | 283,910 |
| Factory moving (48 kHz, AVX2 filter) | 524,288 | 165,207 | 81,564 |
| Panel moving (48 kHz) | 524,288 | 0 | 0 |
| Factory held (44.1 kHz) / factory moving (96 kHz) | 524,288 each | 0 each | 0 each |
| Entire original ten-row qualification grid | 3,972,576 | 2,037,699 | 1,096,251 |

The old normal GCC14.2/PIC object had one `stmxcsr`/`fnstcw` pair before any runtime test on every active oscillator tick. The new normal object's same pair lies behind explicit integer-rate/validity/control branches. Named `tick` entry offsets are 0x0c/0x1a versus 0x81/0x86; these are static offsets, not cycles. The new separate observer counts entries into the control-read pair; both new normal and observed runs reproduce all ten original clean-stock trace hashes (37,732,960 bytes per arm), including continuation/audio and callback FP state. The observed arm also recomputes the real stock root/physical test and matches every original per-row certification/decline count. [All 20 rows](new-dispatch.json). Rates above are host rates; the certificate still applies only at exactly 96 kHz internal. No new AVX instructions are required for this integer prefilter; the filter backend is independently selected.

All four new direct/restored-state comparisons matched. The 3 residual-sNaN and 2 elapsed-sNaN cases exercised both actual compiled Engine tick symbols: equal tested interrupted bytes, signal status/errno and continued public results. The original unsupported-profile/FP controls and admitted-sample proof still apply. Source transformation, build identities and compact receipts are included. This follow-up does not make a new all-toolchains, full-CTest, native-plugin, deadline or universal safety claim.

**The main limitation remains:** on eight held factory notes this change removes only 558 pairs (0.11%). The same many unsuccessful numerical certificate attempts and knot tests still run. Globally, 1,934,877 control-read pairs avoided is a machine-work count, not 48.7% less DSP time. No candidate was clocked; do not promote based on these counts.

For a clean reconstruction, run the parent package's three `engine.py` stages. With the resulting pinned work directory:

    python3 replay.py /path/to/engine-work /new/empty/prefilter-work

The runner generates a new derived header from the original uninstrumented prototype, rebuilds its normal and separate shadow/observed Engine and driver, and verifies every fresh trace against the original clean-stock hashes. No network, plugin, performance timer or external binaries are required beyond the parent’s own built objects. `python3 -B verify.py` reads only the supplied receipts and the parent matrix.
