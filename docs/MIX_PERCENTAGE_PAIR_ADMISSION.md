# MIX percentage-pair input validation

MIX displays channel I/channel II percentages, for example `68/32`. Previously
the parser used the first number and ignored everything after `/`. Thus
`68/garbage`, `68/`, `68/31`, and even negative or oversized second values could
silently become a valid edit and host automation gesture.

The parser now consumes the complete second decimal number. Both percentages
must be finite and within 0–100, and their sum must equal 100 within 0.000001
percentage points (a decimal-conversion tolerance). This is validation of the
displayed percentage pair, not interpretation of arbitrary ratios: `25/25` is
rejected rather than guessed to mean 50/50.

Canonical formatted values, optional whitespace, decimal-comma normalization,
and fractional pairs such as `68.5/31.5` remain accepted. Existing bare-number
behavior is unchanged, including its normal range clamping. Other parameter
units, MIX formatting, parameter IDs, DSP and state formats do not change.
Malformed pairs leave the output argument untouched; the attached control
therefore emits no value change or host edit gesture. A valid pair remains a
single balanced attached-parameter edit.

Native tests cover endpoints, fractional pairs, bare values, missing/trailing
text, extra slashes, inconsistent totals, out-of-range/negative values,
non-finite tokens and unsupported exponent/unit suffixes. The existing
950-case formatter/parser compatibility witness remains a separate check.
This is parser and callback verification, not native OS/DAW qualification.

## Verification

The unchanged preceding implementation completed the final matrix with
**17 failures out of 76 checks**. The fixed typed-value target completed
**76 checks with zero failures**. The 950-case formatter/parser witness remains
946 accepted cases with digest `ab1393115089aa5f`, unchanged by this fix.
Four native JUCE CTests passed together: typed values, context-menu lifecycle,
switch context, and undo replay (4/4, 2.67 seconds).

The retained official JUCE 8.0.11/Linux Release setup is unchanged. Nonfatal
Fontconfig cache warnings do not affect these callback results and do not
constitute rendered-font, screenshot, cross-platform or DAW qualification.
