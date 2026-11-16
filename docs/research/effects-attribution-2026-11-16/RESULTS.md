# One fixed effects-enabled held window

Terminal run passed after a preserved compile-only diagnostic repair (mixed enum initializer-list changed to explicit integer array). No DSP/workload change or rendered data was discarded. Native and Valgrind produced identical 16,384-byte PCM and named-state/control streams; all16 outer MXCSR status records also matched in this run. This is observed equality, not universal hidden-state/FP equivalence.

The declared eight voices, source selector0,48k/128,2x96k and applied classic FX controls were verified.750warmup callbacks preceded16collected callbacks. Two seconds is not a claim of fully settled reverb. All linked baseline archives match the previously frozen selector10 baseline receipts.

Client dump summary and footer both contain191,153,384 simulated guest instructions. Executed modern FX-slot path has12,288 calls and5,168,146 inclusive instructions (2.704% of the complete callback window). Its self instructions, summed across disjoint source-file rows, are4,079,445. Classic Chorus is an executed child with4,096 calls,818,365 self and1,088,701 inclusive instructions (0.570% of total). Its8,192 libm calls account for270,336 instructions (0.141% total); the preserved baseline address receipt maps target0x83e40 to __sinf_fma. These child/ancestor inclusive costs overlap and MUST NOT be summed.

Plate reverb is inlined: positive instruction rows inside Reverb::process's comb/allpass loops and FxSlot case4 establish its execution. There is no separate Reverb call count or trustworthy standalone complete cost from merely summing the function's own source lines: inline DelayLine/vector operations carry other line locations. The combined modern-slot inclusive value above bounds the entire observed slot path in this instruction accounting, not its cycle share. No large hidden effects/libm hotspot appeared.

The separate original BbdChorus path still executes even when switched off by design; it is not the newly enabled modern-slot chorus. Its inclusive4,640,768 instructions must not be misattributed to the new effects configuration. AVX filter path has32,768 actual calls.

## Stop decision

This closes the specific coverage gap but does not establish a substantial new optimization target. Do not compare against the old dry34-warmup profile as an FX delta, infer CPU speed from instruction shares, alter production, or launch another case/sweep. No native timing was collected. Existing known oscillator/filter/readout families remain dominant; no new candidate is proposed.
