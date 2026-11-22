# Preset refinement publisher review and attempt repair

Observed: 2026-11-22 16:34 UTC. Scope: the one-off unsigned development publication for preset revision 2026.11.1.

## Result

The requested first-attempt-only repair is complete. All 13 offline publisher tests pass, retaining the original 11. No additional blocking source/asset implementation issue was found in this bounded review. Actual publication remains gated on completion and success of build **45044040694**, source **6d3cc220edef5a4a2c7cd968ea8a786c7c83198c**. The API still reported that build and its four native jobs in progress; no publication was attempted.

Changed only these three assigned repository files:

- `scripts/publish-preset-refinement.py`
- `scripts/test-publish-preset-refinement.py`
- `.github/workflows/publish-preset-refinement.yml`

No staging, commit, push, rerun, workflow dispatch, release mutation or edits to the original failure evidence were performed.

## Attempt policy

The build API record must carry integer `run_attempt == 1`. Every required job must carry the same integer attempt 1 and the fixed build/source identity. A workflow-run trigger must also identify attempt 1. The audition provenance must carry the exact string `"1"`, matching the capture recipe's environment-derived format. Missing attempts, reruns, wrong types and mismatches are rejected.

`release-provenance.json` now records `build_run_attempt`, with `run_attempt` on each required-job record. The existing latest-jobs query cannot admit a successful rerun because both the enclosing run and every returned job are constrained to attempt 1. Raw GitHub REST evidence confirms that the live jobs expose both `run_attempt: 1` and the exact source SHA; the connector's normalized jobs wrapper omits those fields, but the publisher uses the raw REST endpoint.

Focused fixtures cover missing/wrong build and job attempts, mismatched audition attempts, successful reruns and mismatched trigger/job attempts before any mutation. Existing success, archive refusal, complete-payload-before-draft, asset integrity, partial-draft resume and published-release idempotence fixtures remain intact.

The YAML diff changes only its two source references and two run-ID references: `be460b0… / 45042166237` becomes `6d3cc220… / 45044040694`. Event conditions, required success, permissions, concurrency, absence of user inputs and step definitions are otherwise byte-identical.

## Source and asset review

All six required job names and every named required step match the current build workflow. The publisher requires whole-build success and every named native test, provenance, package/upload and stable-validator gate to pass. It does not substitute the beta comparison for a failed stable validator.

The fixed inventory SHA `fe3ddc6ecdb9fa25e77170f1cd9f81e6f2fe0ef5aabd105506a0fa487eb286fa` matches the committed final-identity evidence. A direct comparison of the Python capture selection literal and publisher table confirms four folders, exactly 24 programs and 117 seconds, with matching pressure, hold and tail settings.

The publisher binds all five input artifacts to the exact build/source and requires a unique, unexpired artifact set with GitHub digest and size matches. It checks the four package manifests, inner/external checksums, exact source/revision, JUCE provenance and native binary headers. It verifies the audition inventory, all 32 checksummed members, capture completion, program/value identities, WAV format/duration/finite sample bounds and recomputed metrics. It uploads the original artifact ZIP bytes; the synthetic transaction test confirms that all five payload verifications precede draft creation.

Source-to-publication comparison requires the fixed source as ancestor and permits only README, docs and the three named publisher files. The unrelated diagnostic YAML would be rejected and remains omitted. Existing tags or assets cannot be moved/replaced; a conflicting publication fails. The seven-asset set is rechecked before making the draft public.

## Contract and evidence boundaries

This publisher proves the six-job build and artifact contract for unsigned CI development bundles. It does not claim signing, notarization, all-DAW qualification or hardware/acoustic equivalence. The separate installer-release process in `docs/release/RELEASE_QUALIFICATION.md` calls for exact-source build, DSP and screenshot success; these auxiliary workflows are not automated dependencies of this one-off publisher. Retain that distinction in publication wording.

The original d0c3 attempt-1 Linux post-SUCCESS SIGSEGV remains historical failure evidence. A later source build passing cannot establish that the host issue was fixed. The first-attempt policy prevents silently replacing an earlier attempt of this new build, while the original d0c3 history remains in the separately maintained documentation.

## Earlier stale guard proof

The d0c3-to-be460 comparison showed all eight canonical validator step dictionaries unchanged apart from the headless step's new diagnostic `id`; all eight validation shell payloads were identical. The Linux wrapper was identical at SHA-256 `41aa495e5fe84cddfe37c2f415e65cff0db71e72c8d549b67c1bbf5e4c5af105`. Only the Linux whole-region fixture needed its expected hash changed from `dd36be196c6f754017a19517bc9974b0d0b1e376ae4e4af4b5143e6cef78e9a2` to `c72b9d5e34364eb77ff9a62d546e7b1c1a031d7f448f5e71b188a6cd95227caa`. Windows and macOS region hashes were unchanged. Root published that isolated guard correction as 6d3cc220 with ordinary CI.

## Validation

- `python3 scripts/test-publish-preset-refinement.py`: **13 tests passed**.
- `git diff --check` for the three assigned files: passed.
- Read-only source/contract comparison: six required jobs and their step names, 24 selections/117 seconds, final inventory hash and literal-only YAML retarget all matched.
- Supporting raw run/job API snapshot: `publisher-review-6d3cc220-evidence.json`.

