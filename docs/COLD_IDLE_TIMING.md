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
