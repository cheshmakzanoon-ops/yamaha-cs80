# Preset preview XML and arithmetic boundaries

Revalidated on 2026-11-09 against main `b542c17` (the screenshot-only child of
`31a6d5b`). This is a narrow port of PR #82, commits `258ee4a` and `58e6070`,
not a merge of its old base or of other branch history. The later trash handling
still has no explicit permanent-delete fallback; native OS recoverability is
not guaranteed. Local production/test checkpoint: `9b7059d`.

## Defects still present on that main

The browser had two independent imported-preset readers: one for thumbnail and
detail signatures, and one for its fallback edited-marker comparison. Both
converted XML values to `float` and passed them through `clampValue`, which
does not reject NaN. A direct `ch1_v_a` child with `value="nan"` could therefore
put NaN into the preview before the user loaded the patch. Infinity and finite
double values overflowing float were clamped to range edges rather than using
a finite fallback. The processor already sanitizes these values on restore.

Both readers also accepted parameter children from unrelated roots. The
display-name reader used JUCE's case-insensitive `hasTagName`, whereas the
processor's ValueTree type check is case-sensitive.

There was a separate arithmetic defect with entirely finite controls. The
shared-axis position can round below `total` while release progress rounds
above one. The fractional power of its negative base produces NaN. One GCC
reproduction uses attack `0x1.11815cp+4`, decay `0x1.2dddbcp+4`, sustain 1 and
release `0x1.abbf1p+2` on both channels.

## Bounded change

- Share one preview/edit-reference reader and accept only exact `CS80_PARAMS`
  roots for values and display metadata.
- Substitute finite defaults before clamping malformed imported values.
  Retain the restored Original SUB fallback of off for a named invalid/missing
  value, rather than its fresh-instance default.
- Keep the last matching direct child authoritative, including a child with a
  missing or non-finite value. Ignore nested and unknown IDs. Reset values and
  the usage mask for each read.
- Preserve existing finite parsing/range clamping. Bound only the release
  power's base at zero; progress in [0, 1] uses the original base and exponent.

This is not a full schema/policy migration, stricter finite-token grammar or
discrete-value normalizer. It does not promise that a preview fully reproduces
the sound after restore. Processor loading, state format, factory parameter
data, audio engines and DSP are unchanged.

## Fresh local evidence

Linux GCC 14.2, C++20, official JUCE 8.0.11:

| Check | Result |
| --- | --- |
| Production XML helper | 4,629 checks passed |
| Production release helper | 65,543 checks passed |
| Display metadata, including wrong-case root | Passed |
| Fresh ASan/UBSan/float-cast-overflow plus library assertions, including rebuilt JUCE core | All three helpers passed |
| Seven focused CTests: the four preview checks plus modern-state integers, MSEG text and parameter-state index | 7/7 passed |
| Exact extracted signature arithmetic | 2,905 cases, 894,740 finite-field checks passed |
| Exact-source native `CS80Snapshots` build, including the changed browser translation unit | Built and linked |
| Adjacent native component/processor CTests: voice activity host, parameter keyboard menus, typed units, Scala session UI and transient controls | 5/5 passed |

The signature test extracts the actual production structure/functions. An
inert shim supplies only JUCE's binary32 pi constant. It covers all 523 factory
programs, a default patch, each parameter's endpoints, 2,000 deterministic
finite random patches and the rounded-release witness. It is registered for
GNU/Clang Unix builds, including the JUCE-free core build. It does not create a
processor, message loop or native window, and it does not render pixels.

Negative controls were rerun against `b542c17`: its reader loops, adapted only
to accept the test's in-memory XML and preserve both callers' default-value
initialization, fail 1,952 of the 4,629 helper checks; its metadata reader fails
the wrong-case case; the former power expression fails three release checks.
The extracted original complete signature math fails two finite-field checks
in the same 2,905-case sweep. The specific finite-preset rounding witness is
compiler-dependent; the helper test also checks portable one-ULP overshoot.

ASan was run with `detect_leaks=0` because LeakSanitizer is unavailable under
this environment's ptrace restrictions. This is not a leak-check pass.
Independent review and fresh helper/sweep reruns found no source blocker.
The native build and adjacent component tests do not exercise preview drawing
in a native window. Helper/arithmetic checks are not GUI or real-host qualification.

## Reproduction

With a configured JUCE-enabled build:

```sh
cmake --build build --target cs80_preset_file_values_tests cs80_preset_display_name_tests cs80_preset_preview_math_tests
ctest --test-dir build -R '^cs80_preset_(file_values|display_name|preview_math|signature_math)_tests$' --output-on-failure
```

The pure math target and extracted-signature check also run with
`CS80_BUILD_PLUGIN=OFF`. For the current-main negative arithmetic control:

```sh
git show b542c17:src/plugin/PresetBrowser.cpp > /tmp/preset-browser-before.cpp
python3 scripts/test-preset-signature-math.py --source /tmp/preset-browser-before.cpp
```

That command is expected to fail on the recorded GCC profile. The same script
without `--source` must pass. A compile error is not a successful negative
control: the recorded failure comes from the executed finite-field checks.

## Historical CI is not current-port CI

Exact upstream head `58e6070` had a successful [PR build 42345637673](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42345637673),
[DSP run 42345633803](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42345633803)
and [screenshots 42345633817](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42345633817).
Its parallel [push build 42345633766](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42345633766)
remains a failure: Linux job `128327819596`, GUI validation seed `0x1cf0381`,
exited 139 after completing Fuzz parameters without printing SUCCESS. All
four native build jobs, Windows validation, Linux headless validation and the
fixed-seed GUI run passed. Two same-seed diagnostic replays also passed, but
there was no ELF core/stack to explain the original failure. Nonreproduction
does not prove infrastructure failure or implicate/exonerate the preset code.

Those runs do not validate this current-main port. No current-port remote CI,
new screenshots, VST3/AU build, native-peer or DAW/release qualification is
claimed here.
