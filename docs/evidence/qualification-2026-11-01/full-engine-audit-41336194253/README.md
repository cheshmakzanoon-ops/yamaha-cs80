# Complete 2026-11-01 three-rate source-engine audit — original raw evidence

This directory preserves original raw CSVs, the run log, completion marker, provenance, and the frozen Markdown reporter from [GitHub Actions run 41336194253, artifact 13097476296](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41336194253/artifacts/13097476296). The **complete ten-file original ZIP** is preserved losslessly, with its bytes base64-encoded across `archive.b64.part01` through `archive.b64.part08`; the large `audit-summary.json` is preserved inside that archive even if omitted as a separate standalone file in the evidence commit. The original ZIP is 227,720 bytes and its SHA-256 is `a76dd6ee33dcfa4f14be4223ecb06331330d19d90a0baf84147745cef2c88f19`.

## Reconstruct the original complete archive

From this directory, concatenate the zero-padded parts in filename order and decode:

```bash
cat archive.b64.part0{1,2,3,4,5,6,7,8} | base64 --decode > complete-engine-audit.zip
sha256sum complete-engine-audit.zip
unzip -t complete-engine-audit.zip
```

The SHA-256 output **must** equal `a76dd6ee33dcfa4f14be4223ecb06331330d19d90a0baf84147745cef2c88f19`, and `unzip -t` must pass all ten members. The eight shards were reconstructed and SHA-256-checked before submission. To inspect the complete frozen JSON summary without storing 2.68 MB twice:

```bash
unzip -p complete-engine-audit.zip audit-summary.json
```

The extracted files in this evidence directory must remain byte-for-byte identical to their counterparts inside the archive. The base64 parts are transport encoding only—not new measurements or an edited reporter.

## Verified identities

- Run source: `39ae6c2684456950302cb4ed10ecfae980c37ddd`
- Production audio commit: `83b7f7670f000a95c6b44f96ffb0df3894755d14`
- `HEAD:src`: `7dd159d491012fd47b46c0aa651e86ea959ca289`
- Frozen audit registration: `00da4498792523cf8d97fd40c17e12f215e190b8`
- Frozen reporter SHA-256: `3f1a68208c8c848c645088dae8d1a398190f32c05c1344bbffd3f4c071aa2fd9`
- Completion marker time: `2026-11-01T19:29:20.204708+00:00`
- Provenance JSON SHA-256: `36cd995e4113448329c4c555313840a48b42adca29c4d290ea3bd30d57625018`

`unzip -t` passed all ten entries. The five raw CSV SHA-256 checks match `audit-provenance.json`. They contain **885 data rows**, plus headers: latency (40), alias (168), FX (102), phrase (566), and click-causality (9). Zero nonfinite numeric CSV cells were reported. Both software source and Legacy engine policies were rendered; the 222-program frozen bank is complete at 44.1 kHz but subsampled at 48/96 kHz, along with 11 paired tone-tab fixtures at each rate. The separate curated 301-program bank, PURE mode, and CPU suite were not run by this audit.

## Original numerical criteria (Source Performance)

| Criterion | Target | Worst | Exceedances | Verdict |
|---|---:|---:|---:|---|
| Phrase peak | ≤0 dBFS | −0.370566 dBFS | 0/283 | PASS |
| Retrigger score | ≤1 | 1.23689 | 1/283 | FAIL |
| Voice-steal score | ≤1 | 1.03039 | 1/283 | FAIL |
| Single-note-off score | ≤1 | 1.62689 | 22/283 | FAIL |
| Chord-note-off score | ≤1 | 1.84628 | 37/283 | FAIL |
| Ring-off saw/pulse alias, 48 kHz | ≤−70 dB | −71.14 dB | 0/14 | PASS |
| Original physical-panel causality | ≤1 | 1.440924 | 2/3 | FAIL |
| Tone-tab causality | ≤1 | 1.626889 | 3/3 | FAIL |
| Electrical-programmer causality | ≤1 | 0.385795 | 0/3 | PASS |

Source phrase metric extrema are preserved by name in `audit-summary.md` and row-by-row in `audit-summary.json`. Original physical-panel Bass Stack fails at 48 and 96 kHz; Funky 1 + Funky 2 note-off fails at all three. Software-only evidence cannot establish original hardware or real DAW behavior. A successful GitHub Actions execution means the registered coverage, reporter, and artifact publication succeeded; **it is not an all-target pass**.

## Verify without mutation

1. Reconstruct and verify the complete ZIP as shown above. Compare each extracted CSV to SHA-256 entries in `audit-provenance.json` and to the corresponding ZIP member.
2. Inspect `audit.complete` against `audit-provenance.json` for the run, source, and registration identities.
3. Compare the raw 885 rows to the summary. Never delete, filter, or relax a failing row to obtain a green result.

This is frozen evidence for source commit `39ae6c...`; if `src/**` changes, run a **separately preregistered** complete audit at the new exact source, retaining this evidence unchanged.
