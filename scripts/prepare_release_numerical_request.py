#!/usr/bin/env python3
"""Create exact-main source IDs for a SAME-JOB raw audit and CPU producer.

This is a read-only provenance preflight except for fresh JSON files under
--out. It never accepts a caller-supplied qualification receipt or PASS flag.
The original full-audit preflight and CPU producer independently recheck pins.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

REPO_SLUG = "cheshmakzanoon-ops/yamaha-cs80"
BENCHMARK_SOURCE = "src/tools/source_factory_benchmark.cpp"
AUDIT_REGISTRATION_SHA = "00da4498792523cf8d97fd40c17e12f215e190b8"


def require(condition, explanation):
    if not condition:
        raise ValueError(explanation)


def commit_sha(sha, label):
    require(isinstance(sha, str) and len(sha) == 40
            and all(character in "0123456789abcdef" for character in sha),
            f"{label}: complete lowercase Git SHA required")


def validate_ci_context(env, head, current_main):
    require(env.get("GITHUB_ACTIONS") == "true", "GitHub Actions required")
    require(env.get("GITHUB_EVENT_NAME") == "push", "only an exact-main push is authorized")
    require(env.get("GITHUB_REPOSITORY") == REPO_SLUG, "wrong GitHub repository")
    require(env.get("GITHUB_REF") == "refs/heads/main", "only refs/heads/main is authorized")
    event_sha = env.get("GITHUB_SHA")
    commit_sha(event_sha, "GITHUB_SHA")
    require(head == event_sha, "checked-out source is not exact event main")
    require(current_main == event_sha, "current main has changed from this push")
    for field in ("GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT", "GITHUB_JOB"):
        require(bool(env.get(field)), f"missing trusted CI field: {field}")


def cpu_request(contract, main, source_tree):
    spec = contract["cpu"]
    before = spec["registered_before_source_sha"]
    bench = spec["registered_benchmark_blob_sha"]
    for label, value in (("before", before), ("main", main), ("tree", source_tree),
                         ("benchmark blob", bench)):
        commit_sha(value, label)
    require(main != before, "before source and reviewed main cannot coincide")
    require(spec["required_before_rounds"] == spec["required_after_rounds"] == 3,
            "registered three-plus-three CPU rounds changed")
    return {"schema_version": 1, "before_sha": before, "reviewed_sha": main,
            "reviewed_source_tree_sha": source_tree, "before_benchmark_blob": bench,
            "after_benchmark_blob": bench, "rate": spec["rate_hz"],
            "block": spec["block_frames"],
            "callbacks": spec["callbacks_per_held_phase_min"],
            "rounds_per_source": spec["required_before_rounds"], "suite": spec["suite"]}


def source_identity(contract, audit_registration, main, source_tree, registration_sha):
    commit_sha(main, "main")
    commit_sha(source_tree, "source tree")
    commit_sha(registration_sha, "audit registration")
    require(registration_sha == AUDIT_REGISTRATION_SHA,
            "frozen audit registration changed")
    require(audit_registration["expected_src_tree"] == source_tree,
            "current main src tree differs from preregistered original audit")
    require(audit_registration["command"] == contract["audit"]["registered_command"],
            "original audit command changed")
    audio = audit_registration["production_audio_commit"]
    commit_sha(audio, "audio source anchor")
    return {"reviewed_sha": main, "source_tree_sha": source_tree,
            "audio_sha": audio, "registration_sha": registration_sha,
            "before_sha": contract["cpu"]["registered_before_source_sha"]}


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args], text=True).strip()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True,
                        help="A new directory under this CI job's RUNNER_TEMP")
    args = parser.parse_args(argv)
    try:
        repo = args.repo.resolve()
        head = git(repo, "rev-parse", "HEAD")
        current = git(repo, "rev-parse", "refs/remotes/origin/main")
        validate_ci_context(os.environ, head, current)
        require(not git(repo, "status", "--porcelain", "--untracked-files=no"),
                "tracked source changed during CI preflight")
        tree = git(repo, "rev-parse", "HEAD:src")
        contract = json.loads((repo / ".github/release-numerical-contract.json").read_text())
        audit = json.loads((repo / ".github/full-engine-audit-request.json").read_text())
        cpu = cpu_request(contract, head, tree)
        identity = source_identity(contract, audit, head, tree, AUDIT_REGISTRATION_SHA)
        require(subprocess.run(["git", "-C", str(repo), "merge-base", "--is-ancestor",
                                cpu["before_sha"], head], check=False).returncode == 0,
                "registered CPU before source not an ancestor of reviewed main")
        for sha in (cpu["before_sha"], cpu["reviewed_sha"]):
            require(git(repo, "rev-parse", sha + ":" + BENCHMARK_SOURCE)
                    == cpu["before_benchmark_blob"],
                    "registered benchmark source blob changed")
        require(subprocess.run(["git", "-C", str(repo), "merge-base", "--is-ancestor",
                                identity["audio_sha"], head], check=False).returncode == 0,
                "original audio anchor is not an ancestor of current main")
        require(subprocess.run(["git", "-C", str(repo), "merge-base", "--is-ancestor",
                                identity["registration_sha"], head], check=False).returncode == 0,
                "original audit registration is not an ancestor of current main")
        require(not args.out.exists(), "refusing to overwrite any prior evidence/request")
        args.out.mkdir(parents=True)
        (args.out / "cpu-request.json").write_text(json.dumps(cpu, sort_keys=True, indent=2) + "\n")
        (args.out / "source-identity.json").write_text(json.dumps(identity, sort_keys=True, indent=2) + "\n")
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as exc:
        print(json.dumps({"source_locked": False, "error": str(exc)}), file=sys.stderr)
        return 1
    print(json.dumps({"source_locked": True, "main": head, "src_tree": tree,
                      "note": "request only, no numerical qualification"}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
