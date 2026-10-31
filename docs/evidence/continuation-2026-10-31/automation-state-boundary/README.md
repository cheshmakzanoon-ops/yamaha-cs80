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

The flush oracle reads the actual stored physical Float32 values through
`IComponent::getState`, which reaches the processor's locked, synchronised
state-save API. It independently normalizes those values using the declared
parameter ranges. The submitted normalized number is not assumed to survive
JUCE's float range conversion without rounding.

## Qualification status

The witnesses are written. Baseline execution, correction and registered
mutation results are pending; source inspection alone is not a passing gate.
