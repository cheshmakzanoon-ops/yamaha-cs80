# Host snapshots, undo ownership, and the modern state child

2026-10-31. This is a software synchronization contract. It introduces no
audio model, parameter, default, or hardware claim. Acceptance was published
before evaluation as `4f1bd64c7c5db3e61967c8db2ae93612b58f6a30` in
`tests/parity/tolerances-v2/state-tree-synchronization.json`.

## Mechanisms corrected

A host may call `getStateInformation()` on a worker while the message thread
waits for it. The former `syncNow()` returned immediately on that worker.
JUCE 8.0.11's subsequent `AudioProcessorValueTreeState::copyState()` called
`flushParameterValuesToValueTree()` with the APVTS's UndoManager. Pending host
automation could therefore become a generic ValueTree undo action in the
user's current transaction. Undo could revert an unrelated automated control,
and the generic action bypassed the plugin's parameter-aware host gestures.

The former `ModernLayerState::store()` also replaced the live `MODERN_LAYER`
child outside the APVTS common tree lock. Its separate configuration lock was
released before those ValueTree writes. A properly locked host snapshot or
state replacement could still overlap a shape/routing edit on the same tree.

Allowing worker synchronization also exposes an implementation hazard: setting
a shared plain `writing` flag on a worker would suppress an unrelated
message-thread user edit while that worker wrote a different parameter. The
correction keeps the flag entirely on its owning message thread. The additional
precommitted contracts register a deterministic overlapping-edit witness,
exact host-value checks, and history query/transaction lock witnesses, so
passing the original six fixtures alone cannot qualify the implementation.

The final history audit also found two callers outside those scopes: factory
program loading cleared history after `syncNow()` released its internal lock,
and MSEG mouse-down called the raw UndoManager transaction method. The factory
clear could overlap a worker restore or a toolbar history read. MSEG mouse-down
could start its transaction before pending external values reached the live
tree. The precommitted extension `2ee33dcc8113aaca4141dc7d77fb374384d8560c`
registers actual destruction and mouse-event witnesses for these paths.

The real MSEG drag fixture exposed a separate serialization defect under its
already registered exact-round-trip requirement. `store()` published the
original sanitized floats to the running configuration, but `encodePoints()`
wrote only six decimal places. Undo/redo and session restoration decoded those
rounded points. The recorded drag changed point time from `0x3e06a3b5` to
`0x3e06a3be` (nine float steps) and level from `0x3f64c416` to `0x3f64c415`
(one step); the earlier volume edit stayed exact. Thus the live shape and its
replayed version could differ even when transaction ownership was correct.

Point serialization now uses the existing JUCE locale-independent scientific
formatter with `std::numeric_limits<float>::max_digits10` digits after the
decimal point. Its ten significant digits preserve every admitted float,
including tiny nonzero values. The schema and existing numeric reader remain
compatible. Fresh live edits already used these values; the intentional
behavior change is that replay and restored modern modulation retain them.
The circuit-host audio commit follows the separate V2 pin/strict-gate sequence;
the synthesis laws, parameter layout, defaults, and PURE policy are unchanged.

## Ownership and lock order

`UndoableParameterState::syncNow()` reconciles external parameter changes on
any non-audio thread, without an UndoManager. Pending user edits are recorded
only on the message thread. Its `copyState()` takes the common lock, calls
`syncNow()`, creates a detached tree, and overlays current parameter atomics
on that detached result. It does not invoke JUCE's generic undo-recording
flush. A worker snapshot sees a still-open user gesture's current value while
leaving its live-tree history and coalescing to the message thread. No
MessageManagerLock, synchronous UI dispatch, or waiting for a timer is used.
The recording-suppression guard reads and writes its flag only on the message
thread. Workers never touch that flag. Synchronization copies the canonical
APVTS physical atomic without an additional normalized-value roundtrip.

The processor's APVTS accessor exposes the concrete undoable state so normal
callers select this snapshot implementation. The existing host/preset copy and
replace sites continue to take `treeLock()` and call `syncNow()` first.

Modern configuration reads, read/modify/write edits, insertion/replacement of
the live child, and reload callbacks use the same common tree lock. The order
is always **common APVTS lock, then configuration lock**. The configuration lock
is never held while acquiring the common lock. Worker configuration edits are
external state changes and do not enter user undo history. Message-thread
editing retains the existing UndoManager behavior.

Processor undo/redo holds the common lock for the whole history transaction,
including generic actions on the modern child. Parameter actions still send
one begin/value/end sequence per changed parameter and exclude their replay
from new history. The audio thread continues to consume the existing mailbox
with its try-lock; it never takes the common tree lock.

The toolbar's four history queries consume a detached `HistorySnapshot` read
under the common lock. User transaction opening holds that lock across external
synchronization and `beginNewTransaction()`. Those reads and writes therefore
cannot overlap a worker restore clearing the UndoManager. Message-thread user
classification flags retain their existing owner.

Factory program loading holds the same lock across `syncNow()` and the real
`clearUndoHistory()` call. The MSEG editor owns a `ScopedUserEdit` from
mouse-down through mouse-up, using the already synchronized transaction path.
Its multiple point updates remain one named "MSEG edit" transaction, and
pending external parameter values are reconciled before the first drag.

These worker guarantees apply while the processor remains alive. The fixtures
create and destroy processors on the message thread; they do not qualify
arbitrary concurrent owner destruction or replace the separate validator host
lifetime evidence. The existing host/wrapper lifecycle still owns processor
destruction and serialization with UI callbacks.

## Deterministic regression and mutation gates

| Contract | Preregistration commit | Registered fixtures / mutations |
| --- | --- | --- |
| `state-tree-synchronization.json` | `4f1bd64c7c5db3e61967c8db2ae93612b58f6a30` | 6 / 7 |
| `state-tree-recording-ownership.json` | `706c673c54772eb45cb9fe41bd7dbf342792c64a` | 2 / 2 |
| `state-tree-history-lock.json` | `706c673c54772eb45cb9fe41bd7dbf342792c64a` | 2 / 2 |
| `state-tree-history-clear.json` | `2ee33dcc8113aaca4141dc7d77fb374384d8560c` | 2 / 2 |

`tests/StateTreeSynchronizationTests.h` is included in the complete UI smoke
suite and its focused `--ui-completion-test` mode. `--state-tree-test` runs the
same new fixtures alone for mutation qualification. It does not replace any
existing test or alter a tolerance.

The original six registered fixtures cover immediate worker automation followed by a
host snapshot, direct worker synchronization, a snapshot during an open user
gesture, modern insertion/replacement, the production live-tree reader, and a
non-default MSEG/routing snapshot/restore with undo and redo.

Four additional fixtures check the final value of a still-open user gesture
while a different worker write is paused at its actual ValueTree callback,
1,701 legal host-value cases across nine controls, synchronized history snapshot
reads, and transaction opening. The worker rendezvous uses explicit events and
also signals completion if a mutant omits the expected callback, so omission
fails an assertion instead of deadlocking. The exact-value oracle observes the
legal setter result; it does not assume perfect inversion of nonlinear ranges.

The final two fixtures destroy a real stored `UndoableAction` during factory
loading and drive a real MSEG point drag. The action's destructor probes the
common lock from another thread. The MSEG fixture checks the pending external
value immediately after mouse-down, before any drag or message dispatch, then
checks exact shape undo/redo and the independent preceding user transaction.

Fourteen additional coordinate cases exercise the existing exact MSEG snapshot
requirement through real worker save/restore calls. They include the observed
drag values, adjacent float boundaries, zero, the maximum permitted time,
normal values below one millionth, and admitted subnormal times, levels, and
curves. The expected inputs come from the model's sanitizer, and comparison
also checks the actual float bits so a tiny nonzero value cannot silently
become zero. No tolerance was added or loosened.

Lock witnesses run at actual synchronous ValueTree mutation callbacks or at
the production operation's live-tree read. A distinct thread calls only
`tryEnter()` and returns immediately. Its acquisition must fail while the
production operation owns the common lock. The test never infers exclusion
from a sleep, arbitrary elapsed time, or a separate reference lock. The private
reader dependency permits that witness without an audio-path mode switch.

The original seven registered mutations restore the old worker sync/snapshot mechanisms,
omit the pending-user overlay, remove modern write/read/replay lock scopes,
or remove undo host gestures. Every mutant must compile and fail the intended
assertion; compilation failures, crashes, deadlocks, and environmental harness
errors do not count as detection. All raw failures are retained.
Four additional mutations restore cross-thread recording suppression, perturb
the synchronized physical value by one representable float, or remove the
history-reader/transaction-open common lock. The private reader/writer witness
dependencies execute the same production scopes and real JUCE history calls.
Two final mutations remove the factory-clear lock or restore the raw MSEG
transaction call. The action-destructor and immediate pre-drag assertions must
detect those mechanisms independently of the earlier transaction-open witness.
An additional serializer mutation restores six fixed decimal places. It must
fail the already registered exact MSEG redo assertion; the boundary matrix also
detects its loss of small nonzero values. There are fourteen mutations in total.

## Qualification

Qualification is performed in an isolated worktree using the complete project
sources and frozen, hash-recorded unchanged JUCE module objects. Every project
translation unit, including Engine, uses the isolated headers, so no Engine
layout is mixed across revisions. The integration branch still owns its full
CMake/CTest, immutable audio, native host, and CI qualification.

Results and source hashes are recorded in the accompanying state-tree evidence
manifest after the registered control, mutations, and original UI suite run.

The 2026-10-31 local qualification passed: the control reported zero failures;
all fourteen mutants compiled and failed their intended assertions; the full
existing UI suite ended with `UI SMOKE TEST PASSED`. The exact-value matrix
covered 1,701 legal host setter results and exposed 161 cases where an extra
normalized roundtrip would differ from the canonical APVTS atomic. All fourteen
MSEG coordinate cases, including nine admitted subnormal fields, survived
worker save/restore with zero bit mismatches. The before/after drag diagnostic
also confirms exact point time and level on redo after the serializer fix.

The [evidence summary](evidence/continuation-2026-10-31/state-tree-synchronization/summary.json)
links every raw control, mutation, full-suite, and precision diagnostic log.
The [complete manifest](evidence/continuation-2026-10-31/state-tree-synchronization/qualification.json)
retains source/dependency hashes and all compile/link commands. Setup failures
(a missing generated font header, missing X11 tools, and sandbox-denied X
sockets) are retained separately and do not count as gate results. The initial
real exact-redo failure is retained as the serializer counterexample.

Reproduce the twelve registered fixtures, additional MSEG coordinate cases,
and fourteen mutations after building the normal
Release `CS80Snapshots` target:

```sh
python3 scripts/check-state-tree-synchronization.py \
  --build-dir build --output-dir /tmp/cs80-state-tree-qualification --jobs 2
```

The output directory must be new. The runner leaves production sources and
build objects untouched, records every compile/link command and raw test log,
and requires the configured dependency to be the pinned JUCE 8.0.11 revision.
It reruns the complete existing UI suite with the restored control executable
after all mutations. Only the documented environmental Openbox exit 70 is
retried, once, retaining both attempts.
