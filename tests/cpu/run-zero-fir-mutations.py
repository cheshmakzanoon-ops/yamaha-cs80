#!/usr/bin/env python3
"""Qualify the exact FIR history cache and actual-source mutations in isolation.

The registered chronological oracle retains the original summation order and
signed-zero/subnormal fixtures. Original source files are never modified.
"""
from __future__ import annotations

import argparse
import difflib
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def replace_once(source: str, before: str, after: str) -> str:
    if source.count(before) != 1:
        raise RuntimeError(f"Expected one mutation anchor: {before!r}")
    return source.replace(before, after, 1)


def mutations(source: str) -> dict[str, tuple[str, str]]:
    guard = "nonzeroHistory == 0 && std::bit_cast<std::uint32_t>(newest[-center]) == 0u"
    count = "exact history count including subnormals"
    output = "full FIR chronological convolution including signed zeros"
    return {
        "treat_nonzero_as_silent": (replace_once(source, guard, "true"), output),
        "forget_history_decrement": (replace_once(source,
            "        nonzeroHistory -= hasMagnitude(hist[(size_t) idx]) ? 1u : 0u;\n", ""), count),
        "forget_history_reset": (replace_once(source,
            "idx = 0; nonzeroHistory = 0;", "idx = 0;"), count),
        "premature_silence": (replace_once(source, guard,
            "nonzeroHistory <= 1 && std::bit_cast<std::uint32_t>(newest[-center]) == 0u"), output),
        "discard_negative_zero_sum": (replace_once(source, guard, "nonzeroHistory == 0"),
            "coefficient-aligned signed zeros retain full FIR output bits"),
        "classify_history_through_daz": (replace_once(source,
            "const volatile std::uint32_t bits = std::bit_cast<std::uint32_t>(value);\n        return (bits & 0x7fffffffu) != 0;",
            "return value != 0.0f;"), count),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--registration-repository", type=Path,
                        help="Local repository containing the already-published acceptance commit")
    parser.add_argument("--candidate-header", type=Path)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--registration-sha", required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    root = args.source.resolve()
    header_path = (args.candidate_header or root / "src/core/Dsp.h").resolve()
    original = header_path.read_bytes()
    source = header_path.read_text()
    contract_path = "tests/cpu/zero-fir-history.json"
    contract_bytes = (root / contract_path).read_bytes()
    if not re.fullmatch(r"[0-9a-f]{40}", args.registration_sha):
        raise RuntimeError("Supply the full published registration SHA before evaluating")
    committed = subprocess.run(["git", "show", args.registration_sha + ":" + contract_path],
                              cwd=(args.registration_repository or root), capture_output=True, check=True).stdout
    if contract_bytes.replace(b"\r\n", b"\n") != committed:
        raise RuntimeError("Current acceptance differs from its published registration")
    contract = json.loads(contract_bytes)
    if contract["absolute_error"] != 0 or contract["comparison"] != "bitwise float equality":
        raise RuntimeError("Exact registered FIR limits must remain unchanged")
    evidence = args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    driver = root / "tests/ZeroFirHistoryTests.cpp"
    results = {}
    with tempfile.TemporaryDirectory(prefix="cs80-zero-fir-") as temp:
        directory = Path(temp)
        header = directory / "core/Dsp.h"
        header.parent.mkdir()
        binary = directory / "zero-fir-test"
        for name, (text, expected) in {"candidate": (source, ""), **mutations(source),
                                     "restored_candidate": (source, "")}.items():
            header.write_text(text)
            command = [args.compiler, "-std=c++20", "-O2", "-DNDEBUG", "-Wall", "-Wextra", "-pedantic",
                       "-I", str(directory), "-I", str(root / "src/core"), "-I", str(root / "src"),
                       str(driver), "-o", str(binary)]
            built = subprocess.run(command, text=True, capture_output=True, timeout=120, check=False)
            (evidence / (name + "-build.log")).write_text(
                "COMMAND " + json.dumps(command) + "\n" + built.stdout + built.stderr
                + "\nEXIT " + str(built.returncode) + "\n")
            if built.returncode:
                raise RuntimeError(name + " did not compile; build failures are not mutation kills")
            run = subprocess.run([str(binary)], text=True, capture_output=True, timeout=120, check=False)
            (evidence / (name + "-run.log")).write_text(run.stdout + run.stderr
                + "\nEXIT " + str(run.returncode) + "\n")
            match = re.search(r"Zero FIR history: (\d+) checks, (\d+) failures", run.stdout)
            if match is None:
                raise RuntimeError(name + " did not finish its assertion gate")
            result = {"exit_code": run.returncode, "checks": int(match[1]), "failures": int(match[2]),
                      "expected_assertion": expected, "expected_assertion_observed": expected in run.stderr,
                      "header_sha256": hashlib.sha256(text.encode()).hexdigest()}
            results[name] = result
            print(json.dumps({"case": name, **result}), flush=True)
            if name in ("candidate", "restored_candidate"):
                if run.returncode != 0 or result["failures"] != 0:
                    raise RuntimeError("Candidate failed the exact FIR contract")
            elif run.returncode != 1 or not result["failures"] or not result["expected_assertion_observed"]:
                raise RuntimeError(name + " did not fail the registered assertion")
            if name not in ("candidate", "restored_candidate"):
                (evidence / (name + ".patch")).write_text("".join(difflib.unified_diff(
                    source.splitlines(True), text.splitlines(True),
                    fromfile="a/src/core/Dsp.h", tofile="b/src/core/Dsp.h")))
    if header_path.read_bytes() != original:
        raise RuntimeError("Original candidate header changed during qualification")
    record = {"registration_commit": args.registration_sha,
              "candidate_header_sha256": hashlib.sha256(original).hexdigest(),
              "contract_sha256": hashlib.sha256(contract_bytes).hexdigest(),
              "driver_sha256": hashlib.sha256(driver.read_bytes()).hexdigest(),
              "compiler": args.compiler, "outcomes": results,
              "scope": "Exact component continuation and work classification; whole-engine compatibility and timings are separate."}
    (evidence / "qualification.json").write_text(json.dumps(record, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
