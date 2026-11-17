# Processing worker design (design approved; implementation requires review)

New standalone prerequisite, no native plugin integration. Future HistoryOwner
privately owns worker plus native module/instance and invokes fixed A-D operations.
Generic callable completion alone is never evidence that a render/flush protocol
occurred. Actual helper receipts must be validated by that owner.

A real dedicated std::thread performs submitted Render/Flush callables. UI owner
alone submits, waits with optional bounded event pumping, and explicitly closes.
submit is queue-only; await validates sequence/deadline/budgets before authorizing
actual worker dispatch. One outstanding operation includes completed-but-not-consumed work. A task has
monotonic sequence, named kind, actual worker thread identity, attempted/returned
status, completion timestamp and exception_ptr, recorded only at dispatch boundary.
No public acceptCompletion(bool), writable receipt, supplied thread result, or
caller's shutdown boolean. Failed/reentrant/wrong-thread use is terminal.

Mutex/CV protect state. No task/pump callable or task construction/destruction is
invoked under the mutex. UI callable allocation occurs before queue publication,
inside checked submission so OOM latches failure. Stored callable must not own a
last native module/instance reference requiring UI destruction; its worker-side
destruction is part of the operation boundary. Worker startup records real thread
identity; construction failure creates no valid receipt. Construct the worker
before acquiring native plugin resources where practical.

Wait takes exact expected sequence, absolute steady-clock deadline, finite pump
step count and positive bounded CV wake quantum. Check state/deadline, invoke one
explicit bounded pump step on UI without mutex, check again, then predicate-based
CV wait until completion or next pump/deadline boundary. No polling sleep or quiet
interval certifies quiescence. Pump callbacks can still block: the process watchdog
is authoritative for any unreturning task/pump. Wait deadline failures do not imply
worker stopped. Caller must join before touching native lifetime.

Explicit close signals stop, cancels queued-but-not-started work, and joins active
work without detach. An active callback can prevent return indefinitely; external
subprocess timeout is the only supported forced recovery. Destructor stops/joins
as safety fallback, not acceptance. Destruction on worker thread is fatal misuse,
not permission to detach/unload. Even failure cleanup requires verified actual
join before native objects are released on UI.

JoinReceipt is immutable/private-construction and instance-bound, created only
following actual std::thread::join. A failed job may still yield joined-safe
cleanup evidence but cannot yield healthy admission. Healthy admission additionally
requires every submitted task returned, was consumed once with matching sequence,
no cancellation, no terminal failure, and explicit (not destructor) close. Future
HistoryOwner must consult its OWN worker/receipt, not accept an arbitrary external
worker's successful token. Its token mint also requires its actual A-D/lifecycle
and module seal evidence.

Critical integration blocker: current withProcessingOperation catches failure and
closes the instance immediately on the calling thread. Using it directly from this
worker would perform UI-only teardown on the processing thread. Integration must
first provide reviewed deferred failure cleanup: retain terminal error, complete/
join worker, then UI performs native cleanup. No current wrappers are changed here.

Tests planned: actual distinct thread and sequence; Render/Flush names; one slot;
completed-unconsumed rejection; callback and pump exception capture; terminal no
retry; submission OOM and startup allocation failure; wrong UI thread/reentrancy;
wait deadline and late completion; bounded pump ordering; queued cancellation;
explicit joined-failure cleanup vs healthy admission; cross-worker receipt
rejection; child watchdog for hung callback and destructor safety (never detach).
