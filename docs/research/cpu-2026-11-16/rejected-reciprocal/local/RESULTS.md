# Standalone numerical/assembly gate completed

Terminal0 on GenuineIntel; strict GCC -O3, contractionoff, noFMA. Fixed1,100,013 finite sample points (grid/random plus threshold/clampneighbors); all exceptional/control fallback checks passed. Selected approximate path3,300,046times including odd/adjacent probes.

Maximum sampled absolute difference versus current rational8.1046280797636427e-15; derivative expression1-t² difference1.609823385706477e-14; sampled oddness error0. Conservative analytic bound6.1144168944847653e-13, below frozen1e-12 local gate. This is not proof of global solver/audio stability or inaudibility.

Adjacent representable-input inversions: baseline262460, candidate326405. Both rounded functions fail strict discrete monotonicity on this dense nextafter test; candidate has more inversions. No threshold adapted, no monotonicity clearance claimed. Maximum inversion magnitude was not part of this fixed reported fixture. Continuous derivative and closed-loop effects require separate approval/test design before integration.

Actual assembly candidate1f68..1fa7: CVTSD2SS, shuffle, RCPPS, CVTSS2SD; two literal rounded Newton stages each multiply/subtract/multiply, then numerator multiply. No FMA or double division in admitted calculation. Original baseline retains DIVSD. This adds conversion/shuffle/reciprocal and four extra double multiplies/two subtracts versus one division; divider latency/throughput savings are hardware-dependent and may be outweighed by dependency chain/packing/guards. This scalar standalone mechanism is not yet a packed AVX implementation or cost proof. Counter overhead is diagnostic only.

No Engine source edits, solver thresholds/caps changed, native CPU timing, calibration retry or publication. Recommend independent proof/fixture audit, then decide whether inversion behavior and expected cost justify an isolated solver-fidelity experiment; do not promote on local1e-12 alone.

## Pre-authorized supplemental inversion magnitude analysis

Separate supplement.cpp/executable reruns the IDENTICAL frozen sample sequence and fixed gate, adding only maxinversion counters; original source/binary/results unchanged. Terminal0, same original counts/errors. Worst baseline inversion8.8817841970012523e-16, candidate9.9920072216264089e-16. Additional inversions are at rounding-scale in this sample, but neither sampled bound nor count establishes monotonicity globally. Original localerrorgate unchanged.

For a future packed4lane implementation, double→float→double conversions can be packed, reducing scalar broadcast overhead; still two refinements require four serially dependent multiply and two subtract operations, then numerator multiply, replacing one packed divide. Independent denominator lanes may pipeline but solver evaluations depend on earlier results. Therefore fewer expensive divisions is only a latency/throughput hypothesis; likely more guest instructions, no established material cost gain. Require actual target packed codegen plus a separately qualified stable-host cost test before any benefit claim. No blanket coefficient/solver approximation follows from this result.
