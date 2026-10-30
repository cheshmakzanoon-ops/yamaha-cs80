# Independent key-pressure helper review

The independent `tests/KeyboardPressureAuditTests.cpp` passed **151 checks,0
failures**, both native GCC14.2 and ASan+UBSan. Exact hashes, commands/flags and
mutation counts are in [independent-audit.json](independent-audit.json).
This is a helper-level pass; it does not substitute for actual processor/editor
runtime tests or claim whole-host real-time safety.

Coverage uses the production JUCE-free headers directly:

- Actual accepted publication supplies queue/epoch/onset/channel/note identity.
  Zero pressure is distinct from note-off; nonfinite/invalid/cross-queue input
  rejects. Same-epoch restrikes keep distinct generations. Combined note/pressure
  FIFO order and sample positions are preserved.
- Old tokens reject before and after reset acknowledgment. Pressure flooding
  reaches the shared bounded capacity, latches overflow, and cannot reopen with
  an old token after acknowledgment. Onset generations are not reset/reused.
- Stack capture is noncopyable/nonmovable, top-only, expected-note/channel and
  processor-specific. First matching attempt wins even when rejected. Nested
  same/foreign scopes and exception unwinding restore correctly. A real second
  producer thread cannot substitute its same-pitch token into a UI-thread scope.
- Audio ownership distinguishes actual new held slots from refused ninth notes;
  rejects old gesture identity after restrike; retains identified UI release
  tails but blocks unknown/host held, pedal and release aliases on all channels.
  Host reuse of the same slot/pitch clears origin. Clear/reset revokes both the
  current gesture table and old slot attribution.
- The exercised consumer drain/reset/ownership paths make zero ordinary C++
  allocations under an instrumented `operator new`/`new[]`. This is not aligned
  allocation instrumentation, malloc interception, or entire-plugin coverage.

The follow-up eight-slot representation also passes: all eight current UI
owners coexist across channels; channel-specific note-off clears only that
channel, a host pitch event clears every matching channel, and last-slot host
reuse cannot preserve an old identity. The helper is **512 bytes** on this
Linux x86-64 ABI (previous keyspace table:65,792 bytes); compile-time bounds
limit it to sixteen tokens and1024 bytes. This removes unnecessary per-instance
storage but does **not** establish the cause of the separate Windows native
smoke SegFault. The original98-check result remains recorded in commit16735ee.

Seven isolated temporary header mutants compiled and were rejected:

| Fault | Failed checks |
|---|---:|
| Omit pressure epoch validation |4|
| Use global capture instead of thread-local capture |2|
| Replace first-attempt capture with latest-attempt capture |2|
| Ignore onset generation in token equality |4|
| Admit unidentified same-pitch host aliases |11|
| Assume allocation despite previously occupied held slots |3|
| Retain slot origins during clear |1|

These sensitivity tests are not a completeness proof. The separate worker's
paused-publication test exercises reset races in the real header for both note
and pressure events; it is a distinct lane, not duplicated by this report.

Static review used the local official JUCE8.0.11 implementation: mouse hooks run
before listener publication, and `mouseExit` releases the pointer note. The real
widget must capture around outer base mouse dispatch, keep same-key drag identity,
and invalidate on exit so same-pitch reentry captures its new actual onset.
Review also found that **refused incoming host onsets** must not revoke an
existing UI release claim. The implementation now conditions that revocation on
an actual allocated slot, with a regression fixture. This correction was
statically rechecked; native host/widget fixtures remain a separate runtime gate.
Genuinely overlapping same-channel/same-pitch UI and host held voices retain the
Engine's pre-existing newest-note-off ambiguity; pressure attribution is not a
new source-targeted note-off API, and Panic remains the documented recovery.

The first native16735ee pointer fixture failed four pressure assertions. Static
inspection found a concrete fixture precondition error: JUCE's real keyboard
hit-test calls `reallyContains`, whose top-level `contains` requires a native
peer. A constructed peerless editor cannot hit any key. The owner corrected
the fixture to attach/show a real peer and assert that each test point resolves
the requested note; pressure assertions are not weakened. Native corrected
fixture execution remains a separate gate, not a result of this helper audit.

Reproduction from repository root:

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror -pthread -Isrc \
  tests/KeyboardPressureAuditTests.cpp -o /tmp/keyboard-pressure-audit
/tmp/keyboard-pressure-audit
g++ -std=c++20 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined \
  -Wall -Wextra -Werror -pthread -Isrc tests/KeyboardPressureAuditTests.cpp \
  -o /tmp/keyboard-pressure-audit-san
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 /tmp/keyboard-pressure-audit-san
```

No leak check, TSan result or native host/widget runtime result is claimed here.
The general editor-safety scope intentionally changes stale gesture/timestamp
edge cases; host MIDI aftertouch and core rendering remain separate contracts.
