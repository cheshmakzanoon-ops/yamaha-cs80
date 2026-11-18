# Shared source-readout retention: whole-engine CPU screen

**Decision:** The complete screening rule did not pass. Do not use these results to claim a general speedup or promote the patch into a plug-in. The shipping engine was not changed. Specifically, the primary lower bound did not exceed its floor and the regression limit was not established for Factory moving.

The Source Performance engine normally solves a shared control network each internal sample. The [isolated candidate](candidate.patch) can retain the published shared readout if its inputs are provably unchanged. For the same resonance, rank controls and exact touch inputs, ordinary finite envelope values in the accepted range do not affect that network's numeric result. Cache hits also require compatible floating-point controls and already-raised exception status. Each cutoff solve, envelope and touch update, voice state and public scalar solver remains in place. Pure stays on the scalar solver. Explicit early invalidation at control updates prevents stale results after interrupted transactions.

## Measured result

| Late eight-voice workload | Estimated CPU reduction | 95% whole-block interval | Shared solves retained / 65,536 internal samples |
|---|---:|---:|---:|
| Factory held (primary) | +2.95% | [+1.35%, +4.37%] | 65,535 |
| Factory moving | −2.16% | [−11.43%, +4.14%] | 0 |
| Panel moving | +2.72% | [+0.38%, +5.60%] | 0 |
| Pure held | +3.72% | [+1.66%, +6.73%] | 0 |

Positive means lower thread CPU in the candidate. The held-factory primary lower bound had to exceed **+2.29%**, the larger of 2%, measured stock-only variability and three times the largest empty-clock fraction. Each of three controls separately had to rule out a slowdown beyond 3%. [Frozen method and identities](admission.json); [full observations](raw.csv); [calculation](result.json).

Four fresh pinned processes per A–B–B–A block, two all-stock calibration blocks and six comparisons per workload: 128/128 slots. GNU C++ 14.2 x86-64 Linux Release/PIC; 48 kHz, 64-frame blocks, 65,536 warmup frames and 32,768 measured frames. Time includes the actual per-block engine control/event handling and render, but not output hashing or test setup. All cases played eight active voices and had nonzero output; each arm produced its workload's same complete measured PCM fingerprint and occupancy. Full raw clocks are per-process sums, not arrays of every callback or a deadline/real-DAW test. Cases were chosen before timing. No outlier removal, replacements or reordering. The reported interval resamples six whole A–B–B–A blocks, not individual callbacks, with the fixed original seed. The cooperative executor was reserved; physical host frequency and all unrelated shared-host activity were not controlled.

## Correctness and limitations

* All 193 stock source-file identities were checked against the baseline and the fully rebuilt stock core archive was byte-identical to the retained shipping archive; complete candidate archives and benchmark drivers used matching headers. Appending the memo changes `sizeof(Engine)`: this is **not** a binary-compatible in-place patch for an existing plug-in.
* [Direct differential](direct.cpp): 10,240 paired cases over rounding/FTZ/DAZ and exception conditions, including 2,048 rejected transactions; three deliberately faulty variants were detected. [Lifecycle and actual trap checks](lifecycle.cpp) cover reset, copy, boundaries, policy, interrupted solves and continuation. Three [public-operation trap traces](public-traps-equality.json) matched stock across 1,858,941 recorded bytes; 14 [engine trace pairs](trace-equality.json) matched across 149,151,968 bytes including PCM and selected state/readouts. These are finite histories, not proof of every state or compiler. The raw trace bytes and binaries are not distributed.
* The unchanged project boundary, factory tone and source policy targets passed on the frozen candidate: 22 tones, 373,977 tone checks and 485 policy checks. Those are focused runs, not the full repository CTest or sanitizer suite.
* No native plug-in timing is included. The historical 16-note native panel fixture is not this eight-voice factory workload. The [shipping JUCE processor](../../../src/plugin/PluginProcessor.cpp) sends its control frame on each callback; the core fixture intentionally sends at the recorded points in [its source](cost.cpp). Do not transfer these numbers to a DAW CPU meter, the product budget, another platform, or unrelated patches.

## Replay

From this directory, run `python3 verify.py` and `python3 -m unittest -v test_verify`. The replay verifies all 128 original slot totals, affinity observations, PCM fingerprints, voice occupancy and the calculation. It performs no synth timing. The [separate filter experiment](../saturated-filter-cutoff/README.md) failed an actual trap check before timing and is not part of this treatment.
