# Linux validator host callback lifetime

2026-10-31. This repair targets the validator host, while the plugin remains on
JUCE 8.0.11. A successful retry is not evidence of removal of a crash mechanism.

## Historical failure evidence

The investigation retrieved the two pages of failed workflow runs (122 runs),
examined 88 build/comparison candidates, and fetched 41 failed Linux validator
or comparison job logs. Two independently captured fault traces locate the
same host callback and relative instruction:

| Run / job | Fault instruction | Caller |
| --- | --- | --- |
| [40967809044 / 123779240630](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40967809044/job/123779240630) | host ELF offset `0x394838` | `juce::RunLoop::Impl::registerEventHandler` callback, +`0x2f` |
| [41023756891 / 123960532608](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41023756891/job/123960532608) | host ELF offset `0x394838` | the same callback, +`0x2f` |

The official v1.0.4 Linux binary SHA-256 is
`d3d342ea92293b0699a5c983937211e3faf2b0f38145b31cb8d78424a25e10ba`.
Its callback symbol is at `0x3949a0`; the logged call-site address is
`0x3949cf`. The fault is 0x197 bytes below that call site. Disassembly locates
it inside the host unordered-map lookup invoked by the callback. The next
frames are LinuxEventLoop dispatch and MessageManager. These are addresses
inside pluginval, not an unmapped plugin instruction.

## Mechanism and upstream removal

The affected validator embeds JUCE 8.0.3, submodule
`5179f4e720d8406ebd1b5401c86aea8da6cc83c9`. The Linux event loop copies its
ready callbacks before invoking them. An earlier callback can destroy the
last VST3 RunLoop owner, unregistering another descriptor, while that other
callback remains in the already-copied dispatch batch. Its raw owner capture
then accesses a destroyed handler map.

[JUCE commit 04e167d64661d3298afd4978f6c2e77f841b5f17](https://github.com/juce-framework/JUCE/commit/04e167d64661d3298afd4978f6c2e77f841b5f17)
repairs this exact shutdown sequence. The callback holds a weak reference to
the map, acquires a live shared reference before access, and checks whether
the descriptor remains registered. It does not access the former RunLoop
owner. Both the missing-owner and removed-descriptor cases return without
calling an invalid handler. This ownership change removes the identified
mechanism; it is independent of whether one randomized validation run passes.

The Linux CI validator is pinned to the official
[pluginval v2.0.0-beta3](https://github.com/Tracktion/pluginval/releases/tag/v2.0.0-beta3),
source `37d6a37bc752606c524f7f3febffc73e0945a740`, embedding JUCE 9.0.3
(`be29c81492b6151c8ea8d14c840e1311963b3a83`) with that fix. The downloaded
archive must match SHA-256
`2e23d311c8c2664457b11be8f6cf4ed29cfdc08765c71c43934a1ed3360f118f`.
The extracted binary checked locally is
`79612f9dd1a93f24920a7244a46c50bc7ff1bdf8174c013acee19032f5047f24`.
The plugin's JUCE dependency and the Windows/macOS validators are unchanged.

## Validation policy

Strictness remains 10, with headless, GUI, and GUI seed `0x2da72db` runs.
Every invocation must exit zero and print SUCCESS without FAILED. The Linux
job now permits one attempt; any signal, even after test completion, fails.
The previous retry-wrapper fixtures remain and an additional no-retry fixture
rejects a wrapper that ignores the one-attempt policy. No validator test is
disabled. The new host enables additional tests; any newly exposed failure
must be repaired in the plugin.

Beta3's CLI always constructs an in-process ValidationPass in
`Source/CommandLine.cpp`. Its removed `--validate-in-process` flag is therefore
omitted for Linux; this preserves the execution mode rather than weakening it.
The old platforms retain their original flag.

The acceptance contract is `tests/validator/pinned-linux.json`. All 14 wrapper
fixtures pass, including the one-attempt case. A mutation hardcoding three
attempts fails that new case, with two attempts instead of one. Actual plugin
validation and final CI are recorded separately after a consistent full build.
This record
establishes source/binary provenance and the matching historical call chain.
It does not claim a new sanitizer reproduction. The plugin's UI timers and
static graphics resources are audited separately; defensive resource cleanup
is not presented as the cause of these host traces. Final CI results belong
in STATUS.md, with earlier failing runs retained as historical evidence.
