# Official upstream validator comparison

This is a **separate, bounded prerelease comparison**, not a replacement for the
existing pluginval **1.0.4** gate in `build.yml`. That gate, its test invocations,
and all [historical failures](VALIDATOR_DIAGNOSTICS.md) remain unchanged. A passing
comparison cannot turn a failed stable run green, establish crash ownership, or
qualify the instrument by itself.

## Why this official binary

Read-only upstream verification on 2026-10-30 established:

| Item | Pinned evidence |
| --- | --- |
| Latest stable release | `v1.0.4`, still the existing gate |
| Separate official prerelease | `v2.0.0-beta3`, published 2026-10-14 |
| pluginval commit | `37d6a37bc752606c524f7f3febffc73e0945a740` |
| Linux asset | `pluginval_Linux.zip`, 8,205,143 bytes |
| Archive SHA-256 | `2e23d311c8c2664457b11be8f6cf4ed29cfdc08765c71c43934a1ed3360f118f` |
| Bundled JUCE source pin | `9.0.3`, commit `be29c81492b6151c8ea8d14c840e1311963b3a83` |

Primary sources: [stable release API](https://api.github.com/repos/Tracktion/pluginval/releases/latest),
[beta release/asset metadata](https://api.github.com/repos/Tracktion/pluginval/releases/tags/v2.0.0-beta3),
[annotated tag target](https://api.github.com/repos/Tracktion/pluginval/git/tags/442d7de9ec40675355e840d7bb3492e176d3f800),
[pinned CMake](https://github.com/Tracktion/pluginval/blob/37d6a37bc752606c524f7f3febffc73e0945a740/CMakeLists.txt),
[official archive](https://github.com/Tracktion/pluginval/releases/download/v2.0.0-beta3/pluginval_Linux.zip).

JUCE 9.0.3 includes the official callback-lifetime change
[`04e167d64661d3298afd4978f6c2e77f841b5f17`](https://github.com/juce-framework/JUCE/commit/04e167d64661d3298afd4978f6c2e77f841b5f17).
Its [tagged implementation](https://github.com/juce-framework/JUCE/blob/be29c81492b6151c8ea8d14c840e1311963b3a83/modules/juce_audio_processors_headless/format_types/juce_VST3PluginFormatImpl.h#L478-L552)
contains the lifetime protection. Inclusion is source evidence, **not proof that
it resolves this repository's intermittent exit139**. No local repair, backport,
sanitizer reproducer, debugger, fault tracer, or custom validator is part of this
workflow. The downloaded executable remains unmodified; only its executable file
permission is restored after ZIP extraction.

`develop` was already at JUCE 9.0.4 when checked, but this comparison deliberately
uses the official published beta3 asset, not a moving branch or a source build.

## Trigger and provenance boundary

[The workflow](../.github/workflows/upstream-validator-comparison.yml) triggers only
after `build` completes for a `push` to `main`. It checks both the event and GitHub's
API metadata for the exact repository `cheshmakzanoon-ops/yamaha-cs80`, source
repository, branch, event, run ID/attempt, source SHA and `build.yml` workflow ID.
Pull requests, forks, other branches, manual builds and other workflow identities
are excluded. There is no manual dispatch or arbitrary run/URL input.

The original build **does not need to be green**: a failed stable-validator job
is an important comparison case. However, exactly one non-expired
`cs80-linux-vst3` artifact must exist in that exact run, and its run/SHA/repository
metadata must match. The download uses that verified artifact ID and source run
ID, never the newest successful build. Missing, ambiguous, expired or mismatched
artifacts fail setup rather than silently substituting another build.
`merge-multiple: true` explicitly extracts that one verified artifact directly
into `vst3/`, preserving the expected `vst3/CS-80 Reference.vst3` bundle path.

No checkout is performed. The workflow does not execute scripts from the source
artifact or source repository; the plugin is loaded only by the official validator.
Actions are pinned to verified upstream commit SHAs. The automatic token has only
`contents: read` and `actions: read`; no secrets, new credentials, status writes,
security settings, kernel settings, caches, or repository mutations are needed.
Artifact transfer/upload uses the ordinary Actions services.

`provenance.json` records the source SHA/run/attempt/conclusion, artifact ID/digest,
comparison workflow SHA/run, validator tag/commit/archive digest/JUCE pin, lane,
seed and settings. Each lane also retains plugin-file and validator-binary hashes,
archive checksum verification, package/runtime-library inventory, version output,
strictness-10 inventory, exact command, timestamps, raw console and XML/JSON reports.
GitHub's source-artifact archive digest is recorded as metadata; the separate
plugin-file manifest hashes the actual extracted files. The validator ZIP's pinned
SHA-256 is explicitly checked **before extraction or any execution**. A mismatch
fails without trying a different download or validator version.

## Two independent, non-equivalent comparison lanes

Both lanes use strictness **10**, one repeat, one fixed seed, sample rates
`44100,48000,96000`, block sizes `64,128,256,512,1024`, a 120,000 ms test timeout,
and a 600-second outer process timeout (10-second termination grace). Each runs
**once**. Matrix fail-fast is disabled so one failure does not cancel the other.
There is no `continue-on-error`, validation retry, rescue invocation or
retry-until-green behavior.

| Lane | Seed | Scope and historical context |
| --- | --- | --- |
| `headless-subset` | `0x4f31605` | Adds `--skip-gui-tests`. Stable [run 40977141585](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40977141585) exited139 after a logged Fuzz-parameters completion without overall SUCCESS. |
| `gui-inclusive` | `0x63421ca` | Omits `--skip-gui-tests`; uses Xvfb at 1920x1200x24. Stable [run 40967809044](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40967809044) printed SUCCESS and then exited139. |

These are ordinary full validation invocations, not isolated crash reproducers.
Seeds anchor a bounded comparison but do **not** reproduce scheduling, plugin
source changes, test order across releases, or earlier process topology. In
particular, the old gate's `--validate-in-process` option is not supported by beta3
and is intentionally not passed. The older regression seed `0x2da72db` remains in
the unchanged stable gate; this separate comparison does not add extra replays.

The beta's [documented CLI](https://github.com/Tracktion/pluginval/blob/37d6a37bc752606c524f7f3febffc73e0945a740/docs/Command%20line%20options.md)
and [registered options](https://github.com/Tracktion/pluginval/blob/37d6a37bc752606c524f7f3febffc73e0945a740/source/SettingsParser.cpp#L176-L220)
use `pluginval validate [options] <plugin>`. There is no `--headless` option.
Unknown options now fail; the historical flat `--validate` alias is deprecated.
Headless here means an explicitly labelled **GUI-skipping subset**, never a claim
that GUI coverage passed. Beta3 adds/enables tests and changes state-restoration
checks, so equal strictness numbers do not mean equal test inventories.

## Result contract and limitations

The process/timeout exit and logging exit are saved independently. A nonzero
process exit, including `139` or timeout `124`, fails even if SUCCESS was printed.
Exit zero additionally requires an explicit SUCCESS line and a nonempty, complete
JSON report with every reported test passed and zero failures/errors. Missing or
invalid reports fail; the report must also identify the expected plugin path,
strictness, seed and test timeout. Failed setup or an interrupted job is not a
validation pass.
These checks follow beta3's actual schema rather than assuming stable output:
[`Validator.cpp`](https://github.com/Tracktion/pluginval/blob/37d6a37bc752606c524f7f3febffc73e0945a740/source/Validator.cpp#L221-L315)
emits literal SUCCESS after a clean completed report;
[`CommandLine.cpp`](https://github.com/Tracktion/pluginval/blob/37d6a37bc752606c524f7f3febffc73e0945a740/source/CommandLine.cpp#L52-L83)
forwards log text without color codes.
[`ReportWriter.h`](https://github.com/Tracktion/pluginval/blob/37d6a37bc752606c524f7f3febffc73e0945a740/source/ReportWriter.h#L32-L46)
defines only passed/failed/error outcomes, and
[`PluginTests.cpp`](https://github.com/Tracktion/pluginval/blob/37d6a37bc752606c524f7f3febffc73e0945a740/source/PluginTests.cpp#L195-L208)
omits GUI-skipped/disabled selections before beginning those cases. There is no
legitimate skipped outcome that the report guard would reject, and no fixed
stable-version test count is imposed. Format-specific no-op cases retain the
upstream test runner's own outcome, not an invented skipped/failure status.
Reports and the job summary are uploaded with `always()` where GitHub permits;
an abrupt runner loss can still prevent complete reporting.

This workflow uses Ubuntu 24.04, matching beta3's
[official Linux build container](https://github.com/Tracktion/pluginval/blob/37d6a37bc752606c524f7f3febffc73e0945a740/.github/workflows/build.yaml).
Ordinary runtime packages come only from that runner's configured Ubuntu package
sources. No external APT repositories are added. Runtime dependency failures are
visible failures; stable's older compatibility floor is not assumed. No local
source compilation is performed. Source-building this release would require
CMake 3.22 through JUCE and C++20, but is outside this workflow.

The comparison does not publish a replacement commit status, alter required
checks, or edit historical evidence. Read each result alongside the **same source
build's original stable result**. A later green beta result cannot erase any
original failed invocation, prove a repair, establish host compatibility on other
platforms, or demonstrate CS-80 hardware fidelity.

### Verification status at introduction

Upstream metadata/source inspection and local workflow/static guard checks are
separate from runtime evidence. The beta executable was **not downloaded or run
locally** while authoring this workflow. Its first runtime result must come from
an identified Actions comparison run after review/publication; until then runtime
behavior is unverified.

Local authoring checks passed: YAML/permission/matrix/download invariants, syntax
checks for every shell block and the JavaScript/Python snippets, 29 mocked
event/API/artifact-origin guard cases, and 16 pure JSON/text report-contract cases.
Those fixtures do not load a plugin or execute the validator.
