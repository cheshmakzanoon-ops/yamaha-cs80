# Compiled VST3 guest work: shipping cutoff vs cutoff + final signed-zero FIR

With both real Linux VST3 modules running under **Callgrind 3.24.0**, the FIR change executed **13.71% fewer guest instructions** in a fresh quiet window and **13.43% fewer** after an audible release had settled. Eight held notes cost **0.037% more** guest instructions. These are differences in translated instruction and data-access counts, **not native CPU percentages, timings, cycles, bandwidth or real-time deadline claims**.

The shipping-cutoff control was SHA-256 `0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2`. The same cutoff plus the final Linux-only FIR was `74f2507db4a7042a23a96b9a8c2249cc2f76d1f7f417ebb4838498d7f0578895`. No reach counters, modified plugin builds, or timers were used. The frozen host counted the direct call edge into JUCE's real VST3 process function, not the host's captures, note construction or state queries.

| 48 kHz, 64 frames, four selected callbacks | Guest instructions: stock → FIR | Data reads: stock → FIR | Data writes: stock → FIR |
|---|---:|---:|---:|
| Fresh quiet | 6,626,789 → 5,718,493 (−13.7064%) | 1,500,974 → 1,307,438 (−12.8940%) | 843,526 → 844,046 (+0.0616%) |
| Eight held notes | 24,975,727 → 24,984,935 (+0.0369%) | 6,692,065 → 6,693,601 (+0.0230%) | 3,226,066 → 3,227,098 (+0.0320%) |
| Settled release | 6,750,357 → 5,844,109 (−13.4252%) | 1,537,351 → 1,344,327 (−12.5556%) | 844,960 → 845,480 (+0.0615%) |

For each scene, five fresh translated processes ran the same timeline: stock first four, a second stock first four, stock all five, stock fifth only, and FIR first four. **A/A were exactly equal; all-five equalled first-four plus fifth** for each integer counter. Six separate fresh native processes supplied stock/FIR references. Each quiet/held instance received 32 initial callbacks, then five test callbacks and an uncounted continuation. For release, eight real note-ons sounded for 1,024 callbacks, eight real note-offs arrived, and 8,192 tail callbacks ran before the five-callback window and continuation. Both quiet windows peaked at zero, held window about 0.0507, and the release fixture's earlier audible peak was about 0.0854.

**Output checks and the failed control:** Within native execution and within translated execution independently, stock and FIR matched all public PCM, callback/FP/status records and four serialized state snapshots in each scene. Across runtimes, fresh/held PCM and all scenes' state snapshots matched. The original strict verifier exits **1** on full-tail stock native-vs-translated PCM: 102,033 of 1,180,416 float words differ, first at frame 300,905, largest absolute difference 1.1866732093214387e−38, **none in the five release-window callbacks**. This cross-runtime difference is present in the control itself and is not a candidate-vs-control mismatch. MXCSR status observations also differ across runtimes (native advertises AVX-512, XCR0 231; translated advertises AVX2, XCR0 7). No tolerance was substituted for the failed bytewise check.

## Replay or collect

Run `python3 check.py` here. It verifies the frozen protocol hashes, reparses all 15 path-normalized selected Callgrind dumps, checks controls and rechecks 21 recorded arm fingerprints/outcomes. The two `*.json.gz` files preserve dumps, source module IDs, public per-file hashes and observed numerical divergence statistics, not raw samples. The mentioned word-level cross-runtime statistics were calculated from the original raw captures; hashes alone cannot reproduce them. Neither this replay nor these fixtures measure performance on a physical DAW computer.

For fresh observations with the **two pinned ELF modules**, GCC, Callgrind 3.24.0, the repository and VST3 SDK:

```sh
bash run.sh PROJECT_ROOT VST3_SDK_ROOT STOCK_PLUGIN.so CANDIDATE_PLUGIN.so NEW_DIRECTORY
# All 21 arms have been recorded before the strict PCM verifier runs.
python3 diagnose.py NEW_DIRECTORY
```

If the known cross-runtime divergence recurs, the first command exits 1 after recording all runs. Retain that status rather than treating the separate diagnostic as a replacement strict pass. The [predeclared plan](PLAN.txt), [host](host.cpp), [original strict verifier](verify.py), and [diagnostic](diagnose.py) are included. On a different binary, CPU, or runtime do not assume the numerical results transfer.
