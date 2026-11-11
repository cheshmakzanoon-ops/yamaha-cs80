# Audio-safe publication of program-display updates

## Reproduced problem, including the intermittent failure

During independent verification of signed Scala mapping commit `6fc5354d`, one
17-suite run passed 16/17 in 64.80 seconds. Its allocation fixture reported
`program change callback allocations=1 frees=0` (193 checks / 1 failure).
Another independent complete run passed 17/17 in 64.41 seconds, and ten isolated
allocation repeats passed. Those later passes did not erase the failure.

A temporary diagnostic warmed `backtrace` before measurement, captured up to 32
frames in fixed thread-local storage on an intercepted allocation, and printed
outside the callback. Capture itself ran with counters disarmed. Run 63 of a
bounded 100-process attempt reproduced the failure at 48 kHz, initial Circuit,
16 voices. Symbol resolution of that exact diagnostic binary showed:

```
__wrap_realloc
juce::MessageManager::postMessageToSystemQueue
juce::MessageManager::MessageBase::post
juce::AsyncUpdater::triggerAsyncUpdate
Cs80AudioProcessor::handleProgramChange
Cs80AudioProcessor::processBlock
main
```

JUCE 8.0.11's Linux queue takes a critical section, adds the message to a growable
ReferenceCountedArray, then writes its notification socket. AsyncUpdater has a
preallocated message, but posting it is not an allocation-free audio-thread
operation. The notifier sources were unchanged between published `cb25449b`
and signed-mapping `6fc5354d`; the new mapping parser is off the callback path.
The diagnostic instrumentation is not part of the shipping test or plugin.

## Change and scheduling behavior

LibraryState no longer derives from AsyncUpdater. Its notification request
stores a lock-free atomic flag in the existing UndoableParameterState object.
That object's existing 10 Hz message-thread timer synchronizes parameter state,
then exchanges/clears the flag and calls updateHostDisplay with programChanged.
No new timer, callback allocation, OS queue post or mutex is introduced by this
publication step. A compile-time check requires the atomic flag to be lock-free.
This does not prove that the entire audio callback is lock-free.

Requests coalesce. A request arriving after the exchange, including one from a
reentrant listener, remains pending for the next timer. The host-display hint
can wait approximately 100 ms plus message-loop scheduling delay. Sound and
current program selection still change at their original sample position.
Hosts without message-loop servicing defer display delivery; rendering does not
wait for the timer. Notification is the timer callback's last operation. Existing
message-thread/message-manager-locked teardown discipline is unchanged.

## Tests and limits

The allocation regression retains the 12 rate/policy/voice configurations and
adds actual processor/listener checks: no synchronous display notification,
latest program selection before delivery, coalesced delivery, empty timer ticks,
fresh requests, a request during listener delivery, and pending destruction.
Timer servicing and listeners run outside the armed audio allocation scope.
An initial new assertion incorrectly equated MIDI program 2 with factory index 2;
it was corrected to use the existing bank/program mapping function.

The strengthened fixture passed 204 checks / 0 failures in 100/100 process
repeats. After strengthening the final destruction assertion to leave the
listener registered, the final 15-suite focused selection passed 15/15 in
26.79 seconds, including allocation 204/0 and undo/state tests. An independent
allocation/undo/state subset passed 3/3 in 2.68 seconds.

**Initial broader native qualification failed.** A 21-suite run passed 20/21 in
70.83 seconds: the 44.1 kHz offline VST3 partition test exceeded its unchanged
error bound (2.80042e-5). An isolated repeat passed three times, then failed PURE
byte equality on its fourth attempt even though its modern partition error was
zero. These failures are retained, not replaced by passing repeats or relaxed
criteria. The final 15-suite selection explicitly excludes all six actual-module
VST3 cases and cannot resolve those failures. A separate investigation reproduced and corrected a state-mirror feedback defect, then passed a final 23-suite selection, an independent repeat, and 20 isolated offline repeats. See [Parameter state mirror feedback](analysis/PARAMETER_STATE_MIRROR_FEEDBACK.md) for exact source/control distinctions and limits. The original failures are not erased, and no universal causal conclusion about those native failures is claimed here. This is a direct-processor/Linux linked-
allocator test, not interception of every shared-library allocation, live
MTS-ESP, a real host audio driver, worst-case deadline proof or real-DAW testing.
