# Variable-rate seeded random LFO oracle

This reviewed design completed its bounded run. See the
[verified results and limits](experiments/lfo-random-variable-rate/README.md).

Test-only standalone ModulationLfo sample-and-hold and smooth-random verification.
No Engine/native-host claim or production changes. Existing fixed-rate tests do
not independently verify variable-rate random sequences from a cumulative phase
integral and a separately implemented integer stream.

24 cases:2 shapes x3 existing Engine seeds x4 internal rates88.2/96/192/384kHz,
each0.25s. Physical float rates follow fixed piecewise-linear knots at normalized
time0/.21/.47/.73/1 with values16.37/19.79/17.13/18.61/16.89Hz. Pin before execution.
Use independent long-double integration of those actualfloat inputs, not queried
production phase. Independent xorshift32 recurrence uses known-answer seed1
vectors and upper24-bit rational mapping, never productionRng.

Match startup consumption:onevalue forS&H,two for smooth;one per subsequent wrap.
Exact held values and reset replay; smooth error uses unchanged existing1e-6
shape tolerance. All samples finite/bounded,atleast4wraps,seed divergence required.
Require reference cycle distance from integer boundaries >1e-10 for every sample;
no edge skips or postfailure tolerance loosening. Save actual/reference sequences
and all four exact input-rate arrays with hashes. No statistical randomness-quality,
Engine smoothing/depth/routing,host automation,hardware or performance claim.

Independent design review recommended this finite scope. Exact source and runner
review before execution, independent actual-file oracle/evidence audit afterward.
All old source/tests/contracts/evidence remain unchanged. No broad suites,GUI,
malformed inputs,benchmark replay or security reproductions.
