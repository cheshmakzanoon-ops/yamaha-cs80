# WSC preregistration — unchanged-source comparator and negative controls

**Date:** 2026-11-01 UTC. **Scope:** isolated experimental branch, source tree
`7dd159d491012fd47b46c0aa651e86ea959ca289` from feature parent
`7c864f266a939aa2a42422440dd4c1c0cabe3814`. No production DSP,
workflow pins or audio baselines were edited. `g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0`,
Linux x86-64, `-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic`.

The frozen independent header differs from the production original only in
five leading explanatory comment lines and the opening/closing C++ namespace:
`diff -u src/core/ReconstructedOscillator.h tests/reference/WscLoopOriginal.h`.
Its production original SHA-256 is
`c9bb458f238ea6cb0bf74444a1a9459441da846bbf3ab98598848b45b147034c`.

Reproduction, from the experiment worktree root (the commands deliberately
need neither CMake nor a preexisting core library):

```sh
g++ -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Isrc -Itests \
  tests/OscillatorFastPathOracleTests.cpp -o /tmp/cs80_wsc_prereg_oracle
/tmp/cs80_wsc_prereg_oracle tests/cpu/wsc-steady-fastpath.json

g++ -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Isrc -Itests \
  -DCS80_WSC_ORACLE_MUTATE_STATE tests/OscillatorFastPathOracleTests.cpp \
  -o /tmp/cs80_wsc_prereg_mutation_state
/tmp/cs80_wsc_prereg_mutation_state tests/cpu/wsc-steady-fastpath.json

g++ -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Isrc -Itests \
  -DCS80_WSC_ORACLE_MUTATE_EVENT tests/OscillatorFastPathOracleTests.cpp \
  -o /tmp/cs80_wsc_prereg_mutation_event
/tmp/cs80_wsc_prereg_mutation_event tests/cpu/wsc-steady-fastpath.json
```

All three compilations succeeded without warnings. Each execution ran the
fixed 56,576-tick / four-rate fixture. Exact observations:

| Build | Mismatches / field comparisons | Events | Quiet ticks | Searches | Exit |
| --- | ---: | ---: | ---: | ---: | ---: |
| Unchanged current vs frozen old-loop | 0 / 44,294,052 | 12,038 | 45,311 | 19,630 | **0** |
| One-ULP state charge witness | 1 / 44,294,052 | 12,038 | 45,311 | 19,630 | **1** |
| One-ULP real-event time witness | 1 / 44,294,052 | 12,038 | 45,311 | 19,630 | **1** |

The state witness prints:

```text
BIT MISMATCH state.charge: 3fe43e14b3f20329 != 3fe43e14b3f2032a
WSC oracle FAIL: bit mismatch budget exceeded
```

The observed event witness prints:

```text
BIT MISMATCH events.absoluteSeconds: 3f5a5f26e79a464b != 3f5a5f26e79a464c
WSC oracle FAIL: bit mismatch budget exceeded
```

These are valid **test-comparator negative controls**, not mutations of the
production algorithm or proof of a proposed fast path. No fast path has been
implemented or evaluated. The managed executor has `g++` but lacks CMake and
Ninja; therefore the requested stock engine benchmark, full core/compatibility
suite and same-host three-before/three-after performance evidence are **not
available** here. The preregistration explicitly prohibits shipping an audio
change on the strength of this standalone comparator alone.
