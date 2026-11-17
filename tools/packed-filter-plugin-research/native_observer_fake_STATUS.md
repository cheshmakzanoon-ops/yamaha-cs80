# SDK adapter fake-runtime follow-up

This follow-up exercises the phase-separated support/adapter revision. It compiles and links one in-process fake SDK program using exactly the17 pinned headers already recorded in `native_observer_sdk_provenance.json`. `INIT_CLASS_IID` supplies header-defined interface identifiers. No SDK source library, synth core, plugin, module loader or real `dlclose` is invoked. The fixture defines its own `dlclose` symbol as an inert status-returning stub.

33 optional, explicitly selected runtime cases pass with `CS80_OBSERVER_SDK=/tmp/cs80-observer-sdk`. Without a selected SDK, the fake-runtime test class reports a clear skip and provides no runtime evidence. With it selected, absent/mismatched headers or compiler failure is an error, not a skip.

Coverage:
- Real adapter Stream methods: binary round trip/seek/tell, invalid-write sticky failure, interface query IDs.
- Real adapter Handler methods: complete callback records, restart flags, encoding, nonfinite callback rejection and concurrent callback serialization.
- Real adapter captureState/captureMetadata against fake SDK interfaces: complete embedded-zero state, rejection of a fake success that hides stream failure, metadata output, nonempty class/parameter sets and default/current normalized range rejection.
- Real NativeOwnership against fake interfaces: exact15-operation close order, return receipts, independent interface reference releases, partial initialization, uninitialized release without termination, combined component/controller ownership, failure in stop/deactivate/disconnect/terminate/ModuleExit/dlclose, caught native C++ exception, and retry prohibition. All remaining cleanup executes after the simulated failures.

Combined-object assumption tested: component/audio/controller/connection queries each contribute an owned reference, but the shared underlying plugin-base object is initialized/terminated only once. Setting both initialization flags on the combined fake deliberately triggers a second terminate error; this negative case passes by rejecting the close. A future real initializer must establish combined identity through the correct SDK identity rules, not guess from interface pointer equality. The helper cannot independently prove the caller's reference ownership flags.

No new adapter defect was found by these cases. They do not establish real native-module behavior, correct real initialization/restoration, bus activation/deactivation, audio processing, deterministic DSP histories, resource identity, complete SDK interoperability, or the108-case observer. A fake method is intentionally simpler than JUCE/plugin implementations. Real driver linking/lifecycle remains unimplemented and unlaunched.

Phase-separated API coverage adds a correct Created→Connected→Setup sequence, separate and combined identity caches, rejection of a cache from another live component, helper-declared wrong phases, and independent fake guards that reject late getControllerClassId or early latency/tail even when the caller lies about phase. The fake sequence is not the full initializer and does not establish host-context/loader provenance.
