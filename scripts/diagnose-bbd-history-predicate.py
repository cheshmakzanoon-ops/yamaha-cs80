#!/usr/bin/env python3
"""Run the frozen Intel predicate witness using CMake's actual target flags.

Original source and all 1,245 assertions stay untouched. Candidate and existing
floating-comparison mutation headers live only in the evidence directory.
"""
from pathlib import Path
import argparse
import difflib
import hashlib
import json
import platform
import re
import shlex
import subprocess
import sys


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "build", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    args = parser.parse_args()
    source, build, out = args.source.resolve(), args.build.resolve(), args.out.resolve()
    driver = Path(__file__).resolve().parents[1]
    probe = driver / "tests/diagnostics/BbdHistoryPredicateProbe.cpp"
    contract_path = driver / "tests/diagnostics/bbd-predicate-contract.json"
    contract = json.loads(contract_path.read_text())
    if not re.fullmatch(r"[0-9a-f]{40}", args.source_commit) or args.source_commit != contract["frozen_source_commit"]:
        raise ValueError("diagnosis requires the registered immutable source")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    if commit != args.source_commit or platform.machine() != "x86_64":
        raise ValueError("unexpected checkout or native architecture")
    status_before = subprocess.check_output(["git", "status", "--porcelain"], cwd=source, text=True)
    if status_before or out.exists():
        raise ValueError("require unchanged source and a new evidence directory")
    out.mkdir(parents=True)
    gate = source / contract["unchanged_gate"]["path"]
    header = source / "src/core/BbdChorus.h"
    original = header.read_text()
    commands = json.loads((build / "compile_commands.json").read_text())
    matches = [entry for entry in commands if (Path(entry["directory"]) / entry["file"]).resolve() == gate]
    if len(matches) != 1:
        raise ValueError("original gate compile command is missing or ambiguous")
    entry = matches[0]
    generated = entry.get("arguments") or shlex.split(entry["command"])
    flags = []
    index = 0
    while index < len(generated):
        value = generated[index]
        if value in ("-o", "-MF", "-MT", "-MQ"):
            index += 2
            continue
        if value in ("-c", "-MD", "-MMD") or (not value.startswith("-") and (Path(entry["directory"]) / value).resolve() == gate):
            index += 1
            continue
        flags.append(value)
        index += 1
    record = {
        "source_commit": commit, "platform": platform.platform(),
        "generated_gate_command": generated, "reused_compiler_arguments": flags,
        "source_sha256": {"header": sha(header), "gate": sha(gate), "contract": sha(contract_path), "probe": sha(probe)},
        "processes": [], "performance_pass_claim": False,
    }
    (out / "contract.json").write_bytes(contract_path.read_bytes())
    (out / "original-IdleExactSkipTests.cpp").write_bytes(gate.read_bytes())
    (out / "original-BbdChorus.h").write_bytes(header.read_bytes())
    (out / "BbdHistoryPredicateProbe.cpp").write_bytes(probe.read_bytes())

    def save():
        (out / "qualification.json").write_text(json.dumps(record, indent=2) + "\n")

    def execute(name, command):
        result = subprocess.run(command, cwd=entry["directory"], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
        (out / (name + ".log")).write_text("COMMAND " + json.dumps(command) + "\n" + result.stdout
                                          + "\nEXIT " + str(result.returncode) + "\n")
        record["processes"].append({"name": name, "command": command, "exit_code": result.returncode})
        save()
        print(f"{name}: exit {result.returncode}", flush=True)
        print(result.stdout, end="", flush=True)
        return result

    def compile_file(name, file, overlay=None, extra=()):
        extra_include = ["-I", str(overlay), "-I", str(source / "src/core")] if overlay else []
        result = execute(name, [flags[0], *extra_include, *flags[1:], *extra, str(file), "-o", str(out / name)])
        if result.returncode:
            raise RuntimeError(name + " did not compile; it is not a numerical witness")

    execute("compiler-version", [flags[0], "--version"])
    compile_file("probe-assembly.s", probe, extra=("-S",))
    compile_file("probe-llvm.ll", probe, extra=("-S", "-emit-llvm"))
    compile_file("probe", probe)
    observed = execute("probe-run", [str(out / "probe")])
    rows = [dict(zip(("mxcsr", "item", "expected", "production", "candidate"), match))
            for match in re.findall(r"mxcsr=([0-9a-f]+) item=(\d+) expected=(\d) production=(\d) candidate=(\d)", observed.stdout)]
    record["probe_rows"] = rows
    record["baseline_predicate_errors"] = sum(row["production"] != row["expected"] for row in rows)
    record["candidate_predicate_errors"] = sum(row["candidate"] != row["expected"] for row in rows)
    save()
    record["probe_control_met"] = (len(rows) == contract["probe"]["cases"]
                                  and record["baseline_predicate_errors"] > 0
                                  and record["candidate_predicate_errors"] == contract["probe"]["candidate_classification_errors"])

    old = "    return (std::bit_cast<std::uint32_t>(sample) & 0x7fffffffu) != 0u;"
    new = "    const volatile std::uint32_t bits = std::bit_cast<std::uint32_t>(sample);\n    return (bits & 0x7fffffffu) != 0u;"
    if original.count(old) != 1:
        raise ValueError("frozen production predicate anchor changed")
    candidate = original.replace(old, new, 1)
    mutant = candidate.replace("    return (bits & 0x7fffffffu) != 0u;", "    return sample != 0.0f;", 1)
    for name, contents in (("candidate", candidate), ("floating-comparison-mutation", mutant)):
        overlay = out / (name + "-include")
        (overlay / "core").mkdir(parents=True)
        (overlay / "core/BbdChorus.h").write_text(contents)
        (out / (name + ".patch")).write_text("".join(difflib.unified_diff(original.splitlines(True), contents.splitlines(True),
                                           fromfile="original/BbdChorus.h", tofile=name + "/BbdChorus.h")))
    gates = []
    for name, overlay in (("original-gate", None), ("candidate-gate", out / "candidate-include"),
                          ("mutation-gate", out / "floating-comparison-mutation-include")):
        compile_file(name, gate, overlay)
        result = execute(name + "-run", [str(out / name)])
        summary = re.search(r"Idle exact skips: (\d+) checks, (\d+) failures", result.stdout)
        if summary is None:
            raise RuntimeError(name + " did not finish the unchanged gate")
        gates.append({"name": name, "exit_code": result.returncode, "checks": int(summary[1]),
                      "failures": int(summary[2]), "named_predicate_failure": contract["mutation"]["required_assertion"] in result.stdout})
    record["unchanged_gates"] = gates
    record["source_unchanged"] = (header.read_text() == original and sha(gate) == record["source_sha256"]["gate"]
                                  and subprocess.check_output(["git", "status", "--porcelain"], cwd=source, text=True) == status_before)
    save()
    baseline, fixed, mutation = gates
    if (not record["probe_control_met"] or not record["source_unchanged"]
            or any(gate["checks"] != contract["unchanged_gate"]["expected_checks"] for gate in gates)
            or baseline["exit_code"] != 1 or baseline["failures"] != 1 or not baseline["named_predicate_failure"]
            or fixed["exit_code"] != 0 or fixed["failures"] != contract["unchanged_gate"]["candidate_failures"]
            or mutation["exit_code"] != 1 or mutation["failures"] == 0 or not mutation["named_predicate_failure"]):
        raise RuntimeError("unchanged gate or existing floating-comparison mutation failed its diagnostic contract")
    record["diagnostic_contract_met"] = True
    record["note"] = "Expected original/mutant failures are retained. This result is not an overall plugin or CPU performance pass."
    save()
    hashes = {str(path.relative_to(out)): sha(path) for path in sorted(out.rglob("*")) if path.is_file()}
    (out / "sha256.json").write_text(json.dumps(hashes, indent=2) + "\n")


if __name__ == "__main__":
    main()
