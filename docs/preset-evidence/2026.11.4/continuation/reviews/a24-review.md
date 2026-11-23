# Agent 24 — original-address bank audit

Read-only review of `src/core/OriginalCircuitLibrary.inc` at source `c4fe30987a5242305be60bd6cfddb144facd6643`, using a fresh inventory from the final local capture executable. This is structural/routing evidence, not a listening or acoustic-match claim.

## Result

No additional definite mapping, rank-swap, registration, struck-attack or initialization defect found.

- All **222 original addresses** resolve by explicit recipe name to **171 distinct curated source recipes**: 22 tone studies, 22 explicitly named programs (including Init), and 178 numbered family variations.
- All **221 musical programs** differ from the 2026.11.3 baseline in existing non-master-volume controls. Ordered names, categories and tags are unchanged.
- **Init Patch** retains its original 198-parameter vector exactly; the appended size values are both neutral 1. Its historical golden conversion from Init Circuit is deliberately preserved by a dedicated branch, rather than receiving the newer finish factors.
- A binary32 comparison of all 521 musical vectors finds **zero duplicates after excluding master volume**. This proves distinct parameter vectors, not distinct perceived sound quality.
- The freshly exported complete inventory SHA256 is `bff48651085358fcc3d1b07ee5f9138f629c84e7fa9e5ae067796395096a0280`, exactly matching the retained final 2026.11.4 inventory. The continuation therefore changes DSP without altering factory vectors.

## Numbered-family mapping coverage

| Family | Variations |
|---|---:|
| Brass | 22 |
| Strings | 24 |
| Pads | 28 |
| Bass | 22 |
| Keys | 18 |
| Leads | 22 |
| FX | 20 |
| Sequences | 22 |

All fourteen finish styles occur: Amber, Velvet, Distant, Neon, Hollow, Golden, Frozen, Electric, Midnight, Slow, Bright, Dark, Wide and Soft. Every lookup resolves; a missing recipe throws rather than silently selecting a generic fallback.

## Finish and identity checks

The finish runs while the source category is still available. This correctly gives tone studies their source instrument's behavior before they are relabelled Hardware Studies. Modern-file arpeggiators have the actual category Sequences, so their short-attack, limited-release and narrower-width handling is active.

For struck-family recipes under Velvet, Distant, Slow and Soft finishes, **all 38 checked rank attacks remain bit-identical to their source recipes**. The finish extends or shades the body without slowing the hit. Separate short accents get smaller decay changes and tightly bounded release growth.

All mapped body-model and body-size values are inherited exactly, with rank exchange where appropriate. Footage is likewise inherited except **Bass Stack**, whose explicit two-8-foot override agrees with its revised description. Tone II studies swap the entire channel controls, all four body controls and body sizes, then complement MIX and remove one-sided detuning. No body-size omission or misplaced named rank was found.

The original Keys variations explicitly disable glide, initial pitch bend, and initial/aftertouch SUB pitch motion. All original programs normalize to the Circuit engine with parity off and the original SUB mode. Sequence overrides update their rate/direction/octave descriptions after changing those controls.

## Continuation reach

- **8 original-address programs** use active tine ranks and inherit the strike-normalization correction.
- **27 original-address programs** use active bowed ranks and inherit stationary body formants.
- Thus **35 original programs** receive one of the two new body fixes. Combined with 9 curated tine and 24 curated bowed programs, the continuation reaches the root's **68-program** total, within the preceding 521-program recipe overhaul.

The exact affected names and all 222 recipe mappings are retained in `a24/summary.json` and `a24/mapping.json`. No new preset, source model or program address is introduced.

## Limits

Inheritance carries the source recipe's known acoustic-model approximations as well as its improvements. Non-neutral sustained-body Partial decay settings remain inactive and must not count as audible changes. Parameter identity and attack checks do not establish perceptual improvement or acoustic equivalence. Full-bank rendered levels and native distribution qualification are separate root-owned checks.

## Evidence

- `a24/summary.json`: counts, source/inventory hashes, duplicate and identity checks.
- `a24/mapping.json`: each original program's named source recipe, finish and rank exchange.
- `a24/protected-attacks.json`: all 38 exact struck-attack comparisons.
- `a24/inventory.json`: fresh complete 523-program export.

No production code, preset data, tests, commits or remote references changed.
