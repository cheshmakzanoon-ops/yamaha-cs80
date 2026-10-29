# Keyboard contour settled-DC experiment

This is an **isolated reference**, not a production synth feature or a claim of physical-key mapping / measured analog equivalence. Neither Engine nor the host parameters/UI use it.

## Inputs and scope

`src/reference/KeyboardContour.h` takes explicit physical CMOS Q1/Q2/Q3 booleans, an independently supplied selected OC0..5, independently supplied note-ladder tap C#=0 through C=11, electrical LOW/HIGH pot travel in [-1,1], and VR1/VR2 in [0,5000] ohms. No MIDI note, octave number, code-to-OC inference, default trim calibration, dB or cutoff-octave conversion exists. The same nominal circuit can represent KBC1 or KBC2; its downstream loads differ and are not included.

It models ideal linear center-tapped10k pots loaded by56k, both15k/56k biased IC14 summers, the1.5k/3k octave ladder, ideal selected OO buffer, eleven1k note segments with conditional active-high endpoint switches, and O1=225/56−Vsample. Reported references and wiper voltages make node comparisons possible. Invalid ranges/non-finite analog inputs throw; there is no silent clamping.

Assumptions: exact nominal rails/resistors; ideal unsaturated opamps; negligible analog-switch resistance/leakage; unloaded OC taps and sample node; settled DC with no scan/hold timing. These assumptions are explicit, not validated device laws.

## Evidence and calibration

See [the evidence ledger](KEYBOARD_CONTOUR_EVIDENCE.md) for original KBC15, Panel2p64, KAS04/05/06 and source job provenance. Gate wiring is independently corroborated, but the physical61-key/code/OC mapping remains unresolved. The original CS80 adjustment manual PDF29 (printed27) specifies LOW minimum/C2/VR1/O1=−2.76±0.14V and HIGH maximum/C5#/VR2/O1=+2.76±0.14V on both boards.

Tests SUPPLY Q110, OC1=L, tap11 for a low fixture and Q101, OC5=H, tap0 for a high fixture. They derive VR1≈2136.2862ohm and VR2≈4136.2862ohm for the nominal2.76V magnitude. This establishes conditional circuit consistency only; it does **not** prove these are the original keys' codes or settle their MIDI numbering. No Johnson-cycle guess has entered the API.

## Checks

`cs80_keyboard_contour_reference_tests` checks all8 raw states ×6 externally selected OC taps ×12 note taps at center; electrical endpoint/loading polarity; independent pot/summer/octave KCL; a separate12-node Gaussian-elimination conductance oracle using decoded gate expressions; LOW/HIGH combinations, trim extremes, conditional calibration fixtures, and input rejection. The implementation uses closed-form endpoint equations; the oracle instead stamps resistors and voltage constraints.

Run:
```sh
cmake -S . -B build -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build --target cs80_keyboard_contour_reference_tests
ctest --test-dir build -R cs80_keyboard_contour_reference_tests --output-on-failure
```

Passing these checks validates the stated nominal electrical experiment, not a1:1 hardware clone. Integration must wait for the missing physical code mapping and receiving-node calibration.
