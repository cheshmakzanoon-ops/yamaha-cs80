# Exact whole-trajectory legacy SUB oracle repair

This bounded test-only repair starts from published source `4ef0666`. The native
GCC 14.2 -O3 -march=native core test has four false failures: its independent
reference splits `speed += (target-speed)*.0015f`, while unchanged Engine fuses
the product/add. The first speed discrepancy is one binary32 ULP at host frame
398 of the existing 7 Hz / 512-frame scenario. Production arithmetic, compiler
flags and compatibility pins must not change to make the test pass.

## Explicit arithmetic contract

Use exactly two independently maintained reference trajectories. Both round the
float subtraction; one explicitly uses std::fma for product/add, the other
forces a rounded binary32 product then a rounded addition. Each owns speed,
phase, held sample, started flag and test-owned xorshift RNG. Do not reuse the
production Lfo or Rng implementation. Compare finite exposed speed AND waveform
value by float bit pattern at every observation. Two sticky eligibility bits
may only be cleared: at least one SINGLE trajectory must match the ENTIRE
scenario. Never choose independently per field/sample or revive a failed track.
Sine uses the same runtime libm, not a cross-libm golden or physical-fidelity
reference. Phase-domain power-of-two waveform scalings are exact; this repair
does not permit arbitrary rounding variation, fast-math or arbitrary FMA sites.

Keep fresh Engine instances in Legacy, no notes/arpeggiator/modern routes, and a
fixed waveform per scenario. Engine reset retains legacy speed and modRng;
therefore this test does not incorrectly restart their histories or qualify
reset/reprepare behavior. Leave existing surrounding lifecycle tests intact.

## Decisive gates

Retain the exact 7 Hz / 512-host-frame scenario for all six waves. Add all six
waves at 44.1/48/96/192 kHz: constant 30 Hz for 0.4 s and 7→30→0.05 Hz over three
0.15 s segments. Assert actual Legacy policy, host/internal rates and factor2;
require multiple wraps and correct independent RNG-draw counts, rather than
claiming sample/hold coverage from a non-wrapping short trace. No waveform,
speed, RNG or count tolerance. No runtime source edit or timing claim.

Test the validator itself with complete fused/split traces, persistent rejection,
nonfinite fields, mismatched speed/value, and a splice whose distinct prefix
and suffix each distinguish the two trajectories. One-ULP corruption must be
outside BOTH allowed outputs at the observation, not accidentally the other
legitimate trajectory. Source mutants must reject changed smoothing coefficient,
skipped/extra RNG draws, phase reset/wrap and missing internal tick. Build/run
GCC and Clang generic/native against matching unchanged core libraries, run
sanitizers and the neighboring default core gates. Preserve the original native
four-failure negative control. Require independent source/test review. Do not
claim native platform/DAW or hardware completion from this gate repair.
