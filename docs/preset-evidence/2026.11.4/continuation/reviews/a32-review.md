# Agent 32 — final inventory and identity review

Source: `e32728b31d97fc356cac291cb98935fdcd38f803` (tree `d570aecf7e8dea5e42476f308497d84b1377c657`).

**Pass:** independently checked the final exported inventory against the 2026.11.3 baseline and the prior c4 export. This review parses the JSON and compares the binary32 values independently of the production comparison script. No audio was rerendered and no production files were changed.

## Stable catalogue and parameter layout

- All **523 ordered host identities, names, categories and tag lists** are unchanged. No programs were added, removed or moved.
- All **198 previous core parameter IDs** remain in the same order. Only `body1_size` and `body2_size` append, giving **200 core parameters**.
- All **521 musical programs** change existing non-volume controls relative to the 2026.11.3 baseline. The per-program count ranges from **7 to 52**. These are structural changes, not evidence that every changed literal is active or improves sound.
- **Init Patch**, host index **43**, and **Init Circuit**, host index **522**, preserve all prior 198 values exactly; both appended sizes equal **1**.
- **Zero duplicate non-volume parameter vectors** across the complete catalogue.
- **259 programs configure enabled instrument bodies**; **264**, including both Init programs, are circuit-only. Configured-body counts do not themselves prove that every rank contributes audibly in every playing condition.
- Every family/count in the retained structural report matches the independent calculation.

| Family | Existing programs | Changed musical programs |
|---|---:|---:|
| Bass | 50 | 50 |
| Bells | 23 | 23 |
| Brass | 59 | 59 |
| FX | 46 | 46 |
| Hardware Studies | 22 | 22 |
| Init | 2 | 0 |
| Keys | 45 | 45 |
| Leads | 56 | 56 |
| Organ | 15 | 15 |
| Pads | 63 | 63 |
| Plucked | 17 | 17 |
| Polysynth | 25 | 25 |
| Sequences | 27 | 27 |
| Strings | 59 | 59 |
| Winds | 14 | 14 |

## Exact difference from c4

All **523 parameter vectors are unchanged** from the c4 inventory. The c4 reference was checked against its committed Git blob and agent 24's independently exported inventory.

Exactly two exported descriptions change:

| Program | Host index | Change |
|---|---:|---|
| Velvet FX 138 | 181 | Inherits the corrected Wind description; its existing finish suffix is unchanged. |
| Wind | 484 | Corrects the reversed entrance order: upper gust first, slower low bed afterward. |

There are no other exported field differences from c4. Thus the final metadata change does not revoice, rename or reorder any program.

## Evidence

- Final inventory SHA256: `0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611`
- Baseline inventory SHA256: `6f070ed15a04259fd5a9d5f0eb06a6e9554f821871c3313e9a793a3b28a2b521`
- Prior c4 inventory SHA256: `bff48651085358fcc3d1b07ee5f9138f629c84e7fa9e5ae067796395096a0280`
- Checked structural report SHA256: `43b693d7990b0e8857867de3b29ed69b02fbc5cec83285f63977b6257825dad9`
- Reproducible independent comparison: `a32/review.py`
- Structured findings and hashes: `a32/summary.json`

This establishes catalogue preservation and parameter coverage. It is not a listening judgment or proof of acoustic-instrument identity.
