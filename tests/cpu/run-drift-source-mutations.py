#!/usr/bin/env python3
"""Compile *actual candidate source* negatives; require specific exit1 assertions.

Site mutants compile Engine.cpp independently, with real counter increments at
the source expression. Direct header mutants compile the real candidate
helper against the immutable original oracle. None modify the frozen fixture.
"""

import argparse
import pathlib
import subprocess
import sys


CASE = (
    ("CS80_DRIFT_MUTATE_DISABLE_SOURCE_FAST", True, "work.realEngineIsolationFastEntered"),
    ("CS80_DRIFT_MUTATE_SOURCE_FAKE_SITE", True, "work.realEngineIsolationSavedSinfCalls"),
    ("CS80_DRIFT_MUTATE_FAST_SIGN", False, "drift.signedZeroAndSineBits"),
    ("CS80_DRIFT_MUTATE_FAST_CLEAR_FLAGS", False, "drift.fpStickyAndExceptions"),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--build-dir", type=pathlib.Path, default=pathlib.Path("build-core"))
    parser.add_argument("--timeout", type=int, default=90)
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[2]
    build = args.build_dir.resolve()
    for macro, site, expected in CASE:
        target = build / f"drift-source-mutant-{macro.lower()}"
        command = [args.cxx, "-O3", "-DNDEBUG", "-std=c++20", "-frounding-math",
                   "-fPIE", "-I", str(root / "src"), "-I", str(root / "tests"),
                   f"-D{macro}"]
        if site:
            command += ["-DCS80_DRIFT_SINE_SOURCE_WITNESS",
                        str(root / "tests" / "DriftSineSourceWorkTests.cpp"),
                        str(root / "src" / "core" / "Engine.cpp"),
                        str(root / "src" / "core" / "FactoryPresets.cpp")]
        else:
            command += [str(root / "tests" / "DriftSineExactOracleTests.cpp"),
                        str(build / "libcs80_core.a")]
        command += ["-o", str(target)]
        print("COMPILE", macro, "real Engine source" if site else "real helper", flush=True)
        compiled = subprocess.run(command, text=True, capture_output=True,
                                  timeout=args.timeout)
        if compiled.returncode != 0:
            print(f"FAIL COMPILE {macro}: {compiled.stderr}", file=sys.stderr)
            return 1
        ran = subprocess.run([str(target)], text=True, capture_output=True,
                             timeout=args.timeout)
        first = next((line for line in ran.stderr.splitlines() if line.startswith("FAIL ")), "")
        print(f"{macro}: exit={ran.returncode} first={first!r}", flush=True)
        if ran.returncode != 1 or expected not in first:
            print(f"FAIL SOURCE MUTATION {macro}: expected normal exit1 {expected}\n"
                  f"stderr={ran.stderr}\nstdout={ran.stdout}", file=sys.stderr)
            return 1
    print(f"PASS {len(CASE)} actual source/helper compiled mutations", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
