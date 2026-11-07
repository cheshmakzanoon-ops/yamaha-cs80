# Packaging-only qualification of an existing Linux build

`qualify-linux-deb.yml` is an isolated, **scoped PR or manual** workflow. It does not
compile, load or execute the product, install the generated `.deb`, run validators,
change ordinary CI/check statuses, publish a release or replace existing gates.
It uploads **text/JSON ABI-packaging evidence only**, not a distributable package
or release certificate. The generated `.deb` remains in ephemeral runner storage.

## Pinned inputs and execution boundary

- Fixed PR source and manual default: successful first-attempt same-repository **push** build run
  `42351562098`, commit `635ad6e8f24978040b02a4a10e646d7e3c592ce4`.
- Its Linux install bundle is artifact `13566771073`, 12,413,145 bytes, ZIP SHA-256
  `01b00fc1e2c32c840507ed27052e9cb9cbf67cf3cc0149df920233be3bb3a29d`.
  Those actual bytes were downloaded and verified locally, not inferred from a
  different platform/archive. They contain the expected checksum file, manifest
  and tarball; the manifest has 13 entries, including 9 regular files.
  PR execution requires this exact run, commit, artifact ID, size and ZIP digest.
  Missing, expired or different artifacts fail closed without substitution.
- The workflow checks out its reviewed helper separately from the packager.
  The **packager checkout is pinned to commit `635ad6e8`**, and the helper verifies
  its full commit ID, clean tracked state and exact `make-deb.sh` Git blob
  `4342b31ddc5762fcc2a5c3dca7842ac3abdf4df2`. It cannot accidentally use the old
  unversioned-dependency packager from main `185e04c`. Later main `b363b325` has
  the same corrected tree, but moving refs are not substituted for this pin.
- Both checkouts are shallow/sparse and do not persist Git credentials. Official
  checkout/upload Actions are pinned by full commit SHA. The job has only
  `contents: read` and `actions: read`; it uploads only to this run's Actions
  artifacts. The repository token is not passed as a command argument or stored
  in evidence.
- Ubuntu 24.04 installs official native runtime libraries and packaging tools.
  No synthetic `dpkg` database, altered library search path or missing native
  `/var/lib/dpkg/status` is accepted. No product recompilation is required.

## Gates before evidence is uploaded

1. Authorize the runner-provided event and bind it to the checked-out Git commit
   and current workflow-run metadata. Reject fork/mismatched repositories,
   privileged or unsupported events, unapproved PR actions, non-main PR bases
   and unpinned PR source inputs. Then validate numeric source run ID/full SHA and
   check the same repository and
   head-repository IDs, expected ordinary build workflow, push event, exact head,
   completed success and first attempt. The current artifact format has no attempt
   identity, so later reruns are deliberately rejected rather than trusting an
   artifact left by an earlier failed attempt. Recheck run success before upload.
2. Require exactly one unexpired `cs80-linux-install-bundle` from that run with
   matching provenance, size and GitHub SHA-256. Other artifacts are not downloaded.
3. Compare source/build-input **Git object metadata**, without reading or running
   other workflow/validator source. Reject changes outside documentation and the
   explicit packaging-file allowlist. The pinned packager's source must match the
   selected binary's production/build inputs.
4. Verify outer ZIP layout, inner checksums, both manifests, every tar file hash,
   mode and size, SOURCE commit and clean pinned JUCE provenance. Reject traversal,
   absolute/ambiguous paths, duplicate entries, links, devices, special modes and
   oversized archives. This qualifier intentionally supports the verified regular-
   file Linux bundle layout; it rejects links even though the general packager can
   preserve certain internal relative links.
5. Materialize only the verified product files and JUCE licence into conventional
   build paths under a fresh runner-temp directory. Inspect ELF headers/dynamic
   tags/version requirements using `readelf`; never execute an artifact payload.
6. Run the pinned packaging script against genuine native library metadata.
   Scanner errors or warnings fail the qualification. Failure diagnostics are
   safely escaped into the job log; failed packages are not uploaded.
7. Inspect the `.deb` without installing it. Require versioned libc6/libstdc++6
   floors, explicit dynamic-library dependencies, amd64 architecture, expected
   root ownership/modes, no maintainer scripts or links, and exact payload and
   notice bytes. Extract only verified regular-file bytes into an isolated path.

Only then is `linux-deb-abi-evidence-<qualification-run-id>` uploaded. Its output
is restricted to eight allowlisted UTF-8 text/JSON files containing the package
digest, declared dependencies, per-file hashes, source manifest metadata, JUCE
provenance, ELF inspection text, packaging output and native package versions.
The report explicitly declares `package_uploaded=false`.

The native `.deb` is still created, inspected and extracted inside runner temp,
but **no `.deb`, ELF/product bytes or product/source archive is copied into the
qualified output or uploaded**. Unexpected files, links or binary evidence fail
the output gate. The evidence does not imply that a distributable package exists.

**Notice scope is intentionally #83-only:** project LICENSE, JUCE licence and
quickstart are required and byte-verified. Font/MTS notice coverage is explicitly
reported **absent**, pending separate PR #70. Nothing from #70 is silently imported
or treated as qualified. Neither successful ABI packaging nor its textual
evidence establishes complete licence, native installation, host compatibility,
loadability or release qualification.

## Safe activation path

### Scoped pull requests

The ordinary `pull_request` trigger handles `opened`, `synchronize` and `reopened`
events targeting `main`, only when the PR changes at least one of:

- `.github/workflows/qualify-linux-deb.yml`
- `scripts/qualify-linux-deb.py`
- `scripts/test-qualify-linux-deb.py`

Documentation-only PRs do not trigger it. A same-repository, non-fork job guard
and independent helper checks restrict execution. It never uses
`pull_request_target`. Merge-conflicted PRs do not run; resolve conflicts through
the normal review process rather than substituting a privileged trigger.

After normal review, a qualifying branch update on an open same-repository PR
can run this check without default-branch manual-workflow registration. It still
uses the fixed approved binary source and corrected packager, **not binaries
built from the PR**. The PR's helper is checked out at the synthetic merge commit
from `GITHUB_SHA`. Raw Git commit parents must match the event snapshot's base
and PR-head commits, including in a depth-one checkout. A stale/mismatched merge
fails closed instead of fetching substitute refs.

Evidence records `qualification_context.checkout_commit` (synthetic merge),
`pr_head_commit` and `base_commit`, plus the qualification run ID/attempt.
Workflow-run API `head_sha` must match `run_head_commit`, the **PR branch head**,
not the synthetic merge SHA. Manual runs instead record the dispatched checkout
as their run head. This distinguishes which helper tree ran from which PR head
the run belongs to; neither value replaces the independent binary/packager pins.

Review the exact event/head and successful evidence artifact before claiming
native qualification. Ordinary repository CI remains independent and may run
its usual tests. This check does not replace those outcomes, bypass failed checks
or qualify the PR's own product binaries.

### Explicit manual runs

[GitHub's manual-workflow documentation](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow)
requires the workflow to exist on the default branch before `workflow_dispatch`.
This registration requirement applies to manual dispatch, not the PR trigger.

1. Review these four new files and their tests. Publish/integrate them through the
   ordinary authorized path, preserving existing checks and failure outcomes.
   Do **not** add skip-CI markers, alter existing triggers or change validator
   workflows to make this easier. Complete normal review and integration before
   manual dispatch.
2. Verify the reviewed workflow is registered on default `main`, its exact head
   commit and this fixed packager pin. In the authenticated GitHub Actions UI,
   select **Qualify existing Linux Debian package**, then **Run workflow**.
3. Select the reviewed `main` revision/branch and use source run `42351562098` and
   source SHA `635ad6e8f24978040b02a4a10e646d7e3c592ce4`. Verify the resulting
   qualification run's `head_sha` is the reviewed workflow commit. The helper
   independently checks the source build and artifact again; expired or failed
   sources stop the run rather than being silently replaced.
   Manual inputs must exactly match the trusted dispatch event. Alternative
   explicit manual source inputs remain subject to every original run, artifact,
   provenance and production/build-input-identity gate; PRs cannot supply them.
4. Read the new job's outcome and verification artifact before claiming success.
   Source build success alone does not prove this packaging job ran or passed.

## Local checks and remaining limit

```sh
python3 -B scripts/test-qualify-linux-deb.py
python3 -B scripts/test-deb-runtime-dependencies.py
```

The helper's offline tests use synthetic archive/package bytes, never execute
fixtures and require no network. The original dependency tests compile inert ELF
fixtures without executing them. Both the older retained main bundle and the
current exact-source `635ad6e8` bundle pass the new reader/provenance checks.

The current local container has no native `dpkg` package database. An earlier
real-product attempt correctly failed closed on missing libasound dependency
metadata. A successful native `.deb` from this new workflow remains **unrun** until
a scoped PR or registered manual run completes and its exact result is verified.
