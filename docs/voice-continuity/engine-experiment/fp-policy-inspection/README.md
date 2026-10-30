# Subsequent untimed FP-policy inspection

This is **not a timing rerun** and not a replacement for the first result.
The first benchmark did not record MXCSR or explicitly set FTZ/DAZ. Source
inspection found no core/bridge FP-policy setter. Production `processBlock`
uses JUCE `ScopedNoDenormals`, so the scopes are not aligned.

`inspect.cpp` was compiled on the same Linux x86-64 environment, linked to the
exact four preserved namespace-separated bridge/core objects. It reads MXCSR
at fresh process entry, after setup of a 48 kHz/128 post-retirement PANEL
fixture, and after one **untimed** callback in each arm. The captured output
shows FTZ=0/DAZ=0 throughout; changing `0x1f80` to `0x1fa0` is an exception
status-flag change, not an FTZ/DAZ change.

```sh
g++ -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Werror \
  -Idocs/voice-continuity/engine-experiment \
  docs/voice-continuity/engine-experiment/fp-policy-inspection/inspect.cpp \
  /tmp/cs80-engine-continuity-replay-first/benchmark-build/*.o \
  -o /tmp/engine-fp-policy-probe
/tmp/engine-fp-policy-probe
```

Exact source/object/executable hashes are retained in `source-sha256.txt`.
The observed state is corroborating fresh-process evidence, **not** a recorded
register from the earlier timing process. No causal attribution of its
outliers follows. A future FTZ/DAZ on/off diagnostic must have distinct source,
metadata and results, with the first result preserved unchanged.
