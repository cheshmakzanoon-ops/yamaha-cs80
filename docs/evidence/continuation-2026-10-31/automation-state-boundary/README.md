# Host automation state at a render boundary

Acceptance was published before the new witnesses were evaluated in
[`ce8b127f7ee9466ca6f2627131ff6311bd031b47`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/ce8b127f7ee9466ca6f2627131ff6311bd031b47):
`tests/parity/tolerances-v2/host-automation-state-boundary.json`.
The existing normalized/audio/timing limits remain 1e-7, 2e-6 and zero
samples. An unchanged double host endpoint must additionally remain exact.
This is a software protocol/state-ownership investigation, not a hardware
measurement or a new circuit law.

## Witnesses

`tests/HostAutomationStateBoundaryTests.h` runs the actual processor and its
APVTS. Across every named key lane, block sizes 7/64/256 and first points at
sample 1 or the last sample, it checks:

- a UI parameter setter supersedes the preceding cached automation value;
- the existing locked, synchronised host-state restore does the same;
- an untouched double endpoint survives a following automation block;
- that endpoint also survives an intervening block with no automation queue;
- every sample matches an independent scalar interpolation from sample -1.

`tests/Vst3AutomationTests.cpp` adds native zero-frame flush witnesses after
the plugin is active. These send actual `IParameterChanges` into
`IAudioProcessor::process`, then a queue with a delayed first point. Each
render is compared to independently sampled one-frame host controls, both
after an ordinary block and after an earlier completed curve. The previous
native partition, source ownership, sixteen-voice and Pure/Legacy gates are
retained unchanged.

The flush oracle connects the actual native `IEditController`, transfers
`IComponent::getState` through `setComponentState`, and reads
`getParamNormalized`. The pinned JUCE controller synchronises from each
processor parameter's current `getValue()`. State capture reaches the
processor's locked, synchronised save API. This avoids both assuming the
submitted normalized number survives Float32 conversion exactly and using
the APVTS snapshot's additional physical projection as though it were the
parameter getter.

## Qualification status

The baseline processor witness failed on the original automation methods:

| Observation | Before correction | After correction |
| --- | ---: | ---: |
| Maximum normalized curve error | 0.45365307582292042 | 2.2204460492503131e-16 |
| Stale external-state start assertions | 216 | 0 |
| Incorrect delayed trajectories | 12 | 0 |
| Exact endpoint losses after empty blocks | 108 | 0 |
| Total failing assertions | 336 | 0 |

The complete [baseline assertion log](processor-before.log) and
[compiler, source, header and frozen dependency provenance](processor-before.json)
are retained. All 26 project translation units were rebuilt from the isolated
current sources; only hash-verified, unchanged JUCE/font objects were reused.
An absent `xprop` utility and a sandbox-blocked X display were resolved before
the assertion run; they are not counted as regression failures.

The source mechanism matches the witness. `attachHostAutomationQueue`
captured an earlier `automationCurrent` before `readParameters`, while the
latter deliberately skipped refreshing an active curve. A zero-frame flush,
UI setter or state restore could therefore update the real parameter without
updating the next curve's implicit start. Separately, an empty block replaced
the double endpoint with a float normalization even when the parameter had
not changed.

The correction compares the actual parameter readback with the
readback associated with the preceding publication before attaching a new
curve. An external change becomes authoritative; an unchanged readback keeps
the existing double endpoint, including across empty blocks. No new smoothing,
fade, parameter range or circuit law is introduced.

The [repaired processor log](processor-after.log) and
[build provenance](processor-after.json) record the passing result. The
unchanged endpoint checks use the registered exact zero limit; the other
normalized trajectories retain 1e-7.

All three registered production mutations were compiled in isolated
translation units and detected by their intended assertions:

| Mutation | Required detection |
| --- | --- |
| Suppress readback-change refresh | Latest external parameter state must own the new start |
| Unconditionally use float readback | Unchanged double endpoints must survive exactly |
| Move refresh after queue attachment | Every delayed-curve sample must follow the latest state |

The fixed and restored controls both pass. The
[mutation report](processor-mutations.json) retains commands, source hashes,
assertions and archived logs. No mutation switch enters production code.

## Native VST3 qualification

The real built VST3 module reproduces the same defect and passes after the
two-file production correction. The module source is the frozen
`5a3027ddfa5462bb9c9e8c50d77f747f69cc7e27` baseline; the repaired module adds
only the readback tracking and refresh changes described above.

| Native observation | Before correction | After correction |
| --- | ---: | ---: |
| Delayed-curve failures after active zero-frame flush | 12 / 12 | 0 / 12 |
| Maximum audio error against per-sample native host | 0.000974897 | 0 |
| Reader observation audio error | 0, byte-identical | 0, byte-identical |
| Original partition error | 0 | 0 |
| Original sixteen-voice error | 0 | 0 |

The [baseline CTest log](native-before-ctest.log),
[repaired CTest log](native-after-ctest.log),
[repaired complete output](native-after-ctest-detail.log) and independent
[frozen-module rerun](native-after.log) retain the results. The original
no-queue, Pure/Legacy, selected factory/memory and positive-control assertions
also pass unchanged.

The observation mutation was registered separately before execution in
[`23270c713a9d1ca615fad875d65c50a339f09d99`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/23270c713a9d1ca615fad875d65c50a339f09d99).
It changes each parameter through the connected native controller immediately
after reading it. On the repaired module, all 12 exact reader-neutrality
assertions detect the write (maximum audio difference 0.000782946), and all 12
delayed-curve comparisons also fail. The fixed and restored controls pass.
The same mutation is masked on the original baseline because its stale cached
curve start ignores intervening writes; that unsuccessful baseline experiment
is retained, not counted as mutation detection.

The [native mutation report](native-reader-mutations.json) records the full
compile/link commands, SDK object hashes, before/after module hashes, source
hashes, all six native runs, and archive hashes. The isolated
[mutation patch](native-reader-mutates-parameter.patch) changes the witness
only; neither module contains a mutation switch.

The separate native Intel AU endpoint investigation remains open here. No AU
endpoint tolerance or oracle was changed by this correction.
