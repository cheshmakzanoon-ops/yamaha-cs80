# Explicit inactive mono input: renderer preparation

`AudioInputTopology::ZeroAudioInput` (1) remains the default. The separately
preselected `DisabledMonoExternalInput` (2) means one enumerated mono input bus
whose activation has already been explicitly disabled by the instance owner.
It does not mean the plugin has no audio input feature. Unknown enum values fail
before invoking the native processor. This helper does not configure a plugin or
prove that the input is actually inactive; the lifecycle owner must establish
and maintain that prerequisite, and bind its topology to the same render protocol.

The pinned SDK's `ivstaudioprocessor.h` requires a ProcessData input descriptor
for every input bus regardless of activation. Each descriptor's channel count and
pointer-array size must match the arrangement. An inactive bus's sample pointers
may be null. Therefore profile2 emits `numInputs=1`, a valid `AudioBusBuffers`
with `numChannels=1`, and a stable one-element channel-pointer array containing
null. No input sample memory is supplied. The input silence flag remains0; it is
not used to excuse missing data for an active bus. Profile1 remains exactly the
prior `numInputs=0`, null-inputs descriptor.

After every callback the renderer requires the original input count, descriptor
address, channel count, pointer-array address, null sample pointer and zero input
silence flags. Mutation is terminal, like existing output/interface mutations.
Checks use owned local descriptors, not dereferences of replacement pointers
provided by the processor. Existing stereo output, event, parameter, context,
PCM-finiteness and bounded callback/queue checks are unchanged.

## Encoding

Profile1 preserves the complete original `CS80RPRT` version1 bytes. Profile2 uses
the same8-byte magic, u32LE version2, then u32LE topology2, followed by all original
version1 fields beginning with sample-rate. No pointers or ABI structs are
serialized. Thus topology is bound into the render protocol digest and the full
protocol embedded in each encoded render observation. `CS80ROBS` keeps its
existing framing version1 because that frame already embeds the complete
self-versioned protocol blob.

## Verification limits

14 new inert controls cover both valid profiles, unknown values, byte-level
protocol separation, partial-final-block audio and input descriptor mutations
(count, bus pointer, channel count, pointer array, sample pointer, silence flags).
The existing50 renderer controls and24 encoding controls also pass unchanged.
No actual CS80 plugin is loaded by these tests, and no CPU saving or production
qualification is claimed. Zero-audio parameter flush behavior is outside this
helper and remains governed by its separate protocol.
