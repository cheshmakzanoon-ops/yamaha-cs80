# Preset search keyboard ordering

## Reproducer

The search field uses JUCE's `TextEditor::onTextChange`, which is delivered by
a posted command message. The visible text changes first. `GlassField` delivers
Return and result-navigation keys synchronously, so those keys could act on the
old result list before the queued filter update ran.

The regression reproduces that ordering through the real text editor and
browser handlers: start with `Previous` selecting `A Previous`, replace the
query using `insertTextAtCaret`, and deliver Enter before pumping messages.
For `Birch`, Enter must load `B Target Birch`; for an unmatched query it must
leave the entire unsaved sound unchanged. The same ordering matters for
Up/Down/Page keys and AUDITION. This is a deterministic component-level
reproduction, not an OS event-injection or clipboard test.

## Contract

A search Return or unmodified Up/Down/Page key uses the current visible search
text. Pending text is reconciled through the existing filter rebuild before
handling the key. The browser records which exact text produced the current
filter; already-applied queued callbacks do not rebuild it a second time.

Existing selection and recall rules remain intact:

- Filtering preserves a surviving selected identity or selects the first match.
- Filtering itself never auditions; explicit navigation keeps its existing
  audition behavior. A key that does not change the selected row does not load.
- Enter explicitly loads the selected match. With no matches, it does nothing
  to the live sound, even if an old result was selected before the edit.
- A delayed callback cannot reset a keyboard-selected row or audition again.
- Modified navigation still belongs to the text editor. The toolbar's NEXT/PREV,
  file admission, save/overwrite, REVERT and physical/session recall policies
  are unchanged. No DSP or parameter-layout change is involved.

## Validation

`tests/PresetSearchTests.h` uses a disposable three-preset fixture and real
`TextEditor` and `ListBox` components without creating a native peer. It covers
pending and delivered callbacks, Enter, unmatched searches, all four forwarded
navigation keys, and AUDITION on/off. The test is also included in the full UI
smoke suite, whose native-peer qualification remains separate.

```sh
cmake --build build-native --target CS80Snapshots
ctest --test-dir build-native -R '^cs80_preset_search_tests$' --output-on-failure
```

The original production code at `8cfd6e8d`, with the new harness only, fails
**22 of 140 checks**. The identical regression passes **140 checks with zero
failures** after the fix. All **18 focused/adjacent CTests** pass sequentially,
covering successful and failed preset loads, XML admission, storage, identity,
refresh, complete opening REVERT, modern recall, undo gestures, controller reset,
keyboard handoff, context controls and typed-value units.

The fresh Linux Release build uses GCC 14 and the pinned official JUCE 8.0.11
source. No existing cache was retargeted. Its final executable SHA-256 is
`d5859be7ef9917cbb6fe8d4b4f26fffaa6c8cf1382a20780d74b33ebed3822fc`.

Independent source/test review found no blocking issue. The reviewer verified
all five source hashes and the exact final executable, repeated the **22/140**
failing baseline and **140/140** passing candidate, and independently passed
all **18 adjacent CTests** sequentially (13.30 seconds).

[Baseline, final/independent logs and source/binary hashes](evidence/preset-search-2026-11-12/manifest.json).

No native-peer GUI, screen-reader, real clipboard, DAW, sanitizer,
macOS/Windows, pluginval or remote CI qualification is claimed by this change.
