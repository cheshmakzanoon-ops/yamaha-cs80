# Host feature qualification review

Date: 2026-11-01. Reviewed source:
`475063ca4b5640c1b17cb45eb89a23706ad18873`.
This is a source and software-protocol review, with native CI evidence. It
introduces no production change, acceptance limit, parameter, or audio pin.

## Native evidence and its scope

The independently retrieved logs are from
[build run 41251946760](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41251946760).
Its PR head is `156d82e9f3ee20089bb952ba853e0f0c779b903f`, base is
`db24a47170610f81236e8829ef5536af69dfc94d`, and the Actions-generated merge
checkout is `7190e8e1ee36d52e76c6296934c798f491052f17`.
The implementation, native host tests, MTS client, state synchronization,
programmer, and tab sources listed in [manifest.json](manifest.json) are
byte-identical between the tested PR head and the reviewed source: 37 files.

| Platform | Job | Native automation evidence | MTS real client / shim / fallback | Whole job |
| --- | --- | --- | --- | --- |
| Linux | [124687124151](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41251946760/job/124687124151) | Actual VST3 SDK host passed | All three passed | Passed |
| macOS Apple Silicon | [124687124200](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41251946760/job/124687124200) | Actual AU component host and serial mutation gate passed | All three passed | Passed |
| macOS Intel | [124687124244](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41251946760/job/124687124244) | Actual AU component host and serial mutation gate passed | All three passed | Failed: separate BBD idle-history assertion |
| Windows | [124687124206](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41251946760/job/124687124206) | Platform-neutral automation contracts and full UI suite passed; no native VST3 queue host is registered on Windows | All three passed | Passed |

All four jobs also pass the three automation curve/bridge contract suites,
`cs80_programmer_integration_tests`, `cs80_memory_panel_tests`,
`cs80_chorus_tabs_tests`, and the complete `cs80_ui_smoke` suite. The manifest
records 45 individual passing per-platform test results. This is not a new
combined gate or a replacement for any existing suite.

The full workflow is **failed**, because the Intel BBD exact-skip assertion
failed. This review does not relabel it as green, infer final release
qualification, or substitute an older checkout for current-head CI. The
separate BBD repair is recorded in the integration branch. Likewise, automated
native hosts do not replace the owner's real-DAW sessions.

Committed log excerpts retain the exact test result lines and checkout
identity. The manifest identifies the full retrieved logs by SHA-256 after
decoding as UTF-8 and normalizing CRLF to LF. It separately hashes each
committed excerpt. Full log retrieval is the GitHub Actions job-log endpoint
for each linked job; the hashes are provenance for this retrieval, not a claim
that the excerpt contains every line.

## Automation review

`HostAutomationBridge.h` lists 18 parameters by enum name: six global
controls, then HP cutoff/resonance, LP cutoff/resonance, VCA level, and pulse
width in each rank. The 18-control roster and the existing limits remain
unchanged: normalized error `1e-7`, audio absolute error `2e-6`, event-position
error zero samples. See [HOST_AUTOMATION.md](../../../HOST_AUTOMATION.md).

The VST3 wrapper borrows the actual host queues for the synchronous render.
The AU wrapper receives real scheduled events and reports capacity and
invalid-input errors. `PluginProcessorAutomation.cpp` applies effective
targets before the corresponding sample and before MIDI allocation at that
position. It preserves the existing circuit smoother. It refreshes the
implicit prior value after external UI/state edits without repeatedly
projecting an accepted double trajectory through Float32 storage.

The native tests compare real built modules against independently sampled
host controls, including mid-block changes, partition changes, source
ownership, and sixteen voices. The processor only enables this timing
extension for the modern circuit policy. Pure and Legacy retain their previous
wrapper semantics. Selected factory and memory sources retain authority over
rank controls; hidden panel automation cannot replace them. Effective changes
are delivered to both active engines in sixteen-voice mode.

The current AU mutation runner contains **seven**, not the five described in
the older prose, actual-component mutations:

1. Restore the former block adapter.
2. Move immediate offsets to sample zero.
3. Remove the Pure/Legacy policy mask.
4. Publish a future ramp endpoint early.
5. Discard the accepted normalized endpoint publication.
6. Retain publication ownership across reset.
7. Advertise ramp capability for unsupported controls.

The serial CTest restores the production files before every mutation, rebuilds
the real component, and requires the native assertion exit code `1`. Build
errors and crashes fail the runner. It finally restores and rebuilds the
unmodified component and requires native exit code `0`. Both macOS jobs pass
that complete runner. The original contract and later endpoint-ownership
publication chronology remain as documented in
[the endpoint evidence](../../continuation-2026-10-31/au-intel-endpoint/).

## MTS client review

The three vendored upstream files match the hashes in
[README.vendor.md](../../../../deps/mts-esp/README.vendor.md), at ODDSound
commit `f214739b8832e7f297cb9970d0c0efbf783f1462`. Their upstream licence is
**0BSD**, reproduced with the client; the handover's MIT description was
incorrect. The plugin compiles the actual unmodified client. Only the shim
test defines the replacement test-backend macro; the fallback is a separate
explicit `CS80_WITH_MTS_ESP=0` target.

The real-client test exercises registration/destruction, no-master behavior,
and native MTS SysEx retuning. The deterministic shim exercises all 128 pitch
and mapping queries, channel `-1`, MIDI forwarding, and failed registration.
Its rendered positive control proves the table affects modern mode 2, while
tuning mode 0 and Pure remain byte-identical with the same table supplied.
The native four-platform results close the compile-test gap. A live installed
external master and per-channel tuning tables are not claimed by these tests.

## Programmer and independent tabs

`ControlFrame::programmer()` carries explicit panel/memory positions.
`Engine::setControlFrame()` validates and compiles both rank transactions
before publication, only for circuit policies and selectors 11 through 13.
`MemoryPanels.h` therefore drives the selected electrical programmer in audio.
Legacy and the older aggregate-frame path retain their original semantics.
The full reference-network, selected waveform/audio, invalid-transaction,
unselected-source, and Legacy checks remain in
`ProgrammerIntegrationTests.cpp`; they passed on all four platforms.

`P_chorusTabs` is appended after the existing markers in `Params.h` and under
`[completion-tabs]` in `createParameterLayout()`, with AU version hint 16.
Its neutral default is the historical switch mapping. Explicit choices
represent both off, chorus, tremolo, and both on. The original physical tab
states also apply in Pure; this is original-control modeling, while the
modern automation and tuning extensions remain inert there. Legacy ignores
the new tab parameter. The parity metadata pins the exact core parameter
count at `139 + 1 + 2 + 9 + 39 + 1 = 191`. Host UI tests verify the append
position, version hint, neutral default, and discrete states.

The original simultaneous OE1/OE2 behavior remains a declared inference with
its existing independent gain oracle. This review adds no fitted hardware law.

## APVTS state ownership audit

All production `copyState()` and `replaceState()` call sites were searched
and read. The four processor operations that copy or replace live APVTS state
take `apvts.treeLock()` and call `apvts.syncNow()` first:

| Operation | Production method |
| --- | --- |
| Host save | `Cs80AudioProcessor::getStateInformation` |
| Host restore | `Cs80AudioProcessor::setStateInformation` |
| User preset save | `Cs80AudioProcessor::writeUserPreset` |
| User preset restore | `Cs80AudioProcessor::loadUserPreset` |

`UndoableParameterState::copyState()` additionally acquires the same recursive
lock, synchronizes, copies a detached tree, and overlays current parameter
atomics. `ModernLayerState::store()` holds that lock and synchronizes before
inserting or replacing its live child. Its reads and reload callbacks use the
same lock order. `normaliseRestoredState()` copies a detached input parsed
from XML, not a second unguarded live APVTS snapshot.

Undo/redo owns the common lock for the transaction, synchronizes before
replay, and `ParameterChangeAction` emits balanced host begin/end gestures
only for changed parameters. The existing deterministic synchronization,
pending external edit, worker snapshot, transaction/history, and exact MSEG
restore tests are part of the complete UI suite that passed in all four jobs.
The mechanisms, original acceptance contracts, and fourteen actual-source
mutation results remain in
[STATE_TREE_SYNCHRONIZATION.md](../../../STATE_TREE_SYNCHRONIZATION.md).

No new production gap was found in these reviewed paths. CPU targets,
event/click score qualification, the filter setup voltage discrepancy, final
current-head CI, release artifacts, and the owner's excluded actions retain
their separately tracked status.
