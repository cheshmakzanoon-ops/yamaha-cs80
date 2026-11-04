# Rank idle clock: 3 kHz to 1.5 kHz

This is a **declared software scheduling approximation** for the silent,
free-running oscillator and two nodal stages, not a measured Yamaha clock.
It changes only `kIdleStepHz` and two source comments in
`src/experimental/ContinuousVoiceCircuit.h`. The complete source retains
the original awake oscillator/filter laws, full-rate wake reconstruction,
and distinct Legacy path. It deliberately changes a circuit-engine idle
trajectory; the V2 audio gate therefore needs a new full-SHA pin.

The original acceptance was registered before this experiment in
`tests/Ws3Tolerances.h` and the `ws3-keyboard`, `cold-idle-timing` and
`completion-dsp` tolerance files. Their exact hashes and registration
commits appear in `qualification.json`. None was altered.

| Unchanged acceptance | Candidate checks | Failures | Observation |
| --- | ---: | ---: | --- |
| Free-running idle/wake | 73,965 | 0 | Worst wake residual −43.33 dB against −40 dB; idle phase 4.36×10⁻⁵ cycles |
| Cold elapsed time | 2,121 | 0 | Maximum error 3.895×10⁻¹² s |
| Cold filter envelope | 13,966 | 0 | IL error and assignment capacitor jump both zero |
| Note-off continuation | 540 | 0 | Existing capacitor/time assertions retained |

A real source mutation changing the clock to **750 Hz** fails two original
free-running wake assertions (−39.298479 and −38.616325 dB). The restored
1.5 kHz source passes. Raw mutation and restored-test logs are archived here;
compilation failure was not counted as detecting the defect.

The final integration was compiled against the continuing feature branch and
passed the same four CTest gates. Before re-pinning, local compatibility
reports Legacy default, Legacy tails and V1 byte-identical, while the existing
V2 `6300163` pin differs as expected for a changed idle law. A separate audio
commit, separate V2 pin commit and strict compatibility/STATUS checkpoint
record the required landing sequence. Three serial whole-engine benchmark
rounds before this change are in the `cpu/unused-lfo` checkpoint. After-change
rounds are recorded in a separate checkpoint; neither CPU target is assumed.

The retained reference is software continuation. No analog hardware voltage,
idle oscillator clock or component value was measured in this experiment.
