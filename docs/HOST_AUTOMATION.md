# VST3 key automation on current main

## Scope

The [portamento extension](VST3_PORTAMENTO_AUTOMATION.md) appends existing
`P_portamento` at lane35 after the unchanged35-lane prefix. Positive time
updates affect the existing live glide coefficient when the footswitch gate
is active; glide-mode selection, gate behavior and DSP laws are unchanged.

This candidate MOD WHEEL depth extension preserves host sample offsets for eighty-three VST3 parameters in
modern circuit policy: global volume, mix, brilliance, resonance, master tune
and detune; each rank's HP cutoff/resonance, LP cutoff/resonance, VCA level and
pulse width, both rank pan controls, original-panel SUB speed, SUB VCO/VCF/VCA depths and SUB touch speed/VCO/VCF amounts, compatibility-mode SUB speed, ring speed/amount/depth/attack/decay, chorus speed/depth, portamento time, master sustain, initial pitch-bend amount, LOW/HIGH keyboard level, LOW/HIGH keyboard brilliance, and both ranks’ INITIAL/AFTER TOUCH brilliance/level amounts and PWM speed/depth, plus both rank VCF LEVEL and VCA envelope SUSTAIN controls, plus both filter EG IL/AL targets and both ranks’ filter/VCA attack, decay and release times and both rank SINE/NOISE levels, plus global ribbon displacement, key release floor, tuning reference and MOD WHEEL > SUB/VCF depths. The [pan extension](VST3_PAN_AUTOMATION.md)
appends two lanes to the original eighteen without moving their indices. It does
not replace DSP smoothing or change a parameter law. The
[SUB extension](VST3_SUB_AUTOMATION.md) appends two further global lanes without
changing either historical capability contract. The
[VCF/VCA depth extension](VST3_SUB_DEPTH_AUTOMATION.md) appends the remaining two
continuous depths while preserving that 22-lane prefix.
Static falling-saw, rising-saw and square selection with those continuous queues
has a separate [bounded native gate](VST3_SUB_WAVEFORM_AUTOMATION.md); discrete
waveform selection itself remains block-rate.
The [touch-amount extension](VST3_SUB_TOUCH_AUTOMATION.md) appends three existing
global amounts after the unchanged 24-lane prefix; pressure-event routing and
DSP response laws are not altered.
The [compatibility-mode speed extension](VST3_SUB_LEGACY_SPEED_AUTOMATION.md)
appends the existing `P_subSpeed` control after that 27-lane prefix. It is
distinct from original-panel speed and does not change mode selection.
The [ring extension](VST3_RING_AUTOMATION.md) appends existing speed and amount
after the unchanged 28-lane prefix. The [ring-depth extension](VST3_RING_DEPTH_AUTOMATION.md)
appends depth at lane 30. The candidate [ring-time extension](VST3_RING_TIMES_AUTOMATION.md)
appends existing attack and decay at lanes 31/32 after the complete 31-lane
prefix. Times are latched at a fresh RING trigger, not applied as live envelope
reshaping. The candidate [chorus-control extension](VST3_CHORUS_AUTOMATION.md) appends
speed/depth at lanes 33/34. Its first active qualification scope is CHORUS
mode only; chorus on/off and chorus/tremolo selection remain block-rate.
This source capability statement is not a claim of published or verified
35-lane qualification; retained checkpoints keep their own pinned scope.

The candidate [master-sustain extension](VST3_MASTER_SUSTAIN_AUTOMATION.md) appends
existing global performance release at lane36. Modern control-frame updates
refresh live release timing using the existing max of rank release, key release
and gated master sustain times ten; this is not a note-off-only latch or a
new hardware potentiometer law. The preceding 36-lane prefix is unchanged.

The candidate [initial-bend extension](VST3_INITIAL_BEND_AUTOMATION.md) appends
`P_initBend` at lane37 without changing the earlier37 lanes. Its existing
smoothed amount multiplies the modern TouchCircuit transient live, rather
than latching at note onset; no transient or smoothing law is changed.

The candidate [LOW keyboard-level extension](VST3_KBD_LEVEL_LOW_AUTOMATION.md)
appends `P_kbdLevelLow` at lane38. Existing smoothing refreshes held keyboard
contour nodes and their direct or factory shared-bus LC receiver paths live;
no contour circuit law is changed. All preceding38 lanes remain fixed.

The candidate [HIGH keyboard-level extension](VST3_KBD_LEVEL_HIGH_AUTOMATION.md)
appends `P_kbdLevelHigh` at lane39, preserving all earlier39 lanes. Its
existing live smoothed contour path is unchanged; the bounded fixture uses
source-proven responsive MIDI62..77 through the unchanged note-base helper.

The candidate [keyboard-brilliance extension](VST3_KBD_BRILLIANCE_AUTOMATION.md)
appends existing LOW/HIGH levers at lanes40/41. Live smoothed KBC1 voltages
feed the existing separate HP/LP cutoff-control receiver paths, not the LC
level path. The complete preceding40-lane roster stays fixed.

A candidate [paired contour qualification](VST3_KBD_CONTOUR_COMBINED_AUTOMATION.md)
combines LEVEL and BRILLIANCE separately for LOW and HIGH on responsive
keyboard cohorts. This is an existing42-lane test-only gate, not another
capability append or a simultaneous-all-four qualification.

The candidate [INITIAL TOUCH extension](VST3_INITIAL_TOUCH_AUTOMATION.md) appends
both ranks’ brilliance/level amounts at lanes42..45. Existing smoothed amounts
remain live only for selected PANEL ranks; factory and MEMORY authority
continues masking their panel trajectories. No touch or DSP law is changed.

The candidate [AFTER TOUCH extension](VST3_AFTER_TOUCH_AUTOMATION.md) appends
both ranks’ brilliance/level amounts at lanes46..49. Existing live smoothed
amounts act on modern TouchCircuit pressure nodes; factory/MEMORY panel-queue
masking remains unchanged. The complete preceding46-lane prefix is preserved.

The candidate [PWM extension](VST3_PWM_AUTOMATION.md) appends both ranks’
speed/depth at lanes50..53. Existing smoothed controls drive continuous
phase and pulsewidth modulation; selected factory/MEMORY masking stays
unchanged. No oscillator or smoothing law changes.

The candidate [VCF LEVEL extension](VST3_VCF_LEVEL_AUTOMATION.md) appends
both rank filtered-path gains at lanes54/55. Existing live smoothing and
factory/MEMORY source authority remain unchanged; the separate sine path
is added after this gain. The complete preceding54-lane prefix is preserved.

The candidate [VCA SUSTAIN extension](VST3_VCA_SUSTAIN_AUTOMATION.md) appends
both rank envelope targets at lanes56/57. Modern live target updates retain
existing state and stage and follow the envelope decay law, not ordinary
rank gain smoothing. Source authority and the full56-lane prefix are preserved.

The candidate [filter IL extension](VST3_FILTER_IL_AUTOMATION.md) appends
both rank release targets at lanes58/59. Existing live filter-envelope target
selection preserves state rather than applying ordinary rank smoothing. The
full58-lane prefix and selected-source masking remain unchanged.

The candidate [filter AL extension](VST3_FILTER_AL_AUTOMATION.md) appends
both rank attack targets at lanes60/61. During Attack, a changed AL reanchors
the existing trajectory from current precise state without replacing the state
or changing stage; this is not ordinary smoothing. Full60 prefix is preserved.

The candidate [envelope ATTACK extension](VST3_ENVELOPE_ATTACK_AUTOMATION.md)
appends filter/VCA attack times at lanes62..65. Existing live timing updates
preserve state, stage, elapsed time and attack anchor, unlike changed AL target
reanchoring. The complete62-lane prefix and source authority remain unchanged.

The candidate [envelope DECAY extension](VST3_ENVELOPE_DECAY_AUTOMATION.md)
appends filter/VCA decay times at lanes66..69. Existing live timing updates
preserve state and stage; a bounded post-attack held window qualifies the
selected decay coefficients. Full66 prefix and source authority are unchanged.

The candidate [envelope RELEASE extension](VST3_ENVELOPE_RELEASE_AUTOMATION.md)
appends filter/VCA release times at lanes70..73. Existing live timing updates
retain state and respect the max of rank release, key release and gated master
sustain. Full70 prefix and source authority remain unchanged.

The candidate [SINE/NOISE extension](VST3_SINE_NOISE_AUTOMATION.md) appends
existing rank levels at lanes74..77. Noise enters before filters, while sine
is added after filtered-path gain; both retain their existing smoothing and
source masks. The complete74-lane prefix is unchanged.

The candidate [combined DECAY/RELEASE qualification](VST3_ENVELOPE_DECAY_RELEASE_COMBINED_AUTOMATION.md)
exercises eight existing timing queues with asymmetric rank sources. Matched
active comparators separate a held group effect from release-group tail effect;
this adds no lanes and makes no individual-lane attribution.

The candidate [RIBBON extension](VST3_RIBBON_AUTOMATION.md) appends existing
global displacement at lane78. Existing controller smoothing and held-voice
frequency-ratio routing remain unchanged; qualification is held-note only.
The complete78-lane prefix is preserved.

The candidate [RIBBON release-mode qualification](VST3_RIBBON_RELEASE_MODES_AUTOMATION.md)
compares within each fixed Sustain I/II mode after identical note histories.
This uses the existing79 lanes and frozen module; it adds no production logic
or live mode-switch qualification.

The candidate [KEY RELEASE extension](VST3_KEY_RELEASE_AUTOMATION.md) appends
global release floor at lane79. Existing live max-release timing and PURE
effective-zero policy are preserved; this is not ordinary rank smoothing or
a source-masked rank control. Full79 prefix remains unchanged.

The candidate [release-floor interaction qualification](VST3_RELEASE_FLOOR_INTERACTION.md)
checks a master-sustain trajectory below and across a fixed key-release floor.
Matched active histories isolate this max-rule interaction without new lanes,
production changes or dynamic rank-release attribution.

The candidate [TUNING REF extension](VST3_TUNING_REF_AUTOMATION.md) appends
global reference frequency at lane80, preserving existing smoothing, oscillator
scaling and PURE440 behavior. Full80 prefix and source authority are unchanged.

The candidate [MOD WHEEL depth extension](VST3_MOD_WHEEL_AUTOMATION.md) appends
existing SUB/VCF amounts at lanes81/82, preserving the exact81-lane prefix.
Stationary explicitly mapped CC1 supplies an audible driver; this is not a
sample-accurate CC1-event capability claim. Existing DSP/smoothing and all53
historical contracts remain unchanged.

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

Latest bounded Linux depth-extension evidence (not a rerun of all old suites):

| Gate | Verified scope/result |
| --- | --- |
| New depth capability | SUB81/VCF82; 72 full cases across three routes, two source layouts, two capacities, three rates and two modes |
| Scalar/partition reference | Blocks1/64/127/256; maximum observed error0; exact1024-frame initial holds |
| Frozen81 compatibility | 72 cases;432 actual byte-equal PCM pairs,864 captures |
| Total bounded execution | 12 native processes;1080candidate+576baseline=1656renders |
| Protected policies | Legacy final-point flatten equivalence, PURE inertness, and separate zero-CC1 amount-inertness |
| Limits | Linux VST3 only; prior CC1/tuning forward guards source-reviewed, not rerun; no DAW/GUI/AU/hardware/CPU qualification |


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
