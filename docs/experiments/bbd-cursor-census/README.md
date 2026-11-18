# BBD cursor repetition: stock-only census

**Observed repetition, not an optimization or speed claim.** No DSP cache or
candidate arithmetic was implemented. This checkpoint contains no timing,
native-plugin qualification or production change.

Shipping `e9e8175f4278d2457279fcb8809cbba1ddb7be2b` performs seven BBD delay reads
per internal sample. When adjacent reads finish on the same unwrapped integer
cursor, their four source samples and three cubic coefficients are identical.
The observer records this after the **original coefficient calculation**, without
feeding any saved value back to DSP. Every original operation still runs.

## Reach and parity

Factory held, factory moving, Panel moving and Pure held each use eight held
voices, 48 kHz, 64-frame callbacks, 2x internal processing, drift 0.37 and dispersion
0.18. Every callback reapplies the normal frame before MIDI. The 384 callbacks plus
one continuation give these identical counts in each scene:

| Window | Shared seven-tap passes | Eligible adjacent repeats |
|---|---:|---:|
| Startup, blocks 0–7 | 1,024 | 3,072 |
| Middle, blocks 8–255 | 31,744 | 95,232 |
| Settled, blocks 256–383 | 16,384 | 49,152 |
| Continuation, block 384 | 128 | 384 |
| Total | 49,280 | 147,840 |

Every pass has exactly **three repeats**, at zero-based tap ordinals 2, 4 and 6.
Across all four scenes: 197,120 passes, 1,379,840 original tap calculations and
591,360 eligible repeats; zero owner/ordinal errors. One shared seven-tap pass
feeds both independent BBD chains. Counts and possible savings are **not doubled**.

All four fresh stock/observed comparisons match **5,203,660 bytes per arm/scene**:
full callback PCM and FP control/status/errno, public voice/routing/preparation
observations, and explicitly serialized BBD DSP state including sample/stamp
buffers, phases, cursors, coefficients, both chain histories and RNG states.
Normal and observed arms use the same driver object. Instrumentation remains
separate from the shipping archive.

Eligibility here requires equal cursor and raw input/coefficient bits, finite
zero/normal floats, stable x87 CW `0x037f`, and MXCSR control `0x1f80` or `0x9fc0`.
These are census predicates, **not proof of a prospective cache's exactness**.
The actual fixture uses `0x9fc0`. The fixture disables chorus, so “moving” means
voice expression, not BBD clock modulation. Pure also runs this BBD source path.
Chorus-on, unusual rates, nonfinite/exceptional inputs and trap fallback remain
unqualified. Pointers, allocator capacity/padding and unrelated private Engine
fields are not part of the serialized state.

## Existing profile context, not a forecast

The [earlier stock profile](../shipping-core-profile/README.md) has these six
physical-function **self** sample shares for `BbdChorus::process`:

| Scene | Requested sampler period | BBD / total samples | Self share |
|---|---:|---:|---:|
| Factory held | 1,999 us | 59 / 4,970 | 1.187% |
| Factory held | 5,003 us | 49 / 3,586 | 1.366% |
| Factory moving | 5,003 us | 32 / 3,522 | 0.909% |
| Factory moving | 1,999 us | 32 / 4,994 | 0.641% |
| Panel moving | 1,999 us | 42 / 4,997 | 0.841% |
| Panel moving | 5,003 us | 29 / 3,544 | 0.818% |

The **0.641–1.366% range is descriptive**, not a confidence bound or CPU-saving
prediction. External libm self PCs were not assigned to their callers. Three of
seven repeated polynomial builds affect only a subset of BBD self work; a future
implementation would add guards, comparisons and copying. No speed forecast is
supported, and bypassing an off BBD would incorrectly freeze its future histories.

## Offline checks and optional replay

```sh
python3 -B verify.py
python3 -B -m unittest test_verify.py
python3 -B verify.py --repo /path/to/local/checkout
```

Checks read small receipts/package files; the optional checkout check authenticates
all 193 stock source files and reconstructs the one observed header in memory.
No builds, DSP, profiling, raw-capture hashing or network access occurs.

A **separate explicit replay** can use the same local checkout and a fresh output
directory:

```sh
python3 -B replay.py --repo /path/to/local/checkout \
  --out /path/to/new-census --execute
```

This builds the three-member stock core and checks its recorded object/archive
identities before compiling the observed Engine and running eight untimed
processes. `--stock-core /path/to/libcs80_core.a` may instead supply the exact
hash-checked archive. GNU C++ 14.2/Linux x86-64 is the recorded build context. The
193-file source and
[committed workload helper](../lp-cross-lane-reuse/cost/cost.cpp) must match their
pinned identities. The helper's original timing main is renamed and never called;
the census entry only invokes `Work::step` and diagnostic observations.

The portable entry point was offline/syntax checked, **not rerun when packaging**.
It refuses existing output, uses bounded subprocesses, and stops on any mismatch.
The completed census is the original recorded run, not an unperformed replay.
Binaries and raw captures are omitted; their saved hashes do not independently
prove those captures. The external manifest hash anchors this package.
