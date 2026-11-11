# VST3 key automation on current main

## Scope

This integration preserves host sample offsets for twenty VST3 parameters in
modern circuit policy: global volume, mix, brilliance, resonance, master tune
and detune; each rank's HP cutoff/resonance, LP cutoff/resonance, VCA level and
pulse width, plus both rank pan controls. The [pan extension](VST3_PAN_AUTOMATION.md)
appends two lanes to the original eighteen without moving their indices. It does
not replace DSP smoothing or change a parameter law.

The main product explicitly selects only VST3 in `CS80_HOST_AUTOMATION_FORMATS`.
The reusable generator retains its reviewed multi-format capability, but main's
AU interface source list and native wrapper remain untouched. AU scheduled
ramps are not enabled or qualified by this change. Standalone has no VST3 queue.

## Timing and ownership

- Native VST3 queues are borrowed only during the synchronous process call.
  There is no arbitrary point cap or audio-thread queue copy.
- Normalized ramps start from the previous value at sample -1. Constant spans
  can render together; changing samples reach the engine before MIDI allocation.
- Legacy and PURE retain final-point-at-block-start behavior. Unsupported
  parameters remain block-rate. Parameter-only zero-frame flushes keep JUCE's
  existing path.
- Current main's physical-unit `ControlFrame { cached, cachedHardwareSources }`
  is retained. No programmer circuit, factory-tone revision or CPU/phase branch
  change is imported. Hidden panel trajectories cannot override selected factory
  or memory controls. Pan is independent of the programmer and remains live
  with every source selection, matching its existing block-rate authority.
- External state/UI/zero-frame changes refresh the next implicit starting value.
  Endpoint publication preserves full curve precision until externally changed.
- Learned ENGINE/PURE controls determine timing before ordinary learned controls,
  using one bounded MIDI-map snapshot and the original block-entry PURE mask.
- A valid modern-to-Legacy program recall finalizes old curve endpoints before
  preset writes and cancels their trajectories. Same-policy modern recalls keep
  their curves. Later recalls cannot revive cancelled trajectories.

The intended recall contract for a process call that actually carried supported
VST3 curves is coherent publication to already-active instruments, while keeping
main's historical active-extension lifetime. For that integration, ordinary
no-queue, AU and Standalone recall was a separate baseline-preservation requirement.
The later [active-extension program correction](PROGRAM_EXTENSION_RECALL.md)
explicitly supersedes this requirement only for control-frame publication after
a valid ordinary Program Change with an already-active extension. It retains
block-boundary activation/retirement and the existing VST3-curve path.

## Verification

The original eighteen-lane curve contract and all numerical tolerances remain
unchanged. A separately versioned pan roster appends two lanes; the native
VST3, learned-policy, program and wrapper gates retain their existing criteria. The additive `MainVst3OnlyAutomationTests.py` verifies
that the actual product selector patches VST3 .cpp/.mm sources and never touches
AU source paths, ordering, properties or bytes, including invalid AU shapes.

`Vst3MainCompatibilityCapture.cpp` runs the same 336 native cases against the
clean exact-main module and candidate. Complete audio, all controller parameter IDs in order, and normalized-state
captures must be byte-identical for no-queue, ordinary non-key and protected-key
paths, including eight/sixteen-note inputs, program events and block partitions.
This is the historical integration gate, not a claim that the later intentional
active-extension program correction preserves every old baseline byte. Its
changed valid-program cases need their own corrected reference; the frozen
contract is retained rather than silently relaxed.
Modern supported-key trajectories are a new behavior and are covered by the
unchanged scalar/partition native gates instead of baseline byte equality.

Direct state witnesses for active-extension multi-recall boundaries supplement,
but do not replace, real VST3 protocol tests. The accepted native legacy MIDI
controller event input is a JUCE-supported path, not a claim about every DAW.

Qualification reports must name the exact source and module hashes. Native
macOS VST3 compilation and AU, actual DAW use, UI/release-wide acceptance and automation CPU budgets
remain separate scopes. No claim of measured Yamaha hardware parity is added.

### Reproduce the main-specific witnesses

With plugin tests enabled on Linux, `cs80_main_vst3_multirecall_state_tests` is a
registered CTest and links the same SharedCode target as the plugin. Its frozen
contract checks both active engines after all five Legacy-to-modern recall
pairs, one-time queue cancellation, extension lifetime and empty-queue controls.

Build `cs80_vst3_main_compatibility_capture`, then run
`python tests/CompareVst3MainCompatibility.py --capture CAPTURE_EXECUTABLE
--baseline CLEAN_MAIN_MODULE --candidate CANDIDATE_MODULE
--contract tests/parity/tolerances-v2/main-vst3-compatibility.json --output OUTPUT_DIR`.
The baseline must be independently built from the exact commit in that contract;
the comparison tool does not infer provenance from its filename. It rejects the
same file supplied twice, validates every complete capture record and EOF, and
requires full byte identity. The binary capture format is the Linux little-endian
fixture format, not a preset or interchange format.
