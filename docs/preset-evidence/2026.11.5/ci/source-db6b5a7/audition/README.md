# Audition qualification checkpoint — source db6b5a7

This directory retains **preparation only** for source
`db6b5a76b3c5e38dd9444d0cf6065a14f6302814`, native run **45346719693**, attempt **1**.
The captured API checkpoint shows Linux producer **138282948362** still installing
build dependencies. Its audition step has not executed and the queried workflow
artifact list was empty. No ZIP or WAV samples were inspected.

The coordinator subsequently reported a stale 200-parameter expectation in a
separate supporting Python test and plans a test-only source correction. These
records retain their db6b attribution. A later source/run needs fresh actual
acquisition and an explicitly selected source contract; it must not inherit a
passing result from this checkpoint.

## Completed source preparation

The unchanged `.5` content checker was executed with `--prepare` against the
explicit Git commit and the separately retained final inventory. It confirms
the source's declared **171 WAV requests**, **81 existing programs**, **40 groups**,
**1,033.2 seconds / 49,593,600 stereo frames**, and **216 regular files / 215
checksummed members**. The full inventory contains **523 × 202 = 105,646 binary32
values**. Its SHA-256 is
`fd91cc5dde92bf9f823114a5c07e609961d92cd69080b6b6791191e86ba2bfa1`.

The independently resolved request list covers all 19 active body selections
and Standard plus four material/vowel selections. These are expectations from
source and inventory, not measurements of a produced recording.

- [Source-bound checker preparation](source-contract.json)
- [Resolved roster and inventory contract](roster-inventory-contract.json)
- [Explicitly pending acceptance](acceptance-pending.json)
- [Original jobs checkpoint](jobs-checkpoint-api.json)
- [First-page artifact wrapper projection](artifacts-checkpoint-api.json)

## Actual acceptance on reactivation

Acquire the actual ZIP and fresh unwrapped artifact API through authenticated
GitHub tools. Reuse A52's complete original Linux producer log and original
attempt APIs; record the identities of those retained bytes. Keep ZIPs in
scratch and do not extract or duplicate the audio unnecessarily.

Run the unchanged
[`verify-audition-archive.py`](../../../verification/verify-audition-archive.py)
with the **actual expected source, run and attempt**, the actual API size and
SHA-256, and the independent inventory pinned to that source. Its output must
remain `release_qualified: false` even after complete content acceptance.
The checker inspects all ZIP membership and checksums, all 171 WAV samples and
12 recomputed metrics, exact request settings, ordered program identities and
all binary32 inventory values.

Then run [`bind-audition-upload.py`](bind-audition-upload.py) with explicit
`--repo`, `--source`, `--run-id`, `--attempt`, `--producer-job`, `--run-api`,
`--jobs-api`, `--artifact-api`, `--producer-log`, `--content-report`, `--archive`
and a fresh `--out` path. It checks the source/run/attempt, actual hash and size,
original successful producer steps, capture command, upload action name/path,
artifact ID/digest/size, and upload timestamps. It accepts only an already
accepted exact-source content report from the unchanged verifier. The binder
is syntax-checked only at this checkpoint; no actual acceptance has executed.

The binder deliberately remains separate from whole-release qualification and
cannot authenticate files supplied by a caller. Authenticated acquisition is
an external evidence requirement. Capture-executable stability is established
by source-pinned producer provenance; no independently downloaded capture
executable is claimed. No human listening approval or acoustic equivalence is
inferred from either checker.
