# Actual worker-to-instance lease: inert integration only

This owner now executes the EXISTING renderObserved and flushObserved helpers
against an actual InjectedInstance SDK processor interface through the real
ProcessingWorker. Tests substitute inert SDK objects and a fake loader; no native
shared library or synth is loaded. It is not the complete A-D HistoryOwner.

## Authority and lifetime

ProcessingInstanceOwner privately owns its instance, worker, lease/results and
completion/join receipts. No raw instance, processor or lease escapes. UI-only
lifecycle forwarding includes connected initialization/metadata/setup and
save/suspend/restore/resume. Public processing accepts protocol values, not a
caller callback or caller-provided shutdown/completion flag. Flush additionally
binds expected IDs to the owner's complete connected parameter roster.

UI begin establishes the private native operation guard and freezes processor and
setup into a private typed task. Worker executes only that task's frozen fields
and operation result storage. It never calls the synchronous native wrapper or
accesses UI phase, primaryError or cleanup flags. Rate/mode/block and flush roster
mismatches fail before process. UI reads worker results only after completion or
join synchronization. Successful kind/sequence/return correspondence and owner
health are checked before releasing the guard. Native lifecycle may then resume
on UI while the joined-not-yet-requested worker idles safely.

An outer owner guard spans argument copying, queue, await and UI pumping. Nested
close/suspend/render cannot clear that guard or free the processor. Wrong-thread
calls atomically record failure without reading/writing UI-owned native state.
The terminal latch is checked both before dispatch and after authoritative task
completion: catching a nested/wrong-thread exception cannot convert the operation
to success. The first violation reason, first exception and separate cleanup
exception/receipts are retained.

All failures route to actual worker close/join, including allocation failure,
queued cancellation, native throw/non-success, timeout and swallowed pump misuse.
Only the owner's genuine cleanup join receipt allows the UI native guard to be
cleared and checked native close performed. Failed work never becomes healthy
admission through successful cleanup. Fixed lifecycle/interface receipt snapshots
contain no live native pointers. A cleanup failure preserves the original work
exception plus the separate cleanup error.

InjectedInstance changes are only private friendship and a default-false
`deferFailureCleanup_` flag. Its fail path still immediately records Failed and
primaryError; only this owner enables deferred automatic close so it can join
first. Explicit UI close remains available. Default synchronous behavior and
withProcessingOperation are unchanged and independently regression-tested.

Constructor ordering creates no native instance before worker construction has
succeeded. The owner joins before destructor cleanup, and members order worker
before result/native destruction. Wrong-thread destruction or destruction inside
an active owner/pump operation terminates rather than returning with freed native
state. A failed/hung join cannot release the lease; destructor fails closed if the
lease remains. Never detach or unload under a live callback. External subprocess
watchdog is mandatory for hung callbacks/joins. Destructor cleanup is not an
admission path.

Module and Handler still have an explicit external lifetime obligation: they must
outlive this owner. Sealed module ownership and the complete HistoryOwner are
separate integration work. acceptedClosure reports this owner closed cleanly,
NOT that any full history/workload occurred. No asynchronous plugin producer or
message-loop quiescence claim follows from worker task completion.

## Tests and limitations

26 SDK-selected integration controls: 23 normally returning cases, one intentional
hung native callback bounded by subprocess watchdog, and two deliberately invalid
destructor cases that terminate with the fixture's sentinel exit. Returning cases
use real dedicated threads and actual observer flush/render helpers. Fake process
requires non-UI thread; lifecycle and interface cleanup require UI outside active
process. Cleanup callbacks query the owner's actual joined authority. Caught
nested misuse explicitly checks native references remain alive before allowing
the callback to exit, then confirms cleanup occurred afterward. Coverage includes
initialization/metadata/setup failures, actual task/startup OOM, canceled predispatch,
worker misuse, timeout, setup/roster mismatch and independent primary/cleanup errors.

Existing 108 initializer/phase controls also pass, including original synchronous
behavior. The new fixture copies the prior inert initializer fake scaffold into a
separate source, adding thread/lifetime assertions and process hooks without
changing the original fixture. No existing helper test gate is removed.

```
CS80_OBSERVER_SDK=/tmp/cs80-observer-sdk python -m unittest discover \
  -s tools/packed-filter-plugin-research -p test_native_observer_processing.py -q
```

SDK absence is an explicit skip, not a native evidence pass. Build flags remain
C++20, pthread, Wall/Wextra/Werror/pedantic. UBSan is supplementary, not TSan or a
proof across arbitrary scheduling. Actual plugin loading, host/runloop advertisement,
full A-D stages, cross-arm identity and CPU timing remain unapproved.
