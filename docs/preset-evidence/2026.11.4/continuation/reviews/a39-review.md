# A39 — continuation evidence and final local provenance review

Target source: `bcc86aa39a33a0162254614e5502705f0113d701`.

This was a read-only audit of existing files, hashes, JSON records and JUnit/log results. No production files or repository documentation were edited; no audio renders or test suites were rerun. Machine-readable details and exact digests are in `a39-summary.json` beside this report.

## Outcome

The recorded evidence is usable, with two publication gaps: the manifest does not cover nine already committed files, and the committed bowed timing log is the earlier run rather than the run summarized by the release text. These require documentation/evidence additions, not DSP changes or weaker acceptance criteria.

## Manifest scope

`continuation/evidence-files.json` lists **25 distinct files**. Every listed byte count and SHA256 matches the current file exactly; there are no duplicate paths or stale listed hashes. All audited continuation JSON files parse successfully.

Nine existing committed files are outside that manifest:

| File | Bytes |
| --- | ---: |
| `a20/FINDINGS.md` | 2,416 |
| `a20/provenance.json` | 1,211 |
| `a20/probe.cpp` | 3,369 |
| `a20/run.py` | 3,915 |
| `a20/trials.csv` | 13,827 |
| `a20/summary.json` | 7,283 |
| `local-affected/affected-junit.xml` | 9,104 |
| `local-affected/affected-tests.log` | 961 |
| `local-affected/summary.json` | 1,022 |

Refresh the manifest after final evidence selection. It should enumerate the intended continuation evidence set, exclude itself, and hash raw bytes; avoid newline conversion of CSV files. A list of selected files is acceptable if explicitly labeled partial, but an updated complete list is clearer here.

The artifact hash maps in a09/a20 also name binaries and private intermediate headers deliberately not committed. All 13 present referenced payloads checked across those two maps match. Describe those maps as provenance for the original experiment, not a guarantee that every named intermediate is distributed. There is no reason to commit large archives or executables solely to fill those maps.

## Bowed raw timing evidence gap

`a10/summary.json` reports the final pinned 11-pair timing experiment, including moving bowed **1.1243×** and fixed bowed **1.0113×** median ratios. Those trials are in scratch `a10/final-bowed-results.log` (4,765 bytes; SHA256 `c8f82566d0f754d99c1ed7af299d2001217e59b4cfeb8188e16b0ab1079f5f0d`). The summary already records this filename and digest.

The currently committed `a10/bowed-results.log` is an earlier timing run. Its spectral rows and 36,115,200-position parity result equal the final log, but its CPU trial values differ. Preserve it as historical and add `final-bowed-results.log` as the raw support for the reported timing medians. Do not replace the earlier file silently or cite its timings as the final measurement.

Also add the small `bowed-tests.log` and `baseline-bowed-tests.log` if retaining the 492-check/174-baseline-failure claim. The repository summary states those counts, while the directly supporting complete logs remain scratch-only. The two 24-byte size logs make the reported Engine/body size deltas directly reviewable.

## Local final test evidence

Scratch final `affected-tests.log` and `affected-junit.xml` are byte-identical to their committed `local-affected` copies. JUnit records **7 executed CTests, 0 failures, 0 skips**. Six suites have a literal stdout truncation notice after the 1,024-byte threshold. Therefore the seven suite passes are observed, but the ending internal assertion totals of those six suites are not visible in this captured final XML. The complete Engine integration output records **18,156 checks, 0 failures**.

The separate final library-audit JUnit records **1 executed CTest, 0 failures, 0 skips**, with an observed duration of **340.341 seconds**. Its complete output reports 301 curated plus 222 original programs, a maximum phrase peak of −4.6 dBFS, loudest-window RMS −29.8 to −18.0 dBFS, and maximum stress peak −1.5 dBFS. The 244-byte text log and 590-byte JUnit remain scratch-only at review time; add them under a clearly named final-bank evidence directory.

These are eight local CTest executions across two commands, not eight new native gates or a sum of individual assertions. Do not combine a06/a10's repeated 2,077-check direct-model runs as unique final test cases. Do not label the source-derived expected voicing count 284 as an observed final count when the retained final output is truncated. Native CI counts, skips, validator lanes and platform results must remain separately sourced.

Raw CTest timestamps contain no explicit timezone; preserve them without relabeling them UTC.

## Source identities and equivalence

The local affected summary names `59d489d` as `base_commit`. That is the historical checkout base before the continuation edits, not an exact description of the tested candidate. Its recorded `InstrumentBody.h`, both new regression tests, corrected `PresetVoicingTests.cpp`, `Engine.cpp`, `FactoryPresets.cpp` and `Params.h` hashes all match final source `bcc86aa`.

Its CMake hash matches `c4fe309`/`e32728b`, not `bcc86aa`. The only subsequent CMake difference is the explanatory Engine-memory comment (about 270 KB → about 480 KiB); no build command or target changed. Preserve the original hash and add a source-equivalence note rather than rewriting historical provenance.

The final parameter-coverage record labels its candidate `e32728b`, binds inventory SHA256 `0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611`, and reports 523 ordered identities/tags, 198 existing IDs plus two appended body-size IDs, 521 changed musical programs, two unchanged Init programs, and zero duplicate non-volume vectors. The actual final inventory has 523 programs and 200 parameter IDs, matching that record. Its 259 body-enabled plus 264 circuit-only programs sum correctly. The `bcc86aa` delta is limited to guides and the CMake comment, preserving the exported `e32728b` library state.

The inventory is 704,822 bytes and coverage is 441,511 bytes. They are useful complete deliverables but are not tiny evidence additions. If the final qualified audition archive already includes these exact inventory bytes, a compact source/hash/structure summary plus the verified archive reference avoids duplication. If publishing coverage claims in the repository, retain the coverage JSON or a directly traceable summary rather than an unexplained aggregate.

## Exact recommended small additions

Copy these existing files without alteration; source paths, sizes and hashes are in `a39-summary.json`:

- `a10/final-bowed-results.log` — final raw CPU/spectral/parity support.
- `a10/bowed-tests.log` — 492 checks, 0 failures.
- `a10/baseline-bowed-tests.log` — 492 checks, 174 expected baseline failures.
- `a10/sizes-before.log` and `a10/sizes-after.log` — measured layout sizes.
- `evidence/final/bank-tests.log` and `evidence/final/bank-junit.xml` — final whole-bank result.

Add one compact final-local summary tying these files to recorded source hashes, their equivalence to `bcc86aa`, and the separation between local tests and native/package qualification. Refresh the evidence manifest after those additions. No new render or production-code modification is required to resolve this review.

