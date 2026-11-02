#!/usr/bin/env python3
"""Challenge the preregistered public-estimate contract in isolated copies.

Run under the project shared heavy-check lock. No production files are edited.
Retained diagnostics are stderr + terminal summary, not complete stdout; hashes
identify the complete stdout streams (the green run separately retains all arms).
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="g++")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    if args.output.exists():
        parser.error("output must not already exist")
    args.output.mkdir(parents=True)
    source = {name: (root / "src/core" / name).read_text() for name in (
        "HardwareControl.h", "FactoryToneHypotheses.h",
        "FactoryTonePublicEstimates.h", "SourceFactoryTones.h")}
    data = source["FactoryTonePublicEstimates.h"]
    pattern = re.compile(r"\{\{(5[1-4]),'([A-F])',(\d+)\},(\d+),Basis::(\w+)\}")
    cells = list(pattern.finditer(data))
    if len(cells) != 25:
        raise RuntimeError("exactly 25 address rows required")
    results = []
    with tempfile.TemporaryDirectory(prefix="cs80-public-tone-mutants-") as directory:
        scratch = Path(directory)
        headers = scratch / "src/core"
        headers.mkdir(parents=True)

        def run(label, changed=None, must_fail=False, literal=False):
            for name, text in source.items():
                (headers / name).write_text((changed or {}).get(name, text))
            exe = scratch / "contract"
            command = [args.compiler, "-std=c++20", "-O0", "-Wall", "-Wextra", "-Wpedantic",
                       "-I" + str(scratch / "src"), str(root / "tests/PublicToneEstimateTests.cpp"), "-o", str(exe)]
            build = subprocess.run(command, capture_output=True, text=True, timeout=60)
            if build.returncode:
                (args.output / (label + ".log")).write_text(build.stdout + build.stderr)
                raise RuntimeError(label + " did not compile; compile failure is not accepted as mutation detection")
            test = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
            summary = test.stdout.splitlines()[-1] if test.stdout.splitlines() else "NO SUMMARY"
            diagnostics = build.stdout + build.stderr + test.stderr + summary + "\n"
            (args.output / (label + ".log")).write_text(diagnostics)
            row = {"name": label, "expected_failure": must_fail, "exit_code": test.returncode,
                   "summary": summary, "literal_value_failures": test.stderr.count("FAIL: literal amended-mark excitation"),
                   "stdout_sha256": hashlib.sha256(test.stdout.encode()).hexdigest(),
                   "stdout_bytes": len(test.stdout.encode()),
                   "diagnostics_sha256": hashlib.sha256(diagnostics.encode()).hexdigest()}
            results.append(row)
            print(json.dumps(row), flush=True)
            if (test.returncode != 0) != must_fail:
                raise RuntimeError(label + " unexpected test outcome")
            if literal and not row["literal_value_failures"]:
                raise RuntimeError(label + " was not caught by independent literal-value checks")

        run("positive-before")
        # Keep declared policy metadata pinned, so the independently written
        # literal tests—not the implementation's self-hash assertion—must catch
        # each changed value. Direct topology mutants become a 1-ohm resistor
        # estimate to remain structurally valid and compile successfully.
        pin = "inline constexpr std::uint64_t fingerprint = manifestFingerprint();"
        assert pin in data
        for match in cells:
            board, column, output, ohms, basis = match.groups()
            replacement_basis = "amendedResistor" if int(ohms) == 0 else basis
            replacement = "{{%s,'%s',%s},%d,Basis::%s}" % (board,column,output,int(ohms)+1,replacement_basis)
            mutated = data[:match.start()] + replacement + data[match.end():]
            mutated = mutated.replace(pin,"inline constexpr std::uint64_t fingerprint = 0x832eee6f44f5cfccull;")
            run("cell-%s-%s-%s" % (board,column,output), {"FactoryTonePublicEstimates.h": mutated}, True, True)
        api = source["SourceFactoryTones.h"]
        needle = "next,publicEstimateScenario(rank,selector),hpGrade,lpGrade"
        assert api.count(needle) == 1
        run("old-median-api", {"SourceFactoryTones.h": api.replace(needle,"next,{},hpGrade,lpGrade")}, True, True)
        run("restored-after")
    manifest = {"schema":1, "kind":"software-policy-mutations-not-hardware", "compiler":args.compiler,
                "source_commit":subprocess.check_output(["git","rev-parse","HEAD"],cwd=root,text=True).strip(),
                "source_sha256":{name:hashlib.sha256(text.encode()).hexdigest() for name,text in source.items()},
                "test_sha256":hashlib.sha256((root/"tests/PublicToneEstimateTests.cpp").read_bytes()).hexdigest(),
                "retained_output":"stderr and terminal summary; full stdout SHA-256 only", "results":results}
    (args.output / "manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")


if __name__ == "__main__":
    main()
