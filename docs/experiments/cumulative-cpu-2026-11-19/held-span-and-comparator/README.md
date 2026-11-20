# One exact short-interval solve for a held oscillator

Only the existing masked, nearest-rounding GCC 14.2 Linux x86-64 borrowed
oscillator entry changes. The direct tick, idle evolution, unsupported
compilers and ordinary interrupted path retain the original equations.
The oscillator has no new persistent field. No Hz, sample rate, voice,
knot, event tolerance, residual term or delay changed.

* The comparator's finite gated value equals its constant and its slope is
  +0. Its terminal coefficients are +0, so the original result is the same
  comparator plateau **plus the live residual**. The addition and buffer
  clear are retained even for a muted pulse, signed zero or signaling NaN.
* The original event search and physical integrator separately compute the
  same pin slope, exponential ratio, decay and coupled input endpoint for
  the current candidate duration. Carry those original rounded operands
  inside that one segment. The search still checks every level and turn;
  as soon as it accepts a shorter bound, it calculates the decay at the
  returned duration. An actual event takes the unmodified physical advance
  (including a boundary exactly at the limit). On an unshortened span the
  original triangle, charge and capacitor updates consume the shared
  operands. Nothing is cached across ticks or across segments.

### Results

Real Linux VST3 controls, 48 kHz, 64 frames, against product parent
`9208eb6323207b5054055dd2e0549e45324c539a`:

| Four genuine callbacks | Guest instructions | Guest reads | Guest writes |
|---|---:|---:|---:|
| Fresh quiet | unchanged | unchanged | unchanged |
| Eight equal held | −2.623% | −2.796% | −1.397% |
| Eight varied held | −2.611% | −2.790% | −1.397% |
| Varied wake | −1.874% | −2.058% | −0.966% |

[Full A/B receipt](vst3-from-base.json) and [increment over the isolated
comparator change](vst3-from-pulse.json). All duplicate-prefix and
independent next-callback arithmetic controls passed. These are translated
operation counts, **not** elapsed CPU, device, deadline or DAW measurements.
The separate [native capture](native-histories.json) matched every public
sample and observed state/event, FP/errno and latency field across 27
histories (82,944 callbacks and 3,286,656 stereo frames per arm).

At each of GCC 14.2 `-O1`, `-O2`, `-O3` and `-Og`, the direct paired
matrix matched 323,584 ticks (261,095 borrowed) and 1,941,534 assertions.
It checks all three pins, every exposed residual and event slot, private
right nodes and status, MXCSR, x87 status/control and errno. It includes
nine legal model profiles and eight clock rates, 8–1536 kHz, changing FP
preparation, tiny restored reset countdowns and poisoned residuals. A
negative control falsely retained the original duration after an event,
causing 413,188 failures in the 114,688-tick release-primary matrix. The
registered test adds 6,912 borrowed targeted residual cases, with all 16
cursor positions and muted or both comparator plateaus.

The actual linked renderer matched a 17,019,552-byte interruption and
continuation capture across 22 cases, including seven real SIGFPE cases
per arm and four follow-up samples per case. SHA-256:
`2eb309790f0460b355a753fe9c3cd30ef91eae7572078a362419da743d7d96ec`.
Separate direct output/elapsed interruption probes found no mismatching
complete 5,984-byte oscillators, diagnostics or continuation on the three
output lanes and valid/invalid cache seeds. This does not prove all
possible interrupts. The actual ordinary GCC 14 Release `Engine.cpp`
object was byte-identical to its parent (SHA-256
`f9b608c0cac85ba8177aaefb0e64f11032efa70e6a1f60945fa4fecc168f9af9`).

[Component work](component-work.json) counted a separate 16-oscillator
mixed-plateau instrument: 3,182,610 → 2,903,566 guest instructions in 4,096
admitted ticks. Reads and writes also fell. Several exploratory arrangements
instead increased writes and were not retained; an additional muted-saw
terminal shortcut worsened the comparator-only mixed case. The real VST3
fixtures above are separate from this selection fixture.

### Isolated replay

From the repository root on a matching GCC 14.2 Linux x86-64 machine:

```sh
here=docs/experiments/cumulative-cpu-2026-11-19/held-span-and-comparator
python3 "$here/replay-direct.py" . /tmp/new-span-direct --mutation-check
python3 "$here/replay-component.py" . /tmp/new-span-work
python3 "$here/replay-traps.py" . /absolute/stock.so /absolute/candidate.so /tmp/new-span-traps
```

Each output directory must be new. The component study needs Valgrind 3.24
Callgrind and its public headers; the FP and interruption studies run
natively. Use `--optimization O1`, `O2` or `Og` for direct sensitivity
controls; no fast-math flags are used. For the linked trap replay, use the
two actual unstripped VST3 binaries with matching headers/dependencies.
