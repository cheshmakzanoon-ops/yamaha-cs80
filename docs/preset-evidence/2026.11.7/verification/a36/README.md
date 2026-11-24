# A36 — evidence for the hammered steel Keys review

See [the review](../../reviews/a36.md) for the conclusion. This evidence investigates an inherited mechanism-specificity concern; it does not contain an accepted sound correction.

## Retained records

- `recipes.json`: fresh inventory descriptions, exact zero-based addresses and all parameter values for the two programs; 523 programs in the source inventory.
- `provenance.json`: frozen source commit, observed HEAD, checksums, source equality checks and temporary caller commands.
- `capture-metrics.csv`: all 24 temporary raw PCM captures, condition labels, exact timing, two-versus-zero parameter changes, peaks, validity and raw hashes.
- `window-metrics.csv`: unnormalized RMS and Hann-windowed stereo-power spectrum metrics for each time window.
- `touch-metrics.csv`: firm-minus-soft onset RMS and spectral contrasts, separately for the real recipes and the Piano diagnostic.
- `piano-substitution-diagnostic.csv`: paired differences caused by the two body model substitutions.
- `summary.json`: bounds, scope exclusions and exact official-capture equivalence results.
- `struck-body-dynamics.log`: unchanged existing regression, 3,439 checks / zero failures, process exit 0.

## Reproduction protocol and isolation

All temporary source and PCM are under `/tmp/cs80-a36-1cee5a`. `probe.cpp` constructs a fresh current `Engine` for each program/condition/note/velocity, prepares it at 48 kHz, and supplies the exact current `programLibrary()` vector. Its one diagnostic changes `P_body1Model` and `P_body2Model` from Steel (7) to Piano (1). It asserts exactly zero changed parameters for actual recipes and exactly two for diagnostic copies. It does not alter a saved preset or production source.

The matrix crosses the names `Dulcimer` and `Distant Keys 113`, MIDI notes 48/60/72, velocities 0.25/0.85, and current/Piano-diagnostic conditions. Each capture is 168,000 stereo frames of little-endian float32, released at frame 120,000. The caller uses 48-frame render blocks. Default channel pressure is zero; fresh-engine initialization and both full vectors are identical within each paired comparison. Frozen headers were exported with `git archive c5549f7d095a5c5f49a5ad22ec0ccc819245d201 src`, and linked only to the current core library. No old/new header-library ABI mixture was used.

The compilation and run commands are in `provenance.json`. Temporary `analyze.py` records the exact numerical analysis. It sums FFT power from each stereo channel separately after a Hann window. Upper/lower integrates 6.5–24.5 / 0.7–6.5 times the nominal note frequency, with the upper limit below host Nyquist. RMS is computed from the untapered stereo samples. Windows are onset 0.02–0.20 s, body 0.05–0.30 s, settled 1.8–2.4 s, and release 3.0–3.5 s. Nothing normalizes or clips the output.

The official-capture crosscheck used:

```sh
/tmp/cs80-build-1cee5a/cs80_preset_capture --library all --preset Dulcimer --preset 'Distant Keys 113' --out /tmp/cs80-a36-1cee5a/official --wav --rate 48000 --note 60 --velocity 0.85 --pressure 0 --hold 2.5 --tail 1
```

Both resulting 168,000-frame WAVs equal the custom caller's current PCM sample for sample. The official utility renders in 128-frame blocks, with exact event boundaries. Its invocation and the custom capture returned zero. The unchanged existing test was invoked directly:

```sh
/tmp/cs80-build-1cee5a/cs80_struck_body_dynamics_tests
```

The actual-patch matrix establishes finite, sounding output, touch contrast and spectral evolution at the three selected notes. The body substitution quantifies collateral changes; it is not a proposed correction, a preferred sound, or a claim of greater realism. No native CI status is inferred from this local evidence.

## Primary references consulted

- Jim Woodhouse, [12.2 Hitting strings: the piano and its relatives](https://euphonics.org/11-2-hitting-strings-the-piano-and-its-relatives/): author's santoor simulations and explicit same-position pluck/strike comparisons.
- Donald E. Hall, [The hammer and the string](https://www.speech.kth.se/music/5_lectures/hall/hall.html): finite contact, hammer/string mass, stiffness and velocity.
- Balázs Bank and László Sujbert, [On the nonlinear commuted synthesis of the piano](https://www.dafx.de/papers/DAFX02_Bank_Sujbert_piano_synthesis.pdf), DAFx-02: separation of hammer interaction, string propagation and soundboard.

These sources justify the distinction between spatial weighting and contact behavior. They do not supply a fitted dulcimer parameter set for either program.
