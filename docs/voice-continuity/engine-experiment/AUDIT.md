# Independent immutable-Engine adapter audit

**Functional result: PASS, not production or timing qualification.** The frozen
independent harness is `EngineContinuityAuditTests.cpp`; exact source hashes,
compiler/flags and results are in [audit-results.json](audit-results.json).
No checked-in production Engine, DSP, Params, host code or golden test changed.
Baseline: `428e4b2bb0b4bcee0a8f3bb8826449f699bdb618`.

## Compatibility evidence

One common harness was compiled separately against pristine428 and the patched
candidate, each with its own headers. The candidate-only define is
`CS80_CONTINUITY_AUDIT_CANDIDATE=1`. GCC14.2, C++20, `-O2 -Wall -Wextra -Werror`.
`cmp` returned0 for all three final comparisons:

| Candidate profile | Cases | Bytes in each corpus | SHA256 (both sides) |
|---|---:|---:|---|
| Default disabled |126|10,183,824|`f1c46b9ba7c756a869aea68eee61ba1c0460ace32653dfda774ab46e73ddc4aa`|
| Explicit disabled |126|10,183,824|`f1c46b9ba7c756a869aea68eee61ba1c0460ace32653dfda774ab46e73ddc4aa`|
| Enabled but masked by Legacy/Isolation |84|6,789,216|`c876e6ac62e26c7b4a0ff97e9d3518b7ee4cbd5e0ce47af5fbeca82286d0fbe1`|

Coverage:44.1/48/96kHz, Legacy/Source Performance/Isolation, all14 selector
identities paired with a second rank source; live PANEL/MEMORY/source edits,
CC1/64/74/120/123, expression, MPE/poly-pressure, ribbon, release, hard Panic,
reassignment, `prepare`, and effective policy transitions. Fixtures deliberately
include noise, dispersion, drift, modulation and FX. The masked lane never
enters Source Performance. These new corpora supplement, not replace or relax,
the historical golden gates. Raw binaries remain temporary; the harness and
hashes make the comparisons reproducible.

## Enabled contract and independent oracles

Final native and ASan+UBSan runs each passed **8,051 checks,0 failures**:

- Explicit cold note/K/seeds, phase draw order, zero SVF histories, supported
  rate endpoints, absent invalid-index/masked diagnostics; zero cold K stays
  zero without invented pitch. Effective changes reset; identical/masked edits
  do not. Invalid profile transactions preserve state **and future audio/state**.
- Dirty reset, new sample rate and policy reentry compared with independently
  initialized engines. Sustain II exposes stale glide/target/contour state that
  a held Sustain I node could hide.
- Every source filter EG compared exactly against a disabled428-path Engine:
  its existing single owner is retained. PANEL/MEMORY filter EGs compared with
  a still-active long-VCA oracle after the candidate's short VCA retires:
  exactly one continued tick, not a second source tick.
- Both rank phases and HP/LP memories advance while muted and bypassed.
  Public audio diagnostics refer to those actual kernel memories.
- After hard Panic, changing hidden circuit excitation changes filter memories
  but **does not change the paired audible output**. Stale amplitude is idle.
  No immediate digital-zero assertion is made about retained FX, decimator,
  output-circuit history or noise.
- Sustain I holds K on release; Sustain II continues the assigned circuit's
  upstream glide. A new other-circuit note does not overwrite its retained
  target. Retired member expressions clear. These test the declared nominal
  adapter, not an inferred CS-80 KAS allocation law.
- Different idle-only noise seeds alter idle memories without changing the
  active circuit or shared audible/output stream. This explicitly tests the
  surrogate stream boundary, not physical common-noise topology.
- Exact audio/state/counters across varied block partitions and repeated
  identical frames, with source changes and MIDI/CC/pedal timelines.
- Domain-v2 multiplier262144 accepted; its next representable float rejected
  transactionally, including future coefficient evolution. Extreme bend/
  footage/ribbon fixtures with actual external SUB +/-1 input remain finite
  with zero rejected samples. A3-second high-Q/noisy muted/bypassed run remains
  finite with exact internal sample counters. These bounded fixtures are not
  exhaustive numerical, aliasing or analog-accuracy proofs.

The separate frozen kernel audit supplies all four electrical NAND combinations
and direct primitive-routing oracles. This full-Engine audit does not silently
replace those electrical tests with a musical key-allocation assumption.

## Mutation sensitivity

Five isolated temporary Engine mutants compiled and exited1 under the final
8,051-check harness; original candidate remained untouched:

| Exact fault injected | Failed checks |
|---|---:|
| Change `if (!v.active && !continuity) continue` to unconditional inactive skip |444|
| Tick source filter EG twice only when continuity is effective |864|
| Omit inactive hard-Panic amplitude reset |80|
| Route idle noise draws through shared `audioRng` |1|
| Omit cold `glidePitch = targetPitch = note` initialization |16|

Mutation rejection demonstrates sensitivity to these faults, not completeness.

## Reproduction and scope

Use [reproduce.sh](reproduce.sh) to extract pristine trees and apply the pinned
patches. Its optimized native suite also retains the established factory gates.
For the independent commands above, use the same source layout and these flags:

```sh
# B is extracted pristine428; C is the independently patched candidate.
H=docs/voice-continuity/engine-experiment/EngineContinuityAuditTests.cpp
g++ -std=c++20 -O2 -Wall -Wextra -Werror -I"$B/src" \
  "$H" "$B/src/core/Engine.cpp" -o baseline-audit
g++ -std=c++20 -O2 -Wall -Wextra -Werror \
  -DCS80_CONTINUITY_AUDIT_CANDIDATE=1 -I"$C/src" \
  "$H" "$C/src/core/Engine.cpp" -o candidate-audit
./baseline-audit --raw baseline.raw
./baseline-audit --raw-masked baseline-masked.raw
./candidate-audit --raw default.raw
./candidate-audit --raw-disabled disabled.raw
./candidate-audit --raw-masked masked.raw
cmp baseline.raw default.raw && cmp baseline.raw disabled.raw
cmp baseline-masked.raw masked.raw
./candidate-audit

g++ -std=c++20 -O1 -g -fno-omit-frame-pointer \
  -fsanitize=address,undefined -Wall -Wextra -Werror \
  -DCS80_CONTINUITY_AUDIT_CANDIDATE=1 -I"$C/src" \
  "$H" "$C/src/core/Engine.cpp" -o sanitized-audit
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./sanitized-audit
```

No leak check, TSan run, host/plugin teardown test or CPU measurement is claimed
here. Sanitization covers this permitted offline numeric/core experiment only.
Prior kernel timing cannot qualify full Engine, DAW real-time behavior,
anti-aliasing, or a recovered electrical model. Functional review found no
blocker in the tested contract; production adoption still requires the separately
predeclared full-Engine cost/quality gates and an explicit integration decision.
