# Real-handle inert module + host + A-D history join

This is a dedicated qualification **child-process** owner, not a reusable general
plugin host. The tests compile actual inert SDK-fake shared libraries and exercise
canonical-path verified/pinned load, real exports/ModuleEntry/factory, native
instance initialization, all four A-D processing stages on the actual worker,
checked instance closure, factory releases, ModuleExit, and guarded dlclose.
No CS80 module, production workflow or performance experiment is launched. E and
reload-transfer minting remain absent.

## Private ownership and host interfaces

The heap graph owns pin, guarded SealedLoader, bounded host/services, dispatcher,
InjectedModule, Handler and HistoryOwner together. No mutable lifetime object or
dispatcher escapes. The advertised wrapper implements IHostApplication and Linux
IRunLoop, with one canonical FUnknown identity through IHostApplication. Both
views retain the wrapper, not the private dispatcher. Loop registration methods
forward to the checked UI dispatcher; host message/attribute creation delegates
to the bounded implementation. The complete graph shell is allocated before load.
Only one active graph is allowed per child; its private process-wide lease is
released only after fully successful teardown (or destruction before any run).

Instance history joins the processing worker and closes native instances first.
The host and loop remain alive and advertised through factory releases and
ModuleExit, including host service queries and new registrations inside ModuleExit.
The actual dlclose gate is reached only afterward. It requires:

- Privately completed/admitted A-D instance history and joined worker.
- Actual recorded successful factory/factory3 releases and successful ModuleExit,
  not merely empty callback counts. Failure/exception cannot be erased by cleanup.
- Exact pre-load versus post-exit task inventory (TID plus kernel start time).
- Zero registered callbacks, healthy dispatcher and successful checked shutdown.
- Exactly two wrapper references: child-owner baseline plus module-owned host query.
- Healthy bounded host context with no outstanding messages/attributes/bytes/keys.

Only then can explicit SealedLoader.close run. Module-owned host release occurs
AFTER dlclose; the final gate requires baseline1 and healthy empty services/loop.
Failure there is reported as a post-unload failure, not falsely as no unload.

## Unsafe failure means quarantine and nonzero child exit

The joined loader explicitly disables all automatic dlclose paths, including
underlying post-dlopen failures and sealed proof/resolve failure/destruction.
Default standalone loader policy is unchanged. Any joined-owner failure preserves
its first exception and releases ownership of the **entire graph intact**, without
native destructors or unload. Bounded diagnostic state includes attempted boundary,
module call receipts, references/registrations and native open/close counts.
Provider-absence confirmation is separate from admission. An atomic process-wide
terminal latch prevents retry/new-owner construction after a caught failure.
The trusted executable may inspect bounded evidence, then MUST terminate nonzero;
there is no catch-and-resume or cleanup-success claim. The tests assert outcomes
outside expected-failure catches and use `_Exit(23)` for quarantined children.

Destruction on the wrong thread or during an active callback terminates before
any graph teardown; a dedicated child sentinel verifies no unsafe unload.

Wrong-thread/nested run requests only atomically latch failure; they never touch
the graph or perform cleanup from that thread/callback. The owning UI observes the
latch at concrete history boundaries (even if no pump occurred), joins/cleans the
instance as possible, then quarantines. Read-only evidence/admission accessors are
UI-idle checked. Retrying a previously successful owner invalidates admission.

## Conservative task inventory, not universal thread quiescence

Before load, and once after ModuleExit immediately before dlclose, the owner reads
bounded `/proc/self/task` entries and bounded complete stat records, retaining
TID/start-time pairs. Exact equality is mandatory. Missing/disappearing/unreadable/
malformed input fails closed; there is no polling until a favorable inventory.
This catches an actual leaked unregistered thread in the inert fixture, but is
NOT proof against future threads, shared pools, signal callbacks, malicious code,
or a thread racing between checks. Registration/refcount checks likewise do not
prove arbitrary plugin producer quiescence. Trusted process/loader state, full
runtime closure, code-memory integrity and the external watchdog remain gates.
Plugin allocations are not bounded merely because observer/service structures
are bounded. Worker deadlines do not cover blocking UI/native lifecycle calls.

## Verification scope

26 actual-library integration controls pass. Positives use the frozen legacy
48k/realtime/8-voice/64-block A-D case, not all108 native cases. Negative cases
include retained timers/host/services, registrations during ModuleExit, failed or
throwing exit, failed factory release, partial factory/component construction,
post-unload host misuse, caught nested/foreign calls, constructor-created unsafe
threads plus failures at both open-proof layers/resolve, leaked unregistered exit
thread, malformed/missing task inventory, overlap/retry refusal and a NODELETE
library remaining mapped after nominal dlclose. Every unsafe-preunload control
asserts zero actual dlclose, and logs show no unload before child termination.

The positive fake library uses `-fno-gnu-unique` to permit ordinary unloading; a
separate `-z nodelete` variant deliberately proves retained mappings are rejected.
These test build flags do not change production build flags. Successful receipts
bind only this inert execution and do not by themselves establish production
sound equivalence, module safety or authorization to run the real synth.

A private constructor now supports the separately preselected terminal-child
process-lifetime profile. It is accessible only to `ProcessLifetimeChild`, whose
success and failure paths exit without destroying the ownership graph. Existing
public owners remain strict-unmap. See `native_observer_process_child_STATUS.md`;
this does not grant E/state-transfer authority or relax native lifecycle gates.
