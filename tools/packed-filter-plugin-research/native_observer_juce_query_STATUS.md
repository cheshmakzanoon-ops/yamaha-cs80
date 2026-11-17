# Pinned JUCE optional-controller query compatibility

This is research-host compatibility, not a synth change or a general VST3 error
relaxation. The strict SDK default remains unchanged.

## Observed blocker and source diagnosis

The first actual historical stock diagnostic (source commit
`0c351b9c509e2d3e8ae1ff978fbc0a3a739974e3`, ELF SHA256
`baec79cbe2c8667b92312c858d40fb9d308ca5433bbf337b01c1257a4ded8d35`)
exited24 at `instance-history`, with `controller query failed`. It completed in
0.456 seconds, with one native open, no native close, no A-D receipt and an empty
owned process group after exit. Its original stderr does NOT record the raw
instance-query result. The source diagnosis below is therefore not a claim that
its exact return code was observed in that attempt. No retry repairs that result.

The pinned official JUCE revision `c352e2489000ffeb9e397051377d75eef2efe641`
provides a separate controller. Its component's `queryInterfaceInternal` does not
list IEditController and returns a default `InterfaceResultWithDeferredAddRef`.
The default `QueryInterfaceResult` stores `kResultFalse`; `extract` sets the output
pointer null for that code and returns it. On Linux this is1, whereas the SDK's
FUnknown queryInterface documentation specifies kNoInterface, which is-1.
The unmodified host accepted only kResultOk+nonnull or kNoInterface+null.
The isolated defect control reproduces normal1/null and the same rejection.

Pinned sources and SHA256:
- `modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp`:
  `16031325b31be2e32b23689f07338bd6c631941475b9bbf87d176d1f2565c202`
- `modules/juce_audio_processors_headless/format_types/juce_VST3Utilities.h`:
  `bafec6a07d7d0fd07f49f1e2c2747f6371d9a3c956bc7cee125f748ba4ac9d2e`
- SDK `pluginterfaces/base/funknown.h`:
  `e0d9609224fe15491c9ccd1463d964c303f1d5a6149fabf849e87f2c14be1951`

## Narrow preselection

`ControllerQueryPolicy::StrictSdk` is1 and remains the default. Policy2,
`PinnedJuceFalseNull`, adds ONLY a normal kResultFalse+null outcome to the absent
combined-IEditController branch. It does not make unsupported queries successful.
Separate CID capture, factory class match, creation, identity distinction,
initialization, component-state synchronization, metadata and connection checks
remain mandatory. Error+nonnull, success+null, other error codes and exceptions
remain rejected. The policy is private const in the instance; evidence is exposed
as a copy, never a writable policy. Controller serialization policy is separate.

Request-v1 implies strict1. Request-v2 adds u64 query policy after the controller
state policy and before timeout. The policy must be selected by trusted input
before module load, never inferred from a failed query.

The standalone driver requests child-receipt-v2 for BOTH strict and compatibility
runs. Immediately after the case blob, before the native-close-success field,
v2 adds u32 query policy, u32 exact raw result (two's-complement for negative
SDK codes), and u32 pointer-present0/1. The values come from the actual initialized
instance and are covered by the existing receipt body hash. The supervisor must
independently expect the policy, raw result and pointer outcome. Existing inert
fixture entrypoints default to receipt-v1 and cannot select compatibility without
explicit v2 recording. The A-D payload itself remains unchanged; the full child
receipt binds query evidence to that payload and its module/input identities.

Failure stderr now includes query_policy, query_attempted, query_normal,
query_result and query_pointer. These are bounded diagnostics, not admission.
Failure/quarantine paths retain the graph and never retry cleanup.

No CPU timing, real compatibility success or production qualification is claimed
by the implementation or inert controls. New diagnostic runs require separate
preselection and their own fresh immutable evidence.

## Second diagnostic, still strict

A fresh, separately recorded strict-policy diagnostic using the v2 observer
confirmed the source diagnosis at runtime: query_attempted1, query_normal1,
query_result1, query_pointer0, query_policy1. It exited24 at the same gate in
0.285 seconds, with unchanged input hashes, an empty process group after wait,
and an empty receipt. This is a failed diagnostic, not an admitted A-D result.
No compatibility success is implied. The original first attempt remains rejected.
