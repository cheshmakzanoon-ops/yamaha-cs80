# VST3 TUNING REF automation

Append existing global P_tuningRef at lane80, for81 lanes. Only the production
roster changes. Existing415..465Hz/default440/linear metadata, IDs, DSP, helper
and51 historical contracts remain unchanged. Historical KEY RELEASE/floor
interaction fixtures permit larger runtime rosters while retaining exact80
contracts and full prefix checks.

## Existing global frequency path

Engine.cpp23 includes tuningRef in global smoothing;655 reads the parameter,
726 retains PURE effective440, and2883 uses midiToHz(pitch,tuningRef) before
existing ribbon/feet/key-ratio multiplication. The global parameter reaches
PANEL13/factory0/MEMORY11 without a rank mask bypass. No tuning-table/MTS,
filter-keyCV or oscillator law is changed. Protected Legacy/PURE compatibility
is separate from the modern positive witness.

## Bounded native qualification

Physical440Hz holds0/128, then432/452/438/460/425/446Hz at
511/1023/1791/2559/3583/4095 via existing normalization. All targets remain within
the interior domain. Unchanged default MIDI48 helper is used. PANELpulse1,
saw0,sine/noise0,PW.5/PWMdepth0,LPF4000Hz/HPF25Hz,resonances0,filterIL/AL0,
A.001/D.002,VCA A.001/D.002/S.7/R.5,VCFlevel.78,VCAlevel.35,mix.5,
portamento/ribbon/initBend0,MPEoff and neutral macros/dispersion/touch/LFO/
modulation provide a fixed sounding path.

Matched active440 holds require exact pre128 PCM and positive held effects in
frames[128,3072). Scalar/five partitions retain original tolerances. One global
lane ×sources13/0/11 ×capacities8/16 (two/sixteen notes) ×44.1/48/96kHz
×realtime/offline gives36 positive full cases. With36 frozen80 compatibility
cases:684 renders in12 processes,108 actual candidate81/frozen80 no-queue/
Legacy/PURE byte pairs. Six CMake entries use
`cs80_vst3_tuning_ref_automation_tests`, original host contract and new
`host-automation-tuning-ref-v1.json`,timeout180 seconds.

All 12 native processes passed: 36 full cases and 36 compatibility cases,
684 renders and 108 exact candidate/frozen PCM pairs. Maximum partition error
was 0; every initial prefix was exact. Held-window positive effect ranged from
0.0128054 to 0.582072. All 51 prior contracts and recorded source/SDK/compiler
identities remained unchanged through execution. The interrupted build was
resumed incrementally; both original and recovery build logs are retained.
Evidence: `docs/experiments/tuning-ref-automation/verification.json`.

No calibrated tuning or pitch accuracy, MTS,
filter invariant, hardware, GUI, CPU or whole-project completion is claimed.

## Deferred wheel-routing excitation

Existing mod-wheel SUB/VCF amounts multiply a wheel state reset to0. The current
supported helper emits notes/optional pressure but no CC1, so it cannot supply a
positive wheel-routing witness without an explicitly reviewed controller path.
A deliberate bounded CC1 fixture extension and unchanged-default regression are
subsequent work; this milestone neither modifies that helper nor claims wheel
routing qualification from unexcited controls.
