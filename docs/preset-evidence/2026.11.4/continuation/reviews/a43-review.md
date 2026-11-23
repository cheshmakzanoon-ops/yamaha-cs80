# A43 — final source equivalence and program preservation

Reviewed native candidate: `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`.
Previous candidate: `bcc86aa39a33a0162254614e5502705f0113d701`.
Previous library baseline: `fa57967c7317828cf769cc7ca97f5857e566520f` (2026.11.3).

## Result

**No production audio or recipe change occurred between bcc86aa and 078c098.** The 22 changed files comprise 20 documentation/evidence files and the two reviewed UI test fixtures, `src/tools/ui_snapshot.cpp` and `tests/PanelEditorTests.h`. The complete `src/core`, `src/plugin` and `src/reference` Git trees are identical; CMake, scripts, resources and the preset-capture source are also unchanged. This is source equivalence, not a claim that separately compiled native packages are byte-identical or qualified.

| Source object | Identical Git object before and after |
|---|---|
| `src/core` tree | `8725aa905836f2f91dddf940b6d43facfc1c3852` |
| `src/plugin` tree | `e53cf7892eb421976d66890783809de1b9554ddf` |
| `src/reference` tree | `661f13dcd7fe0075fd9411fa481a421f23272eaf` |
| Preset capture source | `f23ee3860f8ec7fac36369132086b2511d411ff6` |

## Independent inventory comparison

I reparsed the retained final inventory and compared its values at IEEE-754 binary32 precision against the 2026.11.3 baseline, independently of the production coverage helper. No audio was rendered.

- All **523 ordered indexes, host indexes, names, categories and tag lists** remain unchanged. No programs were added, removed or moved.
- All **521 musical programs** change existing non-master-volume controls: **7–52** controls per program.
- **Init Patch** (host 43) and **Init Circuit** (host 522) retain every one of their previous **198 values exactly**. Both newly appended sizes are neutral at **1**.
- There are **zero duplicate complete non-volume vectors**.
- The final inventory was exported at `e32728b`. Its complete core/plugin/reference trees and preset-capture source are identical through `078c098`, so the two UI fixture repairs do not invalidate its structural findings. Its source label does not replace final native producer qualification.

Final inventory SHA256: `0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611`.
Baseline inventory SHA256: `6f070ed15a04259fd5a9d5f0eb06a6e9554f821871c3313e9a793a3b28a2b521`.

## Stable core and host layout

All **198 prior core IDs** remain an exact prefix, with `body1_size` and `body2_size` appended at positions **198/199**, giving **200** core parameters.

I independently reconstructed the host insertion order from the source groups and unchanged memory-control selection. All **304 prior host IDs** remain an exact prefix, with the two sizes appended at positions **304/305**, giving **306** host parameters. The reconstructed prefix hashes agree with the frozen historical contracts:

| Prefix | FNV-1a64 over IDs and newlines |
|---|---|
| 198 core IDs | `8e37bad444e187c2` |
| 304 host IDs | `b86982a235459491` |

Removing exactly the appended two-size layout block from current `PluginProcessor.cpp` makes the entire file byte-identical to the 2026.11.3 version. Previous creation order and AU hints are therefore retained; the new sizes use hint **17**. This is a static reconstruction, not a new host execution. The native host suites still must run successfully.

The program-mapping header differs from the baseline only in the library revision string. The only `FactoryPresets.cpp` change defaults appended size when selecting a hardware tone; MIDI addressing functions remain unchanged. Combined with the preserved ordered catalogue, the original MIDI addresses remain stable.

## Evidence boundary

The **all-521 matched signal comparison** belongs to historical `59d489d`, before the tine-normalization and bowed-formant corrections. It must not be relabeled as a final `078c098` matched render. Parameter changes and preserved source trees establish coverage and compatibility structure; they do not establish universal audible improvement, exact acoustic realism, or downloadable binary identity.

No repository edits, builds, tests, audio captures, commits or pushes were performed by this reviewer. The compact machine record is `a43-summary.json`. Final native artifacts and validation jobs remain the parent's separate release gate.
