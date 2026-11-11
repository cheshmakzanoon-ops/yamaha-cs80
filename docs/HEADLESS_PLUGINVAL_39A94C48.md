# Headless pluginval qualification with an adapted host

## Result

The current Linux VST3 completed both **strictness level 5 and level 10** with
**`SUCCESS` and process exit 0**, seed 12345 (`0x3039`), one repeat, using the
custom host combination described below. There was no retry or weakened test.

The level 10 log includes audio/non-releasing processing, state/restoration,
automation, parameter/thread-safety and parameter-fuzz phases, plus program and
bus checks. Audio/automation exercised 44.1/48/96 kHz and 64/128/256/512/1024-frame
blocks. GUI tests were explicitly disabled. The external Steinberg validator was
explicitly skipped because no validator path was supplied; the `auval` heading
on Linux is not an AU qualification result.

## Exact host and plugin

This is **not the stock pluginval release binary** or the historical CI validator.
It is pluginval 1.0.4 source rebuilt with pinned JUCE 8.0.11
(`c352e2489000ffeb9e397051377d75eef2efe641`) in Release:

- Retained source archive SHA-256: `f0d14c14743c694350d3bf9fbb06e752bb445f4d7cd3703520acc6758caa1c68`.
- All 44 original archive files were verified unchanged before adaptation.
- An isolated copy replaces `formatManager.addDefaultFormats()` with the documented
  `juce::addDefaultFormatsToManager(formatManager)` in `CommandLine.cpp`,
  `MainComponent.cpp` and `PluginTests.cpp`, because JUCE 8.0.11 deleted the old API.
- `PLUGINVAL_FETCH_JUCE=OFF` uses the explicitly added pinned JUCE checkout.
- LADSPA hosting is disabled because its development header was unavailable.
  VST3 hosting and validator test bodies are unchanged. No plugin source is altered
  for validation. The adapted build completed 29 steps.

Validator executable SHA-256:
`0f7d3ac8d9af430246a4f117173016fa2bfe4d6ba616917fb6d822c4ff9ad224`

Plugin source: `39a94c4812a8b94912f761f0c2a622a9b3bc58ec`; module SHA-256:
`3373cdf366458bc17f4e2d3da00370543949fdadb3b16c3cd24ff99e86cc7400`

## Invocation and limits

For each level, the adapted executable was invoked with:

```sh
pluginval --strictness-level LEVEL --random-seed 12345 --skip-gui-tests \
  --validate-in-process --timeout-ms 120000 --output-dir REPORTS \
  --output-filename REPORT_NAME --validate MODULE_BUNDLE
```

An outer 600-second timeout bounded each invocation; neither timeout fired.
Complete logs and exit status were retained locally. The unmodified host initially
failed compilation on the removed JUCE API; the intermediate adapted host then
failed on the missing LADSPA header. Those were host-build failures, not plugin
validation failures, and are not concealed as passes.

This establishes the named adapted host's headless compatibility result, not
stock-host equivalence, GUI/event-loop qualification, external DAW compatibility,
AU/Steinberg external validation, live MTS/MIDI behavior, sanitizers, installer
qualification, CPU budget or hardware fidelity. The blocked display-driven gates
remain unresolved. Local artifact hashes do not establish reproducible builds or
release signing.
