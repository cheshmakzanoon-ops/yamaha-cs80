# W3: audible IG00153 / IG00158 reconstruction proposal

2026-10-29. **Design and standalone-candidate evaluation, not delivered audible DSP or a measured-hardware claim.** Oscillator code/path inspected at `411452eb071cf6a8c4cd6530d45f25e77e90442b`; concurrent output integration reached `8bb9b81472c7618e910d385d27fc315a8fd40c8b` during review, with that oscillator seam unchanged. This document owns no production change. The proposed profile is `w3-current-fold-v1`; it is deliberately an **engineering-inferred nominal instrument**, with source-derived topology and explicitly selected opaque-IC behavior. Missing hardware is not a gate. The next deliverable is an audible, parity-isolation-only implementation, not another permanently disconnected reference model.

## Decision in brief

Replace three unrelated expressions in `Oscillator::tick` with one event-driven charge/reset state feeding the actual WSC branches:

```
Kv / Rft -> current sink + switched reference charge -> IG00153 pin7 converter
       -> 4.7 uF / loaded input -> WSC rising saw -----------------> HP input
                               -> threshold comparator + clamp --> HP input
                               -> slice-level fold -> triangle
                                  -> 4.7 uF / loaded input
                                  -> inverting diode-feedback shaper -> sine mix
```

The replacements are the bipolar “narrow pulse” added to a generic saw, the separately phased analytic sine with fixed 3.5% third harmonic, and pulse duty computed independently of the actual comparator input. Preserve the existing oscillator byte-for-byte when parity isolation is **off**. Do not add another user-facing fidelity switch or change saved parameter values.

The audible consequences are intentional in isolation mode: zero-baseline reset excursions instead of a large spurious DC offset; reset-dependent pulse crossings; WSC rising-saw polarity; a phase-coherent folded/shaped sine; waveform-dependent pin voltages; and real coupling-state memory. None proves equality to a particular CS-80.

## 1. What is known, what is selected

| Item | Evidence / status | Proposed handling |
|---|---|---|
| IG00153 input-current law | Guidebook PDF35 prints `I = Kv/Rft`; reference-current switch, capacitor, comparator, one-shot and output converter are shown. | Preserve this structure. For the first audio integration, adapt the engine's existing frequency request to current; do not simultaneously claim to have calibrated the keyboard/footage/VIB voltage buses. |
| Reference current / threshold | Generic guidebook marks approximately 800 uA and 10 V. | Useful topology-scale annotations, not a CS80 specimen calibration. Use normalized charge for audio; do not manufacture exact capacitor voltage from those generic labels. |
| Hz/V | CS80 sheet42: .250/.500/1/2/4 V -> 130.8/261.6/523.2/1046/2093 Hz at normal transposition. | The chosen reset interpretation must preserve proportional frequency. Rounding of printed frequencies is not tuning nonlinearity. |
| Reset at pin7 | Rider's firsthand original-IC photographs show a short upper plateau before the falling ramp; he reports about .45 V excess and 30 us duration. | Use these as a nominal observed-condition anchor, not a measured law across pitch. His later 6 Vpp discrete replacement and uncalibrated sine are not original-chip specifications. |
| WSC routing / polarity | Guidebook PDF39 and CS80 sheets41–42: SO5 is inverted/rising saw; PI9 feeds comparator; TI14 feeds triangle fold; TO3 is AC-coupled to STI15. | Derive all three outputs from the same raw, un-antialiased analog trajectory. No independent reset addition at PO7. |
| Triangle fold | Sheet42 shows rectification about the slice level. | Absolute-value fold; midpoint slice is a selected nominal. Internal limiting and exact slice bias remain inferred. |
| Sine shaper | CS20M/40M printed49–51 describes inverting diode/resistor-feedback polygon shaping of triangle. | A monotone, odd, decreasing-slope piecewise-linear transfer, with declared coefficients. Never infer physical diode count or resistor values from the chosen approximation. |
| Comparator | CS20M/40M printed51 describes comparison followed by a level clamp. | Two output levels, events from actual input/threshold crossings. Generic 3 Vpp is a **selected family-level nominal**, not a transcribed CS80 PO7 voltage. |
| Coupling | M41 has 4.7 uF NP / 22 kOhm at the VCO-to-WSC input and again before STI15. | With ideal source and high-impedance receiver assumptions, each time constant is .1034 s, corner approximately 1.539 Hz. Those impedances are assumptions, not a complete IC-port solve. |
| Pin amplitudes | CS80 sheet42 labels VCO 3.5 Vpp, TO3 3.2 Vpp and SIO2 3 Vpp. | Keep separate pin-voltage targets. They are not final mix gains, waveform RMS equality, or exact amplitude guarantees at every pitch. |

### Important primary-source conflict: the one-shot switch polarity

The guidebook drawing does **not** unambiguously label whether the reference-current switch is on or off during its pulse. CS20M/40M PDF47, printed48, describes turning the reference current **off** for a fixed interval after the upper threshold. Taken literally with its own drawn sink `I = Kv/Rft`, reference current `Iref`, and fixed off-time `tau`, periodic charge balance gives:

```
Iref * (T - tau) = I * T
f = (1 - I/Iref) / tau
```

That is opposite the documented positive Hz/V relationship and tends to `1/tau`, rather than zero, as key current tends to zero. A complementary-current interpretation could rescue that prose, but would contradict the simple drawn `I = Kv/Rft` unless an additional undocumented inversion is supplied.

**Selected inference:** the effective restoring-reference pulse is **on for tau**, triggered at the lower capacitor threshold. Then:

```
Iref * tau = I * T
f = I / (Iref * tau)
```

This satisfies the CS80 frequency anchors and the photographed short reset/long falling-ramp behavior. As a scale check only, an illustrative 80 kOhm effective Rft with the generic 800 uA and selected 30 us gives 260.42 Hz at .5 V; that is not proof of the normal-footage resistor or exact trimmer setting. It is an explicit engineering interpretation, not an assertion that the family manual's prose says this. Retain the contrary description in the evidence ledger; do not copy its switch timing into audio while silently reversing the current law.

## 2. Concrete nominal profile

All numerical selections below belong in one immutable `OscillatorNominalProfile`, not scattered literals. Names should carry `Inferred` or `Nominal`; the profile ID belongs in probe manifests.

### 2.1 Charge state and event scheduler

Use double-precision normalized charge `q` and `resetRemainingSeconds`; no per-sample allocation, locks, virtual dispatch, or unbounded Newton iteration.

```
q = C * (Vcap - Vthreshold) / (Iref * tau)
currentRatio = I / Iref = requestedHz * tau

ramp:   dq/dt = -requestedHz
reset:  dq/dt = 1/tau - requestedHz
```

At a falling crossing of `q = 0`, enter reset for exactly `tau`. A stopped capacitor at `q = 0` does not repeatedly trigger the one-shot; restarting positive current may begin a reset from that boundary. Do not zero phase on a live frequency/PW change. In reset, integrate the new current immediately but do not restart its timer. At timer expiry return to ramp. Split each sample at fractional event times; do not round a 30 us event to one or three samples. For constant frequency, `q` reaches `1 - f*tau` after reset, and the complete cycle is exactly `1/f`.

`tau = 30 us` is the selected nominal. The equivalent current adapter is `I = Iref*tau*requestedHz`; this is an interface calibration, **not** recovered CS80 control circuitry. A later voltage adapter can replace it with `Kv/Rft` without replacing the oscillator state machine. In particular, IG00150's documented exponential converter must not be imported into IG00153: these are different chips and control domains.

At zero frequency the ramp freezes; an already-running reset finishes, and coupling capacitors still relax. Validate profile constants at prepare. At runtime, retain the last finite frequency/threshold on a nonfinite control request, set an `invalidControl` diagnostic, and never throw or allocate from the callback; test this explicit transport-failure policy separately from physical behavior. Bound extrapolation to `f <= min(.45*internalSampleRate, .95/tau)` and report a diagnostic flag when limited; `.95` is a numerical-domain policy, not a hardware limit. Test the complete reachable original-control range separately so a guard is not silently retuning ordinary notes. In particular, 8.372 kHz is only the upper unbent pitch used in the initial matrix: the existing source-supported one-octave-up ribbon endpoint reaches 16.744 kHz from it, before master tune, rank detune, initial pitch or SUB/pressure excursions. Derive the complete adapter maximum from the actual control bounds and retain a separately identified out-of-range MIDI stress set.

### 2.2 Pin7 output converter, not a pulse mixed downstream

The IC's output-converter transfer is opaque. Use the smallest explicit model consistent with the observed plateau, charge balance and the nominal pin amplitude:

```
fRef = 261.6 Hz                      // chosen calibration condition
vReset = 3.5 V                       // selected full pin7 span
resetExcessAtRef = .45 V             // observed-condition anchor adopted at fRef
K = (vReset - resetExcessAtRef) / (1 - fRef*tau)
  = 3.074125738798087 V per normalized q

v7 = resetActive ? vReset : K*q
```

The zero reference here is arbitrary DC datum; it is not a claimed actual pin7 offset. The subsequent coupling stage carries and removes its DC state. A separate bipolar reset oscillator is unnecessary and wrong for this construction. The output reset override is a **selected plateau/clamp abstraction of the converter**, not the capacitor's own finite positive reset ramp.

This intentionally preserves fixed converter gain rather than normalizing every note's ramp. Its predictions must remain visible:

| Frequency | Reset fraction | Falling-ramp span | Plateau excess above ramp start |
|---:|---:|---:|---:|
| 130.8 Hz | .003924 | 3.0621 V | .4379 V |
| 261.6 Hz | .007848 | 3.0500 V | .4500 V |
| 1046 Hz | .03138 | 2.9777 V | .5223 V |
| 4186 Hz | .12558 | 2.6881 V | .8119 V |
| 8372 Hz | .25116 | 2.3020 V | 1.1980 V |

These are **model predictions, not recovered measurements**. Short pitch changes can temporarily leave charge outside the settled-cycle range; integrate that state continuously, do not clamp it back onto the new steady waveform. This is a meaningful high-register uncertainty. Compare against a constant-excess alternative (plateau follows ramp end plus .45 V) and a converter-amplitude-normalized alternative. Do not quietly adjust gain per note until their differences disappear. The chosen `fRef` does not assert that Rider photographed exactly 261.6 Hz; his report does not provide that calibration.

### 2.3 AC coupling and the three WSC branches

For the first nominal port boundary, use:

```
b1' = (v7 - b1) / .1034
u = v7 - b1
sawPin5 = -u                        // unity inverter gain is inferred
pulsePin7 = (u > thresholdVolts) ? +1.5 V : -1.5 V
```

Evaluate pulse events at every actual crossing, including reset start/end and moving threshold crossings. Its plateau remains two-level even when reset changes edge timing. A threshold above the ordinary ramp but below the reset plateau can legitimately create a reset-only pulse; that comes from comparison, not a third additive output.

Nominal triangle profile:

```
rampHalfAtRef = (3.5 - .45)/2 = 1.525 V
sliceVolts = 0 V
trianglePin3 = 3.2 V * clamp(abs(u - sliceVolts)/rampHalfAtRef, 0, 1)
b2' = (trianglePin3 - b2) / .1034
triangleShaperInput = trianglePin3 - b2
x = triangleShaperInput / 1.6 V
```

The absolute-value fold is source-informed. **Its midpoint slice and amplitude limiter are engineering choices** that realize the nominal triangle span without interpreting a reset excursion as unlimited triangle amplitude. The limiter is not a transcribed IG00158 diode or rail. Keep an unclipped-fold alternative in sensitivity tests, and identify its larger reset excursions. This is preferable to leaving unknown behavior blank or labeling it physical fact.

Use an inverting odd PWL shaper: `sinePin2 = -Gsin * P(x)`. For nonnegative `x`, interpolate this table; extend oddly for negative `x`:

| x | P(x) |
|---:|---:|
| 0 | 0 |
| .25 | .3826834323650898 |
| .50 | .7071067811865475 |
| .75 | .9238795325112867 |
| 1 | 1 |

The ordinates are `sin(pi*x/2)`: **a selected approximation objective**, not measured IC coefficients. Clamp `P` beyond ±1 in this nominal profile; this additional amplitude limiting is inferred and must be included in the alternative-profile comparison. The four positive-branch slopes are `1.53073372946, 1.29769339529, .86709100530, .30448186995`.

This is realizable as an ideal biased diode-feedback polygon, rather than a mysterious harmonic blend. In normalized magnitudes its inverse is:

```
x = g0*y + sum(gi * max(y - Bi, 0))
g0 = .6532814824381883
Bi = [.3826834323650898, .7071067811865475, .9238795325112867]
gi = [.1173165676349104, .3826834323650894, 2.130986313697834]
```

All conductances are nonnegative: additional feedback reduces forward slope as successive diode branches conduct. They are **dimensionless equivalent conductances**; neither their count nor their conversion into ohms is a physical-chip transcription. The amplitude clamp is an ideal limiting branch outside this polygon.

Choose `Gsin` once at the declared settled reference condition to make pin2 3 Vpp; never auto-normalize each note. A dense periodic calculation of the above equations gives approximately **1.501777 V**. This small trim is a numerical nominal-profile calibration, not a measured resistor. Recompute and freeze it in the implementation fixture with its method/version, rather than treating the rounded number as sacred.

As a math-only sanity check, with reset/coupling distortion removed and triangle traversing ±1, this four-segment construction has approximately .5853% THD and .01885 maximum normalized error against `-cos(2*pi*phase)`. Low-order harmonic cancellation in that ideal uniform-knot fixture is a consequence of the chosen interpolation; it is **not** a CS80 sine spectrum. Reset, slice offset and coupling change that spectrum. No 3.5% third harmonic is added anywhere.

### 2.4 Shared phase and the voltage-to-engine adapter

With a falling ramp starting high, midpoint rectification makes triangle maxima at the reset boundary and a minimum halfway through the ramp. The inverting shaper therefore approaches **negative cosine**, not the old independently chosen `sin(phase)`. Preserve that relation, including reset and coupling shifts. Do not align each waveform independently in analysis: it would conceal the very error being corrected.

The later primary confirmation in [IG00156_MODEL_EVIDENCE.md](IG00156_MODEL_EVIDENCE.md) establishes positive OP1 AI and two inverting integrators; the ideal equations are `HP=2*AI-LP`, negative HP-to-BP integration, negative BP-to-LP integration. Thus no arbitrary overall sign inversion is required by the ideal HP/LP cascade. External HP-to-LP connector routing and the final sine/filter summing still have their own evidence boundaries. The initial integration keeps the current positive cascade convention as an explicitly labeled boundary; it does not claim those external gains have been solved.

Return typed **volts**, e.g. `WscOutputVolts { sawPin5, pulsePin7, sinePin2 }`; expose raw pin7/triangle in diagnostics. `WscControls` takes `pulseThresholdVolts`; an adapter may also accept `sawEnabled`, `pulseEnabled`, but the M41 waveform gate FETs are downstream of the physical WSC pins. Disabled gates must not freeze physical or antialias state. Keep ungated pin diagnostics, label gate-applied audio explicitly, and include gate-transition events rather than claiming a disabled physical pin becomes zero. The oscillator must not accept a raw factory PW bus and pretend it is a duty fraction.

For existing panel controls only, use the explicit inferred adapter `threshold = 1.525*(1-2*dutyProxy)` V. Actual duty then follows the circuit state and need not equal that proxy. The [factory/control-network adapter](FACTORY_TONE_INTEGRATION_PROPOSAL.md) owns the PW/PWM bus-to-threshold gain, offset and clamp: PW through 680 kOhm and processed PWM through 100 kOhm into the WSC op-amp network establish relative leverage under stated port assumptions, not a complete comparator threshold calibration.

A first audible adapter may use one fixed `digitalUnitsPerVolt` for **all** WSC outputs, e.g. `1/1.75`, while retaining the engine's existing downstream mix gains as disclosed provisional boundaries. Do not separately normalize sine, pulse and saw to ±1 and then call the pin-voltage model integrated. The passive gate/mixer and sine bypass gains must subsequently consume the typed voltages; this proposal does not solve that entire network.

## 3. Real-time numerical implementation

1. Keep the engine's existing 2x internal rate and output decimator for the first prototype. Advance charge/reset analytically over each constant-control subinterval; compute fractional reset and threshold events. Use fixed-size state and event storage. Coefficients for the slow coupling poles are prepared once.
2. Keep the **raw physical trajectory** separate from its antialiased audio representation. Feeding a BLEP-smoothed saw into a nonlinear fold/comparator and doing no further correction creates new aliases; it is not an implementation of the intended continuous event graph.
3. For each final branch, schedule value-jump correction (BLEP) and slope-jump correction (BLAMP), including reset override transitions, fold/clamp corners, all PWL sine knees, and PW/CV-driven events. A sine derived from triangle is not exempt from antialiasing. Correct branch events, not a shared reset pulse mixed into every output.
4. Prototype a causal, fractional-event residual bank (64 internal samples, at least 32 fractional phases, precomputed outside the callback). These are **starting numerical choices**, not accepted quality results. Derive/integrate kernels consistently and test their constant/linear asymptotes and moment/DC behavior. Do not assume an arbitrary integrated minBLEP table has a finite zero-tail BLAMP residual. The exact exponential-basis extension below covers the selected coupling trajectory without truncating its curvature. Any introduced frequency-dependent phase response is measured against the ideal-bandlimited reference and shared-branch phase tests; it is not analog behavior.
5. On each constant-slope pin7 segment, integrate the first coupling pole exactly: `u(t)=s*RC+(u(0)-s*RC)*exp(-t/RC)`. Between fold/clamp events, triangle is constant plus an exponential, so the second equal-time-constant pole also has an exact elementary solution with a `t*exp(-t/RC)` term. Comparator roots under held threshold are analytic; sine-knee roots need a bounded safeguarded solve, splitting first at the at-most-one within-segment turning point. Validate against separately integrated continuous equations. Do not replace exact fractional reset timing with sample rounding.
6. If the residual bank fails the preregistered alias/in-band gates at 2x, increase the oscillator-local rate/kernel quality and benchmark. Do not relax the gate after observing a failure or cite the 255-tap output decimator as a cure for already-folded aliases. A local decimator or look-ahead design must declare its phase/latency and align **all** waveform branches and the complete note/control path. Default-off latency stays unchanged; any deliberate isolation-only latency change requires host-notification and alignment tests before merge.

The acceptance criterion is the reported error, not the words “BLEP,” “oversampled,” or “analog.” The exact production kernel is selected by these tests. This is a bounded implementation decision, not an excuse to postpone audible integration indefinitely.

### Reviewed numerical alternatives (standalone evaluation, not integration approval)

The first implementation-side minimum-phase experiment was rejected for an unretained tail L1 of `0.07025094721129384` (initial C++ print: `0.070251`). This is a **candidate numerical failure**, not hardware evidence or a result for every minimum-phase design. A NumPy reconstruction of the full-length real-cepstrum equations, independently rerun during review, reproduces the reported value: `/tmp/oscillator-rejected-candidate/minphase_attempt.py`, SHA256 `0812e9e01d2c4fc97308310420b5ba2fc2c65d8d65a908cc0f31957ca4575dfc`; `report.json`, SHA256 `2e6b1fa806ee7b24098940d5fb79b1a3302d4d6fcf39021de2fd2e28455f198c`. Its manifest records 64-sample prototype support, 64 phases, 32768 FFT points, `.40` cutoff and `1e-12` magnitude floor. This rejected implementation experiment uses full log magnitude, **not** the half-length square-root conversion in the separate illustrative experiment below.

A separate independent review experiment illustrates an additional reason not to accept minimum phase blindly: a 64-internal-sample, 32-fractional-phase homomorphic half-length conversion of a Kaiser-10 low-pass, cutoff `.45` cycles/internal sample, at 88.2 kHz has nonlinear phase even when its in-band magnitude is nearly unity. After removing its first-moment delay, its phase error is approximately -.0144 rad at 8.372 kHz and -.1126 rad at 16.744 kHz. Applied only to the already-in-band harmonics of the stated nominal model at 8.372 kHz, even an optimistically refitted **common** delay for that pitch leaves approximately -33.36 dB combined saw/pulse/sine residual; the alias contribution is not included. This is an **illustrative candidate**, not a measurement of the production oscillator. The experiment is `/tmp/oscillator-review/phase_risk.py`, SHA256 `2effe43375d8a7fcd14cb95e26f2b737b051816d87b0808200f586ba4d864d01`; JSON SHA256 `8cae0dbc966c059bb4c152ed46af6aa59da746d766bed20748b420af8fdd1ca8`. Its script declares SciPy construction and all kernel parameters. Half-length minimum-phase conversion can also halve prototype stopband attenuation in dB; measure the resulting kernel, not merely its precursor.

The first symmetric replacement considered 64 internal samples, `.40` cycles/internal-sample cutoff and 32 internal samples of delay. The **current standalone candidate** instead uses 16 internal samples, 64 fractional phases, `.50` cycles/internal-sample cutoff and a declared 8-internal-sample group delay (4 host samples on the current 2x path). These remain candidate numerical choices pending the complete alias/CPU/response gates. Measure actual end-to-end delay, including the oscillator's sample-time convention and existing output decimator; do not infer total plugin latency just from kernel support.

The short-kernel candidate is designed for the actual 2x engine path: direct-oscillator error is qualified in the final host-audible band, up to `.25` cycles/internal sample. Its response through the rest of internal Nyquist and its transition band must still be reported; this is **not** a full-internal-Nyquist standalone antialias guarantee. The original direct-full-internal-Nyquist target remains a separate **unmet/unqualified target**, not a pass reinterpreted after changing the analysis band. The short candidate intentionally trades that full-band response for a smaller aligned latency on the actual 2x host path. A full-path oracle must not relabel above-host-Nyquist harmonics folded by the decimator as desired harmonics. Preserve a separate actual-filter-response comparison if useful, but count above-host folds as aliasing, and use total residual to catch aliases landing on legitimate harmonic bins. State whether dBc is fundamental-relative or total-AC-relative; exclude DC from the carrier denominator. For requested fundamentals above host Nyquist, the ideal in-band AC reference is zero, so use a declared absolute or input-fundamental-referenced leakage metric rather than divide by zero.

For the equal `.1034 s` coupling poles, every final branch between events can be written in sample-time units as `C+(A+B*t)*exp(-lambda*t)`, with `lambda=1/(internalSampleRate*RC)`. Let the causal kernel have unit area, support `[0,L]`, and cumulative integrals:

```
S(a)  = integral(0..a, h(s) ds)
E0(a) = integral(0..a, h(s)*exp(lambda*s) ds)
E1(a) = integral(0..a, s*h(s)*exp(lambda*s) ds)
```

With `A,B` recentered at the **current observation time**, the full-history filtered continuation is `C+A*E0(L)-B*E1(L)`. An event whose new-minus-old continuation at the event is `deltaC+(deltaA+deltaB*t)*exp(-lambda*t)` adds the finite residual at age `a`:

```
deltaC*(S(a)-1)
+ exp(-lambda*a) * [deltaA*(E0(a)-E0(L))
                   + deltaB*(a*(E0(a)-E0(L))-(E1(a)-E1(L)))]
```

This includes value, slope and higher-curvature changes, and is exactly zero beyond `L` when all tables use the same kernel and interpolation/integration convention. Event-anchored coefficients instead need the explicit age/exponential factor in the base term; omitting it is an error. Unequal coupling time constants require a larger basis and cannot silently use this formula. Validate fractional-table interpolation independently. Define cold-start prehistory explicitly: zero residual buffers with a nonzero basis imply backward extrapolation of that basis, not zero audio before startup. A stopped exact comparator equality uses the declared strict `u > threshold` convention; one-sided event tests must not manufacture a crossing when the derivative is zero.

Before any audible engine integration, the selected latency must align saw, pulse, sine, noise, note/envelope/control timing and other applicable mix paths, with mode-transition/host-notification tests. Preserving old default-off timing and sample/RNG behavior remains mandatory. Standalone numerical success would not by itself authorize an unaligned engine insertion.

### State continuity and scheduling

The M41 VCO has no note-trigger reset connection in the inspected graph. Continue the oscillator and its two coupling states while the VCA is silent, using the last held key/current. Tick inactive rank oscillators in isolation mode without consuming additional noise RNG values; cache their held-current control state. A deterministic startup charge/phase distribution is an inferred initialization convention, not a hardware measurement. Do not expand this patch into an unreviewed filter-idle or scanner model.

Active-tail reuse retains charge, one-shot timer, coupling states and antialias residual buffers, not merely `getPhase()`. Existing parity mode changes already clear notes/effects and are the permitted hard reset boundary. Cold startup and prepare/reset initialize the new states explicitly. Do not copy the legacy oscillator's random note-on phase into every new parity note; conversely, do not change the legacy RNG draw sequence. New state probes must cover the complete state, while keeping the old API available for existing tests.

## 4. Implementation sequence and ownership

**One audible feature, delivered in reviewable commits:**

1. Add the profile, normalized event core and independent numerical fixture; immediately include a standalone render path producing all WSC branches in volts. No transistor-level SPICE dependency is required.
2. Add branch antialiasing and coupling states; run numerical/source-anchor/sensitivity tests below. The independent fixture must not call the production `tick` or reuse its event scheduler.
3. Add `ReconstructedOscillator` alongside, not inside the arithmetic of, legacy `Oscillator`. In `Engine.cpp`, select it only under the existing `global.parityMode`; consume its actual outputs in the filter/sine paths. Preserve the old call and floating-point operation order in the off branch. State the voltage-to-existing-engine boundary in the release note.
4. Wire idle advancement and full active-tail-state preservation, extend probes and run full-engine tests. Verify first audible renders before starting more oscillator “reference-only” work. The adapter should be reusable by the factory-tone integration without remapping saved controls.
5. Exact-commit CI plus level/latency-aligned listening of isolated saw, pulse, sine and their blends. Update the roadmap row only after production selection and probe coverage are present. Completion is a tested inferred oscillator reconstruction milestone, not all W3/W4/W9 parity.

Do not edit output-stage integration, host validator work, or unrelated device/network permissions as part of this scope.

## 5. Preregistered test matrix

**Qualification freeze: 2026-10-30 UTC, numerical decision `w3-host2x-linear16-v1`.** Commit this profile/gate manifest before final qualification. These are implementation acceptance targets, not measured analog tolerances. Exploratory runs before this commit are non-preregistered and do not establish acceptance. A future change to profile, kernel, analysis domain or gates requires a separately reviewed versioned decision and preserved failed reports.

Frozen candidate: analog nominal profile `w3-current-fold-v1`; symmetric Blackman–Harris-windowed sinc, support 16 internal samples, cutoff `.50` cycles/internal sample, 64 fractional phases; exact equal-RC exponential-basis residuals; declared delay 8 internal / 4 host samples at 2x; zero audio prehistory at explicit reset. The release criterion is the **actual 2x oscillator plus existing 255-tap decimator path**, compared against an ideal-lowpassed reference of the same inferred waveform with only one shared fixed time alignment: total residual below -45 dB and off-harmonic energy below -70 dBc, counting folded aliases rather than excluding them. In-band response of the actual decimator is reported separately, not used to hide alias energy. The original direct-full-internal-Nyquist objective is reported separately as unmet/unqualified; no result below changes it into a full-band pass.

| Test | Matrix and acceptance |
|---|---|
| Charge/timing | 32.7032, 130.8, 261.6, 523.2, 1046, 2093, 4186, 8372, 16744 Hz; additional noncoherent and held-out pitches. Constant-current cycle frequency error <.01 cent after settling; reset duration within 1 ns in event traces; normalized cycle-charge residual <1e-10 in the double reference. Frequency changes in both states must not retrigger reset. |
| Public voltage anchors | At declared 261.6 Hz calibration, raw pin7 3.5 Vpp, TO3 3.2 Vpp, SIO2 3 Vpp within .1% **of the selected nominal model** before antialias filtering. Show both reset and ramp-only extrema and pulse excess. This is construction agreement with labels, not independent hardware validation. Report other-pitch drift rather than normalizing it away. |
| Comparator | Duty proxies .05/.10/.25/.50/.75/.90/.95, thresholds outside both ramp and reset extrema, static and moving thresholds. Between corrected edges exactly two raw levels; all edge timestamps agree with an independent root reference within 10 ns at static controls. Include reset-only pulses and overlapping antialias tails. |
| Shape/sign | Test fold minimum/maximum and clamp points, every PWL knot/slope, oddness/inversion, common phase; compare complex harmonic ratios, not just RMS/absolute magnitude. A deliberate sine quadrature rotation or saw sign flip must fail. |
| Coupling | Step, stopped oscillator, startup, pitch changes and silence recovery; separate capacitor-state tests. Compare to high-accuracy continuous integration; in-band normalized RMS difference <1e-4 on a declared transient fixture after fixed latency alignment, and demonstrate improvement at smaller steps. Do not subtract independently fitted waveform means to hide state errors. |
| Aliasing / in-band response | Host 44.1/48/88.2/96/176.4/192 kHz; actual 2x engine path and direct oscillator path. At unbent pitches through 8.372 kHz **and** the source-supported ribbon-up extension through 16.744 kHz, plus the derived complete original-control adapter range, target off-harmonic energy below -70 dBc and total residual below -45 dB relative to a converged, ideal-lowpassed reference of the **same inferred waveform**. Report harmonic magnitude/phase errors separately. Off-harmonic energy alone misses aliases falling on intended harmonic bins. |
| Modulation | PW/PWM 0, .7, 5, 20 and 60 Hz; stationary and swept pitch, octave steps, source-supported ribbon range, touch pitch transient, block sizes 1/17/64/257. Reference uses identical timestamped control reconstruction. Require finite bounded output and convergence; separately report modulation-sideband residual and worst instantaneous error rather than applying a stationary-bin alias test. |
| State | All 16 rank oscillators; idle advancement; active-tail reuse during reset and while BLEP/BLAMP tails overlap; panic, prepare, sample-rate change, mode toggle, preset recall. No accidental note-on phase lock, extra RNG draw, stale correction tail or coupling-state reset. |
| Full path | Saw/pulse/sine separately and all blends, HP/LP cutoff/Q grid, eight-note chords, ring and original effects on/off. Relative phase must not be independently aligned per branch. Level-match only for listening, not voltage/gain assertions. |
| Legacy-off | Compile old and new revisions separately with matching headers; byte-compare the existing 222-preset corpus at 44.1/48/96 kHz and an expanded pitch/PWM/tail/mode-roundtrip corpus. IDs, defaults, state bytes, reported off-mode latency and RNG sequence unchanged. A zero-depth new-state calculation in the off path is not sufficient proof. |
| CPU / memory | 16 oscillators at low/high notes and worst PWM on the supported build machines; fixed allocation count zero in callback, bounded event storage/loops, no denormals. Record p50/p99/max block times and regression against baseline. A kernel that only sounds good offline does not pass. |

Exploratory local runs on uncommitted snapshots are not exact-commit qualification. The numerical gates above were textually predeclared in this proposal before the implementation experiments, but the proposal was not yet committed at that point. Preserve exploratory failures and snapshot hashes; freeze the tolerance/profile manifest in a commit and rerun the complete qualification on that exact revision before reporting acceptance.

The alias reference must contain the inferred reset plateau and nonlinear branch transfer, not the old ideal falling-saw Fourier series from `DSP_MEASUREMENTS.md`. For stationary tests, integrate the periodic piecewise waveform analytically or with converged high-rate quadrature, remove above-Nyquist harmonics **before** decimation, and check reference convergence. For modulation, use independently integrated 32x/64x/128x sequences with a documented low-pass and convergence result. Keep filter/output nonlinearity out of oscillator-only measurements, then test the whole path separately.

Required mutations: invert restored-current polarity; round reset timing; add reset directly to pulse; change one shaper slope; replace shaped sine with independently phased sine; omit one slope-event correction; reset coupling on note-on; disable inactive advancement; route new DSP into default-off. Each must fail an appropriate test, not merely change a snapshot nobody inspects.

### Uncertainty sweep, not a fictional component distribution

Report nominal versus `tau` 20/30/40 us; plateau excess .225/.45/.9 V; constant versus amplitude-normalized converter gain; midpoint slice ±.05 V; triangle/sine limiting enabled versus unclipped continuation; 2/4/8 positive shaper segments; ±10% breakpoint perturbations with monotonic slopes retained; and ideal versus explicitly finite source/receiver impedances. These are **stress hypotheses**, not measured confidence intervals, manufacturing tolerances or permission to add random drift. Publish high-register spectra and sine/filter blend differences. Select a nominal on public evidence, bounded behavior and listening; leave the alternatives identified for later refinement without making hardware access a prerequisite.

## 6. Source and calculation provenance

New reads were visually inspected 2026-10-29. Public source facts above use original Yamaha pages or Rider's original-device observations; his replacement-module section is excluded. Images in `/tmp/w3-proposal` are local inspection artifacts, not committed assets or permanent download links. The URLs/hashes and render/crop recipe below permit independent reconstruction. Crops use `[left, top, right, bottom]` in pixels; no crop is claimed native scan resolution after a PDF render.

### Yamaha IC guidebook

Source: https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf

Source PDF SHA256 `ff051aed0687cad4c4b246d145f85896c229e4cc679333328483118a3e7f8cc1`, 13,608,992 bytes. Local `/tmp/w3-audit/guide.pdf`.

Rendered physical PDF35 and39 with `pdftoppm -f N -l N -scale-to 3600 -png -singlefile`; both 3600 x 2544. Page35 full PNG SHA256 `fac1977c6a8a3c83f22fc6a3c8f575853b01bf3a2b5ade555918b784524064ff`; crop `[1300,400,3400,1450]` SHA256 `59faec3bec8a9c2b396af4081f0d22697b49a948f5bb60a9b78833b6680bc689`. Page39 full PNG SHA256 `f5f3e1eddc21e0f9814ef61f9d34141ae4602a581589878d4db675e7690f1b2d`. These identify current/reset and WSC block graphs, not hidden transistor parameters. Previous smaller renders have different expected hashes.

### Original CS80 sheets41–42

- https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg
- https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/42%20-%20M%20Circuit%20ICs.jpg

Both 5100 x 3300. Sheet41 source SHA256 `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`; established crop/source routing is in [OSCILLATOR_CIRCUIT_EVIDENCE.md](OSCILLATOR_CIRCUIT_EVIDENCE.md). Sheet42 source `/tmp/w3-audit/ic42.jpg`, SHA256 `5698fdfd19600f6b0b8b8f9dfdfc70723248b4b33e8daad39b9f9c0ee11cc24d`. New native WSC crop `[1270,250,2700,3250]`, 1430 x 3000, SHA256 `90c14087c370f03296739a5e56349da698d6c0b0da581cf9b4cba8c1e1c08218`; shows fold, inverter and comparator sketches and nominal triangle/sine amplitudes. Existing VCO detail reviewed at `/tmp/w3-audit/ic42-vco.png`, SHA256 `f87c9e9bbf8f40898d06192c68f3f5e68e3cf56f3432f0e4bcf83dd3a5f614cd`; its pre-existing crop box was not reconstructed, so use the full source for independent frequency-table verification.

### Yamaha CS20M/40M manual: family principle, not CS80 board values

Source: https://www.polynominal.com/sample-packs/yamaha-cs20m/Yamaha-CS-20m-service-Manual.pdf

PDF SHA256 `ee319c12286165d2286cce52c9d6f2f8d96ce471fbfb04b41125a7a29425eb14`, 6,172,162 bytes. Render command `pdftoppm -f 47 -l 49 -scale-to 2400 -png source.pdf prefix`.

| Physical PDF page / printed pages | Render dimensions / full PNG SHA256 | Inspected focused crop / PNG SHA256 |
|---|---|---|
| 47 / 47–48 | 2400 x 1697 / `3891aaa43ef72d26978db77440fd4006a4ce19d7b98321c8ebab8066fd73a7ce` | `[1215,170,2310,1525]` / `b869e1fe08bb7c2adb0d81b1fcd4e040aa1466b3ececfa90bb41c0c88ed4a358` |
| 48 / 49–50 | 2400 x 1697 / `aecd87d5b1f0778eaaf2e5398e19bc8acc00295b88a4fc273df531ee6bf99054` | `[30,875,1660,1580]` / `781a9371754e974fbba3a8d7479483cbb1a002ef31f3b47876d9fbe3e0bbcbb0` |
| 49 / 51 | 1698 x 2400 / `dd8b51b766ce0d997fa0db5b39741b4287d3c00c5a6ff582563f017df474a593` | `[180,1040,1600,2200]` / `00d8ed7927180b92ac148c0a0bcc4b633ecb69f16f11b3f591e0ec9c5dee81a9` |

The first row preserves the switch-polarity contradiction; the second establishes the inverting diode-feedback shaping principle; the third shows comparator followed by clamp. Do not confuse physical PDF page49 with printed49: PDF48 is the spread containing printed49–50.

### Scott Rider original-IC photographs

Article: https://www.cs80.com/vco.html

Original pin7 photo https://www.cs80.com/DSC00038a.jpg SHA256 `2becf64add14e2cf0082bb159814be93ecff8abcac3bd57e890d1354e05de7cf`; reset close-up https://www.cs80.com/DSC00040.JPG SHA256 `966eaa49785e267ab9572e6189c3aea76f00215b89791adb7193edbc7cd9cbcc`; inverted WSC output https://www.cs80.com/DSC00041.JPG SHA256 `2b4be546f5012acac62ee2cc5eaae741d2e0ae0f0edb0723c3c75a964e7103ef`. Full images inspected without a crop, each 1152 x 864. These are photographed scope observations with limited calibration/provenance, not a digital waveform reference corpus. The article's RC arithmetic contains a notation typo; 24.5 kOhm x 1200 pF is 29.4 us.

### Calculations in this proposal

`/tmp/w3-proposal/profile_checks.py` SHA256 `8f41e4c0e3627b89f9f3730eebb4da60dd1284a96539a17b6b328c329377fbad`; JSON SHA256 `fef7fc3ea17f44e5cb312554d0b30ad33b39abfd83fca915a67e1f6a122b62bc`. It evaluates the stated table, inverse conductances, reset prediction table and a 2^20-point ideal triangle/shaper FFT. It does not invoke production code.

`/tmp/w3-proposal/periodic_profile_check.py` SHA256 `aefbb2fbb6729047a2825b45fdef686627a2ed1b40133d7a0a28fac598e79738`; JSON SHA256 `7d6c08f3b96882f22edf21c3325aa689f231ea90dfb96d4defbcff3b5bc00b74`. It samples the selected periodic pin7 expression, applies the continuous high-pass transfer `s*RC/(1+s*RC)` by FFT, folds/limits, applies the second high-pass and the PWL shaper. At 2^17, 2^18 and 2^19 samples/period the derived sine scale is respectively 1.5017768423, 1.5017768264 and 1.5017767959 V. This demonstrates convergence of that calibration estimate, **not** real-time solver quality, alias rejection, hardware fit or CI success. Recreate these equations in the independent fixture and archive its exact-commit artifacts when implementing.

No action is blocked in this proposal. Unresolved chip laws are identified model choices and sensitivity work; they are not a request to wait for hardware.
