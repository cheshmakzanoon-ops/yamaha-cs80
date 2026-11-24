#!/usr/bin/env python3
"""Read the acquired originals, without building or promoting absent/failed jobs."""
from __future__ import annotations

from collections import Counter
from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import subprocess
import tarfile
import xml.etree.ElementTree as ET
import zipfile

import yaml

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
CI = ROOT / "docs/preset-evidence/2026.11.7/ci/source-ee0dabca"
DOWNLOADS = Path("/tmp/cs80-ci-1cee5a/ee0dabca")
SOURCE = "ee0dabcacb4cdff93f8803844f60348e0ceda6ed"
TREE = "17115913cbb29157bd00539485bcc5f38b57219c"
JUCE = "c352e2489000ffeb9e397051377d75eef2efe641"
RUN = 45397189410
DSP_RUN = 45397189426
STAMP = re.compile(r"^\ufeff?\d{4}-\d\d-\d\dT\S+Z ")
ANSI = re.compile(r"\x1b\[[0-9;]*m")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def ident(data):
    return {"bytes": len(data), "sha256": sha(data)}


def file_id(path):
    path = Path(path)
    return dict(path=str(path), **ident(path.read_bytes()))


def read(path):
    return json.loads(Path(path).read_bytes())


def frozen(path):
    return subprocess.check_output(["git", "-C", str(ROOT), "show", SOURCE + ":" + path])


def save(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def plain(raw):
    return [ANSI.sub("", STAMP.sub("", line)) for line in raw.decode("utf-8").splitlines()]


def groups(raw):
    result, current = [], None
    for index, line in enumerate(raw.decode("utf-8").splitlines(), 1):
        message = STAMP.sub("", line)
        if message.startswith("##[group]Run "):
            current = {"first_line": index, "header": message, "commands": []}
            result.append(current)
        if current is not None and "\x1b[36;1m" in message:
            current["commands"].append(ANSI.sub("", message))
        if current is not None:
            current["last_line"] = index
        if message.startswith("##[endgroup]"):
            current = None
    return result


def excerpts(raw, patterns):
    return [{"line": index, "text": message} for index, message in enumerate(plain(raw), 1)
            if any(re.search(pattern, message) for pattern in patterns)]


def receipt_check(folder, name="retrieval-receipt.json"):
    receipt = read(folder / name)
    records = []
    for relative, expected in receipt["files"].items():
        actual = file_id(folder / relative)
        assert all(actual[key] == expected[key] for key in ("bytes", "sha256")), relative
        records.append(actual)
    for item in receipt.get("archives", []):
        actual = file_id(item["local_path"])
        assert all(actual[key] == item[key] for key in ("bytes", "sha256"))
        api = read(folder / (item["kind"] + "-api.json"))
        assert api["id"] == item["artifact_id"]
        assert api["size_in_bytes"] == actual["bytes"] and api["digest"] == "sha256:" + actual["sha256"]
        assert api["workflow_run"]["id"] == RUN and api["workflow_run"]["head_sha"] == SOURCE
        actual.update(artifact_id=api["id"], name=api["name"], original_api_identity_verified=True)
        records.append(actual)
    return {"receipt": file_id(folder / name), "all_recorded_inputs_matched": True, "inputs": records}


def native(path):
    with zipfile.ZipFile(path) as archive:
        assert len(archive.namelist()) == len(set(archive.namelist()))
        assert set(archive.namelist()) == {"ci-juce-provenance.txt", "ctest-results.xml", "Testing/Temporary/LastTest.log"}
        provenance = archive.read("ci-juce-provenance.txt")
        lines = [line.split("=", 1) for line in provenance.decode().splitlines()]
        fields = dict(lines)
        assert len(fields) == len(lines)
        expected = dict(expected_sha=JUCE, actual_sha=JUCE, expected_version="8.0.11", actual_version="8.0.11",
                        workflow_commit=SOURCE, workflow_run=str(RUN), source_override="", source_clean="true",
                        verification="PASS", origin="https://github.com/juce-framework/JUCE.git", git_description="8.0.11")
        assert all(fields.get(key) == value for key, value in expected.items())
        assert fields["configured_source"] == fields["expected_source"] == fields["project_source"] + "/build/_deps/juce-src"
        xml_bytes = archive.read("ctest-results.xml")
        xml = ET.fromstring(xml_bytes)
        cases = list(xml.iter("testcase"))
        assert len(cases) == int(xml.attrib["tests"])
        assert len({case.attrib["name"] for case in cases}) == len(cases)
        counts = Counter(case.attrib["status"] for case in cases)
        selected = []
        for case in cases:
            if case.attrib["name"] in ("cs80_library_audit_tests", "cs80_ui_smoke"):
                output = case.findtext("system-out", "")
                selected.append(dict(case.attrib, output_sha256=sha(output.encode()),
                                     output_evidence=[line for line in output.splitlines() if re.search(r"library audit passed|FAIL|meter.*follows|level.*follows", line, re.I)]))
        return provenance, {"fields": fields, **ident(provenance), "junit": dict(xml.attrib, member_sha256=sha(xml_bytes)),
                            "test_status_counts": dict(counts), "selected_cases": selected,
                            "formal_skips": [case.attrib["name"] for case in cases if case.find("skipped") is not None]}


def package(path, provenance, producer_log, system):
    with zipfile.ZipFile(path) as outer:
        manifest_name = next(n for n in outer.namelist() if n.endswith(".manifest.json"))
        manifest_raw = outer.read(manifest_name)
        manifest = json.loads(manifest_raw)
        inner_name = next(n for n in outer.namelist() if n.endswith(".tar.gz") or n.endswith(".zip"))
        inner = outer.read(inner_name)
        for line in outer.read("SHA256SUMS").decode().splitlines():
            digest, name = line.split("  ", 1)
            assert sha(outer.read(name)) == digest
    assert manifest["commit"] == SOURCE and manifest["preset_library_revision"] == "2026.11.7"
    assert manifest["workflow_run_id"] == str(RUN) and manifest["juce_sha"] == JUCE and manifest["juce_version"] == "8.0.11"
    assert manifest["platform"] == system and manifest["architecture"] == ("arm64" if system == "macos" else "x86_64")
    assert '"sha256": "' + sha(inner) + '"' in "\n".join(plain(producer_log))
    prefix = inner_name.removesuffix(".tar.gz").removesuffix(".zip") + "/"
    contents = {}
    if system == "macos":
        with tarfile.open(fileobj=io.BytesIO(inner), mode="r:gz") as archive:
            for member in archive:
                assert member.name.startswith(prefix)
                if member.isfile():
                    contents[member.name[len(prefix):]] = archive.extractfile(member).read()
    else:
        with zipfile.ZipFile(io.BytesIO(inner)) as archive:
            for member in archive.infolist():
                assert member.filename.startswith(prefix)
                if not member.is_dir():
                    contents[member.filename[len(prefix):]] = archive.read(member)
    assert contents["manifest.json"] == manifest_raw
    expected_files = {row["path"]: row for row in manifest["files"] if row["type"] == "file"}
    assert set(contents) == set(expected_files) | {"manifest.json"}
    for name, row in expected_files.items():
        assert ident(contents[name]) == {"bytes": row["size"], "sha256": row["sha256"]}
    assert contents["evidence/juce-provenance.txt"] == provenance
    source_txt = contents["SOURCE.txt"].decode()
    assert "Source commit: " + SOURCE + "\n" in source_txt and "Preset library revision: 2026.11.7\n" in source_txt
    headers = []
    for name, data in contents.items():
        if system == "macos" and "/Contents/MacOS/CS-80 Reference" in name:
            magic, cpu, subtype, kind = struct.unpack("<IIII", data[:16])
            assert magic == 0xfeedfacf and cpu == 0x100000c
            assert kind == (2 if name.startswith("Standalone/") else 8)
            headers.append(dict(path=name, magic=hex(magic), cpu=hex(cpu), image_type=kind, **ident(data)))
        if system == "windows" and name in ("Standalone/CS-80 Reference.exe", "VST3/CS-80 Reference.vst3/Contents/x86_64-win/CS-80 Reference.vst3"):
            offset = struct.unpack_from("<I", data, 0x3c)[0]
            assert data[:2] == b"MZ" and data[offset:offset+4] == b"PE\0\0"
            machine = struct.unpack_from("<H", data, offset + 4)[0]
            assert machine == 0x8664
            headers.append(dict(path=name, machine=hex(machine), **ident(data)))
    assert len(headers) == (3 if system == "macos" else 2)
    return contents, {"outer": file_id(path), "inner": dict(name=inner_name, **ident(inner)),
                      "manifest": {key: value for key, value in manifest.items() if key != "files"},
                      "regular_members_rehashed": len(expected_files), "all_regular_members_match_manifest": True,
                      "embedded_and_sidecar_manifest_bytes_equal": True, "printed_original_inner_hash_matches": True,
                      "native_and_packaged_juce_provenance_bytes_equal": True,
                      "juce_provenance": ident(provenance), "juce_license": ident(contents["licenses/JUCE-LICENSE.md"]),
                      "source_txt": dict(text=source_txt, **ident(contents["SOURCE.txt"])), "binary_headers": headers,
                      "license_connection": "Original successful clean configured JUCE verification followed by successful frozen-source packager; packager reads configured_source/LICENSE.md directly.",
                      "independent_fresh_dependency_license_checkout_comparison": False,
                      "post_build_dependency_git_status_recheck": False}


def command_checks(raw, source_job, job_api, workspace, names):
    command_groups = groups(raw)
    result = []
    for name in names:
        step = next(step for step in source_job["steps"] if step.get("name") == name)
        expected = step["run"].strip().replace("${{ github.workspace }}", workspace)
        matches = [group for group in command_groups if "\n".join(group["commands"]) == expected]
        assert len(matches) == 1, name
        api = next(step for step in job_api["steps"] if step["name"] == name)
        result.append(dict(name=name, api_step=api, original_log_lines=[matches[0]["first_line"], matches[0]["last_line"]],
                           exact_frozen_command_match=True, command=expected, command_sha256=sha(expected.encode())))
    assert [row["original_log_lines"][0] for row in result] == sorted(row["original_log_lines"][0] for row in result)
    return result


def local_report_pins(report, key):
    rows = report[key]
    if isinstance(rows, dict):
        rows = list(rows.values())
    result = []
    for row in rows:
        path = Path(row["path"])
        if not path.is_absolute():
            path = ROOT / path
        actual = file_id(path)
        assert all(actual[k] == row[k] for k in ("bytes", "sha256"))
        result.append(actual)
    return result


def main():
    workflow_raw = frozen(".github/workflows/build.yml")
    workflow = yaml.safe_load(workflow_raw)
    expected_source = read(OUT / "phase1-source-inspection.json")
    for name, expected in expected_source["frozen_readset"].items():
        assert all(ident(frozen(name))[key] == expected[key] for key in ("bytes", "sha256"))
    integrity, platforms, package_contents = {}, {}, {}
    settings = [
        ("macos-arm64", "macos-arm64", "macos-arm64-native.zip", "macos-arm64-install.zip", "macos", 138455976227),
        ("windows-x86_64", "windows-x86_64", "windows-native.zip", "windows-install.zip", "windows", 138455976256),
        ("macos-x86_64", "macos-x86_64/attempt-1-failed", "macos-x86_64-attempt1-failed-native.zip", None, "macos", 138455976307),
    ]
    for target, folder_name, native_name, install_name, system, job_id in settings:
        folder = CI / folder_name
        integrity[target] = receipt_check(folder)
        api = read(folder / "producer-job-api.json")
        raw = (folder / "producer-job.log").read_bytes()
        lines = plain(raw)
        assert (api["id"], api["run_id"], api["run_attempt"], api["head_sha"]) == (job_id, RUN, 1, SOURCE)
        checkout_positions = [i for i, line in enumerate(lines) if "log -1 --format=%H" in line]
        assert len(checkout_positions) == 1 and lines[checkout_positions[0] + 1] == SOURCE
        for name in ("Run actions/checkout@v5", "Configure", "Verify configured JUCE source", "Build"):
            assert next(s for s in api["steps"] if s["name"] == name)["conclusion"] == "success"
        provenance, native_result = native(DOWNLOADS / native_name)
        assert "-- Verified clean JUCE 8.0.11 at " + JUCE + "; evidence: " + native_result["fields"]["project_source"] + "/build/ci-juce-provenance.txt" in lines
        workspace = native_result["fields"]["project_source"]
        if system == "windows":
            workspace = workspace.replace("/", "\\")
        names = ["Configure", "Verify configured JUCE source", "Build", "Test"]
        if install_name:
            names.append("Package native macOS VST3, AU and Standalone" if system == "macos" else "Package Windows VST3 and Standalone")
        commands = command_checks(raw, workflow["jobs"][system], api, workspace, names)
        metadata = excerpts(raw, [r"^Image:", r"^Version: 2026", r"^-- The (?:C|CXX) compiler identification", r"^MSBuild version", r"^-- Selecting Windows SDK", r"^Product(?:Name|Version):", r"^BuildVersion:", r"^Xcode ", r"^Build version ", r"^(?:arm64|x86_64)$", r"^Microsoft Windows Server", r"^10\.0\.20348$", r"^Datacenter$"])
        row = dict(target=target, source=SOURCE, tree=TREE, run_id=RUN, attempt=1, producer_job_id=job_id,
                   original_job_conclusion=api["conclusion"], original_job_api=file_id(folder / "producer-job-api.json"),
                   original_log=file_id(folder / "producer-job.log"), exact_checkout_verified=True,
                   exact_configure_juce_build_test_commands=commands, actual_environment_evidence=metadata,
                   native=native_result, full_compilation_database_retained=False, full_cmake_cache_retained=False,
                   complete_compiler_flags_observed=False, complete_reproducible_build_established=False)
        if install_name:
            contents, package_result = package(DOWNLOADS / install_name, provenance, raw, system)
            package_contents[target] = contents
            row.update(package=package_result, tracked_product_source_clean_at_package=True,
                       tracked_source_clean_basis="Successful original frozen-source packager enforces git diff --quiet HEAD -- and GITHUB_SHA equality.",
                       build_dependency_package_provenance_accepted=True, decision="accepted_available_original_build_to_package_provenance")
            qualifier = read(folder / "platform-qualification.json")
            assert qualifier["platform_qualified"] is True and qualifier["release_qualified"] is False
            row["platform_qualifier"] = file_id(folder / "platform-qualification.json")
            if system == "windows":
                assert row["platform_qualifier"]["sha256"] == "a86ab93d18fb390359f60be8a50ed9cc09b34c9b70aab8922c23581fa455c048"
                row["qualifier_report_input_hashes_checked"] = local_report_pins(qualifier, "input_report_identities")
                consumer_api = read(folder / "validator-job-api.json")
                consumer_raw = (folder / "validator-job.log").read_bytes()
                assert (consumer_api["id"], consumer_api["run_id"], consumer_api["run_attempt"], consumer_api["head_sha"], consumer_api["conclusion"]) == (138483772246, RUN, 1, SOURCE, "success")
                assert SOURCE in plain(consumer_raw)
                row["consumer_job_id"] = 138483772246
                row["consumer_artifact_selection_evidence"] = excerpts(consumer_raw, [r"14924634293", r"cs80-windows-vst3", r"Started validating:", r"Verified official pluginval"])
                with zipfile.ZipFile(DOWNLOADS / "windows-vst3.zip") as z:
                    consumed = {name: z.read(name) for name in z.namelist() if not name.endswith("/") and name.startswith("CS-80 Reference.vst3/")}
                    packaged = {name.removeprefix("VST3/"): value for name, value in contents.items() if name.startswith("VST3/")}
                    assert consumed == packaged and len(consumed) == 2
                    sidecars = [name for name in z.namelist() if name not in consumed]
                    assert sorted(sidecars) == ["CS-80 Reference.exp", "CS-80 Reference.lib"]
                row["complete_selected_vst3_regular_bytes_equal"] = True
                row["recorded_top_level_compiler_sidecars"] = sidecars
                validator_zip, validator_log = "windows-validator.zip", consumer_raw
            else:
                proof = read(folder / "original-execution-binding.json")
                row["qualifier_report_input_hashes_checked"] = local_report_pins(proof, "input_files")
                validator_zip, validator_log = "macos-arm64-validator.zip", raw
            constants = expected_source["pluginval_source_constants"][system]
            with zipfile.ZipFile(DOWNLOADS / validator_zip) as z:
                download_raw = z.read("pluginval-download.json")
                download = json.loads(download_raw)
                for field, key in [("version", "VERSION"), ("release_id", "RELEASE_ID"), ("asset_id", "ASSET_ID"), ("expected_size", "EXPECTED_SIZE"), ("actual_size", "EXPECTED_SIZE"), ("expected_sha256", "EXPECTED_SHA256"), ("actual_sha256", "EXPECTED_SHA256")]:
                    assert download[field] == constants[key]
                assert download["verified"] is True and len(download["attempts"]) == 1
                assert download["attempts"][0]["transfer_exit"] == 0
                assert any("Verified official pluginval 1.0.4" in line and download["actual_sha256"] in line for line in plain(validator_log))
                row["pluginval_download"] = dict(receipt=download, **ident(download_raw), original_success_output_bound=True)
                if system == "macos":
                    au_path = "AU/CS-80 Reference.component/Contents/MacOS/CS-80 Reference"
                    au_hash = sha(contents[au_path])
                    au_receipts = {}
                    for name in ["au-payload-before-validation.sha256", "au-payload-after-validation.sha256"]:
                        data = z.read(name)
                        hashes = [line.split()[0] for line in data.decode().splitlines()]
                        assert hashes == [au_hash, au_hash]
                        au_receipts[name] = dict(hashes=hashes, **ident(data))
                    assert "codesign --force --sign - \"$AU_SOURCE\"" in "\n".join(lines)
                    row["au_before_after_packaged_executable_equal"] = au_receipts
                    row["signing_scope"] = "Observed original ad-hoc AU signing; no publisher Developer ID or notarization claim."
        else:
            assert api["conclusion"] == "failure" and native_result["junit"]["failures"] == "1"
            assert next(s for s in api["steps"] if s["name"] == "Test")["conclusion"] == "failure"
            assert next(s for s in api["steps"] if s["name"] == "Package native macOS VST3, AU and Standalone")["conclusion"] == "skipped"
            row.update(package=None, tracked_product_source_clean_at_package=None, build_dependency_package_provenance_accepted=False,
                       decision="original_checkout_configuration_juce_and_build_verified_test_failed_no_package_acceptance",
                       retry_request=read(folder / "rerun-request-while-active.json"))
        platforms[target] = row
    arm_license = package_contents["macos-arm64"]["licenses/JUCE-LICENSE.md"]
    win_license = package_contents["windows-x86_64"]["licenses/JUCE-LICENSE.md"]
    assert b"\r" not in arm_license and arm_license.replace(b"\n", b"\r\n") == win_license
    platforms["linux-x86_64"] = dict(target="linux-x86_64", source=SOURCE, run_id=RUN, attempt=1, producer_job_id=138455975798,
                                     original_producer_log_inspected=False, native_archive_inspected=False, install_archive_inspected=False,
                                     actual_toolchain=None, actual_juce_report=None, build_dependency_package_provenance_accepted=False,
                                     decision="pending_original_native_linux_receipts; supporting_DSP_is_separate")
    save("actual-platform-provenance.json", dict(schema=1, inspected_at_utc=datetime.now(timezone.utc).isoformat(),
         source=SOURCE, tree=TREE, run_id=RUN, attempt=1, platforms=platforms,
         cross_platform_juce_license_observation={"arm_bytes": len(arm_license), "windows_bytes": len(win_license),
             "windows_is_exact_LF_to_CRLF_transform_of_ARM": True,
             "basis": "Actual package bytes compared; each license is accepted through its own original clean dependency/read path, not by substituting the other platform."},
         actual_native_archives_inspected=3, actual_install_archives_inspected=2, all_four_platforms_qualified=False, release_qualified=False))
    supporting(integrity)
    save("actual-input-integrity.json", dict(schema=1, source=SOURCE, receipts=integrity, all_checked_inputs_matched=True))
    print("Available original provenance checked: ARM/Windows build-to-package accepted; Intel original failed; native Linux pending. Supporting DSP kept separate.")


def supporting(integrity):
    receipt_path = CI / "supporting-acquisition-receipt.json"
    assert sha(receipt_path.read_bytes()) == "c010e15aa4b1d5528ad3e0c56ea63e7d822afde0de70c7eeee347b2cae896f59"
    integrity["supporting_root_inputs"] = receipt_check(CI, "supporting-acquisition-receipt.json")
    run = read(CI / "supporting-dsp/run-api-at-completion.json")
    jobs = read(CI / "supporting-dsp/jobs-api-at-completion.json")["jobs"]
    assert (run["id"], run["run_attempt"], run["head_sha"], run["conclusion"]) == (DSP_RUN, 1, SOURCE, "success")
    assert len(jobs) == 5
    dsp_workflow = yaml.safe_load(frozen(".github/workflows/dsp-measurements.yml"))
    source_jobs = {138456125317: "sanitizers", 138456125576: "parity-harness", 138456125703: "source-factory-compatibility",
                   138456125758: "legacy-compatibility", 138456125934: "measure"}
    rows = []
    for job in jobs:
        assert (job["run_id"], job["run_attempt"], job["head_sha"], job["conclusion"]) == (DSP_RUN, 1, SOURCE, "success")
        path = (CI / "supporting-dsp/sanitizer-job-138456125317.log") if job["id"] == 138456125317 else OUT / ("actual-supporting/job-" + str(job["id"]) + ".log")
        raw = path.read_bytes()
        assert SOURCE in plain(raw)
        source_job = dsp_workflow["jobs"][source_jobs[job["id"]]]
        executed_commands = command_checks(raw, source_job, job, "", [step["name"] for step in source_job["steps"] if "run" in step])
        rows.append(dict(id=job["id"], name=job["name"], conclusion=job["conclusion"], original_log=file_id(path),
                         original_checkout_full_source_present=True, steps=job["steps"],
                         exact_frozen_execution_commands=executed_commands,
                         evidence=excerpts(raw, [r"^-- The (?:C|CXX) compiler identification", r"^Ran \d+ tests", r"^OK", r"^Synthetic plumbing", r"^DSP probes:", r"100% tests passed", r"Total Test time", r"\*\*\*Skipped", r"ASAN_OPTIONS:", r"UBSAN_OPTIONS:", r"cmake -S .*fsanitize", r"^Image:", r"^Version: 2026"])))
    sanitizer_path = CI / "supporting-dsp/sanitizer-job-138456125317.log"
    sanitizer = plain(sanitizer_path.read_bytes())
    results = []
    for line in sanitizer:
        match = re.search(r"\b(\d+)/226 Test\s+#(\d+):\s+(\S+)\s+\.*\s*(Passed|\*\*\*Skipped)\s+([\d.]+) sec", line)
        if match:
            results.append(dict(ordinal=int(match[1]), number=int(match[2]), name=match[3], result=match[4], seconds=float(match[5])))
    assert len(results) == 226 and len({r["number"] for r in results}) == 226
    skipped = [r for r in results if r["result"] == "***Skipped"]
    assert len(skipped) == 1 and skipped[0]["name"] == "cs80_stereo_pra_exact_tests"
    assert "100% tests passed, 0 tests failed out of 226" in sanitizer
    assert "Total Test time (real) = 3504.38 sec" in sanitizer
    assert "-- The CXX compiler identification is GNU 13.3.0" in sanitizer
    skip_source = frozen("tests/StereoPraExactTests.cpp")
    assert "__GNUC__ == 14 && __GNUC_MINOR__ == 2" in skip_source.decode() and "int main(){ return 77; }" in skip_source.decode()
    artifact_api = read(CI / "supporting-dsp/artifact-collection-api.json")["artifacts"]
    assert all(a["workflow_run"]["id"] == DSP_RUN and a["workflow_run"]["head_sha"] == SOURCE for a in artifact_api)
    acquired = []
    for artifact_id in [14921709992, 14922229219, 14921987990]:
        path = OUT / ("actual-supporting/artifact-" + str(artifact_id) + ".zip")
        api = next(a for a in artifact_api if a["id"] == artifact_id)
        assert ident(path.read_bytes()) == {"bytes": api["size_in_bytes"], "sha256": api["digest"].removeprefix("sha256:")}
        with zipfile.ZipFile(path) as archive:
            report = json.loads(archive.read("report.json"))
            assert report["analog_evidence"] is False
            result = dict(artifact_id=artifact_id, name=api["name"], archive=file_id(path), report=report,
                          members=[dict(path=i.filename, **ident(archive.read(i))) for i in archive.infolist()],
                          original_artifact_api_digest_matched=True)
            if artifact_id == 14921987990:
                assert report["passed"] is True and report["provenance"]["source_revision"] == SOURCE
                assert report["provenance"]["working_tree_dirty"] is False
                assert report["manifest_sha256"] == sha(archive.read("manifest.json"))
                assert report["comparator_sha256"] == sha(frozen("scripts/parity_harness.py"))
            else:
                assert report["candidate"] == SOURCE
                sums = dict((name, digest) for digest, name in (line.split("  ", 1) for line in archive.read("SHA256SUMS").decode().splitlines()))
                result["recorded_checksums"] = sums
                result["compiler"] = archive.read("compiler.txt").decode()
                if artifact_id == 14921709992:
                    assert report["circuit_baseline"] == "22d6a32c48c85f9b813506edcd856f73f4293c2d"
                    assert report["audio_baseline"] == "00ceccfacb3cb9845436701da01c4387d53a361a"
                    assert report["v1_circuit_bytes_unchanged"] is True and report["v2_audio_and_numeric_state_bytes_unchanged"] is True
                    assert sums["artifacts/source-factory-compatibility/audio-current.bin"] == sums["artifacts/source-factory-compatibility/audio-v2.bin"]
                    assert sums["artifacts/source-factory-compatibility/circuit-current.bin"] == sums["artifacts/source-factory-compatibility/circuit-v1.bin"]
                else:
                    assert report["baseline"] == report["corpus_source"] == "d56911a7c5f6953cd244457226b126cc567a538b"
                    assert report["default_off_byte_identical"] is True
                    assert sha(archive.read("baseline-corpus.txt")) == report["corpus_sha256"] == sums["artifacts/compatibility/baseline-corpus.txt"]
                    for name in ("default", "tails"):
                        assert sums["artifacts/compatibility/baseline-" + name + ".f32"] == sums["artifacts/compatibility/current-" + name + ".f32"]
                result["raw_audio_binary_comparison_repeated_locally"] = False
                result["scope"] = "Original successful cmp commands and retained digest pairs verified; original raw .bin/.f32 payloads were not included in this artifact."
            producer = {14921709992: 138456125703, 14922229219: 138456125758, 14921987990: 138456125576}[artifact_id]
            log = (OUT / ("actual-supporting/job-" + str(producer) + ".log")).read_bytes().decode()
            assert str(artifact_id) in log and api["digest"].removeprefix("sha256:") in log
            result["original_upload_id_and_digest_matched"] = True
            acquired.append(result)
    upstream_path = CI / "supporting-upstream-prior-source/job-138456160979.log"
    upstream = plain(upstream_path.read_bytes())
    assert any("SOURCE_RUN_ID: 45385716988" in line for line in upstream)
    assert any("##[error]Unhandled error: Error: Expected exactly one non-expired cs80-linux-vst3 artifact" in line for line in upstream)
    upstream_jobs = read(CI / "supporting-upstream-prior-source/jobs-api-at-completion.json")["jobs"]
    assert any(job["id"] == 138456160979 and job["conclusion"] == "failure" for job in upstream_jobs)
    prior_run_path = CI.parent / "source-c5549f7d/run-initial.json"
    prior_run = read(prior_run_path)
    assert (prior_run["id"], prior_run["head_sha"]) == (45385716988, "c5549f7d095a5c5f49a5ad22ec0ccc819245d201")
    for job in upstream_jobs:
        assert job["run_id"] == 45397242252 and job["head_sha"] == SOURCE and job["conclusion"] == "failure"
        for name in ["Download only the verified build artifact", "Run one ordinary validation and preserve its result"]:
            assert next(step for step in job["steps"] if step["name"] == name)["conclusion"] == "skipped"
    save("actual-supporting-findings.json", dict(schema=1, source=SOURCE, dsp_run_id=DSP_RUN, dsp_attempt=1,
         original_run_api=file_id(CI / "supporting-dsp/run-api-at-completion.json"), all_five_dsp_jobs_success=True,
         original_jobs=rows, sanitizer={"job_id": 138456125317, "registered": 226, "passed": 225, "failed": 0,
             "formal_skipped": 1, "seconds": 3504.38, "formal_skip": skipped,
             "formal_skip_reason": "Frozen exact-specialization fixture requires GCC 14.2; actual original sanitizer configure reports GCC 13.3.0. Else branch returns 77, registered as SKIP_RETURN_CODE.",
             "formal_skip_source": dict(path="tests/StereoPraExactTests.cpp", **ident(skip_source)),
             "nested_python_skip_total": None,
             "nested_skip_visibility": "Unobservable from the original --output-on-failure console for successful cases. No sanitizer LastTest.log or JUnit artifact is listed or uploaded by the frozen workflow.",
             "core_only_plugin_off": True, "native_linux_plugin_qualified": False, "results": results},
         acquired_small_artifacts=acquired,
         artifact_content_not_acquired=[{k:a[k] for k in ("id", "name", "size_in_bytes", "digest")} for a in artifact_api if a["id"] not in [14921709992, 14922229219, 14921987990]],
         supporting_upstream_prior_source={"workflow_run_id": 45397242252, "source_run_id": 45385716988,
             "workflow_definition_head_sha": SOURCE, "targeted_product_source_sha": prior_run["head_sha"],
             "prior_run_identity_context_only": file_id(prior_run_path),
             "all_three_jobs_failed_before_product_download_or_validation": True,
             "jobs": [{key: job[key] for key in ("id", "name", "head_sha", "run_id", "run_attempt", "conclusion")} for job in upstream_jobs],
             "original_full_failure_logs_inspected": 1,
             "original_job_log": file_id(upstream_path), "failure_before_product_download_and_validation": True,
             "evidence": excerpts(upstream_path.read_bytes(), [r"^SOURCE_RUN_ID:", r"SOURCE_RUN_ID: 45385716988", r"^##\[error\]"]),
             "scope": "Separate prior-source artifact-discovery failure; not a validator result for this ee0 source. The separate failed workflow is not reported green."},
         frozen_dsp_workflow=ident(frozen(".github/workflows/dsp-measurements.yml")),
         release_qualified=False, all_ci_green=False))
    save("actual-supporting/acquisition-receipt.json", dict(schema=1, acquired_at_utc=datetime.now(timezone.utc).isoformat(),
         source=SOURCE, run_id=DSP_RUN, attempt=1,
         transport="Authenticated repository-owner GitHub connector, original job-log endpoint and artifact download endpoint. Decoded original logs saved by UTF-8 encoding without newline rewriting; temporary download URLs are not retained in files.",
         raw_files=[file_id(path) for path in sorted((OUT / "actual-supporting").glob("*")) if path.suffix in (".log", ".zip")],
         original_metadata_inputs=[file_id(CI / "supporting-dsp/jobs-api-at-completion.json"), file_id(CI / "supporting-dsp/artifact-collection-api.json")],
         all_downloaded_zip_sizes_and_digests_match_original_metadata=True, release_qualified=False))


if __name__ == "__main__":
    main()
