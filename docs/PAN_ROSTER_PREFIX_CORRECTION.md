# Historical pan roster assertion correction

The Linux job in [build 43199791639](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/43199791639/job/131119881853), for `e208613208767e70346166931f6d2c011527be9d`, passed 307/313 tests. All six general VST3 automation variants stopped at `Vst3PanAutomationTests.h`'s stale assertion that the entire roster contained exactly eighteen original lanes plus two pan lanes. Subsequent approved SUB/RING additions had grown the roster to thirty.

The correction changes only this test's size precondition from exactly twenty to at least twenty. The original eighteen ordered keys and both pan positions remain checked without changes. Later appended lanes remain governed by their dedicated full-roster witnesses, including `Vst3RingAutomationTests.cpp`'s thirty-lane count and unchanged twenty-eight-lane prefix checks. No production roster, DSP, tolerance, fixture trajectory, or ordering is changed.

## Focused validation

Run `python3 tests/check_pan_roster_prefix.py` from the repository. This standalone check extracts the actual prefix guards from the pan witness and compiles them with the actual production parameter definitions and roster, using C++20 with warnings as errors. It does not execute a plugin or the general protocol suites.

On the thirty-lane base `b2ec204b0e42b93ac173e810c59908331685b7c5`, all eight checks passed:

- Current production roster accepted.
- Historical twenty-lane prefix accepted.
- A tail extended with RING depth accepted.
- A tail additionally extended with RING attack/decay accepted.
- Missing second pan lane rejected.
- Original-key reordering rejected.
- Pan-key reordering rejected.
- Pan displacement into the appended tail rejected.

These are roster-contract checks only. The appended examples are not runtime qualification of the corresponding feature branches. No full VST3 suite was rerun; its later pan/audio checks and cross-platform CI remain unverified by this correction. The earlier passing audio output before the CI assertion does not qualify code paths after the assertion.
