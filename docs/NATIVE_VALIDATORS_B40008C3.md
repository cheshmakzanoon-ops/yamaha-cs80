# Current native validator checkpoint

Published source `b40008c3d5caac409615d6c708340b90054f61c0` was rebuilt in the
existing Release Linux native configuration, with AVX2 filter batching disabled.
Its production source is unchanged from `863e6c5b`; intervening changes are
checkpoint documentation and screenshots. Build exit status was 0.

Two sequential single invocations tested the same VST3 artifact with unchanged
validator criteria and no retries:

- Unmodified official Steinberg VST 3.7.14 extensive validator (`-e`):
  **537 reported passes, 0 failures**, process exit 0.
- Adapted pluginval 1.0.4/JUCE 8.0.11 host: **strictness 10 SUCCESS**, process
  exit 0, random seed 12345 (`0x3039`), GUI tests disabled, in-process validation.

Each invocation had a 600-second outer timeout and completed normally. pluginval
used its existing 120000 ms internal timeout. No downloads, installations,
validator source changes or test criteria changes were needed.

## Artifact identities

| Artifact | SHA-256 |
| --- | --- |
| Current VST3 module | `c9a06f8a254c3d8c077e267bafd47a6949df9917aa1c1c48b019ce9c986f6950` |
| Official validator | `1f55157d5a89bc042b5a0a6ce20a21226e2600a576cd904ef453dce729a91ebf` |
| Adapted pluginval | `0f7d3ac8d9af430246a4f117173016fa2bfe4d6ba616917fb6d822c4ff9ad224` |

Pre/post-validation hashes match. Validator provenance and exact adaptations
remain those documented in [the official validator checkpoint](STEINBERG_VALIDATOR_39A94C48.md)
and [the adapted pluginval checkpoint](HEADLESS_PLUGINVAL_39A94C48.md). The former
uses the pinned unmodified SDK 3.7.14 checkout and submodules; the latter is not
a stock distributed pluginval executable. Complete console reports, explicit
exit records and source/build provenance were retained separately.

## Scope and exclusions

The official validator's total includes **260 unsupported 64-bit processing
messages**; it does not qualify double-precision audio. Unsupported speaker
arrangements are negotiated, not thereby supported by the plugin.

pluginval skips its embedded external-validator invocation because no path was
provided there; the official validator ran separately above. Its Linux `auval`
heading is not AU validation. GUI tests were explicitly omitted.

These are current-artifact protocol/host checks, not external DAW, physical MIDI
controller, GUI, AU, live MTS service/discovery, sanitizer, installer, hardware
fidelity or full CPU/deadline qualification. They do not establish universal
absence or explain the cause of historical intermittent offline-wrapper failures.
Any later production change requires its own applicable verification.
