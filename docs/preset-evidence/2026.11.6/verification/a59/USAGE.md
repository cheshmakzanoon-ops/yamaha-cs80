# A59 — causal response coverage

From the repository root, with its existing current `build-preset/libcs80_core.a`:

```sh
python3 docs/preset-evidence/2026.11.6/verification/a59/reproduce.py
```

The runner verifies the published `.4` inventory hash and all 85 core-file hashes
against `fcc3c8e`, generates the exact 28 pairs of controls, compiles a separate
probe in a temporary directory, and records its executable and library hashes.
It does not rebuild or mutate the shared core, presets, workflow, or native gates.

The input has the same current 204-parameter schema for both versions. Published
200-parameter values receive four zero selectors, exactly matching the actual
current values for these 28 programs. The only differing fields are rank VCA
baselines and, for some programs, master volume. Every render overrides master
volume to the same 0.5; all other original controls are retained. This isolates
the baseline edits on one current engine, rather than comparing two DSP versions.

Each of 28 existing programs receives eight fresh-engine renders: two control
versions, each with velocity/pressure `(0.25,0)`, `(0.85,0)`, `(0.5,0)`, and
`(0.5,0.8)`. Bass uses MIDI note 48; the other families use 60. Each held note is
two seconds at 48 kHz stereo, processed in 128-frame blocks. Pressure is set at
onset; no pedal, live-pressure ramp, note release, or arpeggiator is tested here.

`response.csv` retains full-held, early (0–0.2 s), and late (1.5–2 s) RMS values,
peak samples, and the actual PANEL selectors. `response-audit.json` computes
whole-held firm/soft and high/zero-pressure contrasts, retains the exact input
hashes and all program records, and separately gives a simplified isolated-rank
touch calculation. That analytical calculation excludes keyboard, modulation,
time evolution, and mixing; only the full Engine measurements include them.

A change in `20 log10(RMS_firm/RMS_soft)` cannot be produced by applying the same
post-output scalar to both gestures. The resulting evidence establishes a
changed playing response for this gesture. Its magnitude is a measurement,
not a listening-test threshold or an acoustic-realism score. The old controls
are run through the current engine deliberately, so these are not fresh renders
of the published `.4` binary, native CI results, or proof of every shared DSP
improvement.

No WAVs are published by this probe. Metrics are computed directly from the
actual finite full-scale-bounded samples; scratch executables are discarded.
