# Paired native callback timing: design and dry preflight

**Not a native CPU result.** The old [cloud clock study](../vst3-target-qualification/cloud-diagnostic/README.md) did not establish ±2% equivalence; it is not being rerun. The [prospective protocol](PROTOCOL.md) specifies a different paired acquisition for a *large* idle benefit. It requires native per-thread clock endpoints, actual VST3 note/control processing, same-binary qualification, live stock sentinels and retained adverse results. Instruction-count gains alone cannot satisfy it.

The statistical code and following commands are offline and **cannot** launch Q, launch a candidate, or promote anything:

```sh
python3 -B crossover.py schedule
python3 -B crossover.py lane-plan --epoch 0 --cpu-cell 0 --scene wake_varied --output /tmp/new-cs80-order.csv
python3 -B -m unittest -v test_crossover
```

The schedule has 48 serial worker-pair slots, four vCPU strata per time epoch. Lane-plan CSV gives both workers the same musical indices and the quantum/first-lane/extra role. For each matching index both perform the same callback. With quantum 4, one worker completes four callbacks, then the other does those same four. Remaining work is not divided by the number of samples. Neither individual audio samples nor per-callback deltas are inferential replicates.

## Verified zero-clock rehearsal and cost to run Q

The native **dry-only** worker and two-process supervisor were exercised with the pinned cutoff release. One full five-scene rehearsal rendered 40,404 callbacks and captured 60 public states **per worker**; the isolated reference and both peer workers agreed byte for byte on PCM, FP/event histories and states. The full paired slot took 26.54 s elapsed / 26.38 s pooled worker CPU. Extrapolation with enforced epoch spacing: ~23m46s elapsed and ~21m06s pooled worker CPU, before real clock brackets, validation and VM drift. Allow roughly 25–35 min; hard phase-Q bounds are 75 min elapsed / 40 pooled worker CPU min. These **whole-rehearsal** times size the experiment; no callback, placebo or treatment was timed. No native CPU savings follow.

## Proposed acquisition-to-analysis contract

The standalone numeric reducer accepts one complete normalized JSON/JSON.gz file at a time via `python3 -B crossover.py analyze FILE`. Its root is `{role, a_sha256, b_sha256, cells}`: Q and S require both images to be the **same pinned cutoff-main stock ELF**; C requires one distinct, separately sealed candidate. Each of 240 scene cells has those same role and digest fields, zero-based `epoch`, `cpu_cell`, `scene`, `empty_cpu_ns` (two arrays of 128 nonnegative integer thread-CPU empty-bracket durations), and `clocks` (exactly 1,152 rows). Each clock row is **[musical_index, physical_lane_0_or_1, thread_cpu_start_ns, thread_cpu_end_ns, monotonic_start_ns, monotonic_end_ns]**. Optional privacy normalization may remove one common wall origin across both processes and one CPU origin per worker; it must preserve cross-process wall order. The complete raw sequence must obey the paired visit order. Units of inferential replication are complete temporal epochs (12), not cells, callbacks, or logical core labels.

An `analyze` pass validates only the numerical/row contract. These inputs cannot by themselves establish gated binary loading, state parity, module-to-lane identity, preserved machine conditions, complete first-attempt execution, or the upstream Q/live-sentinel locks. Those must be satisfied separately before using results. **No candidate is qualified by an analyzer file alone.** The prospective plan must be reviewed, saved and sealed before starting comparative clocks. No production code or product default is modified here.
