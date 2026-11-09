#!/usr/bin/env python3
"""Check exact adaptive-cache regressions against isolated incorrect variants.

Usage: python3 scripts/test-ensemble-cache-mutations.py NEW_OUTPUT_DIRECTORY
Uses a GNU-compatible C++20 compiler (CXX, or c++). Does not edit runtime source.
"""
import os
from pathlib import Path
import shlex
import subprocess
import sys

if len(sys.argv) != 2:
    raise SystemExit("Usage: test-ensemble-cache-mutations.py NEW_OUTPUT_DIRECTORY")

root = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=False)
compiler = shlex.split(os.environ.get("CXX", "c++"))
header = (root / "src/core/FxModels.h").read_text()


def change_once(source, before, after):
    if source.count(before) != 1:
        raise RuntimeError(f"Expected one mutation marker: {before!r}")
    return source.replace(before, after, 1)


variants = {
    "baseline": header,
    "missing-reset-key": change_once(header, "        lastRate = -1.0f;\n", ""),
    "stale-rate": change_once(header, "            if (rate != lastRate)\n",
                              "            if (lastRate == -1.0f)\n"),
    "held-fallback": change_once(header,
        "            slowPhase += kSlowHz * speed / sr;\n            fastPhase += kFastHz * speed / sr;",
        "            slowPhase += slowIncrement;\n            fastPhase += fastIncrement;"),
}

for name, source in variants.items():
    directory = out / name
    (directory / "core").mkdir(parents=True)
    (directory / "core/FxModels.h").write_text(source)
    executable = directory / "test"
    command = compiler + ["-std=c++20", "-O2", "-I" + str(directory),
        "-I" + str(root / "src"), "-I" + str(root / "src/core"),
        str(root / "tests/EnsembleRateCacheTests.cpp"), "-o", str(executable)]
    built = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, timeout=120)
    (directory / "build.log").write_text(built.stdout)
    if built.returncode:
        raise RuntimeError(f"{name}: build failed, not a detected mutation")
    result = subprocess.run([str(executable)], text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=120)
    (directory / "run.log").write_text(result.stdout)
    expected = 0 if name == "baseline" else 1
    marker = "checks, 0 failures," if name == "baseline" else "FAIL:"
    if result.returncode != expected or marker not in result.stdout:
        raise RuntimeError(f"{name}: unexpected result {result.returncode}, see run.log")
    summary = next(line for line in result.stdout.splitlines() if "compared stereo frames" in line)
    print(f"{name}: {summary}")

print("Baseline passed; reset, stale-key and held-fallback mutations detected.")
