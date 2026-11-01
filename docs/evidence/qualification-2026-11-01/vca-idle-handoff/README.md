# VCA idle handoff: qualification receipt

2026-11-01. Software qualification only. Canonical audio `b75b4d653b6ed6f0aa5ef8ee99b670d83e975250`; local compiled equivalent `754b4cccc6f0e9a37c8e98d221757b0deec19f30`. The common source subtree is `a153b5ec6ca3b64331488af7d32fa13d8d29a7ec`. Separate published V2 pin: `fb43c04e68e91eea120add3a89e6866ef1e64e01`.

Full mechanism, source/inference boundaries and reproduction: [investigation report](../../../click-20261101.md). The original score <= 1 requirements remain open where shown below.

## Gates

- The baseline engine fixture fails 180 of 325 checks. Both deterministic twins have identical output after retirement because the normal tail is disconnected.
- The repaired engine passes all 361 checks, including the separately preregistered cold-reset amendment. Primitive VCA tests pass all 59 checks.
- Current exposed-state event continuation passes 7,438 checks; current ring continuation passes 2,998 assertions. Full assertion logs are retained.
- Non-strict compatibility passes Legacy default, Legacy tails and V1 exactly; its V2 difference against `481c7e9` is expected for this audio change.
- Strict compatibility with the separately published pin passes all four comparisons, including V2 byte identity to `b75b4d6`. The local workflow was restored to its original contents after that command.

## Actual-source mutation controls

Every mutant compiles before execution. Each is rejected at its intended assertion; compilation errors are never counted as rejection. The unmodified and restored controls pass. The runner creates a consistent temporary source tree and leaves production files untouched.

| Mutation | Assertions | Failures |
| --- | ---: | ---: |
| disconnect-idle-output | 361 | 36 |
| disconnect-rank-II-output | 361 | 18 |
| double-count-idle-discharge | 361 | 72 |
| retain-charge-at-panic | 361 | 108 |
| retain-charge-at-cold-reset | 361 | 36 |
| freeze-closed-charge | 59 | 6 |
| attenuate-closed-output | 59 | 9 |
| weaken-integration-contract | 361 | 1 |

The original registration is `61adb02479b566810b597d869be39b69afff6ef1`; the exact reset amendment is `8c5cf6458eff2166ecd71e75bea7a8754345f965`. Mutation commands, patches, full output and source hashes are in `mutations/`. Its manifest retains the actual local qualification SHA rather than relabelling it as the canonical publication.

## Named phrase/counterfactual results

The original 64-frame event quantization, 8 ms windows, references and control coordinates are unchanged. All omitted-event replays have exactly identical pre-event PCM. These named fixtures are not a full-bank worst-case claim.

| Fixture / coordinate | Rate | Before | After | After, event omitted |
| --- | ---: | ---: | ---: | ---: |
| Funky 1 + Funky 2 / tone_tabs | 44100 | 1.62688913455 | 1.62688913455 | 1.6268876393 |
| Bass Stack / original_physical_panel | 44100 | 0.567655911129 | 0.567655707889 | 0.0270218485223 |
| Bass Stack / electrical_programmer | 44100 | 0.294052189432 | 0.294052189432 | 0.29405277458 |
| Funky 1 + Funky 2 / tone_tabs | 48000 | 1.29131001926 | 1.29131001926 | 1.3805030684 |
| Bass Stack / original_physical_panel | 48000 | 1.44092402862 | 1.44092402862 | 0.031435318561 |
| Bass Stack / electrical_programmer | 48000 | 0.269982960216 | 0.269982960216 | 0.237557903226 |
| Funky 1 + Funky 2 / tone_tabs | 96000 | 1.22095816514 | 1.22095816514 | 1.23890355235 |
| Bass Stack / original_physical_panel | 96000 | 1.36530856995 | 1.36531102848 | 0.0730431293724 |
| Bass Stack / electrical_programmer | 96000 | 0.385795006863 | 0.385795006863 | 0.386348194455 |

Physical-PANEL Bass remains above one at 48/96 kHz. Funky note-off remains above one at every rate, and every omitted-note-off control also exceeds one. This capacitor repair does not close those criteria or justify a fade/retuned envelope.

## Required CPU rounds

Each phase contains three completed rounds of the unmodified baseline suite at 48 kHz, 256 frames, 128 held callbacks, all 11 pairs and moving/idle controls: 27 cases and 51 rows. All six rounds report zero nonfinite samples. No quick subset is used.

Root builds and validation overlapped on a shared host. These wall/thread CPU data are retained as diagnostics and cannot establish an improvement or compare directly with the repository's dedicated CI runner. No CPU target completion is claimed.

| Phase | Round | Held tone median RTF range | Quiet idle median RTF |
| --- | ---: | ---: | ---: |
| before | 1 | 0.920643–9.313921 | 0.131378 |
| before | 2 | 0.837015–1.674495 | 0.065896 |
| before | 3 | 1.215328–4.875305 | 0.062429 |
| after | 1 | 0.639069–2.357764 | 0.061994 |
| after | 2 | 0.959161–4.164548 | 0.138642 |
| after | 3 | 0.977183–4.722355 | 0.122824 |

Raw benchmark CSVs, metadata and completion receipts are preserved per round. The caller-verified local audio SHA in the after receipts maps to the canonical source subtree above. `environment.json` records the compiler/host and `manifest.json` records the exact binary hashes.
