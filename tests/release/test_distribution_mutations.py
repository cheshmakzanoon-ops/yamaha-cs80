#!/usr/bin/env python3
"""Reject real distribution-helper defects in isolated synthetic fixture copies."""
from __future__ import annotations
import argparse
import hashlib
import json
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
    ("ignore_dirty_checkout", 'check=False).returncode == 0,', 'check=False).returncode in (0, 1),'),
    ("allow_untracked_notice", '        git("ls-files", "--error-unmatch", "--", path)', '        pass  # mutation: trust an untracked replacement'),
    ("wrong_manifest_digest", '"sha256": hashlib.sha256(data).hexdigest()', '"sha256": "0" * 64'),
]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path)
    args = parser.parse_args()
    reports = []
    with tempfile.TemporaryDirectory(prefix="cs80-distribution-mutations-") as temp:
        scratch = Path(temp)
        for relative in FILES:
            target = scratch / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, target)
        helper = scratch / "scripts/distribution_docs.py"
        original = helper.read_text()
        for name, before, after in [("positive", "", ""), *MUTATIONS, ("restored", "", "")]:
            if before and original.count(before) != 1:
                raise AssertionError(f"Mutation anchor is not unique: {name}")
            helper.write_text(original.replace(before, after) if before else original)
            # Do not let same-size same-second .pyc metadata retain a previous mutant.
            shutil.rmtree(scratch / "scripts/__pycache__", ignore_errors=True)
            result = subprocess.run([sys.executable, str(scratch / "tests/release/test_distribution_docs.py")],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, check=False)
            expected_pass = name in ("positive", "restored")
            accepted = (result.returncode == 0 and "\nOK\n" in result.stdout) if expected_pass else (
                result.returncode != 0 and ("FAIL:" in result.stdout or "ERROR:" in result.stdout)
                and "Ran 11 tests" in result.stdout)
            if args.out_dir:
                args.out_dir.mkdir(parents=True, exist_ok=True)
                (args.out_dir / f"{name}.log").write_text(result.stdout)
            reports.append({"case": name, "observed_exit": result.returncode,
                            "expected": "pass" if expected_pass else "rejection", "result": "PASS" if accepted else "FAIL",
                            "log_sha256": hashlib.sha256(result.stdout.encode()).hexdigest()})
            if not accepted:
                print(result.stdout, file=sys.stderr)
                raise AssertionError(f"Unexpected mutation result: {name}")
    report = {"scope": "Synthetic distribution-document regression and deliberate source mutations; not plugin qualification",
              "helper_sha256": hashlib.sha256((ROOT / "scripts/distribution_docs.py").read_bytes()).hexdigest(),
              "cases": reports}
    if args.out_dir:
        (args.out_dir / "mutation-results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
