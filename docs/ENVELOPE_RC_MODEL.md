# Circuit-engine envelope RC model

Phase 1.3 of [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md). Applies to the circuit
engine (Source Performance and Isolation policies) only: the double-precision
`tickReconstruction` paths of `Adsr` and `FilterEnv` in `src/core/Dsp.h`. The legacy
float paths keep their historical law byte-for-byte.

**Evidence class: engineering inference.** The IG00152/IG00159 pages document the
ranges (1 ms–1 s attack, 10 ms–10 s decay/release) and that a higher timing control
voltage shortens a stage, but not the charging-current law or the sequence
thresholds ([ENVELOPE_TARGET_TRANSITION_REVIEW.md](ENVELOPE_TARGET_TRANSITION_REVIEW.md)).
This model replaces unexplained heuristics with the simplest physical one: a single
capacitor charged through a resistance, with a comparator ending the attack. It is
not a measured law.

## Law

| Stage | Model | Time meaning |
| --- | --- | --- |
| Decay, release, live sustain/IL tracking | `v ← target + (v − target)·exp(−ln(1000) / (T·fs))` | `T` is time to −60 dB of the remaining span |
| Attack (amplitude 0→1, filter IL→AL) | charge toward `V∞ = start + 1.5·(peak − start)`; comparator stops at `peak` | `τ = T / ln 3`, so the peak is reached at exactly `T` from any start level |

Constants: `kRcSettleLog = ln 1000`, `kRcAttackOvershoot = 1.5`, `kRcAttackLog = ln 3`.
The 1.5 overshoot ratio gives the concave, fast-then-slowing analog attack and a
finite stage; it is an inferred choice, not a recovered resistor ratio.

## What changed against the previous circuit-engine law

- The `exp(−6/…)` exponent (−52 dB, despite comments saying −60 dB) is now
  `ln 1000`; decays and releases are about 13 % shorter for the same panel time.
- The ×5 "bend" windows (35 ms decay, 45 ms amplitude release, 40 ms filter release)
  are removed. They changed slope abruptly at a fixed time and had no circuit basis.
- The attack `≥ 0.999` / `1.2·T + 1 ms` timeout is replaced by the comparator.
- A moved AL during attack restarts the charge from the present level.
- **Voice reuse:** a sounding tail reused by a new key keeps its filter-envelope
  level and charges from there toward AL, as the amplitude envelope already did
  ([ACTIVE_TAIL_CONTINUITY.md](ACTIVE_TAIL_CONTINUITY.md)). It no longer jumps to IL.

## Verification

- `tests/EnvelopeRcModelTests.cpp`: attack peak at `T` within one sample at
  44.1–192 kHz from several start levels and in both filter directions; concave
  monotonic charge; −60 dB at `T` for decay and release; no time-window bend; filter
  envelope continuity on reuse. Mutation-checked (old exponent, restored bend,
  reset-to-IL each fail).
- Existing analytic tests updated to the new law: `EnvelopePrecisionTests`,
  `LiveEnvelopeTimingTests`, `tests/test_parity_capture.py`.
- `cs80_engine_audit --suite phrase --quick`, 48 kHz, source policy: worst
  `click_steal` 1.17 → 0.90, `click_retrigger` 1.06 → 0.96, peak −2.9 → −2.2 dBFS.
- Legacy default and tails probes byte-identical; circuit-engine audio V2 gate
  re-pinned to the commit introducing this model.
