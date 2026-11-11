# Official Steinberg validator checkpoint

## Result

On 2026-11-11, the locally built, unmodified official Steinberg VST 3.7.14
validator completed both invocations against the Linux CS-80 Reference VST3:

- Default suite: **47 tests passed, 0 failed**, process exit 0.
- Extensive suite (`-e`): **537 tests passed, 0 failed**, process exit 0.
- An independent repeat of extensive mode also reported 537 passed, 0 failed,
  process exit 0, using the same executable and module.
- The validator's build-time self-test separately reported 43 tests, 0 failed.

These are the validator's reported totals, not counts of fully exercised audio
configurations. In particular, the extensive log contains 260 explicit
`64bit Audio Processing not supported` messages. Those paths return successfully
without qualifying double-precision audio. The plugin supports 32-bit processing.
Unsupported speaker layouts are negotiated by the validator; a successful
arrangement test does not mean every requested layout is supported.

This closes the previously unexecuted external Steinberg validator checkpoint for
this artifact, separately from the [adapted pluginval host checkpoint](HEADLESS_PLUGINVAL_39A94C48.md).
It does not replace future validation of changed artifacts.

## Provenance and build

Plugin production source: `39a94c4812a8b94912f761f0c2a622a9b3bc58ec`.
Subsequent local checkpoint commits only changed documentation. The tested module
is the same Release Linux VST3 used by the preceding headless checkpoint, with
AVX2 filter batching disabled.

Official SDK repository: <https://github.com/steinbergmedia/vst3sdk>.
Tag `v3.7.14_build_55`, commit `43b4e366ff84afc9b4247776acbf5e234683b77f`.
Recursive submodules were checked out at the tag's exact gitlinks before building:

| Submodule | Commit |
| --- | --- |
| base | `823de87a7aa5752c1eca9c33e2015f160fb8642e` |
| cmake | `0d646538203721ab9d036f9ef1d650de19302d84` |
| doc | `84978802d3362fa022a1cba9bd0ccf541d4a9e12` |
| pluginterfaces | `dd77488d3dc329c484b5dfb47af9383356e4c0cc` |
| public.sdk | `b8931a07a48c090e6c833bb36d65e4e878b8cc5f` |
| tutorials | `33b73dfbb87f3fde3bce8c0a10cae934dc66ad34` |
| vstgui4 | `b1e823550e23067c5adee67f05ac3ddaba7a25c9` |

No SDK source changes or validator test changes were made. CMake used Release,
Ninja and GNU 14.2.0, with `SMTG_ENABLE_VSTGUI_SUPPORT=OFF`,
`SMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF`, and
`SMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF`. The official build still defines its
validator target independently of optional hosting examples. Building target
`validator` completed 88 steps and its self-test. Missing optional JACK was not a
build failure.

SHA-256 identities:

| Artifact | SHA-256 |
| --- | --- |
| Official validator executable | `1f55157d5a89bc042b5a0a6ce20a21226e2600a576cd904ef453dce729a91ebf` |
| CS-80 Reference VST3 shared module | `3373cdf366458bc17f4e2d3da00370543949fdadb3b16c3cd24ff99e86cc7400` |

The plugin reports its own embedded VST SDK version as 3.8.0; the independently
built validator is 3.7.14. This is not a claim of using a 3.8 validator.

## Invocation and tested scope

With the plugin's native dependency directory on `LD_LIBRARY_PATH`:

```sh
validator "/path/to/CS-80 Reference.vst3"
validator -e "/path/to/CS-80 Reference.vst3"
```

Each invocation had a 600-second outer timeout and completed normally. The default
suite reports scanning classes, buses, parameters, MIDI mapping, units/programs,
state transitions, suspend/resume, processing on another thread, silence,
parameter flushing, variable block sizes, process formats, bus arrangements and
parameter-change accuracy. Extensive mode adds the validator's expanded parameter
and processing cases, including all 2378 parameters. Test criteria were unchanged.
The log also reports no snapshots in the bundle; this is informational, not a
rendered screenshot test.

This is actual external SDK-validator execution against the built module, not a
mocked adapter or direct processor fixture. It does **not** qualify GUI rendering,
physical mouse/keyboard interaction, an external DAW, AU, live MTS-ESP discovery,
64-bit audio, sanitizers, target CPU/deadline performance, installers or hardware.
No production code was changed to obtain this result.
