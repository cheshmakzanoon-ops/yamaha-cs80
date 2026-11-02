# Independent host and UI completion review

2026-11-01. Reviewed feature-branch source
`8a85faab1a125cc14425d9184f7022f6746e40a6`, whose latest production audio
change is `481c7e9fdb20372288864eecf746bf7efb28190a`. This review adds evidence
and corrects two documentary commit references. It changes no production
code, parameter, default, tolerance, or audio gate. It does not qualify a later
VCA change or the eventual release head.

## Requested features and implementation

| Handover item | Independently checked implementation | Published implementation |
| --- | --- | --- |
| Sample-accurate key controls | Eighteen enum-named lanes cover global volume, mix, brilliance, resonance, master tune and detune, plus both ranks' HP/LP cutoff and resonance, VCA level and pulse width. VST3 queues and AU scheduled events reach effective engine controls before the corresponding sample and MIDI allocation. Both engines receive changed frames. Selected factory/memory banks retain control ownership. | `3a37594`, `6474265`, `6300163`; [host contract](../../../HOST_AUTOMATION.md) |
| Vendored MTS-ESP client | The shipping shim includes the pinned, unmodified ODDSound client. Its actual licence is **0BSD**. Only the test executable substitutes the deterministic C API backend; an explicit no-client target compiles the fallback. | `fa2016c2`; [vendor manifest](../../../../deps/mts-esp/README.vendor.md) |
| Electrical memory/PANEL loading | `ControlFrame::programmer()` carries both selectors, and `Engine::setControlFrame()` compiles the selected electrical receiver transactions before publishing both ranks. Invalid transactions, unselected sources and Legacy boundaries retain independent tests. | `d66d298`; [integration evidence](../../continuation-2026-10-31/PROGRAMMER_INTEGRATION.md) |
| Independent tremolo/chorus states | `P_chorusTabs` is appended under `[completion-tabs]` in both the enum/definition list and host layout, with AU version hint **16**. The parity test pins **191 core parameters**, including the one new parameter. Historical is the neutral default; the other choices represent both off, chorus, tremolo and both on. | `6f1c4aa`; `ChorusTabsTests.cpp` and the host UI tab tests |
| Context menus and MIDI learn | `ParameterContext` binds real host IDs on main/Studio switches, host-parameter combo boxes, source selectors and memory-lid controls. Memory miniature display formatting does not redirect the edit to the live PANEL parameter. Context interception precedes the widget's normal mouse handler. | `2c0866e`; `ParameterContextTests.h` |
| Undo host gestures and snapshots | Changed parameter replay owns a begin/value/end gesture. All four live APVTS copy/replace call sites take `treeLock()` and call `syncNow()` first; detached snapshots and modern-child writes use the same lock. | `0d9f522`, `bbc57fe`; [state ownership](../../../STATE_TREE_SYNCHRONIZATION.md) |
| Voice monitor | The audio callback publishes actual `getVoiceInfo()` and `getRankInfo()` taps, including both ranks and all active engine slots. The coherent atomic observer does not feed audio. | `3c08e8e`; `VoiceMonitorHostTests.h` |
| Sixteen-voice metering and pressure | Slots 8–15 contribute activity/counts; level metering reads the final combined host output. Pressure snapshots both engines and uses assignment ownership to reject stale gestures and host-owned aliases. | `3c08e8e`; `VoiceMonitorHostTests.h` and the retained pressure audit |

Modern timing, MIDI learn, tuning and voice-count extensions remain inert in
PURE. PURE uses eight slots. The additional chorus/tremolo states represent
original physical controls, so they also operate in PURE; Legacy ignores the
new tab parameter. No new host index was inserted into the previous layout.

The monitor's displayed histories are sampled control taps, not measured audio
waveforms or a measured transfer response. Nonparameter session configuration
such as matrix routing is outside the host-parameter context menu roster.

## Fresh local focused checks

The integration checkout's already built Release core executables were run
directly against the unchanged registered contracts. This avoided a second
full build and avoided modifying the integration CTest results. Exact commands,
executable hashes, source identities, outputs and exit codes are in
[verification.json](verification.json) and the nine adjacent logs.

| Existing test | Fresh result |
| --- | --- |
| MTS native client | Exit 0 |
| MTS deterministic shim | Exit 0 |
| MTS explicit fallback | Exit 0 |
| VST3-style host automation curve | Exit 0; maximum normalized error `2.22045e-16` |
| AU automation curve | Exit 0; maximum normalized error `2.22045e-16` |
| AU production-hook contract | Exit 0 |
| Independent chorus/tremolo tabs | **144,037 checks, 0 failures**; independent gain-oracle error `4.79992412e-08`; both-state positive difference `0.0725257993` |
| Programmer integration | **840 checks, 0 failures**; maximum bus error `0.00018398584085 V` |
| Memory/PANEL switch/loading reference | Exit 0; maximum bus error `0.000283 V`; Newton residual `6.54e-15 A` |

These are software protocol and declared circuit-model checks. They are not
hardware measurements. No new gate or acceptance threshold was introduced.

## Actual native CI logs retrieved in this review

[Build run 41269271356](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41269271356)
is a completed successful **push** run, attempt 1, at exactly the reviewed
`8a85faa` head. Its actual four build-job logs were retrieved anew. They
contain **45 passing feature test results**:

| Platform | Job | Reviewed results |
| --- | --- | --- |
| Linux | [124738485243](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41269271356/job/124738485243) | 11: native VST3 queue host, three curve/bridge contracts, three MTS targets, programmer, memory, tabs and full UI smoke |
| macOS Apple Silicon | [124738485050](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41269271356/job/124738485050) | 12: native AU component host and seven-mutation/restored-component runner, the same three contracts, three MTS targets, programmer, memory, tabs and UI smoke |
| macOS Intel | [124738485262](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41269271356/job/124738485262) | The same 12 results, including native AU and its mutation runner |
| Windows | [124738485203](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41269271356/job/124738485203) | 10: three curve/bridge contracts, three MTS targets, programmer, memory, tabs and UI smoke; a native VST3 queue host is not registered on Windows |

[native-ci.json](native-ci.json) records the exact result lines, original
decoded UTF-8 log hashes after CRLF-to-LF normalization, and hashes of the
committed excerpts. The four small excerpts contain the source identity and
test results. Full logs remain retrievable through the linked job endpoints.
This review did not rerun native macOS hosts or present an abstract AU fixture
as an Apple ABI test.

The earlier [native qualification](../../qualification-2026-11-01/host-features/README.md)
at `156d82e` remains a historical **failed whole workflow**, with individually
passing feature gates and an unrelated Intel BBD failure. Of its 37 recorded
feature source files, **35 still match exactly** at this review. `Engine.cpp`
differs only in the subsequent ring/precise-envelope continuation change;
`Engine.h` adds the offline continuity-probe friendship. Their differing
hashes are preserved, rather than extending the older byte-identity claim.
The freshly retrieved successful native run tests the current full source.

## Preregistration identity correction

Two documents named local commit identities that were superseded when the
same contracts were published. The current files are byte-identical to their
published preregistrations, and each registration is an ancestor of the
corresponding implementation:

| Contract | Published preregistration | Implementation |
| --- | --- | --- |
| `host-automation.json` | `08e08e492b62b8078a3364d14754a6870c55ee22` | `3a37594` |
| `mts-client.json` | `6db0012c3f4d8c72b87ccc3ee591af461b4c2183` | `fa2016c2` |

`HOST_AUTOMATION.md` and `deps/mts-esp/README.vendor.md` now cite those published
identities. The contracts themselves remain unchanged. Retained mutation
qualification remains in the linked implementation evidence; only the actual
native AU mutation runner was freshly verified through current CI logs here.

No additional production defect was found in these requested feature paths.
The final integrated build, VCA qualification and release still need
their own exact-head evidence. A live external MTS master, real-DAW sessions,
signing secrets, trademark review and AAX are not claimed by this review.
