# Dormant-noise small-mode mutation target

2026-10-31. Supplemental fault-injection procedure, before the follow-up source
mutation is evaluated. The acceptance contract remains the published
`aa3cadb4147ca9a1cd1cfe843345fcefa3d97432` version of
`tests/parity/tolerances-v2/dormant-noise.json`; no tolerance, seed, model, or
candidate numerical law changes here.

## First attempt retained as evidence

The initial actual-source mutation removed the last factor column of the panel
chain's five-sample transition. At every registered rate the retained-rank
assertion rejected it, but the covariance assertion also rejected it: the
normalized covariance change ranged from about `1.4e-5` at88.2kHz to `5.3e-10`
at768kHz. This confirms rank detection, but it does not isolate the registered
blind spot of an entrywise covariance comparison. The failed witness procedure
and its logs must remain in the qualification record.

## Follow-up target

Inject the omission into the **prepared hiss source**, three-sample transition,
at exactly705600Hz and768000Hz. Those are existing registered cases, and no rate
or sample count is added to the acceptance set. For each of these prepared
factors, compute the column norms and set the smallest nonzero column to zero.
All other prepared factors and source processing stay untouched.

The two low-frequency hiss sections have distinct, noncancelled poles. In the
short three-sample innovation matrix, the final independent direction contains
two small pole-difference factors, so its factor amplitude decreases roughly
with the square of the sample rate. This source-derived numerical argument
motivates the explicitly named high-rate hiss cases; it is not a fitted noise
law or a change to the accepted error. It predicts that the rank witness can
remain sensitive when the registered covariance tolerance masks the omitted
small mode. The follow-up must establish that result; this note does not claim
that it has passed.

Use an actual-source injection in `CircuitNoiseSources::prepare`, where the
sample rate is named, after the normal hiss transition preparation. Observe the
source's prepared plans through const diagnostic accessors and apply the same
independent full-section covariance and retained-rank assertions to them, in
addition to the existing standalone kernel checks. This adds integration
coverage without changing any threshold.

The follow-up counts as the intended witness only if it compiles normally, the
rank assertion rejects the omitted mode, and all entrywise covariance assertions
still pass. Preserve and report a failure of any part of this procedure. No
production branch, source gain, acceptance limit, or statistical seed may depend
on the selected mutation target.
