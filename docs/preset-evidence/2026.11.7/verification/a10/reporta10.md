# A10 — actual Engine texture integration

**PASS.** A05 preserves sustained body texture through the real Engine's realtime and offline paths. Four models × three host rates pass the predefined **0.5 dB** nonharmonic-band tolerance. No production code, presets, shared builds, commits or remote state were changed.

## Method and source binding

`engine_texture.cpp` extends A02's public-API fixture to flute, bowed, clarinet and brass. Each renders four seconds at MIDI 60/velocity 0.8, body mix 1, brightness 0.5, fixed VCA sustain and 113-frame blocks. Panel filters, chorus, drift, analog noise, keyboard contours and pitch modulation are disabled. The normal body, articulation, VCA, output chain and decimation remain active; no private body state is accessed.

Measured Engine telemetry confirms realtime Standard **2×**, explicit HQ/offline **8× at 44.1/48 kHz** and **4× at 96 kHz**. Analysis folds stereo to mono, discards the first second, applies Hann weighting and integrates normalized one-sided spectral power from **1–12 kHz**, excluding ±20 Hz around each harmonic of C4. A ±40 Hz exclusion and two separate subbands cross-check tonal leakage and spectral redistribution.

The initial captures used the preserved `18711f93` core and current core with separate matching header snapshots. A second, isolated compilation of all four core translation units reproduced **all 72 captures byte-for-byte: 36,115,200 float samples / 144,460,800 bytes**. Numeric CSV/JSON also match exactly. Baseline 48 kHz realtime flute exactly reproduces A02's original PCM. Source/library hashes and compiler identity are retained.

## Results

Corrected HQ/realtime nonharmonic power changes, dB:

| Body | 44.1 kHz | 48 kHz | 96 kHz |
|---|---:|---:|---:|
| Flute | −0.05856 | −0.04880 | −0.03774 |
| Bowed | −0.05707 | −0.04823 | −0.03905 |
| Clarinet | −0.05693 | −0.04624 | −0.03991 |
| Brass | −0.05583 | −0.04745 | −0.04067 |

Before correction, the same measurements lose **5.872–5.886 dB** at 44.1/48 kHz and **2.993–2.996 dB** at 96 kHz. After correction, the largest absolute difference is **0.05856 dB**. Doubling harmonic exclusions gives a worst difference of **0.07113 dB**; separate 1–4 and 4–12 kHz bands remain within **0.09791 dB** using ±20 Hz exclusions.

**All 12 corrected offline captures exactly equal explicit HQ**, as do all 12 baseline comparisons. All four corrected **48 kHz realtime** fixtures exactly preserve baseline PCM at the 96 kHz internal reference rate. Every capture is finite; maximum corrected absolute peak is **0.01670**.

Whole-output RMS is recorded only as context: baseline 48 kHz flute changes from 0.01151545 realtime to 0.01151600 HQ despite losing 5.88571 dB of texture. It would not detect the defect.

## Reproduction and limits

From this directory:

```sh
python3 reproduce.py --work /tmp/cs80-a10-reproduce
```

The script independently snapshots baseline/current sources, compiles matching cores, captures PCM outside the repository, and runs `analyze_texture.py`. Retained evidence includes `texture_summary.csv`, detailed powers/hashes in `texture_measurements.json`, actual factor logs, provenance and `source_rebuild_binding.json`; no PCM is retained in the repository.

This verifies sustained texture and offline routing for four controlled, sounded Engine fixtures. It does not separately measure flutter modulation, short contact attacks, complete factory-program voicings, or perceptual instrument similarity. A05's direct-body tests cover complementary excitation mechanisms; this report does not infer those results from total engine RMS.
