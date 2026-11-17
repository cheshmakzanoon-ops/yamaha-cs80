# Fixed A-D instance/worker history: inert integration only

HistoryOwner privately owns the concrete ProcessingInstanceOwner and therefore
its native instance, dedicated processing worker and actual task/join receipts.
It borrows an already-loaded InjectedModule and Handler, which MUST outlive it.
No arbitrary success boolean, caller-created observation or task receipt can be
submitted. The only run method executes this fixed sequence once:

1. Validate frozen case/state policy; initialize Connected, capture native metadata.
2. Derive workload_bridge plan from that owned complete roster; prepare with setup metrics.
3. Complete canonical initial-control flush, then render A.
4. Capture the owned **post-A** component/controller snapshot.
5. Flush exactly the bridge's two-ID/two-value alter subset, then render B.
6. Suspend, restore that same owned post-A snapshot once, resume; render C.
7. Suspend and resume without another restore/setup; render D.
8. Actually join the worker and perform checked native-instance close.

The post-A snapshot is NOT fresh initial DSP state. Restore restores controls,
not oscillator/RNG/delay history. Never assert A=C, A=D or reset equivalence.
Across arms compare matched histories A↔A, B↔B, C↔C, D↔D only. Each render starts
projectTimeSamples at zero under the already frozen segment-relative protocol;
no continuous timeline or new MIDI/transport convention is invented here.

## Authority and observations

Six processing operations must produce the exact private owner's sequences1..6
and kinds Flush,Render,Flush,Render,Render,Render, on its one worker thread. The
owner is private and there is no receipt injection API: matching thread IDs is
not used as a substitute for owner correspondence. Successful close also requires
actual joined submitted/attempted/completed/consumed counts all6, canceled0, and
the owner's authoritative successful closure. E remains unreachable.

Read-only evidence retains exact frozen case, initial/alter/render canonical
bytes, native metadata/roster/setup metrics, two flush receipts, four distinct
render observations and encodings, post-A state, owned restore call receipts,
six task receipts, join and cleanup receipts. Last completed stage and attempted
operation remain available on failure. First process/encoding/allocation error
is preserved separately from secondary cleanup error. Completed earlier stage
observations survive failure, and a completed raw render can survive a later
encoding failure without being admitted. The current lower-level failed process
helper may not return partial PCM/callback buffers; no such evidence is invented.

Admission additionally requires final canonical payload encoding and successful
allocation of a private shared immutable HistoryObservation. Allocation/encoding
failure even AFTER clean native close prevents admission. Exposed observation
access is UI-idle/const; no reference to mutable owner/instance/processor escapes.
A later rejected retry invalidates current admission but does not mutate the
already frozen prior observation. There is no public final-record constructor
accepted as execution authority.

Canonical payload uses existing little-endian Bytes, text header
`cs80-instance-history-A-D-v1`, then length-prefixed case/initial/alter/render,
metadata/setup, encoded initial/alter flush, encoded post-A state, four encoded
renders, six sequence/kind pairs (Flush=1, Render=2), three owned restore-call
attempt flags/result/blob records, lifecycle name/returned-value pairs, interface
close name/returned-value pairs, and five joined counts. Signed returns use their
u64 two's-complement representation. Complete runtime TaskReceipts remain in the
immutable observation; nondeterministic thread/time identities are not converted
into canonical bytes. This is an observer artifact format, not a signature or
proof from an untrusted producer. No independent history decoder exists yet.

## Boundaries still outstanding

The same absolute worker wait deadline is reused for all six processing tasks;
it is NOT a whole-history wall-time bound. UI metadata/state/lifecycle calls,
plan construction, copies and final encoding may block or exceed it. A global
external process watchdog/resource budget remains mandatory. Caller-supplied UI
pump must be the separately owned reviewed host/runloop implementation; sampled
idle alone is not callback/producer quiescence. No callbacks are synthesized here.

Successful A-D admission proves only this instance/worker sequence. It DOES NOT
prove ModuleExit/dlclose, sealed loader identity, runloop producer shutdown,
complete loaded runtime closure, fresh E reload/transfer, production test suite,
pluginval, sound equality or CPU improvement. No reload token is minted. No real
CS80 module is loaded or workflow launched by this code/tests.

21 compiled inert SDK integration controls pass using actual owner/worker helpers,
full generated parameter roster and reversed native enumeration. Positive runtime
coverage is legacy48k/realtime/8voices/block64; this is NOT all108 native grid cases.
Negatives cover each process stage, state/setup/metadata/suspend/restore/resume /
close, invalid case before creation, expired worker deadline, retry, post-A
allocation failure, final encoding allocation after clean close, and combined
process+cleanup errors. Assertions after expected-failure catches check that the
requested injection occurred and later render stages did not execute. Fakes enforce
native processing off the UI thread and cleanup on UI after process completion.

### Joined-owner cancellation boundary

The additive three-argument run overload accepts a cancellation-only boundary
check; the original two-argument API uses a no-op check. The private module owner
supplies its atomic misuse/terminal latch. It cannot attest stage success. Checks
surround concrete owner operations, snapshots and final freeze, including when a
worker finishes before any UI pump. An already-active owner operation may finish;
this does not promise interruption between individual SDK calls inside it. No
boundary callback runs during required failure cleanup. One additional control
(22 total) cancels after a completed initial flush, retains its actual task/flush
receipt, and proves no render runs even though the pump itself does not cancel.
