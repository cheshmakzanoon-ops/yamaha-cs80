# Stop: identical-binary calibration is unusable

All four fixed A/A pairs (eight native runs) completed exit0 with exact audio/workload parity. Both logical roles used the same baseline executable, not the FIR candidate. Total CPU ratios:1.05124110,0.97446782,0.94690478,0.98505909. Source-held median-of-case ratios:1.05264881,1.01283182,0.94358796,0.97139531. Maximum aggregate deviation5.6412%; the predeclared±2%aggregate guard failed. Numerous individual cases also exceeded±10%. These variations are measurement dispersion of an identical binary, not FIR changes.

The fixed protocol stops here. No candidate timing, low-load timing, repeat calibration, adaptive rounds or publication. This does not reject FIR correctness or prove its cost; whole-engine benefit remains unmeasured because this calibration was unsuitable for the modest-benefit screen. No bias subtraction or retrospective threshold change. All observations retained, including favorable/unfavorable ratios and order strata.

Initial preflight failed before timing because the compile database held both production-core and recovery-test entries for Ig00156Batch.cpp. Original runner/log/exit preserved. Reviewed repair selected exact production target output, leaving workload/metrics/protocol unchanged; no measured calibration was discarded.

Independent final raw audit completed:8 terminal0 runs,288 rows,144 case ratios, identical-binary roles/order, workload, clocks and current executable/CSV hashes verified.39 case-pair ratios exceeded±10%. Every case below; raw absolute seconds/RTF and all4 ratios remain in calibration-analysis.json.

| Case | Median logical B/A (same binary) | Min | Max | AB median | BA median |
|---|---:|---:|---:|---:|---:|
| source_performance / mpe_phaser_delay_drive / 0 / warmed_held | 0.98673295 | 0.91709499 | 1.04574986 | 0.98142242 | 0.98673295 |
| source_performance / mpe_phaser_delay_drive / 0 / sustain_pedal_hold | 0.97288364 | 0.89937774 | 1.01304752 | 1.00662868 | 0.92246759 |
| source_performance / mpe_phaser_delay_drive / 0 / release_tail | 1.00755776 | 0.95112900 | 1.31221503 | 0.97855121 | 1.16067857 |
| source_performance / mpe_chorus_reverb / 0 / warmed_held | 1.04837957 | 0.94358796 | 1.14926081 | 1.00472914 | 1.09007482 |
| source_performance / mpe_chorus_reverb / 0 / sustain_pedal_hold | 0.98373049 | 0.90773524 | 1.03988693 | 0.93712713 | 1.02041444 |
| source_performance / mpe_chorus_reverb / 0 / release_tail | 0.98488427 | 0.94958188 | 1.02735522 | 0.96098464 | 1.01236818 |
| legacy / paired_tones / 0 / warmed_held | 1.02566774 | 0.74279444 | 1.10403032 | 0.86366250 | 1.08541762 |
| legacy / paired_tones / 0 / release_tail | 1.04737399 | 0.80947934 | 1.07366158 | 0.93064998 | 1.05829447 |
| legacy / paired_tones / 1 / warmed_held | 0.98510960 | 0.81619320 | 1.03484177 | 0.92551748 | 0.98510960 |
| legacy / paired_tones / 1 / release_tail | 0.96652630 | 0.88931162 | 1.03518397 | 0.92835913 | 1.00041497 |
| source_performance / mpe_phaser_delay_drive / 10 / warmed_held | 1.00730637 | 0.80775140 | 1.02461047 | 0.91618093 | 1.00730637 |
| source_performance / mpe_phaser_delay_drive / 10 / sustain_pedal_hold | 1.00885063 | 0.86822422 | 1.03616894 | 0.95219658 | 1.00885063 |
| source_performance / mpe_phaser_delay_drive / 10 / release_tail | 0.99873297 | 0.99687414 | 1.03406230 | 0.99753317 | 1.01666801 |
| source_performance / mpe_chorus_reverb / 1 / warmed_held | 1.04163633 | 0.93343580 | 1.05835500 | 0.97934551 | 1.05818622 |
| source_performance / mpe_chorus_reverb / 1 / sustain_pedal_hold | 1.02722389 | 1.00684616 | 1.12407547 | 1.02722389 | 1.06546081 |
| source_performance / mpe_chorus_reverb / 1 / release_tail | 0.94323647 | 0.87675258 | 1.01824838 | 0.94750048 | 0.94323647 |
| isolation / paired_tones / 1 / warmed_held | 0.99336150 | 0.95294351 | 1.03717901 | 0.99506126 | 0.99336150 |
| isolation / paired_tones / 1 / release_tail | 0.99338537 | 0.96495121 | 1.14228971 | 0.99338537 | 1.05362046 |
| source_performance / paired_tones / 1 / warmed_held | 1.04015898 | 0.97139531 | 1.07440761 | 1.06352821 | 0.99953223 |
| source_performance / paired_tones / 1 / release_tail | 1.00613137 | 0.83849286 | 1.14511324 | 0.93360472 | 1.06432970 |
| source_performance / paired_tones / 0 / warmed_held | 1.03944428 | 0.90867435 | 1.16384026 | 1.03944428 | 1.03625731 |
| source_performance / paired_tones / 0 / release_tail | 1.02632474 | 0.98828202 | 1.32241643 | 1.01113980 | 1.17053416 |
| source_performance / mpe_chorus_reverb / 10 / warmed_held | 0.93890690 | 0.80944784 | 1.00537594 | 0.90323778 | 0.94308100 |
| source_performance / mpe_chorus_reverb / 10 / sustain_pedal_hold | 0.99981472 | 0.97186270 | 1.03122106 | 1.01772455 | 0.98363205 |
| source_performance / mpe_chorus_reverb / 10 / release_tail | 0.95550271 | 0.91808085 | 1.03581039 | 0.98914168 | 0.94330665 |
| source_performance / mpe_phaser_delay_drive / 1 / warmed_held | 0.91235317 | 0.72702794 | 1.06982407 | 1.00401914 | 0.80676004 |
| source_performance / mpe_phaser_delay_drive / 1 / sustain_pedal_hold | 0.88108596 | 0.75654723 | 1.37121371 | 1.14423879 | 0.80072764 |
| source_performance / mpe_phaser_delay_drive / 1 / release_tail | 0.92881586 | 0.80966146 | 1.00395546 | 0.95206353 | 0.88356079 |
| legacy / paired_tones / 10 / warmed_held | 0.96636461 | 0.91196942 | 1.01491560 | 1.00882562 | 0.92098150 |
| legacy / paired_tones / 10 / release_tail | 1.08266692 | 0.86569951 | 1.32600893 | 0.91538196 | 1.26313918 |
| source_performance / paired_tones / 10 / warmed_held | 0.98883580 | 0.92510744 | 1.59556770 | 1.31441331 | 0.93476006 |
| source_performance / paired_tones / 10 / release_tail | 1.01258980 | 0.89272912 | 1.58818668 | 1.29891844 | 0.95412926 |
| isolation / paired_tones / 10 / warmed_held | 0.97144117 | 0.88130103 | 1.01967958 | 0.99959740 | 0.92233408 |
| isolation / paired_tones / 10 / release_tail | 1.02341498 | 0.65379020 | 1.17964266 | 1.11024242 | 0.82988899 |
| isolation / paired_tones / 0 / warmed_held | 0.97901013 | 0.92780218 | 1.44163272 | 1.21012861 | 0.95359898 |
| isolation / paired_tones / 0 / release_tail | 1.04319899 | 0.87509811 | 1.13199588 | 1.12335794 | 0.92338804 |
