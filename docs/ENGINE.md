# Engine and signal path

## Design target

The CS-80 is a dual-channel polyphonic instrument. Each of the eight voices has two independent synthesis layers, often described as two voice cards, with their own oscillator, high-pass filter, low-pass filter, filter contour and amplifier contour. That gives sixteen oscillator/filter/VCA chains. The two layers are mixed through the channel balance and can be panned independently.

This engine follows that structure rather than building one oscillator into a stereo voice:

```text
                         ┌─ Channel I  ─ VCO ─ HPF ─ LPF ─ VCA ─ pan ─┐
8-voice allocator ──────┤                                                ├─ ring ─ chorus ─ FX 1..3 ─ DC ─ output
                         └─ Channel II ─ VCO ─ HPF ─ LPF ─ VCA ─ pan ─┘
                                      │
                                      └─ sine bypass ────────────────┘
```

The core has no JUCE dependency, so it can be rendered offline, tested with plain CTest, and reused by any plugin shell.

## Voice model

`Engine` owns eight `Voice` objects. A voice is active while either rank's amplifier envelope is not idle. Allocation uses the oldest idle voice first, then the oldest voice if all eight are busy. Every voice stores:

- MIDI note, velocity, source channel, pressure and timbre
- pitch target and glide position
- initial-bend transient
- two independent `Rank` objects

Each `Rank` contains an oscillator, HPF, LPF, filter envelope, amplifier envelope, and per-card trim values for tune, cutoff, level, envelope and pan. Trims are assigned deterministically at `prepare()` so a patch is consistent between runs but the voice cards are not identical.

## Oscillators

`Oscillator` produces four outputs from one phase accumulator:

- **Saw**: falling ramp with PolyBLEP correction at the reset.
- **Pulse**: variable-width pulse with PolyBLEP correction on both edges.
- **Reset pulse**: a fixed ~30 microsecond edge, scaled by frequency, added to the saw and pulse paths. This is the characteristic narrow transient of the IG00153 current-source oscillator rather than an idealized mathematical waveform.
- **Sine**: a shaped sine with a small third harmonic, used by the sine path that bypasses the filters.
- **Noise**: uncorrelated per-sample noise injected before the filters.

The footage switch is a six-position octave selector. `kFeetSemitones` is `{-12, 0, +12, +24, +36, +48}` for 16', 8', 4', 2', 1' and 1/2'. Pulse width has a 50-90% panel range so the pulse never collapses to silence.

The engine runs at **2x the host sample rate**. The oscillators, filters, envelopes and effects all run internally at 96 kHz for a 48 kHz host. A 31-tap windowed-sinc decimator removes the out-of-band content before host-rate output. The decimator's symmetric group delay is reported as 8 host samples for plugin delay compensation.

## Filters

Each rank has a serial high-pass and low-pass stage. Both use a topology-preserving state-variable filter with nonlinear input saturation and independent resonance. The high-pass feeds the low-pass, matching the CS-80's HPF -> LPF order. The sine path bypasses both filters and is summed after the VCA, as on the hardware.

Cutoff is modulated by:

```text
panel cutoff
  + brilliance
  + key tracking
  + filter envelope
  + initial velocity brightness
  + aftertouch brightness
  + CC74 timbre
  + mod wheel VCF
  + sub-oscillator VCF
  + LFO VCF
```

The filter contour has initial level (IL), attack level (AL), attack, decay and release. Its release can be longer than the amplifier release, which is what lets a CS-80 brass swell open after the key is released.

## Envelopes

Both contours use a calibrated exponential segment. The coefficient is derived from a **time-to-rest (-60 dB)** rather than a raw RC time constant, so a 200 ms release is finished in roughly 200 ms instead of ringing for seconds. Decay and release include a short fast bend followed by the slower tail; this bend is responsible for the characteristic "brass" movement.

The amplifier contour also receives a velocity gain (`0.38 + 0.62 * velocity`) and a continuous level offset from initial level and aftertouch level. The filter contour is driven separately by initial and aftertouch brilliance.

## Modulation

### LFOs

Three free-running global LFOs each have rate, depth and destination. Destinations are pitch, VCF, VCA, PWM, pan and off. They are summed independently so a slow filter drift and a fast shimmer can coexist.

### Sub-oscillator

The CS-80 sub-oscillator is a global modulation source, not a sub-bass oscillator. It has six waveforms, a rate from 0.05 to 30 Hz, and destinations for VCO pitch, VCF cutoff and VCA level. Pressure can increase its rate and VCO amount. The mod wheel adds a separate sub-oscillator amount.

### Ring modulator

The ring modulator is global and has a dedicated AD envelope retriggered by the first key of a phrase. Speed sets the carrier frequency, depth sets the carrier amplitude, attack/decay shape that amplitude, and amount is the dry/ring balance. The result is suitable for bells, metallic leads and slow sci-fi sweeps.

### Ribbon and wheels

The ribbon is a continuous pitch controller with a ±12/±18 semitone travel and a spring return to centre. It is independent of the pitch wheel. The pitch wheel uses the host-visible `pitch_bend_range`; MPE member channels use `mpe_bend_range`, which defaults to the 48-semitone MPE convention. The modulation wheel is CC1, CC64 is the sustain pedal, and polyphonic pressure is scoped to the member channel when MPE is active.

## Voice dispersion and drift

`voice_dispersion` scales deterministic per-card trims for tune, cutoff, level, pan and envelope. At zero the eight voices are calibrated. At 0.4-0.6 they spread enough to produce the unstable, chorused quality of a large analog polyphonic.

`drift` adds a slow independent sine movement per voice. `analog_noise` adds a low-level noise floor after the voice mix. These are performance controls, not random modulation that changes the patch on every block.

## Chorus and modern FX

The authentic chorus is a stereo BBD-style delay pair with complementary modulation, band-limiting and a small amount of cross-feed. Tremolo uses the same modulation source and is selected by the `CHORUS / TREM` switch.

Three serial FX slots sit after the chorus. The type values are fixed by the UI:

| Value | Effect | Implementation |
| --- | --- | --- |
| 0 | Off | bypass |
| 1 | Chorus | second, cleaner stereo chorus |
| 2 | Phaser | six first-order all-pass stages, triangle LFO |
| 3 | Tape Delay | 340 ms stereo delay, filtered cross-feedback, wow |
| 4 | Plate Reverb | four stereo comb filters, two all-pass stages per side |
| 5 | Drive | soft OTA-style saturation |

Each slot has an independent mix. A slot at 0% is bypassed. The output stage then applies a 10 Hz DC blocker, a smoothed master volume, expression and a soft limiter.

## Arpeggiator

The arpeggiator holds up to 32 keys and builds a sequence in one of five orders:

- Up
- Down
- Up / Down
- Random
- Played order

One to four octaves can be added. The clock runs at the internal sample rate and a new note triggers the normal CS-80 voice envelope. The current note is released at 58% of the clock period (45% in random mode), leaving the release tail to overlap the next attack. Releasing all keys stops the pattern; changing the rate is heard on the next step. Up/down reserves enough sequence space for the complete ascending traversal plus its reverse.

## Macros

Macros are neutral at 0.5. They are applied after the panel values in `buildSettings()`:

- **Brightness**: ±0.75 octave on the global brilliance offset.
- **Motion**: 2^(-1.1 * macro) envelope time scaling and a small LFO depth lift.
- **Width**: detune, voice dispersion and chorus depth.
- **Space**: ring amount and sub-oscillator VCF/VCA depth.

Because macros are offsets, turning one does not destroy the panel settings or require a separate macro state.

## Presets and state

`ParamValues` is a flat array of 122 floats. The table in `src/core/Params.h` defines ID, display name, range, default, skew, unit and group. The APVTS builds one `AudioParameterFloat` per entry, so every control is host-automatable and serialised in the plugin state.

The factory bank contains:

- 22 hardware tone programs: each of the 11 tone-selector positions for channel I and channel II.
- 22 handcrafted performance patches covering brass, strings, pads, bass, keys, leads, FX and arpeggiated motion.
- 178 deterministic performance variations generated from the same circuit-model parameter families.

Factory presets are immutable. User presets are XML `ValueTree` files in the platform application-data directory under `CS80Reference/Presets`.

## UI architecture

The editor is drawn at a 1600x1000 design resolution and scaled to the window. Components are positioned in design coordinates and transformed in `resized()`, which keeps the panel coherent at the 1120x700 minimum and at high-DPI sizes.

- `LookAndFeel` owns the graphite/amber visual language: knurled knob skirts, engraved legends, amber arcs and backlit switches.
- `ParamKnob` combines a rotary slider with a live, unit-aware readout; every instance has an APVTS `SliderAttachment`.
- `LedButton` uses a `ButtonAttachment` and paints the illuminated state.
- The main panel is deliberately not a generic grid: it reproduces the channel/master/tone-selector hierarchy of the instrument.
- `AdvancedPanel` is a separate modern layer so the primary surface stays playable.
- `PresetBrowser` indexes factory and user patches, filters by text and category, previews descriptions, and loads on double-click or LOAD.

## Verification

`tests/CoreTests.cpp` exercises the engine without JUCE. `src/tools/ui_snapshot.cpp --self-test` instantiates the JUCE processor and editor, checks the APVTS, round-trips state, renders MIDI audio, checks the editor at two sizes, opens all advanced pages and the browser, and saves/loads a user preset. CTest runs both on Unix.

The Linux CI job additionally writes `artifacts/cs80-demo.wav` and seven PNG screenshots. The demo is a five-chord performance with pressure and ribbon moves, not a static held note.
