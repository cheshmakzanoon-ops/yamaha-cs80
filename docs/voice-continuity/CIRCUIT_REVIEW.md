# Independent electrical-state continuity review

2026-10-30. Public-source inspection supporting a **nonproduction, nominal
continuous-circuit experiment**. This is not a measured CS-80 model, an Engine
integration approval, or a CPU qualification. The playable `428e4b2` behavior,
existing three render policies, and their exact compatibility gates are outside
this experiment.

## M41: what the inspected connections establish

The [Yamaha M41 drawing][m41] connects TR63 to the IC12/IC13 envelope gates.
No traced TR branch resets IC8's timing circuit or the IC10/IC11 audio-filter
capacitors. IC8 has key voltage K37 at pin2, footage at pin1, modulation at pin15,
and its local oscillator-cycle timing network at pins10–14. Its cycle reset must
not be confused with a keyboard-triggered reset. Both filter chips have connected
0.0015 uF C1/BP and C2/LP capacitor branches, without a drawn TR discharge switch.

This supports retaining and evolving circuit state when the downstream VCA is
silent, rather than resetting it because a software voice was retired. It does
**not** identify an exact oscillator waveform, transient CV feedthrough, numerical
SVF-to-capacitor conversion, or continuously positive oscillator frequency for
every possible key voltage. Reusing the existing oscillator/SVF laws is an
explicit engineering approximation, not a recovered electrical solution.

The independently rechecked [IC42 table][ic42] gives 0.250 V -> 130.8 Hz,
0.500 V -> 261.6 Hz and 1 V -> 523.2 Hz. A default 523.2 Hz/V therefore uses a
rounded published anchor. It must not be silently substituted for the existing
Engine's separately chosen A4=440 MIDI interface calibration.

## SH7/SH8: conditional key-voltage hold

[SH7][sh7] routes KI through a controlled bilateral switch to the K buffer. The
switch control is NAND(TR, SUS). SUS has a 22 kOhm pull-up to +8.5 V.
[SH8][sh8] identifies IC1/3 as TC4011P, IC2/4 as TC4016P, and the main IC5–12
buffers as CA3140T. The exact electrical truth table is:

| TR electrical level | SUS electrical level | NAND output | Ideal KI-to-K boundary |
|---|---|---|---|
| Low | Low | High | Track/acquire |
| Low | High | High | Track/acquire |
| High | Low | High | Track/acquire |
| High | High | Low | Hold |

"Track" here means the switch conducts; an instantaneous assignment `K = KI`
is an **ideal-switch approximation**, not measured acquisition behavior.
"Hold" does not mean zero leakage for an unlimited duration.

[Panel66][panel66] SW3-T1 goes to SH SUS; SW3-M1 goes to VR2-3, which is tied to
KAS -6.5 V terminal96. The other SW3 contact is open. SH8 reciprocally identifies
SUS17 as PN3-SW3-T1 plus SH-SUS33. Thus one switch position leaves the pull-up
high, while the other forces SUS low. Preserve the source discrepancy: SH7
prints terminal18 for the upper SUS input, while SH8 and the revised Panel66
annotation identify17. This does not change the named-net logic.

The [Yamaha servicing guide][guide], PDF34–35 / printed25–26, directly explains
the same NAND acquisition/hold sequence: Sustain I acquires during key-on and
holds on release; Sustain II keeps the switch conducting. **That explanatory
example is explicitly CS-50/four-channel.** Its 0.22 uF value and statement that
free channels receive the latest key code are not independently established
CS-80 component/allocator facts merely because the switch logic agrees.

SH7 separately draws a 0.02 uF input capacitor in its alternate IC5–12 application
note (labelled CA4016P as printed); it is not drawn at every main buffer input.
Do not turn that note into a universal installed capacitance or leakage time.

## Upstream boundary and polarity cautions

[KAS6][kas6] plots KO key-on at -6.5 V and key-off at +8.5 V. Its key-code
notation labels the negative level "1". That is **not** the conventional
electrical-high Boolean used in the NAND table above. An API should explicitly
name `trHigh` and `susHigh`, rather than accepting an ambiguous key-code bit.

[KAS4][kas4] routes KO through a noninverting buffer and a PNP level-shift stage
to TR. The transistor's arrowed upper terminal is the emitter/TR node, not an
NPN collector. [KAS5][kas5] reciprocally connects TRn to SH-TRn and Kn to SH-KIn.
The guide's explicit active-low TR waveform corroborates this interpretation.

KAS6 also says the power-on clear memorizes C4# after 2 +/- 1 seconds. This is a
documented **key-code initialization**, not proof that every SH output has
already acquired it, nor a startup phase/capacitor specification. Explicit cold
key voltage and rank phases remain appropriate experimental inputs.

Unconditional "hold the last assigned MIDI note forever" is therefore not a
source-derived rule. Neither is continuing a MIDI glide through all releases.
The bounded kernel should receive electrical KI/TR/SUS, with allocator, glide,
ribbon placement and MIDI-to-voltage mapping owned by a separately declared
adapter. No global `lastNote` policy should be hidden inside it.

## Independent software acceptance boundary

- Keep production Engine, parameters, presets and all existing golden outputs
  unchanged; a new model version/opt-in would require separate integration review.
- Advance both oscillator/rank chains and both HP/LP memories at explicit internal
  sample rate even when output gain is zero. Output gain is a downstream mute,
  not a switch disconnecting oscillator/noise from the filters.
- Treat bypass as output selection, with the hidden states still clocking; retain
  the historical production bypass behavior outside this experiment.
- Supply noise explicitly. Do not consume extra shared Engine RNG draws while
  clocking inactive circuits and thereby change other voices' noise histories.
- Keep EG/touch/loaded-bus updates caller-owned, once per internal sample; the
  kernel must not double-clock source filter EGs already advanced elsewhere.
- Validate the whole sample transaction before mutating K, either rank, filter
  coefficients, or clocks. Reject invalid preparation similarly. Independent
  tests must check future trajectories too, not only visible immediate snapshots.
- Test all four electrical gate combinations, muted/bypassed continuation,
  cold state, invalid inputs, rate changes, long-run finiteness and exact sample
  sequence determinism. These test the declared model, not hardware equivalence.

## Reproducible primary-source bytes

Originals were inspected locally, including native crops and rendered guide
pages; no manual scans are committed. SHA-256:

| Original | SHA-256 |
|---|---|
| M41 | `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080` |
| IC42 | `5698fdfd19600f6b0b8b8f9dfdfc70723248b4b33e8daad39b9f9c0ee11cc24d` |
| SH7 | `99ac2cbfe0551bbd0d536f94ab6e92bbf924e5bf21b6c646c07b17459495de0c` |
| SH8 | `657c27864bd78456d5c88ab84c92027bf0a7648815765d3ff87ebd4cbfaa4e46` |
| Panel66 | `d24815efb81591c6a194020490627d68c6dc83de94f8c39b436fb0eba7bbd64c` |
| KAS4 | `989e4755453bc030e01d329bd5ce58d2ee3461bb173131c7b8f0805c85489cc9` |
| KAS5 | `16540f43305c2cc56a96e1e4b4c8c35af724bd4e0b3791fd9adaa33d52f0e548` |
| KAS6 | `89db6cb157f6bb6ec195e66e19739a8231a8bc44828eccbf3548d35607bf0324` |
| Servicing guide | `2738f7ddc46f0bedbab38dcad8e2c112c41b3e6ca34670d471d0cb8c0dbde633` |

[m41]: https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg
[ic42]: https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/42%20-%20M%20Circuit%20ICs.jpg
[sh7]: https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/07%20-%20SH%20Circuit%20Diagram.jpg
[sh8]: https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/08%20-%20SH%20Circuit%20Board%20%26%20Wiring.jpg
[panel66]: https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/66%20-%20Panel%203%20Circuit%20Diagram.jpg
[kas4]: https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/04%20-%20KAS%20schematic.jpg
[kas5]: https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/05%20-%20KAS%20Circuit%20Board%20%26%20Wiring.jpg
[kas6]: https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/06%20-%20KAS%20ICs.jpg
[guide]: https://www.goltsynth.com/resources/downloads/CS/cs_series_servicing_guide.pdf
