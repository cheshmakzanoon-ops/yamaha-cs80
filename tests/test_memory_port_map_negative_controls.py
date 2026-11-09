#!/usr/bin/env python3
"""Prove the loaded-map gate detects omitted rail coupling and forced reciprocity.

GNU/Clang command-line conventions. Mutations live only in a temporary include
directory; the repository's sources are never changed. This is model consistency,
not a test of shipping audio or nonlinear excursion accuracy.
"""

import argparse
from pathlib import Path
import subprocess
import tempfile


def replace_once(source: str, old: str, new: str) -> str:
    if source.count(old) != 1:
        raise RuntimeError(f"Expected one mutation marker: {old!r}")
    return source.replace(old, new, 1)


def run(root: Path, directory: Path, compiler: str, header: str) -> subprocess.CompletedProcess:
    include = directory / "experimental"
    include.mkdir(parents=True)
    (include / "MemoryPanelPortMap.h").write_text(header, encoding="utf-8")
    executable = directory / "test"
    subprocess.run(
        [compiler, "-std=c++20", "-O2", "-I", str(directory), "-I", str(root / "src"),
         str(root / "tests/MemoryPanelPortMapTests.cpp"), "-o", str(executable)],
        check=True, capture_output=True, text=True, timeout=60,
    )
    return subprocess.run(
        [str(executable), str(root / "tests/parity/tolerances-v2/memory-port-map.json")],
        capture_output=True, text=True, timeout=30,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, help="GNU/Clang C++ compiler executable")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    original = (root / "src/experimental/MemoryPanelPortMap.h").read_text(encoding="utf-8")
    no_rails = replace_once(original, "next[i] += railToBus[k][i] * rail[k];", "next[i] += 0.0 * rail[k];")
    no_rails = replace_once(no_rails, "value += railToBus[k][i] * currentToRail[k][j];", "value += 0.0;")
    reciprocal = replace_once(
        original, "value += railToBus[k][i] * currentToRail[k][j];",
        "value += 0.5 * (railToBus[k][i] * currentToRail[k][j] + railToBus[k][j] * currentToRail[k][i]);",
    )
    with tempfile.TemporaryDirectory(prefix="cs80-memory-port-map-") as temporary:
        directory = Path(temporary)
        for name, header in (("baseline", original), ("no-shared-rails", no_rails), ("reciprocal-followers", reciprocal)):
            result = run(root, directory / name, args.compiler, header)
            expected = 0 if name == "baseline" else 1
            marker = "0 failures" if name == "baseline" else "FAIL: factorized derivative matches every full-reference Jacobian port"
            if result.returncode != expected or marker not in result.stdout:
                raise RuntimeError(f"{name}: unexpected result {result.returncode}: {result.stdout}{result.stderr}")
            summary = next(line for line in result.stdout.splitlines() if line.startswith("Memory port map:"))
            print(f"{name}: {summary}")
    print("Memory port map baseline passed; omitted-rail and reciprocal-follower mutants detected")


if __name__ == "__main__":
    main()
