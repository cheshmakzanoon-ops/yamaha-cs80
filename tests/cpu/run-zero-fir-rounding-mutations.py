#!/usr/bin/env python3
"""Qualify the published directed-rounding FIR extension without editing sources."""
from __future__ import annotations

import argparse
import difflib
import hashlib
import json
from pathlib import Path
import re
import subprocess


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def replace_once(source: str, before: str, after: str) -> str:
    if source.count(before) != 1:
        raise RuntimeError(f"Expected one mutation anchor: {before!r}")
    return source.replace(before, after, 1)


def mutants(source: str) -> dict[str, tuple[str, str]]:
    old = "            && nearestOutputRounding())"
    all_rounding = "all-rounding FIR ordered convolution including positive-center signed zeros"
    cached = replace_once(source, old, "            && nearestAtPrepare)")
    cached = replace_once(cached,
        "    void prepare(int newFactor) noexcept\n    {\n",
        "    void prepare(int newFactor) noexcept\n    {\n        nearestAtPrepare = nearestOutputRounding();\n")
    cached = replace_once(cached, "    unsigned nonzeroHistory = 0;",
        "    unsigned nonzeroHistory = 0;\n    bool nearestAtPrepare = nearestOutputRounding();")
    downward = replace_once(source,
        "        if (std::fegetround() != FE_TONEAREST) return false;",
        "        if (std::fegetround() != FE_TONEAREST && std::fegetround() != FE_DOWNWARD) return false;")
    downward = replace_once(downward,
        "        return (_mm_getcsr() & 0x6000u) == 0u;",
        "        return (_mm_getcsr() & 0x6000u) == 0u || (_mm_getcsr() & 0x6000u) == 0x2000u;")
    return {
        "omit_both_rounding_checks": (replace_once(source, old, "            && true)"), all_rounding),
        "omit_independent_mxcsr_check": (replace_once(source,
            "        return (_mm_getcsr() & 0x6000u) == 0u;", "        return true;"),
            "MXCSR-only FIR ordered convolution including positive-center signed zeros"),
        "cache_rounding_at_prepare": (cached, "same-history FIR rounding transition is exact"),
        "admit_downward_rounding": (downward, all_rounding),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", type=Path, required=True)
    parser.add_argument("--proposal", type=Path, required=True)
    parser.add_argument("--registration-sha", required=True)
    parser.add_argument("--published-contract", type=Path,
        help="Exact-SHA GitHub connector response, sealed below when local git lacks the API-published commit")
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--compiler", default="c++")
    args = parser.parse_args()
    root, proposal, evidence = args.repository.resolve(), args.proposal.resolve(), args.evidence.resolve()
    contract_path = "tests/cpu/zero-fir-history.json"
    contract_bytes = (proposal / contract_path).read_bytes()
    if not re.fullmatch(r"[0-9a-f]{40}", args.registration_sha):
        raise RuntimeError("Supply the full published acceptance SHA")
    if args.published_contract:
        registered = args.published_contract.read_bytes()
        if (args.registration_sha != "156d82e9f3ee20089bb952ba853e0f0c779b903f"
                or sha(registered) != "e03a258785d23ac6135cdf932034ed7b88fc744aeb5d9a0abb3f5118ff604997"):
            raise RuntimeError("Saved published response differs from the independently retrieved acceptance")
    else:
        registered = subprocess.run(["git", "show", args.registration_sha + ":" + contract_path],
            cwd=root, capture_output=True, check=True).stdout
    if contract_bytes.replace(b"\r\n", b"\n") != registered:
        raise RuntimeError("Acceptance differs from its published bytes")
    contract = json.loads(contract_bytes)
    extension = contract["rounding_environment_extension"]
    if (contract["absolute_error"] != 0 or contract["comparison"] != "bitwise float equality"
            or extension["standard_rounding_modes"] != ["FE_TONEAREST", "FE_DOWNWARD", "FE_UPWARD", "FE_TOWARDZERO"]):
        raise RuntimeError("The original exact limits and all four rounding directions are required")
    baseline_path = root / "tests/fixtures/exact_scalar_kernels/raw/Dsp.h"
    baseline = baseline_path.read_bytes().replace(b"\r\n", b"\n")
    manifest = json.loads((root / "tests/fixtures/exact_scalar_kernels/manifest.json").read_text())
    sealed = manifest["files"]["tests/fixtures/exact_scalar_kernels/raw/Dsp.h"]["sha256"]
    if sha(baseline) != sealed:
        raise RuntimeError("Unmodified positive control differs from the published scalar oracle")
    header_path = proposal / "src/core/Dsp.h"
    candidate_bytes = header_path.read_bytes()
    source = candidate_bytes.decode()
    driver = proposal / "tests/ZeroFirRoundingTests.cpp"
    evidence.mkdir(parents=True, exist_ok=True)
    (evidence / "registration.json").write_bytes(contract_bytes)
    (evidence / "driver.cpp").write_bytes(driver.read_bytes())
    (evidence / "runner.py").write_bytes(Path(__file__).read_bytes())
    cases = {"unmodified_positive_control": (baseline.decode(), ""), "candidate": (source, ""),
             **mutants(source), "restored_candidate": (source, "")}
    results = {}
    for name, (text, expected) in cases.items():
        directory = evidence / name
        (directory / "core").mkdir(parents=True, exist_ok=True)
        header = directory / "core/Dsp.h"
        header.write_text(text)
        binary = directory / "test"
        command = [args.compiler, "-std=c++20", "-O2", "-DNDEBUG", "-frounding-math",
                   "-Wall", "-Wextra", "-pedantic", "-I", str(directory), str(driver), "-o", str(binary)]
        built = subprocess.run(command, text=True, capture_output=True, timeout=120, check=False)
        (directory / "build.log").write_text("COMMAND " + json.dumps(command) + "\n"
            + built.stdout + built.stderr + "\nEXIT " + str(built.returncode) + "\n")
        if built.returncode:
            raise RuntimeError(name + " failed to compile; this does not count as a rejected mutation")
        run = subprocess.run([str(binary)], text=True, capture_output=True, timeout=120, check=False)
        (directory / "run.log").write_text(run.stdout + run.stderr + "\nEXIT " + str(run.returncode) + "\n")
        match = re.search(r"Zero FIR rounding: (\d+) checks, (\d+) failures, (\d+) downward witnesses, (\d+) independent MXCSR directions", run.stdout)
        if match is None:
            raise RuntimeError(name + " did not finish the assertion gate")
        result = {"exit_code": run.returncode, "checks": int(match[1]), "failures": int(match[2]),
                  "downward_witnesses": int(match[3]), "independent_mxcsr_directions": int(match[4]),
                  "expected_assertion": expected, "expected_assertion_observed": expected in run.stderr,
                  "header_sha256": sha(text.encode())}
        results[name] = result
        (evidence / "partial-results.json").write_text(json.dumps(results, indent=2) + "\n")
        print(json.dumps({"case": name, **result}), flush=True)
        if expected:
            if run.returncode != 1 or not result["failures"] or not result["expected_assertion_observed"]:
                raise RuntimeError(name + " survived or failed for an unregistered reason")
            if name == "omit_independent_mxcsr_check" and result["independent_mxcsr_directions"] == 0:
                raise RuntimeError("No independent arithmetic-control witness was observed on this target")
            (directory / "mutation.patch").write_text("".join(difflib.unified_diff(
                source.splitlines(True), text.splitlines(True), fromfile="a/src/core/Dsp.h", tofile="b/src/core/Dsp.h")))
        elif run.returncode or result["failures"]:
            raise RuntimeError(name + " failed unchanged exact acceptance")
    if header_path.read_bytes() != candidate_bytes:
        raise RuntimeError("The source candidate changed during qualification")
    record = {"registration_commit": args.registration_sha,
              "contract_sha256": sha(contract_bytes), "baseline_sha256": sha(baseline),
              "candidate_sha256": sha(candidate_bytes), "driver_sha256": sha(driver.read_bytes()),
              "runner_sha256": sha(Path(__file__).read_bytes()), "compiler": args.compiler,
              "outcomes": results,
              "scope": "Exact FIR rounding continuation and four actual-source guard mutations. The original nearest-only history gate, whole-engine compatibility and CPU timings remain separate requirements."}
    (evidence / "qualification.json").write_text(json.dumps(record, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
