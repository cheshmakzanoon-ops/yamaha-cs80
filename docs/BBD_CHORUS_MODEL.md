# OE2 BBD chorus (Source Performance and Isolation)

Implemented 2026-10-30 in `src/core/BbdChorus.h`. This is the first production audio use of the OE2 evidence ([OE2 circuit evidence](OE2_CIRCUIT_EVIDENCE.md), [filter design](OE2_FILTER_REFERENCE_DESIGN.md), [MN3001 reference](MN3001_CLOCK_EVENT_DESIGN.md), [BA617 review](BA617_SOURCE_REVIEW.md)). It is an engineering reconstruction from public sources, not a calibrated model and not a hardware match.

## Where it applies

| Effective policy | CHORUS mode | TREMOLO mode |
|---|---|---|
| Legacy | Generic modulated delay (unchanged, bit-identical) | Generic amplitude law (unchanged) |
| Source Performance, Isolation | OE2 BBD delay path (this document) | Generic amplitude law (unchanged, same bits as legacy) |

No host parameter was added. The choice follows the existing render policy, so old presets and the legacy sound are unaffected. Changing the effective policy already resets all effect history.

## Signal path

Taken from the printed OE2 graph:

```text
mono sum of the stereo bus
  -> IC3 Sallen-Key low-pass (10 k, 10 k, 1.8 nF, 680 pF: 14.39 kHz, Q 0.8135)
  -> MN3001: 256 clock periods of delay (512 stages, two clock phases)
  -> 1 uF into 100 k output coupling (1.59 Hz high-pass)
  -> IC5/IC6 Sallen-Key low-pass (same values)
  -> 0.1 uF into 10 k interstage coupling (159.15 Hz high-pass)
  -> summed with the unchanged stereo dry signal
```

- The two MN3001 chains share input and clock on OE57, so one chain stands for both.
- The delay is not `256 / current clock`. Each input sample is stamped with the clock count when it enters, and the output reads the packet that entered 256 periods ago. A moving clock is therefore integrated, as in the real device.
- The delay chain, filters and oscillator keep running while the effect is off or in tremolo mode, as the OE2 clock does. Switching the effect on gives wet signal immediately.
- Output with the effect off is the input, bit for bit.

## Inferred constants

Each of these is a labelled engineering choice; none is a recovered CS-80 law.

| Constant | Value | Basis and what is unknown |
|---|---|---|
| Nominal clock | 106.9045 kHz | Geometric centre of the 7-12.5 us TP1 period interval in the Yamaha adjustment procedure. The installed VR1 setting is unknown. Nominal delay 2.395 ms. |
| Clock excursion | +/-0.5 octave at full DEPTH | Spans about the same 1.7-3.4 ms delay interval as the adjustment range. The BA617 control law and the LFO summing resistors are not recovered. |
| Wet level | 0.5 of the dry level | OE1 mixing gains are not recovered. The IC5/IC6 gain difference (3.9 versus 10) is treated as trimmed out, consistent with the near-equal 0.65/0.58/0.58/0.65 Vpp O1-O4 amplitudes in the adjustment procedure. |
| Wet polarity | Inverted | IC5/IC6 are printed inverting stages. The IG00151 and OE1 summing polarities are not printed, so the net sign is unresolved; the opposite sign moves the comb notches. |
| Oscillator | One sine at the SPEED control | OE1 DO1/DO2 run at 56 ms and 0.55 s at maximum SPEED (+/-5%). Which oscillator drives the clock, their waveform and any mixing are unresolved, so only one is used. |

## Not modelled

Charge-transfer loss, clock feedthrough, noise, MN3001 bias and input aperture, IG00151 VCA gain laws and trims, OE1 switching, and any stereo decorrelation. With tied inputs and a shared clock both chains carry the same signal, so the wet signal is identical in the left and right channels; stereo width in chorus mode comes only from the dry signal. TREMOLO keeps the legacy amplitude law because no source evidence recovers it.

## Audible change from legacy

The delay is 1.7-3.4 ms instead of up to 18 ms, so chorus mode sounds like a short comb sweep rather than a wide chorus. The wet path has no content below about 159 Hz and rolls off above the 14.4 kHz filter corner. The legacy low-pass, crossfeed and left/right LFO phase offset are gone from this path.

## Tests

`tests/BbdChorusTests.cpp` (`cs80_bbd_chorus_tests`, 70 checks):

- printed filter corner and Q from the component values;
- wet magnitude and phase against the analytic filter chain and a 256-period delay at 88.2, 96 and 192 kHz, from 5 Hz to 2 kHz;
- delay shift against an independent clock-count oracle with a moving clock, at three LFO phases and two depths;
- bit-exact bypass, bit-exact tremolo against the legacy `Chorus`, state continuity while off, and reset;
- bounded finite output at 16 kHz to 1.536 MHz with SPEED and DEPTH at maximum.

The expected delay uses a literal 256 in the test, so changing the library constant cannot change the expectation. Mutants built outside the repository and rejected: 255 and 257 periods, clock modulation removed, an instantaneous `256 / clock` delay in place of the clock integral, output-coupling high-pass removed, interstage high-pass removed, reconstruction low-pass removed, tremolo no longer delegating to the legacy law, bypass no longer exact, and half the clock excursion. Not detected: Catmull-Rom replaced by linear interpolation, because the test tones are too low to separate them.

`OutputTapIntegrationTests` and `PraWahIntegrationTests` mirror the engine pipeline; their oracles now use `BbdChorus` in isolation and `Chorus` in the legacy policy, and fail if the engine path is reverted.

The test agrees with these declared equations. It says nothing about agreement with an original CS-80.

## Next evidence gates

1. BA617 control law and the clock-modulation resistors, to replace the +/-0.5 octave excursion.
2. OE1 oscillator roles and waveforms (DO1/DO2) and the four LFO phases driving the IG00151 VCAs, which would give the stereo decorrelation and a source-based tremolo.
3. IG00151 gain laws and OE1 summing, to replace the wet level and polarity.
4. MN3001 loss, noise and clock feedthrough; the sampled-packet reference in `src/reference/Mn3001Packet.h` is the starting point.
