# Host hardening composition and requalification — 2026-11-02

## Source identity and scope

The live draft PR #6 head was independently checked as
`finish/cs80-qualification-20261031` at
`9c62d7b8813a76ce178712c82da9604a1355b112`, not the older
`codex/cs80-completion` branch. The locally composed implementation tested here
is `d2d5528e66a6c78da493f5645647cfbd26a58ab4`, with `src` tree
`7b0d3db37f3c59958808e6e08280b1119ea5b83f`.

Independent review subsequently approved the production diff. Comment-only
commit `07dd2fad182b015eb143739a160f08cab1651aca` replaces the stale phrase
"untouched client" with "pinned vendored client"; its `src` tree is
`dbbefcc5ca0eeac5b741a90b916c0e79336246fd`. This changes no compiled behavior,
but later source-locked qualification must still use the actual final tree.

Only incremental reviewed host/MTS and async UI-test commits were transplanted
from `experiment/host-curve-extreme-offset-20261102` (`d934868`) and
`experiment/ui-readiness-async-click-20261102` (`dcb3ebb`). Whole branch merges
were deliberately avoided: both experiments predate PR6's latest causal/oracle
inventory. There is **no `src/core` change** and no deletion of PR6 tests.
Plugin source does change, so full-engine source-lock evidence cannot be
silently inherited as qualification of this composition.

## Local test-first reproduction

The raw logs and hashes are in `host-requalification-20261102/manifest.json`.
Compilation and execution held the shared heavy-job lock, after the CPU
same-host baseline rounds. GCC C++20 tests used exact-PR6 core objects only for
the shim's engine integration. No local GUI or actual DAW was run.

| Defect | Before | Reviewed repair | After |
| --- | --- | --- | --- |
| Mapped external MTS-ESP invalid Hz partially updates table; filtered invalid Hz is unsafe | 11 assertions fail on PR6 production (`0fc9543`, tests added only) | Stage all 128 entries; reject invalid mapped table atomically; finite fallback for filtered note | Shim passes |
| Vendored two-byte MTS scale degree carries prior sign bits | 2 native C-API assertions fail | Mask each 14-bit value before choosing denominator and clear accumulator | Native client passes |
| Windows SDK `max(a,b)` macro collides with `numeric_limits::max()` | Required adapter compile fails | Parenthesized standard-library member invocation | Macro compile and execution pass |
| Host initial NaN/infinity contaminates interpolation | Scalar assertions fail | Finite zero fallback before constructing curve | Scalar and UBSan pass |
| `INT_MAX - (-1)` overflows before double conversion | Scalar direction/denominator fail; UBSan explicitly diagnoses signed overflow at HostAutomationBridge.h:62 | Widen operands before subtraction | Scalar and UBSan pass |
| AU malformed previous state moves delayed immediate event earlier | 3 assertions fail after the generic host fix, isolating the AU construction issue | Sanitize initial value before building AU knots | AU scalar under UBSan passes |

The final eight focused program invocations pass: MTS shim, native C API,
disabled-client fallback, Windows macro simulation, host curve ordinary and
UBSan, AU curve UBSan, and AU atomic publication bridge. Original valid-curve
maximum error remains `2.22045e-16` for both VST3 and AU. Existing tolerances are
unchanged. The added `1e-14` comparison applies only to the newly registered,
deliberately out-of-range INT_MAX fixture.

The async UI test fix waits for the actual JUCE-posted click callback and one
balanced parameter gesture, then separately observes the no-mutation status
refresh interval. It retains missing/incomplete/duplicate observer negative
controls. It changes no production UI behavior or gesture assertions; its
preregistration documents the original Intel failure.

## Independently retrieved native CI logs

These are historical **exact-source** results, not composed-source results.
Compressed raw job logs preserve their uncompressed SHA-256 in the manifest.

* PR6 build [41373060591](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41373060591),
  checked-out PR merge `2d05a096298b28d873dc487c219e996bc3c20e01`:
  Windows 108/108; Linux 116/116; Apple Silicon 112/112; Intel macOS x86_64
  112/112. Both macOS logs include native AU automation, serial mutation tests,
  UI/teardown, and VST3/AU/AU-GUI validator SUCCESS. Linux and Windows strictness
  10 pluginval jobs also pass. Thus the previously pending Intel result is
  resolved **for PR6**, not automatically for a later final release.
* Host candidate `d934868` build
  [41406836196](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41406836196):
  all seven jobs pass; retained raw Linux 115/115 and Intel 111/111 logs include
  native GUI/host validations. DSP and screenshots also reported success.
* Async candidate `dcb3ebb` build
  [41407114883](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41407114883):
  all seven jobs pass; retained raw Linux 115/115 and Intel 111/111 logs include
  native GUI/host validations. DSP and screenshots also reported success.

The smaller candidate counts reflect their older ancestry; they are not grounds
for removing PR6's additional tests. Fresh native CI of the composed source is
required next. This report does not claim screenshot visual acceptance from a
green screenshot job.

## Remaining host acceptance boundaries

* Actual supported-DAW session/interaction acceptance remains open. Native
  wrapper fixtures and pluginval are not substitutes for a DAW. A separate
  bounded REAPER Linux smoke is being registered and must report its limited
  scope, raw artifacts and observed source identity.
* Strict panel golden comparison currently runs only on Linux; macOS/Windows
  golden drift is report-only. Native UI tests do not close strict cross-platform
  visual acceptance by themselves.
* Voice monitor telemetry now has live accessible text and a selected-voice/rank
  test; this does not imply every modern plot is fully accessible.
* MTS-ESP polling currently requests channel `-1`. Per-channel tables, external
  live-master interaction and scale-name display have not been newly qualified.
* Public-source panel geometry/lever inferences remain explicitly distinguished
  from manufacturer-specified behavior. No physical observation is fabricated.
