# Remaining exact idle-control opportunities

2026-11-01. This read-only assessment follows the
[nine-checkpoint CPU run](cpu-serial-fir-run/README.md). Its final source,
`23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162`, measures **0.0360525 quiet-idle
callback RTF** and **0.388281375–0.557422125 held-note median RTF**. The original
idle target of at most 0.02 and held target of 0.15–0.20 remain open. The exact
measurements and before/after observables are retained in
[fir-comparison.json](cpu-serial-fir-run/fir-comparison.json).

There is meaningful remaining work in stable electrical-control bookkeeping,
but the existing evidence does not establish a single exact omission that can
close the remaining idle gap. No new profile, build, benchmark, or production
change was made for this assessment.

## Evidence and source scope

The [existing quiet-idle instruction profile](cpu-profile/README.md) was captured
at `d06648c386486cf2630de6e41195ddd5d4360ff0`, before the native BBD predicate
repair and FIR change. It recorded 535,065,489 instructions. These are
instrumented instruction counts, not native CPU-time shares. The raw capture,
compiler settings, driver and source hashes remain in that evidence package.
The current review uses the published
[Engine.cpp](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162/src/core/Engine.cpp),
[Dsp.h](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162/src/core/Dsp.h),
[Smoothing.h](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162/src/core/Smoothing.h), and
[TouchCircuit.h](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162/src/core/TouchCircuit.h).
Engine.cpp, Smoothing.h, TouchCircuit.h and OriginalSubOscillator.h are unchanged
between the profiled and reviewed commits. Dsp.h's later FIR change is separate
from the filter-envelope code examined here.

## What globals and readouts contain

**The 20.03% readout share is included in the 43.72% globals share. They must
not be added.** Likewise, the own-instruction and envelope rows below are
components of the readout row, not additional whole-engine costs.

| Scope in the existing capture | Instructions | Original total | Work |
| --- | ---: | ---: | --- |
| `tickGlobals()` inclusive | 233,955,377 | 43.72% | Global clocks, smoothing, touch, electrical controls and panel noise |
| `updateSourceReadouts()` inclusive | 107,171,328 | 20.03% | Sixteen envelope clocks, drive assembly, gate reconciliation and cache checks |
| `updateSourceReadouts()` own Engine.cpp instructions | 64,341,120 | 12.02% | Loop/addressing work, activity/stage checks, array initialization, stores and exact comparisons |
| `FilterEnv::tick()` called by readouts | 41,943,040 | 7.84% | Reconstructed envelope state and elapsed-time updates |
| `beginSmoothedControls()` inclusive | 41,418,752 | 7.74% | Controller checks, target preservation, smoothing, conversions and gather/scatter |
| `endSmoothedControls()` | 3,538,944 | 0.66% | Restoring event-set controller targets; outside `tickGlobals()` |
| Entire TWS loop and its surrounding stores | 27,000,832 | 5.05% | Eight touch recurrences, acquisition checks, output copies and pressure-bus preparation |

The TWS aggregate comprises 17,367,040 instructions attributed to TouchCircuit.h
plus 9,633,792 attributed to the surrounding Engine.cpp loop and output/bus
preparation. It is already inside the globals total. These source attributions
were obtained by annotating the retained capture, without rerunning it.

### Electrical readouts are audio consumers

`updateSourceReadouts()` supplies `sourceCutoff` and `sourceShared`.
`tickRanks()` consumes them for cutoff, resonance and VCA level, including
free-running idle rank updates. Delaying them until GUI observation would
therefore change circuit evolution.

The existing cache already bypasses nearly all unchanged `evaluateCutoff()` and
`evaluateShared()` work. The repeated cost is constructing `BothRankDrives`,
visiting sixteen envelopes, reconciling stale gates after inactive/panic states,
storing `sourceFilterValues`, packing `{z, touchB, touchA}`, and comparing those
records with `sourceReadoutDrives`. Another cache around the network solve would
not address most of this cost.

### Settled voltage does not mean a stopped envelope clock

`FilterEnv::tickReconstruction()` still executes these statements when its
voltage has settled:

```cpp
preciseTime += 1.0 / static_cast<double>(sampleRate);
stageTime = static_cast<float>(preciseTime);
```

`Engine::getRankEnvelopeState()` exposes elapsed state. Returning early solely
because the voltage is unchanged would change that observable. Replacing the
ordered additions with `count / sampleRate` is not generally bit-identical.

An exact lazy elapsed clock would require ordered materialization, or a
separately proven equivalent algorithm, across sample-rate and arithmetic-state
boundaries. Diagnostic reads, copies and later events must observe the same
state. Stage-only internal queries must not materialize every pending interval
unnecessarily. An unbounded replay on the audio thread would move the cost into
a potentially large callback rather than establish a realtime improvement.

TWS also retains state after release. Its held `b`, high-pass memory, acquisition
window and pressure relaxation cannot be omitted merely because a voice is
inactive. A fully zero state after acquisition closes is a candidate fixed
point; it requires explicit admission and continuation proof.

## Concrete follow-up

A stable-drive fast path and event-invalidated settled-controller mask merit a
separate qualification effort.

For source controls, keep all required envelope elapsed clocks in their existing
order. Track whether an envelope voltage/stage or touch output can change. When
all relevant drives are demonstrably unchanged and the readout cache is valid,
omit repeated drive-array initialization, packing, stores and the second
comparison pass. Invalidate on source/parameter transactions, note and pedal
events, panic reconciliation, touch acquisition, reset/prepare and departure
from the fixed point. Changed drives must still reach all consumers at the
same interval, including idle ranks.

For controllers, track exact target changes and settled state at mutation
points. Avoid gather/advance/scatter/restore only for fields whose effective and
event-set values are identical. Preserve signed-zero target changes, note-age
snapping, optical-expression state and existing nonfinite fallbacks.
`tickRanks()` temporarily zeroes inactive pressure, bend and timbre; omitting
both begin/end functions without retaining their restoration behavior would be
incorrect.

This proposal leaves RNG streams, oscillator phases, free-running rank history
and nonzero tails advancing. Before implementation it needs a frozen exact
state/continuation contract, actual-source mutations, compatibility gates and
three stock timing rounds before and after. The full function totals above are
generous work budgets: guards and required state updates would remain.

## Illustrative instruction arithmetic

Reaching 0.02 from 0.0360525 requires **44.5253% less native callback time**.
For orientation only, subtract the entire old FIR loop's 117,112,832
instructions from the old profile total, leaving 417,952,657. This deliberately
optimistic accounting assumes no replacement bookkeeping. It is not a new
profile of the final source.

| Hypothetical removal from the old capture | Instructions | Fraction of that remainder |
| --- | ---: | ---: |
| Entire readout function, including mandatory clocks | 107,171,328 | 25.64% |
| All controller begin/end work | 44,957,696 | 10.76% |
| Both complete categories | 152,129,024 | 36.40% |
| Both plus the complete TWS loop and wrappers | 179,129,856 | 42.86% |
| Previous row retaining just the two envelope clock statements | 170,741,248 | 40.85% |

If native time were proportional to those instruction counts, deleting the two
complete categories would leave about 0.02293 RTF; adding complete TWS deletion
would leave about 0.02060 before guard costs. **Those values are illustrative,
not native timing predictions or rigorous native-speedup bounds.** Instruction
latency, dependencies, branches, memory traffic, compiler changes and scheduler
effects prevent that conversion. The deletions also include mandatory work,
making them unattainable as stated. This arithmetic supports prioritization; it
does not prove that a future exact implementation can or cannot meet 0.02.

Smaller unused-readout opportunities remain around PWM sine evaluation and the
SUB waveform, but their phase/RNG clocks and idle-rank consumers remain live.
The three modern LFOs already omit unused waveform evaluation, and SUB rate
transforms and settled settings already have caches. The existing evidence
therefore supports the concrete stable-drive/controller follow-up, while both
CPU targets remain open pending implementation and measured qualification.
