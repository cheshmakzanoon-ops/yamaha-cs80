# Factory-tone revision processor results (2026-11-02)

## Baseline, before implementation

- Production/API base: `2a8c726e4a055b7733dd8e314f334de4ac042c87`.
- Test-only registration: `6c49ac29ae46feecd7b943e0bf131524ef1e5ecf`.
- Fixture public-interface correction: `f9cb22d`; the first compile called a
  private JUCE AudioParameterChoice override. The corrected test uses its public
  AudioProcessorParameter base. This compile error is preserved, not called RED.
- Actual focused run: 2026-11-02 13:52:11 UTC, exit **1**, **1,491 checks and
  exactly 3 ordinary assertion failures**. No runtime crash or display attempt.
- All 297 old parameter-prefix rows passed. The missing 298th choice fails the
  count and presence checks. Revision-dependent checks explicitly did **not** run.
- Independent existing-defect witness: Program Change at sample 10 leaves main
  selector **13** / voice_mode **0**, but the still-active extension retains
  selector **0** / voice_mode **1**. The same-aggregate-frame assertion fails.

All 25 production/core project translation units were freshly compiled against
the integrated core ABI. Only the test TU was rebuilt after the test-only public
interface correction; the whole src tree stayed identical between those two
test commits. The pinned JUCE 8.0.11 and generated font objects are unchanged
reused dependencies, with command and object hashes in the receipt. No old
Engine/Processor object was linked into the new-ABI fixture.

Evidence: `docs/evidence/factory-tone-host-20261102/`. This is not native UI,
pluginval, DAW, sanitizer or full composed-source qualification. Implementation
and post-change results remain pending.

## Additional Program Change controls, still before implementation

Test-only 74556f7 ran at 13:59:05 UTC: **1,505 checks, 4 ordinary failures,
exit 1**. The original three-failure baseline above is retained separately.
The extra failure is the same-sample ninth-note extension frame: it still sees
the old source rather than the newly recalled aggregate program. Positive
controls pass: eight held main assignment identities survive, the equal-sample
ninth note reaches the extension, ordinary eight-voice main recall retains its
aggregate coordinate, and an inactive extension stays untouched. Revision-
dependent checks again explicitly did not run because the choice is absent.

## First implementation run and mutation-discovered witness gap

Production 9ba7890543058a1de160b4181cbc413c9d2f5eac (src2425af2dc94923e01193e864f110520a65d2ad5e)
passed 2,255 checks at 14:04:18 UTC. This is a C-locale processor result, not
complete persistence qualification: a separate actual locale probe later fails.

The first actual live-frame-reread mutant exited1 with one supplemental
concurrency failure, but no intended deterministic automation failure. The
qualification script correctly marked this INVALID, not a successful negative.
readParameters had already evaluated the attached sample0 curve without
publication, so the later publisher saw no change. The fixture correction takes
the snapshot before queue attachment and asserts both a distinct initial volume
and the actual changed volume in both engines. No production source or test
criterion is relaxed. The corrected fixture has 2,257 checks.

## Normalization negative-control scope correction

The corrected 2,257-check positive passes. Actual live-reread, missing-extension
and missing-automation mutants now fail their intended deterministic assertions.
The next mutant deletes the entire revision ensure() call: it fails four
malformed-state assertions, but not the originally predicted missing-default
assertion. Pinned JUCE8.0.11 appends an absent PARAM and initializes its declared
default0. That run remains INVALID for its original registration.

The prospective replacement separates the mechanisms: omitted normalization
must fail the malformed opt-in assertions, while a distinct actual C++ mutant
changes the missing-value fallback to the live revision atomic and must fail
the unchanged missing-state V1 assertions. The fixed positive inventory and
all missing/malformed checks are retained; no extra failure is treated as a
substitute for the specifically registered mechanism.

## Qualified local processor scope at src740854f1

The corrected **2,257-check** processor fixture passed before and after all
**11 actual compiled C++ mutants**. Each mutant failed its registered ordinary
assertion, with no compilation/runtime-error substitute. Failure counts were
12,19,12,4,5,8,2,3,1,2,3. Both earlier INVALID registration attempts are preserved;
they are not counted as successful negatives.

Existing host automation-boundary regression passed (maximum normalized error
2.2204460492503131e-16). The existing host circuit-contract mode passed too, but
its OutputMeterHost fixture constructs an offscreen editor and emitted Fontconfig
cache warnings. It created no native peer/window or screenshot; classify this
as limited existing regression coverage, not processor-only or native GUI proof.

The factory-tone locale repair and its separate results are recorded in
FACTORY_TONE_LOCALE_RESULTS_20261102.md. Existing ENGINE/PURE comma-locale state
corruption was independently reproduced and remains a separately scoped blocker.
Native wrappers/pluginval/GUI/DAW, full-source numerical qualification and the
explicit user-facing editor affordance remain pending. The original numerical
source locks are unchanged.

## Independent bounded replay

Agent5 independently approved the local Linux processor/new-choice-locale scope
at845696f/src740854f1. They verified source substitutions and all linked project,
JUCE/font object and executable hashes, then replayed2257+296 positives before
and after all11 existing compiled mutants, the296/13 plain-strtod mutant and
14/14 simulated null-locale control. This was independent replay of qualified
binaries, not an independent recompile or native/GUI/DAW/full-audit acceptance.

The failure counts above are OBSERVED counts, not promised fixed totals. The
live-frame-reread mutant deterministically fails six main and six extension
cached-revision assertions. The single explicitly supplemental concurrent-write
coherence assertion can additionally fail depending on scheduling. The archived
missing-extension run likewise includes that one stress failure. Independent
replay requires exact deterministic failure-family multisets and permits only
that specifically named supplementary variation; candidate positives still
require zero failures. Their first overly exact total-count checker stopped on
13 instead of12, and is preserved rather than called a candidate failure or a
missed mutant. The corrected independent replay passes; both logs are archived.
