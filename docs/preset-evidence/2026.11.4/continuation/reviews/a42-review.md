# A42 — independent UI fixture correction review

Reviewed working-tree test changes relative to `bcc86aa39a33a0162254614e5502705f0113d701`. This review made no repository edits, ran no full builds and did not rerender the editor. It independently inspected source, retained CI images and binary32 fixture values.

## Decision

No source-review blocker to publishing the two test-only corrections for a fresh native and screenshot run. The original failed run remains failed. Neither this review nor the numerical explanation constitutes a passing native UI test.

## Factory recall expectation

`Cs80AudioProcessor::applyValues` writes each recalled physical value through `convertTo0to1` and `setValueNotifyingHost`; `getParamValue` reads the resulting APVTS physical value. The bipolar keyboard contour is a floating parameter with range `[-1, 1]`. The current preset at host index 5 authors approximately `0.0250000003725`; the normalized representation returns approximately `0.0249999761581`. Exact equality with the original authored binary32 is therefore the wrong contract for this recall assertion.

The correction in `src/tools/ui_snapshot.cpp` computes the expected physical value with the parameter's explicit normalization/denormalization round trip. It retains exact equality, requires the parameter to exist, and preserves the exact sustain/glide assigner comparisons. The test still first writes the opposed live contour `0.5`, so doing nothing cannot satisfy the revised assertion. The preceding assertions separately establish the bipolar range/default and preservation of jack/pedal/selector/output wiring. No production recall code or tolerance changed.

I inspected A40's pinned-JUCE probe and its supplied numerical result; this review did not rerun that probe or claim a complete host execution. The new native diagnostic prints authored, canonical and restored values for the actual host run.

## Panel golden fixture

I inspected both retained failed diff images, the 1120×700 candidate, and both existing goldens. The changed regions are control slider positions and their numeric labels. There is no visible layout shift, missing control, changed frame or typography issue in the reviewed images. The changes correspond to revised factory values for the two filter ranks, decay labels, MIX and four keyboard contours.

I independently loaded the retained 2026.11.3 baseline inventory and final continuation inventory, applied A41's fifteen new pins in binary32, and compared the complete existing parameter vectors. All fifteen pins equal the baseline float bits. After preserving the earlier chorus fixture overrides, the only remaining differences among existing controls are:

- `body1_mix`, `body1_brightness`, `body1_decay`
- `body2_mix`, `body2_brightness`, `body2_decay`

The two new size controls are also present. These body controls have no dedicated widgets in the main view; the fixture renders no audio. Thus they do not explain a remaining main-panel visual difference. Actual factory sound/parameter loading remains covered by `ProgramLibraryHostTests`; this layout fixture is not substituted for those checks.

The comparison algorithm and all numerical limits remain unchanged: channel tolerance 40; changed-pixel fraction 0.003; 8×8 block mean limit 8.0. Linux remains strict. No image was regenerated or replaced. Independent hashes of the working-tree PNGs equal their committed versions:

| Golden | SHA256 |
|---|---|
| `tests/golden/main-1600x1000.png` | `13b6dd07937dd15170e8855264a52b27149329934e7e4dd12efa5206d2c5cafc` |
| `tests/golden/main-1120x700.png` | `a4133c5833fecca05c2cf6e1b3ca571a3dfb178405d3c37cc9bc2baf9656ce77` |

## Required follow-through

Run the actual corrected screenshot/host tests and preserve their new source identity and original exit result. The failed `bcc86aa` screenshot job is useful diagnostic evidence, not an accepted run. These fixture changes do not alter the preset vectors, DSP, plugin layout, goldens or production recall behavior.
