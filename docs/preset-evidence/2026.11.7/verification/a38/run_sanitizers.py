#!/usr/bin/env python3
"""Compile unchanged, focused body regressions as individual sanitized TUs.

Run from the repository root. Binaries stay in the isolated --scratch directory.
This deliberately omits LeakSanitizer: the supervised host cannot run LSan.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import time


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--scratch", default="/tmp/cs80-a38-1cee5a")
    parser.add_argument("--compiler", default="g++")
    parser.add_argument("--lifecycle-only", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[5]
    evidence = Path(__file__).resolve().parent
    scratch = Path(args.scratch).resolve()
    scratch.mkdir(parents=True, exist_ok=True)
    tests = [
        "tests/PianoPartialDecayTests.cpp",
        "tests/BodyNoiseRateTests.cpp",
        "tests/StringBodyFormantTests.cpp",
        "tests/LowChoirSpectrumTests.cpp",
    ]
    if args.lifecycle_only:
        tests = ["docs/preset-evidence/2026.11.7/verification/a38/body_lifecycle.cpp"]
    receipt_path = evidence / ("lifecycle-receipt.json" if args.lifecycle_only else "sanitizer-receipt.json")
    flags = ["-std=c++20", "-O1", "-g1", "-Wall", "-Wextra", "-Wpedantic",
             "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
             "-fno-omit-frame-pointer", "-Isrc"]
    environment = {"ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
                   "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}
    receipt = {
        "purpose": "Ordinary valid-input regression execution; no fault probes",
        "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "source_tree": subprocess.check_output(["git", "rev-parse", "HEAD^{tree}"], cwd=root, text=True).strip(),
        "platform": platform.platform(),
        "compiler": subprocess.check_output([args.compiler, "--version"], text=True).splitlines()[0],
        "compile_flags": flags,
        "run_environment": environment,
        "leak_checking": False,
        "source_hashes": {str(p.relative_to(root)): sha256(p) for p in
                          sorted((root / "src/core").glob("InstrumentBody*.h"))},
        "runs": [],
    }
    receipt_path.write_text(json.dumps(receipt, indent=2) + "\n")
    for source in tests:
        stem = Path(source).stem
        binary = scratch / (stem + "-asan-ubsan")
        compile_command = [args.compiler, *flags, source, "-o", str(binary)]
        run = {"source": source, "source_sha256": sha256(root / source),
               "compile_command": compile_command,
               "compile_log": stem + "-compile.log",
               "execution_log": stem + "-sanitizer.log"}
        print("Compiling " + source, flush=True)
        start = time.monotonic()
        with (evidence / run["compile_log"]).open("w") as log:
            compiled = subprocess.run(compile_command, cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=180)
        run.update(compile_exit=compiled.returncode, compile_seconds=time.monotonic() - start)
        if compiled.returncode == 0:
            run["binary_sha256"] = sha256(binary)
            run["binary_bytes"] = binary.stat().st_size
            print("Running " + stem, flush=True)
            start = time.monotonic()
            with (evidence / run["execution_log"]).open("w") as log:
                executed = subprocess.run([str(binary)], cwd=root,
                                          env={**os.environ, **environment},
                                          stdout=log, stderr=subprocess.STDOUT, timeout=600)
            run.update(run_exit=executed.returncode, run_seconds=time.monotonic() - start)
            print(stem + ": exit " + str(executed.returncode), flush=True)
        receipt["runs"].append(run)
        receipt_path.write_text(json.dumps(receipt, indent=2) + "\n")
    return int(any(r.get("run_exit", 1) != 0 for r in receipt["runs"]))


if __name__ == "__main__":
    raise SystemExit(main())
