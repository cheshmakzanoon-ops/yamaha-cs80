# Custom factory-tone revision editor results (2026-11-02)

Test-only0159cb04114cb9da21916c560f9e54b00cf54e09, source0f8a26e733644ac9f5985164e0175d4143c5d917, ran against matching fresh root5b26 project objects/headers and pinned JUCE/fonts, with a newly compiled component driver. At17:35:45 UTC it produced the exact prospective **5 checks/1 ordinary failure**: the custom editor has no unique revision ComboBox. Existing host metadata, Settings-page access, numeric-state preservation and unchanged Engine publication all pass. All widget-dependent assertions are explicitly NOT RUN, not passed. Zero desktop components. Raw commands/hashes are in `factory-tone-editor-20261102/original-absence-baseline`; no native window/input/render/DAW acceptance is inferred.

The additive fresh-constructor fixture at3338af07ec24a9acb84176a4fe20f93fa4ea7cf0 was replayed with a newly compiled driver against the same unchanged production source. It again produced exactly5/1 at17:42:35 UTC, with only the missing-selector assertion failing and later controls explicitly NOT RUN. Its receipt is retained separately; the initial test-only baseline is not rewritten.

## First implementation run: preserved fixture-precondition failure

Candidate1eb4d9924db1f0ef159cac4c784895c0d9324a81/source871d58acf578a849bc198f1a1ba69be47f8b62a8 changes only production AdvancedPanel.cpp. It executed all54 registered assertions at17:54:09 UTC with **54/1**, so this run is NOT qualified. Only the case labelled accepted MIDI Program Change failed. A diagnostic-only fresh test object retained all54 assertions and printed the actual pre-event state: **PURE1, SOURCE1, effective policy2 (isolation), revision1 (V2), selector ID2**. It reproduced the same54/1. The event is intentionally ignored in effective PURE; this is not evidence of a broken editor binding.

The fixture helper configure() uses applyParameterValues(), whose Processor.cpp599 deliberately preserves live PURE. Earlier policy/session/preset tests leave PURE on. Agent5 independently reviewed this cause and approved an explicit fixture precondition: set PURE off and assert actual sourcePerformance before accepted MIDI, then assert actual isolation before ignored MIDI. All original54 assertions remain; the two added preconditions give56, with prospective positive56/0. No production processor or editor behavior changes for this correction.


## Corrected positive and actual-source controls

The corrected fixture59522333955aea4801204ae37c57f40257ac2a08 executed **56/0** at18:02:26 UTC. Production remains1eb4d9924db1f0ef159cac4c784895c0d9324a81, whole-source tree871d58acf578a849bc198f1a1ba69be47f8b62a8; the original54/1 is not relabelled. The new test object was freshly compiled. The unchanged candidate AdvancedPanel object and the24 other byte-matched root project objects were reused, alongside19 verified pinned JUCE/font objects. The receipt enumerates the45 objects/libraries actually linked (26 project including the test driver,19 JUCE/fonts), rather than counting superseded reference objects as linked. Positive binary SHA256: `96198fef5b3ff41a3a98a7b9c57782c3fd5ee543582c046f536afc107e5f2ca1`.

Five independently compiled, isolated AdvancedPanel.cpp counterfactuals exercised every56 assertions. Each acquired/released the shared heavy-check lock separately. The exact candidate source and positive linked objects/binary were hash-checked before and after every execution; production was never edited to run a mutant. The mutated helper, original/replacement bytes, compile/link argv, actual linked-object hashes, logs and binary identities are recorded in `factory-tone-editor-20261102/compiled-controls` (full mutant source and binaries remain in the matching workspace evidence directory).

| Actual source control | Checks | Ordinary failures | Required observed mechanism |
|---|---:|---:|---|
| Wrong MIX attachment |56|17|Real revision write/gesture and corresponding readback/publication failures only|
| One-way binding |56|9|External/session/preset/A-B and corresponding choice readbacks; user selection remains functional|
| Immediate UI-thread audio publication |56|2|Exactly both deferred-publication assertions|
| Keyboard focus removed |56|1|Exactly accessible title/description/focus assertion|
| Stored-ENGINE-only constructor gate |56|2|Exactly the two fresh source-off policy combinations, including PURE override|

The two variable-total controls are checked against their prospectively fixed required/allowed assertion families;17 and9 are the recorded totals, not counts inferred before execution. The other three match exact predeclared failure multisets. No missing-widget fallback, skipped later tests, compile failure, runtime error or timeout counted as a successful negative. Unchanged positive-before and positive-after both executed56/0; final positive completed18:14:46 UTC with the identical binary SHA above. Zero desktop components in every run. Independent final replay review is still pending at this checkpoint.

These are real custom component construction, callback binding, metadata, geometry and processor-transaction checks without a peer. They do not establish native mouse/keyboard operation, screen-reader API behavior, painted text legibility, screenshot/golden acceptance, plugin-wrapper lifecycle or DAW behavior. The unchanged assertions are registered in the ordinary native panel suite for later exact-composed-source execution. No parameter IDs/defaults, processor persistence, DSP, golden image or comparison threshold is changed by this editor increment.
