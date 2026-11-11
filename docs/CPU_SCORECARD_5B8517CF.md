# Current standalone core CPU diagnostic

## Scope and source

Three serial runs of the unchanged `cs80_source_factory_benchmark` baseline suite tested `5b8517cf8d52c70183d89a6ef5307541a5a14565`. Each selected 48 kHz, 256-frame buffers, 1,024 measured held callbacks and one second of warmup, with all 27 applicable cases (51 phase rows): eleven two-rank pairs covering all 22 tabs in Isolation and Legacy, moving tabs/pressure/wah, and the existing idle cases. Release tails use the fixture's unchanged minimum and cap. There is no quick-subset selection, changed tolerance or production modification.

The existing GNU 14.2 Release build uses `-O3 -DNDEBUG`, with the AVX2 filter batch disabled. Rebuilding the benchmark completed two steps. Commands requested CPU 8 via `taskset -c 8`; the benchmark does not record observed affinity, so this is command provenance rather than an independently measured affinity witness. CPU model is not established: `lscpu` could not read the container's absent CPU sysfs entry. This shared container is not a controlled audio workstation.

Timing covers `setControlFrame`, applicable controller events and core rendering using a monotonic wall clock. Checks, statistics and output bookkeeping are outside that per-callback interval. It does not include the native wrapper, external host, driver or actual real-time scheduling. RTF is elapsed time divided by represented audio time, not process CPU utilisation.

## Complete results

All three commands exited 0 and wrote completed manifests with 27 cases / 51 rows. All 153 rows had finite positive timing fields and zero nonfinite audio. Per-case output checksums agreed across rounds. No row or round was dropped or rerun.

| Measured group | Round 1 median RTF range | Round 2 | Round 3 | Range of per-case minimum median across three rounds |
|---|---:|---:|---:|---:|
| Isolation, eight held voices, including moving controls | 0.495621–0.609595 | 0.493229–0.591887 | 0.495439–0.593492 | 0.493229–0.591887 |
| Legacy, eight held voices, including moving controls | 0.184157–0.203290 | 0.185583–0.214247 | 0.186125–0.201373 | 0.184157–0.196301 |
| Isolation quiet idle | 0.112176 | 0.110350 | 0.111561 | 0.110350 |

The last column deliberately follows the earlier scorecard's minimum-of-three convention; it is not a fourth run or the only evidence retained. The moving Isolation row had medians 0.609595, 0.591887 and 0.593492, with 9, 13 and 4 callback budget exceedances respectively. Across all Isolation active rows, p99 RTF ranged from 0.613145 to 1.178819 and each 1,024-callback row recorded 0–22 budget exceedances. Across all 51 rows, each round recorded 69, 63 and 57 exceedances. These are observed offline wall-clock budget exceedances, not measured audio-driver dropouts.

The current diagnostic remains above the historical active target of 0.15–0.20 RTF and quiet-idle target of 0.02 RTF. It is not an improvement claim against the old source or hardware: no contemporaneous old-revision control was run. It also does not refresh the larger Source Performance/MPE/FX, multi-rate or multi-block matrices.

## Evidence and independent audit

The retained local evidence contains the exact binary hash, source/tree identity, build configuration, all command exits, metadata, raw phase rows and a reducer that requires all three complete matching case sets. Independent read-only review reproduced the summary exactly, confirmed the binary hash and checked every row and completion marker. It did not repeat timing. Raw evidence remains local; no previously denied benchmark payload is included.

This diagnostic does not qualify CPU deadlines, idle optimisations, GUI/DAW performance, live MTS-ESP, alternate compilers or all machines. It does not resolve earlier intermittent offline audio comparisons. Any proposed optimisation still needs separate exactness and cost gates.
