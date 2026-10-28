# Reproducible software DSP measurements

This harness measures **software numerical behavior**, not the similarity of a physical Yamaha CS-80. Its reference is an ideal mathematical bandlimited waveform. No hardware audio or electrical capture is present. The plots must not be presented as a fidelity percentage or proof of authentic sound.

## Run

From the repository root, build the dependency-free probe with a C++20 compiler:

```sh
c++ -std=c++20 -O2 -Isrc src/tools/dsp_probe.cpp \
  src/core/Engine.cpp src/core/FactoryPresets.cpp -o /tmp/cs80-dsp-probe
/tmp/cs80-dsp-probe artifacts/dsp-probe
python3 scripts/analyze_dsp.py artifacts/dsp-probe
```

Python requires NumPy and Matplotlib. The probe itself has no Python or JUCE dependency. CMake builds the `cs80_dsp_probe` target and CTest runs its numerical checks on all three desktop platforms. Do not run two probes into the same output directory concurrently.

The probe exits nonzero on failed numerical assertions. The analysis refuses a failed probe and exits nonzero if the corrected ramp fails its conservative comparative alias-regression gate. Outputs are deterministic CSV traces, `probe.json`, `measurements.json`, and five PNG/SVG pairs in `plots/`:

1. `oscillator-waveform-spectrum`: corrected and historical wrong-sign ramp against an ideal bandlimited reference.
2. `oscillator-alias-comparison`: off-harmonic energy at four coherent pitches.
3. `filter-envelope-rate-comparison`: IL, AL, held cutoff and note-off release at four rates.
4. `ring-frequency-envelope`: actual engine carrier/envelope telemetry through legato and new-phrase events.
5. `decimator-response-latency`: captured FIR response, actual quadrature-tone gain, retained-phase impulse and design comparisons.

`measurements.json` records analysis-time source-file SHA256 hashes, Git HEAD, and dirty working-tree status. These identify the available source bytes, but do **not** by themselves attest which bytes produced an already-built executable. Use a clean exact-commit CI checkout and archive build logs with CSVs, JSON and figures for authoritative provenance. In a shared working tree, rebuild and rerun after edits stop; do not reuse old graphs after changing the DSP.

## Oscillator method

The probe runs the core oscillator **directly at 48 kHz**, without the engine's 2× oversampling, output decimator, filters, effects or gain stages. This isolates the edge-correction algorithm; it is not the spectrum of the complete instrument output.

Each capture contains 65,536 samples at FFT bins 151, 601, 3001 and 6001, corresponding to approximately 110.596, 440.186, 2197.998 and 4395.264 Hz. The phase increments have exact binary representations, making the captures coherent and avoiding window leakage. A rectangular FFT is appropriate for these deliberately periodic records. Frequencies are near musical test pitches, not exact A440 or notes from a keyboard tuning test.

The actual `Oscillator::tick()` output includes a separately modeled reset pulse. The test subtracts its exposed contribution to isolate the falling ramp. `saw_full`, pulse and narrow-reset signals remain in the CSV for separate future analysis; **the reported ramp result does not validate the 30 microsecond reset assumption or its amplitude**.

The historical comparison reconstructs the former wrong-sign equation from the same actual oscillator phase. It is not an old plugin binary or a complete historical preset render.

### Reference generation

The Python reference builds an analytic periodic falling-saw Fourier series on a 16×-rate grid. Harmonics at or above the host Nyquist frequency are removed before decimation. The output is shifted one host sample to match the oscillator's phase-advance-before-output convention. This is an ideal low-pass, bandlimited reference; it has neither PolyBLEP's frequency response nor an assumed analog CS-80 response.

### Distinct error metrics

- **Off-harmonic energy**: inverse FFT of bins outside DC and the intended below-Nyquist harmonic locations, expressed in dB relative to reference RMS. It exposes folded components at incorrect frequencies. Aliases coinciding with intended harmonic bins are not counted, so this metric is not total alias energy in every circumstance.
- **Total reference residual**: RMS of the time-domain difference from the ideal reference. This includes in-band harmonic droop/phase error as well as aliases. Do not relabel it “alias level.”
- **Peak amplitude**: a useful boundary check, not a loudness or authenticity measure.

The comparative gate requires at least 6 dB lower off-harmonic energy than the historical wrong-sign formula at these four pitches. It is deliberately a regression check with substantial margin, **not** a perceptual acceptance threshold or a claim about every frequency/pulse width.

First working-tree measurement on 2026-10-28:

|Frequency (Hz)|Corrected off-harmonic energy (dB/reference RMS)|Historical wrong sign|Improvement|
|---:|---:|---:|---:|
|110.596|−41.68|−19.91|21.77 dB|
|440.186|−35.57|−13.87|21.71 dB|
|2197.998|−27.34|−6.56|20.79 dB|
|4395.264|−25.59|−3.44|22.15 dB|

These numbers establish that correcting the sign materially improves this isolated algorithm. The remaining residual is substantial at high pitches, so the result is a starting point for antialiasing/oversampling improvements, not a declaration of production-quality high-frequency synthesis.

## Filter-envelope method

The same `FilterEnv` implementation is run at 44.1, 48, 96 and 192 kHz. The stimulus uses IL=−1.5 model octaves, AL=+2.25, attack=0.12 s, decay=0.35 s, release=0.4 s and held offset=0. Note-off occurs at 1.2 s, after decay has settled. Traces cover 3 s and include both the current bent and unbent forms; the main chart overlays the bent contours at four rates.

Assertions verify finite output, peak arrival at AL, zero held offset, final return to IL and an idle terminal state. The 0.001 tolerance matches/exceeds the implementation's finite segment-end thresholds; it is not a Yamaha voltage tolerance. Similar overlaid curves establish sample-rate consistency under this stimulus. They do not validate the envelope time law, current three-octave scaling or analog bend constants.

The source-backed functional ordering comes from [Yamaha owner manual, printed pp33–35](https://www.therogoffs.com/cs80/CS80E_2.pdf). See [circuit specification](CIRCUIT_MODEL_SPEC.md) for the separate problem of control voltage, filter gain and actual component calibration.

## Ring method

An actual `Engine` instance renders 48-sample blocks at 48 kHz, and the probe reads `getRingFrequency()` and `getRingEnvelope()` on that same rendering thread. Base speeds are approximately 6.667, 40 and 200 Hz. Depth is 1; attack and decay are 0.08 and 0.45 s. A second key at 0.3 s is legato; all keys are released before a new phrase starts at 1.4 s.

Assertions compare telemetry against the current declared `ringModulationFrequency()` function. This tests wiring consistency, not the physical transfer law. The displayed one-octave frequency excursion is **explicitly provisional**. Frequency-envelope routing is documented by [Yamaha's engineering explanation](https://europe.yamaha.com/en/musical-instruments/keyboards/explore/montage-portal/essentials/cs.html); the [original adjustment procedure, printed p19](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF) provides the 6.667 and 200 Hz base calibration anchors. The intermediate 40 Hz point is a software test stimulus, not a service calibration point.

## Decimator response and latency

The probe captures 1,024 internal-rate samples from the **actual C++ Decimator impulse response**, and records its actual sine/cosine quadrature gain after 4,096 warm-up samples. Two quadratures prevent a phase-null false pass at exactly host Nyquist. The C++ gate requires less than 0.1 dB passband error at the selected test frequencies through 20 kHz at 44.1 kHz host, and at least 80 dB rejection for selected tones at/above internal normalized frequency 0.25. These are explicit engineering acceptance targets, not Yamaha component specifications.

The Python stage FFTs the captured impulse on a 262,144-point grid, checks the whole sampled passband/stopband, and checks DC normalization, symmetry and agreement of measured phase-slope latency with the engine's declared latency. The sampled grid is dense numerical evidence, not a formal continuous-frequency extremum proof. Gain is measured against the internal signal, not an already aliased host-rate source.

### Phase convention matters

Let the internal input be indexed from zero, and let the host output be the filtered sample at index 2m+p, where p=1 is the **second/odd** phase retained by Engine::render. For an odd-length symmetric FIR of length N, its internal group delay is D=(N-1)/2. Its complex host output for input exp(j*w*n) is:

    H(w) * exp(j*w*p) * exp(j*2*w*m)

Thus the indexed-source-to-host phase delay is **(D-p)/2 host samples**, not simply D/2 rounded to an integer. The old 31-tap kernel has FIR-only delay 7.5 host intervals but odd-phase indexed delay 7; declaring 8 did not match that convention. A 33-tap filter retaining the even phase, or a 35-tap filter retaining the odd phase, could fix the integer delay but would not fix poor alias rejection. A 255-tap filter retaining the odd phase has exact indexed delay **63 host samples**: 1.429 ms at 44.1 kHz or 1.3125 ms at 48 kHz.

This is the latency of the linear decimation stage with the stated input index origin. It is not a MIDI-to-audio event-latency measurement, and does not include envelope onset or any additional effects-path delay.

### Why 255 taps rather than a cosmetic phase patch?

Initial double-precision DFT screening of float32-rounded coefficients (32,769 frequencies) gave the following design results. These numbers are mathematical screening results; the authoritative **compiled implementation** result is the captured CSV/JSON from an exact-commit CI build.

|Design|Passband target at 44.1 kHz host|Worst stopband from internal 0.25|Odd-phase host delay|
|---|---:|---:|---:|
|31-tap Hamming, cutoff 0.22|Already -3.31 dB at 20 kHz / 48 kHz host|-19.77 dB|7|
|127-tap Kaiser beta 8.6|18 kHz, <0.001 dB|-86.61 dB|31|
|191-tap Kaiser beta 8.6|18 kHz, <0.001 dB|-91.60 dB|47|
|255-tap Kaiser beta 8.6|18 kHz, <0.001 dB|-93.59 dB|63|
|255-tap Kaiser beta 8.6, revised cutoff|**20 kHz, <0.001 dB**|**-87.07 dB**|**63**|

The 18 kHz designs use cutoff (18000/88200 + 0.25)/2. The 127-tap option attenuates 20 kHz at 44.1 kHz by approximately 5.76 dB; it was rejected in favor of preserving the full 20 kHz passband. The selected 255-tap design uses cutoff (20000/88200 + 0.25)/2 = 0.23837868480725624 cycles/internal sample. At that tighter transition, 191 and 223 taps rejected only about 46.78 and 66.88 dB, respectively. Blackman alternatives at the wider 18 kHz transition gave about 75.29, 79.10 and 83.01 dB for 127, 191 and 255 taps.

The selected design uses a portable I0 power series to form the Kaiser window, double-precision design/normalization, and float32 coefficient storage. Runtime symmetric-pair evaluation only on emitted odd samples avoids computing discarded outputs. With 127 pairs plus the center coefficient per channel, the target cost is 256 coefficient multiplies per stereo host frame (12.288 million/s at 48 kHz); this is an operation count, **not a measured CPU benchmark**. History must still advance on every internal input.

The graph compares the actual capture with a reconstructed legacy design and the lower-latency 127-tap alternative. Those comparison curves are not recordings of historical plugin binaries. This antialiasing improvement cannot remove aliases already folded into baseband inside oscillators/nonlinear filters; complete-signal-path measurement remains necessary.

## Next measurements

- Compare the complete 2× oscillator/decimator path against a high-rate reference, including arbitrary noncoherent pitch, PWM and the narrow-reset contribution.
- Benchmark the optimized decimator's actual CPU cost and measure complete oscillator/filter/decimator residuals.
- Add small-signal SVF amplitude/phase sweeps and level-dependent nonlinear tests once circuit-law corrections are stable.
- Obtain calibrated physical-unit recordings and test-point captures with exact settings and provenance before any hardware-match claim.
