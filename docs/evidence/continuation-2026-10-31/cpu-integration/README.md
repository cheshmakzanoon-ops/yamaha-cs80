# Serial CPU integration measurements

All numbers are software callback timing on the same shared cloud host, not
hardware CS-80 measurements or a DAW/realtime qualification. Each checkpoint
runs the unchanged `cs80_source_factory_benchmark` three times at 48,000 Hz,
256 frames and 1,024 callbacks per held phase. Each round completes all 27
cases / 51 rows with the original warm-up, tail, and parameter-motion fixtures.
No local builds, tests or other benchmarks run concurrently with these timing
rounds. Source-only analysis and remote CI may continue.

For each matching scenario, `summary.json` selects the minimum of the three
callback median RTFs; p99 and deadline exceedances come from that same round.
RTF is callback wall-clock elapsed time divided by the audio budget. Separate
process/thread CPU counters in the original CSV include checks and are not
substituted for the callback metric. `files.json` records raw-file SHA-256
values; the benchmark metadata records source SHA, compiler and FTZ/DAZ policy.
The per-run manifest, where present, also hashes the benchmark executable.

Initial checkpoints:

| Directory | Exact source | Held-tone median RTF range | Quiet idle RTF |
| --- | --- | ---: | ---: |
| main-before | db24a47170610f81236e8829ef5536af69dfc94d | 0.614106–0.713402 | 0.132898 |
| continuation-before | c881983fffbf24ef8abe7305ee60d2864711406a | 0.658197–1.012575 | 0.104590 |
| cold-time | 46a7a5a2ce077e95b9025de81da85b3480134176 | 0.666591–0.960710 | 0.098381 |
| 01-transforms | 50930af10b871d26ac8aada23b6703d9f8d30782 | 0.638141–0.952687 | 0.094334 |

The held range covers the paired-tone rows. The moving tabs/pressure/wah row
is reported separately in each summary; at cold-time it is 1.006754 RTF.
The earlier 0.51–0.61 / 11% scorecard is historical and is not substituted
for these fresh main and continuation baselines. Shared-host variability
requires retaining all rounds and prevents attributing small differences to
a code change without qualification. CPU targets remain 0.15–0.20 RTF held
and at most 0.02 RTF idle. Subsequent exact and numerical changes add their
own three-round directories and compatibility results.

The exact-transform checkpoint preserves all four strict captures byte-for-byte.
Its moving-control median is 0.954667 RTF; held-tone p99 spans
1.005389–1.999151 RTF with 1,121 selected-round deadline exceedances.
The corresponding cold-time figures are 1.006754, 1.006498–2.205308 and
1,775. These callback timings do not establish real-time safety or the CPU
targets. Exact component tests include 199,562 checks and a detected signed-zero
cache-key mutation; see the linked production evidence and raw logs.
