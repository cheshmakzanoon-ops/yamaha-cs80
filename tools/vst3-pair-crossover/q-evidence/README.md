# Same-binary cloud timing: qualification Q passed

**All 35 predeclared qualification gates passed in all five scenes. Both sides loaded the same physical cutoff-stock VST3. No candidate was timed, so this is not a CPU saving.** Q establishes that *this* cloud callback-clock setup can proceed to a separately sealed comparison of a large idle change, provided the candidate also passes native parity and concurrent-in-campaign stock sentinels. It does not qualify physical DAW hardware or small, universal improvements.

| Factory workload | Identical stock work: B/A change (95% interval) | B measured with +12.5% genuine callback work (95% interval) |
|---|---:|---:|
| Fresh idle | +0.83% (−0.55%, +2.23%) | +13.62% (+12.06%, +15.20%) |
| After equal-velocity release | +0.14% (−0.88%, +1.17%) | +12.60% (+11.53%, +13.69%) |
| After varied-velocity release | −0.03% (−0.91%, +0.86%) | +12.70% (+11.90%, +13.50%) |
| Held varied velocities | −0.06% (−1.33%, +1.23%) | +12.43% (+11.09%, +13.78%) |
| Varied onset/wake window | −0.28% (−2.01%, +1.47%) | +12.14% (+10.17%, +14.15%) |

**Controls and inference.** Two isolated processes, one physical stock ELF, same logical vCPU per pair. The clock sampled thread CPU on either side of a genuine `IAudioProcessor::process` call, excluding turn handoff. The ordinary control counts 512 matched calls; the positive control includes an additional 64 genuine matching-timeline calls from B, with the *same* denominator. It checks a known difference in the amount of audio work counted, not a guarantee that a candidate's implementation will be faster.

Each of 12 time epochs contains four serial logical-vCPU cells (CPUs 0, 2, 4 and 6). Every scene's 95% t interval uses those **12 epoch-level log contrasts**, not hundreds of thousands of callbacks as independent replicates. In every scene, the same-work interval lies wholly inside ±2.5%; the extra-work interval lies wholly inside +7.5% to +17.5% and its point estimate inside +10% to +15%. Stock-twin quantum-specific points stayed inside ±3%, CPU-stratum and temporal-half points inside ±4%. Every individual scene/cell met the empty-clock (<2%) and adjacent-handoff (<3 callback wall durations) diagnostics. Across all 240 cells their observed worst cases were **0.80% and 1.08×**. See the [prospective protocol](../PROTOCOL.md) for exact methods and limitations; the [previous failed design](../../vst3-target-qualification/cloud-diagnostic/README.md) remains failed.

**What ran.** 48/48 fresh paired slots, 240 scene cells, **276,480 timed real callback rows** and **61,440 empty-bracket rows**; no replacement, candidate, or exclusions. The acquisition compared 24,947,518 public capture bytes per worker per slot to the retained isolated reference, including full warm, measured and continuation audio, floating-point/event data and state for all five scenes. Affinity, genuine callback counts, original endpoint chronology and fixed workload/order were checked before a slot advanced. Logical guest CPUs are not a claim about physical cores, stable frequency, DSP deadlines or real-world utilization. Empty overhead was not subtracted.

Wall time: **23m49s** (75m cap). Pooled worker CPU: **20m37s** (40m cap); per-pair wall cap 100s; 1 GiB retained-data cap. The scheduler enforces 120 s between internal epoch starts and 10 s after the previous epoch's workers finish. Actual observed end-to-next-worker-start gaps were at least 11.04 s. Actual worker start follows epoch setup; the artifact does not encode a separate timestamp for that internal supervisor start.

**Identity and independent replay.** Stock ELF SHA-256 `0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2`. Q-only stage seal `48e3499addacc08e363f43c75e5888ceee98b52c76978f450191e7f4994dd8ae`. A separate audit read both workers' original CSVs for every slot, checked the real/empty endpoints and ordered baton, confirmed the 48 entries in the attempt/verification ledger, verified each normalized slot hash and its place in the combined output, checked the declared controls and recomputed all gate decisions. All **35** agreed; across **195** checked numerical values the largest absolute difference was **3.6×10⁻¹⁵**.

[`sufficient.json.gz`](sufficient.json.gz) contains all ordered per-cell timing sums split by lane, quantum and extra/common assignment; the integer statistics needed for the overhead/handoff gates; per-slot resource bounds and identities; original digest receipts; and the original decision. It contains **no private paths, native binaries, PCM dumps or full raw timestamp arrays**. From this directory:

```sh
python3 -B replay.py
python3 -B test_replay.py
```

This offline replay independently reproduces the statistical decision, numerical bounds and all gates without importing the primary estimator. Its pinned SHA-256 is `bc3693f582112c9bb21e86e112f9766248a6fc63dcddb7b1df7beef0182022bf`. It does not re-render audio or authenticate omitted raw material. If the original sealed Q directory is independently available, `python3 -B export.py /path/to/sealed-run /path/to/new.json.gz` rechecks the raw-vs-normalized ordering/counters and regenerates the compact evidence. No command here can launch phase C.
