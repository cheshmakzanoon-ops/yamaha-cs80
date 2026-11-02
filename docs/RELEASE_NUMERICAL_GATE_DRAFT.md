# Numerical release qualification — preregistered additive guard (DRAFT)

**Registered 2026-11-02. Implementation and test fixture are on an isolated
review branch, not on `main`, not a release workflow, and not permission to
dispatch.** Existing build/DSP/screenshot pass checks, immutable main-head
checks, screenshot-only ancestry, and the original full-engine-audit
preflight/finalize gate remain necessary and are not weakened.

## Evidence contract

The machine-readable source of truth is
[`/.github/release-numerical-contract.json`](../.github/release-numerical-contract.json).
It pins the frozen original V1 reporter SHA-256
`3f1a68208c8c848c645088dae8d1a398190f32c05c1344bbffd3f4c071aa2fd9`
and its **nine existing** full-audit targets. In particular, the Source phrase
peak ceiling is 0 dBFS; Source retrigger, steal, single note-off and chord
note-off ceilings are each 1.0; ring-off Source alias at 48 kHz must be
at most −70 dB; Source causal panel, electrical programmer and tone tabs
ceilings are each 1.0. No numerical threshold is relaxed.

One complete registered `--suite all --rates 44100,48000,96000` audit must
contain exactly 566 phrase, 168 alias, 102 FX, 40 latency and 9 causal rows
(885 total), both Source and Legacy policy coverage and all three sample
rates. The gate checks original raw-file SHA-256/length/count against the
summary and source-locked provenance, `audit.complete` against its digest and
provenance digest, no nonfinite exported values, and every V1 target's
**independently recalculated** max and number of exceedances. A successful
capture or full-audit CI job is not numerical PASS.

The same-host CPU comparison requires **three complete before rounds and
three complete after rounds**, 51 CSV rows and 27 cases per round, the exact
48 kHz/256-frame isolated warmed-held workload, at least 1,024 callbacks per
held phase, the same 11 paired-tone identities and one quiet-idle identity,
the same benchmark driver/compiler/flags/runner job, complete finite rows
and same-source checksum/peak/RMS textual consistency. For each fixture the
lowest of its three after-run median RTFs is selected; its p99 and deadline
miss count come from **that same run** and all three raw measurements remain
visible. Every selected held preset must be ≤0.20 RTF and quiet idle ≤0.02.
These are callback real-time factors, not aggregate operating-system CPU.
The before source is fixed to
`7c864f266a939aa2a42422440dd4c1c0cabe3814` and the common benchmark
source blob to `92ea7e0b1801bf98fccaed21c13e676890039c94`; neither can be
substituted with a convenient faster reference.

## Required trusted producer (not yet wired into CI)

1. An exact-main workflow checks out reviewed source commit `M`, obtains
   `git rev-parse HEAD` and `git rev-parse HEAD:src` from the checked-out
   tree, and confirms the prior comparison commit `B` is its ancestor.
2. The original reporter and registered full audit run *at M*, preserving
   the actual CSV files, summary, provenance, `audit.complete` and frozen
   reporter. Separately, the benchmark runner produces 3+3 complete raw
   rounds on **one host in one CI job**, before `B` and after `M`.
   `scripts/run_release_cpu_producer.py` is a local preregistered producer:
   it refuses non-main/non-CI context, checks M and M:src, checks B ancestry
   and both benchmark Git blobs, compiles the two exact sources before
   timing, then alternates B/M rounds 1–3 serially. Partial/failed runs
   cannot emit its final completion marker.
3. The trusted runner generates a `qualification-receipt.json` **per round**
   with `host_id`, `runner_job_id`, `compiler_id`, `flags_sha256`,
   `benchmark_source_sha256`, `runner_environment_sha256`, and a unique
   `capture_id`. The job must bind `runner_job_id` to its actual CI job ID,
   not accept a caller-supplied artifact pretending to be from that host.
   Preserve the binary/build/workload provenance used to construct each
   receipt. All six receipts and benchmark metadata must agree on the
   environment/driver, while source commits are respectively `B` and `M`.
4. The job calls `scripts/release_numerical_gate.py` with its trusted
   `--expected-main-sha`, `--expected-source-tree-sha`, and
   `--expected-runner-job-id`, plus source identity, original audit directory,
   `--before-round` ×3 and `--after-round` ×3. It fails closed and emits a
   machine-readable summary. Production use requires the workflow to verify
   artifact digests and the actual runner receipt source, not only a copied
   self-asserted JSON document.
5. The isolated `scripts/dispatch-reviewed-release.cjs` addition now expects
   a **fourth** exact-main/push success called
   `release-numerical-qualification.yml`, in addition to its unchanged build,
   DSP and screenshot checks. If that workflow is missing, pending, failed,
   PR-only or from another source SHA, dispatch is denied. The original
   screenshot-only `S` ancestry and duplicate-release check still apply;
   a successful capture job does not substitute for the fourth workflow.
   This guard must **not** land on main ahead of its fully tested trusted
   numerical producer workflow, but in the meantime the missing workflow is
   intentionally fail-closed.

### Manual and tag release entrypoints (isolated preflight proposal)

Review exposed an independent bypass: `release.yml` accepts both direct
`workflow_dispatch` and `push` of `v*` tags, so a fourth check in the
separate dispatcher alone does not protect installer construction. The
isolated `scripts/preflight-release-entrypoint.cjs` and `release.yml`
changes reuse the **same** reviewed main/ancestry/four-workflow proof but
require `.github/release-request.json` from the explicit reviewed request
branch. They reject direct manual events not on `refs/heads/main`, require
manual event SHA = checked-out S, and for tag pushes require a semver `vX.Y.Z`
ref whose **peeled** lightweight/annotated target and event SHA both equal
the checked-out approved current main S. An existing active or successful
release for S also blocks. Every installer job `needs: preflight`, checks
out the exact S from its verified output, and rechecks that main has not
moved before uploading artifacts. The shared screenshot-only `M→S` comparison
also rejects renames with missing old-path metadata or with either the new
path **or the previous path** outside `docs/screenshots/`: moving production
code into the screenshots folder cannot count as a screenshot-only refresh.
None of this code dispatches a release.

This is locally tested, **not** on default main. Its protection assumes the
updated workflow definition executes. A tag pointing to an older commit
that contains an old unguarded release workflow might still execute that
historical definition; similarly, a direct manual dispatch deliberately
selecting an older workflow ref is outside the reach of a guard added only
to future commits. On 2026-11-02 the repository's read-only GitHub REST
`/rulesets?includes_parents=true` response was an empty list `[]`; no tag
ruleset was observed. **Owner-admin release blocker:** before any installer
publication, establish and verify protected creation of `v*` tags **or** a
protected `release` environment with actual owner approvals covering all
installer jobs, and manually review the tag's peeled commit against current
approved main S. Do not create tags or alter repository rules without new
owner authorization. Code-only preflight is not complete bypass prevention.

### Privileged request-branch execution removed (isolated proposal)

The historical `.github/workflows/dispatch-reviewed-release.yml` used a
`push` event on the mutable `codex/cs80-release-dispatch` branch, with
`actions:write`. Both its workflow YAML **and** its default checked-out
`scripts/dispatch-reviewed-release.cjs` came from that branch. Merely
pinning a checkout inside that YAML would not protect against a branch writer
who replaces the workflow definition itself.

The separate red-first `trusted-main-dispatch` proposal removes the
request-branch `push` trigger entirely. It permits only a repository-owner
`workflow_dispatch` on `refs/heads/main`, with a required **full request
commit SHA**. Before executing project code, the trusted default-main
workflow API-checks current main against the event SHA, then checks out
that **immutable SHA** for all scripts/tests. Its main-pinned dispatcher
reads exactly `.github/release-request.json` via GitHub API at the supplied
request commit, requires the request branch tip to match, and accepts only
a single JSON change relative to current main; no request-branch executable
code, workflow YAML or artifacts are checked out or run. It also verifies
authenticated owner/request identity, PR #6 being closed/merged/non-draft
onto main with the exact reviewed head and merge SHA, and the original 3+1
exact-main green workflows before calling the existing dispatcher. The
initial conservative request contract requires the validated SHA to equal
that exact PR merge commit; integration must review whether a later
non-`src` main successor needs a separately preregistered safe ancestry rule.
The independent `release.yml` **direct manual and tag** preflight also calls
the same PR #6 merged-identity verifier, so it must reject an unmerged/draft
PR, changed head/merge SHA, non-main PR base or a request whose validated
commit differs from the actual PR merge commit, even if the four workflow
statuses happen to be green. A reviewed screenshot-only current-main `S`
after the exact PR merge `M` remains permitted by the existing ancestry gate.

Red-first test cases reject forged branch scripts/YAML, extra diffs, stale
main/request refs, wrong event/actor/checkout/request SHA, altered PR identity,
missing numerical CI and incorrect workflow event wiring. This is still
**local review-only code**, not an approved release request or active default
branch. Historical `workflow_dispatch` YAML selectable at older refs and
unprotected owner-level branch/tag settings remain owner-admin blockers;
manually choosing `main` in this new workflow must not be represented as
retroactively disabling old workflow definitions.

### Producer-workflow integration still blocked

The frozen V1 full-audit preflight is source-tree-locked by registration
commit `00da4498792523cf8d97fd40c17e12f215e190b8` to production `src`
tree `7dd159d491012fd47b46c0aa651e86ea959ca289`. Any future production
DSP fix needs **new preregistered exact-source capture evidence** while
keeping that historical gate and its nine limits intact. A workflow which
simply downloads the old 39ae6c artifact, alters the frozen registration,
or treats a green audit capture as PASS would be invalid. Therefore a new
`release-numerical-qualification.yml` workflow is deliberately **not yet
supplied**. It must be preregistered and tested once M, source identity,
original V1 reporter compatibility and the exact-source audit pipeline
are known; it should run the complete audio audit and 3+3 CPU producer,
verify raw same-run artifacts/digests and source identity in a final
qualification job, and go green only after `release_numerical_gate.py`
returns `pass=true`. Review its actual job/runner provenance and mutation
controls before enabling release dispatch.

## Red/green verification

`tests/release/test_numerical_release_gate.py` was committed first with a
deliberately failing import (preimplementation RED). The standalone
implementation passes synthetic full-coverage numerical acceptance and
rejects missing/relaxed scopes, falsified maxima/pass flags, honest red
reports, nonfinite data, wrong source/tree/reporter/manifest digest, quick
or incomplete capture, missing/mixed CPU rounds, altered checksums, lost
fixture rows, broken source ancestry, and median held/idle overruns. It
additionally tests selected-round p99/miss attribution. The original 885-row
archive is tested **unchanged** when `CS80_ORIGINAL_AUDIT_DIR` is provided;
it correctly fails at Source phrase retrigger.

The frozen original source audit is from source `39ae6c2684456950302cb4ed10ecfae980c37ddd`, with ZIP SHA-256
`a76dd6ee33dcfa4f14be4223ecb06331330d19d90a0baf84147745cef2c88f19`
([Actions artifact 13097476296](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41336194253/artifacts/13097476296)). Its scorecard fails six
sound-quality scopes: retrigger 1/283, steal 1/283, single off 22/283,
chord off 37/283, causal panel 2/3 and causal tone tabs 3/3. It therefore
**does not** qualify any later source `M`, nor does it numerically pass at
its own `39ae6c` source. CPU timing also remains above release bounds.

No code in this draft dispatches a release; no hardware/DAW/audio-unit host
equivalence, signed installer or final handover is claimed.
