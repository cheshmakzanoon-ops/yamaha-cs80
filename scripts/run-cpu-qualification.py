#!/usr/bin/env python3
"""Compile immutable checkpoints first; retain three serial timing rounds each.

This is an evidence collection tool. The unchanged benchmark and existing
reporter own fixture semantics; no performance threshold is imposed here.
"""

import argparse
import csv
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time


BENCHMARK_SOURCE = "src/tools/source_factory_benchmark.cpp"
TARGET = "cs80_source_factory_benchmark"
FIXED_REQUEST = {"schema_version": 1, "rate": 48000, "block": 256,
                 "callbacks": 1024, "rounds": 3, "suite": "baseline"}
# Published historical checkpoint identities. A future request may append
# additional full-SHA revisions; removal, reordering and rewriting are errors.
CHECKPOINT_KEYS = ("label", "source_commit", "benchmark_blob")
IMMUTABLE_PREFIX = (
    ("main-before", "db24a47170610f81236e8829ef5536af69dfc94d", "b73298678acb6684654ed1b3771fb442352853c0"),
    ("inherited-before", "2ee33dcc8113aaca4141dc7d77fb374384d8560c", "92ea7e0b1801bf98fccaed21c13e676890039c94"),
    ("lazy-filter-histories", "476c2f1fed4dcec18c5a42bc8d29ae542dab02b9", "92ea7e0b1801bf98fccaed21c13e676890039c94"),
    ("idle-exact", "f79b6f7fe6831a9a518bc2a94435a6214e46290b", "92ea7e0b1801bf98fccaed21c13e676890039c94"),
    ("stationary-controls", "8dac795558eab6bae99a0812cfa1fc6632cfc024", "92ea7e0b1801bf98fccaed21c13e676890039c94"),
    ("unused-lfo", "193f1a31a636b78f7c50f642ad1794fc6023017b", "92ea7e0b1801bf98fccaed21c13e676890039c94"),
    ("rank-idle-1500", "18400c29a54ac2204564b355c0e52ffd3b0dcb8e", "92ea7e0b1801bf98fccaed21c13e676890039c94"),
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n")


def read_command(args, cwd=None):
    return subprocess.run(args, cwd=cwd, text=True, check=True,
                          stdout=subprocess.PIPE).stdout.strip()


def run(args, log, cwd=None):
    log.parent.mkdir(parents=True, exist_ok=True)
    print(json.dumps({"command": list(map(str, args)), "log": str(log)}), flush=True)
    with log.open("w") as stream:
        stream.write("command=" + json.dumps(list(map(str, args))) + "\n")
        stream.flush()
        result = subprocess.run(args, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"command exited {result.returncode}; see {log}")


def host_snapshot():
    files = {}
    for name in ("/proc/loadavg", "/proc/stat", "/proc/meminfo", "/proc/self/status",
                 "/sys/fs/cgroup/cpu.stat", "/sys/fs/cgroup/cpu.max",
                 "/sys/fs/cgroup/cpuset.cpus.effective"):
        path = Path(name)
        if path.is_file():
            files[name] = path.read_text()
    for path in sorted(Path("/sys/devices/system/cpu").glob("cpu*/cpufreq/scaling_*")):
        if path.name in ("scaling_cur_freq", "scaling_governor", "scaling_max_freq", "scaling_min_freq"):
            try:
                files[str(path)] = path.read_text().strip()
            except OSError as error:
                files[str(path)] = str(error)
    return {
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "monotonic_ns": time.monotonic_ns(), "platform": platform.platform(),
        "logical_cpus": os.cpu_count(), "load_average": list(os.getloadavg()),
        "affinity": sorted(os.sched_getaffinity(0)), "files": files,
        # Executable names only: never expose command arguments or the environment.
        "processes": read_command(["ps", "-eo", "pid,ppid,comm,pcpu,pmem,etime,stat", "--sort=-pcpu"]),
    }


def validate_request(request):
    for key, expected in FIXED_REQUEST.items():
        if type(request.get(key)) is not type(expected) or request[key] != expected:
            raise ValueError(f"request must retain {key}={expected!r}")
    checkpoints = request.get("checkpoints", [])
    if not re.fullmatch(r"[0-9a-f]{64}", request.get("reporter_sha256", "")):
        raise ValueError("reporter SHA-256 must be a full immutable lowercase digest")
    if not 1 <= len(checkpoints) <= 16:
        raise ValueError("request must list 1..16 immutable checkpoints")
    labels, commits = set(), set()
    for checkpoint in checkpoints:
        label, commit, blob = (checkpoint.get(key, "") for key in
                               ("label", "source_commit", "benchmark_blob"))
        if not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,63}", label):
            raise ValueError("checkpoint labels must be short lowercase filename components")
        if not re.fullmatch(r"[0-9a-f]{40}", commit) or not re.fullmatch(r"[0-9a-f]{40}", blob):
            raise ValueError("commits and expected benchmark blobs must be full immutable SHA-1 values")
        if label in labels or commit in commits:
            raise ValueError("duplicate checkpoint label or commit")
        labels.add(label)
        commits.add(commit)
    if len(checkpoints) < len(IMMUTABLE_PREFIX):
        raise ValueError("the seven preregistered checkpoint identities cannot be removed")
    for checkpoint, expected in zip(checkpoints, IMMUTABLE_PREFIX):
        if tuple(checkpoint.get(key) for key in CHECKPOINT_KEYS) != expected:
            raise ValueError("historical checkpoint identity or order changed")


def timing_schedule(built):
    """Exactly three serial, round-major sweeps of the already-built binaries."""
    for round_number in range(1, FIXED_REQUEST["rounds"] + 1):
        for item in built:
            yield round_number, item


def verify_reporter(path, requested_sha256):
    if digest(path) != requested_sha256:
        raise ValueError("existing reporter differs from the request's frozen reporting convention")


def verify_checkpoint(repo, checkpoint):
    commit = checkpoint["source_commit"]
    if read_command(["git", "rev-parse", commit + "^{commit}"], repo) != commit:
        raise ValueError(f"{checkpoint['label']}: full source commit does not resolve exactly")
    if read_command(["git", "rev-parse", commit + ":" + BENCHMARK_SOURCE], repo) != checkpoint["benchmark_blob"]:
        raise ValueError(f"{checkpoint['label']}: immutable benchmark source blob differs")


def verify_binary_and_source(binary, expected_sha256, source, expected_commit):
    if digest(binary) != expected_sha256:
        raise ValueError("built benchmark binary changed after compilation")
    if read_command(["git", "rev-parse", "HEAD"], source) != expected_commit:
        raise ValueError("source checkout HEAD changed after compilation")
    if read_command(["git", "status", "--porcelain"], source):
        raise ValueError("immutable source checkout changed after compilation")


def validate_round(directory, commit):
    metadata = json.loads((directory / "metadata.json").read_text())
    if (metadata["source_commit_caller_verified"] != commit or metadata["suite"] != "baseline"
            or metadata["quick_subset"] or metadata["callbacks_per_held_phase"] != 1024
            or metadata["hardware_equivalence_or_realtime_qualification"] is not False):
        raise ValueError(f"{directory}: benchmark did not retain the requested invocation")
    complete = json.loads((directory / "completed.json").read_text())
    if not complete.get("completed") or complete["cases"] != 27 or complete["rows"] != 51:
        raise ValueError(f"{directory}: incomplete unchanged 27-case / 51-row sweep")
    with (directory / "results.csv").open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) != 51 or any(int(row["rate"]) != 48000 or int(row["block"]) != 256 for row in rows):
        raise ValueError(f"{directory}: unexpected fixture dimensions")
    keys = {(row["rate"], row["block"], row["mode"], row["scenario"],
             row["initial_selector"], row["phase"]) for row in rows}
    if len(keys) != 51:
        raise ValueError(f"{directory}: repeated fixture instead of original 51 rows")
    for row in rows:
        if any(not math.isfinite(float(row[key])) for key in ("median_rtf", "p99_rtf", "peak", "rms")):
            raise ValueError(f"{directory}: nonfinite timing or signal summary")
        if int(row["nonfinite_count"]) != 0:
            raise ValueError(f"{directory}: nonfinite output samples")


def collect(args):
    repo = Path(read_command(["git", "rev-parse", "--show-toplevel"])).resolve()
    request_path = args.request.resolve()
    request = json.loads(request_path.read_text())
    validate_request(request)
    reporter = repo / "scripts/summarize-cpu-benchmarks.py"
    verify_reporter(reporter, request["reporter_sha256"])
    if args.out.exists() or args.work.exists():
        raise ValueError("work and output directories must be new; evidence is never overwritten")
    args.out.mkdir(parents=True)
    args.work.mkdir(parents=True)
    shutil.copyfile(request_path, args.out / "request.json")
    shutil.copyfile(reporter, args.out / reporter.name)
    shutil.copyfile(__file__, args.out / Path(__file__).name)
    save(args.out / "host-before-builds.json", host_snapshot())
    provenance = {
        "driver_commit": read_command(["git", "rev-parse", "HEAD"], repo),
        "request_sha256": digest(request_path), "runner_script_sha256": digest(Path(__file__)),
        "reporter_sha256": digest(reporter),
        "runner": {key: os.environ.get(key) for key in (
            "RUNNER_OS", "RUNNER_ARCH", "RUNNER_NAME", "ImageOS", "ImageVersion",
            "GITHUB_REPOSITORY", "GITHUB_SHA", "GITHUB_REF", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT")},
        "compiler": read_command(["g++", "--version"]), "cmake": read_command(["cmake", "--version"]),
        "ninja": read_command(["ninja", "--version"]), "python": sys.version,
        "cpu": read_command(["lscpu", "--json"]),
        "build_parallelism": 2, "timing_order": "round-major, manifest checkpoint order, all serial",
        "performance_pass_claim": False,
        "qualification_scope": "Single workflow job on one GitHub VM; hypervisor interference is uncontrolled; no hardware or DAW qualification",
    }
    save(args.out / "provenance.json", provenance)
    built = []
    # Every configuration/build finishes before the first timed subprocess.
    for checkpoint in request["checkpoints"]:
        label, commit = checkpoint["label"], checkpoint["source_commit"]
        source, build, evidence = args.work / label / "source", args.work / label / "build", args.out / label
        evidence.mkdir()
        source.parent.mkdir()
        verify_checkpoint(repo, checkpoint)
        run(["git", "worktree", "add", "--detach", str(source), commit], evidence / "checkout.log", repo)
        run(["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
             "-DCS80_BUILD_PLUGIN=OFF", "-DCS80_BUILD_TESTS=ON"],
            evidence / "configure.log")
        run(["cmake", "--build", str(build), "--target", TARGET, "--parallel", "2"], evidence / "build.log")
        binary = build / TARGET
        initial_binary_hash = digest(binary)
        verify_binary_and_source(binary, initial_binary_hash, source, commit)
        for name in ("CMakeCache.txt", "compile_commands.json", "build.ninja"):
            shutil.copyfile(build / name, evidence / name)
        shutil.copyfile(source / BENCHMARK_SOURCE, evidence / "benchmark-source.cpp")
        source_paths = read_command([
            "git", "ls-files", "src/core", "src/experimental", "src/reference",
            BENCHMARK_SOURCE, "CMakeLists.txt"], source).splitlines()
        record = {**checkpoint, "source_tree": read_command(["git", "rev-parse", "HEAD^{tree}"], source),
                  "source_sha256": {path: digest(source / path) for path in source_paths},
            "executable_sha256": initial_binary_hash, "rate": 48000, "block": 256,
                  "callbacks": 1024, "rounds": 3, "compile_completed_utc": host_snapshot()["utc"]}
        save(evidence / "manifest.json", record)
        built.append((checkpoint, source, binary, record["executable_sha256"], evidence))
    save(args.out / "host-before-timings.json", host_snapshot())
    schedule = []
    for round_number, (checkpoint, source, binary, binary_hash, evidence) in timing_schedule(built):
        verify_binary_and_source(binary, binary_hash, source, checkpoint["source_commit"])
        directory = evidence / f"round-{round_number}"
        observation = {"label": checkpoint["label"], "round": round_number,
                       "before": host_snapshot()}
        save(evidence / f"round-{round_number}-host-before.json", observation)
        run([str(binary), "--out", str(directory), "--source-commit", checkpoint["source_commit"],
             "--suite", "baseline", "--rate", "48000", "--block", "256", "--callbacks", "1024"],
            evidence / f"round-{round_number}.log")
        observation["after"] = host_snapshot()
        save(evidence / f"round-{round_number}-host.json", observation)
        validate_round(directory, checkpoint["source_commit"])
        schedule.append({"label": checkpoint["label"], "round": round_number,
                         "start": observation["before"]["utc"], "end": observation["after"]["utc"]})
        save(args.out / "completed-rounds.json", schedule)
    # Summaries run only after every timing has finished, with the existing tool.
    summaries = []
    for checkpoint, _, _, _, evidence in built:
        run([sys.executable, str(reporter), str(evidence)], evidence / "summary.log")
        summaries.append(json.loads((evidence / "summary.json").read_text()))
    table = ["# CPU checkpoint observations", "", provenance["qualification_scope"], "",
             "Reporting uses the minimum callback median per fixture across three rounds; p99 and misses come from that same round. Every raw round is retained. No performance threshold is asserted.", "",
             "| Checkpoint | Full source commit | Held median RTF range | Quiet idle RTF | Held selected-round deadline misses |",
             "| --- | --- | ---: | ---: | ---: |"]
    for checkpoint, summary in zip(request["checkpoints"], summaries):
        low, high = summary["held_median_rtf_range"]
        table.append(f"| {checkpoint['label']} | {checkpoint['source_commit']} | {low:.6f}–{high:.6f} | {summary['idle_median_rtf']:.6f} | {summary['held_selected_round_budget_exceedances']} |")
    table.extend(["", "Main's original benchmark blob b7329867 returns its existing PANEL frame;",
                  "subsequent checkpoints use benchmark blob 92ea7e0b, which constructs the electrical programmer frame.",
                  "Those source differences remain part of the published checkpoints and are not normalized away."])
    report = "\n".join(table) + "\n"
    (args.out / "SUMMARY.md").write_text(report)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as stream:
            stream.write(report)
    save(args.out / "host-after-timings.json", host_snapshot())
    save(args.out / "completed.json", {"completed": True, "checkpoints": len(built),
                                      "rounds_each": 3, "performance_pass_claim": False})
    save(args.out / "sha256.json", {str(path.relative_to(args.out)): digest(path)
         for path in sorted(args.out.rglob("*")) if path.is_file()})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--request", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    args.work, args.out = args.work.resolve(), args.out.resolve()
    preexisting_output = args.out.exists()
    try:
        collect(args)
    except Exception as error:
        if not preexisting_output and args.out.is_dir():
            save(args.out / "failure.json", {"completed": False, "error": str(error)})
        raise


if __name__ == "__main__":
    main()

