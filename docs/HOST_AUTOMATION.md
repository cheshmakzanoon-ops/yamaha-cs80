# Host parameter timing

## Contract and sources

`tests/parity/tolerances-v2/host-automation.json` was committed before
implementation or evaluation (local commit
`6807df1`). These are software protocol gates, not physical CS-80 measurements.
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

AU immediate/ramp scheduling has separately preregistered acceptance in the
same JSON file. The AU wrapper remains open until its scheduling bridge and
native AudioUnitScheduleParameters gate are committed and pass on macOS.
