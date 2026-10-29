# Aftertouch sub-oscillator filter depth

> **Policy precedence (2026-10-29):** Under [the current requirement](ANALOG_FIDELITY_REQUIREMENT.md) and [roadmap](PARITY_ROADMAP.md), missing physical measurements do not block development or reviewed integration. Use public evidence plus explicitly labeled engineering inference, with assumptions, uncertainty and tests. Earlier measurement/promotion gates below remain limits on claims of physical verification, not a prohibition on inferred models; recorded offline status and source conflicts remain unchanged.

## Evidence and scope
Yamaha CS-80 owner's manual, printed page39, TOUCH RESPONSE [12], states that
SPEED, VCO and VCF use the Sub Oscillator FUNCTION and basic SPEED, with pressure
increasing the selected effect. [Original manual scan/OCR](https://device.report/m/08eee3fd014ab14d5f15fe50a150900b9c3ed22ad75895b5736f46b2e186f9b0).
The implementation previously exposed only AT SPEED and AT VCO. This increment
adds the missing AT VCF depth control to the main panel and host automation.

The selected shared sub waveform is multiplied by each voice's pressure and the
AT VCF depth, then added to that voice's existing sub-filter modulation. It
affects both HPF and LPF of both ranks. This is separate from each rank's sustained
AFTER BRILL offset. Zero depth retains prior behavior; releasing pressure removes
this added modulation. Existing MIDI poly-pressure and channel-pressure dispatch
supply the pressure values.

## Explicit fidelity limit
This closes a missing behavioral route, not the full circuit-fidelity requirement.
The current 1.5-octave scaling, linear pressure/depth multiplication, instantaneous
pressure update and shared sub waveform implementation remain provisional.
Lever taper, pressure voltage calibration, receiving resistor network, analog
smoothing and saturation must be derived and compared against hardware. Slider
direction and the other global controls still require the complete panel-direction
audit. No exact transistor/IC or measured1:1 equivalence is asserted.

## Compatibility
Core indexes0..121, both30-control channel blocks and their IDs remain fixed.
The new core extension is122, ID sub_at_vcf, defaultzero. Core-index order and
host-index order are intentionally different: the host parameter is appended
after all228 previously shipped parameters, at index228 with JUCE AU versionhint3.
The original122 retain hint1; programmer controls retain hint2.
State completion supplies defaultzero to old states missing the parameter.
Memory selection does not overwrite this live global control.
Factory presets also default it off until reference preset wiring is established.

## Verification
Core regressions cover zero-depth audio identity, bipolar time-varying cutoff
rather than static brightness, independent held-key pressure, both filters/ranks,
pressure release, and44.1/48/88.2/96kHz. Host/UI regressions use a literal snapshot
of all228 previous IDs/order, versionhint checks, automation-to-core delivery,
state roundtrip, missing-state defaulting, live slider updates and real keyboard
gesture checks. These are numerical/software regressions; Actions must execute
them before any passing result is claimed.
