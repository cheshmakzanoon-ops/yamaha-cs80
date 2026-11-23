# Agent 15 — Plucked recipe review

Scope: all 16 recipes in `src/core/curated/Plucked.inc`, final host indexes 388–403. Read against the 2026.11.4 final inventory, fluent helper semantics, Engine body replacement/filter routing and modal definitions. No production files changed. No acoustic listening or identity-to-recording claims.

## Checklist

| Host index | Existing preset | Main / upper model | Feet | Main / upper VCA decay; sustain | Assessment |
|---|---|---|---|---|---|
|388|Guitar I|Nylon / Nylon|8 / 8|4.5/0.12 s; 0/0|Nylon main and short high-passed pick; correct zero sustain.|
|389|Guitar II|Steel / Steel|8 / 8|5.8/0.18 s; 0/0|Steel main; broader bandwidth and longer upper/body decay than Guitar I.|
|390|Funky Pluck I|Clav / Clav|8 / 8|1.25/0.24 s; 0/0|Tangent/clav plus resonant circuit pulse; intentionally synthetic rhythmic wah.|
|391|Funky Pluck II|Clav / Steel|8 / 8|2.55/0.39 s; 0/0|Clav/steel hybrid with slower filter contour; intentionally synthetic wah.|
|392|Harp|Nylon / Nylon|8 / 4|9/1.12 s; 0/0|Long nylon body with separately shorter octave course; 2.8 s release.|
|393|Koto|Steel / Clav|8 / 8|4.25/0.12 s; 0/0|Steel/clav body and brief plectrum, initial pitch bend active; physical string-material caveat below.|
|394|Banjo|Steel / Clav|8 / 8|1.45/0.08 s; 0/0|Fast steel/clav loss and nasal circuit body; no measured membrane resonator.|
|395|Mandolin|Steel / Steel|8 / 8|3.15/2.75 s; 0/0|Paired steel courses; 1.8-cent detune and faint 12 Hz amplitude flutter; no automatic re-plucking.|
|396|Drone Pluck|Steel / Steel|8 / 8|4.5/0.18 s; 0.16/0|Intentional hybrid: pluck plus 0.16 sustained circuit bed, pressure and faint ring mix.|
|397|Nylon Pluck|Nylon / Nylon|8 / 8|4.8/0.08 s; 0/0|Nylon main; soft short pick, zero detune/chorus; no held oscillator buzz.|
|398|Steel Pluck|Steel / Steel|8 / 4|6.2/2.15 s; 0/0|8-foot plus 4-foot steel courses; octave decays sooner than root.|
|399|Muted Pluck|Steel / Clav|8 / 8|0.42/0.055 s; 0/0|Short zero-sustain steel/clav, rapid partial loss and pick; detached strokes.|
|400|Kalimba|Tine EP / Celesta|4 / 2|2.8/0.16 s; 0/0|4-foot tine plus short2-foot celesta; velocity destinations active; EP pickup-model caveat below.|
|401|Zither|Steel / Steel|8 / 8|7/3.6 s; 0/0|Paired steel courses with unequal long decay/release, shallow chorus and1.2-cent detune.|
|402|Dulcimer Pluck|Steel / Steel|8 / 4|4.5/0.28 s; 0/0|Steel root plus short octave; deliberately plucked dulcimer articulation.|
|403|Pressure Pluck|Nylon / Steel|8 / 8|4.2/0.16 s; 0.12/0|Intentional hybrid: nylon/steel attack over0.12 sustained circuit bed; pressure works.|

## Findings

No definite octave, touch-route or unintended sustained-note recipe defect was found. All 32 body ranks use struck models with active modal decay; none has the inert sustained-body decay issue identified elsewhere. Harp, Steel Pluck and Dulcimer Pluck correctly give the upper rank a 4-foot octave. Drone Pluck and Pressure Pluck deliberately retain sustained circuit signal; their aftertouch settings therefore act on a remaining sound. Mandolin explicitly requires repeated strikes for tremolo picking; its 12 Hz SUB is only faint level flutter.

At full body mix, oscillator waveform/sine/VCF-output-level literals are dormant for Harp, Nylon Pluck, Steel Pluck and Kalimba main ranks. HP/LP filters, rank envelopes and touch remain active. This should not be confused with inaudible filter parameters.

### Named-instrument limitations that remain

**Kalimba is using an electric-piano excitation approximation.** `makeTineModes()` explicitly generates pickup odd harmonics (3,5,7,9 times fundamental), plus brief 6.267 and17.55 components, and `pickupMotion` changes those harmonic weights with excursion. Acoustic kalimba measurements instead identify prominent inharmonic beam modes, with a reported first overtone around5.3–5.9 times the fundamental depending on tine geometry. Boundary/contact effects can add harmonics, so integer components are not categorically forbidden; the current EP-specific law is nevertheless not a measured acoustic kalimba model. The tine normalization correction sensibly preserves hard excitation while removing soft-tone boost, but does not establish kalimba realism. A future correction should fit measured lamellophone modal ratios/levels/contact behavior rather than simply swapping to celesta or altering a cutoff.

Primary references:
- https://dael.euracoustics.org/confs/acoustics2008/data/articles/000215.pdf — measured kalimba modes and geometry-dependent first overtone.
- https://pubmed.ncbi.nlm.nih.gov/22280717/ — measured first three inharmonic transverse beam modes.
- https://digitalcommons.kettering.edu/naturalsci_facultypubs/5/ — offset boundary/contact can also generate harmonic peaks; avoids an overstrong no-harmonics claim.

**Koto uses the shared steel-string profile.** Primary koto-string research discusses silk and synthetic/polyester fiber strings. The preset description honestly calls its authored body a bright steel-string body, but the model should not be described as an exact physical koto. A nylon-only swap without fitting excitation/bridge/body response would not itself establish improvement.

Primary references:
- https://pub.dega-akustik.de/ISMA2019/data/articles/000063.pdf — finite-element modeling of Japanese koto strings.
- https://www.jstage.jst.go.jp/article/transjtmsj1972/49/7/49_7_T175/_article — silk-like polyester koto-string research.

Banjo also lacks a dedicated membrane/cavity model; its steel/clav and circuit approximation is disclosed by the description. This is a modeling scope limit, not an identified routing regression.

## Focused Kalimba signal check

Twelve complete-preset captures compare pre-fix and corrected tine builds at MIDI36/60/84 and velocities0.2/1.0;0.8 s holds,48 kHz, no tail. Exact CSV metrics are in `a15/kalimba-metrics.json`. Soft-to-hard whole-clip RMS span increased from6.96–9.27 dB to10.86–13.09 dB. Hard-strike peak/RMS metrics remain exactly equal at reported precision; all12 captures are finite and have no full-scale samples. This is a regression/dynamics check, not acoustic matching.

## Inputs

- `src/core/curated/Plucked.inc` SHA256 `4bb4a23d1f09f1a3318ae59f592ccb9409d3de511276fded347948eef38fc7ac`
- `src/core/InstrumentBody.h` SHA256 `d33f34f5cff6341abc9bc2c85056ce381b492d141a0fad856e0c7cbd4f06422e`
- `src/core/Engine.cpp` SHA256 `9712fce722056b254fa260c5f1bc0c92219086bff30a8bca681247398f5c1ce2`
- `docs/preset-evidence/2026.11.4/development/final-bank/inventory.json` SHA256 `bff48651085358fcc3d1b07ee5f9138f629c84e7fa9e5ae067796395096a0280`
