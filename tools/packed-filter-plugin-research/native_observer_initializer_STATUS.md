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

## Phase-split, bounded history extension

`initializeConnected()` now stops after connection/initial component-state sync.
`captureConnected()` owns the phase check and a single metadata enumeration;
`connectedParameterRoster()` exposes readonly declared count and ordered unique,
non-sentinel IDs gathered from those same ParameterInfo calls, not a second pass.
Partial metadata on failure is not admitted metadata. `prepareAndStart(config)`
performs original topology/setup/activation. Optional second argument `true`
records setup latency/tail at the valid Setup checkpoint before activation;
default `start(config)` retains original native sequence and rejects invalid grid
before component creation. No parameter delivery policy is guessed here.

Private phase/config/history counters govern admission, not public compatibility
receipt booleans. One initial activation plus at most two suspend/resume pairs is
permitted. `saveState(policy)` is owned and once-only in initial Processing history;
first suspension must consume that same instance's exact stored state through
`restoreSavedState()` once before resume. Controller unsupported policy follows
the reviewed state helper. Second suspension permits only plain resume, not a
second restore. Saved data is not accepted from an arbitrary external instance.
A future fresh-instance E restore needs separately reviewed cross-instance
identity binding. Resumes keep original setup and do not call setupProcessing
again. Restore controls does not restore oscillator/RNG/delay history; A=C PCM
is not claimed. The suite uses matched histories across arms instead.

`withProcessingOperation(callback)` supplies the owned audio interface and const
stored setup only in private Processing phase; callback returns void. All callback
errors, including invalid flush inputs, become terminal. Nested lifecycle misuse
also becomes terminal even when the callback catches it: native cleanup is
postponed until that callback unwinds, then checked once. Borrowed typed getters
are phase-checked when obtained but cannot revoke a retained C++ reference. Do
not retain interfaces across callbacks/phase transitions or call concurrently.
No worker-thread/UI-thread, idle, or callback-quiescence claim is made: the real
driver must establish these boundaries independently before invoking methods.

Suspension consumes stop/active/bus ownership before each call, attempts all
remaining stop/deactivate/bus-off operations despite errors, then terminally
cleans; no failed operation is retried. Native returns/exceptions are retained
in fixed calls, state receipts or NativeOwnership cleanup receipts. Primary
failure survives secondary cleanup errors. A caller-provided callback must keep
its own detailed process/descriptor receipt (as the flush helper does); this
wrapper is not a general process recorder or timeout boundary.

96 fixed initializer call slots have a conservative55 bound: largest separate
start31 + optional metadata1 + optional metrics1 + two suspend/resume pairs16 +
final stop/deactivate/bus-off4 =53; two temporary identity releases conservatively
add2 on partial paths. Earlier failure removes later calls; terminal guards forbid
new lifecycle work. State helpers have separate2/3-call receipts and final native
ownership uses its existing fixed15 slots. Every new failure/positive control
asserts the initializer count remains within55. No receipt scheduling allocation
is introduced; metadata/state storage may allocate and failures still clean up.

58 new phase controls plus the existing50 initializer controls pass, including
full combined/separate histories, same-enumeration roster, all selected suspend/
restore/resume return/exception failures, genuine std::bad_alloc injection during
metadata/callback/cleanup, spoofed public booleans, late/stale access and nested
misuse (including caught nested processing-wrapper rejection with live outer
processor access until unwind). The real flush template is instantiated against InjectedInstance with
inert process calls, including invalid/repeat failures. Nine representative new
phase paths passed ASan/UBSan with detect_leaks=0; this is not leak-sanitizer proof.
No production synth was loaded, no audio rendered, no driver launched.
