# Loaded preset identity

## Reproduction

On `d5ed3ce3`, save two different sounds to `A Identity.cs80xml` and
`B Identity.cs80xml`, then give both XML files the same `presetName`. Load B
through the real browser list and LOAD button. B's parameter values are recalled,
but the loaded marker identifies A and the button remains LOAD rather than
RELOAD. Toolbar NEXT/PREV consequently step from A instead of B.

The retained baseline witness runs real processor saves, XML edits, browser
selection and button callbacks. Of 120 checks, only the three new assertions
for loaded identity, NEXT and PREV fail; the existing failure/retry suite passes.

## Contract

The processor now owns transient preset provenance alongside its name/index:
unknown, factory program, or exact user file. It is not a browser selection cache.

- Successful file load/save publishes the exact `juce::File`. Failed parses,
  missing-file loads, failed writes and rejected overwrites preserve provenance.
- Factory, INIT and MIDI program recall replace it with factory provenance.
  MIDI still publishes only its existing atomic index on the audio thread. The
  metadata reader resolves that index; rename consumes it without reviving an
  older user-file origin. No file/string work or locks were added to the callback.
- Name edits, hardware-source edits and randomise retain provenance. Parameter
  edits still use the existing edited-state rules. Name/index equality alone
  does not establish origin, including user files named after factory programs.
- In-process A/B snapshots, copy/toggle undo/redo, randomise metadata history and
  the browser opening/REVERT snapshot carry provenance with the sound. A change
  of origin alone can enable REVERT even when names and sound values are equal.
- Closing/reopening the browser or constructing another browser does not change
  provenance. Refresh retains the selected-file and opening-sound policies.
- Session loading intentionally restores **unknown** provenance, including old
  sessions and serialized inactive A/B slots. Neither preset XML nor session/A-B
  XML gains file paths or a new schema field. These sounds can have a fallback
  selection, but no loaded marker until an explicit library load/save establishes
  origin. The editor displays them as USER PATCH rather than guessing FACTORY.

A known user file is matched by full `juce::File` equality, with JUCE's existing
platform case policy. The catalogue's filename-based keys/favourites are unchanged.
A missing/moved file or one loaded from outside the catalogue gets no loaded row;
another same-name or same-basename file is never substituted. If the original path
reappears, refresh can match it again. This is path provenance, not inode identity,
a file watcher, a content hash, or a persistent link after external rename.
Replacing bytes at the same path remains the same origin and the existing edited
comparison uses the catalogue modification-time cache. REVERT restores its owned
sound even if the original file is missing; it does not reload the file.

The editor's factory labels and display/edited baselines also consume provenance,
so switching equal names/indexes does not reuse another file's baseline. Fresh
`Init Patch / Original SUB` retains its existing edited-baseline treatment.

## Publication and scope

Name, index and origin use the existing non-audio metadata lock. Successful save
publishes before the host-display callback without rewriting the live program
index, preserving the previous save behavior during MIDI publication. Audio MIDI
publication remains the existing atomic override, without a new lock, allocation or retry loop. This does
not make sound capture/recall atomic against concurrent host automation or change
the existing ordering of parameter, tree and metadata notifications.

No DSP, audio parameters/order, state normalization, file admission, favourites,
physical-setup recall, PURE policy or fader-control code changes are involved.

## Verification

The isolated native regression uses `PresetIdentityAccess::run`, including
same-name/different-value files, navigation, factory collisions, external recalls,
metadata edits, separate browsers, reopen/refresh, missing/repair, outside-path
basename collisions, overwrite, A/B/history, session omission, MIDI-then-rename
and identity-only REVERT. Files stay in disposable temporary fixtures. Moving a
fixture out of its catalogue models deletion; the tests never operate real Trash.
The existing atomic-save storage tests also check origin preservation on failed
writes/publication without opening modal dialogs. The callback-allocation gate
starts MIDI recall from a real saved-file origin after dropping the save result,
so the processor owns the retained path while allocation/free probes are armed.

The final Linux Release source passes **18/18 targeted CTests** serially:
79 identity checks, 608 existing save-storage checks, 112 failed-load/retry
checks, 1,387 REVERT checks, 2,354 modern/session/A-B checks, 2,784 normalized
file-recall checks, and 228 callback-allocation checks. Adjacent gates cover
XML display names/values, undo gestures, two component lifecycles, CLI admission,
controller reset, keyboard handoff, context-menu keyboard and typed values.

```sh
cmake --build build-native --target CS80Snapshots cs80_audio_callback_allocation_tests
ctest --test-dir build-native -R '^cs80_preset_identity_tests$' --output-on-failure
ctest --test-dir build-native -R '^cs80_preset_save_storage_tests$' --output-on-failure
```

The snapshot executable SHA-256 is
`06bedba96e0d59120550ffe1cddd847235afd32ec2c23835a73360cf75c414ec`.
A final snapshot rebuild had no work. The final save-publication adjustment
was rebuilt into both snapshot and allocation executables before all 18 tests
were rerun (8.95 seconds, exit 0). The storage-only harness
initially failed to compile because its no-op event pump omitted an integer
argument; that test-only error was corrected before final qualification.

The full native-peer UI smoke was attempted but **did not start**: Xvfb could
not establish local listening sockets. Offscreen editor components are covered,
but native windows, dialogs, OS input, screen readers and full UI smoke are not
qualified. No full registered-suite, sanitizer, pluginval, macOS/Windows,
real-DAW or remote-CI pass is claimed. The isolated build uses GCC 14.2 and
repository-pinned official JUCE 8.0.11; no other cache was reconfigured.

Independent final review found no blocker, verified all 15 source and four
binary hashes, and repeated **18/18 tests** (8.95 seconds, explicit exit 0).
The review includes the corrected save publication, deferred MIDI provenance,
A/B/session lifetime and editor cache boundaries.

[Exact hashes, baseline, final logs and limits](experiments/preset-loaded-identity/manifest.json).
