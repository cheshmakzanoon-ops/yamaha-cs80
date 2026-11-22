#!/usr/bin/env python3
"""Check retained first-attempt Apple Silicon job metadata and raw logs."""
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
SOURCE = "6d3cc220edef5a4a2c7cd968ea8a786c7c83198c"
RUN_ID = 45044040694
run = json.loads((HERE / "run.json").read_text())
job = json.loads((HERE / "job.json").read_text())
log = (HERE / "job.log").read_text()
assert run["id"] == RUN_ID and run["head_sha"] == SOURCE and run["run_attempt"] == 1
assert run["head_repository"]["full_name"] == "cheshmakzanoon-ops/yamaha-cs80"
assert job["run_id"] == RUN_ID and job["head_sha"] == SOURCE and job["run_attempt"] == 1
assert job["name"] == "macOS / macos-15 / VST3 + AU + Standalone"
assert job["status"] == "completed" and job["conclusion"] == "success"
required = {
    "Record native platform", "Configure", "Verify configured JUCE source", "Build", "Test",
    "Fetch pluginval 1.0.4", "Register built AU", "Validate VST3 with pluginval",
    "Validate AU with pluginval", "Validate AU with pluginval GUI",
    "Package native macOS VST3, AU and Standalone", "Upload native macOS install bundle",
}
steps = {step["name"]: step for step in job["steps"]}
assert len(steps) == len(job["steps"])
assert all(steps.get(name, {}).get("conclusion") == "success" for name in required)
plain = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", log)
passed = re.findall(r"100% tests passed(?:, 0 tests failed)? out of (\d+)", plain)
assert len(passed) == 1, "Missing or ambiguous complete CTest success"
suite = ET.parse(HERE / "ctest-results.xml").getroot()
cases = suite.findall(".//testcase")
assert suite.tag == "testsuite" and int(suite.get("tests")) == len(cases) == int(passed[0])
assert int(suite.get("failures", "0")) == 0 and int(suite.get("errors", "0")) == 0
assert int(suite.get("disabled", "0")) == 0
expected_skips = {"cs80_filter_batch_cap_tests", "cs80_stereo_pra_exact_tests"}
skipped = [case for case in cases if case.find("skipped") is not None]
assert {case.get("name") for case in skipped} == expected_skips
assert int(suite.get("skipped")) == len(skipped) == 2
assert all(case.get("status") == "notrun" and case.find("skipped").get("message") == "SKIP_RETURN_CODE=77" for case in skipped)
executed = [case for case in cases if case.find("skipped") is None]
assert all(case.get("status") == "run" and case.find("failure") is None and case.find("error") is None for case in executed)
assert len(executed) == 241 and len(cases) == 243
rows = re.findall(r"(?m)^\d{4}-\d\d-\d\dT\S+Z\s+(\d+)/(\d+) Test\s+#(\d+):\s+(\S+)\s+\.+\s*(Passed|\*\*\*Skipped)\s+([\d.]+) sec\s*$", plain)
assert len(rows) == 243 and {int(row[0]) for row in rows} == set(range(1, 244))
assert {row[3] for row in rows} == {case.get("name") for case in cases}
assert all(int(row[1]) == 243 and int(row[0]) == int(row[2]) for row in rows)
assert {row[3] for row in rows if row[4] == "***Skipped"} == expected_skips
assert sum(row[4] == "Passed" for row in rows) == 241
markers = re.findall(r"(?m)^(?:\d{4}-\d\d-\d\dT\S+Z\s+)?SUCCESS\s*$", plain)
assert len(markers) == 3, "Expected three exact pluginval completion markers"
assert plain.count("--strictness-level 10") >= 3
assert "set -euo pipefail" in plain
assert not re.search(r"(?m)^.*##\[error\]", plain), "Job contains an error annotation"
validator_records = []
for filename, kind in (("pluginval-vst3.log", "VST3"), ("pluginval-au.log", "AU"), ("pluginval-au-gui.log", "AU GUI")):
    text = (HERE / filename).read_text()
    assert text.rstrip().endswith("\nSUCCESS")
    assert text.splitlines().count("SUCCESS") == 1 and "Strictness level: 10" in text
    seeds = re.findall(r"^Random seed: (0x[0-9a-fA-F]+)$", text, re.MULTILINE)
    assert len(seeds) == 1
    validator_records.append({"format": kind, "filename": filename, "strictness": 10, "random_seed": seeds[0], "exact_success_markers": 1, "sha256": hashlib.sha256((HERE / filename).read_bytes()).hexdigest()})
test_records = [{"name": case.get("name"), "status": "skipped" if case.find("skipped") is not None else "passed", "junit_status": case.get("status"), "seconds": float(case.get("time")), "skip_message": case.find("skipped").get("message") if case.find("skipped") is not None else None} for case in cases]
(HERE / "test-cases.json").write_text(json.dumps(test_records, indent=2) + "\n")
proof = {
    "assignment": 52, "status": "PASS", "run_id": RUN_ID, "run_attempt": 1,
    "source_commit": SOURCE, "job_id": job["id"], "job_name": job["name"],
    "job_conclusion": job["conclusion"], "started_at": job["started_at"], "completed_at": job["completed_at"],
    "required_steps": [steps[name] for name in sorted(required)],
    "ctest_count": int(passed[0]), "ctest_passed": len(executed), "ctest_failed": 0,
    "ctest_skipped": len(skipped), "ctest_disabled": 0,
    "ctest_junit_notrun_status": len(skipped), "ctest_not_run_other_than_skips": 0,
    "skipped_tests": [{"name": "cs80_filter_batch_cap_tests", "reason": "Raw LastTest.log: AVX2 unavailable or disabled; no SIMD qualification.", "skip_return_code": 77}, {"name": "cs80_stereo_pra_exact_tests", "reason": "No runtime explanation printed. Source guards this exact specialization to qualified Linux x86_64 GCC14.2 arithmetic and returns77 on other platforms; arm64 macOS does not enter it.", "skip_return_code": 77}],
    "pluginval_exact_success_markers": len(markers), "validators": validator_records,
    "validator_exit_evidence": "Each successful required step uses set -euo pipefail, runs pluginval through tee, and then requires an exact SUCCESS line. Step success establishes a zero pipeline status including the validator; the log does not separately print numeric raw child exits.",
    "inputs": {name: {"bytes": (HERE / name).stat().st_size, "sha256": hashlib.sha256((HERE / name).read_bytes()).hexdigest()} for name in ("run.json", "job.json", "job.log", "ctest-results.xml", "LastTest.log", "ci-evidence-downloads.json")},
    "scope": "This exact Apple Silicon builder/CTest/VST3/AU/AU-GUI gate only. The full run may still have other platform jobs pending."
}
(HERE / "ci-proof.json").write_text(json.dumps(proof, indent=2) + "\n")
print(json.dumps({key: proof[key] for key in ("status", "run_attempt", "job_id", "ctest_count", "ctest_passed", "ctest_skipped", "ctest_disabled", "ctest_not_run_other_than_skips", "pluginval_exact_success_markers")}, indent=2))
