# Cold idle scheduling and physical elapsed time

2026-10-31. Numerical investigation; no hardware measurements. The existing
M41 reading keeps the WSC and filter running while TR is released and the VCA
is closed. The existing cold key, phase seed, one-second oscillator warm-up
and SH7 held-K leakage remain declared initial/model choices. They are not
parameters to fit against the click metric.

## Mechanism under investigation

`Engine::reset` initializes `idleClock` with a positive per-voice stagger.
The first silent step then uses that counter as its physical elapsed sample
count, although part of it was supplied by the scheduler. The initial
`IdleRankControls` simultaneously has frequency zero. A centered idle window
uses that zero frequency over its first half interval. Those two time errors
can partially cancel; they do not make the trajectory correct.

For voice6 at88.2k internal rate, the idle interval is29samples and the
positive stagger21samples. Only8samples really elapse before the first step,
whereas half of the nominal interval is14.5samples. The resulting6.5-sample
phase excess predicts about0.0204cycles at the cold key frequency. At96k,
the32-sample interval and24-sample stagger predict8samples, about0.0232cycles.
These explain the observed scale; the registered test will evaluate the
actual state rather than accepting this arithmetic prediction as proof.

## Registration before candidate evaluation

`tests/parity/tolerances-v2/cold-idle-timing.json` fixes the cold-clock fixture
and limits. Exact elapsed-time conservation allows only1e-8s accumulation
rounding over6.25s. The phase limits remain the original ws3 values:1e-4cycles
for static controls plus held-K droop, and2e-3 for the original Bass phrase's
moving controls. All original wake residual gates remain mandatory.

The cold test uses the existing oscillator, independently clocked at every
internal sample from the same captured initial state and stated SH7 frequency
trajectory. Every one of the16rank chains is checked, including wake before
the first scheduled idle step. It does not use the approximate scheduler as
its reference. Mutations must reintroduce false elapsed stagger or an unseeded
first frequency. The original click scores and all their windows stay fixed.

The candidate separates scheduling from physical time: a first-step threshold
may stagger work, while the elapsed counter starts at zero. A single actual
internal sample initializes the previously unclocked cold oscillator/filter
controls before decimated operation; it adds no elapsed time beyond that sample.
No fade or alternative envelope law is part of the proposed correction.

### Oracle implementation correction before candidate evaluation

The first test implementation incorrectly held charge at zero throughout the
one-shot. The registered model instead raises charge at
`1 / resetSeconds - frequency` during reset, then discharges at `frequency`.
The incorrect phase results from that initial test are discarded; its elapsed
time comparison was independent and already showed up to291.667microseconds
of fictitious time. The charge-ramp term is corrected before any production
candidate is evaluated. An added independent constant-frequency self-check
requires the oracle to match `phase = 0.25 + f*t (mod 1)` after two seconds
within1e-9cycles at250,277and2,048Hz. Engine phase/time limits are unchanged.


## Split-engine harness adaptation before integration evaluation

The qualification branch starts at `068456506d8bb9c8606059ae551045b4d7de4e08`.
Its structure-of-arrays engine owns contiguous rank state and exposes references
into that storage, so copying or moving an Engine is deliberately disabled.
The checkpoint test therefore reconstructs an independent engine and replays
exactly the same reset, 32-frame prehistory, reset, and render boundaries before
assigning its eight voices. Assignment still advances no audio sample. The
charge oracle and all previously registered limits are unchanged. The prior
32-frame run also checks that an explicit reset re-establishes cold scheduling.
This is a test-harness adaptation, not a new engine-copy or state-transfer API.

## Published integration qualification

Audio repair `46a7a5a2ce077e95b9025de81da85b3480134176` was pushed before
the separate V2 pin `6ab46dc8e1f75f4b559b279c54a254ace0480fbd`. It retains
the global/rank/mix/output split, 16-chain storage, existing caches and
3,000 Hz idle cadence. The physical counter starts at zero; the separate
first-step threshold staggers work. One real cold sample seeds the controls,
without an extra shared RNG draw or a changed closed-idle VCA treatment.

| Registered quantity | Before | After | Unchanged limit |
| --- | ---: | ---: | ---: |
| Elapsed-time error, seconds | 0.000291666673046 | 8.00071120466e-12 | 1e-8 |
| Cold phase error, cycles | 0.0490844331589 | 6.82538964014e-7 | 1e-4 |
| Original Bass phase versus full-rate voice 6, cycles | diagnosed cold phase defect | 0.00010704994201 | 0.002 |

The fresh integration test goes from 1,928 failed checks to zero out of
2,121. The original free-running wake, cold filter charge and note-off
continuity gates also pass. The retained isolated qualification and all
three mutation rejections use byte-identical core and focused test sources;
[the evidence README](evidence/continuation-2026-10-31/cold-idle-soa/README.md)
records that relationship rather than relabelling old runs as fresh ones.

The compatibility script passes Legacy default, Legacy tails and V1 unchanged.
After the separate pin, `scripts/check-compat-gates.sh --strict-v2` also reports
V2 byte-identical to `46a7a5a`. Raw logs are in the evidence directory.

The broader 86-test integration run exposed one pre-existing touch-smoothing
fixture mismatch and two generated executable-permission failures. The
permission issue is environmental; the unchanged filter-cache test passes
after its execute bit is restored. The smoothing fixture compares a warm
filter capacitor at IL with a freshly prepared capacitor at zero, despite
the intentionally preserved charge law. The same failure is reproduced at
pre-repair `780658d`. Its correction is qualified separately; this timing
repair does not declare the whole suite green. Three-round CPU integration
timings and the final full suite are recorded at subsequent checkpoints.

The physical-PANEL Bass retrigger scores after this repair are 0.571043 at
44.1 kHz and still above one at 48/96 kHz. The electrical-programmer and
event-omission results remain separate. No click window, attack time, VCA
pedestal, gain, coefficient, control coordinate or fade was changed here.
