# Offline verified logarithm interval-span experiment

## Preregistered scope and acceptance

The [point-operand experiment](memory-interval-cost.md) does not qualify an
Engine or audio-rate path. This increment remains offline and keeps the exact
stored-coefficient target, strict arithmetic assumptions, all-node residual
certificate, requested node-error budget and publish-on-success policy.

`detail::logInterval` currently computes two independent certified endpoint
logarithms. For `0 < a <= b`, monotonicity and integration of `1/x` give

```
log(a) <= log(x) <= log(a) + (b-a)/a, for every x in [a,b].
```

Try one certified `log(a)` plus an outward-rounded span correction only when
the computed relative span's upper bound is at most the exact constant
`2^-40`. Exact point intervals need only one logarithm. Retain the existing
two-logarithm path for wider spans or invalid span arithmetic. Do not weaken
the atanh series, tail, runtime arithmetic checks, diode-current contraction,
full-node residual or final acceptance budget. The span test is a cost/width
choice, not the proof of containment; the inequality and outward arithmetic
must hold independently. No libm accuracy assumption is introduced.

Before accepting the candidate, independently verify exact-rational enclosure
of both endpoint logarithms over a deterministic corpus including point/narrow/
wide intervals, negative and positive logarithms, intervals crossing 1,
subnormals, maximum finite values, reduction thresholds and the span threshold.
Invalid inputs must still fail. Add a mutation which omits the span correction
and require a numerical enclosure failure, not a compilation failure. Existing
primitive tests and all compiler/environment/fault profiles must pass. Compare
mandatory diode availability and report every change in the optional tight
width probes; require no regression from the baseline's 860 / 864 accepted
optional probes on this fixed corpus. Retain the complete 2,016-case sparse
network gate, all 94 nodes, allocation checks, sanitizer run, configuration and
failed-publication checks. Preserve original numerical limits.

Measure the same complete-output 128-evaluation workloads with exactly the same
probe source and compiler options for both versions. Warm/cold solve scenarios
traverse inputs 0..127; certificate-only/preparation repeat their fixed source.
Use seven adjacent alternating-order pairs for warm and certificate-only
scenarios, separately record wall and single-process CPU time, and keep both
unpinned and child-only allowed-CPU-affinity phases. Report all pairs, failures,
medians and regressions, including discrepancies between median pair ratios and
separate medians. These are shared-machine cost diagnostics, not WCET, measured
instrument utilization or an audio scheduling/fallback qualification. No
activation is authorized regardless of the scalar improvement.

## First candidate availability finding

The first lower-endpoint anchor passed the exact-rational interval oracle and
all mandatory diode vectors, but reduced optional 1e-15 A availability from
860 to 858 accepted cases. It is rejected by the gate above; the budget is not
relaxed. Before further timing, test a centered anchor as a precision refinement:
choose a finite representable `c` in `[a,b]`, enclose `log(c)` once, and apply
separate outward integral bounds `(c-a)/a` on the left and `(b-c)/c` on the right.
The proposed midpoint is not trusted without containment checks. Zero-length
sides need no extra expansion. Keep the original fallback and all acceptance
limits. Preserve the rejected prototype and its result as evidence.

## Result: reject both prototypes, preserve the certificate implementation

Both constructions enclose the independent exact-rational endpoint oracle on
1,476 intervals and reject nine invalid domains. The optional-width availability
gate nevertheless fails:

| Version | Accepted at 1e-15 A | Safely rejected | Previously accepted cases newly rejected |
| --- | ---: | ---: | ---: |
| Existing two-endpoint baseline | 860 | 4 | 0 |
| Lower-endpoint one-log prototype | 858 | 6 | 2 |
| Centered one-log prototype | 856 | 8 | 4 |

All three pass the 7,374-vector required containment checks and 56 environment/
self checks. The changed statuses are retained in [availability.txt](memory-log-span-study/availability.txt).
They are safe rejections with unchanged destination, not false certificates;
they still violate the preregistered availability requirement. The centered
prototype also passes the 2,016-case / 533,518-check full-node network gate,
with the same maximum discrepancy/bound and zero observed allocations. That
success does not override the failed primitive-availability gate. The lower
prototype was not run through the full network grid.

The rational oracle detects an explicit omitted-correction mutation with 268
numerical enclosure failures. The 114 point and 411 sufficiently wide fallback
outputs are unchanged from baseline on this corpus. An independent reviewer
rebuilt all three frozen versions and reproduced the 860/858/856 acceptance
counts, exact changed statuses, primitive/self checks and rational results.

**Neither prototype is retained in `src/`.** The original point-path header
is restored exactly, SHA-256
`5989c0969b9c8292817f44e9fd3e55b89c44e884f820bceaca67d6c712c01bb0`.
Only the new optional research oracle/interface and availability-comparison
instrumentation remain. The rejected source snapshots, test outputs and
[manifest](memory-log-span-study/manifest.json) are archived to make the
negative result reproducible. Run `bash docs/analysis/memory-log-span-study/reproduce.sh
/path/to/scratch-build` for the bounded reproduction.

No timing comparison was performed because the numerical availability gate
failed first. No sanitizer, compiler-profile or speed qualification is claimed
for these rejected prototypes. Existing scheduling, fallback/publication and
Engine-activation limitations remain unchanged. A future experiment should
prefer transformations that retain exact interval endpoints, rather than
recovering speed by spending more of the same certificate precision budget.
