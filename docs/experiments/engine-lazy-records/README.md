# Two smaller engine records: exactness and work

These are **independent standalone core comparisons** to `9208eb6`, not a sum, a whole-plugin result, a native timing test or a physical DAW CPU claim. Both retain the existing source-policy clocks, internal rate, latency, fallback and arithmetic.

* **Lazy modern-voice inputs.** An inactive modern modulation matrix returns before consuming any per-voice input. Materialize that record inside a small outlined function, only on a live route. `beginSample` still advances transport, including when there are no routes; a live matrix receives the prior sample's rank envelopes and unchanged gate, strike, channel state, circuit order and global-source eligibility. The original statements remain in unqualified compiler/math builds. A first **inline** guard was rejected: it matched observations but increased translated work in all nine GCC 14 cases (+0.70% varied held).
* **Compact uniform-receiver staging.** For the already-qualified equal-drive solve, retain two distinct rank results until both solves succeed. Then expand their voltages, Q and gain to the eight receiver readouts in each rank. No changed accumulation, formulas, rejection or cross-sample memo; failure on rank II cannot leak a partially updated rank I into the previous public result. The public scalar solver and the existing build/environment/input certificates have not been broadened.

## Selected four-render instruction counts

Percentage change in translated instructions; negative means fewer. Work is measured over the core render, not the non-render parts of the audio callback. Percentages need not track physical execution time.

| 48 kHz / 64 frames | Lazy, GCC 14.2 | Lazy, GCC 13.3 | Uniform, GCC 14.2 |
| --- | ---: | ---: | ---: |
| Fresh factory quiet | −0.566% | −0.643% | −3.750% |
| Eight equal held | −0.769% | −0.801% | unchanged |
| Eight varied held | −0.765% | −0.789% | unchanged |
| Factory with three live routes | +0.048% | −0.002% | unchanged |
| Panel with three live routes | +0.040% | +0.003% | unchanged |
| One held | −0.365% | −0.396% | unchanged |
| Pure, eight held | −0.757% | −0.758% | unchanged |
| Legacy, eight held | −1.628% | −1.107% | unchanged |
| Live route edits and circuit reuse | −0.273% | −0.316% | unchanged |

For the compact transaction the quiet arm also recorded **5.533% fewer guest reads and 18.086% fewer writes**. The other eight selected windows did not change any of the three counts. The lazy helper is not free in every active-routing scene: the two live GCC 13 windows have additional reads. The [machine-readable receipt](core-work.json) retains all Ir/Dr/Dw counts, observed-state hashes and controls. These short windows are not representative of every preset or CPU.

## What was checked

* Actual same-build pairs on strict Debian GNU 14.2.0 and 13.3.0, Linux/x86-64, normal Release/PIC, same host fixture and companion core objects within a pair. Compact staging is internal to the existing GCC 14.2-only uniform solver. Stock-only replications and independently counted `4 + fifth = 5` controls matched in the quiet, varied, actively routed and route-edit scenes for both lazy builds; the independent compact comparison repeated quiet and varied.
* In [the core probe](probe.cpp), nine native scenes per pair gave **5,845,680 identical bytes** of PCM, observed engine fields, errno and FP state. The translated histories were independently compared within their runtime, not presumed equivalent to native FP behavior. The [observer](../wsc-terminal-staging/observe_engine.h) is explicit about its field roster; this is not a complete memory dump. An eight-voice chord uses two different velocity fixtures. The route-edit scene enables then withdraws the last route, releases and reassigns notes during bypass, and re-enables it during the measured interval.
* The existing [22-case core interruption fixture](../wsc-terminal-staging/render-traps.cpp) delivered seven real SIGFPE in each arm per pair. Its 17,019,552 packed observation bytes, including surviving partial state, FP observations and four masked continuation samples, matched within each pair. The [six longer render histories](../synchronized-thermal-bank/trace.cpp) additionally matched 1,249,792 bytes per scene on GCC 14 for each change: quiet/held/moving, release/wake with rate/policy changes, panel and Pure.
* A deterministic 65,536-case directed differential for the compact transaction checked 27,337 rejected and 38,199 accepted evaluations, all 832 bytes of the selected output ABI, boolean return, MXCSR, x87 status and errno. It covers two rank loads, clamping/saturation, both signed zeros, denormals, infinities, signalling/quiet NaNs, all six sticky flag bits, and independent DAZ/FTZ settings. The candidate matched both the immediate prior uniform implementation and the independent public scalar solve. The [standalone check](uniform-check.cpp) reruns the latter without any old binary.

## Re-run the checks

These tests are diagnostic, use no network, and need no changes to the shipping DSP. On the intended strict GNU **14.2 x86-64** build, from repository root:

```sh
g++ -std=gnu++20 -O3 -DNDEBUG -fPIC -Isrc \
  docs/experiments/engine-lazy-records/uniform-check.cpp -o /tmp/cs80-check-uniform
/tmp/cs80-check-uniform
```

To repeat the core work study, build each source arm separately using the same compiler and include tree. Link `core/Engine.cpp`, `core/FactoryPresets.cpp` and `core/Ig00156Batch.cpp` as Release/PIC; only the last TU additionally takes `-ffp-contract=off`. Compile the probe with the same flags plus `-fno-access-control` **only for the private observer TU**, `-Isrc`, the observer include path `-Idocs/experiments/wsc-terminal-staging` and Valgrind's public headers. Link its object first, then the three unchanged-policy core objects, `-no-pie`. The probe accepts a scene index 0–8 (table order), a measurement selector (0 = first four, 1 = five, 2 = fifth only), and an output filename. Each launch starts fresh and executes 32 preparatory and five observed renders. Compare complete output files with `cmp` under the same runtime. For guest work:

```sh
valgrind --tool=callgrind --instr-atstart=no --collect-atstart=no \
  --simulate-cache=yes --callgrind-out-file=case.cg ./core-probe 2 0 case.bin
```

Use the **totals** line of the emitted `case.cg.1` part for Ir/Dr/Dw, not process-exit totals or a simulated cache timing model. The static receipt is not independent authentication of omitted binaries; a rebuild is a new experiment.
