# Preset browser preference persistence

## Reproduction

At `54479a9a`, open the library with saved `#audition=off` and `#facet=style`
preferences. Click AUDITION and TECHNIQUE, then restart the plugin process.
Both choices revert unless an unrelated favourite or row-density action also
happened. The preference writer already serialized these fields, but their
controls never called it.

The retained negative witness uses real JUCE button dispatch and fresh
processes. It reports 12 expected assertion failures for immediate writes,
callback ordering and restart restoration. Every process terminates normally;
this is a functional preference regression, not a crash reproduction.

## Change

Only the AUDITION and STYLE/TECHNIQUE button handlers request persistence.
The setters accept an explicit `persist` argument that defaults to false,
matching the existing row-density pattern. Programmatic setters, construction
and catalogue refresh remain read-only. The audition choice is saved before
loading the selected row can notify a caller, so a reentrant reader sees the
new value.

The existing file format, favourite keys/order, row density and best-effort
write behavior are unchanged. A missing preference directory is created by
the existing writer. If writing fails, the selected choices continue to work
for the session. No preset metadata, XML admission, selection/navigation,
recall policy, parameter, DSP, UI lifetime or hardware behavior changes.

## Verification

A fresh Linux Release build using GCC 14.2 and repository-pinned JUCE 8.0.11
passes **85 checks in seven fresh processes**:

- Enable and disable through real button dispatch, then independently reopen
  in a new process for each direction.
- Immediately persist each individual choice without a later favourite or
  density action; the audition callback observes the already-saved value.
- Keep existing favourites and slim row density unchanged.
- Detect incidental same-content writes during constructor, layout,
  programmatic setters and refresh using a pinned fixture timestamp.
- Preserve exactly-once audition, silent disable and silent facet changes.
- Create missing preference storage and reopen it; keep session-only behavior
  and the blocking file intact when storage cannot be created.
- Repeat the active facet choice without changing the saved settings or sound.

All preference and preset fixtures are disposable. No real user settings,
Trash, desktop peer or native dialog is used. The driver overrides
`CS80_FAVOURITES_FILE` for each process.

The final source also passes **10/10 targeted CTests**, serially, exit 0:
preferences, REVERT, modern/session recall, user-file recall, XML admission,
failed LOAD, atomic save storage, loaded identity, catalogue refresh and
pending search keyboard actions. This is focused component/host-object
verification, not a full native plugin or registered-suite qualification.
No native-peer UI smoke, pluginval, macOS/Windows, real DAW or remote-CI result
is claimed for this local change.

Independent review verified the executable and all six changed source/test
hashes, found no blocker, and repeated the 85-check process test and all ten
CTest selections, both with explicit exit 0. A final build had no work to do.

```sh
cmake --build build-native --target CS80Snapshots
ctest --test-dir build-native -R '^cs80_preset_preferences_tests$' --output-on-failure
```

[Retained baseline and final evidence](evidence/preset-preferences-2026-11-12/).
