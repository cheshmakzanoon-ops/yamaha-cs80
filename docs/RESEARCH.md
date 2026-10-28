# Research notes and scope

This is an independent behavioral recreation. It does not ship any Yamaha or Arturia samples, presets, artwork or binaries.

## The hardware

The Yamaha CS-80 was released in 1977. The references below describe it as an eight-voice polyphonic analog synthesizer with a 61-note weighted keyboard, polyphonic aftertouch, a ribbon controller, and two independent synthesis layers per voice. The two layers each have their own oscillator, high-pass and low-pass filters with resonance, filter contour and amplifier contour. The instrument therefore contains sixteen voice boards/chains, not eight.

The same references identify the performance and tone-shaping system that this project models:

- dual channel/rank architecture with channel balance and independent panel controls
- serial high-pass into low-pass filtering
- separate filter and amplifier envelopes
- global sub-oscillator as a modulation source
- ring modulator with its own envelope
- chorus/tremolo with a BBD-style delay pair
- eleven hardware tone-selector positions per channel
- velocity, polyphonic pressure and ribbon as separate performance dimensions

Sources:

- [Yamaha CS-80 — Wikipedia](https://en.wikipedia.org/wiki/Yamaha_CS-80)
- [Yamaha CS-80 history — Perfect Circuit](https://www.perfectcircuit.com/signal/yamaha-cs-80-history)
- [Yamaha CS-80 Owner's Manual (PDF)](https://synthfool.com/docs/Yamaha/CS_Series/Yamaha%20CS-80%20Owners%20Manual.pdf)
- [Yamaha CS-80 architecture discussion — Gearspace](https://gearspace.com/threads/yamaha-cs-80-architecture.1413333/)

## Arturia CS-80 V4

Arturia's current product page and manual describe the V4 modern layer as including MPE compatibility, analog dispersion, a modulation system, three FX slots, a ribbon controller and a large preset library. A retailer summary lists three FX slots with fifteen effects, a two-source modulation mixer with three destinations, and over 200 presets.

Sources:

- [Arturia CS-80 V overview](https://www.arturia.com/products/software-instruments/cs-80v/overview)
- [Arturia CS-80 V 4 manual (PDF)](https://dl.arturia.net/products/cs-80v/manual/cs-80-v_Manual_4_0_1_EN.pdf)
- [Arturia CS-80 V4 feature summary](https://www.proaudiodesign.com/products/arturia-cs-80-v4-license)

## What this implementation covers

| Reference idea | Implementation |
| --- | --- |
| Two independent layers per voice | Two full ranks per voice, each with VCO, HPF, LPF, filter envelope, amplifier envelope and pan |
| 8-voice polyphony | Eight-voice allocator with oldest-idle/oldest-voice stealing |
| 16 voice chains | 8 voices x 2 ranks |
| HPF into LPF | Serial TPT state-variable filters with independent resonance |
| Filter/amplifier contours | Independent ADSRs with bent decay/release and initial/attack levels |
| Polyphonic aftertouch | Per-note pressure routed to filter brilliance and VCA level |
| Ribbon | Continuous spring-return pitch controller |
| Sub-oscillator | Six-waveform global modulation source with VCO/VCF/VCA/AT routing |
| Ring modulator | Global carrier, AD envelope, depth and dry/wet amount |
| Chorus/tremolo | Stereo BBD-style delay pair with complementary modulation |
| Tone selector | Eleven hardware tone positions per channel |
| Modern layer | Three LFOs, MPE, dispersion, macros, three serial FX slots, arpeggiator |
| Preset library | 222 original factory programs |

## Deliberate differences

- **FX count.** The modern layer has three slots and five effect algorithms (chorus, phaser, tape delay, plate reverb, drive). Arturia's V4 lists fifteen FX choices. The five here are fully implemented in the DSP core; no placeholder menu entries were added just to match a feature count.
- **Modulation routing.** The modern layer has three LFOs with fixed destination menus and a set of dedicated performance routes. It is not a general-purpose modulation matrix.
- **Preset content.** The 22 tone-selector programs and 200 performance patches are original parameter states generated from the engine. They are not copies of Yamaha ROM programs or Arturia presets.
- **Analog modeling.** The aim is a stable, playable behavioral model with circuit-informed oscillator/filter/envelope choices. It is not a SPICE netlist simulation and does not claim component-level accuracy.
- **Formats.** VST3 and Standalone are built on all platforms; AU is built on macOS. AAX is omitted because the AAX SDK is not redistributable here.

## Why these choices

A hardware CS-80 is a large, expensive, high-maintenance instrument. A credible software instrument needs to preserve the *relationships* that make it feel like a CS-80: the two-layer voice, the HPF/LPF order, the pressure-to-brilliance response, the slow brass envelope, the ring and sub-oscillator as global performance sources, and the separation between the authentic panel and the modern layer.

The implementation therefore prioritises:

1. A correct, testable voice architecture over a large number of shallow controls.
2. Real DSP for every effect and modulation route over decorative UI.
3. Deterministic, bounded rendering so it can run in a host and in CI.
4. A panel that a player can read and operate, not a wall of generic knobs.
