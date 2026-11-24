#!/usr/bin/env python3
"""Inspect frozen build-provenance inputs; never qualify unavailable CI outputs."""
from __future__ import annotations

import ast
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
EVIDENCE = ROOT / "docs/preset-evidence/2026.11.7"
SOURCE = "ee0dabcacb4cdff93f8803844f60348e0ceda6ed"
TREE = "17115913cbb29157bd00539485bcc5f38b57219c"
OLD_SOURCE = "c5549f7d095a5c5f49a5ad22ec0ccc819245d201"
JUCE = "c352e2489000ffeb9e397051377d75eef2efe641"
RUN = 45397189410
ATTEMPT = 1
PLATFORMS = {
    "linux-x86_64": (138455975798, "ubuntu-24.04", "linux", "x86_64", "cs80-linux-test-evidence", "cs80-linux-install-bundle", "cs80-linux-vst3", "cs80-pluginval-reports"),
    "windows-x86_64": (138455976256, "windows-2022", "windows", "x86_64", "cs80-windows-test-evidence", "cs80-windows-install-bundle", "cs80-windows-vst3", "cs80-pluginval-windows"),
    "macos-arm64": (138455976227, "macos-15", "macos", "arm64", "cs80-macos-15-test-evidence", "cs80-macos-15-install-bundle", None, "cs80-pluginval-macos-15"),
    "macos-x86_64": (138455976307, "macos-15-intel", "macos", "x86_64", "cs80-macos-15-intel-test-evidence", "cs80-macos-15-intel-install-bundle", None, "cs80-pluginval-macos-15-intel"),
}


def git(*args: str) -> bytes:
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def identity(data: bytes) -> dict:
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def local(path: Path) -> dict:
    return dict(path=str(path.relative_to(ROOT)), **identity(path.read_bytes()))


def write(name: str, value: dict) -> None:
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def constants(source: str) -> dict:
    result = {}
    for statement in ast.parse(source).body:
        if isinstance(statement, ast.Assign) and len(statement.targets) == 1:
            target = statement.targets[0]
            if isinstance(target, ast.Name) and target.id in {"VERSION", "RELEASE_ID", "ASSET_ID", "EXPECTED_SIZE", "EXPECTED_SHA256"}:
                result[target.id] = ast.literal_eval(statement.value)
    return result


def main() -> None:
    assert git("rev-parse", SOURCE + "^{tree}").decode().strip() == TREE
    names = [".github/workflows/build.yml", "CMakeLists.txt", "scripts/verify-ci-juce-source.cmake",
             "scripts/package-ci-artifacts.py", "scripts/download-pluginval-macos.py",
             "scripts/download-pluginval-windows.py", "scripts/run-pluginval-linux.sh",
             "scripts/patch-juce-automation.py", "scripts/patch-juce-state-mirror.py"]
    data = {name: git("show", SOURCE + ":" + name) for name in names}
    texts = {name: value.decode() for name, value in data.items()}
    workflow = texts[".github/workflows/build.yml"]
    verifier = texts["scripts/verify-ci-juce-source.cmake"]
    packager = texts["scripts/package-ci-artifacts.py"]
    checks = {
        "cmake_fetch_immutable_juce_sha": "GIT_TAG " + JUCE in texts["CMakeLists.txt"],
        "workflow_three_native_definitions_require_exact_juce_sha": workflow.count("EXPECTED_JUCE_SHA=" + JUCE) == 3,
        "workflow_three_native_definitions_require_exact_juce_version": workflow.count("EXPECTED_JUCE_VERSION=8.0.11") == 3,
        "verifier_reads_successful_configure_cache": 'load_cache("${build_dir}" READ_WITH_PREFIX configured_' in verifier,
        "verifier_rejects_source_override": 'if(NOT "${configured_FETCHCONTENT_SOURCE_DIR_JUCE}" STREQUAL "")' in verifier,
        "verifier_enforces_own_git_root": 'if(NOT git_root STREQUAL actual_source)' in verifier,
        "verifier_records_complete_git_status": 'status --porcelain=v1 --untracked-files=all' in verifier,
        "verifier_requires_clean_matching_sha_version": all(t in verifier for t in ['if(NOT actual_sha STREQUAL EXPECTED_JUCE_SHA)', 'if(NOT changes STREQUAL "")', 'if(NOT actual_version STREQUAL EXPECTED_JUCE_VERSION)']),
        "packager_checks_tracked_product_tree_against_head": all(t in packager for t in ['"diff", "--quiet", "HEAD", "--"', 'os.environ["GITHUB_SHA"] == commit']),
        "packager_requires_source_bound_passing_juce_report": all(t in packager for t in ['fields.get("verification") == "PASS"', 'fields.get("source_clean") == "true"', 'fields.get("workflow_commit") == commit']),
        "packager_requires_exact_fetched_dependency_path": 'source.resolve() == (build / "_deps/juce-src").resolve()' in packager,
        "packager_reads_juce_license_directly_from_configured_source": all(t in packager for t in ['license_file = source / "LICENSE.md"', '"licenses/JUCE-LICENSE.md": license_file.read_bytes()']),
        "packager_embeds_original_report_bytes": '"evidence/juce-provenance.txt": report_data' in packager,
        "packager_source_and_revision_metadata": all(t in packager for t in ['Source commit: {commit}', 'Preset library revision: {preset_revision}', '"workflow_run_id": os.environ.get("GITHUB_RUN_ID", "")']),
        "packager_no_signing_claim": 'signature_status="not inspected; no release signing/notarization performed by this helper"' in packager,
        "mac_au_ad_hoc_sign_then_copy_then_compare": all(t in workflow for t in ['codesign --force --sign - "$AU_SOURCE"', 'cp -R "$AU_SOURCE" "$AU_DEST"', 'au-payload-before-validation.sha256', 'au-payload-after-validation.sha256']),
        "linux_pluginval_version_url_without_independent_archive_pin": all(t in workflow for t in ['PLUGINVAL_VERSION: "1.0.4"', 'releases/download/v${PLUGINVAL_VERSION}/pluginval_Linux.zip']) and "EXPECTED_PLUGINVAL_LINUX_SHA" not in workflow,
    }
    assert all(checks.values()), checks
    ci = EVIDENCE / "ci/source-ee0dabca"
    receipt = json.loads((ci / "initial-api-retrieval-receipt.json").read_text())
    receipt_checks = {name: identity((ci / name).read_bytes()) == expected for name, expected in receipt["files"].items()}
    assert all(receipt_checks.values()), receipt_checks
    run = json.loads((ci / "initial-run-api.json").read_text())
    jobs = json.loads((ci / "initial-jobs-api.json").read_text())["jobs"]
    commit = json.loads((ci / "source-commit-api.json").read_text())
    assert (run["id"], run["head_sha"], run["run_attempt"], run["repository"]["full_name"]) == (RUN, SOURCE, ATTEMPT, "cheshmakzanoon-ops/yamaha-cs80")
    assert commit["sha"] == SOURCE and commit["tree"]["sha"] == TREE
    a40_dir = EVIDENCE / "verification/a40/source-ee0dabca"
    context = json.loads((a40_dir / "consumer-context.json").read_text())
    assert (context["source"], context["tree"], context["run_id"], context["attempt"]) == (SOURCE, TREE, RUN, ATTEMPT)
    context_pins = {name: identity((ROOT / row["path"]).read_bytes()) == {key: row[key] for key in ("bytes", "sha256")} for name, row in context["helpers"].items()}
    assert all(context_pins.values()), context_pins
    unchanged_trees = {name: git("rev-parse", SOURCE + ":" + name).decode().strip() == git("rev-parse", OLD_SOURCE + ":" + name).decode().strip()
                       for name in (".github", "cmake", "scripts", "src/core", "src/plugin", "src/vendor", "assets", "installer")}
    assert all(unchanged_trees.values()), unchanged_trees
    source_readset = {name: dict(git_blob=git("rev-parse", SOURCE + ":" + name).decode().strip(), **identity(value)) for name, value in data.items()}
    write("phase1-source-inspection.json", {
        "schema": 1, "status": "frozen_source_contract_checked_no_actual_artifact_acceptance",
        "inspected_at_utc": datetime.now(timezone.utc).isoformat(),
        "source": SOURCE, "tree": TREE, "factory_revision": "2026.11.7", "run_id": RUN, "attempt": ATTEMPT,
        "workspace_head_observed_only": git("rev-parse", "HEAD").decode().strip(),
        "workspace_head_is_not_binary_source": True,
        "frozen_readset": source_readset, "source_checks": checks,
        "initial_api_receipt_checks": receipt_checks,
        "initial_raw_inputs": [local(ci / name) for name in ["initial-api-retrieval-receipt.json", "context.json", "initial-run-api.json", "initial-jobs-api.json", "source-commit-api.json"]],
        "a40_context": local(a40_dir / "consumer-context.json"), "a40_helper_pin_checks": context_pins,
        "independently_compared_unchanged_trees": unchanged_trees,
        "pluginval_source_constants": {key: constants(texts["scripts/download-pluginval-" + key + ".py"]) for key in ("macos", "windows")},
        "linux_pluginval_archive_digest_pin": None,
        "actual_native_platforms_inspected": [], "actual_install_packages_inspected": [],
        "actual_build_to_package_provenance_accepted": False,
        "release_qualified": False,
    })
    matrix = {}
    for target, (job_id, runner, system, arch, native, install, payload, validator) in PLATFORMS.items():
        job = next(j for j in jobs if j["id"] == job_id)
        assert (job["run_id"], job["head_sha"], job["run_attempt"], job["labels"]) == (RUN, SOURCE, ATTEMPT, [runner])
        windows = system == "windows"
        matrix[target] = {
            "producer_job_id": job_id, "source": SOURCE, "run_id": RUN, "attempt": ATTEMPT,
            "expected_runner_label": runner, "expected_binary_platform": system, "expected_binary_architecture": arch,
            "initial_api_observed": {key: job[key] for key in ("status", "conclusion", "started_at", "completed_at", "labels")},
            "initial_api_steps_observed": [{key: step[key] for key in ("name", "status", "conclusion")} for step in job["steps"]],
            "required_artifact_names": {"native_test_evidence": native, "install_bundle": install, "vst3_payload_for_separate_validator": payload, "validator": validator},
            "declared_configure_command": 'cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCS80_BUILD_PLUGIN=ON -DCS80_BUILD_TESTS=ON' if windows else "cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=ON -DCS80_BUILD_TESTS=ON",
            "declared_build_command": "cmake --build build --config Release --parallel" if windows else ('cmake --build build --parallel "$(nproc)"' if system == "linux" else "cmake --build build --parallel"),
            "declared_packager_command": "python scripts/package-ci-artifacts.py --platform " + system + " --build-dir build --out-dir packages",
            "required_raw_inputs": ["completed authenticated run and exact-attempt API", "completed authenticated producer-job API and full original log", "artifact API ID/name/digest/run/head metadata", "actual outer install ZIP and selected inner package", "actual native-test ZIP", "original validator job/log and validator ZIP"],
            "checks_when_actual_inputs_arrive": [
                "Bind exact checkout SHA and clean tracked product source to original configure, JUCE verify, build, tests, packaging and upload sequence.",
                "Read actual runner OS/version, architecture, compiler/version, generator and macOS Xcode/SDK evidence from logs and package; distinguish missing fields.",
                "Hash outer archives against authenticated API digests and original upload logs.",
                "Check manifest/source metadata names ee0dabca full SHA, revision .7, run 45397189410 and expected native architecture.",
                "Compare native ci-juce-provenance.txt and embedded evidence/juce-provenance.txt byte for byte; reject wrong workflow/source/run/path/clean/version fields.",
                "Connect bundled JUCE license bytes to successful original clean dependency verification and source-pinned packager read path; do not substitute a local JUCE checkout.",
                "Inspect original Mac/Windows pluginval receipt against frozen official asset constants, or record Linux version URL and lack of independent digest pin.",
                "Reuse platform payload and signing-scope reports only after checking their raw metadata and log bindings.",
            ],
            "actual_native_test_archive": None, "actual_install_archive": None,
            "actual_checkout_sha_observed_in_original_log": None,
            "actual_toolchain": None, "actual_os": None, "actual_architecture": None,
            "actual_juce_provenance": None, "actual_native_package_provenance_bytes_equal": None,
            "actual_juce_license_package_connection": None, "actual_pluginval_receipt": None,
            "decision": "pending_actual_receipts_not_qualified",
        }
    write("phase1-four-platform-input-matrix.json", {
        "schema": 1, "source": SOURCE, "tree": TREE, "run_id": RUN, "attempt": ATTEMPT,
        "status": "exact_inputs_prepared_no_actual_native_archives_inspected", "platforms": matrix,
        "remaining_scope_limits": [
            "Initial API status is historical; no inference about the current run status is made.",
            "Expected suite counts and 15 validator lanes are context only and are not qualified by this provenance review.",
            "Source pinning, successful build logs, and package hashes do not establish a complete reproducible build.",
            "The workflow does not upload full CMakeCache.txt or compile_commands.json with native test evidence; unseen final compiler flags are not inferred.",
            "The JUCE verifier checks all tracked/untracked dependency source changes at verification time; the packager checks tracked product changes at package time and consumes the earlier report.",
            "Successful source-pinned packaging plus reviewed intervening commands supports the dependency-license path; it is not a fresh post-build JUCE clean check.",
            "macOS AU ad-hoc signing and artifact hashes are not publisher Developer ID signing, notarization, or release authorization.",
        ],
        "release_qualified": False,
    })
    print("Frozen source contract and initial API bindings verified; actual platform archive acceptance remains pending for all four targets.")


if __name__ == "__main__":
    main()
