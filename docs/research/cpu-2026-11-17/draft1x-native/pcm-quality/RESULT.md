# Native Draft1x PCM interpretation: descriptive, not fidelity acceptance

Read-only audit of study44090859110 retained PCM. No renderer, benchmark or frozen source was executed or modified. Independently checked108 timed receipts and1,215,000 event rows, exact source selectors/rates/frame counts, canonical SHA256/byte lengths, within-arm repeat identity and allsix stock/off identities. Twelve unique PCM files were decoded; all34,560,000stereo frames are finite. Both channels were preserved throughout; in this centered-pan workload left/right results happen to match.

Alignment is strictly stock[t+67] versus Draft[t+8], using declared latencies and2,879,933common logical frames. No fitted delay, gain, phase correction, EQ or favorable-window selection. Declared group-delay alignment is not exact frequency-dependent phase alignment, so differences cannot be called distortion or alias energy.

| Row | Source pair | Relative waveform RMS difference | Draft/stock RMS level |
|---|---:|---:|---:|
| Held |0|26.709%|98.348%|
| Held |4|9.452%|99.778%|
| Held |7|8.006%|99.973%|
| Held |10|11.002%|99.844%|
| Pressure triangle |4|9.478%|99.780%|
| Fixed release cycles |4|12.450%|99.822%|

These are per-channel values (identical here), not THD, perceptual distances or percentages of audible error. RMS loudness proxies remain close, but the waveforms are not identical. The measurements alone do not separate phase changes from spectral/temporal coloration and cannot establish whether the tradeoff is acceptable to a listener.

All15 pressure cycles, all10 held/release cycles and all20 release-row onset/offset neighborhoods are retained in result.json. Pressure-cycle relative differences range6.879–15.648%. No tail was stopped when quiet. Peak output across these sixrows is0.147668stock/0.149046Draft, occurring in the release-cycle workload; no nonfinite samples occurred. Peak adjacent-sample differences are reported but are not independently a click/alias/quality score.

## Important tail-boundary interpretation
Nominal latency alignment shifts the comparison window forward in each rawcapture. For the firstnine late-release windows, the shifted end includes the earliest rawsamples following the next scheduled noteon. Their tiny nonzero RMS must not be presented as longer decay or noise introduced by Draft. A disclosed supplementary raw-window check ending exactly BEFORE the next noteon shows allten finalone-second release windows at roughly1.41e-26–1.51e-26 RMS for botharms. The final aligned tail, which has no nextnoteon, is likewise near1.46e-26. Both raw and nominal-aligned results are preserved, not substituted selectively.

The final common aligned window excludes67frames (1.396ms at48k) solely to keep botharms within the same retained file. Intermediate held/release windows retain every declared frame under that alignment.

## Limits
Only this48k/256, eight-voice six-workload source-factory subset is covered. No alias spectrum, fullfilter response grid, fullbank/MPE/FX/EXT, independent listening, hardware fidelity, plugin/PDC or live-switch acceptance follows. CPU findings are audited separately; the measured CPU saving does not itself validate sonic quality. Default production behavior remains unchanged.

Scripts: audit.py, verify_events.py, raw_tail_boundary_check.py. Detailed outputs: result.json, events-verified.json, raw-tail-boundary-check.json. Artifact inputs were read only; results reside outside native-study.
