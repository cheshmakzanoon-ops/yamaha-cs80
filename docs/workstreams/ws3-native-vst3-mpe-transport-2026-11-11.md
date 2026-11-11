# Native VST3 MPE transport qualification, 2026-11-11

This is additive qualification of the built Linux VST3 module, not a new MPE
feature or a direct-processor test presented as VST3 evidence. The existing
native host already exercised `IMidiMapping` for learned CC20/21/22. This work
extends that coverage to channel-specific MPE expression and configuration.
No production DSP, wrapper, parameter, frozen fixture, or tolerance changes are
included.

## Transport and controls

`tests/Vst3MpeTransportTests.h` is included by the existing
`cs80_vst3_automation_tests` executable. Its path is:

1. Load the built module and instantiate its actual component/controller.
2. Connect their native `IConnectionPoint` interfaces.
3. Ask `IMidiMapping` for channel/controller assignments.
4. Supply controller points through `IParameterChanges` and notes through
   `IEventList` to the component's `IAudioProcessor::process`.
5. Compare the resulting stereo samples, without Engine introspection or APVTS
   MIDI/expression injection.

The module uses pinned JUCE 8.0.11 with the repository's normal wrapper patches.
Its controller conversion truncates normalized values multiplied by 128 for
CC/channel pressure, or 16384 for pitch bend. Exact integer/128 and
integer/16384 inputs therefore carry the intended MIDI values without a 127 or
16383 normalization ambiguity.

All setup and expression ordering controls use **distinct sample offsets**.
RPN selectors precede data entry, MCM precedes notes, and comparisons retain the
same absolute note timeline and allocation. Reversing parameter-queue
enumeration and using 7-, 256-, and 1536-frame partitions must preserve the
combined expression render exactly. The test does not infer arbitrary MIDI
wire order from VST3's separate controller and note lists.

## Bounded matrix

Each rate/process-mode run includes:

- 112 distinct mapping assignments: 16 channels times bend, channel pressure,
  CC74, CC101, CC100, CC6, and CC38. IDs must be exposed, disjoint, and separate
  from core and hardware-source parameters. All controller parameter metadata
  is also checked for duplicate IDs.
- 48 routing cases across Legacy/Circuit, eight/sixteen voices, lower/upper
  zones, member/manager, and the three expression kinds. Addressed notes must
  change audibly, earlier samples must remain equal, sibling-member routing
  must follow the sender's role, and the opposite zone/gap must remain equal.
- 24 pre-onset cases. Member expression delivered at sample 64 or 127 must
  produce the same future note at sample 128, and differ from absent expression.
- 12 PURE/MPE-off boundary cases. PURE masks MPE and excludes bend/CC74; it is
  not incorrectly expected to suppress original pressure. MPE-off ignores
  MCM, retains conventional global bend, and retains channel-scoped pressure
  and CC74 for non-manager channels.
- 16 range cases. MCM's 2/48 defaults must match independent explicit RPN0
  settings and override deliberately different fallback knobs. RPN0 plus CC38
  produces 12.5/24.5-semitone sensitivities. Exact dyadic wheel fractions compare
  them with independently equivalent bends under 2/48 sensitivity:
  `12.5 * (1024/8192) == 2 * (6400/8192)` and
  `24.5 * (3072/8192) == 48 * (1568/8192)`.

The sixteen-voice fixtures keep eight earlier notes held, then add an audible
ninth witness note. Unrelated-channel comparisons preserve this allocation and
all earlier onsets. A separate absent-witness positive control prevents an
inaudible or unallocated ninth note from making the scope checks vacuous.

Positive audio controls require a maximum sample difference greater than
`1e-6`, consistent with the existing native MTS transport controls. Isolation,
range equivalence, and ordering checks require exactly equal sample vectors;
no existing tolerance is relaxed.

## Results

The fresh Release build passed all six existing native VST3 configurations,
including their pre-existing automation, MTS SysEx, and Scala-session checks.
The new MPE checks reported zero failures, zero isolation/range-equivalence
error, and zero queue/partition-ordering error in every configuration.

| Rate | Process mode | Result | Smallest positive audio delta |
| --- | --- | --- | --- |
| 44,100 Hz | Realtime | Pass | 0.00531581 |
| 44,100 Hz | Offline | Pass | 0.00533535 |
| 48,000 Hz | Realtime | Pass | 0.00433407 |
| 48,000 Hz | Offline | Pass | 0.00428115 |
| 96,000 Hz | Realtime | Pass | 0.00121413 |
| 96,000 Hz | Offline | Pass | 0.00120939 |

The complete six-configuration run took 467.03 seconds of test wall time. This
is not a CPU benchmark. No functional runtime defect was reproduced, so no
runtime change is justified or included. Independent source review found no
blocker. Independent sequential re-execution passed the same six configurations
in 487.22 seconds, including all 600 grouped MPE cases, with matching positive
minima and zero isolation/range/order errors. Both native binaries and all three
test source files were hash-verified unchanged before and after that run.

Qualification source base: `a6cf7509218df3097b83a3d4aec31bda2a3243a8`.
Production sources, build registrations, and frozen tolerances are unchanged
from that base. SHA-256 evidence for the tested native artifacts and sources:

| Artifact/source | SHA-256 |
| --- | --- |
| Linux VST3 `CS-80 Reference.so` | `b4a08bbe44cffc40f16a8f923f4b64bc0d681308e1bfdea1c09ef5cdbec5d83e` |
| `cs80_vst3_automation_tests` | `b3cf47f23aff7f535372a3482a4508a03b020ab52cb8976acd0a5c25e1122d74` |
| `tests/Vst3MpeTransportTests.h` | `954d592a0a4a8a2c3ddb0b8a88c24b710044765b2ca70ffe5f97287a42a35cdd` |
| `tests/Vst3AutomationTests.cpp` | `edea276545d9c58d36d70dfdc91db30a03319986ca6a250a3ee5d55619b93e61` |
| `tests/Vst3AutomationPolicyBoundaryTests.h` | `bb254d26e00231ecc4c9c38f6af6542a4b727c87838223bc88278033a4272b59` |

Run the registered native matrix after building `cs80_vst3_automation_tests`:

```sh
ctest --test-dir <plugin-build> -j1 -V \
  -R '^cs80_vst3_automation(_[0-9]+_(realtime|offline))?_tests$'
```

## Limits

This qualifies the bounded synthetic native protocol traces above. It does not
claim a commercial DAW session, GUI interaction, physical MPE controller,
full MPE conformance, arbitrary equal-offset ordering, zero-frame MIDI
transport, or CPU/performance validation. Each scope trace has one addressed
note, plus eight earlier gap-channel fillers in sixteen-voice mode; there is
no internal voice-allocation introspection. Expression witnesses use positive
bend, pressure, and CC74 values, with neutral resets in the combined trace.
Note-off/released voices, sustain, negative bend, arbitrary zone resizing, and
malformed RPN sequences are outside this qualification. Previous failed or unqualified
offline experiments remain historical evidence; this additive test does not
reclassify them.
