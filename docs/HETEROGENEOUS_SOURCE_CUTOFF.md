# Packed cutoff readouts for different voice drives

The exact equal-drive specialization still runs first. When it does not
apply, the Engine can evaluate the cutoff of distinct voices in SSE2
pairs in the verified Linux x86-64 GCC 14.2 arithmetic configuration.
Both touch and envelope sums still accumulate voice 0 through 7 in order.
The high-pass and low-pass sides use independent vector lanes with the
same operations and the same scalar rounding boundaries. No filter stage,
clock, control input, audio path, approximation or state update is
removed.

An integer representation check validates the model identity, drive
domains and coefficient bounds before any extra floating-point work.
The bounds rule out overflow, underflow and saturation on the admitted
path. Tiny, out-of-domain, nonfinite, or unsupported values, other
arithmetic policies and unmasked exceptions use the ordinary scalar
transaction. No state or readout is changed on rejected scalar inputs.

## Exactness and scoped work

- **374,307** factory nominal-model checks passed, with exact
  heterogeneous output and scalar fallback regressions.
- The standalone differential and fuzz suites paired 1,086,712
  component calls. Native exception/continuation checks exercise the
  original protected arithmetic rather than a different trap policy.
- The compiled Linux VST3 matched the preceding FIR-enabled main build
  across **112,640 real callbacks, 7,208,960 stereo frames and 113 public
  component snapshots per arm**. These include differing MIDI velocities,
  moving controls, native held, settled release and wake. All six
  independent native automation cells matched 216 captured files.
- A separate translated work comparison of the same production module
  passed identical-stock and independently counted extra-work controls.
  It executed **1.438% fewer guest instructions** with varied notes held,
  **0.942% fewer** over a varied-velocity wake window and **0.726% fewer**
  with equal-velocity notes held. Fresh quiet work was **0.060% higher**,
  with unchanged guest read/write counts in that fixture.

Instruction counts, data references and fallback tests do not establish
native CPU time, a DAW load reduction, or representativeness for all
platforms and patches. The modest fresh-case increase is retained rather
than trimmed or hidden behind a pooled average.

A separately declared longer translated experiment rendered 1,024 varied
held callbacks followed by 8,192 true release callbacks before the quiet
window. The production candidate executed **5.092% fewer guest
instructions** over four selected genuine VST3 callbacks, with 1.231%
fewer guest data reads and 5.697% fewer guest writes. Stock repeats
matched; counting all five calls was exactly the first four plus the
independently counted fifth. All 9,222 stock/candidate audio and recorded
state/FP observations matched within the native runtime and within the
translated runtime. The stock native/translated release tail contained
small different float words (largest absolute difference about
1.187e-38), so this long result remains a scoped translated-work
diagnostic, not a native CPU measurement. It is not pooled with the
four shorter scenes.

Verified Linux release candidate ELF SHA-256:
`b98341f274ad88e83ffce342a0828eed8de4a1260f77083c64810b18ebab0c10`.
