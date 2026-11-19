# Stock interleaving: cloud diagnostic result

**All 12 triplets completed; none of the three calibration gates passed.** The
plugin used in every role was the same stock ELF. A researcher treating a few
percentage points of difference on this shared VM as a plugin improvement would
risk mistaking run-to-run variation for a code change.

| Stock-only contrast | Observed change | 95% triplet interval | Required interval |
|---|---:|---:|---:|
| S1 versus S0, identical measured work | −2.47% | −6.32% to +1.53% | −2% to +2% |
| S+ matched 512 callbacks versus pooled twins | −1.67% | −4.57% to +1.32% | −2% to +2% |
| S+ measuring 6.25% extra genuine callbacks versus pooled twins | +4.54% | +1.56% to +7.60% | +3% to +10% |

All intervals, including the first two which cover zero, fail **equivalence**
because they extend outside the prespecified bounds. The last interval is
positive but its lower limit misses +3%. The statistic is a geometric contrast
within each fresh-process triplet; the analysis does not treat individual
callbacks, stripes, or repeated role positions as independent observations. No
retries or outlier exclusions. No candidate was loaded or measured, including
the eight-wide prototype. No production performance or energy claim.

**Capture.** One isolated, untimed stock reference and one three-instance
untimed rehearsal preceded the 12 measured three-instance processes. Every one
of the 40 instance histories executed 1,569 audio callbacks (plus its initial
zero-frame flush). Actual audio over the complete history, per-callback
FP/probe/MIDI/result/silence, initial/final public metadata, controls and all four
public state captures matched the isolated reference by SHA-256. This is the
eight-held-note, 64-frame, 48 kHz native VST3 workload, not a DAW session. The
12 triplets contain 19,584 window rows: 18,816 timed callbacks and 768 genuine
but untimed catch-ups.

**Environment.** Pinned logical CPU 0 on the dot's cloud Linux x86-64 guest;
CPU model string Intel Xeon Platinum 8573C, hypervisor flag set. Frequency
governor, physical SMT sibling and thermal counters were not exposed. The 1,152
measured-phase visits recorded 18,203 involuntary guest thread switches; none
recorded a voluntary switch or major/minor page fault. These OS counters include
code outside the inner callback clocks. They do **not** identify the cause of
an individual timing difference and do not justify filtering the data.
The normal stock build SHA-256 was
`8d4078be629d097e6470ec90fb12ccfdb7ad486663f14bb8e3aaef3076bd8725`
(source `db02cd8fa56892c6ea2cf7df084bcf190204968e`).
No dedicated physical target was available or claimed.

**Portable evidence.** [Summary](summary.json) and [decision](decision.json)
include every process total and gate. [Measurement receipt](measurement.json)
includes the source and binary identities available to the collector, workload,
public-artifact SHA-256s for every lane, the original per-slot raw-file hashes,
schedules and external resource bounds. `clocks.csv.gz` and `os.csv.gz` contain
every measured and unmeasured window row from all 14 processes, in order. One
common wall origin and one per-process CPU origin were subtracted, preserving
all durations and cross-process wall order exactly. Paths and private loader
device/inode numbers are omitted. PCM/state binaries are not bundled; their
matching hashes are receipts rather than a re-execution or independent proof
of those omitted bytes. Files are evidence for one run, not a signature of
authorship or protection against coordinated tampering.

From the parent directory, replay without native code:

```sh
python3 -B cloud_evidence.py replay cloud-diagnostic
python3 -B -m unittest -v test_cloud_evidence
```

Replay success means **the raw evidence reproduces failed cloud calibration**.
This is not a cue to repeat the run until it passes. See the
[fixed protocol](../METHOD.md) and [runner](../README.md).
