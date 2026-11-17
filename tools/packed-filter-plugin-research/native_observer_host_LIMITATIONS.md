# Pinned official host-service control: not admitted

`OfficialHostContext` deliberately requires a `DefectControlOnly` construction tag and exposes `admissibleForObserver=false`. It is a research control over the unchanged official SDK helpers, not a usable qualification-host promise. No real synth/plugin was loaded by these tests.

## Verified upstream behavior

Source: JUCE revision `c352e2489000ffeb9e397051377d75eef2efe641`, SDK `public.sdk/source/vst/hosting/hostclasses.cpp`, SHA256 `f9fbcd410d09ea3352342fdcb645e6a9dd1a424ff5b42430287fd4885a063c1d`. All28 source/header closure files were independently SHA256-matched to the authentic preflight dependency manifest from run44123289466. No SDK implementation was patched or copied into this repository.

The Attribute move-assignment at lines159–168 replaces its union/type/size without releasing prior string/binary storage. Setters assign into an existing map entry (string line273, binary line298, corresponding scalar setters). Its destructor at lines170–174 deletes binary storage only when `size` is nonzero. This additionally leaves a `new char[0]` allocation unfreed.

The inert fixture overrides new/delete using allocation headers and snapshots outstanding bytes/blocks before constructing an attribute list and after destroying it. Controls and reproductions are separate processes:

| Case | Outstanding after destruction |
| --- | --- |
| Integer→integer | 0 bytes, 0 blocks |
| One string write | 0 bytes, 0 blocks |
| One nonempty binary write | 0 bytes, 0 blocks |
| UTF16 `abc`→same string, or string→integer | 8 bytes, 1 block |
| 5-byte binary→2-byte binary, or binary→float | 5 bytes, 1 block |
| One zero-length binary write | 0 bytes, 1 block |

The reproducer returning success means it reproduced the expected defect, not that the host passed admission. It does not depend on unavailable LeakSanitizer.

Minimal semantic reproduction: create `HostAttributeList::make()`, call `setString("key", u"abc")` twice, destroy the list, compare allocation counters. The exact executable fixture also verifies controls, type replacement and empty binary. Reproduce all15 official-semantic/control cases with:

`CS80_OBSERVER_SDK=/tmp/cs80-observer-sdk python -m unittest discover -s tools/packed-filter-plugin-research -p test_native_observer_host.py -q`

The test compiles our code with warnings-as-errors. The separate unchanged SDK helper TU uses an explicit `-Wno-error=format` exception because pinned `funknown.cpp:488` passes int32* to sscanf `%2x`; its warning is retained, not suppressed. This is a disclosed test-build exception, not a production compiler-flag change.

## Lifetime and concurrency constraints

Official HostApplication addRef/release return constant1. The C++ owner must outlive every module/instance reference and callback; a successful query does not extend its lifetime. Official attribute maps are unsynchronized. Raw binary pointers belong to the list and must not be used after replacement/destruction. These constraints are not solved by the thin control wrapper.

## Next design, not yet implemented

Do not ban overwrites or silently patch/vendor the SDK: real plugin messages may overwrite values. A separately reviewed bounded host implementation should preserve observable SDK semantics while owning typed values safely, balancing interface refs, serializing calls, bounding resources and making allocation/limit failures terminal for the observer. It must pass overwrite/type-change/empty-binary allocation-accounting regressions and exact interface/lifetime tests before replacing this inadmissible control.
