# Focused audit of the Linux post-SUCCESS crash

## Finding

**No new demonstrated plugin lifetime defect was found. The 2026.11.1 refinement introduces no plugin ownership, callback, timer, thread, registry, or teardown implementation changes. The original Linux GUI validator invocation remains failed, and its crash ownership remains unresolved.**

This is a read-only source and retained-evidence audit. It is not a validation pass or a repair. No build, audio render, validator replay, test run, network download, repository edit, or audition-artifact modification was performed.

The audited binary source is `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`, tree `88ee322bf66b380c4227e224821783f2db04bbee`; build run `45044040694`, attempt string `"1"`; Linux validator job `137324977642`. Current source was reviewed at `96c466f0e7c25055ac8e745d2c796af643860fc6`. The comparison baseline is the earlier 2026.11 candidate `d0c3c23be68ef048aa56969f1c493c221c1562c5`, which already had an unresolved post-SUCCESS failure.

## What the current failure establishes

| Invocation | Retained result |
| --- | --- |
| Original randomized GUI, seed `0x4369262`, PID 2883 | `SUCCESS` at `2026-11-22T19:24:05.9974371Z`, then raw return code `-11`, signal `SIGSEGV`, normalized exit `139` |
| Original headless, seed `0x3e72560`, PID 2851 | Exit `0` |
| Original fixed GUI, seed `0x2da72db`, PID 2936 | Exit `0` |
| Same-seed GDB replay, PID 2994 | Exited normally; no crashing stack |
| Same-seed fault-trace replay | Exit `0`; trace file contains handler-installation messages only |

The original diagnostic record explicitly says no local ELF core was available. Consequently, these files establish native signal termination, but cannot identify the faulting instruction, object, thread, library, or earlier corruption. Passing diagnostic replays do not replace this failed original invocation.

Evidence: `review-w09-linux/revision2026.11.1/run-45044040694/attempt-1/validator-job.log:531,680-682,866-881,1097-1100,1378-1383`; `pluginval-evidence/pluginval-gui-core.log:5-7`; `pluginval-evidence/pluginval-gui-gdb.log:155-156`. Absolute paths and SHA-256 hashes are recorded in `audit.json`.

## Changes that can be established exactly

Git object comparisons give the same `src/plugin` tree, `d7b52b190e002561b646e1eb9d10bd5186a9d8dd`, and the same `src/vendor` tree, `50f4f45d1704d9a13e16fdd79d48e8d30ea03f2e`, for d0c, 6d3 and the reviewed 96c commit. This establishes content identity of those complete directories, not merely a search that found no suspicious lines.

The only production-source changes from d0c to 6d3 are nine files: `CuratedLibraryLevels.inc`, `FactoryPresets.h`, and the Bass, Brass, Fx, Pads, Plucked, Polysynth and Winds family files. They change preset numeric assignments, four calibration gains, descriptions, and the revision string. CMake adds only the preset-character test target. The JUCE pin remains `c352e2489000ffeb9e397051377d75eef2efe641` (`CMakeLists.txt:927`). There are no `src` or CMake changes from be460b0 to 6d3, or from 6d3 to the reviewed 96c source.

The exact production/CMake diff is retained as `production-source-diff.patch`, SHA-256 `1f314a9d4bfa28e66200ad8cb139d5def82fe38d9504762fb16757e20fc97d29`.

Unchanged implementation does **not** prove that new values, heap layout, instruction layout or timing cannot expose an existing defect. It does mean there is no newly introduced destructor, registration, callback, or thread path to fix in this preset revision.

## Focused lifetime review

| Area and source | Source finding and limit |
| --- | --- |
| Preset creation: `src/core/CuratedLibrary.inc:177-192`; `src/core/FactoryPresets.cpp:643-655` | The builder copies program values and strings into owned preset records synchronously. The factory bank is an owned static vector; changed preset expressions do not create deferred callbacks or references to temporary `Program` objects. |
| Preset signatures: `src/plugin/PresetBrowser.cpp:23-60,2883-2894` | The immediately invoked initializer captures `bank` only while computing owned numeric signatures. The static vector stores `shared_ptr<const PresetSignature>`, not processor references or retained closures. No new teardown dependency is introduced by the changed values. |
| Editor and browser teardown: `src/plugin/PluginEditor.cpp:1030-1036,2350-2355`; `src/plugin/PresetBrowser.cpp:2508-2518` | The editor stops its timer before resetting its implementation. The implementation clears the meter callback and stops frame updates. The browser stops its timers, removes listeners and disconnects its list model. These unchanged guards are not proof of safety across every possible module-unload schedule. |
| Keyboard callbacks: `src/plugin/HardwareKeyboard.h:592,721-740`; `src/plugin/PluginProcessor.cpp:114-143` | The keyboard timer stops on destruction; its watcher unregisters and cancels pending async updates. Host MIDI is published through atomics, while keyboard-state servicing explicitly occurs on the message thread. I found no current producer/destructor overlap establishing a keyboard callback UAF. |
| Posted UI and undo callbacks: `src/plugin/UndoHistory.cpp:110-115,159-173`; `src/plugin/UiComponents.cpp:4036-4043,4083-4084`; `src/plugin/ParameterContextButton.cpp:146-150`; `src/plugin/KeyboardTuningPage.cpp:229-243` | The undo callback captures raw `this` alongside a checked weak lifetime token; the reviewed UI callbacks use component safe pointers. These guards rely on the existing message-thread or message-manager-locked teardown discipline, documented in `docs/PROGRAM_DISPLAY_NOTIFICATION.md:50`. They are not arbitrary cross-thread ownership mechanisms. No current evidence demonstrates that the discipline was violated. |
| MTS, static resources and threads: `src/plugin/MtsEspClient.cpp:22-26`; `src/vendor/mts-esp/Client/libMTSClient.cpp:162-196,239-246` | Client registration/deregistration and the module-global optional-library loader are unchanged. The current evidence contains no faulting stack or library mapping implicating them. Project core/plugin searches found no thread-start or FD-registration calls. Prior static-image/typeface/timer audit remains recorded in `docs/VALIDATOR_DIAGNOSTICS.md:101-129`; it was not rerun. |

## Existing host hypothesis is not a new finding

`docs/analysis/validator-6c560f1.md:116-166` already records the corrected immutable-source attribution: pluginval 1.0.4's pinned JUCE host callback captures raw `this`; later upstream commit `04e167d64661d3298afd4978f6c2e77f841b5f17` adds a shared-map/weak-pointer guard. The historical replay stack through `RunLoop::Impl` is relevant existing evidence, but neither it nor that source defect identifies the cause of the current original PID 2883 failure.

Host map lifetime, raw handler lifetime, reentrant registration mutation, plugin client-wrapper scope lifetime and earlier memory corruption remain distinct possibilities. The host implementation is not the same as the plugin's VST3 client wrapper. This audit supplies no basis for applying a speculative host repair or claiming one would fix this invocation.

## Action

Retain the original Linux validator failure and blocked release qualification. Available artifacts may be accurately described as development bundles with this limitation. This review does not justify a source patch, another CI run, a weakened gate, or a green-replay-based repair claim.

A future crash repair requires evidence connecting a demonstrated lifetime defect to the relevant path, or a stack from the original failing process. That evidence is absent here; no new experiment was launched. The completed audition verification and its files remain unchanged.
