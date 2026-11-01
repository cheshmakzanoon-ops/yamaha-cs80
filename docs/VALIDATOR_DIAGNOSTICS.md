# Validator crash diagnostics

The original validation result remains authoritative: an exit-zero run with an
explicit clean SUCCESS is required. A diagnostic rerun never replaces a failed
validation attempt. The existing bounded retry eligibility is unchanged and is
not evidence of crash ownership.

Each actual Linux validator invocation is wrapped by run-pluginval-with-core.py.
It runs in a private temporary directory with absolute executable, plugin and
report paths. Only the child's core resource limit is changed, bounded to 1 GiB
when the existing kernel pattern writes a local file and its hard limit permits.
No sysctl, kernel core pattern, collector configuration or security setting is
changed. Linux pipe collectors ignore RLIMIT_CORE, even a zero soft limit;
existing collector behavior is not claimed to be disabled.

A local ELF core candidate can produce an argument-free thread backtrace and
shared-library listing. Its PID is not automatically matched, so the log explicitly
labels it an unverified candidate rather than ownership proof. No raw core,
environment, local-variable, register or memory dump is uploaded. Private core
files are removed locally when possible; cleanup/optional diagnostic failures
cannot replace a completed child process's exit status.

The separate failure-only GDB rerun checks for a live inferior before requesting
a stack. A normally exited process is not misclassified merely because registers
are no longer available. CI fixtures check exits 0, 7 and 139, an unwritable
optional log, normal-exit GDB handling, and actual signal propagation only when
the kernel is not using a pipe collector.

This is diagnostic infrastructure, not proof that a crash is in the validator,
the plugin, or a particular library. Tests printing SUCCESS before process exit
do not turn exit139 into a pass. Record exact source, validator version, run,
attempt and evidence when diagnosing intermittent failures.


## 2026-10-29 checkpoint: post-success exit remains a failure

Commit `f89de6382b40c278e02497cc439178ca442c9dc7`, [build run40925492958](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40925492958), Linux validator job123632463354:

- Headless seed `0x382bb75`: SUCCESS and exit0.
- GUI seed `0x2afb526`: SUCCESS, then exit139; this gate failed.
- Known state-regression seed `0x2da72db`: SUCCESS and exit0.
- Same-seed GDB and fault-trace diagnostic replays exited0. Nonreproduction is inconclusive; they do not replace the failed original invocation or establish ownership of this particular crash.
- All four platform build/test jobs, Windows validation and both native macOS VST3/AU/GUI validations passed. DSP, compatibility and screenshot workflows passed. The overall build workflow remains failed because of the Linux GUI process exit.

The previous executable checkpoint `1962ae5308521c6fa03ea9c3040cc9cada5472a4` passed all its workflows; that does not erase this later intermittent failure. No assertion failure was logged in this invocation, and no new crash stack was obtained. Pluginval remains the official released1.0.4; its latest release was rechecked on2026-10-29. Do not relabel a post-SUCCESS crash as a pass, retry until green, or infer a fixed teardown defect from successful diagnostic replays.

## 2026-10-29 PRA reference checkpoint

Commit `2c93ca0f0df9992d8839f094d4f5a25004dea11c`, [build run40933500223](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40933500223), Linux validator job123660820515:

- Original headless seed `0x65245c3` printed SUCCESS, then exited139. No assertion failure was logged; the process gate failed.
- GUI and fixed-seed validation were not reached after the headless failure. Do not count them as passed.
- No local ELF core was available and no new crash stack was obtained. Diagnostic infrastructure fixture tests passed, but no actual same-seed validator replay was recorded in this job.
- DSP numerical, sanitizer, raw legacy-compatibility and screenshot workflows passed. These do not override the failed host-validator gate. All four platform builds/tests, both native macOS validations and Windows validation also passed. The overall build remains failed solely at the recorded Linux validator gate.

This checkpoint adds offline reference code, not a validator repair. A later successful run, if any, cannot erase this original failure.

## Subsequent OE2 reference checkpoint

Commit `594f4b36e834c3faed991b41fbc14260a0f0bb93` passed [build/platform validation run40934695128](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40934695128), [DSP/sanitizer/raw-compatibility run40934695058](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40934695058) and [screenshots run40934695046](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40934695046). Both native macOS jobs, Windows and Linux validators completed successfully for this exact checkpoint. No validator repair was included; the earlier failed invocations remain failed and the intermittent issue remains unresolved.

## 2026-10-29 rollback-fixture correction checkpoint

Commit `411452eb071cf6a8c4cd6530d45f25e77e90442b`, [build run40967809044, Linux validator job123779240630](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40967809044/job/123779240630):

- Headless seed `0x1dfb33c` passed. Original GUI seed `0x63421ca` printed SUCCESS and then exited139; the job failed. The fixed regression seed `0x2da72db` passed.
- Existing same-seed GDB diagnostic replay exited normally. The existing fault-thread diagnostic replay instead reproduced exit139 after SUCCESS.
- Its trace includes `juce::RunLoop::Impl::registerEventHandler` callback invocation, `juce::LinuxEventLoop::registerFdCallback`, and `juce::MessageManager::runDispatchLoop`. Frame0 is an unsymbolized address. This supports an event-callback-path investigation, but does not establish exact object lifetime, fault ownership, or a verified repair.
- No local ELF core was collected. The diagnostic trace is evidence from a replay, not a stack captured from the original failed invocation. Neither a passing replay nor SUCCESS text changes that failed gate.
- The separate macOS ARM rollback-fixture failure from `a494118` is resolved in this checkpoint's ARM job: all tests and native validators passed. DSP/sanitizer/legacy-compatibility workflow40967809049 passed. Other platform results must be read from the exact run rather than assumed.

No validator or production DSP repair is included in the fixture correction. The intermittent Linux GUI failure remains unresolved and visible; this checkpoint is not fully qualified.

## 2026-10-30 standalone source-tone checkpoint

Commit `feff6b9606a150b5627187554c39b8ddd47e6bc9`, [build run40977141585, Linux validator job123809934341](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40977141585/job/123809934341): headless seed `0x4f31605` exited139 after the logged completion of `Fuzz parameters`, **without an overall SUCCESS line**. This is not labeled a post-SUCCESS pass. No local ELF core or new crash stack was obtained. GUI and fixed-seed stages were not reached. All four platform builds/tests, both native macOS validation jobs and Windows validation passed; DSP/sanitizer/compatibility and screenshots also passed. The Linux validator failure remains a failed overall gate with unestablished ownership. This commit added a standalone converter, not an audible source route or validator repair.

## 2026-10-31 integration review: code paths that outlive an instance

Phase 7 asks for a root cause by code review rather than reruns. Review of the
merged tree (all eight workstreams) for anything that could still run, or still
be referenced, after the host destroys the last instance and unloads the module:

| Candidate | Finding |
| --- | --- |
| Function-local statics holding JUCE objects | `ArtCommon.cpp` keeps the two embedded Barlow `Typeface::Ptr`s and the wrinkle tile `Image`; `UiComponents.cpp` keeps the LRU `PaintCache` of `Image`s; `PluginEditor.cpp` a crinkle-tile `Image` map; `PresetBrowser.cpp` a relief `Image`. All are plain heap or reference-counted objects destroyed by the module's static destructors. |
| Timers | `UndoableParameterState` (owned by the processor) and the editor-owned toolbar, tour, help, drawer and page timers; every one is a member of an instance or its editor and stops in its destructor. None is static. |
| Threads | none started by project code (the library audition and MIDI paths use the host's threads). |
| File-descriptor callbacks | none registered by project code. On Linux only JUCE's VST3 wrapper (`EventHandler`) registers descriptors with the host's `IRunLoop`. |

Evidence added: `CS80Snapshots --teardown-probe` (registered as
`cs80_teardown_probe`) runs one module life cycle - JUCE start, an instance and
its editor painted at both sizes (warming every cache above), destruction, JUCE
shutdown, then the static destructors after `main` returns. Under valgrind
memcheck on this container (Xvfb, 2026-10-31, tree `00ceccf` plus the probe):
**0 errors**, exit 0. So the static resources above are destroyed cleanly after
JUCE's shutdown; they are not shown to be the fault.

The only captured fault trace (2026-10-29, fault-thread replay above) shows the
host's `RunLoop::Impl` invoking a registered descriptor callback whose frame 0 is
an unsymbolized address, consistent with a callback into code that is no longer
mapped. Project code registers no descriptors; within the plugin, only JUCE's VST3
wrapper does. Root cause: **not established**; no repair is claimed. The Linux
validator jobs on the integration branch passed headless, GUI and the fixed seed
on `3961c1c` ([build run 41190530504](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41190530504), job 124509162512); later runs are listed in
[STATUS.md](STATUS.md). A passing run is not a fix. Next step for whoever can run
pluginval locally: compare GUI validation runs that keep the module loaded with
runs that unload it, with debug logging added around the JUCE VST3 wrapper's
run-loop registration, to see whether a wrapper `EventHandler` registration
survives `IPlugView::removed()`.

## 2026-11-01 root cause: JUCE's *validator-host* VST3 RunLoop UAF

The original failing Linux pluginval v1.0.4 logs were retrieved directly from
GitHub Actions (not inferred from later passing runs):

| Linux job | Stage and seed | Actual verdict |
| --- | --- | --- |
| [123632463354](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40925492958/job/123632463354) | GUI `0x2afb526` | SUCCESS, then exit139 |
| [123660820515](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40933500223/job/123660820515) | Headless `0x65245c3` | SUCCESS, then exit139 |
| [123779240630](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40967809044/job/123779240630) | GUI `0x63421ca` | SUCCESS, then exit139; diagnostic replay fault in JUCE RunLoop callback |
| [123809934341](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40977141585/job/123809934341) | Headless `0x4f31605` | exit139 after `Fuzz parameters`; no overall SUCCESS; failed gate |

The exit139 mechanism is now established by an independent, publicly documented
upstream JUCE fix, not by a green rerun. [JUCE commit
`04e167d64661d3298afd4978f6c2e77f841b5f17`](https://github.com/juce-framework/JUCE/commit/04e167d64661d3298afd4978f6c2e77f841b5f17),
"VST3 Host: Fix an occasional crash when removing callbacks from the message loop
during shutdown", describes the exact vulnerability. The Linux event loop copies
ready fd callbacks before invoking them. Callback A can release the last VST3
host `RunLoop::Impl`, unregister callback B and destroy its fd-handler map;
callback B is still in that dispatch batch and invokes a lambda capturing the
freed `this`. Its subsequent `eventHandlerMap[fd]` is use-after-free. The upstream
[pluginval issue 185](https://github.com/Tracktion/pluginval/issues/185)
independently reproduces exit139 after SUCCESS with the *identical* stack:
`std::unordered_map::operator[]` from `juce::RunLoop::Impl::registerEventHandler`
then `juce::LinuxEventLoop::registerFdCallback`, all inside the validator host.
pluginval v1.0.4 embeds JUCE 8.0.3 (`5179f4e720d...`) and predates this fix.

An unchanged plugin VST3 built locally in `RelWithDebInfo` reproduced the fault
under the released pluginval 1.0.4 Linux binary and the same failing headless
seed `0x4f31605`, with at least three SIGSEGVs across repeated batches. An
`LD_PRELOAD` *diagnostic-only* signal handler captured a fault in pluginval's
host ELF at relative instruction offset `0x394849`, resolved by `objdump` and
`nm -C` to the fd callback's `unordered_map<int, vector<IEventHandler*>>`
lookup; the faulting instruction was **outside the plugin's ELF mapping**. This
matches the original October 29 RunLoop callback trace and the upstream
crash stack. The signal handler did not continue execution or change a gate
verdict. GDB ptrace was prohibited in this container, so no local core/backtrace
is claimed. The signal-address/register diagnostic remained local and is not
added to CI artifacts.

The Linux-only CI repair builds the *same pluginval v1.0.4 test suite* from
upstream pinned commit `ed19c2c16b57a6d94db391bea3ef4a80b769d5bf` and
its pinned JUCE submodule `5179f4e720d8406ebd1b5401c86aea8da6cc83c9`.
It applies the exact ownership mechanism of JUCE `04e167d` to that older
JUCE source via
[`scripts/patches/pluginval-juce-vst3-runloop-04e167d.patch`](../scripts/patches/pluginval-juce-vst3-runloop-04e167d.patch).
The fd callback now captures a `weak_ptr` to the map, returns if expired, and
never accesses the destroyed RunLoop. Source verification checks that vulnerable
raw ownership is absent and rejects four mutations (raw lambda owner, absent
weak lock, raw map storage, and removed key guard). The local patched validator
built successfully and passed **30/30** repeated strictness-10 headless trials
on the same unchanged VST3 and original seed; a passing 30-run batch is
corroboration, **not** the sole evidence of a repair.

Local GUI coverage is separate and limited: on this container's Xvfb setup,
both the unmodified release host **and** the patched source-built host failed
at `pluginval / Editor...` with seed `0x63421ca`. A local signal probe resolved
the patched host's different, early Editor-stage fault to
`juce::XEmbedComponent::Pimpl::componentParentHierarchyChanged` (host image
offset `0x40b8cc`, fault address `0xe0`), not the RunLoop map callback site.
There is no evidence that the weak-pointer patch caused this local GUI failure,
but there is also **no local passing GUI result**. It must not be called green
until the unchanged strict GUI test succeeds on GitHub Actions under its
configured Xvfb environment; any persistent Editor-stage crash there is a
separate blocker requiring diagnosis.

Crucially, no synthesizer DSP, JUCE plugin-wrapper, processor timer, editor
timer, font/image cache, `PaintCache`, undo flush or other plugin lifetime code
is changed by this validator-host fix. The original crashes also reproduced
with `--skip-gui-tests`, when editor-only resources are not required. A newly
vendored ODDSound MTS-ESP client's module-static `dlopen`/`dlclose` is a distinct
future teardown consideration, **not** the cause of failures recorded before
it was introduced. The original validation strictness, required `SUCCESS` *and*
exit-zero check, diagnostics, and retry rules remain unchanged; the workflow's
post-change green run is still required before qualifying this CI correction.
