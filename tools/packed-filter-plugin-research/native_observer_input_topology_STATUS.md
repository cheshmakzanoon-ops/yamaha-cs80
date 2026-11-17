# Frozen disabled-mono-input topology

Historical CS80 stock diagnostic3 passed the preselected optional controller
query policy, then exited24 at `frozen synth bus topology`. No A-D receipt was
admitted. Source at commit0c351b9c509e2d3e8ae1ff978fbc0a3a739974e3 declares one
`External Modulation` mono input, disabled by default, and one stereo output.
A disabled bus is not an absent bus. No synth feature is removed or changed.

The private const instance profile defaults to `ZeroAudioInput`(1). Explicit
`DisabledMonoExternalInput`(2) requires one input bus with channelCount1,
kMain, flags0 and exact name `External Modulation`; one stereo audio output,
one event input and no event output remain required. It requests one mono input
arrangement and one stereo output, then verifies both readbacks. Even the legacy
zero-input arrangement call supplies a valid dummy input pointer, avoiding
null-pointer arithmetic inside wrappers that form begin+count.

Immediately before each of the three activation cycles, the owner makes one
successful `activateBus(kAudio,kInput,0,false)` request. It never enables the
input. This is evidence of the exact successful request, not a claim that an
unavailable SDK is-active getter observed private plugin state. Failure is
terminal; cleanup only deactivates buses actually enabled by this host. Restore
and reactivation do not bypass the explicit inactive request.

SDK ProcessData requires descriptors for every enumerated bus even when inactive.
For profile2, every audio render call supplies one input AudioBusBuffers with
numChannels1, silenceFlags0 and a stable one-element channel-pointer array whose
sample pointer is null. This carries no input audio. SDK permits inactive sample
pointers to be null, but not the channel-pointer array itself. The renderer checks
all input pointers/counts/flags after processing and rejects mutation. Profile1
retains zero input descriptors. Parameter-only zero-frame flush stays the
separate documented zero-audio flush protocol, not a rendered audio call.

The instance/worker render profile must match the immutable selected lifecycle
profile. History constructs the render protocol from this selection. Legacy
render protocol bytes remain version1; profile2 uses CS80RPRT version2 then u32
profile2 before the previous body, so profiles cannot silently compare equal.

Request-v3 appends u64 topology after query policy, before timeout; v1/v2 imply1.
The standalone driver emits child-receipt-v3, with u32 topology immediately after
the query-policy/result/pointer triple. The parent independently binds this to
request, expected contract and render protocol; profile2 additionally requires
exactly three successful `external-input-disable` calls before activation. The
canonical A-D payload remains v1, but contains the versioned render protocol and
full lifecycle ledger; the enclosing hashed v3 receipt binds topology as well.

## Source basis

JUCE revision c352e2489000ffeb9e397051377d75eef2efe641:
- Plugin wrapper getBusCount counts declared buses regardless disabled state;
  getBusInfo/getBusArrangement report last-enabled layout. SHA256
  16031325b31be2e32b23689f07338bd6c631941475b9bbf87d176d1f2565c202.
- SDK ivstaudioprocessor.h documents all-bus descriptors and inactive null sample
  pointers. Its existing pinned SDK provenance is unchanged.
- juce_VST3Common.h mapper accepts null samples only for host-inactive buses,
  skips client-inactive buses or clears scratch when client-active/host-inactive.
  SHA256 ac8ef5f9bbc1df1a0ef83d40b49c79ac7fc3e6899d0f4bb7eba1a06307c81103.

Inert controls cover strict rejection of extra input, exact metadata/arrangements,
readback, failed inactive requests, all three cycles and descriptor mutations.
No real topology-profile success, CPU saving or production qualification is
claimed by these controls. Diagnostic3 remains a failed result.
