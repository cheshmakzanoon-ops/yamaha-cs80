# Combined mono/poly MSEG correctness review

This draft integration combines the exact production changes from
[PR #71](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/71)
(`62e1186`) and
[PR #79](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/79)
(`06fc9df`) on main `a53c1a8`.

## Interaction and scope

The mono change retains phrase onsets while an envelope's routes are dormant,
without advancing its unused clock. The poly change clears that envelope's
per-voice delayed rate when it actually restarts. They operate on separate
runner/rate state and preserve the existing one-sample rate-routing convention.
The production header combines without conflict and is byte-identical when
the two patches are applied in either order.

The only integration conflict was the insertion point of the two CMake test
targets. Both original four-line registrations are retained unchanged. This
integration adds one mixed regression; neither original regression is changed.
There are no workflow or validator changes, parameter/state-format changes,
new allocations or locks. Affected routed performances intentionally change;
PURE and default/no-route behavior remain unchanged.

## Mixed regression

`tests/MsegCombinedInteractionTests.cpp` checks 576 scenarios, with four
assertions per scenario (2,304 total):

- Six internal rates, from 16 kHz through 1.536 MHz.
- Every assignment of mono, poly and free modes to the three MSEGs.
- Free and synced envelope time, and positive/negative cross-rate routes.
- Dormant route activation through amount, source, destination or via edits.
- Recycled notes and explicit poly trigger-mode restarts while the shared
  envelopes continue.
- Continuity of the third, free-running sibling.

Separate `MsegRunner` instances form an explicit scheduling oracle. The test
checks mixed mono/poly trajectories, onset retention and first-tick base rate,
not just agreement between two instances of the changed `ModernLayer`.
The initial mode-change request is consumed before dormancy so it cannot
accidentally mask the missing-onset defect.

The final test passes 2,304/2,304 assertions on the combined candidate. Negative
controls using the same test fail 1,728 assertions on unchanged main and 1,152
assertions with either original proposal alone.

## Local verification

- GCC 14.2 Release: all six focused MSEG/matrix CTest suites pass.
- Original regressions: 206/206 mono-onset checks and 1,442/1,442 poly-retrigger
  checks pass on the combined production code.
- Both original regressions and the mixed regression pass AddressSanitizer,
  UndefinedBehaviorSanitizer and standard-library assertions; leak detection
  is disabled. The mixed test also builds with `-Wall -Wextra -Wpedantic -Werror`.
- Strict compatibility on the combined production code is byte-identical:
  Legacy default/tails versus `d56911a`, V1 circuit numerics versus `22d6a32`,
  and V2 circuit audio versus `00ceccf`. The pinned cases do not cover every
  historical routed MIDI performance.

```sh
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --parallel 2 --target \
  cs80_mseg_tests cs80_mseg_transport_rate_tests \
  cs80_mono_mseg_routing_tests cs80_mseg_retrigger_rate_tests \
  cs80_mseg_combined_interaction_tests cs80_mod_matrix_tests
ctest --test-dir build-core --output-on-failure \
  -R '^cs80_(mono_mseg_routing|mseg(_transport_rate|_retrigger_rate|_combined_interaction)?|mod_matrix)_tests$'
scripts/check-compat-gates.sh --strict-v2
```

## Prior CI discrepancies and merge hold

The separate proposals have differing CI outcomes; this integration does not
explain or waive them:

- #71 [push run 42200433445](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42200433445)
  failed Linux GUI validation. Its
  [PR run 42200498204](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42200498204)
  passed Linux validation but had a canceled Apple Silicon job, so the run
  was not fully green.
- #79 [push run 42323384698](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42323384698)
  passed all six jobs. Its
  [PR run 42323438591](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42323438591)
  failed Linux fixed-seed GUI validation.

These are verified run/job summaries, not a diagnosis. Keep this integration
draft and unmerged until the required checks pass for its exact revision.
Local standalone tests do not establish plugin, GUI, DAW, native ARM or
physical-hardware qualification, or resolve the existing validation discrepancy.
