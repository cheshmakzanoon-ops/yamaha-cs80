# OE1/OE2 chorus and tremolo (Source Performance and Isolation)

First implemented 2026-10-30 in `src/core/BbdChorus.h`; extended 2026-10-31 (ws4-circuits
Step 3, [report](workstreams/ws4-circuits.md)) with per-transfer charge loss, the MN3001
output-pair hold, clock feedthrough, independent noise in each chain, true left/right wet
paths and an OE1 tremolo. This is the production audio use of the OE2 evidence
([OE2 circuit evidence](OE2_CIRCUIT_EVIDENCE.md), [filter design](OE2_FILTER_REFERENCE_DESIGN.md),
[MN3001 reference](MN3001_CLOCK_EVENT_DESIGN.md), [BA617 review](BA617_SOURCE_REVIEW.md)).
It is an engineering reconstruction from public sources, not a calibrated model and not a
hardware match.

## Where it applies

| Effective policy | CHORUS mode | TREMOLO mode |
|---|---|---|
| Legacy | Generic modulated delay (unchanged, bit-identical) | Generic amplitude law (unchanged) |
| Source Performance, Isolation | OE2 BBD delay path, clock moved by OE1 DO1 + DO2 | OE1 four-VCA law on the OE2 returns (since 2026-10-31; previously the generic law) |

The original implementation added no host parameter. Its renderer selection still follows
the existing policy, so old presets and legacy sound remain unaffected. Changing the
effective policy already resets all effect history.

### 2026-11-01 independent switch interpretation (declared inference)

The original two-parameter interface (`chorus_on`, `chorus_mode`) selects either
CHORUS or TREMOLO, but cannot represent both controls off or both simultaneously
on. The printed Yamaha BA617 adjustment procedure p21 names separate TREMOLO and
CHORUS tabs; OE57 depicts the DO1/DO2-controlled BBD clock and OE58 the four
separate L1–L4 VCA controls. These pages support distinct control nodes, **not**
the unverified position, electrical interaction, or simultaneous user operation
of both physical tabs.

The appended `chorus_trem_routing` host parameter therefore defaults to 0
(previous selector, completely neutral), with opt-in positions 1 neither,
2 CHORUS, 3 TREMOLO, and 4 both. In the combined software model the clock
follows the existing CHORUS law while the same wet packet returns receive the
existing TREMOLO VCA law. This is a **declared inference**, not a new hardware
measurement; the alternatives are mechanically exclusive tabs, priority of one
control over the other, or non-additive switching/cross-coupling. The legacy
six-argument API and all PURE CS-80/Legacy playback ignore the new override.
New positions are not a claim of literal hardware switch calibration.

## Signal path

Taken from the printed OE2 graph:

```text
mono sum of the stereo bus (OE1 O1 -> OE2 I(53))
  -> IC3 Sallen-Key low-pass (10 k, 10 k, 1.8 nF, 680 pF: 14.39 kHz, Q 0.8135)
  -> MN3001, two tied-input chains on one clock:
       packet acquired on CP2, at upper512 256 periods later; OUT1/OUT2 hold it one period
       512 transfers with a declared inefficiency; independent noise per chain
  -> per chain: OUT1/OUT2 through 2 x 2.2 k (1.1 k Thevenin), 1 uF into 100 k (1.57 Hz)
  -> IC5 (upper chain) / IC6 (lower chain) low-pass, loaded by the 1.1 k: 13.67 kHz, Q 0.8124
  -> 0.1 uF into 10 k interstage coupling (159.15 Hz high-pass)
  -> IG00151 VCAs: O1 (IC7), O2 (IC9) on the upper chain -> OE1 I2/I3 -> LEFT
                   O3 (IC8), O4 (IC10) on the lower chain -> OE1 I4/I5 -> RIGHT
  -> summed with the unchanged stereo dry signal
```

- One packet history serves both chains (tied inputs and clock on OE57); each chain has
  its own noise, recombination, coupling and reconstruction filters, so left and right
  wet signals are genuinely separate paths.
- The delay is not `256 / current clock`. Each input sample is stamped with the clock count
  when it enters, and the output reads the packets that entered 256 periods ago, so a
  moving clock is integrated, as in the real device.
- The output reads seven taps on that clock-count axis: the per-transfer loss smear (the
  first three negative-binomial terms over 512 transfers) convolved with the one-period
  output-pair hold (Simpson weights at 256, 256.5 and 257 periods). The effective delay
  centre is therefore 256.5 periods plus the 0.26-period loss mean.
- Clock feedthrough is the residual square wave at the clock, synthesized band-limited:
  only its odd harmonics below 0.45 x the internal rate, faded in over 0.05 x the rate.
  At the usual 88.2-96 kHz internal rates the clock is above Nyquist and nothing is added;
  it is ultrasonic and inaudible at every rate.
- The VCA trims normalize each wet branch to unit magnitude at 1 kHz, then weight the four
  branches in the documented 0.65/0.58/0.58/0.65 proportions. At rest each side sums to
  the 0.5 wet gain.
- The delay chain, its noise, filters and both oscillators keep running while the effect
  is off. Output with the effect off is the input, bit for bit.

## Constants and their evidence class

| Constant | Value | Basis and what is unknown |
|---|---|---|
| Nominal clock | 106.9045 kHz | INFERRED: geometric centre of the 7-12.5 us TP1 period interval in the Yamaha adjustment procedure. The installed VR1 setting is unknown. |
| Clock excursion | +/-0.5 octave at full DEPTH | INFERRED: spans about the same 1.7-3.4 ms delay interval as the adjustment range. The BA617 control law is not recovered. |
| Clock modulation source | mean of DO1 and DO2 | INFERRED: the BA617 clock node has two equal 100 k modulation feeds (OE57); one per oscillator. |
| DO1 / DO2 | DO1 at the SPEED value in Hz, DO2 at 0.056/0.55 of it | Ratio source-derived (adjustment procedure printed 21: 56 ms and 0.55 s at maximum SPEED, TREMOLO on). Tracking one SPEED control and sine shapes are INFERRED. The documented 17.9 Hz maximum is beyond the host's 8 Hz range. |
| Output-pair hold | one clock period, centre 256.5 periods | Derived from the MN3001 graph (OUT1 on CP2 slots, OUT2 on the following CP1 slots, [MN3001_CLOCK_EVENT_DESIGN.md](MN3001_CLOCK_EVENT_DESIGN.md)). |
| Transfer inefficiency | 5e-4 per transfer, 512 transfers | INFERRED: typical of PMOS bucket brigades; no MN3001 value was available. |
| Packet noise | 1.5e-4 engine units rms per packet, independent per chain | INFERRED: about -79 dB below a 0.5-peak sine in the wet path. |
| Clock feedthrough | 0.01 engine units residual square | INFERRED: what remains after the complementary OUT1/OUT2 pedestals cancel. |
| Recombination Thevenin | 1.1 kOhm | Printed 2 x 2.2 k (OE57); stiff pins are an INFERRED boundary. |
| VCA trims | 0.65/0.58/0.58/0.65 proportions, referenced to 1 kHz | Proportions source-derived (adjustment procedure printed 21); the 1 kHz reference is INFERRED (the printed key label is not mapped to a pitch). |
| Wet level | 0.5 of the dry level per side at rest | INFERRED: OE1 mixing gains are not recovered. |
| Wet polarity | Inverted | IC5/IC6 are printed inverting stages. The IG00151 and OE1 summing polarities are not printed, so the net sign is unresolved; the opposite sign moves the comb notches. |
| TREMOLO law | linear VCA control, index 1 at full DEPTH; LO1 = +DO1, LO2 = +DO2, LO3 = -DO2, LO4 = -DO1 | INFERRED: the four VCA control lines L1-L4 come from OE1 LO1-LO4 (OE58); phases and control law are not recovered. Left and right move in antiphase at two rates; the mono sum of the wet returns stays constant. |
| TREMOLO dry path and clock | dry kept, clock nominal | INFERRED "minimal switching": the 159 Hz interstage high-pass would make a wet-only tremolo bass-less. |

Alternatives considered for the tremolo are in the [ws4 report](workstreams/ws4-circuits.md#step-3-oe1oe2-chorus-and-tremolo).

## Not modelled

MN3001 bias and input aperture, leakage, nonlinear charge transfer and distortion, the
IG00151 exponential control input and its CV feedthrough, OE1 FET switching transients,
the VCA output capacitors (0.015 uF, revised) and the OE1 100 k input coupling, the 0.1 uF
smoothing at the BA617 control node, and component tolerances between the two chains.

## Audible change from legacy

The delay is 1.7-3.4 ms instead of up to 18 ms, so chorus mode sounds like a short comb
sweep rather than a wide chorus; since 2026-10-31 the sweep has a second, ten-times-slower
component from DO2. The wet path has no content below about 159 Hz and rolls off above
the 14 kHz filter corners. Tremolo modulates the wet returns in left/right antiphase at
two rates rather than the whole signal, so it has a phasing, rotary-like character. Each
side carries its own low BBD hiss whenever the effect is on.

## Tests

- `tests/BbdChorusTests.cpp` (`cs80_bbd_chorus_tests`, 70 checks): printed corner and Q;
  wet response against the declared analytic chain (literal 256.5 periods, 512 transfers at
  5e-4, 1.1 k loading, 1 kHz trim) at 88.2, 96 and 192 kHz; delay shift against an
  independent clock-count oracle with the DO1 + DO2 clock; bit-exact bypass; tremolo is not
  the legacy law; state continuity while off; reset equals a fresh instance; bounded output
  from 16 kHz to 1.536 MHz.
- `tests/Oe2StereoChorusTests.cpp` (`cs80_oe2_stereo_chorus_tests`, 97 checks): left and
  right wet transfer, each against its own truth branch built from the nonideal MN3001 packet
  graph (`NonidealChannel`), the two-level output staircase and `oe2_filters::branchTransfer`;
  noise level against the analytic truth and L/R independence; clock feedthrough where
  representable and no alias where not; tremolo sample trajectories against
  `reference/Oe1Modulation.h`. The mutation checks are recorded in the ws4 report.

The tests agree with these declared equations. They say nothing about agreement with an
original CS-80.

## Next evidence gates

1. BA617 control law and the clock-modulation resistors, to replace the +/-0.5 octave excursion.
2. OE1 oscillator waveforms, the LO1-LO4 phase network and the IG00151 control law, to
   replace the declared tremolo phases and linear index.
3. IG00151 gain laws and OE1 summing, to replace the wet level and polarity.
4. MN3001 transfer efficiency, noise and pedestal data.
