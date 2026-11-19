# Construct queued keyboard events only as they are captured
The native render callback used to default-initialize a local array of
256 UI keyboard events before checking whether there were any. A
callback-local snapshot now starts a slot's event lifetime only when
the audio thread accepts that item. The queue still reads and publishes
the same atomic boundaries and filters the same epoch, and only the
snapshot's live prefix can be accessed. The UI snapshot never borrows
ring slots that producers can overwrite. No allocation is introduced.
Host MIDI and parameter processing are unchanged.

The real stock Linux Release emitted a 14,336-byte memset followed by
channel and token default stores for each of the 256 events. Neither is
needed for the empty capture. Callgrind expands the guest libc's repeated
byte stores; the guest instruction delta in the table is not an
equivalent hardware instruction or CPU-time number.

Tests cover empty and full captured queues, pressure tokens, release
order, 32-bit timestamp wrap and regression, old epochs, overflow recovery,
and retaining the snapshot while producers reuse the ring. The real
processor was also driven with UI events at 1, 8, 128 and 256 events per
callback, mixed notes and pressure, zero-frame flush, and overflow. Audio,
public state, voice observables, host events and FP traces matched stock.

## Whole-plugin work and evidence

[Build and source binding](integration-pin.json) pins actual main
`3f78365fcc45f27c71131fb781fb699731a12dd5` and the exact keyboard-only
candidate built on it. With the UI queue empty, nine real native VST3
workloads (fresh silence, held notes, and one to 120 host automation ramps)
had bit-identical audio, state, input order and recorded FP observations
under both native execution and the guest counter. Same-build and extra
callback controls matched. Each four-callback window used 62,572 fewer
guest instructions, 16 fewer reads and 59,392 fewer writes. Instruction
changes were −1.13% fresh, −0.27% held, and −0.22% for one ramp to −0.11%
for 120 ramps. Host automation ramps are distinct from the UI queue.

63 VST3 host runs and 28 true-UI native captures exercised the accepted
prefix, including the Source Performance path. Another 648 native
PWM/ring streams matched at 44.1, 48 and 96 kHz with realtime and offline
rendering. The original standalone C++17 queue test is unchanged; the new
C++20 CTest checks snapshot retention, timestamps, pressure and epochs.
The four-worker queue test and memory/undefined-behavior sanitizers
passed separately.

The [chain comparison](chain.json) runs in the same guest-counted host
from automation-only main `0658d1b3` to the combined oscillator, smoother,
filter and keyboard image. It is independent of the immediate
keyboard-only contrast. The entire stack used 4.23% fewer instructions in
fresh idle and 5.17% fewer on the tested held case. Neither comparison
establishes native CPU use, cache misses, worst-case deadlines, or DAW load.
