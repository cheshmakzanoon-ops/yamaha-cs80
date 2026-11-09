# Exact legacy SUB oscillator regression oracle

This is a test-only repair of a compiler-dependent false failure. It changes no
Engine, oscillator, RNG, smoothing, parameter, host or saved-state behavior.
The implementation is `1782677`, the preregistration is `3c45847`, and the final
fault-control checker is `c95e1fa`. The comparison base is published `4ef0666`.

## Why the old native test failed

The original CoreTests reference wrote the same source recurrence as Engine:
`speed += (target - speed) * .0015f`. GCC 14.2 `-O3 -march=native` contracted the
Engine multiply/add but emitted separate multiply/add instructions in four
waveform paths of the test oracle. At host frame 398 of the existing 7 Hz,
48 kHz, 512-frame fixture, reference speed was `0x1.89e99cp+2` and Engine speed
was `0x1.89e99ep+2`, one binary32 ULP apart. The waveform still agreed at the
first discrepancy. Native Engine matched explicit fused arithmetic; generic
Engine matched explicitly separated arithmetic. Original tests against the
unchanged native core reproduce four failures among 11,508 checks.

A scratch outlined reference removed those failures, but an optimizer-specific
`noinline` arrangement is not a portable arithmetic contract. Neither DSP flags
nor a floating-point tolerance was changed to make the test pass.

## Two complete, explicit arithmetic trajectories

`tests/LegacySubLfoOracle.h` maintains two independent references. Both force a
binary32 subtraction. One then uses `std::fma`; the other forces a binary32
product followed by a binary32 addition. Each owns its speed, phase, held sample,
started flag and literal xorshift32 state. It does not call the production Lfo
or Rng. Phase-domain power-of-two waveform scaling is exact, so the admitted
choice concerns this speed product/add, not arbitrary extra rounding differences.

At each **host-frame boundary**, after references advance both internal ticks,
the test compares both exposed speed and waveform by finite float bit pattern.
Each arithmetic choice has a sticky eligibility bit. A mismatch clears it for
the rest of the scenario. One complete trajectory must match every observation;
speed and waveform cannot choose different tracks, and a rejected track cannot
revive. Internal DUT states are not directly observed. Sine uses the same runtime
libm, not a cross-libm golden or hardware waveform reference.

All scenarios use fresh Legacy Engine instances, no voices, no arpeggiator or
modern routes, and a fixed waveform. `Engine::reset` retains legacy speed and
modulation RNG history; this fixture does not invent restart semantics or claim
to qualify reset/reprepare. Existing neighboring lifecycle tests remain intact.
Fast-math and changes to the floating-point environment are outside this gate.

## Coverage and evidence

The original six 7 Hz / 512-host-frame comparisons remain in CoreTests. The
separate registered test covers 54 scenarios: those six, then six waveforms at
44.1/48/96/192 kHz for a 30 Hz, 0.4-second run and a 7 → 30 → 0.05 Hz schedule
with three 0.15-second segments. Actual Legacy policy, rates and factor two are
checked. Both references must reach at least four phase wraps in every expanded
scenario; sample-and-hold draw count equals wraps plus its initial draw, random
wave draw count equals internal ticks, and other waves consume no RNG.

The validator also rejects nonfinite values, a one-ULP value outside both valid
outputs, mismatched speed/waveform fields, and a splice with genuinely distinct
prefix and suffix contracts. It verifies permanent rejection after a mismatch.

- GCC 14.2 and Clang 19.1.7, `-O3 -DNDEBUG`, generic and `-march=native`: each
  passes **54 scenarios / 1,958,109 checks**, the **11,508-check core suite**, and
  both registered focused CTests. Every generic scenario retains only the split
  track; every native scenario retains only the fused track.
- Complete core and focused/core test targets built with GCC `-O1 -g1`, ASan,
  UBSan, float-cast-overflow and libstdc++ assertions: both pass those same
  counts. Leak detection was disabled. This is not a plugin/GUI sanitizer run.
- Both GCC core archives are byte-identical to their matching published-base
  builds, in addition to every production source byte remaining unchanged.
- The complete configured default core-only CTest suite passes **142/142**,
  zero failures, normal exit 0, in **907.75 seconds**. This is the completed
  restart; the earlier interrupted attempt ended after 35/142 and remains
  incomplete. Neither run is native plugin validation.

All seven source fault controls must complete 54 scenarios and exactly 1,958,109
checks, exit normally with status 1, and emit one terminal summary. Compile or
runtime failure cannot masquerade as numerical detection.

| Fault | Failing checks |
| --- | ---: |
| Wrong speed smoothing coefficient | 1,941,582 |
| Extra modulation RNG draw | 647,194 |
| Missing sample-and-hold draw on wrap | 254,455 |
| Phase reset before each tick | 1,582,342 |
| Missing phase wrap | 1,272,275 |
| Alternate LFO ticks omitted | 1,836,797 |
| Oracle eligibility revived every observation | 7,396 |

## Independent review and limits

The independent reviewer approved the explicit two-track source contract and
ran the focused GCC native gate: 54 scenarios, 1,958,109 checks, zero failures,
with fused-only eligibility throughout. The reviewer separately reran the
coefficient and revived-eligibility fault binaries, reproducing 1,941,582 and
7,396 failures with normal exit 1. The other five fault binaries were reviewed
through their source changes and complete logs, not independently rerun.

The final `c95e1fa` checker review accepted two valid parser inputs, rejected 11
invalid forms, and verified all seven final restarted log/count comparisons
against the first complete run. It is source/parser/log review, not an independent
compiler-matrix, sanitizer or full-suite rerun. The archive records those scopes
separately and retains the interrupted attempts without assigning them a pass.

The two pitch-cache candidates remain separately rejected on their unchanged
cost gates. This test repair supplies no speedup, no CPU-target completion, and
no new physical-model, DAW or release qualification.

See the [preregistered contract](experiments/legacy-lfo-oracle/preregistered.md)
and [reproduction/evidence guide](experiments/legacy-lfo-oracle/README.md).
