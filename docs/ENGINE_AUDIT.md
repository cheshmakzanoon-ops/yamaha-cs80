# Engine sound-quality audit

`cs80_engine_audit` (`src/tools/engine_audit.cpp`) renders the core engine offline and
measures engineering properties of the output: non-finite samples, level, DC and
sub-sonic content, clicks at note boundaries, voice-steal steps, out-of-band and
aliasing energy, output-path latency, and modern-FX behaviour. It exists to find
software defects. **Every number is a property of this software on this machine's
arithmetic. None of it is a measurement of, or a claim of parity with, a Yamaha
CS-80**, and none of the thresholds below is a hardware tolerance. Judging
resemblance to the instrument needs recordings of a real unit (see "What this cannot
establish").

## Running it

```
cmake -S . -B build -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target cs80_engine_audit
build/cs80_engine_audit --out DIR [--suite phrase|click-causality|fx|alias|latency|all]
                        [--rates 44100,48000,96000] [--quick]
```

One CSV per suite is written to `DIR` (`phrase.csv`, `fx.csv`, `alias.csv`,
`latency.csv`); `--quick` limits the factory-bank rows. The tool is deterministic
(fixed engine seeds, fixed event times), so two builds can be compared row by row.
The full default run, all three rates and both policies, takes tens of minutes on a
shared core. It is a manual tool and is not registered with CTest.

## What is rendered

- **Programs**: the factory bank under the legacy policy and under Source
  Performance (`P_sourcePerformance` = 1). The full bank is rendered at the first
  rate; every sixteenth program at the others.
- **Tabs**: the eleven hardware tone-selector positions (both ranks set to the same
  source tone, the plugin's physical tab adapter) under both policies, at every rate.
- **Phrase** (fixed times, independent of the rate, 64-sample blocks so an event lands
  within about 1.5 ms of its time): a single note, a four-note chord, a held-then-
  released chord, a quick retrigger of one key, an eight-voice chord released and
  followed 150 ms later by a four-note chord (with all circuits busy in release this
  reuses four voices that are still sounding: a **voice steal**), then silence.
- **FX**: one to three slots of each modern effect type, at mix 0.5 and 1.0, in
  several combinations, on a plain saw patch.
- **Rates**: 44.1, 48 and 96 kHz hosts.

## Metrics

| Column | Definition |
| --- | --- |
| `nonfinite` | count of NaN/inf samples (replaced by 0 afterwards) |
| `peak_db`, `samples_over_1` | whole-phrase peak in dBFS; number of samples with magnitude above 1 |
| `softclip_frac_chord` | fraction of samples at or above 0.6 in the held chord: about where the legacy `tanh(0.85x)/0.85` stage has lost 1 dB |
| `rms_single_db`, `rms_chord_db` | RMS of the sustained single note and chord |
| `weighted_chord_db` | the chord RMS after a 100 Hz second-order high-pass; a crude loudness proxy, **not** K-weighting or LUFS |
| `loud_short_db` | highest 300 ms weighted RMS in the chord window (works for percussive programs) |
| `dc_single_rel`, `dc_chord_rel` | mean over RMS in the sustained windows |
| `dc_idle_abs`, `idle_rms` | mean and RMS in the last 0.4 s after everything has been released (noise floor) |
| `subsonic_peak`, `subsonic_rel` | peak after a 10 Hz second-order low-pass, absolute and relative to the phrase peak; sees a DC step or thump, an audible note does not excite it |
| `click_off`, `click_chord_off` | largest sample-to-sample step in 8 ms after a release, over the same measure in the 60 ms before it |
| `click_retrigger` | same step after the second of two quick presses, over the largest step in the chord window |
| `click_fresh_chord` | step in 8 ms after a chord on idle voices, over the largest step in the earlier chord window |
| `click_steal` | step in 8 ms after the stealing chord, over the largest step of the eight-voice chord |
| `voices_at_end` | voices still active when the 12 s phrase ends, 2.35 s after the last release; nonzero only for programs whose release is longer than that (every factory program goes idle eventually) |

A click score near or below 1 means the step is no bigger than the reference window.
A larger score flags a candidate discontinuity; it does not establish causation.
Programs with a fast attack score near
1 on a fresh chord by construction (the attack is the reference), so compare
`click_steal` with `click_fresh_chord`, not with a threshold.

**Click-causality suite** (`click-causality.csv`). The original phrase score,
8 ms window and reference windows are unchanged. For the Funky 1 + Funky 2
tab it renders a second copy with only the first note-off omitted and requires
sample-identical output before that event. This exposes ordinary waveform
edges that are already present without a release. For Bass Stack it reports
the first strike at 6.0 s as well as the repeated strike at 6.25 s against the
same chord reference. Neither row overrides the registered target of 1.
The exact capacitor-continuity regression is `cs80_note_off_continuity_tests`;
its event-time delta limit is zero. See the dated
[investigation](evidence/continuation-2026-10-31/DSP_INVESTIGATION.md).

**Alias suite.** One sustained saw or pulse (50 %) note of rank I with detune, drift,
dispersion and noise off, the high-pass off and the low-pass open, 65 536 samples
under a Blackman-Harris window. Bins within six bins of `k * f0` count as
harmonics; everything else from 20 Hz to 20 kHz counts as non-harmonic (the ring
modulator rows treat `k * f0 +/- 150 Hz` as legitimate). `nonharmonic_db_vs_partials` is that
power over the harmonic power; `worst_spur_dbc` is the largest single non-harmonic
bin against the strongest harmonic. The engine renders at twice the host rate and
decimates, so energy that folds at the internal Nyquist is attenuated by the
decimator and what remains is measured at the host rate. This is an estimate: the
band edge and the window leak a little into "non-harmonic" at high levels of the
legitimate partials.

For the ring-modulator rows the carrier is fixed at 150 Hz (depth 0) and the lines
`k * f0 +/- 150 Hz`, including the carrier feed-through at `k = 0` that a DC offset in
the saw leaves, are legitimate. The ring rows therefore test only that the modulator
adds no aliasing of its own at a low carrier; a high-carrier test is not included.

**Latency suite** (`latency.csv`: `rate,freq_hz,delay_host_samples,reported_latency,gain_db`).
The group delay and gain of the 2x-to-1x decimator at several tones, measured the way
`Engine::render` uses it, against the reported 63 host samples.

**FX suite** (`fx.csv`: the phrase columns plus `fx_slots`, `fx_mix`, `tail_ratio_db`,
`gain_vs_dry_db`). `fx_slots` is the three slot types as digits (`123` is chorus, phaser,
tape). `tail_ratio_db` is the level in the last second over the level 0.5 s after the
chord release: it is about 0 for effects without a tail and strongly negative for a tail
that decays. `gain_vs_dry_db` is the chord RMS against the same patch with no effect.

## Results of the 2026-10-30 run

Commit state: the `engine-qa` branch. Phrase suite: 566 rows (222 factory programs under
each policy at 48 kHz; 14 programs under each policy at 44.1 and 96 kHz; 11 tabs under each
policy at the three rates). The FX suite ran 102 rows, the alias suite 168 rows. The raw
CSVs are not committed; rerun the tool to reproduce them.

Ranked by how audible the defect is, with what was done about it.

### Fixed

| Finding | How measured | Severity | Change |
| --- | --- | --- | --- |
| Two FX slots of the same type ran as one effect: they shared and double-advanced one set of delay lines, so two tape slots echoed at about 170 ms instead of 340 and 680 ms | `testFxSlotsAreIndependent`; FX suite rows `222`, `333`, `444` (48 kHz legacy chord RMS for two phasers -25.6 dB before, -31.6 dB after; three plates' tail ratio -24.9 dB before, -14.6 dB after) | High for a session using it, none otherwise (the factory bank has no active FX slot) | Each slot owns its own effect state (`Engine::FxSlot`) |
| A slot switched off or changed to another type kept the old effect's audio and replayed it, late, when switched on again | same test: with the old behaviour a tape echo returns at 0.44 s after the slot reopens | Medium: a stray echo or reverb burst when an FX knob is toggled | State of the old effect is cleared when the slot goes quiet or changes type |
| Plate reverb times grew with the sample rate: first comb echo 59 ms at 44.1 kHz, 65 ms at 48 kHz, 129 ms at 96 kHz; the tail decayed 7 dB more slowly at 96 kHz | `testReverbDelayTimesAreRateIndependent`; FX suite `400`: tail ratio at 96 kHz -8.8 dB before and -15.6 dB after (48 kHz -16.0 dB, unchanged) | Medium: the same preset sounded different at 96 kHz | Delay times are seconds with the factor a 48 kHz host always had, so 48 kHz is bit-identical |
| In the two source policies a stolen release tail's amplitude envelope was zeroed in one sample: a tick under a passage | Largest sample-to-sample difference against a twin without the steal, eight factory programs: 0.0006 to 0.022 full scale before, 0.0006 to 0.0074 after; worst phrase `click_steal` score 2.21 (Dark Pads 58) before, 0.17 after | Low to medium | VCA envelope of a reused tail restarts from its current level (source policies only); see [ACTIVE_TAIL_CONTINUITY](ACTIVE_TAIL_CONTINUITY.md) |

The legacy-policy phrase rows of the re-run (11 tabs and 12 programs at 48 kHz) and 14 of the 17
legacy 48 kHz FX configurations (every one except the three same-type multi-slot ones) are bit-identical
before and after the changes. Where a change is audible (same-type slots, toggled slots, 44.1 and
96 kHz reverb, source-policy steals) it is intentional and documented in
[ENGINE.md](ENGINE.md) and [ACTIVE_TAIL_CONTINUITY.md](ACTIVE_TAIL_CONTINUITY.md).

### Measured and not fixed

| Finding | Numbers | Why not fixed |
| --- | --- | --- |
| **Aliasing on high notes.** Non-harmonic energy from a sustained saw or pulse | Saw, 48 kHz host: non-harmonic energy -41.7 to -33.8 dB below the partials, worst single line -53 to -35 dBc (the worst is a 4' note at 8.4 kHz). Pulse: -49 to -39 dB, worst line -58 to -41 dBc. 44.1 kHz is 1 to 1.5 dB worse, 96 kHz 5 to 11 dB better. Legacy and source policy agree within 0.2 dB. A ring-modulator carrier at 150 Hz adds nothing | The oscillator alone (PolyBLEP saw plus reset pulse) is -50 to -60 dB at 48 kHz; the 16 to 18 dB excess at full level comes from the filter's provisional `softClip(1.1x)` pre-saturation running at only 2x oversampling, and shrinks to about 4 dB at 0.3 amplitude. Removing it needs a higher oversampling ratio or an anti-aliased saturator in `Svf`: a change to the sound of every program, not bit-identical, outside a small fix |
| **Source-policy hardware tabs exceed full scale** and are louder than the legacy tabs (**resolved in Phase 1.5**, see below) | 18 of 33 source tab rows have samples above 1: peak up to +3.9 dBFS (688 samples above 1 in one phrase at 96 kHz); chord loudness -29.3 to -9.8 dB against -33.8 to -18.1 dB for the legacy tabs (Funky 3 + 4 is 17 dB louder). Source factory-program rows never exceed 1 | Documented behaviour: the source policies drop the output soft clip and keep values above 1 ([SOURCE_FACTORY_RUNTIME](SOURCE_FACTORY_RUNTIME.md), [PARITY_ISOLATION](PARITY_ISOLATION.md)); adding a limiter or level trim would assert a headroom the sources do not give. Float hosts pass the excess; a 16-bit export clips it |
| **Default ANALOG DRIFT and dispersion are large** | Drift 0.25 is +/-22.5 cents per voice at 0.031 Hz with spread phases (`Engine.cpp`), written into every factory program; dispersion 0.35 adds about 2.5 cents and a pan scatter | A product default, already listed as an open decision in [PARITY_ROADMAP](PARITY_ROADMAP.md); changing it changes every program |
| **Three of five voice trims are generated and unused** (cutoff, level, envelope) | n/a | Wiring them would change the default sound of every program (dispersion is 0.35 by default), and there is no source for the distributions; the roadmap asks for source-informed ranges first. Reported, not wired |
| **Loudness differs across the bank** | Factory programs (legacy, 48 kHz): 300 ms weighted level -32.4 / -23.5 / -20.2 dB at the 10th, 50th, 90th percentile, extremes -38.0 and -18.8; sustained families (Strings, Bass, Leads, Keys, Pads) sit within 3 dB of each other, Brass 2 to 4 dB lower, FX and Sequences 8 to 10 dB lower, plucked hardware studies lowest. Peak -24 to -2.7 dBFS, no sample above 1, the output soft clip never reached 0.6 amplitude | Part of the spread is the measure (plucks are short); the bank tables are not to change. A per-program output trim would be a data change |
| **Legacy voice steal and retrigger reset the envelope, phase and filters** | `click_steal` up to 1.01, `click_retrigger` up to 1.35 (a harpsichord, whose own attack is that step) | Legacy compatibility is the documented contract ([ACTIVE_TAIL_CONTINUITY](ACTIVE_TAIL_CONTINUITY.md)); legacy rows are unchanged |
| **Drive FX level** | Single slot at mix 1: +6.6 dB; three drives in series: +18 dB and a +0.7 dBFS peak (legacy); chorus -1.6 dB, phaser -3.5 dB, tape -3.2 dB, plate -7.7 dB | Modern, optional FX; a level choice, not a defect. Worth a gain compensation later |
| **Sub-sonic content** | 10 Hz low-passed peak is 1.6% of the phrase peak at the median, up to 8.8% (FX programs with slow modulation); DC after release at most 4e-6; noise floor about -81 dBFS | Legitimate slow modulation, blocked by the 10 Hz blocker or the coupling network; not audible on typical systems |

### Phase 1.5 gain staging (2026-10-31)

The source policies now scale the post-volume LEFT/RIGHT source by a fixed
`kCircuitOutputReference` (-6 dB) and pass it through `circuitOutputDriver`, an
inferred smooth driver-headroom ceiling at full scale (unity below -1 dBFS), before
the loaded output network (`src/core/OutputTapCircuit.h`). Both are declared software
choices, not measured hardware levels. Re-running `cs80_engine_audit --suite phrase
--quick` at 48 and 96 kHz: source tab peaks -2.9 / -3.0 dBFS (were up to +3.2 dBFS),
no samples above 1, loudest tab chord -19.6 / -19.3 dB against -18.6 / -18.8 dB for
the legacy tabs. A dense eight-note velocity-127 PANEL chord peaks at 0.996 in the UI
self-test, held by the driver. Legacy audio is unchanged.

### Phase 1.1 IG00156 circuit filter (2026-10-31)

The source policies now run each filter stage with the transconductor saturation
inside the loop instead of `softClip(1.1x)` on the input (`Svf::processCircuit`,
[IG00156_REALTIME_MODEL.md](IG00156_REALTIME_MODEL.md)). Alias suite, 48 kHz, source
policy: worst saw non-harmonic energy -33.8 -> -51.1 dB, pulse -39.3 -> -53.5 dB; this
is now near the PolyBLEP oscillator's own floor, so the -70 dB target needs the Phase
1.2 oscillator. Legacy rows are unchanged. Phrase suite: several string/brass tabs are
up to 5-6 dB louder because the 22.6 Hz feedforward contribution raises very low LP
cutoffs; the worst tab peak is -0.8 dBFS. CPU (scratch benchmark, 8 voices, 48 kHz):
circuit engine 0.23 -> 0.37 RTF.

### Checked and fine

- No NaN or infinity in any row of any suite; no hung voice (every factory program goes idle; the rows
  with `voices_at_end` above zero have releases longer than 2.35 s).
- Latency is 62.99 to 63.01 host samples at 44.1, 48 and 96 kHz from 250 Hz to 20 kHz against the
  reported 63; passband gain within 0.004 dB.
- Level does not depend on the sample rate: a fresh chord at the same RNG state agrees within 0.3 dB
  at 44.1, 48 and 96 kHz. The audit rows show up to +/-3 dB between rates because the random start
  phases of octave-related partials differ when the engine has consumed a different number of noise
  draws; running the same chord after 0.7 s or 1.3 s of idle moves it by up to 2.5 dB at one rate.
- Note-off steps are at most 1.18 times the waveform's own step, chord releases 1.45; programs with
  a release of a few milliseconds are the high ones.
- FX: every configuration is finite and stable, including three tape or three plate slots in series.
  The phaser coefficient sign was fixed before this audit; at the engine's rate 0.32 Hz and depth 0.72
  the sweep is about 160 Hz to 2.9 kHz. Denormals: the plugin renders under `juce::ScopedNoDenormals`;
  the offline reverb tail keeps decaying at about 15 dB per second with no floor when analog noise is
  zero, so a host without flush-to-zero could reach denormals after roughly 50 s of exact silence (CPU
  cost not measured).

## What this cannot establish

None of this says anything about how close the engine is to a CS-80. There are no recordings of a
real unit: the loudness spread, the alias floor, the click behaviour, the envelope retrigger and the
tail behaviours are properties of this code, and "defect" here means an engineering fault (a state bug,
a rate dependence, a step), not a deviation from the hardware. A real instrument's own aliasing-free
oscillators, voice-to-voice tolerances, envelope capacitor behaviour at retrigger, output headroom and
noise floor can only be compared against calibrated captures at identified settings. The loudness
figure is a 100 Hz-high-passed RMS, not a perceptual loudness, and the alias figure is an estimate with
a window and band edges. The suite renders the factory bank and a fixed phrase; it does not cover
every control combination (for instance ring-modulator carriers above 150 Hz, the arpeggiator, the
sub-oscillator, external modulation or live parameter automation).
