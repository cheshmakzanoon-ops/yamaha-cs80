# Native VST3 Scala session roundtrip

`tests/Vst3ScalaSessionTests.h` extends the existing actual-module VST3 host.
It loads the built Linux module and uses `IComponent::getState`/`setState` with
`IBStream`; notes and voice-mode changes use native VST3 events/parameter queues.
It does not construct a direct `Cs80AudioProcessor`, link a tuning parser, or
substitute a processor-only test for wrapper qualification.

## Fixture and checks

The fixture captures a real native state, checks the pinned JUCE 8.0.11 private
state trailer, then edits only the `CS80_PARAMS` XML while retaining the complete
trailer. The SCL has five nonuniform steps (100/300/500/800/1200 cents); the KBM
has three entries (`-1`, `0`, `x`), formal octave degree 3, a 440 Hz reference,
and playable range 36 through 91. XML escaping is exercised by the description.
No external tuning file is needed after the embedded session is restored.

For Legacy/Circuit and eight/sixteen voices, the host:

- Restores embedded tuning, renders an audible reference before saving, then
  captures native state and restores it into a fresh component initialized with
  the opposite engine and voice selectors.
- Checks exact SCL/KBM serialization and full native state roundtrip, and compares
  fresh note replay with the pre-save reference using the existing 0.000002
  maximum absolute audio-error bound. This restores saved parameters/tuning, not
  a continuation of the donor's in-flight DSP voices.
- Requires audible differences against absent tuning and an unsigned KBM control.
- Requires exactly silent output for sixteen `x`-mapped/out-of-range note inputs.
- Restores a session without the tuning child and checks both cleared saved
  tuning and equal-temperament audio against the absent-tuning control.
- Attempts malformed SCL and malformed KBM with and without prior tuning. The
  retained session deliberately differs in volume and hardware source selection.
  Complete native state must remain byte-identical, prior audio must remain
  equivalent, and a subsequent valid restore must succeed.

Two more cases consume restored tuning in eight-voice mode before switching to
sixteen voices through a zero-frame native parameter flush. They compare against
an independent instance that restores tuning only at the same activation point,
plus an audible eight-voice control and unmapped-note silence. Both extension
engines start at the same frame, so this comparison does not confuse circuit
free-running oscillator phase with tuning delivery. The four ordinary roundtrip
cases separately cover sessions saved in sixteen-voice mode.

## Verification scope

The existing six CTest registrations remain unchanged: 44.1, 48, and 96 kHz,
each realtime/offline. The original no-extra-arguments invocation is still
48 kHz realtime. Existing automation, policy/program, pan, and MTS SysEx checks
and all frozen tolerance files are unchanged. The new session cases run in each
registration. No production plugin/DSP code is changed.

The first completed run passed the 48 kHz realtime module registration and
the supplemental direct Scala admission registration: **2/2 in 29.88 seconds**.
The module reported zero roundtrip and activation audio error across all six new
session cases. The direct processor fixture reported **103 checks, zero
failures**; it supplements rather than substitutes for native wrapper coverage.
An earlier test process was interrupted by an execution-environment restart
during the existing pan checks, before the Scala cases. That incomplete attempt
is not counted as a pass.

The full registered native matrix then passed **6/6 in 270.76 seconds**.
Each of the six configurations reported six new session cases with zero
roundtrip/activation audio error and zero failures, for 36 session cases total.
The original partition, sixteen-voice, zero-frame boundary, and reader observation
error summaries were also zero in this run. These are measured results, not a
universal bit-exact offline-render guarantee.

Final tested binary identities:

- Native host SHA-256: `47b366839300181e6f303396ff3a16b8a1a0463099c2e242c90103f42bafb732`.
- Built module SHA-256: `fff39d7790875938158ba916f58501645883bc5217af69f67307840d32ed22dc`.

The independent sequential repeat passed **7/7 in 270.33 seconds**, including
all six native registrations and the supplemental direct admission test. Each
native configuration again reported zero roundtrip/activation audio error and
zero failures; direct admission again reported 103 checks with zero failures.
The reviewer verified both exact binary hashes and found no source/documentation
blocker. No primary and independent native workloads overlapped. Test durations
are qualification runtime, not audio-performance evidence.

## Reproduction

After configuring a Linux native build with the plugin and tests enabled:

```sh
cmake --build build --target cs80_vst3_automation_tests cs80_scala_session_admission_tests
ctest --test-dir build --output-on-failure -R '^cs80_vst3_automation(_[0-9]+_(realtime|offline))?_tests$'
ctest --test-dir build --output-on-failure -R '^cs80_scala_session_admission_tests$'
```

## Limits

This is Linux Release, JUCE 8.0.11, float32 audio, 256-frame Scala fixture blocks,
and AVX2 filter batching OFF. It covers the stated embedded fixtures and selected
state/activation order, not arbitrary concurrent restore or session restoration
of in-flight DSP voices. Each instance retains one sample rate/process mode.
It does not qualify external DAWs, GUI/display behavior, live MIDI/MTS-ESP,
64-bit processing, sanitizers, CPU throughput, or hardware fidelity. Earlier
offline comparison failures remain a separate historical limitation; successful
bounded tests do not establish universal exact offline rendering.
