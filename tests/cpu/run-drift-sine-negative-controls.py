#!/usr/bin/env python3
"""Compile each independently preregistered drift/sine negative control.

Expected failure is an ordinary exit 1 with the named *runtime* assertion.
A compiler error, signal, timeout, wrong assertion or exit 0 is not a kill.
This is test-only oracle mutation, not a production source mutation yet.
"""

import argparse
import pathlib
import subprocess
import sys


MUTATIONS = {
    "CS80_DRIFT_MUTATE_ZERO_SIGN": "drift.signedZeroAndSineBits",
    "CS80_DRIFT_MUTATE_UNSAFE_ZERO_SKIP": "drift.signedZeroAndSineBits",
    "CS80_DRIFT_MUTATE_CROSS_LANE": "drift.signedZeroAndSineBits",
    "CS80_DRIFT_MUTATE_FLAGS": "drift.fpStickyAndExceptions",
    "CS80_DRIFT_MUTATE_THERMAL": "drift.thermalOrWornTermBits",
    "CS80_DRIFT_MUTATE_ENGINE_GOLDEN": "engine.frozenAudioBits",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--build-dir", type=pathlib.Path, default=pathlib.Path("build-core"))
    parser.add_argument("--timeout", type=int, default=90)
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[2]
    build = args.build_dir.resolve()
    source = root / "tests" / "DriftSineExactOracleTests.cpp"
    core = build / "libcs80_core.a"
    if not core.is_file():
        parser.error(f"missing compiled original-source library {core}")
    for flag, expected in MUTATIONS.items():
        output = build / f"drift-negative-{flag.lower()}"
        cmd = [args.cxx, "-O3", "-DNDEBUG", "-std=c++20", "-frounding-math",
               "-fPIE", "-I", str(root / "src"), "-I", str(root / "tests"),
               f"-D{flag}", str(source), str(core), "-o", str(output)]
        print(f"Compiling {flag}: {' '.join(cmd)}", flush=True)
        compiled = subprocess.run(cmd, text=True, capture_output=True, timeout=args.timeout)
        if compiled.returncode != 0:
            print(f"FAIL COMPILE {flag}: {compiled.stderr}", file=sys.stderr)
            return 1
        ran = subprocess.run([str(output)], text=True, capture_output=True,
                             timeout=args.timeout)
        first_fail = next((line for line in ran.stderr.splitlines() if line.startswith("FAIL ")), "")
        print(f"{flag}: exit={ran.returncode} first='{first_fail}'", flush=True)
        if ran.returncode != 1 or expected not in first_fail:
            print(f"FAIL MUTATION {flag}: expected exit1 and first named '{expected}'\n"
                  f"stderr={ran.stderr}\nstdout={ran.stdout}", file=sys.stderr)
            return 1
    print(f"PASS: {len(MUTATIONS)} compiled named normal-exit negative controls", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
