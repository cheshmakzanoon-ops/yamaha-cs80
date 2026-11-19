# What the run certificate costs before a single CPU clock is compared

These are **observed predicate/operation counts, not cycle measurements, speedups or confidence bounds**. The additional observer was built from the same pinned shadow Engine after the correctness run. It increments integer counters around the actual attempts/declines, not a numerical approximation of crossing decisions. Every row matched the original shadow counts and the original stock byte-stream hash, size, PCM/readout/FP receipts and all sampled oscillator snapshots. [Raw per-scene counts](evidence/setup-counts.json).

| Eight-voice fixture | Active WSC ticks | Stock scheduler calls avoided | Attempts to mint a four-tick certificate | Successful certificates | Individual shaper-knot distance tests |
|---|---:|---:|---:|---:|---:|
| 48 kHz host, factory held, AVX2 filters | 524,288 | 283,910 | 310,913 | 71,093 | 1,484,454 |
| 48 kHz host, factory moving, AVX2 filters | 524,288 | 81,564 | 113,833 | 30,190 | 559,109 |
| 48 kHz host, Panel moving, AVX2 filters | 524,288 | 0 | 0 | 0 | 0 |
| 44.1 kHz host, factory held, AVX2 filters | 524,288 | 0 | 523,682 | 0 | 0 |

At 48 kHz held, **239,820 of 310,913 setup attempts failed**. 205,770 failed at a shaper-knot distance test. A failure executes original stock work *after* that setup effort; a successful call covers up to four ticks, not always all four if controls change. Among all ten predefined rows (including the explicitly identified duplicate held sound with scalar filters), the observer recorded **1,789,454 setup attempts, 284,276 grants, 5,867,705 scalar-language knot distance tests and 1,096,251 skipped scheduler calls**. In the normal release Engine object, GCC emits a common wrapper entry with `stmxcsr` and `fnstcw` before its control predicates. Thus Panel with changing pulse width can pay this per-tick entry and get zero avoided calls. The unsupported 44.1 kHz fixture frequently reaches setup but returns at the bitwise sample-rate gate before numeric bounds.

In that same normal Engine object, `nm -S` gives 12,063 bytes for the stock weak tick symbol and 814 + 1,094 + 11,562 = 13,470 bytes for the prototype's tick, certificate helper and stock fallback body, respectively. Those are named symbol sizes, not total emitted core size, dynamic instructions, cache misses or latency. Eight source tests are not necessarily eight machine branches after optimization. Do not subtract these unlike counts from each other to infer a speed. Together they give a concrete reason to withhold production admission despite exact audio and many genuinely skipped stock searches.

A possible, **untested** follow-up would eliminate work that can never succeed (e.g. check a setup-time explicit nonnominal-rate flag before touching FP controls) or deliberately run the original path for a few ticks after a failed certificate instead of attempting certification on every nearby tick. Such changes would be new treatments requiring fresh exactness, work accounting and independently capable native timing. They are not part of the evaluated patch.

After the three `engine.py` stages have run in a fresh work directory, `python3 setup_census.py /path/to/engine-work` reconstructs the separate counting arm, recomputes positive and negative counts, and checks its trace against the saved clean matrix. It will not overwrite an existing `setup-census` in that directory. It neither measures nor claims speed.
