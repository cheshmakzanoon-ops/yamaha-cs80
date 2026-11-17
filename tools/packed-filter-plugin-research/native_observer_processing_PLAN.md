# Actual instance/processing-worker lease design

Design approved; implementation requires independent review. New owner, no real plugin or loader CLI.
Worker's approved six files stay unchanged. Sealed-loader work is separately owned.

ProcessingInstanceOwner privately owns an InjectedInstance, ProcessingWorker and
private frozen lease/results. Module and Handler must outlive owner. No raw instance
or processor accessor is exposed. A narrow forward declaration/friend declaration
in InjectedInstance permits owner to set/release its existing operation guard;
a private default-false deferFailureCleanup_ flag prevents automatic failure close
only for this owner. Existing synchronous wrapper semantics stay unchanged.

UI initialize/capture/prepare/save/suspend/restore/resume use guarded forwarding.
Actual render/flush entry guards the owner across protocol copy/construction,
queue, await and UI pump. Begin calls private phaseGuard(Processing), sets
instance.operation_, and copies setup and processor pointer into lease state.
Worker never reads or writes instance phase, primaryError or lifecycle flags.
It runs actual renderObserved or actual flushObserved via a private frozen lease
adapter, and writes only operation-owned results. Protocol/setup equality is
checked before native process. UI reads results only after worker publication.

Success requires this owner's actual task receipt with expected sequence/kind,
returned result and consumed completion. Only then UI releases the instance guard.
Other lifecycle phases can proceed while idle worker waits for the next operation.
Owner misuse in a pump marks terminal but does not release guard, clear outer
operation, close instance, or invalidate active worker pointer. Off-UI owner calls
use an atomic violation latch and throw without reading/writing instance state.
No caller can supply an acceptance, completion or shutdown boolean.

On any failure, including submission allocation failure, canceled unattempted
queue, native process exception/return failure, deadline or caught nested misuse:
join this actual worker first. Obtain and validate its instance-bound cleanup join
receipt (healthy admission is separate). UI then clears guard, records instance
failure and runs checked native cleanup. Retain original failure and results.
Never use the existing synchronous withProcessingOperation directly on worker:
its catch path would tear down native interfaces on the wrong thread.

Normal close also joins before native cleanup. Destructor joins before clearing
guard/instance destruction, never detaches, and cannot issue admission. Wrong-thread
or self destruction is fatal ownership misuse rather than permission to unload.
A hung callback/join requires external subprocess watchdog. Member declaration
order must make worker destruction precede instance destruction even during C++
constructor unwinding; no partially constructed worker may call a missing owner.

Real-thread + SDK fake tests must exercise actual renderObserved and flushObserved,
with fake process recording dedicated worker identity and fake lifecycle/cleanup
asserting UI identity. Include partial initialization, render/flush protocol error,
native throw/return failure, submission OOM, canceled predispatch, nested pump
close/suspend even caught, wrong-thread calls, deadline/late completion, failed
join/admission distinction, destructor lifetime, hang watchdog and synchronous
initializer regression suite. No actual native module is loaded in these tests.
