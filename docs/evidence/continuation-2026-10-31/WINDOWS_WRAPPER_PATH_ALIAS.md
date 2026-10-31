# Native Windows wrapper-path identity

2026-10-31. This corrects a test-fixture path comparison. It changes no plugin
source, generated wrapper, audio, parameter or numerical limit.

## Observed mechanism

The native Windows build at `c8bf1b3` compiled successfully, then
`cs80_host_wrapper_platform_tests` stopped at check 8: the generated wrapper's
path did not compare lexically equal to the expected temporary-directory path.
The failure message did not expose either path, so path aliasing was initially
only a hypothesis.

Diagnostics-only commit `b5901b2f846468038da5330ea9ce7271b8a7a8ef`
retained every assertion and added a separate early Windows source-contract
job. The job checks out pinned JUCE
`c352e2489000ffeb9e397051377d75eef2efe641` and runs the complete existing
`LANGUAGES NONE` fixture, including source-hash checks. Ordinary native Windows
CTest coverage remains unchanged.

[Run 41239902650, job 124651460860](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41239902650/job/124651460860)
provided the exact identity evidence before the same assertion failed:

| Observation | Native result |
| --- | --- |
| Actual parent spelling | `C:\Users\runneradmin\AppData\Local\Temp\...` |
| Expected parent spelling | `C:\Users\RUNNER~1\AppData\Local\Temp\...` |
| Both basenames | `juce_audio_plugin_client_VST3.cpp` |
| Resolved absolute paths | Identical long `runneradmin` path |
| `samefile()` | `true` |
| Original lexical assertion | Fails at check 8 |

The complete common suffix and both resolved paths are preserved in
[native-before-identity.json](windows-wrapper-path-alias/native-before-identity.json).
The Windows short-name alias and the long directory name identify the same
generated file. The failure did not establish an incorrect wrapper or language
extension. The diagnostic log proves that distinction directly.

## Registered correction

Contract commit `abcf9ea8b5038c7308a3d0cd1806d738960518b3` published
[windows-wrapper-path-alias.json](../../../tests/parity/tolerances-v2/windows-wrapper-path-alias.json)
before candidate implementation or evaluation. The single existing assertion
now requires all three conditions:

```python
wrapper.name == original_name
and wrapper.resolve() == expected_wrapper.resolve()
and wrapper.samefile(expected_wrapper)
```

This preserves the exact native basename and extension, the canonical absolute
target and actual filesystem identity. It accepts the observed OS alias while
rejecting a differently named or different generated file. All **92 checks**,
zero allowed failures, source hashes, bridge checks, original-file integrity,
source ordering, configure dependencies, invalid-source rejection and LF/CRLF
comparisons remain intact. Raw and resolved path diagnostics remain enabled.

## Qualification and mutations

The full local fixture reports **92 checks, zero failures**. Both registered
mutations were applied only to temporary copies of `HostAutomation.cmake`:

| Mutation | Observed identity | Result |
| --- | --- | --- |
| Redirect VST3 to its existing original JUCE source while still generating the reviewed wrapper | Correct basename; different canonical path; `samefile=false` | Fails the same assertion at check 8; exit 1 |
| Generate/select `.mm` for the native `.cpp` case | Existing generated file with the wrong basename/extension | Fails the same assertion at check 8; exit 1 |
| Restored production generation and corrected fixture | Exact basename, canonical target and file identity | 92 checks, zero failures; exit 0 |

The local runs exercise every registered C++/Objective-C++ source-list shape
against the pinned upstream wrappers. **Native Windows qualification of the
corrected predicate remains pending its published CI run.** A Linux pass alone
does not establish the Windows result; the earlier native run establishes the
alias mechanism, not a successful correction.

The raw diagnostic/full job log, identity JSON, local/restored logs, mutation
patches and results, and source hashes are archived under
[windows-wrapper-path-alias/](windows-wrapper-path-alias/). No tracked production
CMake or core source was modified by this correction.
