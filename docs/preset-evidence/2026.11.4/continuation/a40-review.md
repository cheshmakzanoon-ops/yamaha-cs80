# A40 — factory recall assertion correction

## Diagnosis

The failed screenshot workflow assertion compared an authored binary32 value directly with the value stored after JUCE host normalisation. Program 5, **Tone II: Brass 2**, now authors `kbd_level_high = 0.025f` on a linear bipolar `[-1, 1]` range. `applyValues` calls `convertTo0to1` before publishing the parameter; `AudioParameterFloat` and APVTS denormalise that host representation.

The actual pinned JUCE `NormalisableRange<float>` implementation and current source-built factory recipe produce:

| Stage | Value | Binary32 bits |
|---|---:|---|
| Authored | 0.0250000003725 | `3ccccccd` |
| Normalised | 0.512499988079 | — |
| Host canonical / APVTS | 0.0249999761581 | `3cccccc0` |

The absolute discrepancy is **2.421438694e-08**. The second normalise/denormalise step used by APVTS is stable. Thus the original raw equality fails even when the authored value is recalled correctly. The separate physical wiring preservation assertion passed in the failing CI log.

## Change

Only `src/tools/ui_snapshot.cpp` changed. The expected contour value now comes from the parameter's explicit normalise/denormalise conversion. The assertion still requires **exact equality**, a valid parameter, and exact sustain/glide assigner equality. No tolerance was introduced. A diagnostic prints authored, canonical and restored values. The deliberately opposed live value `0.5f` still fails the comparison, so ignoring the factory recall cannot pass.

No production DSP, factory recipe, parameter range, golden image, or test acceptance limit changed.

## Verification and limits

- A standalone probe using the real JUCE range header at pinned commit `c352e2489000ffeb9e397051377d75eef2efe641`, linked against the current `build-core/libcs80_core.a`, passed with exit 0.
- It reproduces the original equality failure, exact canonical equality success and rejection of the opposed live value.
- `git diff --check -- src/tools/ui_snapshot.cpp` passed.
- The probe uses the actual range implementation and reproduces the documented AudioParameterFloat/APVTS conversion sequence. It does **not** instantiate a native host or execute the full UI smoke test. The final native/UI rerun remains required.

Evidence: `a40/recall-roundtrip.cpp`, `a40/recall-roundtrip.log`, `a40/summary.json` and `a40/ui-snapshot.patch` in the task scratch directory. The source report is based on `bcc86aa39a33a0162254614e5502705f0113d701`; the final changed-test commit is assigned by the parent.
