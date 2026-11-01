# Completed engine audit summary

Source snapshot: `1e5893854c3d0d9b0aa1ab1c725fd2e8084717f6`. Production change: `23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162`.
Generated 2026-11-01T03:24:58.580986+00:00 from the completed CSVs. The integration owner's completion marker is recorded in JSON.

This is an analysis of software renders, not a hardware measurement, host validation, or release/CI claim. Raw rows are preserved without filtering in the CSVs and verbatim in the JSON; extrema below use their emitted numeric precision.

## Coverage and unchanged targets

The command is `cs80_engine_audit --suite all --rates 44100,48000,96000 --out audit` at the default oversampling setting. Its first rate is 44.1 kHz. The program factory is `factoryPresets()`: the frozen **222-program Legacy Library under both engine policies**. The separate 301-program curated library is not rendered by this tool. All eleven paired tone selectors run at every rate. Secondary program rates retain indices 0,16,…,208 only. `all` excludes the CPU suite; these CSVs do not establish the CPU budgets.

| Policy | Rate | Programs | Tab pairs | Program coverage |
| --- | --- | --- | --- | --- |
| legacy | 44100 | 222 | 11 | 222-program bank |
| legacy | 48000 | 14 | 11 | Every 16th: 14 programs |
| legacy | 96000 | 14 | 11 | Every 16th: 14 programs |
| source | 44100 | 222 | 11 | 222-program bank |
| source | 48000 | 14 | 11 | Every 16th: 14 programs |
| source | 96000 | 14 | 11 | Every 16th: 14 programs |

Source phrase program rows use the electrical programmer; Legacy ignores that new coordinate. Tone-tab rows use their selected factory networks. Original physical-PANEL Bass is reported separately by the causal suite. Phrase CSVs do not export ring state, so ring-off/on separation belongs to the explicit alias rows only.

Targets retain their original scope. The registered oscillator limit is **−70 dB for Source saw/pulse, ring off, at 48 kHz**; other alias rates and all ring-on rows remain reported diagnostics. Source peak ≤0 dBFS and the existing click/retrigger/steal limits of 1 are reported without changing a metric or excluding failing rows. Legacy is a compatibility comparison, not a retuning target.

| Existing criterion / scope | Limit | Worst exported value | Exceeding rows / evaluated | Within target |
| --- | --- | --- | --- | --- |
| Source phrase peak, rendered coverage | 0.0 | -0.370568 | 0/283 | True |
| Source phrase retrigger, rendered coverage | 1.0 | 1.6192 | 2/283 | False |
| Source phrase steal, rendered coverage | 1.0 | 1.03039 | 1/283 | False |
| Source phrase single note-off, rendered coverage | 1.0 | 1.62689 | 23/283 | False |
| Source phrase chord note-off, rendered coverage | 1.0 | 1.6135 | 37/283 | False |
| Source saw/pulse, ring off, 48 kHz | -70.0 | -71.14 | 0/14 | True |
| Source causal baseline: original_physical_panel | 1.0 | 1.440924 | 2/3 | False |
| Source causal baseline: electrical_programmer | 1.0 | 0.38579501 | 0/3 | True |
| Source causal baseline: tone_tabs | 1.0 | 1.6268891 | 3/3 | False |

## Phrase results by policy, rate and source family

| Policy | Rate | Family | Rows | Peak dBFS | Off | Chord off | Retrigger | Steal | Nonfinite frames |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| legacy | 44100 | frozen_legacy_library | 222 | -3.00479 | 1.17989 | 1.20541 | 1.2049 | 1.18409 | 0 |
| legacy | 44100 | tone_tabs | 11 | -4.64653 | 0.954173 | 0.938273 | 1.19487 | 1.09393 | 0 |
| legacy | 48000 | frozen_legacy_library | 14 | -5.15521 | 1.1602 | 0.902129 | 1.05011 | 0.740904 | 0 |
| legacy | 48000 | tone_tabs | 11 | -5.14263 | 1.01817 | 0.951517 | 1.34769 | 1.08883 | 0 |
| legacy | 96000 | frozen_legacy_library | 14 | -5.09956 | 1.02372 | 1.18897 | 0.970191 | 1.01431 | 0 |
| legacy | 96000 | tone_tabs | 11 | -5.28813 | 1.02847 | 1.02386 | 1.13043 | 0.835518 | 0 |
| source | 44100 | frozen_legacy_library | 222 | -20.2651 | 1.46513 | 1.46621 | 1.6192 | 0.988469 | 0 |
| source | 44100 | tone_tabs | 11 | -0.699051 | 1.62689 | 1.6135 | 0.908938 | 0.706603 | 0 |
| source | 48000 | frozen_legacy_library | 14 | -20.9416 | 1.03091 | 1.48248 | 0.320147 | 0.469702 | 0 |
| source | 48000 | tone_tabs | 11 | -0.415768 | 1.29131 | 0.726792 | 0.665386 | 0.845239 | 0 |
| source | 96000 | frozen_legacy_library | 14 | -20.6821 | 1.07748 | 1.10016 | 0.305112 | 0.503841 | 0 |
| source | 96000 | tone_tabs | 11 | -0.370568 | 1.22096 | 0.760491 | 0.763877 | 1.03039 | 0 |

### Named phrase extrema

Each maximum is over that policy's rendered coverage, including the subsampling above. JSON retains all tied rows and all other metric ranges.

| Policy | Metric | Maximum | Named raw row(s) |
| --- | --- | --- | --- |
| legacy | peak_db | -3.00479 | Hollow Bass 93; group=program:Bass; rate=44100; phrase.csv:171 |
| legacy | click_off | 1.17989 | Midnight Pads 69; group=program:Pads; rate=44100; phrase.csv:147 |
| legacy | click_chord_off | 1.20541 | Amber FX 137; group=program:FX; rate=44100; phrase.csv:215 |
| legacy | click_retrigger | 1.34769 | Harpsichord 1 + Harpsichord 2; group=tab; rate=48000; phrase.csv:19 |
| legacy | click_steal | 1.18409 | Tone II: Harpsichord 2; group=program:Hardware Studies; rate=44100; phrase.csv:48 |
| legacy | subsonic_rel | 0.0646904 | Bright FX 147; group=program:FX; rate=44100; phrase.csv:225 |
| legacy | dc_idle_abs | 3.53046e-06 | Frozen Pads 67; group=program:Pads; rate=44100; phrase.csv:145 |
| source | peak_db | -0.370568 | Guitar 1 + Guitar 2; group=tab; rate=96000; phrase.csv:315 |
| source | click_off | 1.62689 | Funky 1 + Funky 2; group=tab; rate=44100; phrase.csv:294 |
| source | click_chord_off | 1.6135 | Brass 1 + Brass 2; group=tab; rate=44100; phrase.csv:287 |
| source | click_retrigger | 1.6192 | Laser Sweep; group=program:FX; rate=44100; phrase.csv:359 |
| source | click_steal | 1.03039 | Clavichord 1 + Clavichord 2; group=tab; rate=96000; phrase.csv:312 |
| source | subsonic_rel | 0.0538019 | Dark FX 148; group=program:FX; rate=44100; phrase.csv:509 |
| source | dc_idle_abs | 1.19551e-05 | Hollow FX 155; group=program:FX; rate=44100; phrase.csv:516 |

## Ring-off and ring-on spectra

Nonharmonic power is relative to legitimate partial power; spur is the strongest individual off-partial bin relative to the strongest legitimate bin. Ring-on is the fixed 150 Hz carrier test, not a high-carrier qualification.

The tool's ring-on mask admits k*f0 +/-150 Hz (including the carrier at k=0), but does not separately admit the unshifted k*f0 signal. The declared Source multiplier contains a 0.005*x signal-feedthrough term. Consequently, its ring-on nonharmonic metric also counts intended unshifted signal energy. An ideal single-line calculation with equal downstream gain gives 10*log10(2*0.005^2) = -43.0103 dB relative to the two sidebands. This is a code-derived diagnostic interpretation, not a new gate or proof that every residual is alias-free; filtering, other components and finite windows remain in the actual exported measurements.

| Policy | Rate | Ring | Wave | Rows | Worst nonharmonic dB | Worst spur dBc | Nonharmonic worst row |
| --- | --- | --- | --- | --- | --- | --- | --- |
| legacy | 44100 | 0 | pulse | 7 | -34.92 | -35.16 | pulse; rate=44100; note=96; feet_ratio=4.00; ring=0; alias.csv:21 |
| legacy | 44100 | 0 | saw | 7 | -32.45 | -33.36 | saw; rate=44100; note=96; feet_ratio=4.00; ring=0; alias.csv:7 |
| legacy | 44100 | 1 | pulse | 7 | -34.92 | -35.33 | pulse; rate=44100; note=96; feet_ratio=4.00; ring=1; alias.csv:28 |
| legacy | 44100 | 1 | saw | 7 | -32.81 | -33.53 | saw; rate=44100; note=96; feet_ratio=4.00; ring=1; alias.csv:14 |
| legacy | 48000 | 0 | pulse | 7 | -39.3 | -40.92 | pulse; rate=48000; note=84; feet_ratio=4.00; ring=0; alias.csv:75 |
| legacy | 48000 | 0 | saw | 7 | -33.78 | -34.93 | saw; rate=48000; note=96; feet_ratio=4.00; ring=0; alias.csv:63 |
| legacy | 48000 | 1 | pulse | 7 | -39.3 | -40.85 | pulse; rate=48000; note=84; feet_ratio=4.00; ring=1; alias.csv:82 |
| legacy | 48000 | 1 | saw | 7 | -34.13 | -35.39 | saw; rate=48000; note=96; feet_ratio=4.00; ring=1; alias.csv:70 |
| legacy | 96000 | 0 | pulse | 7 | -42.74 | -45.17 | pulse; rate=96000; note=96; feet_ratio=4.00; ring=0; alias.csv:133 |
| legacy | 96000 | 0 | saw | 7 | -38.97 | -42.26 | saw; rate=96000; note=96; feet_ratio=4.00; ring=0; alias.csv:119 |
| legacy | 96000 | 1 | pulse | 7 | -42.74 | -45.32 | pulse; rate=96000; note=96; feet_ratio=4.00; ring=1; alias.csv:140 |
| legacy | 96000 | 1 | saw | 7 | -39.31 | -42.41 | saw; rate=96000; note=96; feet_ratio=4.00; ring=1; alias.csv:126 |
| source | 44100 | 0 | pulse | 7 | -76.77 | -82.97 | pulse; rate=44100; note=72; feet_ratio=4.00; ring=0; alias.csv:45 |
| source | 44100 | 0 | saw | 7 | -71.45 | -77.56 | saw; rate=44100; note=72; feet_ratio=4.00; ring=0; alias.csv:31 |
| source | 44100 | 1 | pulse | 7 | -43 | -40.07 | pulse; rate=44100; note=84; feet_ratio=1.00; ring=1; alias.csv:53 |
| source | 44100 | 1 | saw | 7 | -43.01 | -40.07 | saw; rate=44100; note=72; feet_ratio=4.00; ring=1; alias.csv:38 |
| source | 48000 | 0 | pulse | 7 | -76.36 | -81.68 | pulse; rate=48000; note=72; feet_ratio=4.00; ring=0; alias.csv:101 |
| source | 48000 | 0 | saw | 7 | -71.14 | -76.97 | saw; rate=48000; note=72; feet_ratio=4.00; ring=0; alias.csv:87 |
| source | 48000 | 1 | pulse | 7 | -43.02 | -40.11 | pulse; rate=48000; note=84; feet_ratio=1.00; ring=1; alias.csv:109 |
| source | 48000 | 1 | saw | 7 | -43.03 | -40.11 | saw; rate=48000; note=72; feet_ratio=1.00; ring=1; alias.csv:93 |
| source | 96000 | 0 | pulse | 7 | -87.14 | -92.78 | pulse; rate=96000; note=84; feet_ratio=4.00; ring=0; alias.csv:159 |
| source | 96000 | 0 | saw | 7 | -83.91 | -88.84 | saw; rate=96000; note=84; feet_ratio=4.00; ring=0; alias.csv:145 |
| source | 96000 | 1 | pulse | 7 | -43.02 | -39.95 | pulse; rate=96000; note=72; feet_ratio=1.00; ring=1; alias.csv:163 |
| source | 96000 | 1 | saw | 7 | -43.03 | -39.95 | saw; rate=96000; note=72; feet_ratio=1.00; ring=1; alias.csv:149 |

## Event controls: physical PANEL, electrical programmer and tone tabs

These rows all use Source Performance. Baseline and omitted-event renders share their complete initial state and preceding events. A score exceeding one remains an unmet criterion even if the omitted-event control also exceeds one; causal evidence does not waive the score. Peak offset is relative to the actual 64-frame-quantized event. First-strike and retrigger columns are intentionally blank for note-off.

| Name | Rate | Coordinate | Event | Baseline score | Omitted score | First strike | Retrigger | Prefix max abs | Peak offset ms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Funky 1 + Funky 2 | 44100 | tone_tabs | single_note_off | 1.62688913455 | 1.6268876393 |  |  | 0 | 1.2244898 |
| Bass Stack | 44100 | original_physical_panel | note_on | 0.567655911129 | 0.0270218485223 | 1.05041697771 | 0.567655911129 | 0 | 7.9818594 |
| Bass Stack | 44100 | electrical_programmer | note_on | 0.294052189432 | 0.29405277458 | 0.114090498048 | 0.294052189432 | 0 | 0.54421769 |
| Funky 1 + Funky 2 | 48000 | tone_tabs | single_note_off | 1.29131001926 | 1.3805030684 |  |  | 0 | 1.5833333 |
| Bass Stack | 48000 | original_physical_panel | note_on | 1.44092402862 | 0.0314353112129 | 1.12329136581 | 1.44092402862 | 0 | 7.7916667 |
| Bass Stack | 48000 | electrical_programmer | note_on | 0.269982960216 | 0.237557903226 | 0.120942636349 | 0.269982960216 | 0 | 7.9375 |
| Funky 1 + Funky 2 | 96000 | tone_tabs | single_note_off | 1.22095816514 | 1.23890355235 |  |  | 0 | 0.875 |
| Bass Stack | 96000 | original_physical_panel | note_on | 1.36530856995 | 0.0730429978427 | 1.10607857225 | 1.36530856995 | 0 | 7.8125 |
| Bass Stack | 96000 | electrical_programmer | note_on | 0.385795006863 | 0.386348194455 | 0.310214964467 | 0.385795006863 | 0 | 5.3020833 |

## Optional FX stress and isolated decimator

FX rows are a separate bright-saw stress fixture, not additional library programs. Slot strings identify all three slots; `000` is dry. Tail ratio is RMS over [11,12] s relative to [4.9,5.4] s, as implemented. A value near zero can be a noise floor, and this report invents no tail threshold.

| Policy | Rate | Rows | Peak dBFS | Gain versus dry min/max dB | Tail ratio min/max dB | Peak row | Nonfinite frames |
| --- | --- | --- | --- | --- | --- | --- | --- |
| legacy | 44100 | 17 | 0.774436 | -20.4896 / 17.7611 | -26.9331 / 0.644633 | bright saw; group=fxbase; rate=44100; fx_slots=555; fx_mix=1; fx.csv:16 | 0 |
| legacy | 48000 | 17 | 0.725615 | -20.8894 / 18.0127 | -25.9587 / 0.559984 | bright saw; group=fxbase; rate=48000; fx_slots=555; fx_mix=1; fx.csv:50 | 0 |
| legacy | 96000 | 17 | 0.822149 | -20.4454 / 18.5212 | -24.2048 / 0.225141 | bright saw; group=fxbase; rate=96000; fx_slots=555; fx_mix=1; fx.csv:84 | 0 |
| source | 44100 | 17 | -7.82851 | -23.0873 / 20.2406 | -17.3281 / -6.13217 | bright saw; group=fxbase; rate=44100; fx_slots=555; fx_mix=1; fx.csv:33 | 0 |
| source | 48000 | 17 | -6.116 | -22.4896 / 20.2422 | -18.8589 / -5.594 | bright saw; group=fxbase; rate=48000; fx_slots=555; fx_mix=1; fx.csv:67 | 0 |
| source | 96000 | 17 | -5.97371 | -22.4888 / 20.2422 | -17.0673 / -5.42615 | bright saw; group=fxbase; rate=96000; fx_slots=555; fx_mix=1; fx.csv:101 | 0 |

Latency measures the decimator in isolation. The Source engine's reported latency also includes the oscillator's 8/internal-factor host-sample term; subtracting the full reported latency from this isolated measurement would falsely report that extra delay as an error. Phase unwrapping uses the existing 63-sample decimator reference, so this is not an independent whole-plugin latency test.

| Policy | Rate | Factor | Rows | Measured decimator delay min/max | Engine reported min/max | Gain min/max dB |
| --- | --- | --- | --- | --- | --- | --- |
| legacy | 44100 | 2 | 6 | 62.9996 / 63.0002 | 63 / 63 | -0.0005 / -0 |
| legacy | 48000 | 2 | 7 | 62.9927 / 63 | 63 / 63 | -0.0032 / 0.0001 |
| legacy | 96000 | 2 | 7 | 62.9987 / 63.0082 | 63 / 63 | -0.0022 / 0.0018 |
| source | 44100 | 2 | 6 | 62.9996 / 63.0002 | 67 / 67 | -0.0005 / -0 |
| source | 48000 | 2 | 7 | 62.9927 / 63 | 67 / 67 | -0.0032 / 0.0001 |
| source | 96000 | 2 | 7 | 62.9987 / 63.0082 | 67 / 67 | -0.0022 / 0.0018 |

## Nonfinite reporting and raw provenance

Phrase exported nonfinite stereo-frame count: **0**. FX: **0**. Alias rows reporting the explicit `999` nonfinite-render sentinel: **0**. Nonfinite numeric CSV cells across all five suites: **0**.

A phrase/FX counter increments once per stereo frame when either channel is nonfinite, before the renderer substitutes zero. Alias reports only a sentinel, not its frame count. The causal and latency CSV schemas do not export audio nonfinite-frame counts; their finite numeric cells do not prove an unavailable count is zero.

| Raw CSV | Rows | Bytes | SHA-256 |
| --- | --- | --- | --- |
| phrase.csv | 566 | 125019 | `41817ac2335b8c1503325ac1fa2004ba420d3c246356b4992aba06c7bbfb2dcc` |
| fx.csv | 102 | 24011 | `fafe6a3a1376bc80fcbac9548632d58181c5b7daa6eca080b6268ba7dbde5edd` |
| alias.csv | 168 | 8315 | `bf0d02f2c4c063e0185b2c950ac5c61c15811a43ad260bc25023dd5bd4d7cf3d` |
| latency.csv | 40 | 1839 | `b9d8b89b1a2ca87417c1aeaa48e5bc807d483729c99d4150e52e35dd8a2e6d30` |
| click-causality.csv | 9 | 1918 | `740111a31b6c6a4a0820cd5266fe53d1bc2e326d20db44022d40042ae638b30f` |

Summarizer SHA-256: `3f1a68208c8c848c645088dae8d1a398190f32c05c1344bbffd3f4c071aa2fd9`.

The JSON retains every raw row, source/contract identities, all extrema ties, target exceedance references and coordinate annotations. Source policy is not PURE; this audit does not render PURE, the curated library, high-carrier ring cases, every control combination, or a DAW session. A successful audit process means rendering completed; it does not imply all targets passed.
