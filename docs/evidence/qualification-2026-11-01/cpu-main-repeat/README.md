# Original-main repeat: three stock factory rounds

This retains the new session's unchanged original-main timing measurement before
joining the already-published qualification history. It is diagnostic core timing,
not hardware measurement, a DAW result, or a hard real-time guarantee.

Audio source is `db24a47170610f81236e8829ef5536af69dfc94d`. The executable records
`5d6bfdb54a781308120f44e22801b542c228c6ff`, whose sole change from that main is an
unreferenced prospective test-tolerance header. No core, tool, build setting or
production audio difference exists between those two revisions. That redundant
preregistration is retained on its original branch and is not an audio change.

Build: GCC 13.3, C++20, Ninja, Release; plugin disabled and core tests enabled.
Each round runs the stock `cs80_source_factory_benchmark` with `--rate 48000
--block 256 --callbacks 1024`. All three rounds are sequential and complete all
51 rows. The command manifests preserve exact arguments. Team compiles/tests
were paused during these measurements; scheduling load outside this team's work
remained variable on the shared executor.

## Minimum of three per-case medians

For each case, select the lowest median RTF among its three rounds. Report that
same selected round's p99 and deadline exceedances; do not combine the lowest
median with another round's p99. Every raw row and all three medians remain in
`min-of-three.json` and the unmodified CSVs.

| Original-main case | Median RTF | Selected-round p99 RTF | Deadline exceedances |
|---|---:|---:|---:|
| Circuit, 11 held two-rank tone pairs, 8 voices | 0.5939466–0.7953424 | 1.0436361–28.2118571 | 581 / 11,264 |
| Circuit, quiet idle | 0.1331263 | 0.6119889 | 7 / 1,024 |
| Circuit, moving tabs/pressure/wah | 0.7522414 | 1.5335258 | 79 / 1,024 |
| Legacy, 11 held pairs | 0.2452954–0.3111662 | 0.4555044–2.1125301 | 29 / 11,264 |
| Legacy, quiet idle | 0.0206286 | 0.0803589 | 0 / 1,024 |

The circuit idle median is 13.31% of one real-time audio budget. The held
0.15–0.20 RTF and idle 2% targets are unmet at this original-main revision.
The very large p99 excursions, including in Legacy, preclude a claim of an
uncontended before/after speedup from these measurements. Later immutable
same-runner checkpoint measurements must be reported separately.

`sha256.json` seals the retained raw files. These measurements do not replace
any existing compatibility, physical accuracy, continuation or mutation gate.
