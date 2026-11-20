# Split-word admission in the existing IG00156 stage quartet

The strict GNU x86-64 AVX2 solver still advances the same four stages. Only the
common-case **eligibility checks** change: shuffle each binary64 word’s upper
32 bits into integer lanes, and check two coefficient/history vectors at a
time. The numerical ceilings (1,000,000 for static coefficients, 1,024 for
history) have zero low words. A strictly smaller high word therefore proves
the corresponding magnitude is below the ceiling. An equal high word does
**not** decide the result: use the prior ordered checks to distinguish the
allowed endpoint from its out-of-range, one-ULP neighbor. Signs and minimum
normal gains are checked separately. Two adjacent clock/validity flags are
compared together as object bytes. A non-normal restored slope, including
signed zero or a subnormal, still gets its original ordered FP observations.

There is no new stage, approximation, clock, solver iteration bound, timing
target, or change to the scalar path. The explicit GNU nontrapping profile keeps
the preceding checks.

## Boundary and state evidence

Against `5499056a60ee5044275ba17eb7dddded4c96e90d`, **161,734**
direct/wrapper cases per compiler (20,760 accepted) produced **199,741,490**
identical transcript bytes per arm. Observations include every recorded
coefficient and capacitor/slope field, outputs, admission status, MXCSR,
x87 status and errno. The independent cases cover all four rounding
directions and DAZ/FTZ combinations, seeded flags, mixed lanes, endpoints,
signed zeros/subnormals, nonfinites, and direct rejections with unmasked
exceptions. Rejections by the raw kernel also check that state/output did not
change. The focused permanent suite passed 1,207,674 checks per arm. The
explicitly scalar-disabled and GNU nontrapping objects remained byte-identical
within each compiler. See [GCC 13.3](gcc13-oracle.json) and
[GCC 14.2](gcc14-oracle.json).

The public network fixture uses an exactly representable binary conductance
and a calibrated input capacitor to place just one coupling word on either
side of the limit. The [negative controls](mutations.json) show that
incorrectly widening the static high-word test, widening the dynamic one,
or discarding the ordered fallback all fail the permanent suite.

## Counted component work

Strict GCC 14.2 Linux Release/PIC; each first window is 256 actual four-stage
kernel calls following 256 warm calls, plus the deterministic drive and
checksum. These are Callgrind guest instructions/reads/writes for this
fixture, **not** audio-thread time, a VST3 or physical CPU percentage.

| Quartet | Guest instructions, stock → candidate | Reads | Writes |
|---|---:|---:|---:|
| HP, LP, or mixed physical networks | 145,051 → 139,163 (−4.06%) | 36,971 → 34,667 | 11,758 → 11,502 |
| Custom input capacitance, ordinary finite coupling above two | 145,051 → 139,163 (−4.06%) | 36,971 → 34,667 | 11,758 → 11,502 |
| Synthetic coupling **exactly** at the 1,000,000 admission ceiling, zero drive | 119,054 → 239,886 (+101.49%) | 30,469 → 52,997 | 11,011 → 18,179 |

The adverse case forces the more expensive ordered fallback on *every* call;
it is not representative of the built-in networks, and is retained rather
than averaged away. All arms agreed on native and translated output hashes.
Each included an independent identical-window repeat and a separately
measured following window whose marginal work added to the two-window
difference. [Raw arm counts](component-work.json).

**Replay** from this checkout using Python 3.11+, a strict GNU C++20 compiler
on an AVX2-capable x86-64 Linux host and a Git checkout containing the stated
basis. For work, install Callgrind and its client headers. Each invocation needs
a fresh output directory. `--cxx` and `--valgrind` select executables.
The driver freezes the common headers and static oracle at the cited basis;
the candidate and updated permanent tests come from the working checkout.

```sh
python3 replay.py /tmp/cs80-filter-oracle --mode oracle
python3 replay.py /tmp/cs80-filter-mutations --mode mutations
python3 replay.py /tmp/cs80-filter-work --mode work
```
