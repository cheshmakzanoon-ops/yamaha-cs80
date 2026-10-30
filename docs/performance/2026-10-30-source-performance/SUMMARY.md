# Source Performance: measured controller/FX cost

**Typical dry and modest-FX cost leaves useful core-only margin on this machine; observed deadline overruns prevent a real-time guarantee.** Source Performance held medians were **0.236–0.274 RTF dry**, **0.246–0.294 with live MPE + chorus/reverb**, and **0.244–0.296 with live MPE + phaser/delay/drive**. These are different performance configurations, not an isolated FX cost attribution.

Measured DSP: `d7b6e8510e42cf8bd7f35edad49c81795bc48801`, clean immutable core snapshot; GCC 14.2.0 `-O3`; cloud AMD EPYC 9V74; 2026-10-30. [Methodology and reproduction](METHODOLOGY.md), [full aggregate results](results.csv), [context](context.txt), [metadata](metadata.json), [source/harness hashes](source-evidence.txt).

The [932c6d4 isolation baseline](../2026-10-30-source/SUMMARY.md) remains untouched and separately labelled. This run adds **Source Performance**, which leaves expressive controllers and selected FX active; it does not relabel Isolation or alter DSP.

## Eight held notes, both ranks, all 22 source tabs

Each row below spans 11 paired selectors / **11,264 callbacks**. RTF = callback wall duration / callback audio duration; **1.0 uses the entire callback budget**. The profile recipes and block-rate MPE/CC1/11/64/74 stimulus are specified in the methodology.

| Source Performance profile | Host Hz | Frames | Median RTF range | Worst p99 RTF | Worst callback ms | Budget exceedances / 11,264 |
|---|---:|---:|---:|---:|---:|---:|
| Dry | 44100 | 128 | 0.236–0.253 | 0.613 | 6.243 | 5 |
| Dry | 44100 | 256 | 0.242–0.254 | 0.517 | 5.813 | 1 |
| Dry | 48000 | 128 | 0.259–0.274 | 1.158 | 9.243 | 22 |
| Dry | 48000 | 256 | 0.259–0.274 | 0.656 | 8.476 | 6 |
| MPE + chorus/reverb | 44100 | 128 | 0.246–0.281 | 0.954 | 3.885 | 11 |
| MPE + chorus/reverb | 44100 | 256 | 0.247–0.279 | 0.536 | 7.758 | 1 |
| MPE + chorus/reverb | 48000 | 128 | 0.268–0.294 | 0.656 | 4.452 | 14 |
| MPE + chorus/reverb | 48000 | 256 | 0.266–0.288 | 0.598 | 29.143 | 7 |
| MPE + phaser/delay/drive | 44100 | 128 | 0.244–0.260 | 0.465 | 5.276 | 9 |
| MPE + phaser/delay/drive | 44100 | 256 | 0.246–0.272 | 0.614 | 14.891 | 3 |
| MPE + phaser/delay/drive | 48000 | 128 | 0.271–0.296 | 0.614 | 6.044 | 7 |
| MPE + phaser/delay/drive | 48000 | 256 | 0.270–0.291 | 0.631 | 7.318 | 2 |

Across source-held rows: **88 budget exceedances / 135,168 callbacks**. The two expressive profiles contribute **54 / 90,112** of those; their median cost is modest here, but their worst individual durations are not.

## Outliers remain part of the result

- The largest held callback was **29.143 ms** at 48 kHz/256, Source Performance chorus/reverb, selector pair 3 (Flute / Brass 3). Its budget was 5.333 ms (**5.464 RTF**); the same case had median 0.288 and p99 0.557. A good median/p99 does not erase that observed overrun.

- The highest individual RTF across all phases was **5.710**, a **16.573 ms** chorus/reverb release callback at 44.1 kHz/128 (2.902 ms budget), pair 8 (Guitar 1 / Guitar 2).

- Worst source-held p99 was **1.158 RTF**, a dry 48 kHz/128 case. Across all modes/phases there were **127 exceedances / 256,402 callbacks**. Nothing is discarded as an outlier or silently reclassified as a scheduler artifact.

### CPU-clock context, not outlier classification

Thread and process CPU clocks are sampled only at phase boundaries, outside the callback timing. Their scopes include input generation and checks; summed timed wall excludes them. The held-profile totals below illustrate the distinct measurements, not a per-callback causal diagnosis.

| Source Performance held profile | Timed wall sum s | Process CPU s including checks | Thread CPU s including checks |
|---|---:|---:|---:|
| Dry | 50.139051 | 50.158817 | 50.158807 |
| MPE + chorus/reverb | 52.815326 | 52.836126 | 52.836125 |
| MPE + phaser/delay/drive | 52.324592 | 52.351731 | 52.351727 |

Aggregate CPU tracked summed wall closely in this run. Without per-callback CPU traces/profiling, this **does not identify the cause of a particular spike** or justify discounting it. No DAW/audio-driver dropout trace was collected.

## Separately labelled current-commit anchors

Pairs 0, 1 and 10 only, at the same four rate/buffer settings, with dry neutral settings. These are not all-22 anchor sweeps, nor reruns of the older commit.

| Mode | Held cases / callbacks | Median RTF range | Exceedances |
|---|---:|---:|---:|
| isolation | 12 / 12,288 | 0.247–0.281 | 6 |
| legacy | 12 / 12,288 | 0.201–0.243 | 9 |

## Controller, tail and numerical evidence

- **156 cases / 400 rows / 256,402 timed callbacks** completed. Accepted render policy/source routes and all unchanged-selector source-compilation checks passed.

- **768,768 MPE member-state checks** validated channel identity, pressure and member bend. **2,596,176 controller calls** were exercised. CC11 actually moved the source PRA target; CC74 was dispatched for every member.

- **5,984 sustain-hold callbacks** verified all eight members remained pedal-held after their note-offs. CC64 up then released them. These are actual sustain semantics, not merely sending CC64 while keys remain held.

- All **156 release rows** reached zero active voices. Expressive profiles observed at least three seconds after CC64 up; final FX-window block peaks ranged from approximately **1.33e-16 to 1.02e-6**, not universally zero. This is **not a complete-silence claim or export tail bound**.

- Held, sustain-hold and release phases had respectively **103 / 159,744**, **7 / 5,984**, and **17 / 90,674** budget exceedances, including the separately labelled anchors. Release distributions mix active-voice and FX-only processing.

- **Zero nonfinite or absolute-at/above-1 sample values** in measured windows. Largest peak was **0.188156** at the fixed master 0.12. Expressive-profile held peaks were at most 0.034254 (chorus/reverb) and 0.039803 (phaser/delay/drive); expression/wah also changes level, so these are not isolated FX-gain comparisons. Other master settings, FX/patch combinations and live transients still need headroom.

- Performance preflight passed **15 cases / 36 rows**; the unchanged default-suite CLI also passed **11 cases / 19 rows** (integrity-only preflight after the exclusive measurement window). Existing-output and invalid-suite requests returned exit 2 without overwriting or creating invalid output. Source/harness hashes match the measured copy; old baseline evidence has no diff.

Run `python3 docs/performance/2026-10-30-source-performance/verify_results.py` to recheck exact matrix coverage, explicit mode labels, controller/voice/tail integrity and callback totals. Timing exceedances are printed as observations, not pass/fail criteria.

## Practical interpretation

The measured typical cost supports trying this opt-in mode with a modest FX chain on the intended host. **Start host validation at 44.1/48 kHz and 256 frames**, leave output and CPU headroom, then measure actual underruns and responsiveness before lowering the buffer. Even 256-frame cloud results had large isolated overruns; this is a test starting point, not a guaranteed support configuration.

This core-only test excludes the JUCE/plugin wrapper, APVTS/MIDI parsing, UI, driver, DAW graph and other plugins. It does not qualify 64 frames, 96/192 kHz, every FX/control combination, many instances, a different CPU or a long session. No experimental oscillator or hidden DSP optimization is enabled.
