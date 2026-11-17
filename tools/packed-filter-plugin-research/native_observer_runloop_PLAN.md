# Linux runloop design: no implementation or real-module execution

## Verified pinned behavior

Inspected exact JUCE commit `c352e2489000ffeb9e397051377d75eef2efe641`.
Every source below was downloaded separately and SHA256-verified against the
existing authentic preflight dependency receipt. Source hashes accompany this
note in `native_observer_runloop_sources.json`. No SDK or plugin source changed.

* `pluginterfaces/gui/iplugview.h`, Linux interfaces: IRunLoop is provided by
  IPlugFrame and/or IPlugFactory3::setHostContext context, including no-editor
  hosts. Readable fd handlers and repeatedly invoked millisecond timers are the
  contract. It does NOT specify a queue-empty/barrier API, detailed duplicate
  registration policy, ownership refcounts, or comprehensive cross-thread rules.
* `juce_audio_plugin_client_VST3.cpp:159-331`: EventHandler attaches all known fds
  to ONE selected host loop. One handler registers MANY fds. Unregister removes
  every fd for that handler; changing known fds causes wholesale unregister then
  re-register. `onFDIsSet` adopts the calling thread as JUCE message thread and
  invokes the fd callback. Attachment stops JUCE's fallback background thread.
* `...VST3.cpp:777-807,855,2673,4111-4134`: factory setHostContext queries the
  loop; factory createInstance has a temporary ScopedRunLoop; component and
  controller each retain a scoped loop. Passing only IHostApplication does not
  establish a host-driven message loop. The same loop object must be supplied
  consistently through factory context and any future frame.
* `juce_LinuxMessageThread.h:46-127`: fallback starts its own message thread;
  stop joins it. EventHandler destruction may restart it. Therefore no promise
  that the entire load/unload lifetime is free of JUCE background dispatch.
* `juce_Messaging_linux.cpp:38-110,183-210`: the message socket callback drains
  `while (popNextMessage)` and can invoke recursively posted messages. One host
  fd callback is potentially unbounded. Posting/queues use their own locks.
  Callback selection is copied under lock and invoked outside that lock.

## Proposed host contract (requires separate implementation/review)

1. A single dedicated, identity-recorded UI/host thread performs factory and
   instance lifecycle, state, controller and metadata calls, all fd/timer callback
   dispatch, and final teardown. Own a separately refcounted IRunLoop object that
   survives any context-owner loss; expose it only after implementation tests.
   Do NOT change the existing bounded host's advertised interfaces yet.
2. Keep the loop valid before setHostContext and through all instance/controller
   releases, factory host-context removal/release and ModuleExit. Retain handlers
   per successful registration and retain callback snapshots until return. Own
   registrations, not the plugin's fds: never read, close or alter fd flags.
   Failed registration must be sticky host failure because JUCE's adapter ignores
   these return codes. Bound registrations, receipt entries and retained refs.
3. Model fd entries as `(handler identity, fd, registration generation, ordinal)`.
   Permit one handler with many fds, and remove ALL its entries on unregister.
   Freeze duplicate-pair policy explicitly (proposed idempotent success without
   extra ref). Inert tests must cover full-set replacement during callbacks.
   Poll readability without consuming bytes. Snapshot readiness and dispatch in
   frozen ordinal order; revalidate generation before each invocation so a
   removed/replaced entry is not called from stale readiness. POLLNVAL is failure;
   freeze HUP/ERR policy, never spin silently. Fd-number reuse outside registration
   discipline is not detectable reliably and is a plugin ownership assumption.
4. Use a monotonic real clock for timer intervals, checked conversion/addition,
   bounded maximum intervals and no zero-interval busy loop. Deterministic due
   ordering is `(deadline, registration ordinal)`. Proposed missed-deadline policy:
   invoke once, advance from prior deadline by whole periods to first future due
   time, record lateness/skipped periods, no unbounded catch-up. This policy must
   be frozen and tested; it is NOT proof of equal wall-time timer interleaving
   across arms. A virtual clock would not virtualize JUCE's independent timers
   and must not be substituted silently.
5. Invoke no external SDK callback/addRef/release while holding registry locks.
   Reentrant self/other unregister is safe through generation tombstones and
   owned snapshots; no callback nesting in the host dispatcher. Off-thread
   register/unregister is a design gate, not solved by pretending SDK forbids it.
   JUCE's fd-change listener can be called by the thread changing an fd. Start
   with an explicitly diagnosed UI-only qualification policy only if observed
   usage/source proof permits it; otherwise implement a reviewed concurrent
   mutation protocol. A blocking worker unregister can deadlock against a UI
   callback waiting on that worker; a nonblocking one cannot promise in-flight
   callback completion. Do not advertise universal thread semantics without
   resolving that distinction.
6. Processing uses a separate identified worker, never the host UI dispatcher.
   State transitions and host-side snapshots occur only after worker rendezvous.
   Freeze exactly when UI pumping is allowed between workload phases or blocks;
   no unscheduled sleeps or opportunistic per-arm pumping. Do not run synchronous
   control/state calls concurrently with audio. Whether pumping must continue
   during a render to satisfy plugin behavior is a separate protocol decision.
   It can change callback order and cannot be inferred from CPU kernel evidence.

## Bounded pumping and honest barrier terminology

`pump(phase, absolute_deadline, max_callbacks, max_rounds)` may poll and dispatch,
recording ordered registration generations, fd readiness, timer events, callback
entries/exits, handler notifications, and stop reason. Idle means only that the
last readiness sample had no eligible work. Poll with no events twice, sleeping,
or several unchanged hashes does NOT prove the plugin cannot enqueue tomorrow.
Host timers remaining scheduled mean future work is expected.

A host-owned FIFO sentinel proves only preceding host commands completed, not
that JUCE/private producer queues are empty. IRunLoop has no API for injecting a
marker into those queues. A genuine protocol barrier needs identified producers
stopped/joined or a plugin-supported acknowledgment with defined scope. Neither
is presently established for all JUCE/plugin background producers. Store
`snapshot_idle`, not `globally_quiescent`, and never admit full notification
identity solely on a quiet-time heuristic. Cross-arm equality requires the same
phase/event protocol and explicit treatment of asynchronous notifications, not
just byte comparison after arbitrary waits.

Host budgets cannot interrupt a callback that fails to return; the pinned JUCE
socket callback can itself drain indefinitely. A parent process watchdog must
bound the whole child and mark timeout terminal/no-retry, preserving partial
logs. This is necessary even with callback-count and poll deadlines.

## Shutdown and tests before any real module

Stop new render commands; finish/join worker; complete only protocol-authorized
UI pumping; perform checked instance stop/deactivate/bus/disconnect/terminate
and interface releases on UI thread while loop remains valid. Observe registrations
removed; do not forcibly erase live plugin registrations and call that success.
Clear/release factory context with checked lifecycle evidence, then release
factory and exit module while services remain alive. No callback dispatch into
module code after ModuleExit/dlclose. Outstanding handler refs/registrations at
unload are failure and prohibit unloading beneath callable pointers. Fallback
thread restart during wrapper teardown remains an explicit source-backed risk
requiring complete verified shutdown, not a host counter inference.

Inert tests required: pipe readability without host reads; one-handler/many-fd;
full unregister/re-register; duplicate pairs; fd reuse generations; self-unregister
and remove-other during snapshot dispatch; handler ref balance after owner loss;
timer ordering/overflow/missed deadlines; sustained-ready fd budget exhaustion;
invalid fd; thread identity errors; concurrent mutation policy if implemented;
non-returning callback caught by subprocess watchdog; receipts for partial failure;
shutdown refuses registrations/in-flight callbacks. No production plugin load,
new loader CLI, or callback-quiescence claim is authorized by this design note.
