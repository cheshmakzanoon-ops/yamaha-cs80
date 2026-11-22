# Prolonged Linux Test step — bounded source inspection

**No new unbounded test wait was identified. The current elapsed-time cause is
undetermined, and final acceptance remains pending.** This inspection executes
no test or product, changes no source or workflow, and performs no retry,
cancellation or authentication.

The parent observed candidate run `45044040694`, source `6d3cc220`, attempt 1,
Linux job `137274597635`, still in Test around 19:08 UTC after a Test start of
16:43:01 UTC. This document does not independently observe the remote process or
claim that it has hung.

## Historical scope correction

The preserved `final-linux-x86_64.json` record for **d0c3c23 / run 45029235535**
reports **578 registered cases, 577 passed, one skipped, zero failed and zero
disabled**, with **6,629.97 seconds** wall time. Its skip is
`cs80_stereo_pra_exact_tests`, `SKIP_RETURN_CODE=77`.

The different **575-case / 6,926.01-second** record belongs to older
`bb4d751e / run 45002603673`. It must not be assigned to d0c3c23.

The public d0c3c23 CMake file was fetched at that immutable source and is
**byte-identical to 7f55a95**. Current source adds exactly one CTest registration,
explaining **578 → 579**, rather than four newly added tests.

## Added and changed work

| Case | Source finding | Configured CTest timeout |
| --- | --- | ---: |
| New preset character test | 23 fixed renders, 11 programs, 31.4 seconds of audio at 44.1 kHz; 1,384,740 frames in positive bounded chunks | 120 s |
| Packaging notice fixtures | Finite revision declarations, guide links and source/notice checks; existing packaging helpers | 60 s, unchanged |
| Diagnostic selection fixtures | Finite outcome grids and 12 mocked replay-shell combinations; no pluginval or debugger executes | 30 s, unchanged |
| macOS download fixtures | Expected workflow-region fingerprint and comment correction only | 30 s, unchanged |

The character test's render loop advances by a positive count of at most 128
frames until its fixed buffer ends. Its five fixed-register DFTs use 8,192-sample
windows and 45 total bins: 368,640 bounded inner iterations. Measurement loops
also operate on fixed clamped windows. There are no subprocesses, thread joins,
network operations, sleeps or wait-until-release loops in the new test.

The diagnostic fixture's individual Python subprocess calls do not provide
their own `timeout` argument. Their finite selector/mock programs run inside a
CTest case that already had a 30-second deadline. Its new shell cases replace
`timeout` with a stub that records arguments and exits; the real debugger,
validator and Xvfb are not launched by those fixtures.

The existing full-bank audit, voicing suite and capture CLI suite retain their
600-, 180- and 60-second CTest limits. Their source is unchanged. The bank audit
uses fixed sample budgets, not newly extended release waits. Preset parameters
can affect per-sample work, but no new loop cardinality or timeout expansion was
introduced into those tests.

Expanded 24-WAV audition generation occurs **after** the native Test step. Real
pluginval diagnostics belong to the downstream validator job. The one-off
publisher and its fixtures execute in a separate publication workflow. Those
operations cannot be attributed to the currently active native Test step.

## Profile and remaining uncertainty

All nine Configure/Build/Test step objects across Linux, Windows and macOS are
unchanged between d0c3c23 and 6d3cc220. Linux still specifies Ubuntu 24.04,
Release, plugin/tests enabled and serial CTest.

A **read-only** local `ctest --show-only=json-v1` inspection confirms 579
registered cases, timeout scale 1, and the stated limits. It also finds **18
inherited cases without an explicit per-test timeout** and existing explicit
limits up to **1,200 seconds**. Consequently this review does not claim that the
entire pre-existing suite is bounded by small deadlines. All these cases and
limits precede the refinement. The local profile is not the remote process's
live state; some local executables were never built.

The added case and changed fixtures do not provide a supported source-level
explanation for the current longer elapsed time. Identifying a slow or waiting
case requires its current log or completed JUnit/LastTest evidence. Elapsed time
alone establishes neither a pass, a failure nor a confirmed hang. No speculative
repair or acceptance-gate change is proposed.

## Evidence

`summary.json` records source hashes and qualifications. `registration.diff`
preserves the exact one-test delta. `profile-comparison.json` compares the
immutable historical and candidate workflow steps; `timeout-profile.json` and
`local-configured-tests.json` preserve the local registry. The fetched historical
sources are `d0c3-CMakeLists.txt` and `d0c3-build.yml`.
