# Exact stationary controls

The original recurrence is omitted only at a proved exact Float32 fixed point.
Signed zeros, nonfinite fallbacks and the first subsequent control movement
retain the frozen software behavior. This is an exact CPU optimization, not
a new hardware claim or a fitted smoothing law.

The acceptance contract was published at
`706c673c54772eb45cb9fe41bd7dbf342792c64a` before this evaluation. The
frozen comparator passes 373 checks; seven actual-source mutants are rejected
by their intended assertions and the restored control passes. Source and
driver hashes are recorded in qualification.json. The three affected CTest
gates pass and all four strict compatibility captures remain byte-identical,
including V2 at `6300163`.

The immediately preceding three-round factory checkpoint is `f79b6f7`; the
after-change timings are recorded separately after publication. Exactness
alone makes no whole-engine speedup claim.
