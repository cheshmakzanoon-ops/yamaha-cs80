# Bounded tanh candidate: numerical certificate, not audio qualification

2026-10-30. **Experimental mathematical component only. No Engine code, native implementation, SIMD implementation, or CPU improvement is claimed.** The execution workspace was unavailable during this verification; JavaScript BigInt checks were available independently. The pending all-22 tone and locale changes are separate work.

## Purpose and exact expression

The event-split VCO/VCF investigation evaluates the existing nonlinear law repeatedly. This candidate approximates the same tanh function; it does not remove saturation, alter drive, or establish that the provisional saturation law matches a Yamaha IC.

For finite input magnitude below 2^-20, return the input. At magnitude at least 20, return its sign. Preserve NaN, infinities, and signed zero as in the checker. Otherwise evaluate the following **specific binary64 operation graph**, with z = abs(x)/4:

```text
s  = z*z
p  = z*(10395 + s*(1260 + 21*s))
q  = 10395 + s*(4725 + s*(210 + s))
p2 = p*p
q2 = q*q
a  = p2 + q2
y  = (4*p*q*a)/(a*a + 4*p2*q2)
```

Clamp y to at most 1 and restore the input sign. There is one division in this expression. That operation count is not a performance measurement. The expanded algebraic denominator has a different floating-point operation count and is not automatically covered by this certificate.

## Mathematical argument

The starting identity is the [Lambert continued fraction for tanh, NIST DLMF 4.39.1](https://dlmf.nist.gov/4.39.E1). Positive real arguments have alternating neighboring convergent bounds. The sixth convergent is C6=P6/Q6, with the p/q polynomials above. The seventh has:

```text
P7(t) = t*(135135 + 17325*t^2 + 378*t^4 + t^6)
Q7(t) = 135135 + 62370*t^2 + 3150*t^4 + 28*t^6
P7*Q6 - P6*Q7 = t^13
```

Thus C6 <= tanh(t) <= min(C7,1). Apply the double-angle identity twice. Its combined form is g(v)=4v(1+v^2)/(1+6v^2+v^4), exactly the expression above in real arithmetic. On [0,1], g is increasing and its derivative 4(1-v^2)^3/(1+6v^2+v^4)^2 is decreasing. Odd convergents can exceed 1; using their unconstrained values as inputs to a monotonicity argument would be wrong.

The checker divides t in [0,5] into 5,000 rational intervals of width 1/1000. Positive polynomial coefficients give a lower bound L=P6(low)/Q6(high). The pointwise convergent gap is bounded above by high^13/(Q6(low)*Q7(low)). Multiplying that by g'(L) bounds the transformed error. Every comparison uses exact BigInt integer arithmetic; no monotonicity of C6(t) is assumed. Every interval is strictly below **1.86e-11**. The largest interval certificate occurs at x in [6.792,6.796].

For the stated operation graph, positive-term forward-error accounting bounds the relative errors of p, q, numerator and denominator by gamma7, gamma8, gamma35 and gamma38 respectively (gamma_n = n*u/(1-n*u)). The checker verifies the resulting ratio-error budget is below 100*2^-53. This argument assumes IEEE binary64 round-to-nearest, normal intermediate values and no unsafe fast-math/reassociation/approximate reciprocal. Exact power-of-two scaling is used; FMA contraction does not increase the counted roundings. The tiny-input branch avoids a subnormal-intermediate assumption. The domain bounds keep the denominator positive and intermediate values far from overflow.

Combining those bounds gives **absolute error below 2e-11**. The tiny-input error is below x^3/3. A positive 60-term Taylor lower bound proves exp(40)>1e17, bounding saturation error beyond magnitude20 by 2e-17. Clamping to the true range [-1,1] cannot increase absolute error.

## Reproduce and distinguish evidence

Run `node artifacts/fast-tanh/verify.mjs` in a BigInt-capable JavaScript runtime. The checker verifies the determinant, all 5,000 exact intervals, the conditional floating-point budget, branch bounds and special values. It also performs a separate 400,001-point double sweep against Math.tanh. The observed maximum difference was approximately 1.75481e-11. That sweep is a sanity check, not the uniform proof; Math.tanh is not itself an exact-real oracle.

[Recorded result](../artifacts/fast-tanh/certificate.json). The certificate establishes a numerical-component bound under its assumptions, not hardware equivalence or an end-to-end audio result.

## Required adoption gates

Before any native use, replay the exact operation graph against std::tanh and qualify the compiler's floating-point behavior. Compare matched same-rate complete filter trajectories at both nonlinear sites with unchanged scaling, event-side handling and state-dependent evaluations. Cover the prior high-note/blend cases, cutoff/Q/drive limits, modulation, resets, long runs, silence and denormals. Report state/output drift, residual and alias metrics.

Separately repeat multirate convergence against the existing high-rate std::tanh reference, so approximation error cannot hide solver/rate error. Finally measure uncontended whole-island and 16-voice CPU cost, including antialiasing/rate reduction. **No native speed, full-island quality, real-time capability or production integration is established by this document.**
