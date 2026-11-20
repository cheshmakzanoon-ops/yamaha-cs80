# Exact linear delay adjacent-sample addressing

For a prepared linear delay line the original clamp, binary32 negative wrap
and rounded-endpoint guard already place the first cell inside the ring.
Consequently `(i0 + 1) % size` is zero only at the last cell and otherwise
is `i0 + 1`. Only this integer address selection changes. There is no new
buffer, processing mode, interpolation, clock, latency or state. The Hermite
reader and integer-tap API are not part of the change. The classic stereo
plate calls the linear reader twelve times per internal sample; classic
chorus and tape use it as well. This is **not** an optimization of the OE2/BBD
chorus or the modern plate model.

The stock header is from `5499056a60ee5044275ba17eb7dddded4c96e90d`. The
[receipt](receipt.json) identifies both headers and the compiler/flags.
A separately compiled GCC 14.2 Linux x86-64 reader emits integer `idiv` in
stock and compare/conditional move instead in the candidate.

## Checked scope

- The existing portable delay test and a modulo-address oracle covering
  every head and quarter-sample tap in 4 through 19-cell rings, including
  copied running state, pass (1,141 checks). ASan/UBSan also passed with leak
  scanning disabled; that is not a leak qualification.
- 50 separately compiled stock/candidate effect histories, 131,072 internal
  stereo samples each, include active copy, repeated reset, restoration and
  rate reprepare. Raw audio words plus MXCSR with newly cleared status for
  each sample match in **78,643,200 bytes per arm**. Rates 16, 88.2, 96,
  384 and 1,536 kHz; classic chorus/plate; all four rounding directions plus
  nearest/FTZ+DAZ.
- 222,264 independent prepared-reader edge records match, including **20,809
  actual SIGFPEs per arm**. Records cover returned sample bits or signal/code,
  interrupted SSE control/status, x87 control/status and errno. Sample cells
  include signed zeros, subnormals, finite extremes, infinities, quiet and
  signaling NaNs. The old reader has no defined float-to-index conversion
  for a NaN **delay**; this does not add one. Binary output SHA-256 is in
  the receipt; raw bytes are not shipped.
- A separate switch of the existing float wrapping loop to a single `if`
  produced a directed-rounding mismatch in an optimized effect render and
  was left out. The spring cascade memory-layout experiment also was not
  retained; its preliminary timing changes at 96–1,536 kHz were neutral to
  slightly adverse. Neither is present in the candidate header.

## Exploratory timing, not a CPU-budget result

In a single cloud Linux/x86-64 guest using one allowed logical CPU and
`CLOCK_THREAD_CPUTIME_ID`, the isolated classic plate's median paired time
changes were **−11.93%, −7.51%, −6.24%** at 88.2/96/384 kHz.
For the isolated classic chorus they were −4.57%, −8.26%, −1.42%. Eight
paired trials per row, four each way, unchanged audio checksum. Intrinsic
variance was large: individual pairs went the other way in five of the
six rows. Setup and hashing are outside the timed loop. These are effect
fixtures, not VST3 timing, a factory-patch speedup, or a physical-target result.

## Replay

From this directory on Linux x86-64 with GCC and Python 3:
```sh
python3 replay.py /path/to/stock /path/to/candidate > exact.json
# Optional: select one available logical CPU and retain all paired timing rows.
python3 replay.py /path/to/stock /path/to/candidate --cpu 4 --timing > run.json
```
