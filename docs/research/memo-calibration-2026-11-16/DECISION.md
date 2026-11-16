# Memo research: calibration unusable, candidate speed unmeasured

The fixed request02 experiment stopped at active identical-baseline calibration. No memo candidate cost comparison, low-load calibration or B/A default-OFF cost comparison ran. Do not retry, retime, relax gates or promote this candidate from these results.

Four fixed A/A pairs (eight runs, 288 rows) reproduced exact output/work inventories. The second pair exceeded the predeclared ±2% aggregate limit:

| Aggregate | Pair 1 | Pair 2 | Pair 3 | Pair 4 | Maximum absolute deviation |
|---|---:|---:|---:|---:|---:|
| Total callback CPU/audio | 1.001973 | 1.020074 | 1.005086 | 1.004101 | 2.0074% |
| Source static held, 3 cases | 0.998937 | 1.044140 | 1.010007 | 1.012460 | 4.4140% |
| Isolation static held, 3 cases | 1.001437 | 1.045729 | 1.009537 | 1.009097 | 4.5729% |

All per-row calibration ratios stayed inside the separate ±10% row gate. That does not override the aggregate failure. Ratios compare identical baseline executables, not candidate versus baseline. These are callback-thread CPU/audio metrics, not system/DAW CPU or evidence of the user's below-2% target.

## Qualification and provenance

The approved exact runtime libm binding passed. All three fresh arms built; nine Engine/history/dispatch corpora matched byte-for-byte; focused/lifecycle and scheduling/batch tests passed. The three retained benchmark executables were independently rehashed. Actual raw Engine/helper → powf PLT/GOT linkage and before/after runtime binding receipts matched the qualified implementation. Candidate reachability was verified separately using counted copies: their four qualification renders and incidental clock columns are **not cost measurements** and must not be pooled with A/A data.

Baseline: 5836041138bc1708c3fd2d4240cf63a3e1def4b3. Candidate source: 46812652364337aec1c8d51443427d1b63df5351. Published request02 tooling: b82dfb83461df227f500d2e17790340859e14cbe. Protocol SHA256: 1afb4de7f5d8f7ca621692cf9f4c05a0c1f2cf7119280ee15aeb0c7107148bcc.

[Request02 run43815271850](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/43815271850). Full original artifact ID14201983146, SHA256202b22f7300f0cbcf37c1db3f24e3765b4b67e44edcde4e9197b0dd4595f2687, is preserved unchanged outside this compact package. The separate failed request01 artifact ID14201691687, SHA2565b773e510e7d707100a4066f21c61d6acca0cfa760a155139bc18295891c881a, remains preserved separately: its patch-inventory tooling failure occurred before calibration. Request02 repaired that exact inventory guard, not any numerical threshold.

## Conclusion

Numerical qualification is not a native performance result. This fresh GitHub runner was not sufficiently repeatable for this frozen experiment; prior usable calibration on another run does not guarantee future stability. Memo benefit and default-OFF/bypass costs remain unestablished. Main DSP remains unchanged. Independent audit agrees with the terminal stop.
