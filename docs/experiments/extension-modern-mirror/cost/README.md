# Extension modern-mirror cost: no reliable admission

**The complete, predeclared 80-slot one-shot finished, but its controls failed. No reliable numerical cost reduction is admitted.** Every observed slot and all ten failure reasons are retained. This was a new isolated-method experiment, not a rerun or reinterpretation of a whole-Engine study.

The measured operation was the real native `cs80::ModernLayerState::mirrorToEngine(first, second)`, compiled with the normal GCC 14.2 Release/PIC options. Candidate V1 avoids redundant normalization of an already-sanitized primary configuration while keeping rebuild, transport and fallback. It was linked from the genuine native wrapper object, not a substitute function. V2 was excluded.

## Scope: settled, hot method only

Each process prepared two independent Engine pairs: a main sixteen-voice pair and a separate known-work shadow pair. Each Engine held the same eight-note chord, with 48 kHz / 64-frame full controls and 16 real warm rendering callbacks. This was not a sixteen-distinct-note native-plugin workload. Preparation, configuration changes, renders, validation and PCM continuation were outside the timer.

After one genuine post-render mirror, the timed loop invoked the same method **131,072 times on already-mirrored owners**. Therefore `transportSamples` was zero in the repeated loop, unlike a normal first-post-render callback. This measures hot repeated-method plus loop/call cost. It does **not** measure callback-separated, cold, edited-state, fallback, full-plugin, DAW or deadline performance. Loop counts follow the retained compiled control flow; they are not an instrumented per-call counter oracle.

Two fixed scenes were used: canonical default modern configuration, and three canonical 32-point MSEGs with all sixteen routing slots and nonzero amounts. See the exact setup in [`sources/host.cpp`](sources/host.cpp). The host source, callback fixture, normal compiler arguments and final-loop listings are included; original binaries and archives are deliberately omitted.

## What failed

Per scene, three identical-physical-stock shams had an individual limit of 3% absolute apparent effect and a mean-log effect limit of 1.5%. A/B are collection labels; all sham executions used the **same stock image bytes**.

| Scene | Individual sham apparent B/A effects | Mean-log sham effect |
| --- | --- | ---: |
| Default | -4.010%, -6.724%, -0.481% | **-3.772%: fail** |
| Dense | -2.870%, -9.254%, -11.140% | **-7.822%: fail** |

Four individual shams and both scene means failed. All four known-work controls also failed the frozen extra/ordinary ratio range **1.08–1.18**. They added one real stock mirror on the separately warmed shadow per eight main calls, exactly 16,384 extra method invocations.

| Scene | Extra side | Observed extra/ordinary ratio | Result |
| --- | --- | ---: | --- |
| Default | A | 1.753963 | Fail |
| Dense | A | 0.976627 | Fail, wrong sign |
| Default | B | 1.396705 | Fail |
| Dense | B | 1.076405 | Fail |

All empty-clock limits passed: the largest individual empty-clock cost was 0.157351% of its process's timed region, and the largest three-probe mean was 0.053024%, below the strict 0.5% and 0.1% bounds. This does **not** rescue the failed shams or known-work controls, or establish why they failed. The run cannot separate measurement-condition effects from control/context effects. No threshold was changed afterward.

## Descriptive observations only

There were five independent four-process comparison batches per scene. Iterations are **not** independent replicates. Candidate measurements were lower in each comparison batch, but the failed controls prevent a reliable quantitative admission.

| Scene | Stock batch means, ns/mirror | Candidate batch means, ns/mirror | Median paired stock-minus-candidate difference | Range of paired differences |
| --- | ---: | ---: | ---: | ---: |
| Default | 323.987–337.954 | 82.921–95.410 | 237.658 ns | 228.577–253.820 ns |
| Dense | 568.546–617.287 | 268.362–295.399 | 321.888 ns | 284.680–327.524 ns |

At 48 kHz / 64 frames, 750 assumed calls per audio second would arithmetically scale these median paired differences to **0.178243 and 0.241416 ms per second of audio**, respectively. Those are **unadmitted hot-loop extrapolations**, not measured plugin savings or CPU percentages. The paired-difference median is not generally the difference between independently taken marginal medians.

The one-shot completed all 80 fresh process slots in **31.654395 CPU seconds / 31.980071 wall seconds**, within its 120 CPU / 180 wall limits. There was no timing retry, replacement, numerical early stop or excluded observation.

## Replay, without running DSP or timing

Python 3.9+ standard library only:

```sh
python3 -B replay.py
python3 -B replay.py --self-test
```

An exit code of zero means **the recorded evidence and frozen failed-admission result replay correctly**, not that performance was admitted. The verifier checks the complete fixed order and role mapping, image identities, all raw clock intervals, process/whole budgets, original stdout reconstruction hashes, count conservation, expected public PCM/config/transport/FP identities, the normalized ledger and the exact frozen calculation functions. The optional software-only negative tests reject missing/reordered/altered records and exercise the original gate with invented inputs. Neither command builds, loads native code, renders or reads a measurement clock.

- [`PROTOCOL-frozen.md`](PROTOCOL-frozen.md): exact premeasurement procedure and thresholds.
- [`plan.json`](plan.json): all 20 ABBA/BAAB batches and 80 slots, including every sham and known-work mapping.
- [`records.jsonl`](records.jsonl): all raw observed integer timings, three empty-clock controls per process, public-state hashes and process observations, normalized without exclusions.
- [`analysis-frozen.json`](analysis-frozen.json): the exact original complete analysis, including every process, batch and failure.
- [`terminal.json`](terminal.json), [`ledger.jsonl`](ledger.jsonl): completion and preserved ordering.
- [`sources/study-original.py`](sources/study-original.py): archival collector/analyzer source. It is **not** the offline replay entry point and is not an invitation to rerun the consumed study.
- [`frozen_math.py`](frozen_math.py): verbatim pure analysis functions extracted from that source, without collection or clock code.
- [`provenance/`](provenance/): omitted-binary identities, exact compiler arguments with named path placeholders, native setter-selection receipts and uninstrumented loop listings.
- [`untimed/`](untimed/), [`countercontrols/`](countercontrols/): the eight original untimed public-output parity receipts, 17 pre-run software countercontrols, and the retained software-fixture development correction. Export did not rerun any DSP test.

## Provenance and limits of portability

The original plan, terminal, analysis, host source and collector source are preserved byte-for-byte and pinned. The observed C++ stdout can be reconstructed exactly from each retained ordered `observed` object using compact JSON plus a newline; each original stdout SHA-256 is checked. Clock integers are never rounded. Host paths and authorization digests are removed, and process IDs are replaced by ordinals preserving their distinctness. Original private receipt hashes identify the omitted originals; hashes alone do not prove omitted bytes.

The actual stock native link map selected `ModernLayer::setConfig` from `ModernLayerState.cpp.o`, as do these isolated executables. The trusted helper is byte-identical to the exact-normal-core-flags object used for the native V1 qualification. The normal wrapper flags include `-O3 -DNDEBUG -std=gnu++20 -fPIC -fvisibility=hidden -fvisibility-inlines-hidden`; complete definitions, includes and link order are in `provenance/build-commands.json`. Named placeholders denote the pinned stock/candidate source roots, normal native build, JUCE modules, system prefix and dependency sources, not public download links. `sources/host.cpp` is the unchanged file historically compiled as `src/host.cpp`.

The source baseline binding is recorded in `provenance/source-pins.json`; the candidate patch and separate correctness work are in the parent experiment directory. Rebuilding these sources would be a **new** build, not proof of historical execution or permission to repeat this one-shot. This export independently checks the saved records and arithmetic. It does not independently authenticate omitted binary execution, recover raw PCM from hashes, or provide a new full-state/native qualification.

**No new C++ build, DSP execution, timing run, publication or decision to ship was performed to create this export.**
