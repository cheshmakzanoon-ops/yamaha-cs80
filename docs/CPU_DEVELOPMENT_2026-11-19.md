# CS-80 CPU development checkpoint, 2026-11-19

The project is lowering work without lowering the quality setting, skipping
physical voice clocks or changing preset sound. The [Improvement Plan](IMPROVEMENT_PLAN.md)
sets a target of at most 0.15 realtime factor for eight source-mode voices at
48 kHz/2× and 0.02 for idle. This checkpoint **does not establish those targets**.

## Shipped reductions and known costs

Each comparison observes a different change against its immediate
predecessor. Guest instruction counts identify work, not CPU time, and do
not add up across rows. These are scoped Linux Release observations.

| Change | Work removed in the tested setup | Retained limits |
| --- | --- | --- |
| [Unchanged source routes](experiments/stable-source-route/README.md) | Removes two descriptor copies from an unchanged control frame; 863 fewer guest instructions per 64-frame callback in short quiet and held cases. | Other controls, smoothing and readout reset still run. Whole-instrument time not established. |
| [Equal source cutoff drives](UNIFORM_SOURCE_CUTOFF.md) | 9.855% fewer guest instructions in fresh quiet and 1.385% fewer in the short equal-velocity held example. | Varied-velocity release often does not qualify. |
| [Exact signed-zero FIR](EXACT_SILENT_OUTPUT_FIR.md) | 13.71% fewer guest instructions in fresh silence and 13.43% fewer after the selected release settles. | 0.037% higher guest instruction count in the short held case; signed mixed/dirty histories and unsupported arithmetic use original output. |
| [Packed heterogeneous cutoff](HETEROGENEOUS_SOURCE_CUTOFF.md) | 5.092% fewer guest instructions after varied-velocity release, 1.438% fewer varied held and 0.942% fewer in the varied wake fixture. | 0.060% higher fresh quiet guest instruction count. Scoped to the qualified Linux/x86-64/GCC arithmetic. |

Release builds were compared through a real VST3 host for the listed
changes. The individual reports retain byte comparisons, numeric controls
and replay evidence. State refers to the particular public component
state and explicitly recorded FP/audio observations; it is not blanket
equivalence for all hidden DSP state, presets, machines or compiler modes.

## What native measurements do and do not show

The [native stock profile](experiments/native-idle-profile/README.md)
observed source readout work at about 28–32% of quiet callback samples;
about 44% of held callback samples were in oscillator code. Those are
attributions from sampled program counters, not removable fractions.

The [clock test](../tools/vst3-target-qualification/cloud-diagnostic/README.md)
finished all 12 stock-only triplets. All three numerical calibration gates
failed: even the same ELF did not establish the required small-effect
equivalence. No candidate was timed in that run. Translation tests offer
useful reproducible differences in software work but do not substitute
for a qualified native clock, particularly around tiny release tails.

The next large-effect test calls the same musical indices in two independent
native hosts. It uses fixed quiet, release, held and wake workloads and a
stock-only gate before any candidate time. No DAW or physical-machine
measurement is inferred from this shared-cloud design.
