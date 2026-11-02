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
