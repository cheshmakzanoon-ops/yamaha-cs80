# Dedicated processing worker: inert preparation only

ProcessingWorker owns a real std::thread. No plugin, native loader, existing
InjectedInstance wrapper or host runloop is integrated. Future HistoryOwner must
privately own the worker and actual render/flush operations and validate their
separate receipts. Generic lambda success is not proof an A-D history happened.

## Execution and receipts

The constructing UI thread alone may submit, await or explicitly close. A private
operation guard spans callable construction and every pump callback, preventing
nested misuse from stopping/freeing the outer operation. Callbacks execute on the
actual worker thread recorded in the task receipt. Task kind is Render or Flush;
sequences are monotonic, one outstanding slot only. **submit only queues**.
`await(expectedSequence, policy, pump)` validates ownership, sequence, deadline
and budgets before authorizing dispatch. Invalid/expired waits never invoke work.
Close-before-await cancels queued work without an attempt; it is not admission.

State and completion publication timestamps are recorded under mutex. Task,
pump and callable construction/destruction run outside it. Captured closures are
destroyed on the worker when executed or canceled: they must not own the final
native object reference or perform UI teardown there. Errors retain exception_ptr
and terminal first-failure state. New submission after error is prohibited.

Wait uses a condition-variable predicate, an absolute steady-clock completion
deadline, finite pump count and wake quantum strictly above zero and at most one
second. The pump callable is an explicit UI hook, not an implicit sleep. Its own
bounded event dispatcher remains the caller's obligation. A completion published
after deadline fails even if first observed already complete. A completion
published before deadline may be consumed after a slow UI pump returns; observation
delay does not rewrite completion time. However once timeout was observed and
latched, later success cannot clear it. This deadline is NOT a guarantee that an
unreturning pump/task will return to the caller; subprocess watchdog is mandatory.
Sampled idle in any future pump does not establish callback quiescence.

TaskReceipt and JoinReceipt constructors/state are private; no caller success or
shutdown flags are accepted. Receipts bind to a monotonically allocated instance
identity, not reusable object address. Task receipts identify exact kind, sequence,
actual worker thread, publication time, return status and retained exception.
JoinReceipt exists only after actual thread::join. `authorizesCleanup` checks the
owner's joined receipt identity; `authorizesAdmission` additionally checks sticky
health, exact submitted/completed/consumed count and no canceled work. A failed
or canceled task can be safely joined for UI cleanup without qualifying success.
Cross-worker receipts are rejected. HistoryOwner must consult its OWN worker,
not accept a caller-supplied unrelated successful worker.

Explicit close joins outstanding execution without detach. Destruction also
joins as safety fallback but produces no success receipt; either may block forever
on a hung callback. Destruction on the processing thread is fatal invalid ownership,
never a justification to detach or unload underneath it. Startup synchronization
can also block on broken thread/OS progress. External process watchdog bounds the
whole child. It must mark a real timed-out study terminal, not retry it.

## Evidence and remaining integration gates

32 mandatory controls compile independently of SDK availability: 30 returning
real-thread cases and two subprocess watchdog cases for hung explicit close and
hung destructor. Tests cover distinct processing identity, queue-only ordering,
named sequences, slot rejection, wrong-thread/reentrant calls, startup allocation
failure, task OOM/copy exception, callback/pump exceptions, deadline publication
semantics, no retry, canceled cleanup-only receipt, cross-instance receipt rejection,
and foreign callable destructors acquiring the worker mutex to verify they run
outside the lock. Strict compilation uses C++20, pthread, Wall/Wextra/Werror/pedantic.
No fake thread factory or caller-supplied completion event establishes authority.

```
python -m unittest discover -s tools/packed-filter-plugin-research \
  -p test_native_observer_worker.py -q
```

This is not a thread-sanitizer or arbitrary scheduling proof. Actual native
processing integration remains blocked: current withProcessingOperation closes
an instance synchronously on failure, which would wrongly do UI teardown on this
worker. Add a separately reviewed deferred-cleanup boundary first. Freeze actual
UI pump scheduling, native phase/processing ownership and HistoryOwner's stage
validation before any native plugin execution or token minting.
