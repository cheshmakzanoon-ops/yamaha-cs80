# Offline conditional IG00151 VCA reference

Build `cs80_vca_reference` with `CS80_BUILD_PLUGIN=OFF`. This independent double
ODE model makes the existing inferred voice VCA laws reusable for research. It
is not recovered hardware, an envelope generator, a whole-note truth renderer,
a Bass Stack click fix, or a replacement audio path. All existing production
VCA tests and parity policies remain unchanged.

## Input and example

One combined endpoint file binds the signal and all controls to the same clock:

```csv
sample,input,lc,ei,envelope
0,0,1,0,0
1,0.5,1,0.25,0.5
2,-0.5,0.8,-0.25,1
```

```sh
cs80_vca_reference --input endpoints.csv --rate 48000 --samples 3 \
  --out NEW_DIR --compare-realtime true
```

N declared endpoints define exactly N-1 intervals, with piecewise-linear signal,
LC, EI and envelope between knots. No extrapolation, normalization, resampling,
bandlimited interpolation, or extra final interval is implied. Input, LC and
output use **uncalibrated engine units**, EI uses octaves, and envelope is a
unitless gain in [0,1]. A recorded or exported signal needs an explicitly stated
unit conversion before use; this tool does not supply hardware calibration.
Negative LC and LI are supported, with only the gain floored at zero.

Every line must end in LF (CRLF accepted); exact header and canonical contiguous
indices starting at 0 are required. Values are finite representable ASCII
decimals, without spaces, quotes, hexadecimal numbers or extra columns. Input,
LC, EI and initial LI are bounded to [-8,8], envelope to [0,1]. Files must be
regular, at most 64 MiB of bytes actually read, with at most 256 bytes before
each newline. Exactly `--samples` rows are required, catching whole-row
truncation as well as partial final lines. Rate is an integer in 8000..384000 Hz;
N >= 2, duration <=10 seconds, and combined coarse/fine steps <=10 million.
Unknown, duplicate, missing, malformed and conflicting arguments fail nonzero.

## Conditional model and initialization

`src/reference/IG00151Reference.h` implements these independent equations:

```
dLI/dt = (LC - LI) / tauLI
gain = max(0, LI) * 2^EI * envelope
raw = gain * (input + offset) + feedthrough * gain
dcoupling/dt = (raw - coupling) / tauAC
output = raw - coupling
```

The source-derived LI capacitor (10 nF) and five branches (180k, 560k, three
270k ohms) give tauLI through their **inferred ideal-source** parallel
resistance. tauAC=1e-6*100000 seconds is the already-declared next-stage coupling
inference, not a measured output network. The multiplier law, SUB-to-EI mapping,
offset and feedthrough are also inferences. See
[IG00151_VCA_MODEL.md](IG00151_VCA_MODEL.md) for sources and alternatives.
Offsets independently spell the current declared float literals `1.5e-3f` and
`1e-3f`, promoted to double (0.001500000013038516 and
0.0010000000474974513), rather than silently substituting ideal decimal values.
Time constants use double arithmetic. No production solver is called by the
reference.

Initial LI defaults to the first LC; `--initial-li X` overrides it for explicit
startup experiments. Coupling starts at zero. The first output is the actual
instantaneous model value, which can be nonzero with a nonzero first envelope.
It is not a sample-period-advanced output or an invented silent prehistory.

## Integration and diagnostics

RK4 splits every interval at its knots, using `--steps` substeps (1..128,
default 8), then repeats the entire trajectory with twice that number. The
finer trace is exported. `--tolerance` (default .001, allowed (0,1]) bounds the
maximum **absolute** endpoint h/h2 difference in LI, coupling or output before
any output is created. It is a numerical refinement guard in engine units, not
a model/hardware acceptance gate. Differences are not divided by 15 or claimed
as rigorous error bounds: the zero-gain floor is nonsmooth when LI crosses zero,
and an endpoint comparison does not bound errors between endpoints. Increase
steps for tighter numerical studies. Both trajectories check finite states.

Optional `--compare-realtime true` runs actual `Ig00151Vca` at the same sample
rate, passing float-converted endpoint drives. It requires first envelope zero
and initial LI equal first LC. The time-zero process call then leaves both
production states unchanged and produces zero output. Later calls represent
one sample-period advance per endpoint. Reference forcing is piecewise linear;
production updates LI then coupling using its endpoint drives. Differences
therefore include forcing/clock discretization and float arithmetic, not merely
integrator accuracy. Only public `levelNode()` and `process()` output are
exported. There is no claim to observe production coupling, recover hardware,
or certify production parity. No comparison threshold is enforced.

## Artifacts and failure behavior

- `trace.csv`: N endpoints at 17-digit precision with sample, time, drives,
  LI, gain, coupling and output; optional realtime LI and output columns.
- `stimulus.csv`: exact byte snapshot parsed, hashed and rendered. No reread.
- `manifest.json`: units, initialization, solver, rates/counts, all constants,
  refinement tolerance/differences, optional production differences, input hash
  and byte count, configure-time source hashes for renderer, reference, SHA
  helper, production model header, model document and this workflow document.

CMake regenerates provenance when those sources change, including dirty edits.
Outputs omit source paths and wall clocks; the same executable/input/options
produce deterministic bytes. Cross-platform libm last bits may differ.
Existing output paths, including dangling symlinks, are rejected. The parent
output directory must already exist. Computation completes before directory
creation. Write/flush/close failures return nonzero and remove this run's owned output
files (including optional WAVs) and its directory if empty. Crashes or concurrent external mutations can
still leave partial outputs; consumers must require successful process exit.

## Optional diagnostic listening export

Use `--wav-gain 0.1` to export `reference.wav` from the fine reference `output`
column, plus `realtime.wav` from `realtime_output` if comparison is enabled.
The supplied finite linear gain in [0,1000000] applies equally to both; zero mutes.
Without this option no WAV is written. CSV values and numerical metrics are
unchanged. This is the isolated VCA output, not a complete instrument.

Start playback at low volume. These uncalibrated diagnostics are not hardware or
perceptual proof. Values above full scale are retained and may clip in players or
audio devices. No normalization, auto-level, clipping, limiting, latency shift,
trimming or resampling is performed. Each double output times the shared gain is
rounded once to float32.

Mono little-endian IEEE float32 WAV uses format 3, fmt extension size 0 and a
`fact` sample count. N samples include t=0 at i/rate: endpoint span (N-1)/rate
and playback duration N/rate are distinct. The manifest `audio_export` records
these, shared gain, exported float32 peak and count strictly above absolute 1.
Output is deterministic. Invalid sample range or write failure fails the run and
cleans up owned files. The shared helper hash is included in source provenance.
