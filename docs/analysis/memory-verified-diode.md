# Offline verified logarithm and diode-current primitive

This page records the original `b57e3ca...` primitive and its execution evidence.
The subsequent [centered-series cost experiment](memory-certificate-cost.md)
uses an exact additional power-of-two reduction and a 12-term signed series
with its outward-enclosed tail. The original 24-term proof/data below remain
historical baseline evidence, not a claim about the later header's identity.

## Scope and status

`src/experimental/MemoryVerifiedDiode.h` is a bounded, nonallocating research
primitive. It is not an Engine path, a real-time cost qualification, or a full
circuit certificate. Its target is the exact-real equation formed from the
caller's stored finite binary64 coefficients. It does not claim those rounded
coefficients exactly equal the unrounded resistor-preparation formula, a
measured instrument, or any uncertain physical component value.

The caller supplies an interval for the voltage across the complete series
resistor/diode branch; positive exact stored coefficients R, Is and nVt; an
untrusted current hint; and a maximum current-enclosure width in amperes.
`encloseDiode` changes its output only on success. A failed or wide enclosure
never authorizes publication. A full-node residual/margin calculation, model
identity and parameter validity remain the integrating caller's responsibility.

Header SHA-256:
`b57e3ca031c3a3220cac48891d2251ed037ab12ea97ede2171f09ec6323ac248`.

## Arithmetic contract

- IEEE binary64 with round-to-nearest and gradual underflow, masked floating
  exception traps, no fast-math or reassociation. Each elementary operation is
  stored separately through volatile. Its rounded result is expanded by one
  adjacent representable value in each required direction.
- Compile with strict floating arithmetic. Verified profiles use
  `-ffp-contract=off`; `__FAST_MATH__` and finite-math-only builds are rejected.
- Public log/diode entry points check rounding and denormal behavior, including
  separately modified SIMD rounding that `fegetround` alone might miss.
  Low-level interval arithmetic helpers require the caller to check and retain
  this environment. No function changes the environment, but checks and ordinary
  arithmetic can set floating exception flags. Masked traps are a precondition,
  not something safely discoverable by deliberately triggering a trap.
- Overflow, nonfinite intervals, zero-containing division denominators and
  unsupported entry-point environments fail closed. Success is conditional on
  this explicitly supported arithmetic/compiler contract, not a proof of an
  arbitrary C++ toolchain or hardware implementation.

## Logarithm enclosure, without a libm error assumption

An exact binary bit decomposition gives `x = 2^k*m`, `1 <= m < 2`, including
positive subnormal x. For `z = (m-1)/(m+1)`, use

```
log(m) = 2 sum(j=0..N-1) z^(2j+1)/(2j+1) + T
0 <= T <= 2*z^(2N+1)/((2N+1)*(1-z*z)),  N = 24.
```

Every operation in the partial sum and remainder bound is outward enclosed.
The positive tail must not be omitted merely because it is smaller than typical
binary64 rounding. The `log(2)` endpoints are adjacent hexadecimal binary64
values proved using exact rational arithmetic: the lower endpoint is at most
the 32-term rational sum at z=1/3, and that sum plus its rational tail upper
bound is at most the upper endpoint. The Python test reconstructs those integer
inequalities with `fractions.Fraction`; it does not assume `math.log` accuracy.
A negative k uses ordinary interval multiplication, including endpoint reversal.

## Diode-current enclosure

For each voltage v in the supplied interval, the exact current is the unique
root on `i > -Is` of

```
g(i) = R*i + nVt*log((Is+i)/Is) - v.
g'(i) = R + nVt/(Is+i) > R > 0.
```

An analytic starting bracket is `[-Is, max(v_hi,0)/R]`, with the lower endpoint
raised to zero for nonnegative voltage intervals. The limiting endpoint `-Is`
is never evaluated inside the logarithm. The numerator `Is+i` is added before
division; forming `1+i/Is` would lose valid reverse-saturation arguments.

At a candidate inside the certified bracket, a downward bound on
`R + nVt/(Is+bracket.hi)` is a valid derivative lower bound on the complete
candidate-to-root segment. R is retained as a safe fallback. If interval g has
endpoints `g_lo,g_hi` and the lower derivative bound is mu, then all roots lie in

```
[candidate - max(g_hi,0)/mu, candidate - min(g_lo,0)/mu].
```

Intersect with the existing bracket using outward arithmetic. Sign-proven
endpoints further tighten it. A reverse-limit probe, the untrusted hint and at
most 64 midpoint proposals make at most 66 verified contractions. No property
of the hint, including its source libm implementation, decides acceptance.
The interval must meet the caller's width request before it is returned.

## Independent execution evidence

GCC 14.2 / Linux x86-64 profiles: `-O0`, `-O2`, `-O3 -march=native`, and
`-O2` with ASan, UBSan, float-cast-overflow and library assertions. LeakSanitizer
is disabled in this sandbox; no leak qualification is claimed.

Each profile passed 5,448 vector checks and 56 boundary/environment checks:

- 1,406 elementary interval cases checked against exact Python fractions,
  including cancellation and gradual underflow;
- 1,738 log cases checked against 120-digit Decimal values, including normal,
  subnormal, near-one, negative-exponent and largest-finite inputs;
- 2,304 diode cases checked against independent 120-digit Decimal bisection,
  including voltage intervals, three diode corners, six resistances and
  deliberately wrong/nonfinite hints. Decimal is an independent test oracle,
  not part of the certificate's mathematical justification.

The standard 1e-11 A requests all succeeded. Additional 1e-15 A availability
probes produced 853 successes and 11 safe rejections at low-drop/high-current
corners. The rejected calls preserved the output. This is not a universal
promise to resolve arbitrarily tight current widths; no production tolerance
was relaxed to make those availability probes pass. Every accepted interval
was checked for containment and its actual requested width.

Reproducible fault injections were detected:

| Fault | Failed vector checks |
| --- | ---: |
| Remove outward endpoint expansion | 2,259 |
| Trust a finite candidate without verification | 1,512 |
| Inflate the derivative lower bound by 1e12 | 1,684 |
| Replace the proved log(2) bracket with 0.7 | 2,953 |

The fast-math build failed as intended. All three alternative rounding modes,
independently changed SIMD rounding and DAZ/FTZ modes were rejected without
output mutation. A separate source reviewer independently checked the interval
logic, positive series tail, exact log(2) rational inequalities, reverse limit,
local derivative and fail-closed current contraction. This review does not
extend to a consuming sparse solver or its full-network residual accumulation.

## Reproduction / integration

Run `tests/run_memory_verified_diode_checks.sh [build-directory]`. It needs
GCC and Python's standard library, not a new arbitrary-precision dependency.
The script builds four normal profiles and four deliberate mutants, verifies
the rational constant proof, runs the independent vectors and environment
checks, and confirms fast-math rejection. It preserves the transcript.

The header is retained at `src/experimental/MemoryVerifiedDiode.h`; its probe,
independent oracle and reproduction runner live under `tests/`. Standalone
CMake targets explicitly disable FP contraction (or select MSVC strict FP).
Review the arithmetic compile flags on any future integrating target. Do not
treat a low-current bracket as a full circuit residual certificate.
