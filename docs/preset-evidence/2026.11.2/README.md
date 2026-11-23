# Circuit-library evidence — 2026.11.2

The binary source for this revision is
[`9c0d2baef5643a983a0e7a378b6be033371248f4`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/9c0d2baef5643a983a0e7a378b6be033371248f4).
Its native build is [run 45112404257, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45112404257).
The [publication record](ci/source-9c0d2ba/source-publication.json) records the exact source
tree and fast-forward update. Later screenshot and documentation commits do
not identify different binary bytes.

## Current native and download qualification

[Final qualification](ci/source-9c0d2ba/release-qualification.json) records the
completed **six-job build workflow**, all four native platforms, all required
validators and the actual installation/audition downloads. Every current result
uses source `9c0d2ba`, run **45112404257**, attempt **1**.

| Target | Passed / registered native CTests | Formal skips | Nested Python skips | Required pluginval invocations |
| --- | ---: | ---: | ---: | ---: |
| [Windows x64](ci/source-9c0d2ba/windows-x86_64/acceptance-summary.json) | 223 / 225 | 2 | 11 | 2 headless |
| [Apple Silicon](ci/source-9c0d2ba/macos-arm64/acceptance-summary.json) | 245 / 247 | 2 | 5 | 4: VST3, AU, AU GUI and fixed AU GUI |
| [Intel Mac](ci/source-9c0d2ba/macos-x86_64/acceptance-summary.json) | 245 / 247 | 2 | 5 | 4: VST3, AU, AU GUI and fixed AU GUI |
| [Linux x86-64](ci/source-9c0d2ba/linux-x86_64/acceptance-summary.json) | 582 / 583 | 1 | 3 | 3: headless, GUI and fixed GUI |
| **Platform-execution total** | **1295 / 1302** | **7** | **24** | **13** |

There are **zero native failures**. The 24 nested Python skips are separate
from the seven formal CTest skips; these are platform executions, not 1302
unique cross-platform test definitions. All 13 required pluginval 1.0.4
strictness-10 invocations completed with their original exit zero and **zero
validation retries used**. Both required Mac GUI paths and both Linux GUI
invocations completed the required editor/processing/automation phases. The
optional separate Steinberg VST3-validator executable was not configured, as
stated in the original logs. Windows pluginval coverage is headless.

The four actual installation archives account for **114 inventory entries and
10 native binary headers**. Their public downloads, API hashes/sizes, source/run,
architectures, complete inventories, notices and producer attempts were checked.
The actual Windows and Linux VST3 validator-input payloads match their installation
bundles. Earlier per-platform API snapshots retain the in-progress or queued
states observed at their check times; the final aggregate separately records the
completed successful workflow. A transient queued overall status caused one
Linux offline installation-check rejection; the unchanged checker accepted a
fresh real in-progress snapshot. No runtime gate was retried or relabeled.

The four corrected Linux fixtures now also have **551 checks and zero failures
in the actual native CI artifact**: 95 SUB-routing, 93 MIDI-map notification,
288 allocation and 75 multi-recall checks. The
[four-fixture proof](ci/source-9c0d2ba/linux-x86_64/corrected-fixtures-proof.json)
retains their original LastTest output, passing JUnit cases and exact durations.
The multi-recall JUnit output is explicitly truncated before its summary; its
count comes from the complete original LastTest block. This new CI evidence is
separate from the earlier local 551-check witness.

The [actual audition acceptance](ci/source-9c0d2ba/audition/acceptance-summary.json)
verifies **68 WAVs / 56 programs / 431 seconds**, all **523 × 190** inventory
values and all **81 checksummed members**. The excerpts are unnormalized
48 kHz stereo float32. All 41,376,000 channel samples are finite and below full
scale; the only silent excerpt is zero-pressure Pressure Lead. The highest
excerpt peak is **−14.517 dBFS**. These single-note capture results are distinct
from the full library audit's phrase/chord protocols and do not establish
acoustic or hardware equivalence, complete release tails or listening preference.

All current install and audition public links have recorded API expiry
**21 February 2027 at 04:31:50 UTC**. They are CI bundles without release signing
or notarization, not permanent release assets. Use the
[current download table](../../../README.md#downloads) and its installation guide.
Earlier failures remain attached to their original executions; the new Linux
normal exits do not establish the cause or a permanent fix for the historical
post-SUCCESS crash.

## Sound development and local integration

[voicing-review.json](voicing-review.json) preserves selected measurements from
16 completed development reviews behind the
[circuit-library report](../../CIRCUIT_LIBRARY_2026_11_2.md). It records preset
names, parameter changes, sample/note/velocity settings, analysis windows,
representative results, source hashes and the reviewed scope of each check.

**Before and after refer to each focused recipe edit, before the final library
VOLUME calibration.** They do not compare old and new qualified release binaries.
The source hashes identify family or test snapshots, not complete executables.
The input hashes identify the temporary measurement records used for this
reduction; those intermediate records and WAV files are not included here.

Review counts overlap. No listening evaluation, exact acoustic-instrument match,
hardware equivalence or fresh native-CI result is established by this record.
Integrated final-source and release qualification is separate. The committed
[precision tests](../../../tests/PresetPrecisionTests.cpp) provide the ongoing
signal-role regression checks; this JSON preserves the development results.

[local-validation.json](local-validation.json) records the complete 523-program
level calibration, local voicing/character/precision tests, keyboard-register
review and selected native-host checks. Its stated desktop and build limits
remain part of the record. The [60-agent evidence index](../../TEAM_REVIEW_2026_11_2.md)
distinguishes implementation, review, preparation and actual artifact acceptance.

## Current-source supporting checks

All five jobs in the current [numerical workflow](ci/source-9c0d2ba/fast-gates.json)
passed on `9c0d2ba`. The [fresh sanitizer record](ci/source-9c0d2ba/sanitizers.json)
accounts for all 196 registered cases: **195 passed, zero failed and one formal
compiler-specialization skip**. The configured core C++ build uses ASan/UBSan
with leak detection and halt-on-error; no sanitizer diagnostics were reported.
Plugins were disabled, and Python scripts are not C++ sanitizer-instrumented.
Nested Python skips are unavailable because passing child output is suppressed
and no separate sanitizer JUnit/LastTest artifact is uploaded.

The fresh [screenshot record](ci/source-9c0d2ba/screenshots.json) identifies
source `9c0d2ba`, all **11,508 core checks**, full editor smoke success, and zero
changed pixels at both strict Linux golden sizes. The screenshot workflow
published the actual editor gallery in commit `19dec1dd`. The full log and Git
gallery were checked; the screenshot ZIP itself was not independently downloaded.

The [filter matrix](ci/source-9c0d2ba/filter-batch.json) records six successful
GCC/Clang and DEFAULT/ON/OFF configurations: **24 passing executions of four
unique CTest cases**, with no failed or formal skipped entries. DEFAULT is
explicitly ON. Clang ON/DEFAULT still uses the scalar implementation; this result
does not qualify a Clang AVX2 path.

[Current numerical artifacts](ci/source-9c0d2ba/dsp-artifacts.json) records six
complete public downloads and all 60 ZIP members. Their source/run identities,
API sizes and digests, producer upload IDs, reports, source hashes and applicable
internal checksums were verified. These include the fresh DSP measurements,
synthetic comparison, frozen-baseline and V1/V2 reports, plus two diagnostic
core-engine captures. Those captures are separate from the factory audition.
The raw frozen-baseline PCM and V1/V2 binary inputs are not uploaded; their
comparisons remain supported by the successful strict CI `cmp` steps and reported
hashes. Synthetic comparison is a harness check, not recorded-hardware evidence.

## Corrections and retained earlier executions

[ui-screenshots.json](ci/ui-screenshots.json) records the completed `8ffe1e4` native Linux editor
job, **11,508 core checks**, the full UI smoke pass and zero differences against
both committed main-panel golden images. The initial Apple Silicon
[UI failure](ci/initial-macos-arm64-ui-failure.json) and the
[local correction](ci/ui-repair-local.json) remain visible; the failed initial
run is not relabeled as a passing build. The corresponding
[UI-source publication](ci/ui-source-publication.json) identifies that intermediate
source separately.

The subsequent `8ffe1e4` Apple Silicon suite passed **245 of 247 registered
CTest entries**, with two formal skips and zero failures. Its headless VST3 and
AU validators passed, but full AU GUI automation aborted in
`AppleParavirtCommandBuffer` with seed `0x2b4eef5`. The
[failure record](ci/corrected-macos-arm64-gpu-failure.json) preserves the exact
downloaded evidence and failed validator exit. No Apple Silicon package was
produced by that job.

Source `366c145` requests JUCE's supported synchronous CoreGraphics preference
for the macOS plugin editor and adds the exact failing seed as another required
full AU GUI gate. The [local policy record](ci/macos-rendering-policy-local.json)
records compilation, unchanged panel images, 22 workflow tests and 19 detected
unsafe mutations. It does not establish native runtime success or assign
exclusive responsibility for the earlier abort to the plugin, host or runner.

The previous source's [screenshots.json](ci/screenshots.json) separately records
the fresh `366c145` native editor job: all **11,508 core checks**, full UI smoke
success and zero image differences at both golden sizes. The resulting gallery
was committed by the screenshot workflow with the exact source/run provenance.
The screenshot ZIP's API metadata is retained; this record claims a fetched
Git gallery and checked complete job log, not an independent download of that ZIP.

The previous `366c145` source's [Apple Silicon acceptance](ci/macos-arm64/acceptance-summary.json)
records **245 passed / 247 registered native tests**, two formal skips, and all
four required strictness-10 validator runs with exit zero. Those include both
AU GUI runs and the recorded graphics-failure seed. The
[Windows acceptance](ci/windows-x86_64/acceptance-summary.json) records
**223 passed / 225 registered native tests**, two formal skips and both required
headless VST3 validator runs. Their complete installation archives were downloaded
and checked. These are platform-specific results; they do not establish a pass
for another platform or for an earlier failed execution.

The same source's first Intel job failed two native deadlines: the library audit
at 600 seconds and UI smoke at 240 seconds. Its
[original failure](ci/macos-x86_64-initial-timeout/failure-summary.json),
[progress comparison](ci/macos-x86_64-initial-timeout/failure-progress.json) and
[renderer-path review](ci/intel-renderer-path-review.json) remain preserved.
The UI continued writing screenshots before termination, and unchanged tests
were broadly slower than in the prior successful Intel run. The editor-rendering
preference is not consulted by that snapshot executable. These observations do
not complete either failed test or exclude an audit stall. All four validators
and packaging were withheld in that execution.

That same `366c145` run also failed four Linux native fixtures: **578 passed,
four failed and one formal skip among 583 registered cases**. The
[complete failure record](ci/linux-initial-failure/failure-summary.json),
[test roster](ci/linux-initial-failure/test-roster.json) and
[failed assertions](ci/linux-initial-failure/failed-test-output.json) preserve
that execution. Its package, audition and dependent Linux validators were
withheld. No retry of that source was executed.

Source `9c0d2ba` corrects those fixtures to assert the intended fixed Circuit
policy and the retained sample-accurate automation queues. The
[local before/after record](ci/linux-policy-fixture-repair.json) first reproduces
all **74 failing assertions across 438 checks**, then records **551 checks with
zero failures** in the corrected fixtures. It preserves allocation probes,
held-note continuity, MIDI mapping notifications, both SUB waveform sets and
the independent multi-recall endpoint oracle. Exact-zero tolerances remain
unchanged. This is a direct-processor/offscreen local witness, not native
window, VST3-wrapper or downloadable-package qualification.

The same commit gives only Intel macOS two finite CTest allowances: **1200
seconds for the library audit and 480 seconds for UI smoke**. Other platforms
keep the existing 600/240-second limits. Test assertions, internal GUI waits,
required validators and workflow exit handling are unchanged. The complete
product `src/` tree, parity contracts and workflows are byte-identical to
`366c145`. The [independent review](ci/source-9c0d2ba/independent-correction-review.json)
checks these immutable objects and the actual earlier failures. The fresh native
results above separately qualify `9c0d2ba`; neither the local correction nor
earlier platform successes change a failed historical result into a pass.

[fast-gates.json](ci/fast-gates.json) covers `8ffe1e4`'s five completed
numerical/compatibility jobs. [dsp-artifacts.json](ci/dsp-artifacts.json) records
four actual public downloads, API digest/size matches, every member's hash and
the downloaded reports. Frozen baseline PCM and V1/V2 binary inputs were not
uploaded, so those comparisons are supported by the successful strict CI
`cmp` steps and reported hashes. Only their uploaded corpus/report bytes were
independently downloaded. The frozen historical core check does not expose an
alternative engine in the plugin. The `366c145` source's
[publication record](ci/source-publication.json) proves that the numerical
inputs are unchanged. These results retain their actual `8ffe1e4` execution
provenance; they are not relabeled as a fresh `366c145` run. The changed macOS
workflow test scripts require the current source's local/native results.

The retained `8ffe1e4` [sanitizer record](ci/sanitizers.json) records **195 passed, zero failed and one
formal compiler-specialization skip** under AddressSanitizer and
UndefinedBehaviorSanitizer. Plugins were disabled in that instrumented build.
Nested Python skip counts are unavailable because successful child output was
suppressed and no sanitizer JUnit or LastTest artifact was uploaded. The complete
job log and successful required steps support the recorded result; it does not
claim instrumented AU/VST3 or GUI coverage.

## Download verification tools

[inspect-native-evidence.py](inspect-native-evidence.py) reads a complete raw jobs
API collection, exact native-test ZIP and completed job log. It requires a
successful native Test step, clean JUnit and matching complete console/JUnit/
LastTest rosters, and records formal and nested Python skips separately. A later
validator failure remains a failure in its native-only proof. The
[parser validation](ci/native-parser-validation.json) demonstrates this on the
actual earlier `8ffe1e4` Apple Silicon evidence, with six refusal controls and
both supported CTest summary formats.

[verify-install-bundle.py](verify-install-bundle.py) checks an actual installation
ZIP against complete artifact/run API objects, its exact source, native target,
inventory, binary headers, guides, notices and JUCE provenance. It does not
execute the downloaded instrument or infer validator success from a package.

[Current Windows/Intel validator checker](ci/source-9c0d2ba/tools/verify-platform-validators.py)
and [Apple Silicon checker](ci/source-9c0d2ba/tools/verify-arm-validators.py) check
the exact jobs and required lanes for `9c0d2ba`. Their reviewed changes from the
[prepared earlier checker](verify-platform-validators.py) are limited to the
source/run identity and, for Apple Silicon, the target/job/artifact names. The
checks, seeds, report requirements and bounded upload binding remain unchanged.
The [earlier parser validation](ci/platform-validator-parser-validation.json)
separates its offline Windows preflight from historical timestamp fixtures.

[Current Linux validator checker](ci/source-9c0d2ba/tools/verify-linux-validators.py)
requires all three Linux
lanes, original child exit zero, complete matching reports and exact VST3 payload
identity with the installation inventory. Every internal wrapper retry is
accounted for separately. Its [historical parser controls](ci/linux-validator-parser-validation.json)
include rejection of the actual earlier post-`SUCCESS` SIGSEGV. Those controls
are parser evidence, not a current Linux runtime result.
Only its source, run and native producer job constants differ from the
[prepared earlier checker](verify-linux-validators.py).

Package manifests contain the source and workflow run, but no producer-attempt
field. The earlier `366c145` [Windows and Apple Silicon upload bindings](ci/producer-bindings-windows-arm64.json)
join each exact artifact ID and byte count to its completed producer job and
attempt. The explicit two-second upload-step timestamp tolerance handles the
observed API/log precision difference; exact IDs, bytes and job identity remain
required. Later run attempts cannot relabel earlier artifacts.

[verify-audition.py](verify-audition.py) checks the actual **68-WAV / 56-program /
431-second** audition contract, the inventory of all **523** existing program
addresses, parameter digests, checksums and decoded audio metrics. It requires
Python 3.10 or newer and NumPy. Audio is unnormalized 48 kHz stereo; the
zero-pressure Pressure Lead example is intentionally silent.

Use the [current download table](../../../README.md#downloads) and its final
platform evidence for qualification. Neither a prepared verifier nor an uploaded
archive alone establishes a completed native or validator pass.
