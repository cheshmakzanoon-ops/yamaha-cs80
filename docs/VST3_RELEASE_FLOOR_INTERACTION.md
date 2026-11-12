# Candidate release-floor interaction qualification

Test-only interaction of existing masterSustain lane36 and keyRelease lane79
within the unchanged80-lane roster. Production, helper, metadata, source routing
and50 earlier contracts remain unchanged. The frozen80 module is reused; only
the supported test host is compiled for direct qualification.

## Effective maximum and matched history

Existing release uses max(rank release,keyRelease,gated masterSustain*10),
followed by fixed scale/tolerance. MasterSustain is read directly, not through
the ordinary generic parameter smoother. The fixture fixes active keyRelease
at.5s, initial active masterSustain at.02 (performance term.2s), enabled sustain
gate with assign0/no pedal, static sustainMode0,neutral macros/dispersion.
PANEL rank fR.30/vR.28 and default MEMORY11 match; factory0 maximum is approximately
.09103s. Thus static rank floors remain below the key floor. Rank releases are
not dynamically queued or individually qualified by this test.

All notes use the unchanged helper history, off3072..3087. Both scenarios hold
master.02 through3100, compared with the SAME active key.5/master.02 constant
queues and identical source/mode/note histories:

- BELOW: master.04 at3580 then.03 at4095; maximum performance term.4s remains
  below key.5s, so effective release is identical for the whole capture.
- CROSS: master.08 at3580 and4095. Linear parameter interpolation gives a
  performance term.45s at3300 and.575s at3400, safely below/above key.5s.
  Exact PCM before3300 and positive effect in frames[3400,4096) are checked.

No exact floating-point crossover index is asserted. BELOW proves PCM equality,
not stored-controller state equivalence or behavior after a later floor change.
The CROSS comparison identifies a global max-floor interaction, not independent
rank contributions or complete release response.

Other explicit settings retain the KEY RELEASE sounding path: filter/VCA
A.001/D.002,filterIL.5/AL.3,VCA S.7,pulse1,saw0,sine/noise0,PW.5/PWMdepth0,
LPF4000Hz/HPF25Hz,resonances0,VCFlevel.78,VCAlevel.35,mix.5 and neutral touch/
LFO/modulation. Scalar/five partitions retain existing thresholds.

Two scenarios ×sources13/0/11 ×capacities8/16 (two/sixteen active notes)
×44.1/48/96kHz ×realtime/offline gives72 cases. Six compatibility renders,
scalar/five partitions and one active comparator per case yield936 renders
in six processes. The216 capture identities are NOT baseline byte comparisons.
No historical-suite reruns or new plugin build are implied. Six CMake entries
use `cs80_vst3_release_floor_interaction_tests`, original host contract and new
`host-automation-release-floor-interaction-v1.json`,timeout180 seconds.

This qualification remains bounded. No dynamic rank-release, internal-state, complete
release, calibrated hardware, GUI, CPU or broader integration claim is made.

## Verified local result

[Retained verification](experiments/release-floor-interaction/verification.json)
records direct supported-host compilation (exit0) and one six-process matrix:
all72 cases and936 renders passed. Maximum partition error was0 against the
unchanged2e-6 threshold. All72 conservative pre3300 prefixes were exact. All36
BELOW cases preserved whole PCM exactly; the36 CROSS tail effects ranged
from.000653298 to.013814. The216 retained capture identities are not baseline
comparisons. No failures or retries occurred.

SDK/compiler/Python and source identities were captured before compilation and
verified unchanged after runtime. Host/module identities were captured before
runtime and checked afterward. Final result prose was added after the snapshot.
All production sources, shared renderer, prior exporters and50 prior contracts
remain byte-unchanged. No plugin build, CMake execution or CTest run occurred.
Raw PCM and native binaries are retained in the ignored local workspace;
committed hashes/logs are not a complete replay bundle. This does not establish
internal-state equivalence, exact crossover sample, dynamic rank-release
qualification, full release timing or calibrated hardware behavior.
