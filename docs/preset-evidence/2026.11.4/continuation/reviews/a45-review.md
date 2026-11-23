# A45 — native evidence parser

## Delivered

Scratch-only parser: `/workspace/scratch/dde4ad85ed5b/a45/parse_native_evidence.py`.

No repository source, tests, fixtures, workflows or Git state changed. No native test, audio render or validator process was rerun. This tool summarizes supplied evidence; it **never marks a release qualified** and does not fetch data or extract archives.

## What it records

- CTest executions per supplied platform: registered, passed, failed, formal skipped, disabled and unknown. It checks JUnit totals against actual testcase records and reconciles test names and successful outcomes with `LastTest.log`.
- Exact formal-skip names and messages. CTest can write `Test Passed.` for a return-code skip in `LastTest.log`; the JUnit `<skipped>` element remains authoritative.
- Nested Python unittest skips from complete `LastTest.log` output, per CTest case. It counts each terminal unittest summary once, never adds the duplicate JUnit output, and does not add individual `... skipped` reason lines to the terminal count. These counts are separate from CTest executions and internal assertions.
- The exact required **13 pluginval lanes** across four supplied targets: Linux 3, Windows 2, macOS arm64 4, macOS x86_64 4. The console and report copies count as **one** lane, not two invocations.
- Per-lane strictness, seed, 523-program count, terminal `SUCCESS`, failure markers, warnings, explicit skip notices, whether editor tests started, and fixed-seed agreement. The AU `!!! WARNING: Current program is -1...` warning is retained separately; it is not silently promoted to a failed test.
- Linux original-child termination diagnostics and preserved `attemptN` logs, including retry counts. Any retries still require manual original job-log and retry-eligibility review.
- Pluginval 1.0.4 version text or verified download report. Every input member and supplied ZIP receives byte/hash records.

Known CTest log forms handled include empty output and `Test Pass Reason: Required regular expression found`, not only `Test Passed.`. ZIPs are read without extraction; canonical names, duplicates, member types and bounded sizes are checked. This structural reader does **not** replace the strict installation, audition or validator-payload helpers.

## Invocation

Create a JSON file with explicit paths. Paths are relative to that JSON file unless absolute. The four exact target names are `linux-x86_64`, `windows-x86_64`, `macos-arm64`, `macos-x86_64`.

```json
{
  "provenance": {
    "source": "078c0981ffa1b5472342e0fb5ab8702bc3bd2889",
    "run_id": "replace with actual final run",
    "attempt": 1
  },
  "native": [
    {"target": "linux-x86_64", "archive": "linux-test-evidence.zip"},
    {"target": "windows-x86_64", "archive": "windows-test-evidence.zip"},
    {"target": "macos-arm64", "archive": "macos-arm64-test-evidence.zip"},
    {"target": "macos-x86_64", "archive": "macos-x86_64-test-evidence.zip"}
  ],
  "validators": [
    {"target": "linux-x86_64", "archive": "linux-pluginval.zip"},
    {"target": "windows-x86_64", "archive": "windows-pluginval.zip"},
    {"target": "macos-arm64", "archive": "macos-arm64-pluginval.zip"},
    {"target": "macos-x86_64", "archive": "macos-x86_64-pluginval.zip"}
  ]
}
```

The example filenames are caller-selected local paths, **not claims that those current artifacts exist**. For known exact already-unpacked evidence, substitute `"directory": "/absolute/directory"` for `archive`. For a native raw pair, use `"junit": "/absolute/ctest-results.xml"` and `"lasttest": "/absolute/LastTest.log"` instead.

Default exact native archive members are:

- `ctest-results.xml`
- `Testing/Temporary/LastTest.log`
- `ci-juce-provenance.txt` when present

If the actual producer ZIP uses a different root, specify `junit_member`, `lasttest_member` and `provenance_member` explicitly. There is no basename guessing. Validator names come directly from the workflow; optional `console_members`, `report_members`, `core_members` dictionaries can bind lane IDs to explicitly inspected member names.

```bash
python3 /workspace/scratch/dde4ad85ed5b/a45/parse_native_evidence.py \
  --inputs /absolute/final-inputs.json \
  --out /absolute/fresh-native-observations.json \
  --require-complete
```

Use a fresh `--out` path. Existing outputs are refused. Exit 0 means the requested observations parsed and their text/test counts met these checks; it **does not** mean release qualification. Exit 1 retains a report with failing/unknown observations; exit 2 reports malformed, missing or inconsistent inputs. Omit `--require-complete` only for explicitly partial evidence review.

## Verification performed

- Python syntax compilation and CLI help succeeded.
- Parsed retained historical **9c0d2ba** evidence already in the repository: one macOS arm64 native run and Linux/macOS arm64 validator logs. The final parser invocation exited 0 and observed **247 registered, 245 passed, 2 formal skips, 0 failed, 5 nested Python skips**; **7 validator lanes / 7 invocations / 0 retries**. This is a parser compatibility observation, **not current-release evidence**.
- Historical parsing identified empty-output and regex-pass CTest formatting, plus warning-only AU text; the parser handles those cases explicitly. No new test fixtures or plug-in executions were created.

The historical inputs and final observation are retained under `a45/`. The earlier intermediate observation recorded the overly broad warning classifier before correction and must not be mistaken for an actual historical plugin failure.

## Mandatory external qualification

The output deliberately leaves `original_process_exit` / `ctest_process_exit` unknown and sets `release_qualified: false`. Root must bind authentic artifact API ID/name/size/digest/run/head SHA/expiry; producing upload steps and intended attempt; original successful native Test and all 13 validator steps; source-pinned exit propagation; exact Windows/Linux validated payload equality; Mac AU/VST3 identities; and all supporting outcomes. A `SUCCESS` followed by a crash cannot pass based on this parser's text result. Caller-supplied provenance is labeled **unverified**. No current-source native or validator outcome was available or asserted by A45.
