# Evidence-only instrumentation

These files are **not part of `candidate.patch` or either normal core archive**.

`counter.cpp` defines a GNU linker `--wrap` shim around the actual compiled `tryReuseIg00156LpCoefficients` symbol listed in `wrapper-symbol.txt`. The shim calls the real function and increments integer call/hit counters afterwards. It does not replace the helper's arithmetic. Link it only into the separately counted fixture.

`tests/trace.cpp` uses `-fno-access-control` only on that test translation unit to serialize pre-existing private fields. Core files and class declarations are not instrumented or given added friends. Build the trace normally for stock and V2; add `-DCOUNTED`, this counter object and the wrap option only for the counted V2 executable.

The cost fixture has separate `LANE_COST_DIAGNOSTIC` and `LANE_COST_OBSERVERS` switches, both defaulting to zero. Observers require diagnostics. Normal arms must have both off and must not contain `cs80_lane_count_*` or the wrapper symbol. The existing untimed receipt records matching normal/diagnostic identities and ordinary-arm counter-symbol absence.

These witnesses establish that the helper is exercised and the observed results remain equal in the tested workloads. Their counts are not a substitute for observer-free comparative timing. No instrumentation patch is required for the normal candidate.
