# A51 — corrected-source Intel Mac qualification (pending)

## Current execution

This continuation targets source
`c76ad288e02fbeca48f39652bf4fcdd99837f793`, tree
`416c1f6bfb4cd9f7dd5dbd9f5cbc211edda2bf00`, native run **45351731801**,
**attempt 1**, Intel producer **138299557267** on `macos-15-intel`.
At **08:54:00 UTC on 24 November 2026**, its complete native Test step remained
in progress after starting at **08:22:16 UTC**. Configure, pinned JUCE source
verification and build had succeeded. There were no Intel artifacts to inspect.
Native results, pluginval lanes and installable payloads remain **unqualified**.

The explicit attempt-one context belongs to this Intel producer. A separate
Apple Silicon job rerun may advance the workflow's latest attempt; it does not
make this producer an attempt-two execution. Final acceptance must use original
attempt-one APIs, this producer's steps and its original uploads.

## Source history remains distinct

The preceding [A51 db6b checkpoint](a51.md) is preserved. In that superseded run,
the Apple Silicon native suite failed its parity-capture schema test. The Intel
job was **cancelled**, not accepted or described as having failed that test.
[Original superseded outcomes](../ci/source-db6b5a7/superseded-summary.json)
remain unchanged.

Exactly one file differs between db6b and c76: `tests/test_parity_capture.py`.
The corrected contract requires 202 exported controls and both appended variant
defaults at Standard zero, with negative cases for stale, missing, renamed or
nonstandard variants. No production DSP, factory recipe, workflow or packaging
source changes in that commit. This source comparison is not a substitute for
the new actual native run.

## Prepared exact-source gates

The [source contract](../ci/source-c76ad28/macos-x86_64/source-contract.json)
retains eight exact Git input identities. The [Intel original-execution binder](../ci/source-c76ad28/macos-x86_64/qualify-macos-x86_64.py)
is reviewed and adapted from A50's c76 Mac binder. All original source/run/job
and upload bindings, complete native test rosters, JUCE provenance, four
validator lanes, actual process-exit propagation, AU whole-bundle comparisons
and VST3 packaging-path/order checks remain required.

A58's F1 closure is mandatory. In addition to the actual ZIP/API SHA-256 and size,
the binder joins the raw parser's archive records and every parsed member to
those same archives using the pinned independent
`bind-parser-archives.py` checker. It also binds the original parser's identity,
input-manifest bytes, source/run/attempt context and the independent checker's
five consumed inputs. Raw helpers retain their false release/attempt/platform
qualification fields. Only the separate original-execution binder can accept
the Intel platform after all actual gates pass.

The [verification runner](../ci/source-c76ad28/macos-x86_64/run-actual-checks.py)
pins the reviewed A41, A37, native parser and A58 helper hashes. It requires all
three real ZIPs and original inputs before creating any acceptance report.
It writes each command, log and exit status to fresh exclusive paths and never
executes a downloaded product. Syntax checking passed; **no actual artifact
inspection has yet run**.

The [pending record](../ci/source-c76ad28/macos-x86_64/acceptance-pending.json)
leaves actual result totals null and both platform/release qualification false.
The previous failed/cancelled source and the current partial execution remain
separate. No production, workflow, shared-build, old evidence or remote state
was changed by this assignment.

## Fifteen-minute checkpoint

The original Intel job remained in its Test step at the final checkpoint;
no Intel artifacts were available. Explicit attempt-one jobs and current
artifact-list API responses are retained. The prepared binder accepts both
exact CTest terminal formats used by the reviewed toolchains; its preceding
version and the narrowly scoped format adjustment are preserved. No failure,
roster, skip, source or original-exit gate was relaxed. This format observation
came from the actual ARM run and is not presented as an Intel result.

Root can reactivate this assignment after the original producer publishes its
three archives. The prepared runner and binder will then inspect those real
bytes and original execution records; no current acceptance is implied.
