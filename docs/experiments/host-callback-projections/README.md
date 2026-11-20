# Host callback observations without unused work

Host-only candidate against `5499056a60ee5044275ba17eb7dddded4c96e90d`.
At callback end, the audio thread now reads the eight physical `active` bits
directly, without constructing full per-voice inspection records. Release tails
remain active until the real voice retires. The extension is still read only
when active, and the single atomic publication of both masks and capacity is
unchanged. This is a fresh observation, not a cached activity mask; no audio
path maintains extra state.

During normalized automation readback, an empty attachment count means there
are no borrowed curves to inspect. Every owned parameter is still read,
including for zero attachments inside an open VST3 block, in its original
order using the same JUCE getter/FP operations. The nonempty case keeps its
live per-lane test. The processor does not cache normalized values in place
of reading them, defer UI/host edits, or move any borrowed callback.
No synth math, clock, oversampling setting, or latency changed.

## Isolated results

Linux x86-64 Release/PIC, GCC 14.2.0, JUCE 8.0.11
(`c352e2489000ffeb9e397051377d75eef2efe641`). Baseline ELF SHA-256
`0749b78b218186ee220ae53f8b024c1f1ef17911d676c51bbad276c2ec65dbea`;
host-only ELF SHA-256
`d41b759091fc125972cee51c5247312ef0c12537311f58dec56f386a7effca6e`.
Use the [archived native/guest harness](../cumulative-cpu-2026-11-19/resumed-exact-hotpaths/harness/REPRODUCE.md)
on two real modules; the entered process edge, repeated window and prefix-plus-next
controls are checked by that harness.

| True host block | Fresh quiet | Eight varied held notes |
|---|---:|---:|
| [16](16.json) | −0.256% | −0.057% |
| [32](32.json) | −0.137% | −0.029% |
| [64](64.json) | −0.071% | −0.015% |

These are relative changes in Callgrind translated guest **instructions**, not
wall time or DAW CPU. Duplicate/additive controls passed and reads/writes
decreased in all eight measured cells. At 64 frames, the 256-frame window
saved 2,868 instructions for fresh quiet and 2,996 for the three held/wake
scenes. Component observations cannot be summed to estimate a combined build,
and are not a physical 2%/4% CPU result.

[Native comparison](native.json): 27 histories, 82,944 callbacks, 3,286,656
stereo frames and 216 public state snapshots per arm; identical PCM,
FP/return/latency traces and serialized public state. The [connected MIDI
comparison](midi.json) and [196 verified program-selector pairs](selector.json)
also matched. Focused CTest passed: `cs80_voice_release_tests` (6,206 checks,
including all 256 physical release-tail masks in Legacy and Circuit) and
`cs80_host_automation_lookup_tests` (6,307, including all 120 readbacks
outside a host block, after an empty VST3 begin, and with sparse attached
lanes). No native CPU timing, macOS, or Windows result is claimed.
