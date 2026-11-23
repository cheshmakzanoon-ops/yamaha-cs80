# Agent 25 — Wind description and body-decay help

Updated only `src/core/curated/Fx.inc` and the relevant paragraphs of
`docs/PRESET_GUIDE.md`.

- Wind now describes its faster upper airy gust entering before the broader,
  slower low bed. Its description also states that the shorter upper release
  subsides first and describes the shallow periodic SUB movement neutrally.
- Added a short Wind / Surf / Thunder playing guide matching their measured
  entrance order. It identifies them as synthesized noise gestures.
- Explicitly states that Partial decay has no effect on sustained bowed,
  wind, brass or organ bodies. The ordinary rank envelope and key release
  still control complete-note decay and damping.

The Wind correction follows agent 22's isolated-rank captures: upper RMS peak
0.75 s versus low 1.95 s, with half-peak crossings 0.35 s versus 0.75 s, in
that C4 / velocity 0.8 fixture. `InstrumentBody::stringLoss()` confirms the
sustained-body loss behavior.

Verification: `git diff --check` passes. A source comparison replacing string
literals confirms every non-string byte is unchanged, and exactly one string
literal changed. No parameter values, names, tags, order, DSP, tests, commits
or remote refs were changed.
