# Bass Stack retrigger replay (2026-11-11)

**The original click gate remains FAIL/open. No DSP, preset, metric, window or
threshold changed.** This targeted replay is not a full-bank scorecard refresh.

## Source and unchanged measurement

Baseline: `7ec68694d7ac3e621c3beda2cec23cbd0a8b4005`. A standalone probe includes
that revision's unchanged `src/tools/engine_audit.cpp` with its main renamed,
selects `Bass Stack` from `programSetups()`, and uses the original `phrase()`,
`render()` and `analyse()` at **44,100 Hz**. The physical PANEL setup and default
oversampling are unchanged. Compiler: GCC 14.2.0, `-std=c++20 -O3 -DNDEBUG
-DCS80_DISABLE_AVX2_FILTER_BATCH=1` (generic scalar).

The full 12-second phrase reproduces `click_retrigger` **1.51052 in source** and
**0.629321 in Legacy**, with no nonfinite samples, no samples above full scale
and no active voices at the end. These are software measurements, not hardware
fidelity claims.

The original 64-frame event rounding places the second note-on at host frame
**275648**. Its unchanged 8 ms window contains 353 frames, **[275648, 276001)**.
The source numerator is approximately **0.01222316176**, at frame **276000** in
the right channel, **7.981859 ms after MIDI**; the denominator is approximately
**0.00809204392**, the maximum jump in the original **[3.00, 4.40) s** chord
window, including its attack. The maximum lies on a multi-sample waveform slope.
Legacy's absolute numerator is larger, approximately **0.01655519754**, but so
is its denominator, **0.02630643547**. Cross-policy normalized scores alone do
not establish a larger absolute discontinuity.

## Event omission and state inspection

Independent engines replay the identical prefix, then omit exactly the selected
note-on in the control. Every pre-event PCM sample is bit-identical. All scores
below retain the original chord denominator; event-only derivatives are
**diagnostics, not substitute acceptance gates**.

| Event | Original score | Event omitted | Peak after MIDI | Event-only maximum, first 1 ms | Event-only maximum, 6–8 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| First strike, 6.00 s | 1.1796999712 | 0.0226192433 | 6.734694 ms | 0.0000262983 | 1.1734595677 |
| Retrigger, 6.25 s | 1.5105159927 | 0.0309964902 | 7.981859 ms | 0.0000972700 | 1.4981393791 |

The note-on is necessary for the late excess slope. The first ordinary strike
also exceeds the same normalization. This does not establish that the excess is
merely old-tail beating, nor that it is an assignment-instant discontinuity.

Allocation selects inactive, previously unused circuits **5 and 6**. A separate
reference engine completes pending idle catch-up before the zero-time state
comparison. That legitimate catch-up changes **38 and 46** observed fields,
respectively; stale sleeping state must not be mistaken for a MIDI reset.

After catch-up, **2 of 144 compared fields differ at each event**: each rank's
filter-envelope charge initializes **0 → −1.43999993801**. Current `startVoice`
deliberately initializes a cold/reset filter envelope to its initial level when
`envelopeReset` is true. Both circuits are first-use circuits, not reused sounding
tails. The other **142 compared fields** match: oscillator state and residuals,
nodal filter histories, VCA level/coupling, phase proxy, and amplitude-envelope
charge. Gate stages, global/control state and unobserved fields are not covered
by that equality count.

Diagnostics used an isolated source copy with only a friend declaration and
read-only VCA getters added. No audio logic changed. Its first-strike and
retrigger baseline PCM prefixes (**2,127,384 and 2,215,584 bytes**) are each
byte-identical to the unmodified full-phrase source render. The initial ported
zero-field-change assertion failed on the two cold filter initializations;
the revised diagnostic reports them without declaring the click gate passed.

## Prior work and conclusion

Historical commit `56c2616a7c8ee9e42e6985ad80bb93dbdcaf745a` contains
`docs/evidence/BASS_FUNKY_CLICK_ZOOM_20261102.md` and `BassFunkyEventZoom.cpp`.
That study's 48/96 kHz Bass results and all-field equality assertion belong to
its different source snapshot. This replay uses its experimental approach but
**does not import that equality claim** to current source. The physical PANEL
coordinate must not be replaced by the separately studied electrical-programmer
coordinate.

The present evidence identifies an event-dependent late attack slope, without
establishing a bounded oscillator/filter-history/VCA reset or sounding-tail
removal defect. The reference is a different multi-note event with different
phase/envelope history, so a ratio above one alone does not prove a discontinuity.
These observations also do not establish that the attack sound law is correct
or exclude unobserved control contributions. **No causal DSP fix is justified by
this replay. The original failure remains open**, with no smoothing, preset
change, coordinate substitution or threshold relaxation.

## Retained evidence

Local investigation bundle: `retrigger-evidence/` (retained outside the repository).
It contains the source snapshot, compiler identity, harnesses, full PCM,
complete build/run stdout and stderr, exit statuses, and source/artifact hash
manifests. No raw PCM is added to this documentation change.

| Artifact | SHA-256 |
| --- | --- |
| Exact audit harness `probe.cpp` | `7a281152d4d2e79dead330c8cb72658cf2e976316737f9026c40deb1c63ae84f` |
| Original audit rows `baseline.csv` | `5d324e77a7f083a8848ca9f8b12b46aa3d4514649856e680e3ecbe710cb3b582` |
| Adapted diagnostic `zoom.cpp` | `f663b2f76fa9d78b471d00203243c8f3fd1ae998b1cb5380c6d63e91779eb714` |
| Complete diagnostic output `zoom.txt` | `acb377528f65b9f80c6cfd3b6295334756d5abfdff3f4b634b99a49d7f6e610a` |
| Diagnostic-only source patch | `c026247dc661e8810ec72824a0dc920d106ef441866c48e2428e94b2e94ddafb` |
| Original source PCM `source.f32` | `53573d3c1e560fdc84b033b837dfa05e6b39d60441289d6e6b0abc6365e9fd73` |
| PCM prefix comparison | `ae8448778738decdc5063be0e03470d98b8059b8059613d3a1a59503f022dad7` |

The exact audit and final reporting diagnostic exited 0. Earlier compile/API and
zero-state assertion failures are retained, not hidden; diagnostic completion
is not an acceptance pass. No production regression suite was needed or claimed
for this read-only DSP investigation and documentation-only follow-up.
