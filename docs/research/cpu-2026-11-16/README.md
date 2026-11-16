# CPU research status and source-only FIR oracle

**No production DSP, tests or CMake changes in this documentation package. CPU below 2% has NOT been demonstrated.** This package records useful results without presenting rejected or unmeasured changes as shipped optimizations.

| Work | Verified result | Limitation / decision |
|---|---|---|
| Exact stereo FIR candidate | Focused generic/native/OFF/unsafe oracle and full generic/native/OFF corpora qualified on recorded host. Bounded actual callback instruction count fell 2,349,056 Ir in each of two 16-callback cases. | Native CPU benefit UNESTABLISHED. Identical-binary calibration failed, so candidate was never CPU-timed. No retry/promotion. |
| Finite residual helper | Fixed 2,048 synthetic events:1,844,648→1,417,177 simulated guest instructions (−23.17%), fewer reads. | NOT Engine/callback/native time. Writes and simulated cache misses increased; simulator FP-status/control limits disclosed. No CPU benefit claim. |
| Two-refinement reciprocal tanh | Local max sampled error 8.10e-15; conservative local bound 6.115e-13. | REJECTED by feedback-stage screen: maximum normalized error 0.4973 versus the 1e-6 budget. One-ulp slopes crossed original eligibility limits. No integration or tolerance repair. |

## Provenance

Main base verified remotely: 5836041138bc1708c3fd2d4240cf63a3e1def4b3. FIR tested experimental source: 78cfac08532d7cd04965ad2b5bd47086742f664d; patch against that baseline is included strictly as research, not applied. Residual finite helper: 7435dbdfa008f671671290e4785f40512c163b20 and scoped instruction evidence d946693ac0ba9b670b87a33ba0396beca275c781 are experimental lineage identifiers, not release claims. All results refer to these frozen versions, not later development.

Original portable FIR archive SHA256 a16ce1674f76377b0de7077b22356362220b3dfe171d9d5793d4b3da99481d7b (142,850 bytes) remains a separate historical artifact. This source-tree package is a selected smaller derivative, not that archive. MANIFEST.json hashes every included file except itself. Recorded historical paths are provenance only, not installation paths or turnkey commands. Binaries, dependencies, large Callgrind dumps and PCM/state corpora are deliberately omitted; retained summaries cite hashes/limits. This package cannot independently regenerate every historical result without the listed original inputs.

## Reproduce the focused FIR oracle without touching production

Linux x86-64, strict GCC/G++ C++20 and Python3 are needed for this fixture. Run from any directory:

```sh
python3 /path/to/docs/research/cpu-2026-11-16/fir/run-focused.py --output /new/empty/output-directory
```

The runner refuses an existing output directory and compiles only copied standalone fixture sources there. Default generic and explicit-OFF arms compare original scalar outputs to candidate outputs plus full MXCSR/x87 records. No repository source is patched and no benchmark executes. Optional `--native` and `--unsafe` add those correctness-only own-profile baselines; unsafe is not a production build recommendation. `--cpu N` optionally pins to an allowed Linux CPU; otherwise no affinity assumption. `--cxx` chooses a compiler executable, not arbitrary shell syntax. Runner is a portability adaptation of the qualified original recipe. Its default generic and OFF profiles were executed locally on CPU 5 and passed all 115,520 records per profile; see fir/runner-verification for logs and hashes. This verifies the new runner on the recorded cloud host, not a new target host or the optional native/unsafe runner flags. Actual results, compiler version, commands, hashes and errors must be retained.

The fixture requires qualified strict GCC x86-64 for the accelerated FIR route (SSE2 packed double lanes). Broader Engine qualification additionally used AVX2. Native flags on the original FMA-capable host triggered scalar fallback; a different nonFMA host may still admit. Clang/MSVC/ARM, FMA-enabled, unsafe and explicit-OFF contexts are not evidence of FIR acceleration. Check actual selected-path count and machinecode; CPU capability alone is not enough. The runner records rather than presumes native admission. Full Engine corpus, FP fallback, sanitizers and actual-path qualification remain prerequisites before any production consideration; passing this standalone test is insufficient.

## Performance boundary

The closed cloud calibration is terminal. Do not rerun until favorable, relax thresholds or label instruction reductions as CPU speed. A fresh stable-host experiment needs a separately frozen plan, matched compiler/flags/source, quiet CPU, exact workload/PCM gates and valid identical-binary calibration before candidate timing. Report callback-thread CPU divided by rendered audio duration, not whole-system or DAW CPU percentage. Hardware, voices, quality/internal rate, effects and denominator must be explicit. Additional cross-platform/CI and final-source qualification is required before any candidate publication.
