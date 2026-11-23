# A37 — historical versus continuation evidence labels

Reviewed source: `bcc86aa39a33a0162254614e5502705f0113d701`.
Scope: read-only documentation and retained evidence review. No production files, repository documentation, tests, renders or Git state changed. This report is a scratch recommendation for the final documentation publication.

## Finding

`docs/CIRCUIT_LIBRARY_2026_11_4.md` correctly separates development evidence from downloadable artifact qualification, but several unqualified uses of “final” now refer to the earlier `59d489d` stage. The strongest ambiguity is the whole-library audio section: it can be read as a matched audio comparison of the current continuation source, which it is not.

The linked `development/final-bank/provenance.json` records body-header SHA256 `e61da6a5cc9b428f8a9816c1de812cad0adb10b376be5e44e7ca3ae2844fbb71`; this is exactly the header in `59d489d`. Current `bcc86aa` uses `d33f34f5cff6341abc9bc2c85056ce381b492d141a0fad856e0c7cbd4f06422e`. The same historical hash appears in `development/final-affected/summary.json` and `reviews/47-final-sanitizers.json`. Current and historical `tests/PresetVoicingTests.cpp` also differ. Preserve those original evidence records unchanged; qualify their stage in reader-facing prose.

## Exact recommended changes

### 1. Historical bank/audio section

File: `docs/CIRCUIT_LIBRARY_2026_11_4.md`.

Replace heading:

```markdown
### Final local bank and matched-audio checks
```

with:

```markdown
### Historical bank and matched-audio checks — 59d489d

The results in this subsection belong to the pre-continuation source
`59d489d1a1f0065ae2a0f42a08c74edc43e94159`. They precede the tine-normalization
and bowed-formant corrections described above. Their original manifests and
results are retained unchanged; they are not a matched-audio comparison of
`bcc86aa39a33a0162254614e5502705f0113d701`.
```

In the following paragraph change `Voicing, character and precision checks passed` to `At that historical stage, voicing, character and precision checks passed`. Retain the observed **281/281**, **128/128**, **285/285** values exactly. Do not silently replace 281 with a new source-derived count.

Change the opening of the matched-capture paragraph from:

```markdown
A separate matched 48 kHz capture compares all 523 programs with the preceding
revision, using three-second holds and one-second tails.
```

to:

```markdown
A separate matched 48 kHz capture compares all 523 programs in that
`59d489d` stage with revision 2026.11.3, using three-second holds and
one-second tails.
```

Change link label `[Final bank results]` to `[Historical 59d489d bank results]`, leaving its target unchanged. Keep the all-521 signal-change finding and all perceptual qualifications unchanged.

### 2. Earlier development/test paragraphs

In the same file, replace the two paragraphs beginning `Development evidence and release qualification are separate.` and `The final piano-course source also passed...` with this stage-qualified wording:

```markdown
Development evidence and release qualification are separate. The initial
engine run passed **194/194 tests**. Before the continuation, the direct-model
suite expanded to cover fractional-note piano boundaries and passed
**2,077 checks**, generating
**41,062,350 samples**. The earlier engine-integration suite passed **18,156
checks**. Subsequent continuation reruns and native artifact qualification
retain their own exact inputs and source identities. A development capture
is never evidence that a downloadable installer contains those bytes.

That pre-continuation piano-course source also passed its affected model and
integration reruns. Two LFO tests were rerun with fresh capture directories,
and the Mac workflow contract test was updated for the stronger AU payload
check. Those follow-ups have separate source manifests and are counted
separately. The body/filter headers at that historical stage also passed a
scoped ASan/UBSan probe; it did not qualify leak scanning or native host
wrappers. It is not a sanitizer result for the later tine and bowed-body
corrections.
```

Change the adjacent `[Final header sanitizer probe]` link label to `[Historical 59d489d header sanitizer probe]`, leaving the target unchanged.

### 3. Distinct current-source section

When the root publishes final local/native qualification, give it a separate heading identifying the current source, and link the newly retained evidence. Do not relabel or overwrite the `development/final-bank` and `reviews/47-final-sanitizers.json` files. The current local affected-suite record is `continuation/local-affected/summary.json` and binds the corrected body-header SHA above; its seven passed CTest entries are distinct from the historical four bank suites and their internal assertion counts. Current all-bank/native results should use their actual retained outputs, not derive counts from source code.

### 4. Worker ledger link

The existing `[Actual worker assignments](preset-evidence/2026.11.4/work-log.json)` points to the earlier development ledger. In final publication, label it `[Initial development worker assignments]` and add a separate link to the actual continuation ledger. Do not imply that historical planned follow-ups were completed continuation workers.

## Other reviewed documentation

- `docs/INSTALL_AND_QUICKSTART.md` already says revision 2026.11.4 is separate from application version, requires exact manifest/source matching, and warns that earlier archives retain earlier sources. No source-label correction identified.
- `docs/PRESET_GUIDE.md` already gives explicit **Library → FACTORY LIBRARY → LOAD / RELOAD** instructions and correctly states the audition roster as configured output, not completed publication. No source-label correction identified.
- `docs/CI_DISTRIBUTION_STATUS.md` remains explicitly current at 2026.11.3. It should remain so until actual current-source qualification completes, then retain that entire block under a historical heading while adding the newly verified 2026.11.4 record. Old counts and old artifact IDs must remain associated with their original sources.

## Outcome

Documentation-only recommendations; no production blocker or new acoustic claim. The current report already says measurements do not establish audibility thresholds, a realism score or indistinguishability from recordings. Preserve those limits.
