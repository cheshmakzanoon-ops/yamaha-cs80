# W0: opt-in parity isolation

Implemented after baseline `d56911a7c5f6953cd244457226b126cc567a538b`.
This is a comparison fixture, **not calibrated hardware parity**. It removes modern
signal-path extensions while retaining the current provisional circuit models.
The [roadmap](PARITY_ROADMAP.md) remains the acceptance authority.

## Contract

`parity_mode` is appended at host index 243 (244 total), AU version hint 6,
core index 137. Default off. Existing IDs/order/hints are unchanged. Absent,
nonfinite, fractional or malformed saved values restore off. Automation uses the
normal Boolean half-range threshold. Session restore restores the flag; factory
and user preset recall preserve the current session flag.

The Advanced / About page exposes **PARITY ISOLATION**. Original panel controls
continue working. Modern controls remain stored and editable, but their effective
settings are masked while isolation is on; switching off restores those settings.
Preset voicings remain provisional, including the 22 hardware-named studies.
Selecting a performance preset does not turn isolation off or make it historical.

Mode transitions clear sounding notes, release tails, controller state and effects
history, including tape-delay damping. This deliberate discontinuity avoids
carrying modern modulation into a comparison. Change mode before playing or
capturing; it is not a seamless performance switch. The plugin reapplies its
physical pedal position after the reset. New notes must be triggered afterward.

## Effective masks

- Three modern FX slots bypassed; LFOs 1–3 depth/destination disabled; four macros
  held at their neutral 0.5 values before derived settings are computed.
- Arpeggiator and MPE disabled; pitch/mod wheels, MIDI CC1/11/64/74 ignored.
  Channel-scoped panic CC120/123 remains available.
- Both rank filters enabled, rank pan centred, output mono-fold disabled.
- Original SUB forced on; legacy SUB waveform/rate controls inert. Ring base speed
  capped at the documented 200 Hz endpoint.
- KEY RELEASE zero; A4 reference fixed at nominal 440 Hz. Original master tuning,
  rank detune, sustain/assigners, ribbon and touch routes remain active.
- Arbitrary voice dispersion, drift and background noise zeroed. Zero variation
  is a nominal fixture, not measured component tolerances or a noiseless-CS-80 claim.
- Provisional output DC blocker and output soft clip bypassed. Output may exceed
  digital full scale: capture floating-point data and monitor at a safe level.
  Filter saturation and other provisional internal laws are not thereby calibrated.

## Explicit transport/numerical exceptions

The 2x numerical rate and 255-tap output decimator remain, with the same reported
63-host-sample latency. They are numerical implementation choices, not physical
features claimed for the CS-80. Removing anti-alias filtering would not establish
analog fidelity. Comparison manifests must state the relevant stage latency;
the EXT-control interpolation delay is separately 63.5 host samples.

EXT remains available because it transports the original external modulation
function. Its digital normalisation is provisional, not a calibrated voltage.
Physical expression uses `Engine::setExpression`, separately from ignored MIDI
CC11. Ribbon, polyphonic/channel pressure and on-screen pressure/foot controls
remain as software transports for original performance inputs. The on-screen
footswitch is still a latching stand-in, not a claim about contact mechanics.

## Verification

`cs80_parity_mode_tests` checks masks individually and together, original controls,
controller isolation, ring limit, mode transitions, retained stored values and
linearity of direct expression. Mutation checks deliberately break macro neutral,
filter enable, ring cap, controller masks, output clipping, transition reset and
delay damping cleanup and require failing tests.

Plugin smoke tests pin host index/hint/default, session migration, malformed states,
factory/user preset policy, all 15 true Boolean parameters, GUI toggling, physical
expression and polyphonic pressure in isolation. Exact-commit CI, not this prose,
establishes whether plugin/platform checks passed.

The separate [comparison harness](PARITY_HARNESS.md) uses synthetic stand-ins only.
No calibrated captures or hardware acceptance thresholds have been supplied.

### Default-off compatibility checkpoint

On the same GCC 14.2 Release build, `src/tools/compatibility_probe.cpp` rendered
all 222 factory presets at 44.1/48/96 kHz with a fixed note/controller sequence,
24 blocks of 257 frames per preset. Raw interleaved float bytes matched the
pre-change baseline `d56911a7c5f6953cd244457226b126cc567a538b` exactly:
SHA-256 `4a3483f77c9df9895386b98eaa169358d19b463c21283231f994c5d0f6c523de`.
This is one finite regression corpus, not a proof for every possible automation
sequence or platform. Compile the probe separately against each revision's
headers and `cs80_core` library, run with an output-file argument, then compare
raw files. Do not use one revision's headers with the other's binary library.
