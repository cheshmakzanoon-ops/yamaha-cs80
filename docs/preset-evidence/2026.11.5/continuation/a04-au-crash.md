# A04 — macOS AU GUI abort investigation

## Finding

The failed Apple Silicon qualification at source `c76ad288e02fbeca48f39652bf4fcdd99837f793`
does not have a demonstrated application-code root cause. Its original GUI
validator process aborted in **Editor Automation**, using seed `0x3c99748`, with:

```text
Assertion failed: (_commandCurrentStorage.currentOffset), function -[AppleParavirtCommandBuffer endCurrentChunk], file AppleParavirtCommandBuffer.mm, line 266.
pluginval received Abort trap: 6, exiting immediately
```

The original process outcome is failure, exit 9. Native tests and the two
headless validations passed; neither substitutes for the failed GUI lane.

Evidence: [original failure summary](../ci/source-c76ad28/macos-arm64/attempt-1-failed/failure-summary.json),
[original GUI output](../ci/source-c76ad28/macos-arm64/attempt-1-failed/contents-pluginval/pluginval-au-gui.log),
and [original producer log](../ci/source-c76ad28/macos-arm64/attempt-1-failed/original-producer-job.log).

## Existing renderer policy already applies

`Cs80AudioProcessorEditor::wantsLayerBackedView()` already returns false on
macOS. This is committed production code, introduced in `366c1454` during
revision 2026.11.2 after the earlier `0x2b4eef5` failure. Adding that override
again would not fix the present failure.

Pinned JUCE 8.0.11, revision `c352e2489000ffeb9e397051377d75eef2efe641`, establishes
the actual path:

1. `PluginUtilities::getDesktopFlags` maps the editor's false result to
   `windowRequiresSynchronousCoreGraphicsRendering`.
2. AU's `EditorCompHolder::createViewFor` passes these flags to `addToDesktop`.
3. `NSViewComponentPeer` only enables its asynchronous layer path when that
   synchronous flag is absent.

The product does not explicitly create an OpenGL context or a Metal renderer.
Its CMake configuration does not enable
`JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS`. The flag defaults to off;
switching it off again is not a new mitigation. Native images, host-owned outer
views, and OS window compositing can still involve the graphics stack.

Sources: [editor policy](../../../../src/plugin/PluginEditor.cpp),
[pinned JUCE wrapper utility](https://github.com/juce-framework/JUCE/blob/c352e2489000ffeb9e397051377d75eef2efe641/modules/juce_audio_plugin_client/detail/juce_PluginUtilities.h),
[pinned AU wrapper](https://github.com/juce-framework/JUCE/blob/c352e2489000ffeb9e397051377d75eef2efe641/modules/juce_audio_plugin_client/juce_audio_plugin_client_AU_1.mm),
and [pinned NSView peer](https://github.com/juce-framework/JUCE/blob/c352e2489000ffeb9e397051377d75eef2efe641/modules/juce_gui_basics/native/juce_NSViewComponentPeer_mac.mm).

## Hypothesis and its limits

The explicit assertion places the abort in Apple's paravirtualized graphics
command-buffer machinery during heavy editor automation. Graphics command
submission or compositing is therefore the leading investigation path.
No retained stack identifies whether the submitting caller is the plugin,
JUCE's host wrapper, pluginval's containing window, or another system layer.
An assertion in a driver alone does not prove that the plugin is correct or
that the runner alone owns the fault.

The repo's earlier [ARM investigation](../../../../tools/arm-gui-diagnostic/README.md)
also found that pluginval 1.0.4 catches `SIGABRT` and calls `_Exit(9)`, explaining
why an ordinary failed process may lack a useful macOS crash report. That
investigation was inconclusive: changed debugger timing produced
non-reproduction, not an established fix.

## Concrete next action

Retain the full existing randomized GUI lane and old fixed-seed regression.
Add a separate full-GUI strictness-10 replay of **`0x3c99748`**, preserving its
own process exit and output. It should run even if the preceding randomized
GUI lane fails, while the overall job must remain failed in that case.
The new lane must not become a rescue retry or replace the old seed.

For a repeated failure, capture a single bounded LLDB diagnostic on the same
built and registered AU and the same official pluginval binary. Break at
`abort` before pluginval's signal handler exits, retain all-thread stacks and
loaded-image addresses, and keep input hashes before/after. Preserve exact
source, runner image, OS, architecture, seed and both process outcomes.

`tools/arm-gui-diagnostic/replay.py` already implements a strict native/LLDB
pair with stop-aware inspection. It currently hardcodes source protocol
`5fa6ace` and seed `0x41dd1ce`; the frozen workflow cannot be dispatched as a
current-source diagnostic without a new explicitly versioned protocol.

Do not change rendering globally until that comparison identifies a credible
caller/path. If an alternate product renderer is subsequently tested, it
needs its own actual native GUI validation and visual checks; a source-level
configuration test is insufficient. A clean rerun with a new random seed
does not reproduce the original failure.

## Scope of this assignment

Read-only source, retained-log, historical-investigation and pinned-JUCE
review; this report is the only added file. No test gates, production code,
workflows, remote state or validator outcomes were changed. No native macOS
execution was possible in this Linux workspace.
