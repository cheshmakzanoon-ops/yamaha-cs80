# Separate C stage: fixed FIR-only comparison and live sentinels

No candidate was loaded by the stock-only [Phase Q](Q-SUPERVISOR.md). Before
any C timing the new supervisor replays the **original raw Q files** and all
35 gates under Q's unchanged seal. It admits only these two verified images:

- A: previous shipping cutoff-only ELF `0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2`.
- B: FIR on that identical cutoff baseline `74f2507db4a7042a23a96b9a8c2249cc2f76d1f7f417ebb4838498d7f0578895`.
  This is **not** a route-only comparison, heterogeneous-drive test, or timing of later main.

The [admission receipt](c-preflight/complete.json) is **zero-clock**. In six
fresh worker processes (isolated A, isolated B, paired A/A and paired A/B), the
full five-scene program produced **40,404 audio callbacks and 60 public state
snapshots per worker**. Each matched the existing [complete raw reference](zero-preflight/README.md),
including per-sample audio, caller FP/probe/errno, all concrete MIDI inputs and
reads, controller settings and varied-velocity wake continuations. The earlier
[failed dry attempt](c-preflight/first-attempt.json) was caused by the verifier
comparing a Python list to an otherwise identical tuple for the physical file
identity. It stopped after the isolated stock process; no candidate or timing
had run. The check was corrected, that attempt retained, and **its CPU, wall
time and disk are charged to this C stage**.

The Q-qualified interaction and clock endpoints are unchanged. C has a separate
binary identity, separately accepted seal and fresh output/latch. In
each of 48 cells run both a fresh same-physical-A/A sentinel and a fresh A/B
pair, in the protocol's counterbalanced order. A changes physical lane
independently of process launch order. Each worker has its own loaded module,
HOME, lifetime and four-clock native calls on the same pinned vCPU as its peer.
The candidate does not enter a sentinel. Never inspect gate effects until all
96 pair attempts finish; stop only for limits or invalid integrity. An interrupted
campaign cannot be resumed or selectively repaired. C is bounded to 150 min
elapsed (starting from the first dry attempt, including the wait to launch),
80 pooled worker CPU min with all admission attempts charged, 100 s per
two-worker pair, and 1 GiB retained including C dry trees.

```sh
python3 -B build_c_host.py /path/to/JUCE/VST3_SDK /tmp/new-c-worker
python3 -B candidate_pair.py --host /tmp/new-c-worker --stock /actual/cutoff.so \
  --fir /actual/fir-on-cutoff.so --q-stage /actual/completed-q --out /tmp/new-c-dry
python3 -B phase_c.py seal --host /tmp/new-c-worker --stock /actual/cutoff.so \
  --fir /actual/fir-on-cutoff.so --q-stage /actual/completed-q \
  --full-dry /tmp/new-c-dry --out /tmp/new-c-seal
python3 -B -m unittest -v test_phase_c test_preflight test_crossover
```

Supply `--prior-dry DIR` for every retained earlier dry attempt. The seal records
the precise Q, host, cutoff, FIR, source, hardware-affinity, schedule, golden
and prospective budget bindings. Sealing does **not** start clocks. Only after
explicit go: `python3 -B phase_c.py run --sealed /tmp/new-c-seal --accept-seal <C-seal-sha256>`.
The Q authorization string cannot launch the C worker.

The supervisor retains all original endpoint rows and empty brackets, measured
and sentinel cells separately, collects their verified public image/equality
receipts, and recomputes the entire numerical decision from original C/S raw
rows only at the end. Its terminal verdict requires **both** passing fresh live
sentinels and passing candidate gates; a positive candidate-only interval or
a successful process exit is not a qualified result. The 12 whole-time epochs
are the inferential units, not 96 independent hardware targets or the individual
audio callbacks. No automatic publication or product decision follows.
