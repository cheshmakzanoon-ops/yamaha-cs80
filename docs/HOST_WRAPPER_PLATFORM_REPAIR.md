# Host automation wrapper platform repair

Date: 2026-10-31. This is build-system and source-integrity evidence, not a
hardware measurement or a native macOS runtime qualification.

## Observed failure and cause

The macOS arm64 Configure step failed at commit
`6458c5fc0e5b16d1a0cb04f6ecf43769b27e4cbc` in
[build run 41235843552, job 124639336716](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41235843552/job/124639336716):

```
CMake Error at cmake/HostAutomation.cmake:38 (message):
  JUCE VST3 wrapper source not found; sample offsets would be lost
```

This occurs before an AU build or pluginval test. The same Configure failure
appeared in runs 41235524334 and 41234615664. The source-selection code accepted
only `juce_audio_plugin_client_VST3.cpp`. JUCE supplies
`juce_audio_plugin_client_VST3.mm` on macOS; that Objective-C++ translation unit
includes the sibling C++ implementation. Its extension is required to retain
the Objective-C++ compilation mode.

The reviewed upstream source is JUCE 8.0.11, commit
`c352e2489000ffeb9e397051377d75eef2efe641`. The patcher checks these SHA-256
digests after UTF-8 decoding and newline normalization:

| Source | SHA-256 |
| --- | --- |
| VST3 `.cpp` implementation | `adf5c18774c42bfab7d50478525f84dcab43e12ff8d9879749137d80c61336f0` |
| VST3 `.mm` trampoline | `2f074fef9a5940c0f0e7aef15ca9ae024e44bf613ca1c6b828842b9282ebd532` |
| AU `_AU_1.mm` implementation | `a4a8f4efb3b20977a1dfa8865c02800834e8790043975428b28358417d1315f5` |

## Repair

`cmake/HostAutomation.cmake` now recognizes the reviewed C++ and Objective-C++
VST3 source names. It replaces exactly one wrapper translation unit per present
target, preserves its extension, and retains all unrelated interface sources in
their original order. Missing or duplicate wrapper sources fail configuration.
For the macOS trampoline, both the trampoline and its implementation are
configure dependencies.

`scripts/patch-juce-automation.py` verifies the selected source hash. When the
source is the macOS VST3 trampoline, it separately verifies the sibling C++
implementation and emits the patched implementation into the generated `.mm`
file. This keeps the existing sample-offset bridge in the actual compiled
translation unit. The original JUCE source files are never modified.

The existing VST3 C++ and AU generated files are byte-identical before and after
this repair. This change does not alter the automation algorithm, parameter
layout, event timeline, or DSP implementation.

## Registered contract and local qualification

The contract in
[`tests/parity/tolerances-v2/host-wrapper-platforms.json`](../tests/parity/tolerances-v2/host-wrapper-platforms.json)
was committed and pushed as `cf52fef87c2e230dcc0c688121abea96ccafac4d` before
implementation or evaluation. It permits zero failures.

`tests/HostWrapperPlatformTests.py` uses real copies of the pinned JUCE files and
small CMake interface-target fixtures with `LANGUAGES NONE`. The original
implementation reproduces the macOS Configure failure after 30 checks. The
repair passes all 92 checks, covering C++ and Objective-C++ selection, AU,
LF/CRLF equivalence, filename preservation, bridge insertion, unchanged
upstream files and unrelated sources, repeated configuration, dependency
tracking, and rejection of missing, duplicate, or unreviewed wrappers.

The test is registered as `cs80_host_wrapper_platform_tests` in plugin test
builds. The first candidate fixture run exposed CMake line-wrapping in the
expected error diagnostic. The fixture now compares that diagnostic after
whitespace normalization; the required error words and failure status are
unchanged. The initial diagnostic log is retained with the other evidence.

All four preregistered mutations were applied to isolated temporary copies and
were rejected by the same fixture:

| Deliberate defect | Observed rejection |
| --- | --- |
| Omit `.mm` discovery | Valid macOS source list fails configuration |
| Emit the pristine `.cpp` include instead of the bridge | Compiled translation unit lacks the bridge include |
| Disable source-hash verification | Changed trampoline no longer fails closed |
| Drop unrelated interface sources | Source-list order and preservation assertion fails |

Each mutation exited with the fixture's assertion failure, rather than an
unrelated execution error. The production candidate files remained unchanged.
Logs, exact mutation patches, source/output hashes, and the CI error excerpt are
under
[`docs/evidence/continuation-2026-10-31/host-wrapper-platforms`](evidence/continuation-2026-10-31/host-wrapper-platforms/qualification.json).

## Qualification boundary

The local fixture proves wrapper discovery, generated content, and rejection
behavior without requiring an Apple SDK. Native macOS arm64/x86_64 compilation,
AU and VST3 host automation tests, and pluginval remain required CI gates. A
passing Linux source-list fixture does not establish those native results.

## Native AU macro compatibility follow-up

The repair published as `c8bf1b3dc6210a8cc76b901bced1f7ec35310995` passed native
macOS arm64 configuration in
[run 41237400418, job 124643945990](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41237400418/job/124643945990).
The AU compile then exposed a separate failure: JUCE's
`juce_IncludeModuleHeaders.h` defines `Point` as `juce::Point` on macOS. That macro
rewrites the private `Point` declaration and uses in `AuAutomationQueue.h`,
producing a qualified-name/template collision. The existing portable AU bridge
fixture did not include this native preprocessor environment.

The follow-up renames that private type to `AutomationPoint` at its three uses.
Replacing the new spelling with the old spelling reproduces the original
header byte for byte; fields, arithmetic, event ordering, and public interfaces
are unchanged. JUCE's macros are preserved.

The separate contract in
[`au-wrapper-macro-environment.json`](../tests/parity/tolerances-v2/au-wrapper-macro-environment.json)
was published as `111c3ed37491f818ccbf5f96534cf7c464de6051` before evaluation.
`AuWrapperMacroEnvironmentTests.cpp` compiles the production queue with JUCE's
exact `Component` and macOS `Point` macro definitions active, then checks that
both aliases remain intact. The original header fails with the same `Point`
collision. The repaired fixture compiles and exits successfully; the isolated
mutation reverting the private name to `Point` fails compilation as required.

The existing AU curve and bridge contract tests also compile and pass with
their unchanged registered tolerances. The curve's reported maximum normalized
error is `2.22045e-16`. Exact compiler logs, the mutation patch, candidate hashes,
and the native diagnostic excerpt are recorded in
[`au-wrapper-macro-environment/qualification.json`](evidence/continuation-2026-10-31/au-wrapper-macro-environment/qualification.json).
Native macOS compilation and runtime qualification are still required after
this follow-up; the portable macro fixture does not substitute for them.
