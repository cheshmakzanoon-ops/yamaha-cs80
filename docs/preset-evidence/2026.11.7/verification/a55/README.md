# A55 retained-evidence coverage matrix

This directory joins existing evidence. It creates no audio, invokes no product
binary or tests, and makes no human-listening, native-artifact or release decision.

## Files

| File | Contents |
| --- | --- |
| `program-coverage.csv` | All 523 stable addresses with exact names/categories, zero-based host and MIDI addresses, default scene, change flags, mechanism eligibility, targeted-review membership and slow-entrance gaps. |
| `category-coverage.csv` | Fourteen musical categories plus Init; exact totals and distinct scoped evidence counts. |
| `coverage.json` | Full matrices, definitions, per-review address lists and scene limits, pressure-only exception records, slow-entrance rows and qualification boundaries. |
| `summary.json` | Small exact-count summary. |
| `source-binding.json` | SHA-256/byte bindings for inputs and outputs, plus the actual operation performed. |
| `build-matrix.py` | Deterministic retained-data join; no compilation, rendering or test execution. |

The complete interpretation and category/slow-entrance tables are in
[A55](../../reviews/a55.md).

## Reproduction

From the repository root:

```sh
python3 docs/preset-evidence/2026.11.7/verification/a55/build-matrix.py
```

The script reads A12's authenticated `.7` inventory, A33's accounting, all three
retained bank CSVs, family/follow-up measurements, and the named receipts. It
requires all 523 bank rows to align by address/name/category and the recorded
capture settings to match. It recomputes the `.4→.7` and `.6→.7` recorded signal
metric-change flags directly from those CSVs, excluding parameter digests.
Named supplemental membership is extracted from retained current-patch records;
exact duplicate identities across scenes and reports count once.

`121` is the targeted **A13–A27 plus A33–A36 review union**, including two Init;
it is not a count of every program rendered anywhere. All 523 already have
matched default measurements and the reused bounded LibraryAudit phrase/stack
gate. Existing regression fixtures may render other individual programs. The
402 musical programs outside this particular supplemental index must therefore
not be labeled unrendered or unreviewed.

The register/rate/gesture descriptions in `targeted_review_scopes` apply to their
specifically named scenes. A review containing several gestures does not imply
that every one of its programs received every gesture. `mapped_attack_beyond_3s`
uses A34's stated cold-card/velocity 0.8 mapping; extended-current-capture flags
refer only to the fourteen crossing programs and the reviewed evidence set.

CSV host indices and MIDI programs are zero based. Original addresses 0–221 use
MIDI MSB 1; curated addresses 222–522 use MSB 0 with local index `host−222`. For
either bank, LSB is local index divided by 128 and program is its remainder,
matching the retained `programIndexForMidi` source contract reviewed in A42.

The output files retain source context for the sound evidence and final binary
source without rebinding old native results. The prepared 238-WAV/92-program
audition roster remains a separate artifact contract. A55 makes no assertion
that the actual CI archive or final native platform results were accepted.

Temporary raw PCM may be absent following the two retained cleanup manifests.
This reproduction needs only retained measurements/records and does not claim
to recompare or listen to removed PCM.
