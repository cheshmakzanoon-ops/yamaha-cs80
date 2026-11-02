# Numerical release qualification — preregistered additive guard (DRAFT)

**Registered 2026-11-02. Implementation and test fixture are on an isolated
review branch, not on `main`, not a release workflow, and not permission to
dispatch.** Existing build/DSP/screenshot pass checks, immutable main-head
checks, screenshot-only ancestry, and the original full-engine-audit
preflight/finalize gate remain necessary and unchanged.

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

## Required trusted producer (not yet wired into CI)

1. An exact-main workflow checks out reviewed source commit `M`, obtains
   `git rev-parse HEAD` and `git rev-parse HEAD:src` from the checked-out
   tree, and confirms the prior comparison commit `B` is its ancestor.
2. The original reporter and registered full audit run *at M*, preserving
   the actual CSV files, summary, provenance, `audit.complete` and frozen
   reporter. Separately, the benchmark runner produces 3+3 complete raw
   rounds on **one host in one CI job**, before `B` and after `M`.
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
5. **Only after independent review and CI tests**, make the existing release
   dispatcher additionally require the numerical qualification workflow's
   successful **exact-M** conclusion and its source-bound PASS receipt before
   sending `.github/release-request.json`. Preserve its current three green
   exact-main checks and screenshot-only `S` ancestry. A green build, DSP,
   screenshot or *capture* job must not satisfy or override this fourth gate.

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
