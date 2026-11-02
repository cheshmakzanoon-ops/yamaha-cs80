#!/usr/bin/env python3
"""Reject real distribution-helper defects in isolated synthetic fixture copies."""
from __future__ import annotations
import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FILES = ["scripts/distribution_docs.py", "scripts/package-ci-artifacts.py",
         "tests/release/test_distribution_docs.py", "installer/linux/make-deb.sh",
         "installer/windows/cs80-reference.iss", ".github/workflows/release.yml"]
MUTATIONS = [
    ("omit_barlow_ofl", '    "licenses/BarlowCondensed-OFL.txt": "assets/fonts/barlow-condensed/OFL.txt",\n', ""),
    ("omit_mts_notice", '    "licenses/MTS-ESP-LICENSE.txt": "deps/mts-esp/LICENSE",\n', ""),
    ("omit_font_attribution", '    "licenses/BarlowCondensed-SOURCE.txt": "assets/fonts/barlow-condensed/SOURCE.txt",\n', ""),
    ("change_notice_bytes", '    return payload\n', '    payload["licenses/MTS-ESP-LICENSE.txt"] += b"mutated"\n    return payload\n'),
    ("ignore_workflow_identity", 'require(not workflow_sha or workflow_sha == commit,', 'require(True,'),
    ("ignore_dirty_checkout", '["git", "-C", str(root), "diff", "--quiet", "HEAD", "--"],\n                           check=False).returncode == 0,',
     '["git", "-C", str(root), "diff", "--quiet", "HEAD", "--"],\n                           check=False).returncode in (0, 1),'),
    ("allow_untracked_notice", '        git("ls-files", "--error-unmatch", "--", path)', '        pass  # mutation: trust an untracked replacement'),
    ("wrong_manifest_digest_after_self_check", '    return manifest\n',
     '    corrupted = {**manifest, "files": [{**item, "sha256": "0" * 64} for item in manifest["files"]]}\n'
     '    (out / "distribution-manifest.json").write_text(json.dumps(corrupted) + "\\n")\n'
     '    return manifest\n'),
]

# The original untracked-notice defect now has two independent barriers. Removing
# only the index-membership check is correctly harmless. This explicitly joint
# control also restores the old working-file payload lookup; no survivor is
# counted as a semantic kill.
JOINT_REPLACEMENTS = {
    "allow_untracked_notice": (
        '    payload = {output: revision_notice(root, source, commit) for output, source in NOTICE_INPUTS.items()}',
        '    payload = {output: read_notice(root / source, root) for output, source in NOTICE_INPUTS.items()}'),
}


EXPECTED_FAILURES = {
    "omit_barlow_ofl": ["test_ci_and_stage_reject_untracked_notice_replacement",
        "test_ci_payload_missing_notice_fails_closed", "test_ci_payload_preserves_every_notice",
        "test_deb_payload_roundtrip", "test_stage_rejects_linked_notice",
        "test_stage_rejects_missing_notice", "test_staged_metadata_is_exact_and_explicitly_limited"],
    "omit_mts_notice": ["test_ci_payload_preserves_every_notice", "test_deb_payload_roundtrip",
        "test_stage_rejects_missing_notice", "test_staged_metadata_is_exact_and_explicitly_limited"],
    "omit_font_attribution": ["test_ci_payload_preserves_every_notice", "test_deb_payload_roundtrip",
        "test_stage_rejects_missing_notice", "test_staged_metadata_is_exact_and_explicitly_limited"],
    "change_notice_bytes": ["test_ci_payload_preserves_every_notice", "test_deb_payload_roundtrip",
        "test_staged_metadata_is_exact_and_explicitly_limited"],
    "ignore_workflow_identity": ["test_stage_rejects_workflow_identity_mismatch"],
    "ignore_dirty_checkout": ["test_stage_rejects_dirty_source"],
    "allow_untracked_notice": ["test_ci_and_stage_reject_untracked_notice_replacement"],
    "wrong_manifest_digest_after_self_check": ["test_deb_payload_roundtrip",
        "test_staged_metadata_is_exact_and_explicitly_limited"],
}


def classify_run(returncode, output, expected_failures):
    """A runtime failure, skip, wrong assertion or incomplete run is not a kill."""
    failures = re.findall(r"^FAIL: (\w+) ", output, re.MULTILINE)
    errors = re.findall(r"^ERROR: (\w+) ", output, re.MULTILINE)
    inventory = re.findall(r"^Ran (\d+) tests in ", output, re.MULTILINE)
    complete = inventory == ["11"] and "skipped=" not in output
    expected_summary = (f"FAILED (failures={len(expected_failures)})"
                        if expected_failures else "OK")
    summary = re.findall(r"^(?:OK(?: \(.*\))?|FAILED \(.*\))$", output, re.MULTILINE)
    qualified = (complete and not errors and sorted(failures) == sorted(expected_failures)
                 and summary == [expected_summary]
                 and returncode == (1 if expected_failures else 0))
    return {"qualified": qualified, "ordinary_failures": failures, "runtime_errors": errors,
            "test_inventory": inventory, "expected_failures": expected_failures}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=False)
    contract = json.loads((ROOT / "tests/release/distribution-mutation-evidence-contract.json").read_text())
    if {name: len(tests) for name, tests in EXPECTED_FAILURES.items()} != contract["ordinary_failure_counts"]:
        raise AssertionError("Mutation inventory differs from preregistered assertion counts")
    reports = []
    with tempfile.TemporaryDirectory(prefix="cs80-distribution-mutations-") as temp:
        scratch = Path(temp)
        for relative in FILES:
            target = scratch / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, target)
        helper = scratch / "scripts/distribution_docs.py"
        original = helper.read_text()
        runtime_probe = ("runtime_error_control", "    root = root.resolve()\n    if commit is None:",
                         "    raise RuntimeError('deliberate unqualified mutant crash')\n    if commit is None:")
        for name, before, after in [("positive", "", ""), *MUTATIONS, runtime_probe, ("restored", "", "")]:
            if before and original.count(before) != 1:
                raise AssertionError(f"Mutation anchor is not unique: {name}")
            changed = original.replace(before, after) if before else original
            joint = JOINT_REPLACEMENTS.get(name)
            if joint is not None:
                old, new = joint
                if changed.count(old) != 1:
                    raise AssertionError('Joint mutation anchor is not unique: ' + name)
                changed = changed.replace(old, new)
            helper.write_text(changed)
            shutil.rmtree(scratch / "scripts/__pycache__", ignore_errors=True)
            result = subprocess.run([sys.executable, str(scratch / "tests/release/test_distribution_docs.py")],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, check=False)
            expected = EXPECTED_FAILURES.get(name, [])
            assessment = classify_run(result.returncode, result.stdout, expected)
            runtime_control = name == "runtime_error_control"
            accepted = (not assessment["qualified"] and bool(assessment["runtime_errors"])
                        and assessment["test_inventory"] == ["11"]) if runtime_control else assessment["qualified"]
            (args.out_dir / f"{name}.log").write_text(result.stdout)
            reports.append({"case": name, "observed_exit": result.returncode,
                            "joint_replacement": {"old": joint[0], "new": joint[1]} if joint else None,
                            "expected": "harness_refusal_not_semantic_kill" if runtime_control else
                                        "semantic_rejection" if expected else "pass",
                            "assessment": assessment, "result": "PASS" if accepted else "FAIL",
                            "log_sha256": hashlib.sha256(result.stdout.encode()).hexdigest()})
            if not accepted:
                print(result.stdout, file=sys.stderr)
                raise AssertionError(f"Unexpected mutation result: {name}: {assessment}")
    report = {"scope": "Synthetic distribution-document regression and source mutations; not plugin qualification",
              "helper_sha256": hashlib.sha256((ROOT / "scripts/distribution_docs.py").read_bytes()).hexdigest(),
              "semantic_mutant_count": len(MUTATIONS), "runtime_control_is_not_a_semantic_kill": True,
              "cases": reports}
    (args.out_dir / "mutation-results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
