# Bounded render adapters: preparation only

This additive header has no module loader, default plugin, CLI, workflow, or CPU
clock. All runtime evidence uses an in-process fake IAudioProcessor. No synth
processing or production qualification has occurred.

## Frozen minimal protocol

- Caller supplies an already initialized, setup, active, processing instance with
  matching rate/mode/max block and frozen initial state. The adapter does not
  establish these prerequisites or reset state.
- 44.1/48/96 kHz; realtime/offline SDK mode; float32; zero audio inputs, one stereo
  output, one event input, **no output events**. No transport, tempo or musical
  clock claim: context state is zero, only sampleRate/projectTimeSamples valid.
- 1..1048576 total frames, block 1..4096. Last partial block is exact. Frame zero
  is first sample; inputs require nondecreasing integer absolute frame timestamps
  in [0,total frames). Relative offset is absolute minus current block start.
- Up to 65536 events, notes on/off only by default. Input SDK sampleOffset and PPQ
  must initially be zero; pointer-bearing/SysEx/unknown events are rejected.
  Explicit `legacyCCFixtureConvention=true` also enables CC number 0..127,
  channel 0..15, 7-bit values. The SDK defines this legacy event as **output-only**;
  its input use reproduces a research-fixture convention, not generic VST3 MIDI
  correctness. A real driver must decide/freeze IMidiMapping or this convention.
- Up to 4096 input parameter points, finite normalized [0,1] values and non-sentinel
  IDs. The caller must additionally validate IDs against its complete metadata
  roster. Events retain original stream order including ties. Parameter queues
  retain first-occurrence ID order and each ID's original point order including
  ties. SDK grouping does not retain a cross-ID interleaving order; no such claim
  is made. No separate merged ordering between events and parameter points exists.
- Output parameter data is bounded to 4096 queues/points per callback and 65536
  total points. Invalid output attempts latch failure even if the plugin ignores
  rejection. Captured point order is queue then point; absolute frames restored.
- Captures every raw interleaved PCM sample, callback start/count/result and
  stereo silence flags. Finite PCM is required. Silence flags are captured, not
  used to normalize or erase raw samples; comparison must compare both. This is
  not yet a stable on-disk serialization or a complete lifecycle receipt.

## Allocation and failure limits

Input preparation, vectors, queues and zeroed PCM buffers allocate per block.
Output queue callbacks may allocate (bounded counts); retained observations grow
up to the frame/point limits. This is a correctness adapter, **not realtime safe
or performance/timing evidence**. Caller input storage, allocator overhead and
process resource limits are separate concerns; there is no claimed exact RSS
ceiling. The synchronous borrowed interfaces cannot be retained by the processor;
constant addRef/release are not an asynchronous ownership implementation.

Nonzero process return, process exception, descriptor/context mutation, input
mutation, invalid output or nonfinite PCM stops immediately. Exceptions discard
this function's local partial observation. The orchestrator must externally retain
same-attempt failure/exit evidence and perform checked cleanup; this helper alone
is not terminal ledger evidence, a timeout boundary or crash isolation. It cannot
protect against arbitrary native memory corruption or a hanging callback.

## Evidence and remaining gates

`CS80_OBSERVER_SDK=/tmp/cs80-observer-sdk python -m unittest test_native_observer_render`
compiles with C++20/-Wall/-Wextra/-Werror/-pedantic and executes inert controls.
Absent explicit SDK means skipped evidence, not success. The four additional
SDK headers were obtained from the official pinned JUCE commit and SHA256-matched
against the authentic preflight dependency manifest; existing SDK hashes are
also checked. No headers are fetched by tests.

Still missing: real initializer/host integration, validated initial/reset/restore
history, transport/MIDI policy, complete workload grid and case/capture roster,
fieldwise artifact serialization, immutable executable/runtime provenance,
original tests/validators, checked teardown, subprocess timeout/failure ledger,
actual production archive/map/relink binding, and full plugin qualification.
No production integration or Stage B timing is justified by these controls.
