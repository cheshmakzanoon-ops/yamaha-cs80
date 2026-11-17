# Invocation-local oscillator decay reuse: do not advance

Experimental source only. No production source, registered tests or CMake changes,
no plugin rebuild and no promotion. The candidate reuses one decay evaluation only
when its later positive finite interval is bit-identical. It passes the scoped
exactness tests but is **not proven faster**.

## Retained outcome

The frozen one-shot component walltime study retained all 384 rows, without retry,
drop or replacement. Apparent gain: **0.6067%**, bootstrap 95% interval
**−1.9276% to +3.1633%**. The largest active-case A/A block fluctuation was
**23.0605%**; 10 of 12 case intervals failed the −3% lower-bound guard.
Decision: `NO_ADVANCE_GAIN_OR_UNCERTAIN_GUARD`. This does not prove regression.
This is whole-oscillator walltime, not plugin callback CPU. Exactness tests provide
no timing claim. `RESULT.md` records the chronological proof-then-cost sequence;
its earlier “no benchmark” paragraph describes the pre-cost checkpoint only.

GNU 14.2 normal `-O3 -DNDEBUG -std=gnu++20 -fPIC -Wall -Wextra -Wpedantic`
flags were used; native-ISA runs are supplementary sensitivity checks. Full API
FP checks passed 6,011,358 comparisons in both normal and native runs. Coverage
and intentional-fault headers are test-only, never the timed candidate.

## Source linkage and replay

The reference archive is Git commit
`0c351b9c509e2d3e8ae1ff978fbc0a3a739974e3`, directory
`docs/experiments/oscillator-profile-cache/rejected/quotients/tests/`.
`OscillatorProfileCacheTests.cpp` is byte-identical to that archive.
`OscillatorDecayCostProbe.cpp` is its byte-identical
`OscillatorProfileCacheProbe.cpp`, renamed only. The production/reference header
SHA-256 is `c037a8970f939bdcd7d98ea56348006f82f2412cc64cd912fbf0f85b5ffa9373`.
The packaged fixture adds only a trailing comment to avoid GCC pragma-once
identical-file deduplication; its SHA is
`e52106cc05bba6caa3e03c7bf01c64412e0782d34b70f77905b3c670be63fee5`.
The actual candidate SHA is
`489cc6f9a442691a3ed73032bc37605a3e819e61e49343defc06f448629d2959`.
`evidence/candidate.diff` and the complete candidate/reference headers are retained.

From this directory, an optional correctness-only replay is:

```sh
g++-14 -O3 -DNDEBUG -std=gnu++20 -fPIC -Wall -Wextra -Wpedantic -I. tests/OscillatorDecayApiFpTests.cpp -o /tmp/decay-api-proof
/tmp/decay-api-proof
python3 verify_export.py
```

The verifier checks export hashes and replays only the frozen statistical analyzer
in a temporary directory; it does **not** run a benchmark or native executable.
`cost/run_once.py` is retained historical runner source, not a portable rerun
instruction: its plan paths are redacted labels and binaries are intentionally
omitted. A new cost trial would need a new explicitly frozen plan.

## Evidence limits and preparation failures

`export-manifest.json` preserves original and exported hashes/byte counts for
all text, and original binary hashes without publishing binaries. Only
`cost/plan.json` requires workspace path redaction. Labels such as
`EXPERIMENT_ROOT` are not original argv or filesystem evidence. The export cannot
independently establish execution, original environment or binary provenance.
Frozen compiler, analyzer, runner and executable hashes remain in the plan.
The historical correctness manifest's RESULT hash predates its final cost section;
the export manifest binds the final retained result separately.

- Initial `aa-build.log` is a failed pragma-once compile; use
  `aa-build-separated.log` and `aa.log` for the corrected A/A result.
- `fp.log`/`fp-build.log` belong to the superseded inherited-flags draft, **not**
  the current FP source. That draft source was overwritten and is not recoverable
  here. Rely on `fp-known.log` and the known-prestate source instead.
- `api-fp.log` and `api-fp-native.log` are the full API known-prestate corpus.
- Initial native-stress process exit observation was lost during recovery;
  a separate independent rerun subsequently verified exit 0. Do not treat the
  surviving initial log alone as an observed exit receipt.
- Fault-control failures and coverage counters are retained, but are not timings.
- Earlier requested-1kHz/observed-approximately-250Hz profiler accounting remains
  unresolved; that qualitative motivation is not quantitative cost evidence.
