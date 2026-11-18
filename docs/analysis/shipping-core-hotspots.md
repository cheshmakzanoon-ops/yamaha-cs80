# Where the shipping eight-voice core spends CPU

On the profiled Linux x86-64 release build at 48 kHz and 64 frames, the oscillator family accounted for **42–45%** of samples in all three scenes. The exact batch filter solve accounted for about **16%** in held factory sound. When the eight factory voices received moving MPE controls, filter coefficient setup accounted for **13–15%**. These are exclusive sampled shares of standalone DSP, not savings estimates or native plugin measurements. [Six captures, workload, replay and limitations](../experiments/shipping-core-profile/README.md).

Two interventions tested those findings:

* **Reuse coefficients across different voice lanes in one low-pass pass.** The sole primary, factory-moving, measured -1.29% standalone CPU reduction (95% whole-pair interval -10.03% to +4.13%; primary floor 11.71%). Controls also had to clear their predeclared −3% lower bounds. See the [complete 256-observation screen and all four scenes](../experiments/lp-cross-lane-reuse/cost/README.md). Positive values mean less CPU; negative values mean more. Helper hits alone are not CPU evidence. [What was changed and how numerical, FP and lifecycle behavior were tested](../experiments/lp-cross-lane-reuse/README.md).
* **Avoid materializing a large oscillator endpoint copy.** Two independent revisions were rejected before cost testing. One changed the audio continuation after a floating-point trap; the other repaired that case but changed a retained public diagnostic event after a different interruption. Neither demonstrates an ordinary finite-audio regression. [Patches and executable counterexample sources](../experiments/oscillator-endpoint-copy/README.md).

The production DSP is unchanged by this research checkpoint. A hotspot percentage gives the next experiment a reason to exist; it is not evidence that eliminating that percentage is possible.
