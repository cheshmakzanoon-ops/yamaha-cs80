# A60 — final independent release candidate review

## Decision

**Accepted: the complete original CI/artifact evidence and concrete publication candidate pass this independent review. No blocking finding or open documentation nit remains.** This is A60's actual final delivery, completing its assigned review scope.

The acceptance concerns binary source `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`, source tree `d84661c6c465c53ff41361e8aba0cba5e4a53397`, preset revision **2026.11.4**, original native run **45246863788**, attempt **1**. The original run and all six jobs completed successfully. The reviewed documentation checkpoint is `df9a8d2994e96534eb7c4ee57c0a82ea1fb6916c`; later documentation/evidence publication does not identify different binary bytes.

**Root publication is subsequent.** After this delivery, root records the actual A59/A60 outcomes, retains their final reports, closes factual status/count fields, regenerates final scoped indexes, publishes the reviewed source-equivalent tree and verifies the remote result. This report does not claim those actions have already occurred and does not require a self-hash or self-status cycle.

## Original native results independently recounted

A60 parsed all four actual original `ctest-results.xml` files from their downloaded evidence ZIPs, checked unique testcase names, explicit pass/failure/skip elements and header totals, and reconciled the results with both the complete A45 observation report and the root aggregate. Nested Python skips were independently counted from terminal summaries in each complete `LastTest.log`.

| Original target | Passed / registered | Formal CTest skips | Separate nested Python skips |
|---|---:|---:|---:|
| Windows x86_64 | 227 / 229 | 2 | 11 |
| macOS arm64 | 249 / 251 | 2 | 5 |
| macOS x86_64 | 249 / 251 | 2 | 5 |
| Linux x86_64 | 586 / 587 | 1 | 3 |
| **Total** | **1,311 / 1,318** | **7** | **24** |

All four have **zero failed, disabled and unknown outcomes**. `cs80_stereo_pra_exact_tests` is formally skipped on all four platforms; `cs80_filter_batch_cap_tests` is formally skipped on Windows and both Macs. Each explicit JUnit skip uses `SKIP_RETURN_CODE=77`. A CTest headline or a LastTest passing marker does not turn a formal skip into a pass.

Original successful Test steps, the exact source workflow and the platform producer reviews bind these observations to their real native executions. Counts are platform executions, not unique test definitions or internal assertions. Supporting, local and nested test counts are not added to the native totals.

## All 13 original validator lanes

The accepted result is **13 required pluginval 1.0.4 lanes at strictness 10**, each reporting **523 programs**, with **13 original invocations, zero runtime validation retries and 13 normal original exits of zero**. A60 checked each aggregate lane against the corresponding actual successful original API step.

| Target | Required original lanes | Normal exit evidence |
|---|---|---|
| Windows x86_64 | Randomized headless; fixed-seed headless `0x5037a0c` | Successful original PowerShell steps and immediate unsuppressed native exit propagation |
| macOS arm64 | VST3; AU; AU GUI; AU GUI fixed seed `0x2b4eef5` | Successful original steps and exact source/log-matched `set -euo pipefail` pipelines |
| macOS x86_64 | Same four required lanes | Same original-step/source binding |
| Linux x86_64 | Randomized headless; randomized GUI; GUI fixed seed `0x2da72db` | Successful original steps and direct raw child termination records |

A60 directly read the three retained original Linux core logs. They record child PIDs **2685, 2716 and 2759**, each with **raw return code 0, kind `exit`, exit code 0**. The original Linux evidence has no `attemptN` logs and no original status-139 failure. Its failure-only diagnostic reruns were skipped. Synthetic diagnostic-helper fixture exits, including 7 and 139, are not pluginval failures or extra validator invocations.

The source contract cannot accept `SUCCESS` followed by a nonzero exit or crash. Mac and Windows do not have the separate Linux-style raw numeric child termination file; their normal-exit findings depend on successful authentic original steps and exact unsuppressed child-status propagation. That evidence distinction is retained.

Console/report copies count as one invocation. The **six AU current-program warnings** and **seven optional external VST3-validator skip notices** remain in the detailed evidence. These do not establish standalone Steinberg-validator coverage. ARM's pluginval download fallback is a transfer attempt, not a validation retry.

The authoritative scoped reviews are `macos-validators/FINDINGS.md` and `windows-linux-validators/FINDINGS.md` under the current CI evidence directory. The raw complete parser deliberately retains `release_qualified: false`; the root aggregate supplies the separate producer, original-process and payload binding that parser does not infer.

## Actual installation archives and tested payloads

All four strict installation reviews accept actual archive bytes, complete inner inventory and checksums, exact source/revision/run, sidecar versus embedded manifests, `SOURCE.txt`, clean pinned JUCE 8.0.11 provenance, and every advertised native executable's target header. Original producer/attempt/upload identity is bound separately by the corresponding platform review.

| Target | Actual installer artifact ID | Outer ZIP bytes | Native formats |
|---|---:|---:|---|
| Windows x86_64 | **14856804857** | 6,361,027 | VST3, Standalone |
| macOS arm64 | **14855214289** | 14,949,170 | VST3, AU, Standalone |
| macOS x86_64 | **14858878730** | 16,228,346 | VST3, AU, Standalone |
| Linux x86_64 | **14870428441** | 13,322,687 | VST3, Standalone |

Across its preliminary and final work, A60 independently rehashed **13 unique actual original evidence ZIPs**, including all four installers and the new Linux installation, native evidence, validator input and validator-report ZIPs. A59 independently rehashed all five advertised archives, including the full 300 MB audition archive, and matched their individual API objects and final artifact catalog.

A56's complete Windows/Linux selected VST3 tree equality includes both executable and `moduleinfo.json`, every directory relationship and any link targets. The only container exclusion is the exact format-checked Windows `.exp`/`.lib` pair, bound to the source upload directory and actual consumer-selected VST3 subdirectory; no selected-bundle member is excluded. Linux has no such exception.

A55 establishes both Macs' AU source/registered full-bundle comparisons before and after validation, the four AU digest lines per target and their equality with the distributed executable. Mac VST3 binds the original validated build path to ordered unchanged packaging. The evidence does not invent a Mac VST3 pre/post digest artifact.

## Actual audition and delivered inventory

The actual audition artifact is **14870917591**, **300,067,470 bytes**, SHA-256 `ae8cd5c31e03f0db317852f49a9169ad7ff455e2f385e8a510e8da4569f500ad`. A58's unchanged frozen verifier and supplemental checker both exited **0** against the original artifact, authentic source/run/attempt, successful capture/upload and recorded source-pinned invocation.

| Observed audition result | Accepted value |
|---|---:|
| Ordinary regular ZIP entries | 173 |
| Verified member checksums | 172 |
| Metrics groups / CSVs | 30 |
| WAVs / distinct auditioned programs | 138 / 74 |
| Duration | 870 seconds, 14 minutes 30 seconds |
| Stereo frames / scalar samples | 41,760,000 / 83,520,000 |
| Sample format | 48 kHz stereo IEEE-754 binary32 float |
| Nonfinite / above-full-scale samples | 0 / 0 |
| Active body models | All 19 |
| Delivered inventory | 523 programs × 200 parameters |

A58 checked every actual entry, checksum and audio sample, RIFF/fact/data framing, duration, metrics, settings and parameter digests. The supplemental checks close the frozen helper's API/attempt, all-entry metadata and fresh-output limitations. A60 reviewed those contracts and actual exit records, recomputed the totals from all 138 per-WAV observations, and independently matched the actual ZIP metadata to **all 173 retained entry records**. No audio or audio verifier was rerun by A60.

A60 also read the actual ZIP's delivered inventory and compared its bytes directly to the retained inventory. They are identical, SHA-256 `0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611`. A58's independent binary32 comparison covers all **104,600 values**, all 200 parameter IDs and every ordered index, host index, name, category, description and tag list. The historical export label remains `e32728b`; source equivalence and the actual delivered-byte comparison provide the linkage without rewriting the old record.

The highest observed peak is approximately **−13.674 dBFS**. The zero-pressure Pressure Lead example is intentionally silent. These are representative single-note numerical and archive results for 74 programs, not human listening approval or audio coverage of all 523 programs.

## Supporting outcomes and source preservation

All five original jobs of exact-source DSP run **45246863831**, attempt 1, passed; six actual supporting archives are accepted. ASan/UBSan records **199 passed / 200 registered, one formal skip, zero failures and no reported sanitizer diagnostics**. Successful Python stdout was suppressed, so the supporting sanitizer's nested Python skip count remains unknown.

Filter run **45244531626** passed all six original jobs and **24 CTest executions** at `bcc86aa`. All **91 relevant inputs** are byte-identical to the release source; the executed source is not relabeled. Clang ON/DEFAULT are not presented as AVX2 executions.

Exact-source screenshot run **45246863473** passed its original UI smoke. Both actual golden-size images are byte-identical to unchanged source goldens, with **zero changed pixels**. The preceding screenshot failure and its three fixture assertion failures remain recorded.

A60 reconfirmed **no tracked production, test, script, workflow or resource drift after tested source 078c098**. A43/A57 retain all **523 ordered program identities and addresses**, both Init programs, the 198-core and 304-host automation prefixes, and the appended neutral size controls. Totals are 200 core and 306 host parameters. The earlier `.4` stage reprogrammed all 521 musical programs; the continuation changes no recipe vectors and corrects configured disjoint sets of **17 tine and 51 bowed programs**.

The all-521 matched spectral/envelope capture remains **historical 59d489d** evidence. Numerical response improvement, distinct parameter vectors, source reviews and native CI do not establish perfect realism, universal subjective improvement or a calibrated acoustic/hardware match. Existing family-specific model limits, inert controls, deferred proposals, shared-machine runtime variance and added storage remain disclosed.

## Concrete publication and actual-worker review

A60 read the exact README Downloads, new distribution section, revision report, root aggregate, qualification status, retained team report and actual worker ledger. The five README artifact IDs match the accepted four installers and audition. A59's final authored candidate review was read and accepted; its **103 named checks** and **23 referenced-file byte/hash checks** agree with A60's independent findings.

A60 verified those **23 aggregate file references**, the **22-file audition** and **27-file retained Linux** maps, and the current **196-entry continuation manifest**. It checked **184 local document links**. At the preclosure snapshot, only the two deliberately reserved A59/A60 final-report paths awaited their actual deliveries; root fills them from the delivered bytes.

The concrete ledger contains **exactly 60 unique sequential continuation workers**. All completed primary report paths and available recorded hashes agree. The entire earlier 53-row `.4` work-log contributes **zero** workers to that total. A02/A03/A04 are explicitly root-attested conversational completions whose original standalone reports/transcripts are unavailable; downstream corroboration is not relabeled as original authorship. Historical spawn API receipts were not reconstructed. A10's later provenance addendum remains disclosed.

A57's authored snapshot honestly records **58 delivered scopes**, with A59/A60 final delivery subsequent. A59's actual stable final report is now delivered; this report is A60's final delivery. Root can close the current published ledger/team statuses from those actual outcomes while retaining the original A57 snapshot unchanged. Follow-up turns do not add workers, and 60 workers does not mean 60 simultaneous workers.

Every byte after the old `.3` distribution heading through EOF remains unchanged: **25,173 bytes**, SHA-256 `43a4c2827550c104a5570df9ac7381713854d8b906ed79928ccb3979ea1e720f`. Earlier failed and canceled runs, their source IDs, counts and links are preserved. The new `.4` section also retains the distinct failed/superseded continuation outcomes.

All five advertised APIs expire at **2027-02-21 19:14:35 UTC**, correctly displayed as **21 February 2027 at 15:14:35 AST** for Halifax. The current text retains the Actions fallback and retention limits, host-process architecture, whole-bundle copying, compatible Linux libraries and absence of publisher release signing or Apple notarization. Connector acquisition does not become an independently verified unauthenticated nightly.link route.

The candidate prominently instructs **Library → FACTORY LIBRARY → LOAD / RELOAD, then play fresh notes**, explains stored old-project settings, and distinguishes frozen source-time bundled guides from the later online qualification.

## Parent closure and review boundaries

No CI or actual-artifact gate remains open. No additional product test or optional rerun is requested. Root's remaining work is to retain the stable final reports and observations, record actual final worker outcomes, close qualification status, regenerate scoped indexes without self-hash cycles, publish the exact reviewed source-equivalent tree and verify the remote result.

A60's final structured decision is `a60-review.json`. Scoped observations are `a60/final-evidence-observations.json`, `a60/final-ledger-observations.json` and the retained preliminary evidence record. The earlier A60 checkpoint is preserved separately and is not relabeled as having known final results.

A60 wrote scratch reports only. It did not build or execute the product, rerun native suites or validators, render audio, change source/workflows/Git state, or write to an external service. Final publication and remote verification remain root-owned subsequent actions.
