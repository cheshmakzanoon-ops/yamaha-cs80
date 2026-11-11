# Preset catalogue refresh safety

## Reproducer and boundary

With the USER filter active, leave a factory patch sounding and select saved
patch C from files B and C. Add saved patch A from another instance or outside
the browser, then refresh the library. The old implementation selected B:
the old row-to-entry mapping was applied to the replacement catalogue.

Removing the selected final file is the more serious version of the same
fault. Its old entry index is past the end of the new catalogue, and copying
the previous selection key reads a destroyed entry. Refresh follows the
browser's DELETE action as well as successful saves and library reopening.

The fix captures the selected filename-based key before replacing entries,
invalidates the old row mapping while its entries are still alive, and
restores that key against the new catalogue. Row lookup also rejects an
out-of-range entry index. Selection priority stays unchanged: the sounding
patch wins when visible; otherwise a surviving previous selection remains,
or an active search selects its first hit. A removed selection without a
search or visible sounding patch becomes unselected. Refresh cancels an
armed deletion and never auditions the restored selection.

The second reproduced fault used AUDITION with an unsaved/custom patch name
not in the catalogue. Restoring its factory-slot fallback selection loaded
that factory patch over the live edit. Programmatic selection now uses a
scoped, nesting-safe restoration guard. Ordinary row selection still
auditions, while the save-name field retains its prior selection behavior.

This change does not alter preset data, parameter order, DSP, save/overwrite
semantics, or physical pedal/output/session recall policy. The separate
question of REVERT restoring all opening source/MEMORY state is outside its
scope.

## Regression

`tests/PresetRefreshTests.h` drives the production ListBox selection callback
and actual file enumeration using a temporary preset folder. Files are moved
within that fixture to reproduce removal without touching a real trash folder.
Coverage includes earlier insertion/removal, selected final removal, an empty
user catalogue, deletion disarming, five sort modes in both directions,
audition enabled during refresh, search fallback, sounding-patch selection,
custom-name fallback without sound mutation, and nested restoration guards.

The original-code baseline fails deterministic identity and live-sound
assertions; it does not depend on undefined behavior or a crash. The test
stops on the identity failure before attempting the dangerous removal cases.

```sh
cmake --build build-native --target CS80Snapshots
ctest --test-dir build-native -R '^cs80_preset_refresh_tests$' --output-on-failure
```

The same regression is included in the full UI smoke suite. Its dedicated
CTest runs directly with offscreen JUCE components, without creating a native
peer. It is not an OS-pointer, popup, screen-reader or DAW interaction test.

## Recorded qualification

The original production code at `3d71f110` fails **2 of 8** executed checks:
the custom sound changes during refresh and insertion changes selection
identity. The corrected final source passes **39 checks with zero failures**.
Twelve focused/adjacent CTests also pass sequentially, covering preset display
names, XML values, preview math, refresh, undo gestures, two component lifecycle
gates, CLI admission, controller reset, keyboard handoff, context-menu keyboard
handling and typed-value units. These are targeted results, not a full-suite
pass.

Independent final-source review found no blocker. An independent rerun
verified the exact binary and source hashes, then passed the same **39 checks**
and **12 adjacent CTests**, both with exit 0.

The isolated Linux Release build uses GCC 14 and the repository-pinned official
JUCE 8.0.11 checkout. No existing build cache was reconfigured. The final
snapshot executable SHA-256 is
`797af7d960e8fa8d6a6568efaba7f3cb11ef9255f9e08893e9b4bb65b1036e2b`.

An attempted Xvfb wrapper could not establish local listening sockets in this
execution environment. No native-peer/full UI smoke, sanitizer, pluginval,
macOS/Windows or remote CI qualification is claimed. The focused test needs no
native peer and runs directly, like the existing component gates.

[Baseline, final tests, environment observation and hashes](evidence/preset-refresh-2026-11-11/manifest.json).
