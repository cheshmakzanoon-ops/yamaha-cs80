# Final publication procedure — A60 preparation

This procedure is prepared for compiled source
`c76ad288e02fbeca48f39652bf4fcdd99837f793`, native run **45351731801**, and DSP
run **45351731746**, both attempt **1**. It is not a record of completed
publication. If the compiled source changes again, rebind the procedure and
all dependent evidence to the new exact source before using it.

## 1. Close the actual release gates

Read the final independent platform, audition, supporting and public-download
acceptances. Require the complete conditions in
[publication-acceptance-plan.json](publication-acceptance-plan.json). A planned
roster, a successful artifact upload, or a source-only helper cannot establish
native success. Keep the raw inspection helpers' deliberately limited
`release_qualified: false` fields unchanged.

The final review must specifically close A58's discovered parser binding gap:
each native and validator observation must identify the same ZIP SHA-256 and
byte count as the actual inspected archive and authentic receipt. Require the
corrected new-source binders and the independent archive-identity check.

Original failures and superseded runs stay in history. The 09af numerical
bounds step passed before its capture-metadata tests failed; the overall run
was later cancelled. The db6b run was cancelled overall, while its Apple
Silicon job had already failed its one capture CTest. Neither becomes a
successful release gate through the later correction.

## 2. Replace the download block as one consistent change

Use the final A57 mapping for four installation ZIPs plus the audition ZIP.
Replace all old IDs, build/run references, source links, native totals, skip and
retry totals, audition inventory/duration counts, qualification links and
expiry wording together. Check the exact public URLs against actual full GET
receipts; the historical .4 route-control result does not qualify new URLs.

The minimum expiry must include all five advertised artifacts and be converted
with `America/Halifax`, retaining UTC. Describe these as retained Actions
artifacts, without claiming permanent release assets. Preserve architecture,
format, signing/notarization and compatible-system-library limits.

Update the current README, revision report, distribution status and team
summary consistently. Preserve explicitly historical planning records,
original source attribution and raw limited helper outcomes. The binary source
remains c76ad288; a later documentation commit does not alter compiled bytes,
embedded manifests or source-time bundled guides.

The final prose must retain all 523 ordered identities and no-new-presets
claims, the two unchanged Init programs, and the distinction between measured
signal change and listening or acoustic-equivalence claims. It must not claim
that every tag is unchanged: the final coverage record has 14 changed tag
lists. Retain explicit LOAD/RELOAD and fresh-note instructions for old projects.

## 3. Freeze the reviewed publication content

Finish all tracked reviews and the current 60-worker ledger before the final
staged-tree check. The first six conversational assignments remain plainly
identified; a checkpoint is not a completed native qualification.

Re-read the authenticated remote `main` ref and fetch its exact object. Inspect
any advance since c76ad288 and preserve it. Do not build a new tree on the older
c76 tree while assigning a newer parent: that can discard intervening remote
files. Any changed production, test, build or workflow input requires renewed
source review, not an assumption that the earlier binary qualifies it.

Stage only an explicit reviewed manifest of README/documentation/evidence
paths. Check additions, modifications and any deletion individually. No
production/test/build/workflow change, historical .4 evidence change, cache,
compiled binary, downloaded ZIP or scratch WAV belongs in this publication
commit. Untracked `__pycache__` directories currently exist and must stay out.
Avoid a broad `git add -A`.

For every staged path, record the exact mode, Git blob ID, byte count and
SHA-256. Inspect the actual index rather than the later working-tree contents.
Use `git diff --cached --check` and reconcile the full staged path set with the
manifest. Confirm all newly linked relative files are included or already
tracked. Derive the expected tree with `git write-tree`.

Keep a manifest that contains its own final tree hash outside that tree. Do
not introduce a self-referential hash by adding that manifest or a report
containing the final tree's identity to the same tree after it was computed.
The final commit/tree receipt can be recorded in the coordinator's delivery
or a separately scoped later receipt. If any staged file changes, regenerate
and review the manifest/tree before upload.

## 4. Create and push the exact reviewed tree

Upload the staged contents using proper structured GitHub arguments. Verify
every returned blob SHA, and require the created tree SHA to equal the local
expected tree. Use the fresh, inspected remote head as the commit parent.

The final commit message must contain **`[skip ci]`**, for example:

```
docs: publish qualified 2026.11.5 builds and 60-agent review [skip ci]
```

Update `main` with **`force: false`** and **`expected_sha` equal to the freshly
inspected parent**. A lease rejection requires a fresh read and reconciliation;
it does not justify a force push or dropping intervening work. The user has
already authorized this publication. These are technical consistency checks,
not an additional approval request.

## 5. Verify the public result

Read the remote ref, commit and tree back. Confirm the published README
contains exactly the five accepted artifact IDs and the final source/run,
counts, expiry and evidence links. Confirm the production/test/build/workflow
objects remain those qualified by the compiled-source run, and distinguish
any later documentation-only commit in the delivery.

The coordinator should report the actual pushed documentation commit, compiled
source and four system downloads. Native success, archive identity and public
retrieval support those concrete claims. They do not establish human listening
approval or indistinguishability from real instruments.
