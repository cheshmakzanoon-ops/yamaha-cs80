# Candidate VST3 KEY RELEASE automation

Append existing global P_keyRelease at lane79, for80 lanes. Only the production
roster changes. Existing0..5s/default.18/skew.5 metadata, IDs, DSP, shared helper
and49 historical contracts remain unchanged. Historical held-ribbon/release-mode
fixtures allow larger runtime rosters while retaining exact79 contracts and
full prefix checks.

## Global live floor and policy

Engine.cpp625 reads keyRelease globally; refreshEnvelopeTimes213 and note-off
use max(rank release,keyRelease,gated masterSustain*10) with existing scale and
tolerances. Live `setTimes` preserves state/stage rather than applying ordinary
rank gain smoothing. This is not a rank-source-masked parameter. PURE policy
retains stored values but sets effective keyRelease0 (Engine719). The protected
Legacy/PURE flattening comparisons do not claim the modern positive behavior.

## Chosen source floors and bounded trajectory

Key release starts.5s and holds0/3100, then takes physical .4/.8/.6/.9 seconds
at3327/3583/3839/4095 via existing skew conversion. MasterSustain0, neutral
macros/envTimeScale1,dispersion0,static sustainMode0 and no held pedal isolate
the key floor. PANEL releases fR.30/vR.28 match default MEMORY11 values.
Factory0 uses the ordinary51A/53A source mapping: rank0 fR≈.0209618/vR≈.0221377,
rank1 fR=.01/vR≈.0910298. These are source-derived values, not measurements;
see also the source-floor discussion in [MASTER SUSTAIN](VST3_MASTER_SUSTAIN_AUTOMATION.md).
Thus minimum key release.4 exceeds all selected rank floors. No general claim
about every factory tab or floor-crossing behavior follows.

PANEL bothA.001/D.002,filterIL.5/AL.3,VCA S.7 and normal pulse/filter settings
provide a settled held signal before unchanged helper offs3072..3087. Explicit
pulse1,saw0,sine/noise0,PW.5/PWMdepth0,LPF4000Hz/HPF25Hz,resonances0,
VCFlevel.78,VCAlevel.35,mix.5,zero touch/LFO/modulation are used. Tail duration
below.024s with minimum release.4 retains more than.66 of its initial gap.
This is finite tail excitation reasoning, not calibrated stage timing.

Matched active constant holds require exact pre3100 PCM and positive tail
frames[3100,4096) for all sources13/0/11. Scalar/five partitions retain original
thresholds. One global lane ×three sources ×capacities8/16 (two/sixteen active
notes) ×44.1/48/96kHz ×realtime/offline gives36 positive full cases. With36
frozen79 compatibility cases:684 renders in12 processes,108 actual
candidate80/frozen79 no-queue/Legacy/PURE byte pairs. Six CMake entries use
`cs80_vst3_key_release_automation_tests`, original host contract and new
`host-automation-key-release-v1.json`,timeout180 seconds.

The qualification is limited to the finite tail described above. No complete release, all-factory, floor-crossing,
hardware, GUI, CPU or whole-project completion claim is made.

## Verified local result

[Retained verification](experiments/key-release-automation/verification.json)
records the official candidate VST3 and supported-host build (exit0), followed
by one bounded direct matrix: all12 processes passed,36 full cases and36
compatibility cases,684 renders. All108 actual candidate80/frozen79 PCM pairs
were byte-identical. All36 initial prefixes were exact; maximum partition
error was0 against the unchanged2e-6 threshold. Released-tail positive effects
ranged from.000378611 to.0068754. No failures or retries occurred.

SDK/compiler/Python and source identities were recorded before compilation and
verified unchanged after runtime. Final host/module identities were recorded
before runtime and independently checked afterward. Final result prose was
added only after that snapshot. All49 prior contracts, production files other
than the capability roster, existing exporters and shared renderer remained
byte-unchanged. Historical capability guards retain their full79 prefixes.
The six CMake registrations were added but CTest was not executed. Raw PCM and
native binaries remain in the ignored local workspace; retained evidence
contains their hashes and logs, not a complete replay bundle. This is not
hardware, complete-release timing, or floor-crossing qualification.
