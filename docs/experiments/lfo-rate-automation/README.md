# Modern LFO rate automation qualification

Candidate 89 appends LFO1, LFO2 and LFO3 rate lanes to the exact 86 prefix.
The verified published baseline is `9e8ccd7c262fd50f063f95a9020ca1127d071267`,
whose tree matches local reviewed depth commit `7df771581c834c3534604fc09c329bbb4e70a593`.
Production edits are limited to three roster entries; DSP/UI and rate metadata
remain unchanged. The historical depth test forward guard now checks the literal 86
prefix, but that historical runtime suite was not rerun.

## Completed results

- Linux VST3, 44.1/48/96 kHz, realtime/offline: 96 full + 96 frozen 86 compatibility cases.
- 1536 candidate + 672 baseline renders = 2208 completed renders.
- 384 actual byte-identical cross-build PCM pairs, each 32,768 bytes (768 capture files).
- Maximum block-partition absolute error: 0.
- All 96 held-window effects positive: 0.000375077 to 0.149792.
- All 96 initial 1024-frame prefixes were byte-exact.
- Destination-off and zero-depth controls independently byte-inert; PURE and Legacy
  protected behavior checked separately. Noqueue and representative preceding 86
  curves, including earlier LFO-depth lanes, matched the frozen baseline.
- Four native binaries match recorded pre-run hashes after all runs; all 9 source
  freezes, 622 frozen source/test identities and 620 retained candidate identities
  remain unchanged. All 54 historical v2 tolerance contracts remain unchanged.

The run completed once with exit 0. `summary.json`, `compatibility-pairs.json`,
`binary-before.json`, `retained-identities.json`, and `sha256.json` preserve exact
counts, identities and actual comparisons. No interrupted or failed attempts
were discarded or retried for this rate qualification.

## Limits

Static sine waveforms, fixed depths/destinations, three isolated paths and their
combination only; physical rates vary within 4..18 Hz. Scalar sample histories,
not an ideal oscillator phase law, are the partition reference. The existing
float sine phase rounding remains unchanged. Returning to an earlier rate does
not restore phase; inactive modulation continues advancing phase.

4096-frame fixtures do not establish all-phase/full-cycle accuracy, random
waveforms, waveform switching or all destinations. No DAW, GUI, AU, Windows/macOS,
CPU target, hardware-fidelity, release or whole-project qualification is claimed.
Historical suites were not rerun; native CI remains separate.
