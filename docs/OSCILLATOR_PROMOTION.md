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

### 2026-11-01 analytic-segment event cache

The next root of the reconstructed oscillator's fold/comparator/shaper-knot
equations is retained as a **forecast** across internal samples within the
same analytic segment. Most samples need no root solve; on a candidate crossing
the engine performs the original one-sample root solve against the actual
current physical state, preserving the original integration interval and
residual event time exactly. The physical charge/reset boundary is still
updated every sample; any frequency, pulse-threshold or gate change invalidates
the forecast, as do idle advancement and state restore. A bounded 125 ms
horizon covers a stopped oscillator without placing infinity into the knot
solver. A forecast horizon expiry never splits a physical integration step.
This is a numerical execution optimization of the same declared circuit
equations, not a new Yamaha measurement or a revised oscillator law.

An earlier implementation instead subtracted each sample interval from the
forecast time and advanced to the accumulated remainder. That changed
**25,023 V2 audio bytes out of a 9.3 MB fixture** (mostly last-bit changes),
despite passing the finite-tolerance alias and waveform gates. It was rejected:
the original strict source-audio gate must still be byte-identical. A detached
pre-VCA `818975f` checkout with the corrected forecast-only implementation,
compiled with the same Release/core and `-O2` compatibility probe, now renders
**byte-identically** to original pin `00ceccf`; both full-fixture SHA-256 hashes
are `08a381f4ac9de1709a9c9197a78baefb12ffac93cd79f76d4b1e92d4f329e195`.
A separate isolated filter-only `90947ed` checkout has the exact same original
V2 hash, proving the initial accidental drift came from the oscillator cache,
not the IG00156 Q-coefficient cache. The later intentional IG00151 VCA
discharge change has a separate V2 pin and is not part of this comparison.

The pre-registered `oscillator_segment_cache` gate in
`tests/parity/tolerances-v2/ws1-voice.json` caps root searches to 400 per 4096
stable ticks at 96 kHz, 261.6 Hz. The forecast-only oscillator took **334**
searches; deliberately invalidating the forecast each tick caused **4430**
and the gate failed (42 checks, one expected failure). This red/green mutation
proves that eliminating the per-tick root searches remains enforced while the
separate strict-V2 fixture protects every audio byte.
The independent `cs80_experimental_oscillator_tests` and promoted-engine
`cs80_oscillator_promotion_tests` both pass; the latter still measures
**−88.31 dB saw** and **−88.84 dB pulse** non-harmonic energy at 48 kHz, with
3.4997–3.5000 Vpp pin-7 anchors and 3.0000 Vpp sine. CPU comparisons are
reported separately after three repeatable 48 kHz / 256-frame benchmark rounds.

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
