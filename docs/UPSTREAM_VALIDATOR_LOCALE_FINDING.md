# First official beta comparison: Linux locale finding

## Recorded result, 2026-10-30

[Official beta3 comparison run 40982697406](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40982697406)
used workflow commit `b421c2746efa21166c6b9114c810dfe127ed938d` and Linux VST3
artifact `12944759355` from [source build 40981514188](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40981514188),
source `22d6a32c48c85f9b813506edcd856f73f4293c2d`. The source build's original
stable gate succeeded; this comparison does not replace that independent result.

| Independent lane | Result |
| --- | --- |
| [Headless subset, job 123826294529](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40982697406/job/123826294529) | Seed `0x4f31605`; SUCCESS, exit zero, complete JSON report with 25 passed cases |
| [GUI-inclusive, job 123826294226](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40982697406/job/123826294226) | Seed `0x63421ca`; one locale-stability assertion failed; FAILURE and process exit 1 |

Neither lane recorded exit139. One invocation without that exit does not prove
the earlier intermittent crash repaired. The GUI comparison remains **failed**,
not waived because the stable gate or headless subset passed.

## Direction and limits of the observation

The GUI log explicitly records startup locale `C` and shutdown locale `C.UTF-8`.
Its assertion labels the expected value `C.UTF-8` and actual value `C`, which can
misleadingly suggest the reverse transition.

The [exact beta3 LocaleTest](https://github.com/Tracktion/pluginval/blob/37d6a37bc752606c524f7f3febffc73e0945a740/source/tests/LocaleTest.cpp)
passes `startupLocale` first and `newLocale` second to `expectEquals`. JUCE's API
takes actual first, expected second, so those assertion labels do not describe
temporal direction. The recorded change is **C to C.UTF-8**.

The initial value is captured in a static test object's constructor, not
immediately before opening the plugin editor. Later the test opens/closes the
editor and compares the process-wide locale with that earlier baseline. The
failure therefore does not alone identify the first changing call or prove it
happened within this particular editor-open interval. This GUI-required test is
intentionally omitted by the headless subset, explaining why headless success is
not contradictory locale evidence.

## Source-supported dependency defect

The [tested project's CMake pin](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/22d6a32c48c85f9b813506edcd856f73f4293c2d/CMakeLists.txt#L196-L197)
was JUCE **8.0.6**, SHA `51a8a6d7aeae7326956d747737ccf1575e61e209`. Read-only
inspection found no `setlocale` or `std::locale::global` call in the project's
production `src` tree. That does not exclude dependency calls.

1. The pinned [Linux SystemStats helper](https://github.com/juce-framework/JUCE/blob/51a8a6d7aeae7326956d747737ccf1575e61e209/modules/juce_core/native/juce_SystemStats_linux.cpp#L48-L54)
   switches the global locale to the environment-selected locale while obtaining
   the value it later treats as the old locale. It does not restore the pre-call
   value when the two differ.
2. `getUserLanguage` and `getUserRegion` call that helper; `getDisplayLanguage`
   calls those functions.
3. JUCE [text-shaping options](https://github.com/juce-framework/JUCE/blob/51a8a6d7aeae7326956d747737ccf1575e61e209/modules/juce_graphics/fonts/juce_SimpleShapedText.cpp#L149)
   initialize language with `getDisplayLanguage`. The plugin uses JUCE text
   rendering: a plausible GUI path, not a captured call stack from this run.
4. Upstream fixed the incorrect restoration in
   [`2da6a5fb621ff8fd6c606c3fda0ce7c5349f5b13`](https://github.com/juce-framework/JUCE/commit/2da6a5fb621ff8fd6c606c3fda0ce7c5349f5b13)
   on 2025-10-23, snapshotting the current locale before switching and restoring
   that snapshot afterward.

Official 8.0.10 still has the old helper; **8.0.11**, 8.0.13, 9.0.3 and 9.0.4
contain the correction. Thus beta3's own JUCE 9.0.3 source pin is corrected while
the tested plugin's 8.0.6 is not. The narrower official **8.0.11** update and
[fresh-process regression](LOCALE_PRESERVATION_REGRESSION.md) address this finding.

The correction still temporarily changes global locale during the query. It is
not a per-thread locale implementation or proof that concurrent queries are
race-free. The source evidence and ordinary getter negative control are consistent
with the observed transition, but do not identify the exact first call in the
earlier GUI run, establish fontconfig as the cause, implicate recent DSP changes,
or connect this assertion to the unrelated historical exit139.

## Evidence handling

Do not normalize production `LC_ALL`, disable this test, mutate the validator,
wrap the comparison in a locale reset, or retry until green. Preserve the failed
beta comparison and successful stable build separately.

This document was reconstructed after an execution-workspace replacement from
retained read-only GitHub/source findings. The linked Actions records are the
primary runtime evidence. The old uncommitted local files and binaries did not
survive; recovered local negative-control evidence is explicitly labelled in the
regression document, not presented as a new run.
