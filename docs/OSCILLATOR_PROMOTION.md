# Reconstructed oscillator in the circuit engine

Plan step 1.2 of [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md), 2026-10-31.
`src/experimental/ReconstructedOscillator.h` moved to
`src/core/ReconstructedOscillator.h` and now drives the circuit engine
(Source Performance and Isolation). The legacy policy keeps the PolyBLEP
`Oscillator` and its arithmetic byte-for-byte. The model itself (profile
`w3-current-fold-v1`) is unchanged and remains an engineering-inferred nominal
instrument ([proposal](OSCILLATOR_REALTIME_PROPOSAL.md),
[checkpoint](OSCILLATOR_EXPERIMENTAL_CHECKPOINT.md)).

## What changed

| Item | Where | Class |
| --- | --- | --- |
| Selection: circuit policies tick `Rank::wsc` instead of `Rank::osc` | `Engine.cpp` rank chain | engineering |
| Saw + pulse to the filters, sine to the VCA bypass, all from one charge/reset trajectory | `Engine.cpp` | source-informed topology (M41) |
| One fixed adapter for every pin: 1 / 1.5 V (the 3 Vpp comparator plateau maps to the legacy +/-1 pulse) | `kWscEngineUnitsPerVolt` | **inferred** adapter, never per-waveform normalization |
| Panel PW / PWM to comparator threshold: 1.525 (1 - 2 duty) V | `panelThresholdVoltsInferred` | **inferred** (existing proposal adapter) |
| Cold start: the legacy RNG draw becomes the start phase; zero audio prehistory | `startVoice`, `reset` | convention; RNG stream unchanged |
| Sine: folded triangle -> four-segment PWL shaper (`sin(pi x / 2)` knots), replaces sin + 3.5 % third harmonic | oscillator | **inferred** shaper ordinates (proposal 2.3) |
| One shared immutable residual basis per (internal rate, coupling RC) | `OscillatorExponentialBasis`, `OscillatorBasisRegistry` | numerical |
| Per-tick savings | `tick()` | numerical, see below |

The shared basis lives in a fixed static pool of 16 reference-counted slots:
no heap allocation at setup or in the callback (the existing zero-setup-
allocation check still passes), slots are recycled when unused, and exhaustion
makes `prepare()` fail transactionally. Per-instance storage fell from about
22 KB to 5,984 bytes; one 16,440-byte basis is shared by all 16 rank
oscillators.

Per-tick savings: (1) the node values at the end of a tick are reused at the
next tick's start when nothing changed (bit-identical); (2) control events
that provably add no residual are skipped: a threshold move that does not
cross the comparator input, and a frequency change during the reset plateau
(bit-identical); (3) a frequency change whose slope corner contributes less
than `kNegligibleCornerVolts = 1e-6 V` keeps the new frequency but omits that
corner's antialias residual (bounded approximation: one-ulp float pitch steps
and slow drift). Against the pre-optimization header, steady, PWM-only, gate
and glide scenarios are bit-identical; drift-like and vibrato scenarios differ
by at most 3.3e-7 V and 7.1e-7 V (-133/-127 dB re the 1.5 V pins). Instruction
counts per tick (callgrind, 261.6 Hz, saw+pulse): steady 1076 -> 885,
per-tick tiny frequency change 2422 -> 1076, per-tick PWM 1511 -> 1000.

Latency: the oscillator's linear-phase kernel delays its audio by 8 internal
samples (83 us at 2x) relative to envelopes and controls. The reported host
latency is unchanged (ws2 owns latency reporting); the skew is below 10 % of
the fastest 1 ms attack. Noise is not delayed.

## Results

Tool `cs80_engine_audit --suite alias` (source policy, ring off) and
`tests/OscillatorPromotionTests.cpp`:

| Metric | Before (6719ce4) | After 1.1b (c9f8cf0) | After 1.2 |
| --- | --- | --- | --- |
| Worst saw non-harmonic, 48 kHz | -51.07 dB | -51.02 dB | **-88.3 dB** |
| Worst pulse non-harmonic, 48 kHz | -53.55 dB | -53.55 dB | **-88.8 dB** |
| Worst saw, 44.1 / 96 kHz | -50.67 / -67.35 dB | -50.61 / -67.16 dB | -88.1 / -90.1 dB |

The pre-registered gate (-70 dB, 48 kHz saw and pulse rows) is met at the
current fixed 2x. Measured with a temporary local change (cascaded copies of
the existing 255-tap halving decimator, scratch copy, never committed):

| Internal factor | Worst saw / pulse at 48 kHz | Worst saw / pulse at 44.1 kHz |
| --- | --- | --- |
| 2x (current) | -87.4 / -88.8 dB | -88.1 / -88.3 dB |
| 4x | -89.8 / -89.2 dB | -89.9 / -88.9 dB |
| 8x | -89.8 / -89.2 dB | -89.9 / -88.9 dB |

(2x row from the audit before the per-tick savings; the gate test after them
gives -88.3 / -88.8 dB.) **The lowest factor that meets -70 dB is 2x**; 4x and
8x are at the analysis floor. The oscillator failed only ahead of the old
clipping `Svf`; ahead of the nodal stage, whose in-loop nonlinearity is
integrated (see [IG00156_NODAL_MODEL.md](IG00156_NODAL_MODEL.md)), the
upper-internal-band images do not mix down measurably. The filter's own
top-octave small-signal accuracy, not aliasing, is what improves with 4x/8x.

Pin-level anchors at the 261.6 Hz reference, engine rates 88.2/96/192 kHz:
pin 7 3.4997-3.5000 Vpp, WSC sine 3.0000 Vpp. Third harmonic at the engine
output (sine only, 220 Hz): legacy 3.53 %, circuit 0.32 %.

CPU (`cs80_source_factory_benchmark --rate 48000 --block 256`, 8 held voices,
11 tab pairs, mean of the warmed-held median RTF, minimum of three interleaved
rounds on a shared 4-core host at load 10-13):

| Build | Circuit engine (isolation) | Legacy |
| --- | --- | --- |
| 6719ce4 baseline | 0.296 | 0.235 |
| c9f8cf0 nodal filter | 0.358 | 0.233 |
| step 1.2, before per-tick savings | 0.684 | 0.236 |
| step 1.2 | **0.518** | 0.239 |

The oscillator costs about 0.16 RTF for 16 instances at 96 kHz. That is
materially above the circuit-engine budget: `productionRealtimeQualified`
stays false until the planned post-integration SoA/CPU pass. The work per tick
is spread over node evaluation, event search and the decay polynomial; there is
no single hotspot left for an exactness-preserving fix.

## Tests and mutation checks

`cs80_oscillator_promotion_tests`: alias gate (same fixture and analysis as
the audit), pin anchors, shared-basis identity/reference counting/size,
bit-identical output through a shared basis, third harmonic.
`cs80_experimental_oscillator_tests` (1,418,466 checks) passes unchanged.
The PRA-wah and output-tap pipeline oracles now run the reconstructed sine for
the isolation policy (`tests/CircuitSineSource.h`), matching the engine to
2e-7; they pinned the old 3.5 % sine.

| Mutation | Detected by |
| --- | --- |
| PolyBLEP restored in the circuit engine | alias gate (saw and pulse), third-harmonic check |
| Registry never shares (private basis per prepare) | shared-basis identity |

## Intentional circuit-engine sound changes

Rising WSC saw polarity, comparator pulse whose edges follow the actual
coupled ramp, phase-coherent shaped sine (approaching -cos of the saw phase),
the reset plateau in the saw, coupling-capacitor memory, no 3.5 % third
harmonic, 8 internal samples of oscillator delay.

## Still open

- CPU: SoA/SIMD across the 16 oscillators (post-integration pass).
- Idle advancement and held key voltage belong to ws3 (ContinuousVoiceCircuit).
- Host latency notification of the 8-sample oscillator delay (ws2).
- The uncertainty sweep of the proposal (reset time, plateau, shaper segments).
