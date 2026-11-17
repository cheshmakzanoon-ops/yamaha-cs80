# Injected initializer subset (not a production host)

`native_observer_initializer.hpp` defines typed LoaderOps only. There is no default dlopen/dlsym implementation, CLI, synth linkage or audio process call. Fake loader operations return inert tokens and callbacks. The test fixture never opens a native module.

Implemented bounded scope:
- Canonical absolute path and exact unique audio-class CID selection from at most32 cached classes; typed entry/factory/exit resolution; factory3 unsupported or supported host-context path; owned queried IHostApplication context, with normal/failure cleanup.
- Combined controller obtained by direct component query or separate controller created from the const cached Created-phase CID. The cached accessor rejects combined misuse. A factory-created controller alias is rejected via queried canonical FUnknown identities. Each query/create contributes one independently released reference. Failed initialize is released without terminate; combined initialization/termination occurs only once.
- Separately recorded successful connection directions and handler attachment. A failed second connect disconnects only the first. Initial component state is rewound and transferred to the controller with latched stream-error checks.
- Exact stereo synth topology: zero audio inputs, one stereo output, one event input, no event output; bus metadata, arrangement/readback and float32 support checked. Setup is limited to the declared3 rates,2 modes and3 block sizes. Fakes currently exercise48k/realtime/64, not the entire grid.
- Successful audio/event bus activations are individually owned. Explicit close attempts stop, deactivate, reverse-order successful bus deactivation, then checked connection/controller/component/reference cleanup. A failed kth activation deactivates only earlier successful buses. Errors do not prevent later cleanup attempts; original init error and cleanup receipts remain available.
- Module cannot unload while an instance remains registered. A premature close is terminal for clean admission even if later cleanup succeeds. The module/loader/host objects must nevertheless outlive borrowed instance references. Callers must not destroy their C++ owners in the wrong order.

`loadSucceeded` and `initializationSucceeded` distinguish successful entry into the initialized state from mere successful cleanup of a partial/empty object. `closedCleanly` alone is not initialization admission. No field declares production qualification.

## Verification

50 explicitly opted-in injected-loader/SDK-fake cases pass with `CS80_OBSERVER_SDK` pointing to the pinned SDK. Missing SDK reports no initializer runtime evidence; selected SDK hash/compiler failures are errors. An additional official `ivsthostapplication.h` header was fetched at the same pinned JUCE revision and SHA256-checked against authentic preflight run44123289466. Existing17 header identities are also required.

Tests cover combined/separate positive paths, factory3 positive/negative paths, class identity failures, each selected init failure, partial reference/termination counts, second-direction connection failure, kth bus activation, cleanup failures, alias rejection and unload/retry prohibition. They do not emulate JUCE behavior or prove complete SDK host interoperability.

## Remaining gates

The caller must supply a real conforming IHostApplication with the required message/attribute services; querying its interface does not prove those services work. Real loader/export resolution and bundle identity, factory/instance runtime provenance, complete metadata/parameter capture, separate state restoration, full deterministic controls/events/DSP-history reset, all108 cases, render/PCM capture, host resource/PDC observations and full release tests remain outstanding.

CreatedIdentity captures the pre-init controller CID but does not expose a raw failed getControllerClassId status receipt, only the checked helper failure; expanded per-call observation provenance still needs review. The current implementation assumes native query/create failure contracts leave null outputs; failure-with-nonnull output is not a validated ownership scenario. Malicious or memory-unsafe in-process plugins are outside this harness's protection. No real module may be loaded based on these fake tests alone.

Eight representative injected-fake paths also passed AddressSanitizer/UndefinedBehaviorSanitizer with leak detection explicitly disabled. LeakSanitizer cannot run under this environment's ptrace restriction; no leak-sanitizer pass is claimed.
