# Host parameter timing

## Contract and sources

`tests/parity/tolerances-v2/host-automation.json` was committed before
implementation or evaluation in published commit
`08e08e492b62b8078a3364d14754a6870c55ee22`. These are software protocol gates,
not physical CS-80 measurements.
The registered maximum errors are 1e-7 in normalized control value, 2e-6 in
rendered sample amplitude, and zero samples in event position.

The VST3 truth is Steinberg's
[IParamValueQueue contract](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/classSteinberg_1_1Vst_1_1IParamValueQueue.html)
and [automation playback documentation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Parameters%2BAutomation/Index.html).
A queue describes normalized linear segments. The previous block's value is
an implicit point at position -1. A jump is represented by consecutive-sample
old/new values. Keeping only the final point loses both timing and trajectory.

The circuit engine's existing control smoothing still applies after the
sample-positioned target. This change adds no fade, envelope restart, or new
circuit law.

## Supported controls and policy

The enum roster in `HostAutomationBridge.h` contains 18 controls:

| Scope | Parameters |
| --- | --- |
| Global | volume, mix, brilliance, resonance, master tune, detune |
| Rank I | HP cutoff/resonance, LP cutoff/resonance, VCA level, pulse width |
| Rank II | HP cutoff/resonance, LP cutoff/resonance, VCA level, pulse width |

Other parameters retain JUCE's block-rate path. No parameter ID, index,
version hint, parameter count, state schema, or default value changes.
Without host automation points, the audio remains byte-identical to the
original block path. Legacy and PURE CS-80 retain their original VST3
final-point-at-block-start behavior. The timing extension applies to the
modern circuit policy.

Panel trajectories do not bypass a selected factory tab or memory bank.
Their stored host values can change while the programmer remains the
source of the effective rank settings. Both circuit engines receive effective
target changes in sixteen-voice mode.

## VST3 implementation

JUCE 8.0.11, commit `c352e2489000ffeb9e397051377d75eef2efe641`, applies only the
last point of each ordinary parameter queue before `processBlock`.
`cmake/HostAutomation.cmake` and `scripts/patch-juce-automation.py` generate a
small wrapper bridge in the build tree. The original JUCE checkout is never
edited. The complete source's SHA-256, normalized only from CRLF to LF, must
match the reviewed version; other source changes stop configuration.

The wrapper borrows the host queue for the synchronous process call. It
copies no event array, imposes no queue-point limit, and allocates no queue
storage on the audio thread. The processor owns one cursor per named
parameter. Constant spans remain batched; changing normalized ramps advance
at each host sample. Effective controls reach `Engine::setControlFrame` before
that sample's audio. The usual bounded audio chunks and MIDI positions are
retained. At return, a scope guard publishes the final values through the
original JUCE notification helper and releases all borrowed queue references,
including early-return/suspended paths. Parameter-only zero-frame flushes use
the original JUCE setter path.

An actively changing ramp currently reapplies a complete control frame at
its changed host samples. This has a greater CPU cost than a constant patch;
the source-factory benchmark's constant controls do not quantify that cost.
No approximation of the host curve was introduced to reduce it.

## Qualification

- `cs80_host_automation_curve_tests` compares each sample to an independent
  absolute-knot calculation, across 1/7/64/256/1024-frame partitions. It covers
  implicit starts, slope changes, first/last samples, jumps, constant tails,
  and a 10,000-point queue without truncation.
- `cs80_vst3_automation_tests` is a small native SDK host on Linux. It loads the
  built module, obtains `IComponent` and `IAudioProcessor`, and supplies actual
  `IParameterChanges` and MIDI events. No JUCE host adapter can flatten its
  fixture queues. Independent scalar values rendered as one-sample host blocks
  match 7/64/256/1024/4096-frame partitions with **maximum sample difference 0**.
  The sixteen-voice comparison is also **0**.
- Empty/no-queue renders are byte-identical. Pure and Legacy match independent
  last-point-at-block-start fixtures byte-for-byte. Factory, memory A, and
  memory B preserve source ownership. A positive control confirms that the
  automation changes audible output.
- Curve mutations detected dropped intermediate points, flattened offsets,
  held interpolation, an incorrect implicit origin, a dropped last point,
  truncation at 1024 points, and batching across a ramp. A modified JUCE source
  was rejected by the source-hash check.
- Built-module mutations detected removal of the Pure mask, removal of the
  Legacy mask, bypass of programmer ownership, and omission of the second
  engine's automation updates. The wrapper itself is separately qualified
  by bypassing its queue bridge: the native gate fails with maximum errors
  0.0559275 (ordinary voices) and 0.112002 (sixteen voices). Restoring the bridge
  returns the CTest gate to zero failures and zero audio difference.

## AU implementation and native qualification

The AU v2 bridge uses Apple's
[AudioUnitScheduleParameters contract](https://developer.apple.com/documentation/audiotoolbox/audiounitscheduleparameters(_:_:_:)).
Immediate events take effect at their buffer offset. Ramps interpolate from
the signed start offset through the specified duration; a negative start
represents a ramp already in progress. Events apply to the current render.
Only the 18 supported controls advertise `kAudioUnitParameterFlag_CanRamp`.
JUCE's former AU adapter applied immediate events at block start and ignored
ramps; those exact semantics remain in Legacy and PURE CS-80.

A second checked build-tree patch adds the AU scheduling override. Its
preallocated queue accepts at most 2,048 events per render and reports a
capacity error before accepting a batch that would overflow. Invalid scope,
parameter, non-finite values, time overflow, and offsets outside the render
report errors. Scheduled conversion and sorting allocate no memory. The
processor consumes the same borrowed curve interface as VST3. Each lane
publishes its value at the current render's final sample; a future ramp
endpoint is not prematurely published. Offset-zero immediate setters and
UI/state changes become the initial value before later events. Overlapping
events replace the later interval, with the last scheduled event winning a
tie at the same offset.

Local qualification is complete for the platform-neutral curve and exact
AU hook body. `cs80_au_automation_curve_tests` compares the full registered
partition grid against an independent scalar event timeline, with maximum
normalized error **2.22045e-16**. `cs80_au_automation_bridge_contract_tests`
compiles the production hook body against a narrow AU/parameter contract
fixture and checks scheduling, notifications, protected policies, capacity,
and invalid-input handling. This fixture does not claim to prove Apple's ABI.
Ten mutations were detected: flattened immediate offsets, lost negative
starts, held ramps, premature future endpoint publication, missing overlap
truncation, delayed current-state updates, hidden capacity errors, removed
protected-policy behavior, dropped final notifications, and hidden scope
errors. The native VST3 host gate still passes after the shared interface
extension.

On macOS, `cs80_au_automation_host_tests` loads the exact built `.component`
factory and calls real `AudioUnitScheduleParameters`, `AudioUnitSetParameter`,
`MusicDeviceMIDIEvent`, and `AudioUnitRender` APIs. It compares independently
sampled controls with ramps and immediate events across host partitions,
checks final value readback and advertised capabilities, preserves factory
and memory ownership, and exercises both engines. The accompanying serial
`cs80_au_automation_native_mutations` CTest rebuilds and tests seven temporary
faults in that actual component, then restores and requalifies the source
and module. A mutation counts only when the built host exits with its normal
assertion-failure code; a build error or crash cannot qualify the gate.

Native qualification is now recorded on both macOS architectures. In
[build run 41251946760](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41251946760),
the native component host and the seven-mutation/restored-component gate pass
on both
[Apple Silicon](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41251946760/job/124687124200)
and
[Intel](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41251946760/job/124687124244).
The native Linux VST3 host passes in
[the same run](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41251946760/job/124687124151).
The checked source is PR head `156d82e9f3ee20089bb952ba853e0f0c779b903f`,
with Actions checkout `7190e8e1ee36d52e76c6296934c798f491052f17`.

These are feature-specific passes. That complete workflow remains failed
because the Intel job's separate BBD idle-history test failed; it is not
reported as a green release build. Final integrated CI still follows the
current branch. The automation implementation and native test sources are
unchanged between that tested head and reviewed head `475063c`.
[The dated feature review](evidence/qualification-2026-11-01/host-features/README.md)
retains exact job excerpts, full-log hashes, source hashes, and the remaining
scope limits. Native automated host coverage does not replace the owner's
real-DAW session checklist.

A subsequent independent review retrieved the actual four build-job logs for
[successful push run 41269271356](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41269271356)
at `8a85faab1a125cc14425d9184f7022f6746e40a6`. The native Linux VST3 host and
both native macOS AU hosts and seven-mutation runners pass there, along with
the four full UI suites. The [current-source review](evidence/host-ui-20261101/README.md)
retains exact result lines, log hashes and source differences from the older
feature qualification. This is qualification of that head, not a later release.
