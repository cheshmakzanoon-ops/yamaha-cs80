# FIR-only native timing: result not admitted

**The FIR pair used less callback CPU in all three quiet scenes, but this campaign did not establish a native speedup.** The fixed design also required its live same-binary controls to pass. In fresh idle the A/A difference was **+1.1763% (95% interval −0.2282% to +2.6005%)**. The required interval was strictly inside **−2.5% to +2.5%**. The upper end missed by 0.1005 percentage points. This is a failure of measurement qualification, not evidence that the FIR change is slower.

The [frozen protocol](../PROTOCOL.md) applied one combined decision to all five workloads. The earlier [stock-only Q](../q-evidence/README.md) passed 35/35, but cannot replace the live sentinels. In this campaign the live stock controls passed **34/35** and the FIR comparison passed **19/19** numerical checks. No broad-idle, fresh/equal-only or double-digit win is admitted. **Do not promote any row alone, round away the miss, subtract the placebo from the estimate, or treat it as a DAW CPU reading.**

## What was observed

CPU unit: geometric mean callback-thread CPU seconds per rendered second of stereo audio, at 48 kHz and 64 frames per callback. A = cutoff-only baseline. B = that same baseline with the FIR-only change. The last column is the observed average **relative saving**, not a percent of the whole machine. A negative saving means B used more CPU. Intervals use 12 temporal epochs, not hundreds of thousands of allegedly independent observations.

| Factory workload | A: CPU s/audio s | B: CPU s/audio s | Observed saving (95% interval) |
|---|---:|---:|---:|
| Fresh idle | 0.14883 | 0.13442 | +9.68% (+7.79%, +11.54%) |
| Settled after equal-velocity release | 0.14623 | 0.13418 | +8.24% (+7.61%, +8.86%) |
| Settled after varied-velocity release | 0.16268 | 0.14578 | +10.39% (+7.88%, +12.83%) |
| Eight held, varied velocities | 0.63864 | 0.63466 | +0.62% (+0.03%, +1.21%) |
| Wake after silence, varied velocities | 0.75378 | 0.75643 | −0.35% (−0.92%, +0.22%) |

The matched **A/A** live controls ran in separate, fresh processes during the same campaign:

| Workload | Same work: B/A difference (95% interval) | +12.5% genuine extra work: observed difference (95% interval) |
|---|---:|---:|
| Fresh idle | +1.18% (−0.23%, **+2.60%**) | +13.73% (+12.10%, +15.38%) |
| Equal release | +0.11% (−0.84%, +1.07%) | +12.39% (+11.38%, +13.42%) |
| Varied release | +0.73% (−0.91%, +2.41%) | +13.26% (+11.18%, +15.39%) |
| Held | +0.09% (−0.91%, +1.10%) | +12.70% (+11.68%, +13.73%) |
| Wake | +0.12% (−0.68%, +0.92%) | +12.67% (+11.85%, +13.50%) |

## Bounds and identity

All **48/48** planned cells ran once: 12 epochs × 4 serial vCPUs, each with a new A/A pair and a new A/B pair. All five scenes ran in each pair: 552,960 real callback brackets and 122,880 empty brackets in total. q=1 and q=4 bursts, process roles, test order, elapsed-time spacing, per-CPU strata and controls were fixed in advance. No callback was trimmed; the earlier failed zero-clock setup attempt counted against resources. Total elapsed including preparation/waiting was **49 min 34.2 s** of a 150-minute cap; pooled worker CPU **41 min 40.8 s** of an 80-minute cap; retained extent at terminal **254,874,116 bytes** of 1 GiB. Each pair had a 100-second cap.

- Stock ELF: `0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2`
- FIR-only ELF: `74f2507db4a7042a23a96b9a8c2249cc2f76d1f7f417ebb4838498d7f0578895`
- C host: `a8d8f21f1bdf4f2ebdae0f5d234a40bce849fc7afbbbfc1ed01867ab3b400726`
- Live C stage seal: `418a8a72ac1c6af1a590c98f04006135d42bd1451b04f4003059839b1d09970a`

[Native admission](../C-SUPERVISOR.md) used the exact measured executable without inner clocks. Six fresh workers each matched 40,404 callbacks and 60 public state snapshots across all five histories. The timing comparison covers these **two specific Linux x86-64 ELFs** on one shared VM. It does not measure later heterogeneous-cutoff changes, a physical computer, whole-DAW CPU, deadline headroom, power or instantaneous wake latency. Previous deterministic guest instruction counts are a different metric.

## Independent offline audit

The original native endpoint CSVs for both workers were checked independently of the acquisition and estimation modules. The audit checked physical image identity, exact control data and socket visit ledgers against Q, journal/hashes, individual real/empty clocks, resource envelopes, monotonicity, burst handoff order, full role/slot ordering and both joined normalized files. A separate computation using integer sufficient statistics reproduced **all 54 gate decisions and 320 numerical values**, including the failure; the largest absolute numerical difference was **3.6×10⁻¹⁵**.

`sufficient.json.gz` (68,415 bytes; SHA-256 `bd7c5196100378af7df3f3cfb6fd45973b93bb6604170d8e579ed5199ca743eb`) retains the per-cell CPU sums for each physical role, quantum and extra/common flag; empty/handoff controls; pair resources, hashes, public byte-comparison receipts, original decisions and terminal outcome. It omits local paths, binaries, PCM and full endpoint dumps. To reproduce the gates offline:

```sh
python3 -B replay.py
python3 -B test_replay.py
```

A successful verifier exit means the archived **no-go** was reproduced. If the complete private raw stage, Q ledgers and admitted binaries are available at the paths embedded in the original seal, `python3 -B export.py /path/to/sealed-C /path/to/new.json.gz` independently regenerates the compact artifact. Neither script starts native workloads or changes the original inputs.
