# Conditional IG00156 reference experiment

**Offline only: not a production audio switch, hardware identification or 1:1
claim.** The standalone C++20 header is src/reference/IG00156Linear.h; only its
dedicated test executable includes it. Engine, DSP and host parameters are
unchanged.

## Scope and evidence

[The model evidence ledger](IG00156_MODEL_EVIDENCE.md) gives the recovered
external 1.2 Mohm/180 pF feedback branch, 1.5 nF integrator capacitors,
4.7 Mohm feedforward resistors and equal 33 kohm OP1 feedback resistors.
Ideal amplifiers imply HP=2*AI-LP and two inverting integrators. Resistor values
are defaults, not random device variation. Rs/Rg are **required supplied bench
fixture parameters**, not an invented CS-80 input network.

The conditional port law is Iq=-gq*BP (current leaving AI). gm1, gm2 and gq
are supplied nonnegative siemens. In particular there is **no invented VQ-to-gq
curve**, Q-slider calibration, saturation, leakage, noise, temperature law,
IC-grade calibration or transistor model.

DynamicModel uses state [AI-BP,BP,LP] in volts and the ledger's physical A/B
matrix. AlgebraicModel requires exactly Cfb=0 and eliminates the first state:
HP=inputGain*u+damping*BP-LP. It never divides by zero capacitance.
These are distinct models. Both retain the feedforward conductance 1/Rff
inside the integrator rates; it is not capacitor-parallel leakage.

## Numerical contract

LinearSystem::step integrates constant coefficients with implicit trapezoidal
endpoint input. Changing a model between steps freezes its coefficients over
each whole interval; this is not a validated time-varying circuit solver.
The API returns state and solve diagnostics. Partial-pivot Gaussian elimination
rejects a pivot <=256*epsilon times the original largest matrix entry, and
checks normalized backward residual. This conservative rejection policy is
**not a condition-number estimator**. Invalid/nonfinite quantities, derived
overflow, singular/poor pivots and bad residuals throw diagnostics; nothing
clips or silently repairs state. This is a reference API, not real-time-safe
production code. Extreme representable inputs can be rejected.

## Reproduction and acceptance

Configure with CS80_BUILD_PLUGIN=OFF and CS80_BUILD_TESTS=ON, build, then run
ctest -R cs80_ig00156_reference_tests --output-on-failure.
The CTest timeout is 30 seconds; no JUCE or numerical library is required.

Tests cover finite/positive validation, zero state, OP1 and integrator KCL,
pivoting/failure diagnostics, Routh-Hurwitz fixture stability and discrete
decay at three timesteps. Three tones compare complex HP/BP/LP output to
independently eliminated continuous and bilinear transfer functions.
A separately coded physical-node RK4 reference at 50/25 ns checks trapezoidal
second-order convergence. Cfb=0 has its own KCL, DC response and transfer-limit
checks. The tiny-Cfb state comparison checks **BP and LP only**, not AI or the fast
AI-BP state. Trapezoidal integration is A-stable but **not L-stable**: a very
stiff fast mode can alternate and decay artificially slowly at a fixed step,
even when BP/LP agree with the algebraic model. Such fast-state numerical
ringing is not evidence of analog hardware behavior. Resolving that transient
requires timestep refinement relative to the fast time constant (or a separately
validated stiff transient solver); do not interpret this limit test as validation
of the fast AI waveform. This test uses implicit integration, not explicit Euler. Tolerances are numerical tests of these equations only.

The fixture uses unequal gm1=4.5 uS, gm2=5 uS, gq=20 uS and Rs=Rg=33 kohm.
These values are illustrative, not measurements. Tone comparisons require
absolute complex error <1e-8 to the bilinear response and <0.002 to the
continuous response at 192 kHz. Step refinement must reduce error by >3.5
per halving and reach <1e-5 V against converged RK4.

Still missing: Q-port identification/discrimination against bilateral models,
measured low-level calibration, full loaded HP-to-LP 1 uF coupled solve,
finite amplifiers and identified nonlinear silicon. Numerical agreement
cannot establish any of those, nor justify replacing the production filter.
