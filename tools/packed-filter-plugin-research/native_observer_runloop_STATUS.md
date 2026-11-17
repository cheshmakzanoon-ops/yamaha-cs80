# Standalone UI-only runloop preparation

No bounded-host interface advertisement, real module, plugin CLI, or integration
has been added. `Dispatcher` independently implements pinned Linux::IRunLoop.
The broader design and verified wrapper analysis remain in runloop_PLAN.md.

## Implemented scope

The constructing UI thread alone may register/unregister, pump or shut down.
Wrong-thread calls fail before registry mutation and latch atomic health failure.
QI/refcounts support concurrent valid references; final release on the wrong
thread, or while registrations/dispatch exist, refuses deletion and retains the
last owner reference with terminal failure. The owner must later clean up on UI
and release it. This refusal is leak-safe only with that explicit owner discipline;
it is NOT silent success. No destructor is used to erase registrations and claim
shutdown. Diagnostic receipt/registration getters are UI-only.

The default dispatcher owns its monotonic clock. A separate test-only injection
constructor borrows a Clock, which must outlive all dispatcher references.
Each registry has 256 fixed slots, each with monotonically increasing generation.
One handler can own many fd entries; unregister removes ALL of that handler's
entries. Duplicate fd/handler pair is idempotent. Timer same-handler/same-interval
is idempotent; changed interval is invalid until explicit unregister. Timer zero
is rejected. Multiplication, initial deadline and skipped-period advancement are
checked for uint64 overflow. These are explicit host policies, not universal SDK
semantics.

Readiness uses nonblocking poll(POLLIN). Host never reads, closes, or changes fd
flags. ERR/HUP/NVAL and poll errors (including EINTR) are terminal. Snapshots sort
fds by registration generation, then timers by due-time/generation. Each snapshot
retains handler references and revalidates slot generation immediately before
calling it. Unregister/re-register during callbacks cannot dispatch stale entries.
References are released even after callback error, exhausted deadline or partial
snapshot-acquisition failure. Handler SDK ref methods run outside registry
mutation and reentrant registry calls from those ref methods are rejected.
Returned zero/throwing addRef is treated as acquisition failure; no slot is
published. As with any foreign ABI, a malicious ref method that changes hidden
ownership THEN throws cannot be unwound reliably; this is terminal, not proven
successful cleanup of arbitrary broken SDK implementations.

Timers fire once per sampled round, then advance from prior due time to the first
future period; skipped periods are recorded. Wall-clock-dependent timer order
across arms is NOT claimed deterministic. Callback dispatch is UI-only, no nested
pump. Pump limits include absolute monotonic deadline, callback count, round count
and fixed 4,096-entry receipt capacity. Reaching a limit before an idle sample
fails closed. Idle means sampled idle only; registered future timers may remain.
No queue barrier or global callback quiescence is inferred. The callback itself
can hang; only an external process watchdog can stop that. Never retry a failed
real study on the strength of these fixture controls.

Shutdown accepts only healthy/no registrations/no active callback boundary on UI;
it records shutdown and closes further mutation. Health remains sticky. Failed
shutdown does not erase live registrations. Manual valid unregistration remains
available after failure so cleanup can finish without changing the failed result.

## Evidence

40 optional SDK-dependent tests: 39 returning inert cases plus a deliberately
hanging callback killed/reaped by Python subprocess timeout. The returning cases
use own pipes, a real monotonic timer, deterministic injected clocks for overflow
and ordering, fake handler reference counters, exception/zero ref acquisitions,
reentrant and cross-thread misuse, stale snapshots, callback exceptions, budget
failures, EOF/invalid fd, receipt exhaustion and explicit shutdown/refusal.
All handler references balance in each returning case. No plugin is loaded.

```
CS80_OBSERVER_SDK=/tmp/cs80-observer-sdk python -m unittest discover \
  -s tools/packed-filter-plugin-research -p test_native_observer_runloop.py -q
```

With no selected SDK the suite reports an explicit skip. Strict fixture compile:
`g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread`, no warning exception.
A separate build adds `-fsanitize=undefined -fno-sanitize-recover=all` and runs all
39 returning cases. This is not a thread sanitizer proof or real host validation.

Before integration: independently verify actual JUCE fd-listener registration
thread usage. The SDK does not forbid off-UI registration; if the real wrapper
requires it, this policy must fail, then be extended under review rather than
silently relaxed. Freeze processing-thread separation, UI pump phase boundaries,
async notification treatment, shutdown producers and fallback-thread lifecycle.
No completion of the whole observer message/quiescence contract is claimed here.
