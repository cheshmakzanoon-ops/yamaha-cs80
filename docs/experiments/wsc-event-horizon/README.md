# WSC four-tick interior: trace-exact component and standalone Engine prototype

**Research only. No production header, quality preset, CMake option or plugin has changed. No candidate native CPU timing or speedup is claimed.** This experiment changes a different piece of the oscillator from the earlier endpoint-copy, knee lookup, decay reuse, staged-node and vector-width candidates. It avoids asking the event scheduler a question when four unchanged, nominal-profile oscillator steps are conservatively certified to stay in the same open segment. Every admitted physical update, output, residual and diagnostics readout still advances **one sample at a time**. See the [certificate derivation, guard and limitations](PROOF.md) and the [isolated patch](candidate.patch).

### Where the certificate actually applied

A real standalone shipping Engine, not just a synthetic loop, was exercised under four separately compiled arms: stock, normal prototype, instrumented prototype recomputing the actual stock boundary search at *every* fast decision, and explicit prototype-OFF. Every arm reads the same immutable Git archive `db02cd8fa56892c6ea2cf7df084bcf190204968e`; except for inert trace-friend access, the only candidate source change is the oscillator header. [File/object identities](evidence/provenance.json). GNU 14.2, x86-64 Linux, strict -O3 Release/PIC. Each scene is 256 × 64-frame callbacks, the normal host control frame before fresh MIDI; eight sounding notes where specified.

| Scene | Internal / filter path | Skipped stock root searches / active WSC ticks |
|---|---|---:|
| Factory held | 96 kHz / AVX2 | 283,910 / 524,288 (54.15%) |
| Factory moving MPE + effects | 96 kHz / AVX2 | 81,564 / 524,288 (15.56%) |
| Panel moving MPE + effects | 96 kHz / AVX2 | 0 / 524,288 |
| Pure, held | 96 kHz / AVX2 | 284,237 / 524,288 (54.21%) |
| Legacy held / quiet factory | no active WSC tick | 0 / 0 in each |
| Release, hard stop, idle, wake, policy switches, reset, re-entry | 96 kHz / AVX2 | 162,630 / 302,560 |
| 44.1 kHz host held / 96 kHz host moving | 88.2 / 192 kHz internal | 0 / 524,288 in each |
| Factory held, same instrument as first row | 96 kHz / scalar filter | 283,910 / 524,288 |

**1,096,251 certified decisions in the fixed grid; zero stock-root/physical-end disagreements.** Some fixtures intentionally repeat a workload with a different filter path. This is an operation/eligibility count, not a percentage of whole callback work removed. A [separate measured setup census](SETUP.md) counted **310,913 attempts and 1,484,454 knot-distance tests** to avoid those 283,910 held-factory searches; most attempts did not grant a certificate. Certificates also cost FP-control reading, bounds, and state maintenance. The full callback still does its normal work. Four-arm traces matched **byte-for-byte**, 37,732,960 bytes per arm across ten distinct slots: every stereo sample, callback FP status/control/errno, public source/filter/voice readouts and periodically all 16 internal oscillators including private right nodes, full continuation, residuals, and all 64 retained diagnostic event slots. The checks compare selected private state; they do not serialize the entire Engine object or prove every possible history. Raw trace bytes were matched locally before discarding; [the per-scene hashes and all arm counts](evidence/engine.json) remain. A first exploratory run contained a concurrent but identical-in-all-arms control-frame change; its trace receipts matched the later immutable run, and it is not counted as a second qualifying result.

An [independent entry-prefilter follow-up](entry-prefilter/README.md) avoids reading FP controls for impossible rate/control cases, with unchanged admitted steps and trace hashes. This helps the structural overhead in moving Panel or nonnominal-rate fixtures; it removes only 558 of 524,288 control-read pairs in the held factory primary and does not reduce the original costly failed-knot checks. No CPU speed claim follows.

### Independent controls

* 114,688 paired direct ticks at held, moving/pitched, gate/threshold, near-knee, clock-switch, idle/reset/restore/copy and unsupported configurations. The separate shadow build registered 573,474 full checks with **40,832 positive decisions and zero disagreements**. A further 8,192 restored states covered targeted reset distance, comparator/fold/knee edges, tiny inputs, sNaN elapsed/residual and subnormal residuals, giving 98,304 ticks, 491,521 shadow checks, and 21,576 admissions with no mismatches. Normal public output and right cache are checked independently of the shadow witness.
* A mutation removing the fold/comparator/knee/turn guards produced **1,932 actual contradictory stock boundary decisions** and 306,756 failed assertions. Separate compiler-native/FMA (nonqualified) and force-OFF arms each matched the corresponding reference in 573,485 checks, with no admissions. No tolerance relaxation.
* Actual GNU/Linux SIGFPE tests loaded the compiled Engine object's stock/candidate tick symbols. Three signaling residual lanes and two elapsed-state seeds (valid/invalid endpoint cache) produced **10 real faults across the A/B calls**. Full 5,984-byte interrupted object, signal status/errno and checked continuation matched. Matching stock/stock controls were run separately. Code addresses and offsets differ between implementations; equal fault PCs or general trap equivalence are not claimed. Constructors and rates are unchanged; `sizeof`=5,984, alignment=8 in both, with the run counter using a byte adjacent to the cache-valid flag on this compiler.
* Four unchanged project test executables on the normal prototype passed: reconstructed oscillator (1,418,466 checks, no timing mode); oscillator promotion (34); continuous voice circuit (151,578); and continuous voice audit (33,962). These are not the full project CTest suite, a native DAW test, or new hardware-fidelity evidence.

### Replay without changing shipping code

`python3 -B verify.py` verifies the saved local receipts and manifest; it neither renders nor benchmarks. For new direct comparisons on Linux x86-64 with GNU C++20 (the recorded native fallback test expects a host enabling FMA under `-march=native`), use:

    bash run-direct.sh /path/to/checkout /path/to/new/direct-work

A new **untimed** full Engine matrix, including the shadow, must consume the pinned Git commit rather than a mutable working tree:

    python3 engine.py /path/to/checkout /path/to/new/engine-work --stage prepare
    python3 engine.py /path/to/checkout /path/to/new/engine-work --stage build
    python3 engine.py /path/to/checkout /path/to/new/engine-work --stage run

The matrix requires real CPU/OS AVX2 selection for the indicated rows, Linux x86 FP state capture and a GNU/PIC build. Source trees, compiler executables and temporary space are local. No dependency downloads or security-setting changes. The included patch is for research reproduction only. Native-plugin A/A and positive control qualification on stable hardware would be needed before claiming a CPU saving; candidate throughput was not timed. No low-CPU threshold, product/host CPU%, deadline, DAW/platform compatibility, or 1:1 analog result is inferred.
