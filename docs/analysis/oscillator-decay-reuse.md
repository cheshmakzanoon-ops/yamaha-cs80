# Invocation-local oscillator decay reuse: no advance

The narrow candidate reused an already-computed decay only when the later
positive finite interval was bit-identical. It changed no persistent state,
quotient, root solver or residual processing.

Scoped exactness passed: 6,011,358 full-API/output/state/FP-status checks under
normal GNU14 release flags, with supplementary native-ISA stress and effective
intentional-fault controls. This is not universal or cross-compiler proof.

The frozen component wall-time trial retained all 384 rows. Apparent gain was
0.6067%, with a 95% interval of −1.9276% to +3.1633%. A/A noise reached 23.0605%,
and 10 of 12 case uncertainty guards failed. No useful benefit or regression was
established. **Do not advance or promote this candidate on these results.**

No production source, registered tests or CMake configuration changed; no new
plugin pair was built. See the [complete experimental archive](../experiments/oscillator-decay-reuse/README.md)
for tested sources, exactness controls, raw rows, frozen plan and independent
verification. These wall-time results are not plugin callback CPU measurements.
