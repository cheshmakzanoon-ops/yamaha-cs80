# Preset browser XML admission

## Gap and bounded fix

User-file LOAD already used the strict snapshot admission boundary, but the
browser's `signatureForUserFile` and `readUserFileValues` still passed a file
directly to JUCE's permissive XML parser. Catalogue refresh could therefore
accept a truncated root, mismatched closing tag or trailing content for its
name/category/preview, even though LOAD rejected the same document. The dirty
reference reader could likewise use values from an inadmissible document.

The file-based JUCE parser also supplies a `FileInputSource`. JUCE 8.0.11's
`expandExternalEntity` and `getParameterEntity` can read an external SYSTEM
DTD or parameter entity through that source. The regression uses only a
fixture-owned DTD containing a distinctive name and numeric value, referenced
by its relative filename. It does not inspect unrelated files, use ptrace, or
claim a comprehensive filesystem-access trace.

Both browser helpers now call the existing `presetXml::readCompleteDocument`.
Each read validates one bounded byte snapshot and passes the same bytes to
JUCE without a file input source. There is no new parser, dependency, XML
policy, cache policy or change to CMake. The byte, depth and element bounds,
encoding support, and entity restrictions are those documented in
[Complete user-preset XML admission](PRESET_XML_ADMISSION.md).

Invalid files stay in the catalogue. Their filename supplies the display name
and instrument classification, and default parameter values supply their
signature/styles. LOAD and AUDITION keep the existing explicit error/retry
path. A rejected dirty-reference read has default values and an empty used
mask; the existing in-memory baseline still detects actual live edits. Valid
files retain metadata, category, style, sparse-parameter and dirty-reference
behavior. Refresh after a changed file timestamp invalidates the existing
reference cache, and repairing the file allows ordinary loading again.

No fader, DSP, parameter ordering, host/session state, factory bank or loading
policy changes are part of this follow-up. This does not add filesystem
watching or change the existing timestamp-based refresh/cache semantics.

## Verification

`tests/PresetBrowserXmlTests.h` runs from the existing
`cs80_preset_refresh_tests` CTest and full UI smoke harness. It constructs the
real browser and processor with a disposable preset folder and covers:

- Truncated attributes, mismatched closing tags, extra roots, trailing junk and
  unfinished comments during actual catalogue refresh.
- Fixture-local external SYSTEM DTD and external parameter-entity references,
  with audition both off and on.
- Visible filename/default-preview rows, unchanged live state/provenance,
  zero success callbacks, and the existing selected-row LOAD error.
- Valid built-in/numeric entities, declarations/comments/PI, bare DOCTYPE,
  UTF-8 BOM, UTF-16 LE/BE BOM and non-ASCII/astral Unicode metadata.
- Valid dirty references, files replaced after load, default/empty rejected
  references, genuine live edits, repaired references and a successful retry.

The build uses an isolated Linux Release cache, GCC 14.2, and the exact pinned
JUCE 8.0.11 and libxml2 2.15.4 source overrides with FetchContent disconnected.
Evidence, source/binary hashes and independent review are retained under
`experiments/preset-browser-xml-admission/`. This is focused Linux verification,
not native macOS/Windows, native-peer UI, DAW/pluginval or sanitizer qualification.

On unchanged `321eb705` with the same final harness, **20/193** refresh checks
fail: 14 inadmissible preview/name cases and six inadmissible dirty-reference
cases. Both external resource routes visibly produce
`Flute_CS80_OWNED_SENTINEL`, preview styles `38` instead of fallback `6`, and a
dirty brilliance reference of `1` instead of the loaded `0.375`. The fix passes
**193/193** checks, with filename `Admission`, fallback styles `6`, and zero
used dirty-reference values for every rejected document. All **13/13** focused
CTests pass, including **639/639** LOAD failure/retry, **77/77** XML admission,
**2,784/2,784** file recall, **2,354/2,354** modern/session/A-B recall,
**1,387/1,387** REVERT and **4,629/4,629** preview-value checks.

The retained initial harness log is not the product baseline: its single-
millisecond timestamp increments were truncated by JUCE's Linux setter and
failed to invalidate the dirty cache, and its DTD tokens did not cleanly expose
both sentinel routes. The final harness advances timestamps by two seconds,
verifies the written timestamp and uses a whitespace-delimited DTD with a
single-token unique label. The final baseline patch and hashes identify the
exact corrected regression that was run before and after the production fix.

Independent review verified the nine source/dependency-file hashes and final
binary hash, repeated the **193/193** refresh checks with explicit exit 0, and
approved the narrow change without blockers.
