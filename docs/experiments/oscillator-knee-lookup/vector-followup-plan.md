# Exact vector-backend follow-up, declared before its timing

The scalar-nearest candidate `4fa36a8` is **not qualified**. Its complete first
matrix is retained under `rejected/scalar-nearest/`: the pooled active-16 median
reduction was +7.187532% generic but −1.777077% native, failing the original
>=3%-in-both gate. Native held-high also regressed more than 3%. No row is dropped,
and these results do not qualify a generic-only shipping change.

Fresh isolated assembly inspection shows that the nearest-integer expression
adds dependent FP-to-integer and integer-to-FP conversions on its common path.
That is an implementation observation, not a measured attribution of the native
regression. This follow-up uses a different exact implementation on builds that
already enable AVX2: compare all four original dyadic-knot distances in parallel,
then use the one set mask bit to select the original assignment. There can be at
most one match, and the old assignment cannot reach another neighborhood. Keep
the original strict threshold and signed-side assignment. For builds without
AVX2, retain the independently tested scalar-nearest implementation.

There is no new runtime ISA dispatch or ISA requirement: AVX2 intrinsics are
compiled only when the translation unit already defines `__AVX2__`. Generic
builds remain baseline ISA. No object-state/cache, event search, profile, model,
error tolerance or compiler floating-point flag changes are permitted.

All original truth and promotion gates remain, including the literal-loop
boundary corpus, full public bit comparisons in generic/native GCC and Clang,
sanitisers and mutations. **The same >=3% pooled dual-profile cost gate and
maximum-3%-per-case regression gate remain.** Run the entire 432-row matrix on
this new frozen candidate. If it passes, require a second complete matrix at
131,072 frames with all original rows retained, then bounded paired Engine
confirmation. This extra confirmation prevents selecting a favorable small run
from the exploratory implementation change. If either cost matrix fails, do not
ship this candidate. Whole-instrument CPU/idle targets remain separate.
