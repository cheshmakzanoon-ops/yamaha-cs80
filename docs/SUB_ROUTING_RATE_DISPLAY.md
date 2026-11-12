# SUB aftertouch speed readout

The Studio SUB/RING page's **PRESSURE AT FULL TOUCH** diagnostic now uses the
same configured speed law as the selected SUB mode. This is a display-only
correction; it changes no oscillator, audio processing, parameter, preset,
host-state or controller behavior.

## Reproduced mismatch

At source `b323b8e8`, setting AT SPEED to 50%, selecting ORIGINAL waveforms and
switching ENGINE to CIRCUIT left the readout at **x2.00**, with **max x3.00**.
The existing circuit converter law is exponential: `(200 / 60)^amount` at full
touch, so those figures should be **x1.83** and **max x3.33**. The engine-mode and
waveform-set switches were also absent from this widget's repaint signature.
See the existing law and its declared inference in
[the circuit workstream, Step 1](workstreams/ws4-circuits.md).

The display now follows this existing mode selection:

| Effective selection | Full-touch speed multiplier |
| --- | --- |
| Legacy engine, either waveform set | `1 + 2 * amount` |
| Circuit engine, legacy waveform set | `1 + 2 * amount` |
| Circuit engine, Original waveform set | `(200 / 60)^amount` |
| PURE CS-80, either stored waveform set | `(200 / 60)^amount` |

PURE forces Original, regardless of the stored waveform switch. Its existing
dimmed presentation is retained. The widget's refresh signature now includes
the effective speed-law choice, so changing either relevant switch updates the
readout without another parameter edit. Zero still reads `0` for no added
modulation, as before. The other five depth readouts are unchanged.

This remains a configured full-touch diagnostic, not a live pressure/rate
measurement. It does not add smoothing, modulation-matrix, macro, or measured
audio telemetry to the display.

## Regression evidence and limits

`cs80_sub_routing_display_tests` constructs the actual detached Studio SUB/RING
component. It does not construct an editor, native window/peer, audio device or
display server. It checks:

- Actual rendered numeric-text crops against independently specified strings
  for all eight combinations of PURE, engine and waveform-set requests, at
  zero, quarter, half and full AT SPEED.
- The separate maximum figure and PURE's forced-Original behavior.
- Engine-only and waveform-set-only repaint invalidation.
- A half-depth circuit crop against the equivalent legacy numeric readout.
- Host parameter values remaining unchanged during drawing and polling.

On Linux/GCC 14.2 with official JUCE 8.0.11, the same final regression source
reports **71 checks / 39 failures** against unchanged production `b323b8e8`,
and **71 checks / zero failures** after the display correction. Optional capture
output adds five passing file-output checks, producing **76 / 39** and **76 / 0**
respectively. The initial test setup had a maximum-label kerning mismatch;
that fixture-only issue was corrected before these baseline comparisons.

The focused CTest selection passes **5/5**: SUB routing display, LFO scope
preview, output-meter display, output-meter primitive and voice activity.
Offscreen captures at 712x100 and 498x70 were inspected for the changed text.
These are detached-widget rendering checks, not native editor, DAW, VST3/AU,
hardware-fidelity, full-suite or release qualification.

With an existing native dependency setup:

```
cmake --build build --target cs80_sub_routing_display_tests
ctest --test-dir build -R '^cs80_sub_routing_display_tests$' --output-on-failure
```
