# Factory-tone revision: processor exposure preregistration (2026-11-02)

Base: `2a8c726e4a055b7733dd8e314f334de4ac042c87`; whole `src` tree
`335930af02743a272b0da6ddd0544024794ea296`. This already contains the explicit
core `FactoryToneRevision` API, but the processor cannot opt into it. No source
qualification lock or original numerical/audio criterion is changed here.

## Fixed compatibility contract

Append one **non-automatable** APVTS choice, `factory_tone_revision`, at index
297, AU version hint 17, labels `Frozen V1` / `Public-source estimates V2`, default
index 0. Choice 0 selects core revision 1; choice 1 selects core revision 2.
All previous IDs, indices, normalized default float bits, automation flags and
AU hints remain unchanged. `FactoryToneParameterPrefix.tsv` records all 297
actual existing processor parameters (not wrapper-added MIDI controls); its SHA256
is `a7238174d2be6806b2387fc3fcae9dbfcd09afe9637d13f742a2bdb681d76c8f`.
It was generated from unchanged b91 processor source on Linux, with JUCE 8.0.11,
before this addition. Cross-platform default-bit results must be inspected; a
platform arithmetic discrepancy is not permission to alter existing defaults.

This choice is sound state, independent of ENGINE, PURE and source modelVersion.
Sessions and user presets restore it; missing or malformed values always restore
V1, even into an opted-in instance. Only complete finite numeric 0/1 values are
literal. Existing state schema 3 and its ENGINE migration remain unchanged.
Factory/INIT/MIDI program recall resets V1; hardware tab selection and randomise
preserve the choice. A/B captures, recalls and persists it, with V1 for old or
malformed inactive-slot values. UI affordance is a separate later change.

The APVTS raw atomic is resolved once. Each readParameters transaction takes one
snapshot; all main/extension/automation frame publications use that cached value.
No engine mutation occurs on a GUI/state thread, no RT lock/allocation is added,
and existing programmer-versus-aggregate ControlFrame construction is retained.

## Registered test-first witnesses

1. Exact 297-entry old prefix and appended choice type/index/default/labels/hint/
   non-automatable flag. Missing new parameter is an ordinary baseline failure.
2. Actual main and extension engines: exact 26-bus/provenance matches against the
   separately qualified V1/V2 compilers for two inferred source tabs, prepare,
   block updates, late extension activation and repeated-frame cache stability.
   This is an integration oracle, not independent physical CS-80 measurement.
3. A deterministic transaction witness changes the live choice after the actual
   readParameters call, then forces actual sample-automation frame publication.
   Both engines must retain the captured revision until the next transaction.
   Bounded concurrent atomic writes additionally test main/extension coherence.
4. Session/user-preset/missing/malformed migration; A/B sound and inactive-slot
   persistence; factory/INIT/MIDI recall; randomise/tab preservation.
5. Separate existing-defect witness: a MIDI Program Change inside an active
   sixteen-voice block must publish the same aggregate sound frame to both
   engines. Baseline code updates only the main engine. Diagnose the actual RED
   before proposing the bounded fix; no voice-mode lifecycle redesign is implied.

The focused `--factory-tone-revision-test` mode creates no editor, peer, display,
audio device or DAW. JUCE MessageManager initialization is required for the real
processor AsyncUpdater. This processor test does not replace native wrapper,
UI, pluginval, real-host or full composed-source numerical qualification.
