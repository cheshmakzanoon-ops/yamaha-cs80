# Preserve live modulation routes while editing

The Studio modulation matrix now changes only the field the player selects:
source, via, destination or rank. The other three fields are read from the
current session route rather than copied back from potentially stale widgets.

`ModernLayerState` publishes state changes immediately and notifies the page
asynchronously. Previously, a choice made before that notification rebuilt all
four route fields from the old row. For example, choosing a destination after
a session restore or Undo could silently restore an older source, via and rank.
The page now merges the one selected field into the live route and refreshes
its controls immediately.

Each effective routing choice also has a separate, named Undo step, including
successive choices before the next event-loop turn. Selecting an already-live
value refreshes the row without writing state or consuming the redo branch.
The latent rank remains intact when a global or MSEG-rate destination disables
its control.

This changes UI edit delivery only. No DSP policy, routing enum, state format,
host parameter order or modulation amount changes. This is not a redesign of
cross-thread configuration publication or a claim of atomicity across arbitrary
concurrent state writers.

## Regression evidence

The unchanged production code at `b6b73a6aac5454a0473ab6d3b8217e54c9c2d2d8`
failed **196 of 388 assertions** in the initial real-component fixture. The
same fixture passes **388/388** after the fix. The negative-control binary,
fixture and failed log were retained separately; this was a reproduced state
loss and Undo grouping defect, not a speculative UI change.

`cs80_mod_matrix_routing_ui_tests` / `CS80Snapshots --mod-matrix-routing-ui-tests`
exercise all 16 rows and all four fields with a pending external refresh, whole
configuration preservation, nonzero signed amount/rate and host notification preservation, named
closed Undo steps, repeated Undo/Redo, actual host-state restore, a pending Undo
refresh, no-op redo preservation, new edit branches and destination-dependent
rank availability.

The fixture invokes real JUCE ComboBox callbacks without a native window or OS
popup. It does not qualify physical mouse/keyboard interaction, screen readers,
DAW hosting, the full UI smoke suite or Windows/macOS runtime. No Xvfb/socket or
ptrace access was attempted. Existing unrelated qualification failures remain
unchanged.

Final local Release validation: **481 focused assertions passed**. Thirteen
selected CTests passed: routing UI, core matrix, core MSEG, MSEG text,
point values, shape copy, editor lifecycle, modern sliders, modern recall,
preset revert, preset-file recall, Undo/replay gestures and snapshot CLI.
Independent review verified the exact final binary and source hashes, repeated
all 481 focused assertions and seven adjacent native suites successfully, and
found no source blocker. It did not repeat the full thirteen-suite selection.
