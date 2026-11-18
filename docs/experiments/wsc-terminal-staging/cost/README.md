# WSC terminal staging: whole-core cost evidence

**Disposition: no performance admission.** The single predeclared three-arm
campaign completed all 216 slots. On the primary held Factory-eight workload,
VECTOR-STAGED used an estimated **3.10% less CPU than BASE** (nominal 95% interval
**1.83% to 4.37%**), below the required **21.48%** noise-adjusted superiority
floor. Factory-moving and Pure-held also failed their secondary noninferiority
requirements. These failures do not prove a regression; they prevent the planned
performance claim in this environment.

This is a whole-core renderer experiment on one virtual machine, including normal
control-frame and MIDI-handler work. It is **not a native VST3 callback, DAW CPU
measurement, or realtime-capacity result**. See the adjacent
[correctness and source evidence](../README.md) for the separate semantic gates.

## Arms and observed contrasts

- **B / BASE:** shipping source revision
  `e9e8175f4278d2457279fcb8809cbba1ddb7be2b`.
- **S / SCALAR-STAGED:** original-position scalar terminal V2. This measures the
  total staging transformation, including scheduling, layout and generated code,
  rather than an isolated instruction-level scheduling overhead.
- **V / VECTOR-STAGED:** integrated vector V1b. V versus S measures the incremental
  backend/integration effect. V versus B is the net product-relevant core contrast.

All values below are percentage changes with nominal 95% whole-round bootstrap
intervals. Positive values in the first and third columns mean **less CPU**.
Positive values in the middle column mean **more CPU**.

| Scene | S saving versus B | V extra CPU versus S | V net saving versus B | Predeclared V decision |
|---|---:|---:|---:|---|
| Factory-held, primary | 9.45 [8.08, 10.75] | 7.02 [5.69, 8.18] | 3.10 [1.83, 4.37] | Fail superiority floor |
| Factory-moving | 6.67 [5.20, 8.94] | 6.95 [4.95, 9.68] | 0.19 [-3.20, 2.74] | Fail -3% noninferiority |
| Panel-moving | 6.29 [3.44, 9.52] | 6.51 [3.74, 8.83] | 0.19 [-1.53, 2.32] | Pass -3% noninferiority |
| Pure-held | 1.84 [-0.66, 5.48] | 2.41 [1.15, 4.07] | -0.52 [-4.01, 4.10] | Fail -3% noninferiority |

S's observed savings are descriptive, not evidence of vector arithmetic savings.
The design allowed a scalar-only choice **before** a cost campaign if V was absent.
This campaign selected three arms. It did not select S as a replacement primary,
and does not admit S automatically after V's failed gate. Percentages cannot be
added: `V/B = (S/B) * (V/S)`.

## Observed real-time factors

RTF is CPU seconds per external audio second. For each arm and scene, first take
the geometric mean of its two observations within each complete comparison round,
then the geometric mean of all six round values. Divide by the measured audio
duration `512 * 64 / 48000 = 0.6826666667` seconds. Each cell therefore uses all
12 comparison observations, with equal round weight. Calibration is not pooled
into these values, and empty-clock cost is not subtracted.

| Scene | B RTF | S RTF | V RTF |
|---|---:|---:|---:|
| Factory-held | 0.57514 | 0.52077 | 0.55731 |
| Factory-moving | 0.80851 | 0.75455 | 0.80699 |
| Panel-moving | 0.79930 | 0.74902 | 0.79779 |
| Pure-held | 0.57636 | 0.56577 | 0.57938 |

The project's **0.15 RTF target is absolute**, not a 15% relative improvement and
not the statistical superiority floor. It would allow 0.1024 CPU seconds for this
measured audio window. Every observed arm/scene geometric mean exceeds that
target. These VM observations do not establish another host's native performance.

## Frozen workload and schedule

The exact driver and imported shipping fixture are embedded as source text in
[protocol.json](protocol.json); replay treats them as data and never compiles them.
All arms use the same ordinary driver object and uninstrumented full core archive.

- 48 kHz external rate, 64-frame stereo blocks, internal factor 2.
- Eight notes `[36, 43, 48, 55, 60, 64, 67, 72]`, velocity 0.8, no note release
  within the window. Drift 0.37, dispersion 0.18, drift model 0.
- Factory scenes use selectors 0/0. Panel-moving uses 13/13. Pure-held disables
  Source Performance. Held scenes use profile 0 and channel 1. Moving scenes use
  profile 1 (MPE/FX) and channels 2 through 9.
- Every block reapplies the unchanged normal frame **before** its new offset-zero
  pressure, pitch-bend and CC74 events, then renders. Every fourth block moves one
  channel with `x = float((block * 13) % 97) / 96.f`; pressure and CC74 use x,
  bend uses `x * .1f`. These are direct floating-point core-handler calls, not a
  claim of native, quantized VST3 MIDI equivalence.
- Every slot is a fresh process: 1,024 warm blocks, 512 measured blocks, one
  untimed continuation block. The normal measured loop contains no trace/counter
  observers. CPU time uses `CLOCK_THREAD_CPUTIME_ID` on pinned CPU 0.
- Calibration: 3 rounds × 4 scenes × 6 slots = **72 processes, all actual B**.
  B/S/V labels in these rows are placebos, not candidate executions.
- Comparison: 6 rounds × 4 scenes × 6 slots = **144 processes**, 12 per arm per
  scene. Each scene has 54 total processes: 18 calibration B plus 12 each B/S/V.
- The round permutations are `BSV, SVB, VBS, BVS, SBV, VSB`; each is followed by
  its reverse. Calibration uses the first three. Scene order rotates each round.
  [protocol.json](protocol.json) enumerates every slot explicitly.
- Fixed caps: 10 CPU seconds and 12 wall seconds per slot, 600 wall seconds for
  the campaign. All **216 attempts produced 216 observations**, with no discarded,
  retried, replaced, resumed or slid window. The recorded campaign lasted 322.94
  wall seconds. Earlier, different LP-candidate measurements remain separate.

## Estimator and decision rule

For each scene/round, let b, s and v be the means of the two corresponding log CPU
observations. Average six round contrasts, then transform:

```
staging tax       = exp(mean(s - b)) - 1
vector saving     = 1 - exp(mean(v - s))
net vector saving = 1 - exp(mean(v - b))
```

The intervals use 10,000 percentile-bootstrap draws (seed 20261118017), resampling
the **six whole comparison rounds**. The same round-index draw is used for all
scenes and contrasts. Quantiles use linear interpolation. The 216 process slots
are not treated as 216 independent replicates. Per-round values and positional
drift diagnostics are retained in [result.json](result.json).

The primary held Factory-eight lower 95% bound must be strictly greater than:

```
max(2%, historical policy floor 11.7%,
    current maximum absolute A/A round-pair effect over all scenes,
    3 * maximum empty-clock / CPU-time ratio)
```

The observed global A/A maximum is **21.4846%**, from Panel-moving calibration
round 0, S-to-V placebo labels. The maximum empty-clock fraction is 0.06853%, so
its threefold floor contribution is 0.20558%. The historical 11.7% is a frozen
policy threshold from a different experiment, not data pooled into this one.
All three secondary V/B lower bounds must be at least -3%.

There are only six comparison clusters. Nominal intervals and balanced reversal
do not eliminate nonstationary, workload-correlated VM effects. The large global
A/A result is retained, not replaced by a more favorable per-scene calibration.
No post-hoc endpoint switch, deletion, extra calibration or selective rerun was
used to rescue the result.

## Identities and untimed efficacy

Every normal timed record's frame, final PCM, continuation PCM and caller FP state
matches its frozen scene identity. The independent export checked all 216 original
attempt records and stdout/stderr pairs. All stderr files were empty. Previously
captured complete untimed workload traces are byte-equal across B/S/V for all four
scenes; their hashes and sizes are recorded in [provenance.json](provenance.json).
These trace hashes identify omitted trace files, not a replay of their contents.

Diagnostic-only per-512-block-window counts were:

| Scene family | Arm | Stage invocations | Scalar terminal calls | Vector calls |
|---|---|---:|---:|---:|
| Each of Factory-held, Factory-moving, Panel-moving | S | 65,536 | 1,048,576 | 0 |
| Each of Factory-held, Factory-moving, Panel-moving | V | 65,536 | 0 | 262,144 |
| Pure-held | S and V, each | 0 | 0 | 0 |

Each successful V call consumes four records: 262,144 × 4 = **1,048,576 known
records per active window**. These counts establish route efficacy separately;
they were not collected in the normal timed images.

## Offline replay and integrity

With Python 3.12 and its standard library, run from this directory:

```sh
python3 -B replay.py
python3 -B replay.py --self-test
```

Neither command renders audio, loads native code, compiles, measures cost or uses
the network. The estimator is an independent implementation, not an import of the
original collector/analyzer. It exactly reproduces all original round contrasts,
intervals, calibration pairs, position summaries and the failed threshold result
on the recorded Python/libm environment. Other libm implementations may differ in
last-bit transcendental results; the exact result check then fails rather than
silently changing the evidence. `tests.json` records **22** in-memory tamper and
genuinely fabricated-vector tests, including a coherent altered-CPU rehash,
endpoint substitution, weakened noise policy, process-slot pseudoreplication,
ratio attribution and a secondary-regression veto.

The eight-file bundle contains:

- `observations.csv`: all 216 rows, all original values, 29 flattened columns,
  UTF-8/LF, no machine paths. CPU clocks are integer nanoseconds; wall time is
  seconds. Identity hashes and original stdout/stderr hashes remain intact.
- `protocol.json`: frozen 216-slot schedule, thresholds, settings, fixture source,
  identities and method.
- `provenance.json`: source inventories/deltas, patch/archive/executable hashes,
  compiler/runtime identities, machine characteristics and export-audit scope.
- `replay.py`, `result.json`, `tests.json`, this README and `manifest.json`.

The normalized CSV reconstructs the original JSONL **byte-for-byte**, with SHA-256
`7ed2ff80405b0d15f662ec26006fa3c62ffdd7e711a44e80e7b049b9417f2b46`, and reconstructs
all 216 normal stdout digests. The manifest protects the other seven files.
Original executables, archives, launch logs and full state/PCM/RNG traces are not
bundled. Their hashes provide identity commitments, not independent proof of those
omitted artifacts or machine authenticity. This replay establishes the saved
observations' internal consistency, not exhaustive DSP correctness or a native
speedup.
