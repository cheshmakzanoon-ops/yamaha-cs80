# Candidate VST3 VCF LEVEL automation

This candidate appends the existing rank0/rank1 VCF LEVEL controls at lanes54/55,
for 56 lanes. It changes only the production automation roster, not parameter
IDs, order, range0..1, default.78, conversion, DSP, smoothing or source authority.
All 38 earlier contracts and the shared renderer remain unchanged. The historical
PWM fixture accepts a larger runtime roster while still checking its exact
54-lane contract and every original prefix position.

## Source scope

`src/core/Engine.cpp` includes `vcfLevel` in rank smoothing (line37), reads the
PANEL value (805), and replaces it for selected source tones (833). Both scalar
and batched paths form `filtered * vcfLevel + sine` (3042/3128). Thus the amount
is live, not note-latched, and is not a gain on the separate sine contribution.
`PluginProcessorAutomation.cpp` retains its existing rank mask: only PANEL13
accepts these panel queues; factory0 and MEMORY11 are masked. No routing bypass
is introduced.

## Bounded candidate qualification

The separate `Vst3VcfLevelAutomationTests.cpp` host checks each lane in isolation
for sources13/0/11 and voice capacities8/16 (two/sixteen active notes), at
44.1/48/96kHz in realtime/offline modes. It uses unchanged default MIDI48 onward.
PANEL settings explicitly select pulse1, saw0, sine0, noise0, pulse width.5,
PWM depth0, VCA level.35 and mix.5; other touch/LFO/modulation amounts are zero
and macros neutral. This disables the direct sine contribution while providing
filtered-path excitation. It does not assert a mathematically pure filter output.

The selected gain starts at.5, holds through samples0/128, then takes targets
.2/.8/.3/.7/.15/.6 at511/1023/1791/2559/3583/4095 via existing normalization.
Against a matched active constant queue, the prefix before128 must be exact;
PANEL cases require a positive later PCM effect, while factory/MEMORY cases
require exact selected-source PCM masking. Scalar and five block partitions
retain the existing error threshold. No sine-only inertness control is claimed:
disabling oscillator AC does not prove zero filter offsets or coupling transients.

Planned budget:72 full cases (24 PANEL positive,48 masked),72 frozen54
compatibility cases,1,368 renders in12 processes. The216 actual candidate/frozen
byte pairs cover no-automation, Legacy and PURE captures. These are separate
from the within-candidate active-queue comparisons. The six CMake registrations
use `cs80_vst3_vcf_level_automation_tests`, the original host contract and the
new `host-automation-vcf-level-v1.json` contract, with a180-second timeout.

No runtime outcome is claimed here before qualification. This finite startup
and held-note witness is not calibrated gain response, hardware validation,
whole-voice parity, simultaneous two-lane attribution, GUI or CPU qualification.

## Verified local result

The scoped official VST3 and supported-host build completed successfully. All 12 direct processes passed: 72 full cases and 72 frozen-54 compatibility cases, totaling 1,368 renders. All 216 actual no-queue/Legacy/PURE PCM capture pairs matched the frozen module exactly. These were direct fixture runs, not CTest or a historical full-suite execution.

All 72 pre-change prefixes were exact. The maximum measured partition error was zero (limit 2e-6). PANEL effect ranges were 0.00380196–0.0174046 for rank 0 and 0.0044445–0.0232565 for rank 1. All 48 selected factory/MEMORY cases remained exactly masked. This verifies isolated live filtered-path level automation in the bounded fixture, not calibrated gain, sine-only purity, simultaneous-rank behavior or hardware equivalence.

[Verification and retained evidence](experiments/vcf-level-automation/verification.json) records all outcomes and identities. SDK/compiler/Python/source hashes were captured before compilation and checked after runtime; final host/module identities were captured before and after runtime. All recorded execution inputs were unchanged. All 38 prior contracts, prior exporters, shared renderer and production sources other than the capability roster header matched the base. Final result prose was added after these checks. Raw PCM and native binaries remain in the ignored local workspace; the committed evidence contains their hashes and full logs, not a complete replay bundle. There were no failed attempts or retries in this qualification.
