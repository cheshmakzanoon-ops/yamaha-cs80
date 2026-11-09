# Linux validator failure at 6c560f1: evidence and bounded diagnosis

**Original result: FAILED. Root cause: not established.** SUCCESS text is not a
process-exit pass. No change here relaxes a validation gate, removes a test,
retries the original gate, or claims a repair.

## Original invocation

Source `6c560f130be649dd4a63cbe146cd5cc54dd057a3`, build run
[42579617910](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42579617910),
Linux validator job
[129045088115](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42579617910/job/129045088115):

- Official pluginval 1.0.4, in-process, strictness 10, GUI under Xvfb,
  fixed seed `0x2da72db`. The native build verified plugin JUCE 8.0.11 at
  `c352e2489000ffeb9e397051377d75eef2efe641`.
- SUCCESS was logged at `2026-11-09T07:59:57.9580677Z`. The gate reported exit
  139 at `07:59:58.3353925Z`. No assertion failure was logged.
- Original PID **2713**, raw Popen returncode **-11**, kind **signal**, signal
  **SIGSEGV**. This is an observed native process signal, not inference from
  shell status 139 and not the diagnostic shim's numeric-exit convention.
- No local ELF core was available. The runner's existing kernel pipe collector
  prevented the local-file capture path; its configuration was not changed.
  The child's soft core limit was zero, which does not disable a pipe collector.
- The preceding headless seed `0x9d0e92` and randomized GUI seed `0x856d84`
  both returned raw 0. Their successes do not clear the fixed-seed failure.
- Both GDB and fault-thread replay steps were skipped: the existing workflow
  conditioned them only on the randomized GUI step failing. The fault-shim
  fixtures passed, but those are not an actual failing-validator trace.

The exact plugin artifact is **13658360097**, `cs80-linux-vst3`, 6,488,285 bytes,
ZIP SHA-256 `b506b1d4d45d514fef57d2bd4ed7ec4242e5e8753eeb9100055ea30916792b26`.
Its native plugin SHA-256 is
`b77c622647b9ec6f14818c9b94be738f2b00d5f3a35e758b1010b32e841cc6cb`.
Original report artifact **13657897113** is 7,936 bytes, ZIP SHA-256
`43b85262dc6fde05938876a8719850aef96631363f8919c0cc18fd120019cca6`.
Both archives were downloaded and independently hash-verified before inspection.
Selected original report files and identity metadata are retained in
`validator-6c560f1-evidence/`; no raw core or binary is committed there.

## Relation to the default-off optimization

In the actual failed binary, `Ig00156Batch::selectKernel(bool)` disassembles to
`endbr64; xor eax,eax; ret`; no accelerated implementation is present in its
symbols. Thus the new SIMD path was not executing. Plugin/GUI source, the JUCE
pin and original build workflow are unchanged from parent `68fcb88`.

This does **not** prove the new commit is unrelated. Even default-off integration
adds Engine members, a dispatcher translation unit, a constructor selector call
and a scalar clock-helper extraction. Binary layout or timing can expose a
latent lifetime defect. Exact DSP corpora and passing core/sanitizer checks do
not rule out GUI lifetime corruption. Neither "SIMD caused this" nor "the
validator is definitely at fault" is justified by the available original evidence.

## Local observation and limitation

The local environment could not create Xvfb local/unix listener sockets, including
through the permitted escalated execution path. No GUI plugin invocation ran in
those display preflights. No system security or collector setting was changed.

Exactly one additional **headless variant** used the exact plugin artifact,
official released pluginval 1.0.4, seed `0x2da72db`, strictness 10 and the existing
fault-thread shim after its fixtures passed. It returned 0 with SUCCESS and no
fault trace. This uses `--skip-gui-tests` and a different local runtime, so it is
not a reproduction of the failed GUI invocation and is not exonerating evidence.
No retry-until-green loop was run. Logs retain this altered scope explicitly.

## Bounded same-artifact GUI diagnostic

`.github/workflows/validator-6c560f1-diagnostic.yml` is a separate investigation,
not a validation gate. It is triggered only by its own file change or explicit
manual dispatch, and is pinned to the original source/artifact/seed above:

1. Verify artifact ID, name, source commit, workflow run, expiry, ZIP size/hash,
   exact two-file inventory, non-symlink entries and native-plugin hash.
2. Use original-source diagnostic scripts and the official pluginval 1.0.4 Linux
   archive pinned to SHA-256
   `c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352`.
   This identifies the diagnostic tool; the original runner did not publish a
   validator-binary hash, so original tool-binary identity is not overstated.
3. Run one same-seed GUI invocation under GDB and one under the existing
   fault-thread shim, each bounded to 240 seconds plus 10 seconds termination
   grace. GDB retains ASLR; both modes still alter execution timing.
4. Preserve process/log statuses and upload stack/report text and tool
   provenance, never raw cores, argument/local-value dumps, binaries or secrets.
   A passing diagnostic is merely nonreproduction. Original run 42579617910 stays
   failed regardless of this separate workflow's result.

Local workflow checks parsed YAML and every shell block, verified the two bounded
invocations and read-only token permissions, accepted the exact original archive,
and rejected eleven deliberately wrong identity/digest/inventory/symlink cases.
`workflow-checks.json` records all twelve controls. The reproducible checker
requires PyYAML plus a directory containing `exact-plugin.zip` and
`original-artifact-metadata.json`:

```sh
python3 docs/analysis/validator-6c560f1-evidence/check-artifact-workflow.py --evidence /path/to/downloaded-evidence
```

At this report's preparation, no same-artifact GUI replay result is claimed yet.

## Diagnostic-coverage correction

A separately tested selector now supplies the **same failed log** to both
existing GDB and fault-trace paths. It selects a randomized-GUI failure first,
or the fixed-seed failure when randomized GUI did not fail. Cancellation and
unknown outcomes select nothing. Even when both fail there remain at most one
GDB and one fault-trace diagnostic. Existing strict gates, original core helper,
seed parsing, timeouts, retry rules and exit propagation remain unchanged.

The new seven-method test suite covers all 25 allowed outcome pairs, malformed
and argument cases, both seed paths and workflow wiring. Its CTest selection and
the existing 13 retry cases / seven exit-classification tests pass. This corrects
missing evidence collection, not the underlying crash.

## Falsifiable teardown hypotheses, not attribution

**Source-attribution correction (2026-11-09):** an earlier version of this
section incorrectly attributed a local, uncommitted weak-map patch to released
pluginval 1.0.4. That attribution is retracted. The comparisons below use
immutable `git show <commit>:<path>` contents, not the modified worktree.

Pluginval tag `v1.0.4` peels to
[`ed19c2c16b57a6d94db391bea3ef4a80b769d5bf`](https://github.com/Tracktion/pluginval/tree/ed19c2c16b57a6d94db391bea3ef4a80b769d5bf);
its JUCE gitlink is `5179f4e720d8406ebd1b5401c86aea8da6cc83c9` (8.0.3 headers).
In that immutable [host source](https://github.com/juce-framework/JUCE/blob/5179f4e720d8406ebd1b5401c86aea8da6cc83c9/modules/juce_audio_processors/format_types/juce_VST3PluginFormat.cpp),
`RunLoop::Impl::registerEventHandler` captures raw `this` in the FD callback,
indexes `eventHandlerMap[descriptor]`, and iterates raw `IEventHandler*` pointers.
The source blob is `14611a9b5bf248115b778560d705cf12d68805f8`, SHA-256
`8b916794b33c32fc886b329d17a8df2510da3ff24415a31494868d3715ef4f47`.
Source inspection is not a claim that every released validator instruction was
rebuilt from this checkout or that the original runner's binary was symbolized.

Upstream JUCE [commit `04e167d64661d3298afd4978f6c2e77f841b5f17`](https://github.com/juce-framework/JUCE/commit/04e167d64661d3298afd4978f6c2e77f841b5f17)
(3 September 2026) introduces the shared-map/weak-pointer guard. Its explanation
describes a queued FD callback running after another callback destroys `Impl`
and unregisters that FD. The guard checks map lifetime and current registration
before dispatch. It is absent from both the pinned 8.0.3 source and the
[equivalent 8.0.11 hosting class](https://github.com/juce-framework/JUCE/blob/c352e2489000ffeb9e397051377d75eef2efe641/modules/juce_audio_processors_headless/format_types/juce_VST3PluginFormatImpl.h),
and is an ancestor of JUCE 9.0.3 at `be29c81492b6151c8ea8d14c840e1311963b3a83`.
The presence of this hosting class in the plugin's JUCE checkout does not mean
the plugin executes it: the VST3 client wrapper and host implementation are
different code paths.

A queued callback outliving the host `Impl`, a raw handler surviving its final
lifetime/module unload, or unregister/re-register mutation during an in-flight
callback are therefore distinct hypotheses. The later weak-map guard addresses
the first source-level lifetime risk; it does not by itself establish every
handler's lifetime. None of these schedules has been demonstrated for original
PID 2713, whose fault stack was not captured. The historical callback-path replay
and any isolated lifetime fixture cannot establish that invocation's ownership,
exclude plugin corruption, or replace its failed gate.

Plugin JUCE's `ScopedRunLoop` is owned by controller, component, view and factory
creation scopes. A registration surviving `IPlugView::removed()` alone is
therefore not proof of a leak. The relevant boundary is the final component /
controller / scope destruction followed by actual module close. pluginval deletes
the instance on its message thread, sleeps 150 ms, then signals the worker;
SUCCESS is printed after tests return, not after proof that all host dispatch
and process teardown are complete.

If the two diagnostic modes do not isolate ownership, the next useful experiment
is a separate instrumented host/plugin build recording registration generations,
callback entry/exit/nesting, final scope destruction and module-unload boundaries.
It must distinguish stale registration from reentrant mutation, without
reclassifying the original failure or silently changing production behavior.
