# Core integration checkpoint: 2026-11-10

Runtime source: `215b900` (waiting-key tuning admission fix). The subsequent
README-only provenance correction does not change the tested runtime or tests.

A fresh Release configuration with `CS80_BUILD_PLUGIN=OFF` and
`CS80_BUILD_TESTS=ON`, using Ninja, completed all 259 build steps. CTest then ran
all 145 registered tests, two at a time, in one uninterrupted invocation:

- **145/145 passed, zero failures**, 517.49 seconds total wall time.
- No test was skipped, missing or substituted by a later retry.
- The library audit completed successfully in 186.99 seconds.
- Tuning/MIDI, controller reset, scanner routing, release ownership, envelope,
  filters, modern modulation/FX, memory experiments, packaging preflights and
  the remaining registered core checks passed in the same run.

This broadens the focused evidence accompanying the recent MTS, channel-mode,
unmapped Sustain-II and waiting-key changes. It is a core-only local integration
checkpoint. It does **not** establish native plugin/DAW validation, sanitizer or
leak qualification, installation/signing readiness, hardware equivalence, or the
project's realtime CPU targets. Test durations are not performance benchmarks.
