# A50 actual audition acceptance handoff

**Phase 1 only: the actual archive has not been supplied or accepted.** All
archive, audition, platform, release, listening, and acoustic-fidelity flags
remain false. This directory does not contain a replacement acceptance helper.

The frozen acceptance helpers remain A40's `source-ee0dabca` successors. The
content verifier has SHA-256
`bae2c82d291bee1240a3c49673a82381114f9bf4c0260275ea1b4494fda59df5`;
the CI binder has SHA-256
`65cf483ee4397533ea5ca4bf8d88eb4c5f21353059b99ba761833f7f288e2554`.
Always pass **`--repo .` from the repository root** to the content verifier.

## Retained preflight

`phase1/expected-contract.json` independently derives every expected WAV path,
program identity, note, velocity, pressure, duration, frame count, parameter
digest, and complete roster from the pinned Git source and independent A12
inventory. It compares the resulting 297-file contract with both original A28
and frozen A40 contracts. The 225-request prefix also matches the frozen `.6`
producer exactly. No helper fixture, audio render, or synthetic archive is run.

`phase1/input-pins.json` binds all 13 retained inputs. The preflight receipt binds
those inputs, the derivation script, and the prepared output files. Reproduction
requires a fresh directory:

```bash
python3 -B docs/preset-evidence/2026.11.7/verification/a50/preflight.py \
  --out-dir docs/preset-evidence/2026.11.7/verification/a50/phase1-fresh
```

## Actual inputs and commands

`phase1/command-input-manifest.json` is authoritative for the exact fixed argv
and pending actual-input slots. Angle-bracket tokens are deliberately unresolved;
they are not invented artifact IDs, hashes, sizes, or file paths. Fill only those
tokens from root's authenticated originals, retaining every fixed argument.
Record the actual resolved argv, process exit status, timestamps, output digest,
and log digest for each execution.

The original artifact must be named
`cs80-curated-preset-audition-ee0dabcacb4cdff93f8803844f60348e0ceda6ed`.
It belongs to source `ee0dabcacb4cdff93f8803844f60348e0ceda6ed`, tree
`17115913cbb29157bd00539485bcc5f38b57219c`, run **45397189410**, attempt **1**,
Linux producer **138455975798**. A later Intel attempt-2 run object cannot
replace the audition's original attempt-1 API record.

Keep the original ZIP compressed in `/tmp/cs80-ci-1cee5a/ee0dabca/`. The frozen
content verifier reads a single immutable ZIP byte snapshot, bounded below
512 MiB, and one member at a time without extracting full PCM. The expanded
archive remains bounded below 512 MiB and each member below 16 MiB. Do not
create a second complete audio copy. Acceptance outputs belong under
`ci/source-ee0dabca/audition/`; all output paths must be fresh.

Content acceptance must inspect every actual sample, recompute all 12 PCM
metrics, verify all checksums, and compare the delivered inventory against
the retained A12 reference. Then bind the content report to the actual artifact,
run, job, and log originals. Hash and retain new acquisition receipts without
replacing prior receipts.

The separate producer review must reconcile original checkout/build/test/capture/
upload steps and the direct capture command's real successful process outcome.
An upload can happen after capture failure, so upload presence is insufficient.
Where a successful GitHub shell step has no literal exit line, state precisely
how the original success conclusion, direct `bash -e` command, and unchanged
producer's `check=True` children establish exit zero. Record any missing evidence
as pending; never invent a log line or relax a helper.

Successful technical acceptance does not establish human listening approval,
exact acoustic fidelity, live gestures, independent capture-executable possession,
any platform gate, or overall release qualification.
