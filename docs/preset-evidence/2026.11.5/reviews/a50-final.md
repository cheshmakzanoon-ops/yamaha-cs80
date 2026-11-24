# A50 — final-source Apple Silicon qualification

The replacement final source is `c76ad288e02fbeca48f39652bf4fcdd99837f793`,
tree `416c1f6bfb4cd9f7dd5dbd9f5cbc211edda2bf00`, native run `45351731801`,
attempt 1, Apple Silicon producer `138299557178`. This worker is the same
A50 assignment; reactivation does not add a worker to the requested count.

## Current status

Actual qualification remains pending. The original Test step began at
08:08:35 UTC on 24 November 2026. Configure, JUCE source verification and Build
had succeeded at the first authenticated resumed observation. No target
artifacts were present at the 08:21:05 UTC artifact query.

All ten files in the earlier db6b prepared checkpoint retain their hashes.
The old native run is not accepted or relabeled as this source. The c76 change
repairs the strict parity-capture metadata test for the two appended variant
parameters and adds negative schema mutations; it does not change the sound
source, factory data, acceptance thresholds or native test registrations.

## Input-binding correction

The new [original-execution binder](../ci/source-c76ad28/macos-arm64/qualify-macos-arm64.py)
implements A58 finding F1. It explicitly compares each raw native/validator
parser archive size and SHA-256 with the API identity already checked against
the actual downloaded bytes. It additionally requires the independent A58
checker to bind all parsed member records to those same immutable ZIPs. The
checker's own false qualification flags, pinned code hash, source/run/target
and five input identities must all agree. This preparation is not a claim
that the actual check has run.

The forthcoming qualification still requires complete native JUnit/LastTest
rosters and skips, all fifteen new CTests, exact JUCE provenance, all four
pluginval lanes and original exits, actual upload-attempt identity and
complete installation/validated-payload binding. Raw helper false qualification
fields remain unchanged. No native runtime, package or acoustic approval is
inferred from source review alone.

## Waiting checkpoint — 08:35:16 UTC

The original Test step remains in progress with no failed step reported. No
target artifacts were present in the 08:33:57 UTC query. More than fifteen
minutes have elapsed since this worker resumed without new terminal evidence,
so the prepared work is checkpointed to free the slot. No native assertion or
validator result has been inferred from the running state.

The [pending acceptance](../ci/source-c76ad28/macos-arm64/acceptance-pending.json)
and [file integrity record](../ci/source-c76ad28/macos-arm64/checkpoint-integrity.json)
retain the checkpoint. The binder is syntax-checked and ready for actual data.
It also joins the raw parser code/config identities and the original full-suite
CTest command, in addition to the A58 archive/member requirements.

## Original attempt 1 outcome — failed AU GUI validation

The completed original producer is **failed**, and this Apple Silicon platform
is **not qualified**. The actual native evidence contains **264 passed of 266
registered CTests**, zero failed/disabled/unknown, and two formal skips. The
five nested Python skips are counted separately. All fifteen new regression
cases pass. The corrected parity-capture test also passes and took
2.81015 seconds. Native source provenance verifies clean JUCE 8.0.11 at commit
`c352e2489000ffeb9e397051377d75eef2efe641` and exact c76 source/run.

The VST3 and AU headless validators completed at strictness 10 with 523 programs
and original successful steps. Their seeds were `0x1e0a207` and `0x4829337`.
The third invocation, AU GUI seed `0x3c99748`, entered Editor Automation and
then reported this assertion at 08:37:18.1273420 UTC:

```text
Assertion failed: (_commandCurrentStorage.currentOffset), function -[AppleParavirtCommandBuffer endCurrentChunk], file AppleParavirtCommandBuffer.mm, line 266.
pluginval received Abort trap: 6, exiting immediately
```

The original GitHub step reported exit 9. The AU GUI report member is zero bytes;
its console has no terminal SUCCESS. The required fixed-seed GUI lane and all
installation-packaging steps were skipped. No installation artifact exists for
this attempt. These are three started invocations: two successful and one
failed, with no runtime validation retries. They are not four completed lanes.

### Exact failure preservation

Both original uploaded ZIPs were actually downloaded, checked against authentic
API bytes/SHA-256 and original successful upload ID/name/digest/timestamps, then
read completely. All eleven members are retained, including the empty GUI
report. Lossless text encodings preserve the exact original ZIP bytes even if
a later rerun replaces same-named GitHub artifacts.

| Artifact | ID | Bytes | SHA-256 |
| --- | ---: | ---: | --- |
| Native test evidence |14903471382|80433|`6900bb29e3d926d53802f9ebced7ebe0c0925cd67e26d29fe379ddc1ffb3f9ba`|
| Partial validator evidence |14903346660|6433|`2c88144dbabd2d437795f2c75365704e7e82d58ffc7896716f7898d6bc21bd85`|

The [failure summary](../ci/source-c76ad28/macos-arm64/attempt-1-failed/failure-summary.json),
[preservation record](../ci/source-c76ad28/macos-arm64/attempt-1-failed/preservation-summary.json)
and [native-only observations](../ci/source-c76ad28/macos-arm64/attempt-1-failed/native-only-observations.json)
retain the actual result and its limited scope. Native-only green observations
do not accept the failed job or the incomplete validators.

### Rerun assessment and API restriction

One clean rerun of this same source and the complete original Mac job is a
reasonable bounded diagnostic: the assertion names a graphics command-buffer
path, while the native suite and both headless lanes completed. This does not
establish infrastructure ownership. No crash stack, core or controlled
reproduction rules out a JUCE/plugin/driver interaction. A clean random lane
also does not replay the failed seed; both that seed and the required existing
fixed seed remain documented. No code, threshold or required GUI coverage is
changed to obtain a pass. Any later artifacts need complete independent
qualification and must retain their actual new job/attempt identity.

The coordinator requested one job rerun after these archives were preserved.
GitHub rejected the request with HTTP 403 and: “The workflow run containing this
job is already running.” No rerun started and no attempt changed. This is an
API run-state restriction, not an approval denial. The original workflow must
finish before the coordinator can request only the Apple Silicon job again.
Windows, Intel Mac and Linux keep their original-attempt provenance.

The first failure-inspection script rejected the newer exact CTest console
wording `100% tests passed out of 266`. That diagnostic and its initial source
are retained; the corrected inspector accepts either exact known 266-case
wording and keeps the full JUnit/LastTest zero-failure gates unchanged. This
format correction is separate from the real AU GUI failure. The prepared
checkpoint binder is unchanged; the eventual fresh-attempt copy must use the
observed current console form.
