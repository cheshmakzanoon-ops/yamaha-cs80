# A41 — restore the existing panel-layout fixture

## Change

Edited **only `tests/PanelEditorTests.h`** in the repository. The panel golden fixture now pins the 15 visible controls changed by the 2026.11.4 revoicing to their preceding values. The already established chorus overrides remain unchanged. No factory data, audio code, UI layout, golden PNG, comparison tolerance or production behavior changed.

Source before the test edit: `bcc86aa39a33a0162254614e5502705f0113d701`.
Reference source: `fa57967c7317828cf769cc7ca97f5857e566520f`.

## Cause and evidence

The fixture loads factory program 0, **Tone I: String 1**, then previously pinned only its chorus speed/depth and two policy controls. The new recipe changes additional visible panel controls. Consequently this layout comparison was also comparing the new sound recipe against the older recipe captured in the images.

The two current failure images were visually inspected at 1600×1000 and 1120×700. Their differences occur at slider positions and displayed numerical values, not the panel layout. The larger candidate and original image were also compared directly. CI reported changed-pixel fractions of 0.2710% and 0.3247%, with the local block differences exceeding the unchanged limit. No acceptance limit was raised.

The committed baseline inventory and its provenance identify the preceding factory values:

- `docs/preset-evidence/2026.11.4/baseline/inventory.json`
- `docs/preset-evidence/2026.11.4/baseline/provenance.json`

The existing images were last modified by `b5fc4e7a`; the prior `a81851bc` test correction already established the explicit chorus overrides to retain those images. The added overrides preserve the remaining preceding visible settings.

## Frozen visible settings added

| Parameter | Fixture value |
|---|---:|
| MIX | 0.42 |
| Rank I HP cutoff | 80 Hz |
| Rank I HP resonance | 0.02 |
| Rank I LP cutoff | 3630 Hz |
| Rank I LP resonance | 0.09 |
| Rank I filter decay | 1.48399985 s |
| Rank I amplitude decay | 0.953999937 s |
| Rank II LP cutoff | 5720 Hz |
| Rank II LP resonance | 0.06 |
| Rank II filter decay | 1.90799987 s |
| Rank II amplitude decay | 1.27199996 s |
| Keyboard brilliance low | 0 |
| Keyboard brilliance high | 0 |
| Keyboard level low | 0 |
| Keyboard level high | 0 |

The decay literals retain the actual prior binary32 results of the authored recipe and finish calculations. A direct binary32 comparison confirmed each of these 15 literals equals the corresponding baseline value exactly. The existing chorus overrides remain 0.28 Hz and 0.36 depth.

After applying these overrides to the exported current vector and applying the existing chorus overrides to both fixtures, **all shared non-body parameters equal the previous fixture**. The only six remaining changed shared parameters are the two ranks' body mix, brightness and decay. Those controls have no widgets on this main panel, and the fixture renders no audio. The two appended body-size parameters also have no main-panel widgets.

`ProgramLibraryHostTests.h` continues to compare actual factory values for startup and program recall; the visual fixture does not replace those sound-data checks.

## Verification and remaining gate

- `git diff --check -- tests/PanelEditorTests.h` passed.
- Both golden images remain byte-identical to `bcc86aa`.
- Exact value comparisons and image hashes are retained in `a41-fixture-evidence.json`.
- No new screenshot was rendered by this worker. The parent must require the unchanged comparison thresholds to pass when CI renders the corrected fixture.

Unchanged image hashes:

| Image | SHA256 |
|---|---|
| `tests/golden/main-1600x1000.png` | `13b6dd07937dd15170e8855264a52b27149329934e7e4dd12efa5206d2c5cafc` |
| `tests/golden/main-1120x700.png` | `a4133c5833fecca05c2cf6e1b3ca571a3dfb178405d3c37cc9bc2baf9656ce77` |

No commits or pushes were performed by this worker.
