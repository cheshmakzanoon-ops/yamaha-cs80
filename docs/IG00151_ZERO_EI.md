# IG00151 zero-EI fast path

## Scope

Baseline: main `36581cabdb69a377ae9b02f05349371e088cdb52`.

When the exponential VCA input is either signed zero, `exp2` is exactly one.
The default SUB-VCA depth is zero, so the 16 rank VCAs otherwise evaluate this
transcendental repeatedly without changing gain. The shortcut returns one for
that argument only. Every nonzero input, including subnormals, infinities and
NaNs, retains the original `std::exp2` call. Multiplication order, LI smoothing,
output coupling, pedestal, and all state/layout remain unchanged.

This is a numerical optimization of the existing inferred VCA law, not a new
circuit model, hardware measurement, or a change in modulation depth.

## Verification

`Ig00151ZeroEiTests.cpp` compares against the frozen baseline in
`Ig00151VcaBaseline.h` (only its namespace changed). It makes 1,200,054 output
and LI-node bit-pattern checks across six internal rates, positive/negative zero,
changing nonzero EI, subnormals, reset/idle transitions and exceptional values.
The standalone test passed with GCC 14.2.0, `-O2`, and UBSan; an independent
review repeated those checks with `_GLIBCXX_ASSERTIONS` and no sanitizer recovery.

```sh
c++ -O2 -std=c++20 -fsanitize=undefined -fno-sanitize-recover=all \
  -D_GLIBCXX_ASSERTIONS -Isrc tests/Ig00151ZeroEiTests.cpp -o /tmp/vca-zero-ei
/tmp/vca-zero-ei
```

The JUCE-free `VcaZeroEiEngineProbe.cpp` is compiled against both baseline and
candidate, using the same compiler/options. Its output files must compare exactly:

```sh
# Run in each source tree, with a different executable/output filename.
c++ -O3 -std=c++20 -Isrc tests/diagnostics/VcaZeroEiEngineProbe.cpp \
  src/core/Engine.cpp src/core/FactoryPresets.cpp -o /tmp/vca-engine-probe
/tmp/vca-engine-probe /tmp/vca-engine-output.bin
# cmp BASELINE_OUTPUT CANDIDATE_OUTPUT
```

On this x86-64 Linux runner, 246,888 finite stereo frames were byte-identical:
`1f9aec779dc0dc7eca10a3b3d0058ef254ecc78dccd3c6d79bba53c1199e0ad4`.
The probe covers all three policies and qualities, fractional/invalid host rates,
reprepare, reset and offline toggles. The existing external-interpolation engine
probe also compared exactly for 145,584 finite stereo frames, including nonzero
SUB-VCA depth: `388cd626f7a1d30eb7a5dc6d8079e161e72f6e50e86046b2a45f75327df08d0d`.
These hashes describe this compiler/run, not portable golden hashes.

No plugin processor, GUI, validator or real DAW was executed in this focused work.
Normal platform CI remains a separate qualification step.

## Bounded timing evidence

`tests/diagnostics/VcaZeroEiBenchmark.cpp` measures 65,536 rendered frames per
round after 8,192 warm-up frames, Circuit, eight held voices, Standard quality,
48 kHz, 256-frame blocks. Compile against each tree with the same `-O3` command
as the probe above. Arguments are policy, voice count and SUB-VCA depth.

On the shared x86-64 Linux/GCC 14.2 runner, processes pinned to CPU 2 ran in
baseline/candidate/candidate/baseline/candidate/baseline/baseline/candidate order,
nine rounds each (36 timings per revision). No compilation ran concurrently.

| SUB-VCA depth | Baseline median seconds | Candidate median seconds | Observed change |
| --- | ---: | ---: | ---: |
| 0 (disabled) | 0.719891 | 0.693791 | -3.63% |
| 0.5 (modulated) | 0.709538 | 0.684273 | -3.56% |

The raw per-round timings are in `artifacts/vca-zero-ei/`. Run-to-run variation
was substantial, and even the nonzero control improved, so these numbers do not
isolate the gain from avoiding `exp2`, guarantee a speedup on another platform,
or establish the 0.15–0.20x realtime or idle CPU targets. The exact zero identity
and unchanged outputs are the stronger evidence. No tolerance was relaxed.
