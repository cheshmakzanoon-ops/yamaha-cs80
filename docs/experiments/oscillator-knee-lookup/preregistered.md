# One-candidate oscillator knee lookup: acceptance plan

Base `35f3431400b28d95d41ef6c6a863db0516edfa01` (source `87405ee`).
This plan is frozen before implementation or candidate timing. The earlier
prepared-quotient cache is rejected; none of it is included in this experiment.

## Evidence for choosing this path

A fresh GNU gprofng run of the unchanged current 15-case Source Performance
quick benchmark completed, but its experiment header reports that the collection
interval timer changed from 997 to 0 and profile data may be unreliable. Its
0.212-second sample total over 21.367 seconds is not accepted as a representative
cost share. The raw warning and function table are retained, not used to claim
that a measured percentage of CPU will be removed.

Deterministic source-instrumented counters on two existing eight-voice cases
(dry Source Performance and MPE + chorus/reverb, 48 kHz / 128 frames, selector 0,
256 held callbacks plus the existing warm-up/sustain/release phases) show:

| Case | nodes calls | knot comparisons | actual knot adjustments |
| --- | ---: | ---: | ---: |
| Dry | 3,103,315 | 12,413,260 | 198,808 |
| MPE + FX | 5,216,673 | 20,866,692 | 227,204 |

These are execution counts, not speed estimates. Timing from the instrumented
run is not a product benchmark. The production four-knot loop repeats four
proximity comparisons on every nodes evaluation. It is a candidate for a bounded
local reduction without introducing segment state or changing event search.

## Implementation contract and truth gates

Replace only that loop with one nearest-knot candidate. For ordered `a` inside
`[.2,1.1]`, `int(a*4+.5)` is safely 1..4. Any old match is within `2e-11` of
`k/4`, so the integer expression remains approximately halfway between k and
k+1 and selects the same k. Outside the coarse range no old knot can match.
The four neighborhoods cannot overlap; the old adjustment cannot reach a later
knot. Keep the original strict comparison and exact assignment, including
`aSlope*double(side)`, for signed-zero side behavior. Bounds must precede the
integer conversion, so nonfinite inputs do not enter it. Keep static shape,
profile/rate math, state, events, residuals and failure handling unchanged.

1. Direct DUT helper bit comparisons against the literal old loop around each
   knot and threshold boundary, coarse guards, nearest-bucket boundaries, signed
   zeros, slope/side signs, infinities and quiet NaNs. Include the fourth-knot
   derivative transition and randomized finite magnitudes.
2. Full public oscillator output/state/event bit corpus, generic/native GCC14
   and available Clang19. Fixed floating-point environment; no promise of changed
   rounding/exception-flag behavior. No tolerance, compatibility pin or FP-flag
   relaxation if a profile fails.
3. Unchanged oscillator numerical, promotion and continuity CTests; strict
   Legacy/default-tail/Circuit V1/V2 compatibility; sanitizer and behavioral
   mutation checks. Allocation and object-storage behavior must remain unchanged.

## Cost gates before shipping

Reuse the previous complete-object paired probe with only candidate/baseline
names changed. Identical precomputed inputs and warm-up, all three returned pins
consumed, final complete public state hashed, per-call complete-object read-only
compiler observer. Test 16 and 32 oscillators at 96 kHz: held low/middle/high,
moving pitch/PWM, dense controls, idle. Nine alternating pairs per case, 32,768
measured internal frames. Setup/hash work stays outside timing. Preserve every
row, checksums, source/compiler identities and child affinity observations.

Primary: pooled median of the 45 non-idle 16-oscillator paired reductions >=3%
in both generic and native GCC release profiles. Secondary: no individual case
median paired regression >3%, including 32-oscillator/moving/idle cases. If noisy,
a complete longer matrix may be added with all original rows retained; never
select favorable pairs or silently relax the gate. If the component gate passes,
confirm the direction with bounded paired eight-voice Engine workloads before
integration. A component pass alone does not meet the whole-instrument CPU/idle
targets, qualify real-time deadlines, or justify a segment cache.
