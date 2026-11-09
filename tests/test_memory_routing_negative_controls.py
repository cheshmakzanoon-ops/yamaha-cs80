#!/usr/bin/env python3
"""Verify the loaded receiver gate detects frozen touch feedback and duplicate loads.

GNU/Clang command-line conventions. Only temporary header copies are changed.
These are same-operating-point model consistency tests, not shipping audio.
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
    (include / "LoadedMemoryReceivers.h").write_text(header, encoding="utf-8")
    executable = directory / "test"
    subprocess.run(
        [compiler, "-std=c++20", "-O2", "-I", str(directory), "-I", str(root / "src"),
         str(root / "tests/MemoryControlRoutingTests.cpp"), "-o", str(executable)],
        check=True, capture_output=True, text=True, timeout=60,
    )
    return subprocess.run(
        [str(executable), str(root / "tests/parity/tolerances-v2/memory-control-routing.json")],
        capture_output=True, text=True, timeout=30,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, help="GNU/Clang C++ compiler executable")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    original = (root / "src/experimental/LoadedMemoryReceivers.h").read_text(encoding="utf-8")
    frozen = replace_once(original,
        "injectionPerRail[rail][row] = gain * (velocitySum * map.railToBus[rail][22] + pressureSum * map.railToBus[rail][24]);",
        "injectionPerRail[rail][row] = 0.0;")
    frozen = replace_once(frozen,
        "injectionPerRail[rail][21] = levelGain * (velocitySum * map.railToBus[rail][23] + pressureSum * map.railToBus[rail][25]);",
        "injectionPerRail[rail][21] = 0.0;")
    marker = "    if (!map.evaluate(delta, next.busVolts)) return false;"
    duplicate = replace_once(original, marker, marker + "\n"
        "    for (std::size_t row = 0; row < 26; ++row) next.busVolts[row] /= 1.0 + 10000.0 * "
        "sf::compiledReceiverSiemens(static_cast<int>(row + 1), sf::Grade::B, sf::Grade::B);")
    with tempfile.TemporaryDirectory(prefix="cs80-memory-routing-") as temporary:
        for name, header, failure in (
            ("baseline", original, "0 failures"),
            ("frozen-touch-feedback", frozen, "FAIL: receiver route accepts complete test frame"),
            ("duplicate-loads", duplicate, "FAIL: loaded feedback matches dense 24-bus solution"),
        ):
            result = run(root, Path(temporary) / name, args.compiler, header)
            expected = 0 if name == "baseline" else 1
            if result.returncode != expected or failure not in result.stdout:
                raise RuntimeError(f"{name}: unexpected result {result.returncode}: {result.stdout}{result.stderr}")
            summary = next(line for line in result.stdout.splitlines() if line.startswith("Memory routing:"))
            print(f"{name}: {summary}")
    print("Memory receiver baseline passed; frozen-feedback and duplicate-loading mutants detected")


if __name__ == "__main__":
    main()
