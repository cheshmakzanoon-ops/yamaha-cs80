# A49 — final release prose and claim boundaries

Reviewed native candidate: `078c0981ffa1b5472342e0fb5ab8702bc3bd2889` and the current uncommitted README voicing section / continuation-local-check section. The download block intentionally still describes the preceding qualified 2026.11.3 packages and was not treated as a new-release claim.

This was a read-only review of the repository and A38, A43 and A47 reports. No repository file, recipe, test, build, Git reference or native artifact changed. No new rendering, listening or acoustic research was performed.

## Decision

**No blocking numeric or source-scope error was found in the new 2026.11.4 prose.** The report now correctly labels the all-521 matched comparison as historical `59d489d`, separates the corrected-header local reruns, and preserves acoustic/perceptual limitations. The new README controls paragraph correctly states ten body controls, note-latched model/decay/size, live mix/brightness, and inactive Partial decay for sustained bodies.

Two small wording refinements would remove remaining ambiguity:

1. **README:** replace `The new bodies are` with `The instrument bodies are`. The bodies and 19 model choices existed in 2026.11.3; this revision improves their behavior and adds two size controls. Calling them new can imply new model choices despite the report's explicit preservation contract.
2. **Continuation local checks:** name `078c098` in the UI-fixture equivalence sentence and use `fixture control values` instead of `captured control positions`. The corrected source changed the test fixture's input values; the versioned golden image bytes remained unchanged. Suggested text: `The 078c098 UI fixture corrections change the exact host expectation and fixture control values without changing audio code, factory values, golden images or acceptance tolerances.`

Optional clarity: add the configured total of **68 distinct existing programs** after the separate 17-tine / 51-bowed descriptions, while keeping the detailed measured fixture coverage explicit. The two sets are disjoint, independently verified by A47. This is configuration reach, not 68 individual listening tests or a promise of changed sound at every note/velocity: full-strength tine and fixed-pitch bowed outputs intentionally remain unchanged in the retained parity checks.

## Claims that are supported, with their exact scope

| Claim | Defensible scope |
|---|---|
| All 521 musical programs reprogrammed | Existing non-master-volume parameter coverage relative to 2026.11.3. All 523 ordered names/categories/tags/addresses remain intact; both Init vectors preserve their previous 198 values and append neutral sizes. Do not describe all 7–52 edited controls per program as necessarily audible: some controls are inactive in particular configurations. |
| Whole-bank signal changes | The historical `59d489d` matched fixture changes spectral and relative-envelope metric groups for all 521 musical programs after replacing two pressure-only cases with explicit pressure-one captures. These exceed CSV rounding tolerances, not a perceptual audibility threshold. It is not a final-source matched render. |
| Approximately 4 dB more tine contrast | The targeted within-2026.11.4 normalization correction, measured for Electric Piano I, Electric Piano II and Tine Piano at MIDI 36/60/84, velocity 0.15→1, in the 300–800 ms body-RMS window. Do not present this as an all-521 effect, a real-EP error reduction, or a separately measured 2026.11.3-to-final delta. |
| 90 velocity captures | Five presets × three notes × six velocities: three tine targets plus Analog Piano and Reed Piano controls. They are not 90 different presets or a sweep of all 17 tine-enabled programs. |
| Bowed error reduction | Three retained octave-slide cases compared with the same synthesizer played directly at the target note; per-case maximum harmonic-ratio error changes from 4.10–6.19 to 0.003–0.025 dB. It is not error against a violin recording. |
| Continuation parity | Keep separate scopes: the 10,137,600-sample tine harness compares binary32 bits; the 36,115,200-sample bowed harness checks numeric float equality; the 45 complete-preset cases retain matching FNV audio hashes across 4,320,000 float samples. Do not combine them into a realism score or call the bowed comparison bitwise. |
| 138 WAVs / 74 programs | The capture contract, and only an available download after actual artifact qualification. The accompanying 523-program inventory is data coverage, not proof all 523 were listened to. |
| Native test/validator passes | The actual final-source run, attempt, package identities and original zero process exits must supply these results. Local tests and equivalent production trees do not establish final binary identity or installation success. |

The earlier and current local bank extrema happen to round to the same figures. The draft correctly keeps their separate source records rather than implying one historical capture was rerun on the new source. It also correctly avoids inferring internal assertion totals from the six truncated affected-suite stdout blocks.

## Suggested concise user-facing explanation

> Reprogrammed all 521 existing musical presets while preserving all 523 program names and addresses and both Init patches. The work changes their attack, body, decay, register balance and controller response. Instrument bodies now follow the panel filters; piano attacks have a smoother register-dependent partial structure; sustained instruments respond with changing harmonic balance and authored size profiles. No presets or instrument-model choices were added.
>
> The continuation also fixes two specific synthesis defects used by 68 existing programs. Tine-piano strikes now use a fixed full-strike normalization bound, giving about 4 dB more soft-to-firm body contrast in the measured electric-piano fixtures while preserving hard-strike endpoints. Bowed harmonics now move through stationary body resonances during bends and vibrato. These are demonstrated signal-behavior improvements. The acoustic presets remain synthesized interpretations; the evidence does not establish indistinguishable reproduction of every named instrument or a universal realism score.

After actual package qualification, follow that explanation with the four verified downloads, exact binary source and observed native results. Give the actionable upgrade instruction prominently: **Library → FACTORY LIBRARY → LOAD / RELOAD**, then play fresh notes. An old project can retain its saved recipe even while displaying the same factory name.

## Specific remaining acoustic limits

The draft's broad qualification is accurate. For a candid final answer to a request for exactness, a short concrete example is more informative than an abstract disclaimer: Kalimba still uses the tine model; Xylophone uses marimba modes; Steel Drum uses vibraphone modes with an octave layer. Wind, brass and plucked body weights still retain their onset-captured response during pitch motion. The bowed correction must not be described as fixing those families' formants.

No speculative parameter change or additional source restart is recommended by this prose review. A38 retains the detailed family table and primary-source references from the earlier specialist reviews.

## Inspected working-tree hashes

- `README.md`: `2a03a344f186d1a08230037b2d11101d9e9cdf3aa2ba682bcc23f273c1113149`
- `docs/CIRCUIT_LIBRARY_2026_11_4.md`: `21487a8249e8c115c20ce88cf2190cb4bf08caf58085d1eec66b24775b3e6814`
- `src/core/InstrumentBody.h`: `d33f34f5cff6341abc9bc2c85056ce381b492d141a0fad856e0c7cbd4f06422e`

These hashes identify the text reviewed at this point, not the eventual final publication or downloaded packages.
