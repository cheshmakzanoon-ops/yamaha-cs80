# Rolling oscillator endpoint: native VST3 result

The full preregistered promotion criteria did not pass. The production oscillator remains unchanged. This does not establish zero effect, an audible change, or a universal regression.

The candidate uses the oscillator's own cached endpoint as the working right-hand value in `tick()` and borrows it while the search and advance routines read it. It keeps a distinct pre-event snapshot wherever the event recorder requires one. No control law, arithmetic expression, preset, state format or sample-rate policy is intentionally changed. Baseline: `e9e8175f4278d2457279fcb8809cbba1ddb7be2b`. The [exact source diff](candidate.patch) and [predeclared native study rules](admission.json) identify the treatment and decision.

## Measured result

| Engine policy | Estimated CPU reduction | 95% whole-block interval | Claim floor | Median stock / candidate (ns per host frame) |
|---|---:|---:|---:|---:|
| Circuit (primary) | +5.45% | [-2.18%, +9.88%] | 7.41% | 23,950.3 / 22,193.3 |
| Pure CS-80 | +2.50% | [-2.18%, +6.34%] | 5.35% | 11,979.1 / 11,314.0 |
| Legacy | +2.75% | [-0.05%, +6.43%] | 2.28% | 9,845.1 / 9,497.1 |

Lower callback CPU is positive. The median ns/frame column is descriptive for the 12 comparison-arm observations each; the gain and interval instead preserve the balanced A–B–B–A block ordering. The 95% interval resamples the six complete comparison blocks, not callbacks. The Circuit lower bound must exceed both the largest absolute gain in two A/A calibration blocks and 3× the largest measured empty-clock fraction. Both other modes must separately rule out a slowdown greater than 3% by the same interval's lower bound. No outlier removal, automatic retries, selecting a different primary policy, or subtracting measurement overhead.

GNU C++ 14.2 Release/PIC, INTEL(R) XEON(R) PLATINUM 8573C, Linux x86-64; fresh native VST3 diagnostic host processes, 48 kHz, block 64, realtime processing mode, 16-note input cohort (Pure internally caps polyphony). Four short histories, then 65,536 warm-up frames and 524,288 measured frames per slot; per-thread CPU clock around the real VST3 process callback. Matched build inputs and audited compiler/linker output identified a single changed Engine object in the core archive and its VST3 binary; the module metadata is identical. [Build audit](pair.json). The executor was reserved, but underlying shared-host effects, CPU frequency, and other machines were not controlled.

## Audio / state and timing coverage

* Independent untimed real-plugin comparison across 3 policies × 3 sample rates (44.1/48/96 kHz) × 2 process modes × 2 note counts × 3 blocks (64, 256, 1024): **108 pairs, 432 matched A–D segments, 1,769,472 stereo frames / 3,538,944 sampled float values**. Complete encoded render observations and post-A component-state digests matched within each pair, along with the recorded parameter histories (2,378 native entries). These exercised positive-signal outputs, not silence checks alone; the external audio input was inactive.
* An executor interruption between untimed cases did not trigger an observation retry: 86 completed receipts were re-decoded against the original plan, exact request, process record, current sealed module identity and saved result before the remaining 130 untouched slots ran. No timed run was included in that resumed grid.
* All 96 native timing slots completed under the original caps, with exact six-segment PCM within each policy across arms and repeats. The independent raw audit checked **909,312** callback durations, **73,728** clock-pair samples, recorded post-A component state, read-only parameter history, and the complete frozen schedule. No real-DAW, macOS/Windows, deadline, full host CPU %, or product release claim follows. Five focused stock project CTests and the [component exactness / inconclusive cost experiment](component/RESULT.md) are also retained.
* The output does not certify arbitrary inputs, internal state after all possible histories, other compilers or floating-point modes, or the product's absolute CPU target. It is not a hardware-circuit fidelity claim.

## Recompute without loading a plug-in

```sh
python3 docs/experiments/oscillator-rolling-endpoint/verify.py
```

The compressed export contains all six callback/clock arrays for every timed slot, both arms' untimed render/state hashes, source-derived protocol/PCM hashes and segment-level quality, slot identity, module/plan digests and independently computed statistics. [Manifest](manifest.json) pins the stored bytes. It intentionally omits raw private loader maps, raw PCM, machine names and installation paths. Successful local raw receipts were validated before export; detached hashes cannot independently reproduce native execution. The component CSVs retain every observed row in original order. The component probe and its comparison fixture are archived as source; the exact projected header may be reconstructed by applying the patch to the listed baseline. The archived timing is not an invitation to rerun until it looks favorable.
