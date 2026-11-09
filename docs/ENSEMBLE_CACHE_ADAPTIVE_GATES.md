# Saved adaptive experiment gates

The following notes were written before the adaptive experiments, but were not
separately committed at that time. Trial length was increased from 524,288 to
4,194,304 after the first short generic result was borderline (+2.50%); the
2% gate and correctness requirements were not changed. The report preserves
both the short observation and all longer repeats.

---

# Adaptive Ensemble prototype gates (before evaluation)

Baseline: unmodified Ensemble on main 0b340d9, frozen tests/EnsembleFxBaseline.h.
As-is PR #77 is held because three current model-only benchmark runs measured
fully sample-varying Rate/Depth overhead +6.79%, +3.50%, +8.82%.

Explore a short fixed-window bypass after a Rate miss: bypass executes the full
original expression on EVERY sample, never approximates/holds Rate or phases.
Also compare caching speed alone, retaining original per-sample increment math.

Correctness acceptance: all original 865 checks; new moving-to-held transitions;
combined three-effect serial-slot and Engine byte identity at generic O2 and
native O3/FMA; sanitizer suite; strict Legacy/V1/V2. No tolerance relaxation.

Performance: evaluate original same opt-in scenarios, seven alternating-order
trials, generic Release and native. Do not accept repeated moving-control
slowdowns larger than 2% as a resolved regression. Require median moving-control
change within 2% in each profile across repeat runs, and retain useful held-Rate
benefit. Inspect stepped and held-Rate/moving-Depth results, not only favorable
held controls. If uncertainty or positive drift persists, report the hold rather
than claiming it cleared. Shared-host timing is not whole-plugin qualification.
