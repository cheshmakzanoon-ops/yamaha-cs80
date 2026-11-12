# Modern LFO implementation and qualification map

Read-only reconciliation of published main
`86b622e6c364ecd99a53177f2ce046342ed5f388`, fetched on 2026-11-12.
This map changes no production source, historical evidence or test results.
It does not mark Phase 6 complete. See [STATUS](STATUS.md) for the broader
project checkpoint and [IMPROVEMENT_PLAN](IMPROVEMENT_PLAN.md) for remaining work.

## Available source is not blanket qualification

`src/core/ModulationLfo.h` implements seven shapes: sine, triangle, saw up,
saw down, square, sample-and-hold and smooth random. Engine uses these sources;
its three rate and three depth parameters are in the 89-lane roster.
Sine retains the historical float phase law; other shapes use double phase.
Random shapes own seeded streams, with selection initialization and reset behavior.
`tests/ModernLfoTests.cpp` contains static-shape, random, live-selection and Engine
hook checks. Their presence alone is not a fresh execution result in this map.

Separately, `src/plugin/PluginProcessor.cpp::createParameterLayout` builds
`AudioProcessorParameterGroup` trees, including contiguous global runs named
from `ParamDef.group` and explicit remaining groups. The old claim that group
metadata is ignored is stale. Source grouping does not certify presentation
in every DAW or imply full host/release qualification.

## Published bounded evidence

| Checkpoint | Evidence and verified scope | Important boundary |
|---|---|---|
| Depth86, `9e8ccd7c262fd50f063f95a9020ca1127d071267` | [Depth](experiments/lfo-depth-automation/README.md): 96 full + 96 frozen83 cases, 2016 completed renders, 384 actual cross-build byte-equal PCM pairs. Fixed sine rates/destinations; isolated depths and combination. | Interrupted attempt preserved/excluded. Candidate hashes are final-only, not pre/post recovery identity. |
| Rate89, `244016b22e290a450707947b221f777f18e22c01` | [Rate](experiments/lfo-rate-automation/README.md): 96 full + 96 frozen86 cases, 2208 renders, 384 actual cross-build byte-equal pairs. Fixed sine/depth/destinations; rates 4–18 Hz. | Scalar history reference is not an ideal oscillator law; no full-cycle/all-phase claim. |
| Combined, `2c38988197356a9e92325fceb5a66793d108118c` | [Simultaneous rate/depth](experiments/lfo-rate-depth-combined/README.md): 96 cases, 1536 renders, 288 scalar/block comparisons with zero maximum error, separate positive contribution witnesses and 96 byte-exact queue-order comparisons. | 384 captures are not cross-build pairs. Fixed sine; not an independent DSP/analog oracle. |
| Static non-sine, `78f00e634705c2f0fd63c1bb5fefcf7824b64ec2` | [Deterministic shapes](experiments/lfo-static-shapes/README.md): triangle, saw up/down and square; 96 cases, 1536 renders, 288 zero-error scalar/block comparisons; rates 16–20 Hz. | Reduced manual/manual matrix, 8-voice capacity with two sounding notes; same shape on active LFOs. No inherited factory/16-voice claim. |
| Core random variable-rate, `7eea83a3b9235688c42f01a0fc200ae331fca103` | [Static random selections](experiments/lfo-random-variable-rate/README.md): 24 cases, 1140300 samples compared plus exact reset replay. Independent reference and saved-data audit; held values exact, smooth error ≤1.49011611938477e-07. | Standalone core, not native host, Engine smoothing/depth/routing or randomness-quality qualification. |
| Core random switching, `86b622e6c364ecd99a53177f2ce046342ed5f388` | [Live random selection](experiments/lfo-random-switching/README.md): 24 cases, 144 switches, 1140300 samples compared plus reset replay. Held values exact, smooth error ≤1.19209289550781e-07; independently reconstructed saved timelines. | Only hold↔smooth-random switches, including selected wrap coincidences. No click-free or native discrete waveform-automation claim. |

The first four native checkpoints use Linux VST3 at 44.1/48/96 kHz in
realtime/offline modes. Combined and static-shape tests use the exact module
built from published rate89 source, not freshly changed synthesizer code.
The two core random checkpoints use three Engine seeds and internal sample
rates 88.2/96/192/384 kHz with finite 0.25-second variable-rate histories.
Read each evidence README for exact controls, hashes and matrix reductions;
counts must not be added into a whole-project qualification score.

## Still outside these results

- Native random-wave rate/depth automation and native waveform switching.
- Mixed simultaneous waveform combinations or all destination combinations.
- Full parameter-range or low-rate qualification inferred from these selected histories.
- General Engine smoothing/routing qualification inferred from standalone random tests.
- All-phase/full-cycle accuracy, statistical randomness quality or click-free switching.
- New GUI/DAW, AU, Windows/macOS, CPU-target, hardware-fidelity or release qualification.

Historical suites were not rerun by these scoped checkpoints; native CI remains
separate. Other Phase 6 items, including MSEGs, routing, tuning/MIDI, editing,
FX and learning tools, require their own current-source and evidence review.
