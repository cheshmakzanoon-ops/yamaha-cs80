# Public amended-mark tone estimates — preregistration

2026-11-02. User scope: implement the closest defensible reconstruction
using public information; no hardware is available or requested. This
separate estimated policy is **not a promotion of the 25 cells to source
facts**. The immutable classified table, V1 median overlay/fingerprint,
seven complete-tone corpus, Legacy programs/IDs and original audio gates
remain unchanged.

## Decision before evaluating sound or controls

Prefer the final legible amended mark over an unrelated same-output
median. Where a resistor is visibly crossed with a revised value below,
use the revised value. For the ambiguous T53 B14 glyph use the separately
readable S36 `75K(39K)` board candidate and the independently supported
T53/T51 pairing. For an altered diode/bypass branch choose the drawn
conducting direct branch as a **low-confidence topology estimate**, not
a discovered installed part; retain open and resistor alternatives.

Primary readings and exact source-image hashes are in
[the native 25-cell review](TONE_CELL_NATIVE_REVIEW.md) and
[the Yamaha parts-list cross-check](TONE_CELL_PARTS_LIST_REVISION_AUDIT_20261102.md).
The latter corroborates board families only, not an effective serial range.

| Cell | Selected estimate | Basis / confidence |
| --- | --- | --- |
| T51 A20 | 1.2 kΩ | legible marked value; moderate |
| T51 A25 | direct | altered diode topology; low |
| T51 C25 | direct | bypass interpretation; low; retain 10 kΩ |
| T51 E22 | 12 kΩ | legible marked value; moderate |
| T51 E26 | direct | altered diode topology; low |
| T51 F22 | 12 kΩ | amended value, crossed 3.6 kΩ retained; moderate |
| T51 F25 | direct | crossed resistor/diode interpretation; low; retain 5.6 kΩ |
| T51 F26 | direct | marked diode topology; low |
| T52 A26 | direct | altered diode topology; low |
| T52 B9 | 10 kΩ | amended value, crossed 12 kΩ retained; moderate |
| T52 C22 | 12 kΩ | legible amendment; prior glyph unresolved; moderate |
| T52 E20 | 1.2 kΩ | legible marked value; moderate |
| T53 A20 | 1.2 kΩ | legible marked value; moderate |
| T53 A25 | direct | altered diode topology; low |
| T53 B14 | 75 kΩ | paired-board candidate, ambiguous 25 kΩ alternative retained; moderate |
| T53 B20 | 1.2 kΩ | legible marked value; moderate |
| T53 C20 | 1.2 kΩ | legible marked value; moderate |
| T53 E23 | 22 kΩ | amended value, crossed 16 kΩ retained; moderate |
| T53 E26 | direct | altered diode topology; low |
| T53 F22 | 12 kΩ | legible marked value; moderate |
| T53 F26 | direct | altered diode topology; low |
| T54 A26 | direct | altered diode topology; low |
| T54 C22 | 12 kΩ | amended value, crossed 8.2 kΩ retained; moderate |
| T54 C25 | direct | crossed 56 kΩ/diode interpretation; low |
| T54 C26 | direct | altered diode topology; low |

Confidence is qualitative source-selection confidence, not a probability
or physical tolerance. The full excitation domain remains `[0,1]`.
No selection may change after listening simply to reduce a click score.

## Selected-policy metadata hardening, before evaluation

The independent reviewer identified that a policy-2 version/hash must not
authorize a partial or extra-cell override mask. Before changing the
readout guard, test every one-bit difference of the 26-bit override mask
for every inferred tone and grade combination. The readout must reject
each altered mask without modifying any output field. Retain a positive
V1 partial-sensitivity case: V1 intentionally permits explicit partial
research overrides, and this new guard must not change that behavior.
The policy-2 values, fingerprint, selection rationale, and audio remain
unchanged. Run red before the guard, green after it, and an isolated
guard-deletion mutant followed by restored green.

## Implementation and tests fixed in advance

- Add a separately versioned `public-amended-marks-v2` policy and explicit
  `compileTonePublicEstimate` entry point. The current ordinary
  `compileTone`/engine default stays unchanged during this study.
- Preserve original unknown/inferred/known masks. Carry the selected
  policy version and independent manifest fingerprint in provenance;
  `hardwareMeasured` must remain false.
- Test every literal selected value against independently written
  addresses/ohms, `E=10·x` and `Rth=10000·(1−x)`, where
  `x=10000/(10000+R)` and direct `x=1`. Relative numeric comparison limit
  `1e-12`, fixed before evaluation. No amplitude or spectral fit is used.
- Compare all 22 compiled tones with the original path; the seven fully
  classified tones must remain exactly equal. Every classified source
  equivalent remains equal. The 25 exclusion addresses stay unknown.
- For every cell retain unselected sensitivities at `x=0`, `x=1`, old
  median, all visible alternatives, and the reviewed board candidate.
  Report all computed controls, with no after-the-fact subset selection.
- Check all grades, eight-voice dynamic readouts, malformed addresses,
  forged policy metadata and no-output-on-failure behavior. Existing
  source-tone/V1 integration suites must pass unchanged.
- Run the new literal-value suite against the explicit old-median test
  arm first (`CS80_TEST_PUBLIC_ESTIMATE_BASELINE`), then the new API.
  Mutate the new API back to old median and each selected-cell value in
  isolated copies; require the locked suite to detect them.

Commit this document and the independent expected-value tests before
numeric evaluation. Public/default engine adoption is a separate reviewed
step: original 885-row audio inventory and thresholds, alias/FX/latency,
state/host compatibility and performance must be checked against the
candidate. No source-model improvement is an automatic click PASS, and
the HP CP3 public-spec discrepancy is not closed by a tone-cell policy.

## Independent-review extension, before correlated evaluation

The qualification reviewer accepted the separate non-default estimate
as defensible but identified two gaps in the initial test implementation.
The additional contract is recorded before evaluating these new cases:

1. Compare every semantic numeric control of the seven complete tones
   **bit-exactly**, including signed zero, plus identities/flags and dynamic
   cutoff readouts. Do not use a whole-object comparison with padding.
2. In addition to individual-cell sweeps, report every one of the 22 tones
   under three **correlated** arms: old V1 median; all selected amendments;
   and all low-confidence direct/bypass choices changed to open together
   while selected resistor choices remain. Emit all semantic controls for
   all arms, including unaffected tones. This prevents individual sweeps
   from concealing the joint effect of the 12 direct-branch assumptions.

This strengthens verification; it changes no selected value, limit,
source classification or adoption condition. Initial baseline-arm results
are historical results of the first test revision, not the enlarged suite.
