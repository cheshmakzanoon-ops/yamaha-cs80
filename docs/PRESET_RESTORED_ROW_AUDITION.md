# Audition a silently restored preset row

## Reproducer

With AUDITION enabled, search for a saved patch that is not sounding. Search
correctly highlights the first match without loading it. Clicking that already
highlighted row previously did nothing: JUCE does not emit `selectedRowsChanged`
for an unchanged selection, and the browser had no `listBoxItemClicked` handler.
Enter or a double-click still loaded the patch, so the single-click audition
contract depended on whether the browser had already highlighted the row.

## Bounded fix

A stable key records a selection restored without audition. An explicit click
on that same row and key can audition it. Normal selection changes clear this
key before their existing audition path, so the subsequent JUCE click callback
cannot repeat a successful or failed attempt. No pointer timing or asynchronous
callback order is inferred.

The click checks the current loaded identity first. Repeated clicks on an
already sounding patch do not reload over edits. A failed explicit click keeps
the restored identity so another click can retry if the file becomes available.
Empty results/deselection clear the identity, and sorting follows the stable
file key rather than a former row number.

This fixes the silently restored highlight case. It does not redefine unrelated
click/reselection policies. Filtering and refresh remain silent; ordinary
selection, keyboard navigation, audition toggling and toolbar NEXT/PREV retain
their existing paths. No taxonomy, preference persistence, save/overwrite,
REVERT, preset recall policy, DSP or parameter-layout change is involved.

## Regression

`tests/PresetClickTests.h` creates two valid saved patches in a disposable folder
and drives JUCE's real row `mouseDown`/`mouseUp` handlers through the production
list components. It does not call the new model callback directly or create a
native peer.

The fixture covers:

- Silent search restoration and explicit click with AUDITION on and off.
- Existing selected-row audition toggling and keyboard navigation.
- Repeated clicks preserving edits to an already sounding patch.
- Ordinary different-row selection loading exactly once, including a callback
  that deliberately changes the sounding state.
- Stable selection across sort order changes, and invalidation at empty results.
- Missing-file failure and a later explicit retry, by moving the intact fixture
  file out and back without corrupting or editing its contents.

The focused entry point is `CS80Snapshots --preset-click-tests`, registered as
`cs80_preset_click_tests` and included in the full UI smoke harness. Validation
uses a disposable `CS80_FAVOURITES_FILE` override and no display server.

## Qualification

The unchanged production baseline fails **4 of 41 checks**. The same final
harness passes **41 of 41** with the fix. All **11 focused/adjacent CTests** pass
sequentially (7.28 seconds), including search keyboard ordering, undo gestures,
controller reset, voice activity, keyboard handoff and context/typed controls.

Independent review found no blocker, verified all five source/test hashes and
the exact binaries, repeated the **41/4 baseline** and **41/0 candidate**, and
passed all **11 adjacent suites** sequentially (7.18 seconds).

Targeted baseline/final counts, hashes and independent review are recorded in
[the evidence manifest](evidence/preset-click-2026-11-12/manifest.json).

These are direct-component Linux Release checks, not a full-suite result.
Native-peer UI, screenshots, real pointing devices, accessibility, DAW,
macOS/Windows, pluginval, sanitizers and remote CI are not qualified here.
No security, malformed-file or parser regression suites were run for this fix.
