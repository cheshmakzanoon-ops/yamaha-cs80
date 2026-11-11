# Complete user-preset XML admission

## Failure and scope

The pinned JUCE 8.0.11 parser returns a root for `<CS80_PARAMS` without setting
an error. It also does not compare closing-tag names or require the input to
end after the first root. A non-null tree therefore is not evidence of a
complete XML document. In user-file recall these inputs could replace the live
sound with normalized defaults and announce a successful LOAD or AUDITION.
Checking `getLastParseError()` alone would not close this gap.

User-file recall now reads one bounded byte snapshot, validates that entire
snapshot with a strict XML parser, and only then gives the same bytes to JUCE.
The existing ValueTree conversion, root-type check, normalization, metadata
publication, and `juce::Result` messages are unchanged. No second file read can
race validation. JUCE receives a string without a file input source, so it
cannot resolve a relative DTD or entity against the user's filesystem.

A rejected document reaches no live-state mutation. The already-published
browser error/retry path keeps the selected row, old sound and provenance,
shows `Not loaded:`, and emits no success callback. Repairing the same file and
retrying succeeds without rebuilding the catalogue.

This change is limited to `loadUserPreset`. Session restore, browser catalogue
metadata/preview parsing, factory sounds, the preset schema, parameter order,
DSP and controls are not changed. A malformed file may still appear in the
catalogue; its LOAD/AUDITION is rejected. Existing missing/duplicate/non-finite
parameter normalization and legacy migrations remain permissive *within a
complete admitted document*. PURE, oversampling, output jack and tuning source
retain their live-session recall policy.

## Admission and security policy

Supported encodings are UTF-8, with or without a BOM, and UTF-16 with a
little- or big-endian BOM. A declaration, if present, must name UTF-8 for UTF-8
bytes, or UTF-16/the matching UTF-16LE or UTF-16BE for BOM UTF-16 bytes. Invalid
UTF-8 is rejected rather than invoking JUCE's Windows-1252 fallback. Conflicting
and non-UTF declarations, embedded NUL, invalid surrogates, odd UTF-16 byte
counts and hidden trailing bytes are rejected. These are intentional admission
restrictions, not changes to the preset schema.

Complete comments, processing instructions, XML declarations, whitespace,
CDATA, quoted angle brackets, and built-in/numeric entities remain accepted.
A bare DOCTYPE and internal non-entity declarations remain accepted. DTD entity
declarations, including unused/recursive and parameter entities, and external
DOCTYPE identifiers are rejected before expansion. Internal ATTLIST defaults
do not gain authority: validation's temporary tree is discarded, and the
unchanged JUCE recall tree still comes from the original snapshot.

The parser has a per-context deny-all resource loader, NONET and NO_XXE.
Recovery, entity substitution, external DTD loading, DTD defaults/validation,
and HUGE are not enabled. No process-global error/entity hooks or parser cleanup
are installed, which matters when the plugin shares a host with other software.

The file read is capped at 16 MiB plus one rejection-detection byte. Admission
allows at most 65,536 elements and retains libxml2's default depth limit (256),
name and text limits. All allocation/parsing occurs on the user-file load path,
not the audio callback. These bound input/tree resources; they are not a
wall-clock or universal security guarantee.

## Dependency and builds

The plugin-only dependency is official GNOME libxml2 2.15.4, pinned to
`96498992efa48d52b0e8b83058bd88dbdaf153c1`. The maintained full XML parser avoids
creating a second handwritten XML grammar. Expat was evaluated but not
integrated: its official 2.10.0 release still flags unresolved security reports,
including [CVE-2025-66382](https://github.com/libexpat/libexpat/issues/1076),
whose published example has a long processing stall.

`cmake/PresetXml.cmake` builds a static, hidden-visibility libxml2 with optional
catalogues, network compatibility, iconv/ICU, modules, HTML, XPath, schemas,
compression, output, tools and upstream tests disabled. Thread support remains
enabled. There is no new shared-library runtime requirement. Core-only builds
with `CS80_BUILD_PLUGIN=OFF` do not configure or fetch it.

For offline builds, provide the exact official checkout using
`-DFETCHCONTENT_SOURCE_DIR_CS80_LIBXML2=/path/to/libxml2`. JUCE has its separate
existing source override. The dependency is designed for the existing Linux,
macOS and Windows CMake builds; local Linux verification does not establish a
native macOS/Windows build pass.

The upstream Copyright, supplementary dict.c/list.c notices and pinned source
identity live in `src/vendor/libxml2/` and are included by current CI archives
and Linux/macOS/Windows installers. The historical Debian qualifier remains
pinned to its old artifact/packager contract and was not repurposed here.

## Tests and retained evidence

`PresetLoadTests.h` drives actual processor/browser LOAD, AUDITION, Return and
double-click handlers against a stale row. Its malformed corpus includes
truncated tags, mismatched/nested closures, multiple roots, trailing junk or
unfinished comments/PI, malformed attributes/entities, wrong roots and missing
files. Assertions cover full live state and metadata, callback count, failure
status/path, selection/provenance, repeated failures and repaired-file retry.
Decorated valid documents and complete empty roots retain successful recall.

`PresetXmlTests.h` adds real processor tests for raw encoding, DTD/XXE, size,
depth and element-count limits. A local sentinel has recognizable data, and rejected references cannot
change the live sound or metadata. A syscall trace was attempted but blocked by
the environment's ptrace restrictions; no syscall-level no-read claim is made.
On POSIX, unique owned FIFO sentinels without writers additionally verify that
the tested external DTD/entity paths do not perform a blocking read-open. The
enclosing CTest timeout bounds a regression; this is a behavioral control, not
a comprehensive filesystem-access trace. UTF-8/UTF-16 (including astral Unicode), ordinary DTD declarations,
CDATA, entities and quoted delimiters have positive tests.

The baseline at `1726b935`, with only the expanded load tests, fails **180 of
639 checks**. The valid corpus passes. The initial harness had nine additional
false failures because exact whole-tree comparison observed a few ULPs of
pre-existing default-parameter round-trip rounding. Those expectations were
corrected to successful recall/name/value checks; full unchanged legacy and
normalization suites remain separate gates. Both logs and the final baseline
patch are retained under `experiments/preset-xml-admission/`.

Candidate testing also caught a big-endian UTF-16 decoder-selection error.
The final validator chooses UTF-16LE or UTF-16BE explicitly from the BOM; all
matching declaration and byte-order positives pass. A separate stale-harness
CLI attempt and Unicode-fixture construction mistake are retained as test/build
setup diagnostics, not attributed to the final product.

Final verification counts, source/binary hashes, dependency provenance,
packaging test results and independent review are recorded in the adjacent
manifest and logs. No native-peer UI, sanitizer, DAW/pluginval, remote CI or
native macOS/Windows qualification is implied by the focused Linux tests.

The final Linux run passes **10/10 CTests**, including **77/77 XML admission**,
**639/639 browser failure/retry**, **79/79 identity**, **2,784/2,784 existing
file/recall**, **2,354/2,354 modern/session/A-B**, and **1,387/1,387 REVERT**
checks. The independent review matched the final source and binary hashes and
reproduced all ten CTests with exit 0. Core-only configure/build and fully
disconnected plugin configure also pass. Packaging fixtures pass **38 tests**
with **two native-Windows skips**; required notices were checked against the 19
compiled dependency sources and 45 included upstream files.

The final snapshot SHA-256 is
`aa179ebfc60576bf210112a47fcd7333055ff17f36604cc1b83d181639aa30a7`.
All 733 defined global XML symbols in the static archive are hidden; the
snapshot has neither an exported XML symbol nor a dynamic libxml dependency.
