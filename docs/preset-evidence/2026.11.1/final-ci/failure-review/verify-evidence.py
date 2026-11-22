#!/usr/bin/env python3
"""Read-only byte/provenance/result checks; never launches a validator."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET
import zipfile

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
SOURCE = "6d3cc220edef5a4a2c7cd968ea8a786c7c83198c"
RUN = 45044040694
COMPARISON = 45060638632
BINARY = "270d91d746d7bc4496c0097755b997f0ba102728c3d17f7050225f1392250265"
HOST_BINARY = "79612f9dd1a93f24920a7244a46c50bc7ff1bdf8174c013acee19032f5047f24"
checks = []


def require(label, condition):
    if not condition:
        raise SystemExit("BLOCKED evidence verification: " + label)
    checks.append(label)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def readj(path):
    return json.loads(path.read_text())


original_zip = REPO / "build-coordination/artifact-cache/cs80-pluginval-reports-14769486678.zip"
data = original_zip.read_bytes()
require("original report ZIP bytes/digest", len(data) == 12870 and sha(data) ==
        "8d199d30e368a52390923d0d8d825e7dfb331f76844dd56fcae3b618dab47185")
with zipfile.ZipFile(original_zip) as archive:
    members = [x for x in archive.namelist() if not x.endswith("/")]
    require("original report ZIP has 17 files", len(members) == 17)
    for name in members:
        require("original report bytes " + name,
                archive.read(name) == (REPO / "build-w12-linux/validator-reports" / name).read_bytes())

original = REPO / "build-w12-linux/validator-reports"
core = (original / "pluginval-gui-core.log").read_text()
require("original native SIGSEGV/PID", "PID=2883; raw returncode=-11; kind=signal; signal_number=11; signal_name=SIGSEGV" in core)
require("no original local core", "No local ELF core was available" in core and "Local core capture enabled: False" in core)
gui = (original / "pluginval-gui.log").read_text()
require("original GUI seed and SUCCESS", "Random seed: 0x4369262" in gui and "SUCCESS" in gui.splitlines())
full = (REPO / "build-w12-linux/validator-job.log").read_text()
require("original failure refusal", "pluginval failed (exit 139); not eligible for a post-test retry" in full)
require("same-seed GDB normal exit", "Random seed: 0x4369262" in (original / "pluginval-gui-gdb.log").read_text()
        and "exited normally" in (original / "pluginval-gui-gdb.log").read_text())
require("fault trace installation only", all(x == "diagnostic handler installed; later replacement is possible"
        for x in (original / "pluginval-fault-trace.log").read_text().splitlines()))

source_hashes = {}
for name in ["src/plugin/PluginProcessor.cpp", "src/plugin/PluginProcessor.h",
             "src/plugin/PluginEditor.cpp", "src/plugin/PluginEditor.h", "src/plugin/ArtCommon.cpp"]:
    now = subprocess.check_output(["git", "show", SOURCE + ":" + name], cwd=REPO)
    before = subprocess.check_output(["git", "show", "7f55a95:" + name], cwd=REPO)
    require("unchanged reviewed runtime " + name, now == before)
    source_hashes[name] = sha(now)

native = readj(REPO / "build-w12-linux/native-proof.json")
require("native source/run identity", native["source_commit"] == SOURCE and native["run_id"] == RUN
        and native["run_attempt"] == 1 and native["ctest_reported_total"] == 579 and native["ctest_failed"] == 0)
execution = native["ctest_execution"]
def number_or_length(value):
    return len(value) if isinstance(value, list) else value
require("native actual execution counts", number_or_length(execution["testcase_records"]) == 579
        and number_or_length(execution["executed_passed"]) == 578
        and number_or_length(execution["skipped"]) == 1
        and number_or_length(execution["failures"]) == 0)
candidate = readj(REPO / "build-w12-linux/artifact-proof.json")
require("install candidate .so matches comparison", any(x.get("sha256") == BINARY
        for x in candidate["binary_headers"]))

comp = HERE / "comparison"
run = readj(comp / "run.json")
jobs = readj(comp / "jobs.json")
artifacts = readj(comp / "artifacts.json")
require("comparison run identity", run["id"] == COMPARISON and run["run_attempt"] == 1
        and run["event"] == "workflow_run" and run["conclusion"] == "success")
require("comparison complete job/artifact collections", jobs["total_count"] == len(jobs["jobs"]) == 3
        and artifacts["total_count"] == len(artifacts["artifacts"]) == 3)
expected = {
    "headless-subset": (14769098257, 137325542917, "0x4f31605", 25),
    "gui-inclusive": (14769073337, 137325542836, "0x63421ca", 31),
    "gui-current-regression": (14769337392, 137325542552, "0x3ca4063", 31),
}
observed = []
for lane, (artifact_id, job_id, seed, count) in expected.items():
    meta = next(a for a in artifacts["artifacts"] if a["id"] == artifact_id)
    job = next(j for j in jobs["jobs"] if j["id"] == job_id)
    validation = next(s for s in job["steps"] if s["name"] == "Run one ordinary validation and preserve its result")
    require(lane + " actual validation step", job["conclusion"] == validation["conclusion"] == "success"
            and job["run_id"] == COMPARISON and job["run_attempt"] == 1)
    data = (comp / f"artifact-{artifact_id}.zip").read_bytes()
    require(lane + " report ZIP bytes/digest", len(data) == meta["size_in_bytes"]
            and "sha256:" + sha(data) == meta["digest"])
    directory = comp / f"artifact-{artifact_id}"
    with zipfile.ZipFile(comp / f"artifact-{artifact_id}.zip") as z:
        names = [x for x in z.namelist() if not x.endswith("/")]
        require(lane + " all extracted report bytes", len(names) == 19
                and all(z.read(x) == (directory / x).read_bytes() for x in names))
    p = readj(directory / "provenance.json")
    r = readj(directory / "results.json")
    require(lane + " frozen source provenance", p["source_sha"] == SOURCE and p["source_run_id"] == RUN
            and p["source_run_attempt"] == 1 and p["source_run_conclusion"] == "failure"
            and p["source_artifact"]["id"] == 14768852300 and p["comparison_run_id"] == COMPARISON
            and p["comparison_workflow_sha"] == run["head_sha"] and p["lane"] == lane and p["seed"] == seed)
    require(lane + " no retries or source patch", p["validation_retries"] == 0 and p["repeats"] == 1
            and p["validator"]["source_modifications"] is False and p["strictness"] == 10)
    require(lane + " actual process and logging exits", (directory / "process-exit-code.txt").read_text().strip() == "0"
            and (directory / "logging-exit-code.txt").read_text().strip() == "0")
    require(lane + " JSON all tests passed", r["summary"] == {"tests": count, "passed": count, "failed": 0, "errors": 0}
            and len(r["tests"]) == count and all(t["status"] == "passed" for t in r["tests"]))
    xml = ET.fromstring((directory / "results.xml").read_text())
    require(lane + " XML all tests passed", xml.attrib["tests"] == str(count)
            and xml.attrib["failures"] == xml.attrib["errors"] == "0"
            and len(xml.findall(".//testcase")) == count and not xml.findall(".//failure")
            and not xml.findall(".//error"))
    require(lane + " binary identities", BINARY in (directory / "plugin-files-sha256.txt").read_text()
            and HOST_BINARY in (directory / "validator-binary-sha256.txt").read_text())
    require(lane + " SUCCESS and recorded seed", "SUCCESS" in (directory / "console.log").read_text().splitlines()
            and r["properties"]["random_seed"] == seed)
    if not p["skip_gui_tests"]:
        names = {t["name"] for t in r["tests"]}
        require(lane + " GUI cases executed", {"Editor", "Open editor whilst processing", "Editor Automation", "Editor stress"} <= names)
    observed.append({"lane": lane, "job_id": job_id, "artifact_id": artifact_id, "seed": seed,
                     "tests_passed": count, "process_exit": 0, "logging_exit": 0,
                     "zip_sha256": sha(data), "zip_bytes": len(data)})

result = {
    "assignment": 60,
    "acceptance": "BLOCKED",
    "scope": "Independent bounded diagnosis of original stable Linux SIGSEGV; verified beta comparison is supplementary.",
    "source_commit": SOURCE, "run_id": RUN, "run_attempt": 1,
    "original": {"job_id": 137324977642, "lane": "randomized GUI", "seed": "0x4369262",
                 "success_printed": True, "pid": 2883, "raw_returncode": -11,
                 "signal": "SIGSEGV", "normalized_exit": 139,
                 "original_core_or_fault_stack_available": False,
                 "same_seed_gdb_and_trace": "exit-zero instrumented nonreproductions"},
    "current_native_ctest": {"registered": 579, "passed": 578, "skipped": 1, "failed": 0},
    "source_hashes": source_hashes,
    "comparison": {"run_id": COMPARISON, "run_attempt": 1, "workflow_sha": run["head_sha"],
                   "validator_tag": "v2.0.0-beta3", "plugin_sha256": BINARY,
                   "validator_binary_sha256_recorded_by_ci": HOST_BINARY,
                   "lanes": observed, "original_failing_seed_replayed": False,
                   "qualification": "Compatibility evidence for these actual executions; no repair or ownership proof."},
    "local_native_reproductions": 0, "ci_mutations": 0, "production_or_gate_changes": 0,
    "crash_ownership": "unestablished",
    "next_step": "One bounded same-artifact stable1.0.4 original-seed diagnostic on an already capable Linux host with permitted local core capture; match PID and capture argument-free fault stacks if it faults. Preserve original failure.",
    "evidence_checks_passed": len(checks), "checks": checks,
}
print(json.dumps(result, indent=2))
