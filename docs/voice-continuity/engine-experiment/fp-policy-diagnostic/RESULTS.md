# Targeted FP-policy result

**Diagnostic complete; no acceptance gate, no first-result replacement, no
real-time qualification.** The source, four original objects and executable
were hashed before the separately coordinated short measurement window.
All33 cells /132 rows /33,792 callbacks completed with asserted FTZ/DAZ policy,
finite output, expected active/effective state, exact continuous clock counts
and zero rejected circuit transactions.

Every median/p99/max/overrun and MXCSR flag aggregate was recomputed from
[retained raw durations/registers](results/callback-times.csv.gz).

| Observation | Pristine baseline | Enabled candidate |
|---|---:|---:|
| ON/OFF paired median ratio range |0.971147–1.007683|0.979001–1.011574|
| Median of those ratios |0.994123|0.996351|
| Overruns, OFF / ON |14 /10|37 /35|
| Maximum RTF, OFF / ON |5.346830 /2.517743|12.555659 /3.404806|
| Callbacks setting denormal-operand flag, either policy |0|0|
| Callbacks setting underflow flag, either policy |0|0|
| Final-output samples compared directly, OFF versus ON |3,211,264|3,211,264|
| Reported differing output bits / subnormal outputs |0 /0|0 /0|
| Differing aggregate state / filter-EG checksums |0 /0|0 /0|

The output comparison was performed directly by the frozen harness outside the
timers. Numerical counters and source are retained, **not every audio sample**;
unlike timing/flag statistics, the output comparisons cannot be recomputed from
the persisted raw-duration CSV alone. Equal aggregate state checksums also do
not prove equality of every hidden variable. This explicitly bounds the claim.

This matrix provides **no supporting evidence that FTZ/DAZ policy explains the
first measurement's outliers**. It does not prove denormals can never occur in
longer/different patches or identify any individual scheduling/CPU cause.
Overruns still occurred with FTZ/DAZ ON, and none was removed. The short targeted
matrix is not a second attempt at the first matrix's median gates and supplies
no new favorable substitute for them.

See [machine-readable summary](results/summary.json),
[all aggregates](results/results.csv),
[numerical observations](results/numerical-differences.csv),
[context/hashes](results/metadata.json), and
[source/object/executable manifest](premeasurement-sha256.txt).
`analyse.py` verifies the132 timing/flag rows from compressed raw observations.
The separate [independent audit](results/independent-audit.json), using
`independent_audit.py` rather than the primary analyzer, also verifies every
raw statistic, rotated order, before/after MXCSR policy/mask/rounding/status
aggregate, all7 source/object/executable hashes, and unchanged first-result
hashes. It retains the same numerical-counter evidence limitations above.
The original [first whole-Engine result](../first-measurement/summary.json) and
its143 overruns remain unchanged.
