#!/usr/bin/env python3
"""Collect 3+3 exact-source CPU rounds on ONE trusted exact-main CI job.

Collection does not assert performance PASS. The separate numerical release
gate consumes the full raw evidence. This tool never dispatches a release.
"""
from __future__ import annotations

import argparse
import csv
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys


BENCHMARK_SOURCE = "src/tools/source_factory_benchmark.cpp"
TARGET = "cs80_source_factory_benchmark"
BUILD_FLAGS = ("-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
               "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
               "-DCS80_BUILD_PLUGIN=OFF", "-DCS80_BUILD_TESTS=ON")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def git_sha(value, label):
    require(isinstance(value, str) and len(value) == 40
            and all(c in "0123456789abcdef" for c in value),
            f"{label}: full lowercase Git commit/blob SHA required")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def hash_file(path):
    return digest(Path(path).read_bytes())


def canonical_hash(value):
    return digest(json.dumps(value, sort_keys=True, separators=(",", ":")).encode())


def json_file(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def save_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, sort_keys=True, indent=2) + "\n", encoding="utf-8")


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args], text=True).strip()


def command(args, log, cwd=None):
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8") as stream:
        stream.write("command=" + json.dumps(list(map(str, args))) + "\n")
        stream.flush()
        result = subprocess.run(list(map(str, args)), cwd=cwd, stdout=stream,
                                stderr=subprocess.STDOUT, check=False)
    require(result.returncode == 0, f"command failed {result.returncode}: {log}")


def validate_registration(request, contract, expected_main_sha, expected_src_tree_sha,
                          before_is_ancestor):
    require(request["schema_version"] == 1, "unexpected producer request schema")
    for key in ("before_sha", "reviewed_sha", "reviewed_source_tree_sha",
                "before_benchmark_blob", "after_benchmark_blob"):
        git_sha(request[key], key)
    require(request["before_sha"] != request["reviewed_sha"],
            "must compare a distinct before source against reviewed main")
    require(request["before_sha"] == contract["cpu"]["registered_before_source_sha"],
            "the preregistered before source cannot be substituted")
    require(request["reviewed_sha"] == expected_main_sha,
            "requested reviewed source is not exact main")
    require(request["reviewed_source_tree_sha"] == expected_src_tree_sha,
            "requested src tree is not exact main")
    require(before_is_ancestor is True, "before source must be an ancestor of exact main")
    require(request["before_benchmark_blob"] == request["after_benchmark_blob"],
            "benchmark harness changed between before and after")
    require(request["before_benchmark_blob"] == contract["cpu"]["registered_benchmark_blob_sha"],
            "the preregistered benchmark harness cannot be substituted")
    spec = contract["cpu"]
    expected = {"rate": spec["rate_hz"], "block": spec["block_frames"],
                "callbacks": spec["callbacks_per_held_phase_min"],
                "rounds_per_source": spec["required_before_rounds"], "suite": spec["suite"]}
    for key, value in expected.items():
        require(type(request[key]) is type(value) and request[key] == value,
                f"registered CPU request cannot change {key}={value!r}")
    require(spec["required_before_rounds"] == spec["required_after_rounds"] == 3,
            "expected equal three-round baseline/after source qualification")


def require_ci_context(env, expected_main_sha):
    require(env.get("GITHUB_ACTIONS") == "true", "trusted GitHub Actions environment required")
    require(env.get("GITHUB_REF") == "refs/heads/main", "producer must run on exact main")
    require(env.get("GITHUB_SHA") == expected_main_sha, "CI event SHA is not reviewed main")
    for key in ("GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT", "GITHUB_JOB", "RUNNER_NAME",
                "RUNNER_OS", "RUNNER_ARCH"):
        require(isinstance(env.get(key), str) and bool(env[key]),
                f"trusted runner context missing {key}")
    return {"runner_job_id": "/".join(env[k] for k in (
                "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT", "GITHUB_JOB")),
            "host_id": "/".join(env[k] for k in ("RUNNER_NAME", "RUNNER_OS", "RUNNER_ARCH"))}


def serial_schedule():
    # Build both immutable binaries first; alternate pairs after warm-up.
    for round_number in (1, 2, 3):
        for stage in ("before", "after"):
            yield stage, round_number


def verify_benchmark_source(repo, request):
    for label, key in (("before_sha", "before_benchmark_blob"),
                       ("reviewed_sha", "after_benchmark_blob")):
        sha = request[label]
        require(git(repo, "rev-parse", sha + "^{commit}") == sha,
                f"{label}: full source SHA fails Git resolution")
        require(git(repo, "rev-parse", sha + ":" + BENCHMARK_SOURCE) == request[key],
                f"{label}: registered benchmark source blob changed")


def verify_output(path, source_sha, spec):
    metadata = json_file(path / "metadata.json")
    completed = json_file(path / "completed.json")
    require(metadata["source_commit_caller_verified"] == source_sha,
            "benchmark reports wrong source SHA")
    require(metadata["suite"] == spec["suite"] and metadata["quick_subset"] is False,
            "benchmark ran shortened or unregistered suite")
    require(metadata["callbacks_per_held_phase"] == spec["callbacks_per_held_phase_min"],
            "benchmark callbacks changed")
    require(completed["completed"] is True
            and completed["cases"] == spec["expected_cases_per_round"]
            and completed["rows"] == spec["expected_rows_per_round"],
            "benchmark completion marker mismatch")
    with (path / "results.csv").open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    require(len(rows) == spec["expected_rows_per_round"], "benchmark raw CSV incomplete")
    require(all(int(row["nonfinite_count"]) == 0 for row in rows),
            "benchmark rendered nonfinite samples")


def build_receipt(ci_identity, compiler_id, flags_sha256, source_sha256,
                  environment_sha256, capture_id, source_sha, binary_sha):
    return {**ci_identity, "compiler_id": compiler_id,
            "flags_sha256": flags_sha256,
            "benchmark_source_sha256": source_sha256,
            "runner_environment_sha256": environment_sha256,
            "capture_id": capture_id, "source_sha": source_sha,
            "benchmark_binary_sha256": binary_sha}


def collect(args):
    repo = args.repo.resolve()
    args.work = args.work.resolve()
    args.out = args.out.resolve()
    request = json_file(args.request)
    contract = json_file(args.contract)
    main = git(repo, "rev-parse", "HEAD")
    src_tree = git(repo, "rev-parse", "HEAD:src")
    before_ancestor = subprocess.run(["git", "-C", str(repo), "merge-base", "--is-ancestor",
                                     request["before_sha"], main], check=False).returncode == 0
    validate_registration(request, contract, main, src_tree, before_ancestor)
    ci_identity = require_ci_context(os.environ, main)
    verify_benchmark_source(repo, request)
    require(not args.work.exists() and not args.out.exists(),
            "new work/evidence paths required; never overwrite prior evidence")
    args.work.mkdir(parents=True)
    args.out.mkdir(parents=True)

    compiler_id = subprocess.check_output(["g++", "--version"], text=True).splitlines()[0]
    runner_environment = {"ci": ci_identity, "platform": platform.platform(),
                          "machine": platform.machine(), "logical_cpus": os.cpu_count()}
    flags_sha = canonical_hash({"cmake": list(BUILD_FLAGS), "compiler": compiler_id,
                                "CC": os.environ.get("CC", ""), "CXX": os.environ.get("CXX", ""),
                                "CFLAGS": os.environ.get("CFLAGS", ""),
                                "CXXFLAGS": os.environ.get("CXXFLAGS", "")})
    env_sha = canonical_hash(runner_environment)
    compiled = {}
    manifest = {"registration": request, "ci": ci_identity, "runner_environment": runner_environment,
                "flags_sha256": flags_sha, "compiler_id": compiler_id,
                "timing_order": "round-major / before,after alternating, all builds first",
                "qualified_numerical_pass": False, "built": {}, "rounds": []}
    save_json(args.out / "producer-provenance.json", manifest)
    for stage, sha in (("before", request["before_sha"]),
                       ("after", request["reviewed_sha"])):
        source = args.work / stage / "source"
        build = args.work / stage / "build"
        evidence = args.out / stage
        source.parent.mkdir(parents=True)
        command(["git", "-C", str(repo), "worktree", "add", "--detach", source, sha],
                evidence / "checkout.log")
        require(git(source, "rev-parse", "HEAD") == sha, "source changed after checkout")
        command(["cmake", "-S", source, "-B", build, *BUILD_FLAGS], evidence / "configure.log")
        command(["cmake", "--build", build, "--target", TARGET, "--parallel", "2"],
                evidence / "build.log")
        binary = build / TARGET
        binary_sha = hash_file(binary)
        source_sha256 = hash_file(source / BENCHMARK_SOURCE)
        require(source_sha256 == hash_file(repo / BENCHMARK_SOURCE),
                "benchmark tool source differs from reviewed main")
        compiled[stage] = {"source": source, "source_sha": sha, "binary": binary,
                           "binary_sha": binary_sha, "benchmark_sha": source_sha256}
        manifest["built"][stage] = {"source_sha": sha, "binary_sha256": binary_sha,
                                     "benchmark_source_sha256": source_sha256}
        save_json(args.out / "producer-provenance.json", manifest)
    require(compiled["before"]["benchmark_sha"] == compiled["after"]["benchmark_sha"],
            "before/after source benchmark tool code is not identical")

    for stage, index in serial_schedule():
        build = compiled[stage]
        require(hash_file(build["binary"]) == build["binary_sha"],
                "timed binary changed after compilation")
        require(git(build["source"], "rev-parse", "HEAD") == build["source_sha"]
                and not git(build["source"], "status", "--porcelain"),
                "timed source changed after compilation")
        directory = args.out / stage / f"round-{index}"
        command([build["binary"], "--out", directory, "--source-commit", build["source_sha"],
                 "--suite", "baseline", "--rate", "48000", "--block", "256",
                 "--callbacks", "1024"], args.out / stage / f"round-{index}.log")
        verify_output(directory, build["source_sha"], contract["cpu"])
        capture_id = f"{ci_identity['runner_job_id']}/{stage}/{index}/{build['source_sha']}"
        receipt = build_receipt(ci_identity, compiler_id, flags_sha, build["benchmark_sha"],
                                env_sha, capture_id, build["source_sha"], build["binary_sha"])
        save_json(directory / "qualification-receipt.json", receipt)
        manifest["rounds"].append({"stage": stage, "round": index,
                                   "source_sha": build["source_sha"], "capture_id": capture_id,
                                   "raw_csv_sha256": hash_file(directory / "results.csv")})
        save_json(args.out / "producer-provenance.json", manifest)
    manifest["complete_3_plus_3"] = True
    manifest["completed_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    save_json(args.out / "producer-provenance.json", manifest)
    save_json(args.out / "sha256.json", {str(path.relative_to(args.out)): hash_file(path)
              for path in sorted(args.out.rglob("*")) if path.is_file()
              and path.name != "sha256.json"})


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--request", type=Path, required=True)
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--contract", type=Path,
                        default=Path(__file__).resolve().parents[1] / ".github/release-numerical-contract.json")
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        collect(args)
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        print(json.dumps({"producer_complete": False, "error": str(error)}), file=sys.stderr)
        return 1
    print(json.dumps({"producer_complete": True, "note": "Evidence only; numerical pass not asserted"}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
