#!/usr/bin/env python3
"""Consolidate independently downloaded Windows artifact and first-attempt CI proof."""
from collections import Counter
from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(__file__).resolve().parent
SOURCE = "6d3cc220edef5a4a2c7cd968ea8a786c7c83198c"
RUN = 45044040694
def read_json(name):
    return json.loads((WORK / name).read_text())
def digest(data):
    return hashlib.sha256(data).hexdigest()
run, jobs = read_json("run-api.json"), read_json("jobs-api.json")["jobs"]
assert run["id"] == RUN and run["head_sha"] == SOURCE and run["run_attempt"] == 1
required = {
    137274597948: ["Verify configured JUCE source", "Build", "Test", "Upload Windows VST3", "Package Windows VST3 and Standalone", "Upload Windows install bundle"],
    137283806829: ["Download Windows VST3", "Fetch pluginval 1.0.4", "Validate VST3 headless", "Validate state regression seed 0x5037a0c"],
}
windows_jobs = []
for ident, expected_steps in required.items():
    job = next(j for j in jobs if j["id"] == ident)
    assert job["run_id"] == RUN and job["head_sha"] == SOURCE and job["run_attempt"] == 1
    assert job["status"] == "completed" and job["conclusion"] == "success"
    steps = {s["name"]:s for s in job["steps"]}
    assert all(steps[n]["status"] == "completed" and steps[n]["conclusion"] == "success" for n in expected_steps)
    windows_jobs.append({k:job[k] for k in ["id","name","head_sha","run_id","run_attempt","status","conclusion","started_at","completed_at","steps"]})
native_raw = (WORK / "windows-native-job.log").read_bytes()
validator_raw = (WORK / "windows-pluginval-job.log").read_bytes()
native = native_raw.decode("utf-8-sig")
validator = validator_raw.decode("utf-8-sig")
assert SOURCE in native and SOURCE in validator
rows = []
for line in native.splitlines():
    match = re.search(r"\s(\d+)/221 Test\s+#(\d+):\s+(\S+)\s+\.+\s*(.*)", line)
    if match:
        result = match[4]
        kind = ("passed" if result.startswith("Passed") else
                "skipped" if "Skipped" in result else
                "disabled" if "Disabled" in result else
                "not_run_other" if "Not Run" in result else "failed_or_unknown")
        rows.append({"sequence":int(match[1]),"id":int(match[2]),"name":match[3],"result":result,"category":kind})
assert len(rows) == 221 and len({r["id"] for r in rows}) == 221
assert Counter(r["category"] for r in rows) == Counter({"passed":219,"skipped":2})
assert "100% tests passed, 0 tests failed out of 221" in native
support = {r["name"]:r for r in read_json("supporting-artifacts.json")}
artifact = read_json("artifact-proof.json")
manifest = read_json("verified/windows/x86_64/cs80-reference-windows-x86_64-6d3cc220edef.manifest.json")
matches = []
with zipfile.ZipFile(support["cs80-windows-vst3"]["path"]) as archive:
    runtime = [f for f in manifest["files"] if f["path"].startswith("VST3/") and f["type"] == "file"]
    assert len(runtime) == 2
    for item in runtime:
        content = archive.read(item["path"][5:])
        assert len(content) == item["size"] and digest(content) == item["sha256"]
        matches.append({"path":item["path"],"bytes":len(content),"sha256":digest(content)})
input_api = support["cs80-windows-vst3"]["artifact_api"]
assert f"ID: {input_api['id']}, Size: {input_api['size_in_bytes']}, Expected Digest: {input_api['digest']}" in validator
assert "SHA256 digest of downloaded artifact is " + input_api["digest"].split(":")[1] in validator
with zipfile.ZipFile(support["cs80-windows-test-evidence"]["path"]) as archive:
    xml_bytes = archive.read("ctest-results.xml")
    junit = ET.fromstring(xml_bytes)
    cases = list(junit.iter("testcase"))
    assert len(cases) == 221
    assert {k:junit.attrib[k] for k in ["tests","failures","disabled","skipped"]} == {"tests":"221","failures":"0","disabled":"0","skipped":"2"}
    assert Counter(c.attrib["status"] for c in cases) == Counter({"run":219,"notrun":2})
    skipped = []
    for case in cases:
        skip = case.find("skipped")
        if skip is not None:
            assert case.attrib["status"] == "notrun" and skip.attrib["message"] == "SKIP_RETURN_CODE=77"
            skipped.append({"name":case.attrib["name"],"ctest_status":"skipped","junit_status":"notrun","reason":skip.attrib["message"],"output":case.findtext("system-out") or ""})
    assert {c["name"] for c in skipped} == {r["name"] for r in rows if r["category"] == "skipped"}
    juce_bytes = archive.read("ci-juce-provenance.txt")
    juce_manifest = next(i for i in manifest["files"] if i["path"] == "evidence/juce-provenance.txt")
    assert digest(juce_bytes) == juce_manifest["sha256"]
    (WORK / "ctest-results.xml").write_bytes(xml_bytes)
plain_job = [re.sub(r"^\S+Z\s?", "", line) for line in validator.splitlines()]
actual_success = [line for line in validator.splitlines() if re.fullmatch(r"\S+Z SUCCESS",line)]
assert len(actual_success) == 2
assert not any(line == "FAILED" for line in plain_job)
lanes = []
lane_definitions = [
    ("randomized","pluginval-windows.log","0x402130","Validate VST3 headless"),
    ("fixed_seed","pluginval-windows-seed-regression.log","0x5037a0c","Validate state regression seed 0x5037a0c"),
]
reports = WORK / "validator-reports"
reports.mkdir(exist_ok=True)
with zipfile.ZipFile(support["cs80-pluginval-windows"]["path"]) as archive:
    download = json.loads(archive.read("pluginval-download.json"))
    assert download["verified"] is True and download["version"] == "1.0.4"
    assert download["actual_sha256"] == download["expected_sha256"] == "c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab"
    assert download["actual_size"] == download["expected_size"] == 2408590
    assert len(download["attempts"]) == 1 and download["attempts"][0]["number"] == 1
    assert download["attempts"][0]["transfer_exit"] == 0
    for index,(kind,name,seed,step) in enumerate(lane_definitions):
        content = archive.read(name)
        lines = content.decode("utf-8-sig").splitlines()
        assert [line for line in lines if line.startswith("Random seed: ")] == ["Random seed: " + seed]
        assert [line for line in lines if line.startswith("Strictness level: ")] == ["Strictness level: 10"]
        assert lines[-1] == "SUCCESS" and lines.count("SUCCESS") == 1 and not any(line == "FAILED" for line in lines)
        assert "Validation started" in lines and "Num plugins found: 1" in lines
        (reports / name).write_bytes(content)
        lanes.append({"lane":kind,"step":step,"random_seed":seed,"strictness":10,"headless":True,"skip_gui_tests":True,"validate_in_process":True,"log_sha256":digest(content),"final_output":"SUCCESS","actual_job_success_line":actual_success[index],"step_conclusion":"success","process_exit_evidence":"The native LASTEXITCODE nonzero guard is present in the executed step; the step concluded success and the actual validator output ended SUCCESS. A separate numeric exit line was not printed."})
    (reports / "pluginval-download.json").write_bytes(archive.read("pluginval-download.json"))
    (reports / "pluginval-version.txt").write_bytes(archive.read("pluginval-version.txt"))
assert validator.count("if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }") >= 4
guide_details = []
with zipfile.ZipFile(artifact["download"]["path"]) as outer:
    with zipfile.ZipFile(io.BytesIO(outer.read(artifact["inner_archive"]["name"]))) as inner:
        stem = "cs80-reference-windows-x86_64-" + SOURCE[:12]
        for name in ["INSTALL_AND_QUICKSTART.md","PRESET_GUIDE.md","DISTRIBUTION_STATUS.md"]:
            text = inner.read(stem + "/" + name).decode()
            targets = re.findall(r"(?<!!)\[[^\]\n]+\]\(([^\s()]+)(?:\s+" + r'"[^"]*"' + r")?\)", text)
            external = [t for t in targets if t.startswith(("https://","http://")) and not t.startswith("https://github.com/cheshmakzanoon-ops/yamaha-cs80/")]
            guide_details.append({"name":name,"other_external_links":external,"latency_lines":[line for line in text.splitlines() if "latency" in line.lower()]})
proof = {
    "assignment":49,"verified_at_utc":datetime.now(timezone.utc).isoformat(),
    "windows_qualification":"PASS: first-attempt native build/test/package and both headless Windows VST3 pluginval lanes; actual downloadable bundle bytes verified.",
    "overall_release_qualification":"PENDING: the observed build run is still in_progress. Linux, Intel macOS and remaining release gates are outside this Windows completion.",
    "binary_source":SOURCE,"build_run":RUN,"build_run_attempt":1,
    "run_api_status":run["status"],"run_api_conclusion":run["conclusion"],
    "windows_jobs":windows_jobs,
    "native_ctest":{"total":221,"passed":219,"failed":0,"skipped":2,"disabled":0,"junit_notrun":2,"additional_not_run":0,"overlap_note":"Both JUnit notrun cases are the same two skipped cases, not two additional tests.","real_seconds":1101.04,"skipped_cases":skipped,"junit_sha256":digest(xml_bytes),"junit_summary":junit.attrib},
    "pluginval":{"version":"1.0.4","official_validator_download":download,"lanes":lanes,"input_artifact":input_api,"runtime_files_identical_to_install_bundle":matches},
    "bundle":{"artifact_id":artifact["artifact_api"]["id"],"url":artifact["download"]["url"],"size":artifact["download"]["bytes"],"sha256":artifact["download"]["sha256"],"expires_at":artifact["artifact_api"]["expires_at"],"inner_sha256":artifact["inner_archive"]["sha256"],"manifest_sha256":artifact["offline_verifier"]["manifest_sha256"],"inventory_entries":20,"manifest_identity":artifact["manifest_identity"],"binary_headers":artifact["binary_headers"],"guides":artifact["bundled_guides"],"additional_guide_inspection":guide_details},
    "job_log_sha256":{"native_normalized_lf":digest(native_raw),"validator_normalized_lf":digest(validator_raw)},
    "supporting_artifacts":[{"id":r["artifact_api"]["id"],"name":r["name"],"bytes":r["bytes"],"sha256":r["sha256"],"path":r["path"]} for r in support.values()],
    "limits":["No Windows product binary executed in this workspace.","Both Windows pluginval lanes skip GUI tests; this does not qualify Windows GUI or every DAW.","The two CTest skips are excluded from the passed count; SIMD batch-cap qualification was explicitly skipped.","No claims of signing, notarization, hardware sonic identity or subjective listening quality.","No retries, source edits, release creation or publication performed by this assignment."]
}
(WORK / "windows-qualification.json").write_text(json.dumps(proof,indent=2)+"\n")
artifact["ci_qualification"] = "PASS for first-attempt Windows native build/test/package and both headless VST3 pluginval lanes; overall release remains pending. See windows-qualification.json."
artifact["status"] = "windows_artifact_and_first_attempt_gates_verified"
artifact["gate_verified_at_utc"] = proof["verified_at_utc"]
artifact["qualification_proof"] = "windows-qualification.json"
(WORK / "artifact-proof.json").write_text(json.dumps(artifact,indent=2)+"\n")
print(json.dumps({"qualification":proof["windows_qualification"],"overall":proof["overall_release_qualification"],"counts":proof["native_ctest"],"validator_lanes":lanes,"guide_details":guide_details},indent=2))
