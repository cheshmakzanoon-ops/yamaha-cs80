# Static deterministic non-sine LFO interaction design

This reviewed design completed its bounded run. See the
[verified results and limits](experiments/lfo-static-shapes/README.md).

Test-only coverage of triangle, saw up, saw down and square under simultaneous
rate/depth automation. Exact published89 module remains unchanged. No waveform
switching or random waveforms. All active LFOs share the same static waveform per
case; mixed-wave combinations excluded.

96 cases:4 shapes x4 isolated/combined routes x3 sample rates x2 process modes.
Unlike the prior sine fixture, this reduced matrix uses only manual/manual13/13
sources and8-voice capacity(actual2notes). Do not claim factory-source or16-voice
non-sine coverage. Reuse reviewed independent scalar evaluator, interleaved knots,
both ablations, queue-order, Legacy/PURE/off controls and initial1024-frame hold.
16 renders/case =1536renders;384captures,288scalar/block comparisons,96queue-order
checks. Captures are not cross-build pairs.

## Pre-run edge coverage design

The old8/11/14Hz history could validly have no square edge in the held window at
96kHz. Choose initial18/19/20Hz and endpoints20/16/20/18Hz at1535/2047/3071/4095.
All rates stay16..20Hz; existing non-sine double phase and smoothing unchanged.
For initialized convex rate smoothing, the first half-cycle lies between.025s
and.03125s: samples1102.5..1378.125 at44.1kHz,1200..1500 at48kHz,2400..3000 at96kHz.
Each lies inside held frames[1024,3072). This targets square edges and saw wraps
without lengthening the renderer or weakening positive-effect/audio tolerances.
Source review must confirm smoothing initialization/clock semantics before run.
Triangle has no discontinuous jump; all shapes remain bounded finite histories,
not all-phase/full-cycle accuracy. A returning rate never restores phase.

Depth knots remain0.65 through1023 then0.3/0.8/0.25/0.7 at1279/1791/2815/4095.
Preserve all old contracts/source/tests; new fixture/script independently reviewed
before execution. Pin exact native module and runner hashes before/after; actual
capture/evidence audit before publication. No DSP/UI edits, broad host/scanner,
malformed inputs,GUI,benchmark replay or sanitizer workarounds. Exclude DAW/AU,
Windows/macOS,CPU,hardware fidelity and release qualification.

Independent source review confirmed zero-reset phase, first-sample target snap,
convex clamped one-pole smoothing and internal-rate clock semantics. Thus the
edge-window argument holds with sample-index rounding; this is crossing coverage,
not a guarantee of audible ablation difference. Keep measured effect gates.
