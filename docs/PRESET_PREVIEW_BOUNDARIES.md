# Preset preview XML boundaries

## Defect

The browser has two readers for imported user presets: one supplies thumbnail
and detail signatures, and one supplies the fallback edited-marker comparison.
At baseline `185e04c`, both converted XML values to `float` and passed them
through `clampValue`. That clamp does not reject NaN.

For example, a direct child of a `CS80_PARAMS` document containing
`id="ch1_v_a" value="nan"` yielded a NaN attack in the preview. Its envelope
calculations could then produce NaN geometry. The processor's state restore
instead substitutes the finite attack default, 0.008 seconds. Infinity and
values overflowing `float` also differed: the browser clamped them to an edge
while restore substituted a fallback. This defect occurs in preview parsing;
loading the malformed patch is not needed to reach the preview calculations.

Both readers also accepted values from a document with the wrong root, although
the processor would reject the document. Display-name acceptance used JUCE's
case-insensitive `hasTagName`, unlike the case-sensitive ValueTree type check.

## Change

- Share `readPresetFileValues` between both browser readers.
- Accept only the exact `CS80_PARAMS` root, for values and display metadata.
- Apply a finite fallback before clamping imported parameter values. Retain the
  restored Original SUB fallback of off rather than its fresh-instance default.
- Keep the last direct matching parameter authoritative, including a final
  child with a missing or non-finite value. Ignore nested and unknown IDs.
- Preserve existing parsing/clamping for finite values and reset the output
  values/usage mask on each read.

This is a browser preview boundary, not a full schema migration or a new
numeric-token grammar. Discrete rounding, engine-policy migration, sound
restoration, factory presets and DSP are unchanged.

## Verification

Local GCC 14.2 / C++20 verification on 2026-11-07 used pinned JUCE 8.0.11
(`c352e2489000ffeb9e397051377d75eef2efe641`):

- Production helper tests: **4,629 checks, zero failures**, both optimized and
  UBSan plus float-cast-overflow builds. The UBSan build also freshly compiled
  JUCE core with the same instrumentation.
- The same test suite against the baseline browser reader's extracted loops:
  **1,952 failures**, confirming that the new assertions detect the defect.
- Display-name tests passed, including the added wrong-case root regression.
- `PresetBrowser.cpp` passed a syntax-only compilation with the final helper.
- `git diff --check` passed.

Coverage includes all declared parameters, NaN/infinity/float overflow,
ordinary finite values at and beyond range edges, XML round trips, both reader
outputs, repeated calls, rejected roots, duplicate IDs, absent values, nested
values and Original SUB's compatibility fallback. The tests create no
processor, APVTS, message loop or GUI and perform no file I/O.

An ASan/UBSan attempt could not complete because LeakSanitizer was unavailable
in the execution environment. It is not counted as a pass. Full plug-in builds,
actual signature rendering and processor restore integration were not run
locally; syntax-only and helper tests do not replace those checks.

The regression targets are `cs80_preset_file_values_tests` and
`cs80_preset_display_name_tests` in a normal JUCE-enabled CMake build.

## Finite release-preview endpoint

A deeper numerical check found a separate NaN path even with entirely finite
parameter values. At the last envelope point, separately rounded shared-axis
and release calculations can make `x < total` while release progress is slightly
greater than one. Raising the resulting negative base to the fractional power
1.7 produces NaN. One reproducible finite preset has attack
`0x1.11815cp+4`, decay `0x1.2dddbcp+4`, sustain 1 and release `0x1.abbf1p+2`.

`presetPreviewReleaseShape` now bounds the power's base at zero. Progress in
the valid [0, 1] interval uses the same base and exponent as before; endpoint
overshoot terminates at zero. This affects only the library's drawn envelope,
not either audio engine or the saved preset values.

Additional GCC 14.2 verification:

- Pure production-helper test: **65,543 checks, zero failures** in optimized
  and UBSan plus float-cast-overflow builds. The former unbounded expression
  fails three of those checks. The standalone CMake/CTest target also passes.
- Exact production signature arithmetic, extracted without its unused GUI
  declarations, was exercised with each malformed token in each parameter,
  complete malformed documents, 2,000 deterministic finite random presets and
  rejected documents: **4,296 cases, 1,323,168 finite checks, zero failures**.
  The unguarded production math has one NaN in that same sweep.
- Independent source review and the final browser translation-unit syntax
  check pass. No processor, message loop or GUI execution is involved.

The new `cs80_preset_preview_math_tests` target is JUCE-free and runs in both
core-only and plugin-enabled builds. The extracted arithmetic experiment is
not a rendering or real-host qualification. Other compilers may round the
specific preset boundary differently; the portable regression explicitly
tests one-ULP progress overshoot as well as the finite preset.
