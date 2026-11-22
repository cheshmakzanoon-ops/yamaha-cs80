# Assignment 54 — fixed Linux build and download verification

**Actual result: installation archive and native CTest passed; release qualification failed at the required randomized GUI validator gate.**

The only target is source `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`,
GitHub Actions build `45044040694`, main push, attempt **1**. The native job is
`137274597635`; the completed downstream Linux pluginval job is
`137324977642`. No retry, source edit, publication or
downloaded-product execution is part of this assignment.

## Actual findings

The native Test step succeeded at 2026-11-22 19:21:13 UTC. Its complete log says
`100% tests passed, 0 tests failed out of 579` and records 9,492.20 seconds.
The actual JUnit contains **578 executed passes** and one skipped/not-run case:
`cs80_stereo_pra_exact_tests`, with `SKIP_RETURN_CODE=77`. No case is disabled,
failed or of unknown status. The skipped and not-run lists overlap for this
single case; they do not represent two omitted cases. See `native-proof.json`.

The real install artifact `14768807632` was downloaded without credentials and
matched its API size (13,225,022 bytes) and SHA-256
`4295d202bacb20ab41e7a776290dac51cf4a7458abc66d185a86bc351efd4d30`.
The trusted verifier checked all 20 inventory entries. Both advertised images
are ELF64 x86-64 with the correct VST3 and Standalone roles. Revision, source,
JUCE identity and guide pins passed. See `artifact-proof.json`. The outer ZIP
is retained once in the shared artifact cache; no duplicate inner archive is
retained.

The required randomized GUI lane used seed **`0x4369262`**. It printed
`SUCCESS`, then child PID 2883 terminated with raw returncode **-11 / SIGSEGV**,
reported by the wrapper as exit 139. The wrapper correctly refused a retry
after SUCCESS. The headless lane and required fixed-seed `0x2da72db` lane each
returned raw exit 0. Each of these three original lanes ran once; **zero
internal wrapper retries** occurred.

Both scheduled diagnostic executions replayed the failing seed `0x4369262`:
the GDB and fault-thread runs returned exit 0 with SUCCESS. They are diagnostic
nonreproductions, and neither substitutes for the failed randomized gate.
No local core was available for the original crash because local capture was
unavailable with the kernel pipe collector. The fault-thread log contains
three handler-installation notices and no fault stack. **Crash ownership and
root cause remain unproven.**

The validator report ZIP `14769486678` matched its API size (12,870 bytes) and
SHA-256 `8d199d30e368a52390923d0d8d825e7dfb331f76844dd56fcae3b618dab47185`.
All 17 report members are preserved unmodified under `validator-reports/`;
member hashes are in `validator-reports-manifest.json`. Raw native and validator
job logs and exact completed API metadata are retained here. `run-final.json`
records the overall failed first-attempt build; `run.json` preserves the earlier
source-bound run snapshot used for artifact and native evidence.

`ci-proof.json` records **FAIL** and the complete original and diagnostic
history. The strict success qualifier was also applied to these actual inputs:
it returned exit 1 for the failed validator job, as required. No gate or wrapper
condition was weakened. `capture-native-evidence.py` and
`capture-final-failure.py` reproduce the separate native report and failed CI
report from retained metadata and logs; neither executes a downloaded product.

## Checks

`verify-linux-download.py` checks the exact API artifact identity, digest and
size through a credential-free pinned artifact GET. It imports the trusted
repository publisher and offline inventory verifier, then inspects every ELF
payload. The two advertised products must be little-endian ELF64/x86-64 with
the correct shared-object or executable role, valid program headers, Standalone
executable mode and expected interpreter presence. All file modes are also
checked against the full inventory. It verifies revision `2026.11.1`,
clean pinned JUCE `8.0.11` source, `SOURCE.txt`, source-pinned repository guide
links and local bundled guide targets. It never loads those products.

`check-ci-gates.py` requires completed successful first-attempt native and
pluginval jobs and their exact required steps. It reads the complete CTest
summary, raw JUnit and numbered test lines. The registered count is 579, while
the overall reported total and actually executed successful case count are
reported separately. Skipped, disabled and not-run cases retain their names,
raw status, messages and log lines. These may overlap; they are not relabeled as
executed passes. Any real failure remains a failure.

The three Linux validator lanes require clean final SUCCESS logs **and original
child returncode 0**. A SUCCESS marker followed by a nonzero exit cannot pass.
The named core logs, final logs and any earlier internal attempt logs are
cross-checked against the raw job's termination history. The existing wrapper
permits at most three full attempts only for status139 with no `SUCCESS` or
`FAILED` substring, a previously observed `Starting tests in:` line and a last
matching phase of `Completed tests in:`. Its two phase patterns include the
colon. Log-capture and failed-log-preservation errors cannot become passes.
Such internal retries, if present, are preserved and counted separately:
GitHub `run_attempt=1` does not itself prove that every validator command ran
only once. This verifier does not invoke retries or change those production
conditions.

## Inputs and invocation

Obtain fresh raw GitHub API metadata for the exact run, native job, validator
job and these artifact names:

- `cs80-linux-install-bundle`
- `cs80-linux-test-evidence`
- `cs80-pluginval-reports`

Keep the individual artifact objects as `install-artifact.json`,
`test-evidence-artifact.json` and `validator-reports-artifact.json`. Retain raw
job logs in `builder-job.log` and `validator-job.log`, and raw metadata in
`run.json`, `builder-job.json` and `validator-job.json` in this directory.

```sh
python3 build-w12-linux/verify-linux-download.py \
  --artifact-json build-w12-linux/install-artifact.json \
  --run-json build-w12-linux/run.json

python3 build-w12-linux/check-ci-gates.py \
  --reports-artifact-json build-w12-linux/validator-reports-artifact.json \
  --test-evidence-artifact-json build-w12-linux/test-evidence-artifact.json
```

The archive check creates `artifact-proof.json`. The strict CI qualifier creates
`ci-proof.json` only when its requirements pass; for this observed failed run,
`capture-final-failure.py` created the explicit failed `ci-proof.json` instead.
The outer ZIPs are retained once in `build-coordination/artifact-cache` for later
release/guide verification. Temporary inner archives are removed. Artifact
bytes can be checked while unrelated platforms remain in progress; the full
workflow and release are separate integration gates.

## Preparation evidence

All three Python sources compile. The parser rejects the actual historical
post-SUCCESS crash log's raw `-11` SIGSEGV; its source-log hash and rejection are
in `historical-crash-negative-control.json`. The CTest reporting helper was
checked against the existing local four-case XML and an inert fixture with
skipped, disabled and not-run cases. Neither preparation check executes the
current native product or certifies its CI result.

`last-observed-*.json` preserves the API state observed while the native Test
step was still in progress. There were no run artifacts at that observation.
No complete Linux test or validator pass is inferred from it.

The original preparation state is preserved in `preparation-summary.json`.
The actual outcome is in `summary.json`; it must not be reported as a fully
qualified Linux release.
