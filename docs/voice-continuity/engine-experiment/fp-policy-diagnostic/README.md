# Targeted FTZ/DAZ policy diagnostic (separate from first result)

**No acceptance gate and no replacement/retry of the first measurement.**
The first result, including all143 overruns and its default-FP limitation,
remains unchanged. Root separately authorized this diagnostic to align the
render FP controls with the plugin's `ScopedNoDenormals` behavior.

The executable links the **exact four frozen namespace-separated Engine/bridge
objects** from the first experiment. It changes no core source or fixture law.
Each cell has four independently prepared instances: baseline/candidate times
FTZ+DAZ OFF/ON. FP policy applies during opaque setup (which includes retirement
renders), warmup and callbacks to avoid off-policy prehistory. This deliberately
also covers setup, whereas the plugin explicitly guarantees its scope only in
`processBlock`; it is not an actual AudioProcessor lifecycle qualification.

Predeclared matrix:48/96 kHz ×128/256 frames ×both-source/PANEL ×cold0,
retired0,held1,held8, static controls =32 cells. Add the first run's worst-RTF
fixture (96k/128, mixed source/PANEL, moving controls, held1) =**33 cells**.
Four arms ×256 measured callbacks =**33,792 raw observations /132 rows**,
after32 warmup callbacks per arm. Arm order rotates `(cell+callback+order)%4`.
No favorable subset is substituted for the original matrix or budget gate.

FP setup/restoration, MXCSR reads, output comparisons and observation recording
are outside the timer; the same opaque callback is timed. Sticky exception
status bits are cleared before each callback for observation, while rounding
mode and exception masks are retained. Both FP policy bits are asserted before
and after every callback. MXCSR status flags indicate that an event occurred,
not where, how often, or whether it caused a duration outlier.

Numerical observations compare final output float bits (including signed zero),
double-precision absolute/RMS differences, and bit-classified subnormal samples;
existing aggregate hidden-state/filter-EG checksums are also retained. The
comparison/inspection itself uses FTZ/DAZ OFF. These are not complete internal
denormal-operation counts or a proof that equal checksums mean identical state.

All-matrix **untimed** verification runs four callbacks per row and makes no
clock reads. Its first attempt caught a harness assertion-mask typo (FTZ was
mistakenly included with unchanged rounding/mask bits); that assertion was
corrected before any timing. The full untimed matrix then passed. This was not
a performance rerun.

```sh
g++ -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Werror \
  -Idocs/voice-continuity/engine-experiment \
  docs/voice-continuity/engine-experiment/fp-policy-diagnostic/probe.cpp \
  /tmp/cs80-engine-continuity-replay-first/benchmark-build/*.o \
  -o /tmp/engine-fp-policy-diagnostic
/tmp/engine-fp-policy-diagnostic --verify NEW-UNTIMED-DIRECTORY
# Only during the separately agreed exclusive window:
/tmp/engine-fp-policy-diagnostic NEW-MEASUREMENT-DIRECTORY
```

Timing is pending at this source checkpoint. Source/object/executable hashes
must be recorded before the run, all raw observations retained afterward, and
no individual first-run outlier attributed to denormals without matching causal
evidence. This targeted diagnostic does not itself qualify native DAW deadlines.
