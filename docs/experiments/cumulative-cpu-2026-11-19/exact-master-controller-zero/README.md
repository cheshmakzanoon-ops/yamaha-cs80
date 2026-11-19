# Exact masked master-controller zero shortcut

When five controller targets and their five histories are all raw positive
zero, the original finite-coefficient recurrence is exact and snaps to the
same positive-zero targets. Omit those recurrences only in the existing
qualified masked/nearest render environment. Their snapshots, expression
optics, ribbon, per-voice controls and target restoration are unchanged.
The ordinary entry and fallback compile the same underlying body with the
original statements and no new gate. Negative zero, nonfinite and nonzero
tracks, and values returning to zero execute the ordinary recurrence.

An initial ungated implementation was deliberately not shipped: it matched
ordinary output and masked snapshots but changed a partially committed
controller value under real unmasked interruption. The scoped implementation
matched 80 actual controller interruptions and two continuation snapshots on
each GCC build, plus 22 full renderer cases with seven actual traps each. The
45,760 masked direct snapshots per compiler checked every target/history,
signed zeros, subnormals, masked NaNs, sticky flags and varied rounding.
All ten true native VST3 histories matched public PCM, FP, saved state,
metadata and latency in both toolchains.

The [GCC 14](gcc14-work.json) and [GCC 13](gcc13-work.json) true 48 kHz
VST3 controls recorded −0.873%/−0.904% fresh quiet and about −0.19–0.20%
held guest instructions. All listed reads and writes decreased. The
[fully active two-zone MPE window](active-mpe-work.json) with both bends,
mod wheel and channel pressure moving remained essentially neutral
(−0.00118% guest instructions); each controller was independently shown
to audibly affect the fixture, and center/zero return and state matched.
These are translated work counts only.

The new direct original-to-current captures at 48 kHz Standard, not an
accumulation of relative changes, recorded:

| Buffer | Fresh quiet guest instructions | Eight varied held guest instructions |
| --- | ---: | ---: |
| 16 samples | −41.699% | −15.513% |
| 32 samples | −42.178% | −15.182% |
| 64 samples | −42.441% | −15.012% |

Eight equal held at 64 improved 15.395%, and varied wake 11.713%.
Guest reads and writes fell in all eight measured cells.
The [64-frame](original-64.json) and [16/32-frame](original-16-32.json)
receipts compare the original db02cd8 to the new scoped whole VST3 variant
and preserve every calibrated count. Original/current native public
audio/state/FP and 67-sample reported latency matched in the tested histories.
This is not a DAW or low-end physical CPU percentage.

Two [Legacy moving-pedal controls](legacy-work.json), where master smoothing
does not run, were exact and essentially unchanged: 4,050 fewer guest
instructions (about 0.032%), eight additional reads and eight writes per
four callbacks. Both additivity and duplicate controls passed.
