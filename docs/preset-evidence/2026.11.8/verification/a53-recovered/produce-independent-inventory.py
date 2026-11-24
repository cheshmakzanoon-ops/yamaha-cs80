#!/usr/bin/env python3
"""Regenerate a final-source inventory with a separately rebuilt capture tool.

This performs an actual inventory command and records only observed evidence.
It does not claim to perform or qualify the preceding configure/build steps.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import importlib.util
import os
from pathlib import Path
import platform
import re
import subprocess
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("audition_content", HERE / "verify-audition-archive.py")
content = importlib.util.module_from_spec(spec)
spec.loader.exec_module(content)
require = content.require


def clean_source(repo):
    run = lambda *args: subprocess.check_output(["git", "-C", str(repo), *args], text=True).strip()
    source, tree = run("rev-parse", "HEAD"), run("rev-parse", "HEAD^{tree}")
    require(source == content.SOURCE and tree == content.TREE, "Independent checkout is not the frozen source/tree")
    require(run("status", "--porcelain=v1", "--untracked-files=no") == "", "Independent tracked source is dirty")
    return {"source_commit": source, "source_tree": tree}


def execution_context():
    context = {"kind": "local_recovery", "platform": platform.platform(),
               "python_version": platform.python_version(),
               "recorded_at_utc": datetime.now(timezone.utc).isoformat()}
    if os.environ.get("GITHUB_ACTIONS") == "true":
        repository, run, attempt = (os.environ.get(key, "") for key in
                                    ("GITHUB_REPOSITORY", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT"))
        require(repository == content.REPOSITORY and re.fullmatch(r"[1-9][0-9]*", run)
                and re.fullmatch(r"[1-9][0-9]*", attempt), "Incomplete independent Actions context")
        context.update(kind="github_actions", repository=repository, run_id=int(run), run_attempt=int(attempt),
                       workflow_source_commit=os.environ.get("GITHUB_SHA"))
    return context


def observed_tool(command):
    result = subprocess.run([command, "--version"], text=True, capture_output=True, check=False)
    require(result.returncode == 0, "Requested version command failed: " + command)
    return {"command": [command, "--version"], "exit_code": result.returncode,
            "stdout": result.stdout, "stderr": result.stderr,
            "scope": "Observed tool version; this helper does not assert which tool produced the binary."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--capture", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--receipt", required=True, type=Path)
    parser.add_argument("--cmake")
    parser.add_argument("--compiler")
    parser.add_argument("--configure-log", type=Path)
    parser.add_argument("--build-log", type=Path)
    args = parser.parse_args()
    repo, capture, out, receipt = (p.resolve() for p in (args.repo, args.capture, args.out, args.receipt))
    require(not out.exists() and not receipt.exists() and out != receipt,
            "Independent inventory and receipt require fresh distinct paths")
    require(capture.is_file() and os.access(capture, os.X_OK), "Independent capture executable is missing")
    source_before = clean_source(repo)
    digest_before = content.file_sha(capture)
    command = [str(capture), "--library", "all", "--inventory", str(out)]
    out.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(command, cwd=repo, capture_output=True, text=True, check=False)
    require(result.returncode == 0, "Independent inventory command failed with exit " + str(result.returncode))
    source_after = clean_source(repo)
    digest_after = content.file_sha(capture)
    require(source_after == source_before and digest_after == digest_before,
            "Independent source or capture executable changed")
    raw = content.bounded_snapshot(out, content.MAX_MEMBER, "Independent inventory")
    require(content.sha(raw) == content.INVENTORY_SHA256,
            "Regenerated inventory differs from the retained final-source hash")
    inventory = content.parse_json(raw)
    content.validate_inventory(inventory)
    source_files = {}
    for name in [content.CAPTURE_SCRIPT, "src/tools/preset_capture.cpp", "src/core/FactoryPresets.h",
                 "src/core/FactoryPresets.cpp", "src/core/InstrumentBody.h", "src/core/Engine.cpp"]:
        data = subprocess.check_output(["git", "-C", str(repo), "show", content.SOURCE + ":" + name])
        source_files[name] = {"bytes": len(data), "sha256": content.sha(data)}
    logs = {}
    for label, path in [("configure", args.configure_log), ("build", args.build_log)]:
        if path is not None:
            data = content.bounded_snapshot(path, 64 * 1024 * 1024, label + " log")
            logs[label] = {"path": str(path.resolve()), "bytes": len(data), "sha256": content.sha(data),
                           "scope": "Retained caller-supplied log; its original command and exit need separate review."}
    tools = {}
    for name, command_name in [("cmake", args.cmake), ("compiler", args.compiler)]:
        if command_name:
            tools[name] = observed_tool(command_name)
    record = {"schema": 1, "status": "complete", "recovery_edition": content.RECOVERY_EDITION,
              **source_before, "library_revision": content.REVISION,
              "original_build": {"run_id": int(content.RUN), "run_attempt": int(content.ATTEMPT),
                                 "producer_job_id": content.PRODUCER_JOB},
              "execution": execution_context(), "tracked_source_clean_before": True,
              "tracked_source_clean_after": True,
              "capture": {"path": str(capture), "sha256_before": digest_before, "sha256_after": digest_after},
              "inventory": {"path": str(out), "sha256": content.sha(raw), "bytes": len(raw),
                            "command": command, "exit_code": result.returncode,
                            "stdout": result.stdout, "stderr": result.stderr,
                            "programs": content.PROGRAMS, "parameters": content.PARAMETERS},
              "source_files": source_files, "observed_tools": tools, "prior_build_logs": logs,
              "verifier_sha256": content.file_sha(HERE / "verify-audition-archive.py"),
              "producer_sha256": content.file_sha(Path(__file__)),
              "limits": ["This helper ran the inventory command, not the preceding configure/build.",
                         "Local recovery is not a GitHub Actions qualification run.",
                         "The independent binary is not expected to equal the original native capture binary.",
                         "No audition ZIP, native package or public delivery is accepted by this receipt."]}
    content.write_fresh_report(receipt, record)
    print("Independent inventory complete: " + content.sha(raw) + "; execution=" + record["execution"]["kind"])


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, TypeError, OSError, subprocess.CalledProcessError) as error:
        print("Independent inventory failed: " + str(error), file=sys.stderr)
        raise SystemExit(1)
