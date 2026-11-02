# Future source-bound full engine audit V2 — implementation handoff

**Status: proposal only, deliberately not preregistered.** The exact final production `src` tree is not yet known because CPU/click engineering is ongoing. Do not edit the frozen V1 files or replace their historical run:

- `.github/full-engine-audit-request.json`
- `scripts/full_engine_audit_gate.py`
- `.github/workflows/full-engine-audit.yml`
- `docs/evidence/qualification-2026-11-01/pre-ring-local-integration/summarize-audit.py`
- `tests/parity/tolerances-v2/ws1-voice.json`
- `tests/parity/tolerances-v2/completion-dsp.json`

## Why new registration is required

V1 `full_engine_audit_gate.py` anchors its request, reporter and two tolerance files byte-for-byte to commit `00da4498792523cf8d97fd40c17e12f215e190b8`, then demands `git rev-parse HEAD:src` equals `7dd159d491012fd47b46c0aa651e86ea959ca289`. It correctly refuses future `src/**` changes even if they are safe plugin-only changes. Its original artifact is [run 41336194253](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41336194253/artifacts/13097476296), 885 rows; many click scores fail. Preserve this proof.

The V1 workflow also runs on `src/core/**` pushes. A future core edit may therefore cause the historically source-locked V1 job to fail with an expected identity mismatch. Do not weaken that job to manufacture a green result; use a separately named current-source V2 check and document which test is historical.

## Reviewable V1 → V2 workflow/check migration (source changes only)

1. **Preserve V1 evidence and source rejection.** Do not edit the frozen V1 request, gate, reporter, two tolerance JSONs, or completed original ZIP/provenance. A `src/core/**` commit **F** before V2 registration can trigger V1 and produce a truthful red *historical-source mismatch*. Record that run and its exact F SHA; it is not a DSP or plugin failure, nor may it be marked green.
2. **Versioned source binding.** Commit registration **R** with `expected_src_tree=T=F:src`; inspect the R tree to prove it contains the exact unmodified frozen reporter/limits and complete 885-row schema. Commit wrapper/workflow **B** afterwards. Require the V2 workflow to preflight `HEAD:src==T`, immutable request bytes at R, reporter/tolerance bytes at R, and `F` ancestry. Include `src/**`, new V2 request/wrapper/workflow and relevant tolerance paths in V2 push triggers; use a unique check name and concurrency group. The B push activates V2 on the feature branch without modifying or triggering V1 solely through new V2 paths. V1 remains available for a historical replay at its registered source.
3. **Required-check ownership is an explicit approval boundary.** Query the actual branch protection/ruleset and current PR required check identities (read-only). If V1 is required on a changed source, it is impossible for its frozen preflight to pass honestly. Stop merge. Prepare a reviewed, atomic ruleset migration that **adds the source-locked V2 audit check and separate strict numerical-acceptance check as required first**, and only then retires the V1 historical check from the *current-source* required set. Keep the old V1 workflow runnable and its failing intermediate-F run visible. Record old/new check context names, ruleset revision, reviewer approval, F/T/R/B, and the new completed V2 run before treating the migration as effective. Never create an unguarded interval or bypass a required check locally.
4. **V1 trigger decision.** Prefer leaving `.github/workflows/full-engine-audit.yml` unmodified: it will still fail accurately on any later `src/core/**` push, signaling that the old source registration is obsolete. The final qualifying B commit touches only V2 files, so it produces the V2 check and need not invoke V1 anew. If the repository owner explicitly requires an archival/manual-only V1 trigger to avoid legacy red runs on future core commits, treat that as a separate, reviewed workflow/ruleset migration **after** V2 registration and its equivalent-or-stronger enforcement are established. Do not silently skip, disable, or rename the old required check.
5. **Release acceptance remains independent.** A complete V2 capture is green infrastructure even with numerical failures. A fail-closed fourth numerical release check must verify the exact audited B/T and the original peak, click, alias, CPU and causal thresholds from unmodified reporter/raw evidence; do not promote a green capture to `PASS`. After a merge commit, verify the merged main source tree still equals T and rerun all required main checks at that merge SHA.

## Safe commit sequence after final `src` is frozen

1. Accept only fully reviewed CPU/click audio changes, with the required **separate** production audio commit, full-SHA `SOURCE_AUDIO_V2` pin commit, and strict Legacy/V1/V2 compatibility result. If only a plugin integration changes `src/plugin`, the all-src identity still changes; do not claim it is the old registered source tree.
2. Record the exact immutable SHA of the selected final source commit **F** and `git rev-parse F:src` tree **T**. Do not start before all expected `src` edits have landed on the feature branch.
3. Add `.github/full-engine-audit-request-v2.json` as a new preregistration, derived from V1 but setting `expected_src_tree=T`, `production_audio_commit` to the true latest audio SHA, `registered_utc` to the new date, and a purpose clearly labelled V2. Keep `quick=false`; keep `44100,48000,96000` and their order; same `--suite all`, 5 suite files, 885 rows, 222 factory programs and 11 tab pairs. Keep the same unchanged existing tolerance file paths and frozen reporter. Verify their bytes against V1 registration.
4. **Commit the V2 registration first**, on a dedicated reviewable branch. Record its full SHA **R**. It must be an ancestor of the source branch to audit; do not rewrite this commit after seeing results. A change to `src` after R requires a new separately preregistered source identity, not an edit of R.
5. Add thin wrapper `scripts/full_engine_audit_gate_v2.py` and separate `.github/workflows/full-engine-audit-v2.yml` in another commit. The wrapper imports V1's *unchanged implementation* and selects `REQUEST_PATH=.github/full-engine-audit-request-v2.json` and `REGISTRATION_SHA=R`; `registered_contract()` then checks all frozen request/report/tolerance bytes against R. Use a **distinct workflow/check name and concurrency group**. For the initial pre-merge qualification, include a narrowly scoped `push` trigger on the new V2 workflow file in the exact feature branch, so the wrapper/workflow commit triggers the source-bound audit once. GitHub's `workflow_dispatch` requires the workflow file on the default branch before a new workflow can be manually dispatched; leave that trigger available for post-merge reruns, but do not rely on it for the first feature-branch run. Avoid broad automatic hour-long audits during ongoing development. Preserve `if: always()` upload of completed or partial evidence. The runner should enforce an expected F/source tree before an expensive build, so reruns of later source cannot be mislabelled.
6. Before the full run, prove mutation controls fail appropriately: wrong/stale source tree; altered V2 request; changed reporter; changed tolerance bytes; missing, duplicate or malformed CSV row; bad causal program/event identity; missing rate/coordinate; unregistered extra suite; inconsistent completion marker. Preserve negative and positive fixture logs.
7. Dispatch the exact all-suite audit at the registered source. Validate completion marker is last, ZIP SHA-256, source/registration/report hashes, five file hashes and row counts, and every numerical result. Include **all** failing scores without threshold changes; successful capture is not acceptance. If the final source still fails click/alias/peak gates, keep status open and repeat the engineering/registration process for the next genuinely changed source.
8. Only after the approved final source genuinely meets all required numerical targets may the release integrator merge and verify exact-main build, DSP, screenshots and reviewed release. A new exact-main source tree after the merge or screenshot-only commit must be checked against the correct versioned registration; source byte equality, not an older green PR run, is the identity test.

## Recommended V2 wrapper shape (illustrative, not yet committed)

```python
from pathlib import Path
import full_engine_audit_gate as frozen_gate

frozen_gate.REQUEST_PATH = Path('.github/full-engine-audit-request-v2.json')
frozen_gate.REGISTRATION_SHA = 'FULL_SHA_OF_PREREGISTRATION_COMMIT_R'

if __name__ == '__main__':
    frozen_gate.main()
```

Replace the placeholder only after R exists, in a separate code commit; do not backfill a run into a prior registration. For a stronger contract, add a separately preregistered acceptance check that reads the frozen JSON reporter's `existing_target_comparisons` and fails when any `all_exported_values_within_target` flag is false, while always retaining the raw artifact. Never alter the source-qualified V1 reporter or source-defined tolerances to avoid a red result.

## Evidence obligations

- Explicit full SHAs: F, T, production audio, V2 pin, R, wrapper/workflow commit, audited run, and eventual exact main M.
- ZIP SHA, binary SHA, reporter SHA, original contract hashes, all CSV hashes and counts, nonfinite reporting, workflow job links.
- Source phrase peak, retrigger, steal, single/chord off; causal PANEL/electrical/tab scores at three rates; ring-off alias at 48 kHz; nodal and CPU in their independent suites.
- A line distinguishing **completed audit capture** from **all targets pass**.
- No claim of hardware measurement, independently installed MTS master, signed installers, or owner-only DAW sessions.
