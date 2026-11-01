#!/usr/bin/env python3
"""Qualify the published ring/precise-charge contract using real isolated mutants."""
from pathlib import Path
import argparse
import difflib
import hashlib
import json
import re
import shutil
import subprocess
import tempfile

REGISTRATION = "97147ed03ee325d6d6cd1277cf47efcdf121e412"
CONTRACT = "tests/parity/tolerances-v2/ring-event-continuity.json"
ENGINE = "src/core/Engine.cpp"
DSP = "src/core/Dsp.h"
DRIVER = "tests/RingEventContinuityTests.cpp"


def cases():
    result = []

    def add(name, suite, edits, expected, exit_code=1):
        result.append(dict(name=name, suite=suite, edits=edits, expected=expected, exit=exit_code))

    ring_anchor = "        const bool retainRingState = usesSourceModels(global.renderPolicy);"
    for policy, enum in [("source", "sourcePerformance"), ("pure", "isolation")]:
        condition = f"global.renderPolicy == RenderPolicy::{enum}"
        add(f"phase-reset-{policy}", "ring", [(ENGINE, ring_anchor,
            ring_anchor + f"\n        if ({condition}) ringPhase = 0.0f;")], "source carrier phase")
        add(f"charge-reset-{policy}", "ring", [(ENGINE, ring_anchor,
            ring_anchor + f"\n        if ({condition}) ringAdsr.reset();")], "source precise charge")
    add("telemetry-reset", "ring", [(ENGINE, ring_anchor, ring_anchor +
        "\n        if (retainRingState) ringEnv = 0.0f;")], "source envelope telemetry")
    add("precise-charge-rounded-through-float", "all", [(DSP,
        "if (!preservePreciseCharge || a <= 0.0002f)", "if (true)")], "source precise charge")
    origin = "        if (!preservePreciseCharge || a <= 0.0002f)\n            preciseValue = value;\n        attackFrom = preciseValue;"
    add("rounded-attack-origin", "all", [(DSP, origin,
        origin.replace("attackFrom = preciseValue;", "attackFrom = static_cast<double>(value);"))], "retained precise attack origin")
    rank_call = "std::max(0.001f, rs.vR * global.envTimeScale * r.tol.ampEnvTime),\n                      usesSourceModels(global.renderPolicy));"
    for rank in [0, 1]:
        add(f"rank-{rank}-charge-rounded", "rank", [(ENGINE, rank_call,
            rank_call.replace("usesSourceModels(global.renderPolicy)",
                              f"usesSourceModels(global.renderPolicy) && ch != {rank}"))],
            f"rank{rank} source precise retention")
    helper_anchor = "private:\n    friend struct EngineEventContinuityProbe;"
    add("rank-II-only-rounded-origin", "rank", [
        (DSP, helper_anchor, "public:\n    void mutationRoundAttackOrigin() noexcept { attackFrom = static_cast<double>(value); }\n" + helper_anchor),
        (ENGINE, rank_call, rank_call +
         "\n        if (ch == 1 && usesSourceModels(global.renderPolicy)) r.aenv.mutationRoundAttackOrigin();")],
        "rank1 precise attack origin")
    trigger = "        ringAdsr.noteOn(global.ringAttack, global.ringDecay, 0.0f, 0.35f, retainRingState);"
    add("suppress-first-key-envelope-trigger", "ring", [(ENGINE, trigger,
        "        if (!retainRingState)\n    " + trigger)], "enters Attack")
    add("retrigger-envelope-on-legato", "ring", [(ENGINE, "    if (! ringKeyDown)\n    {",
        "    if (true)\n    {")], "nontrigger selection float elapsed")
    add("change-historical-legacy-reset", "ring", [(ENGINE, ring_anchor,
        "        const bool retainRingState = true;")], "historical Legacy phase reset")
    observer = "        // OBSERVER_INERT_POINT: no elapsed time belongs between event snapshots."
    add("observer-renders-one-extra-frame", "ring", [(DRIVER, observer,
        observer + "\n        float extraFrame[2]; actual->render(extraFrame, 1);")], "nonmutating observer PCM")
    add("neutral-ring-amount-leaks", "ring", [
        (ENGINE, "if (global.ringAmount > 0.001f)", "if (global.ringAmount >= 0.0f)"),
        (ENGINE, "clampf(global.ringAmount, 0.0f, 1.0f);",
         "clampf(global.ringAmount + 0.1f, 0.0f, 1.0f);")], "neutral ring PCM")
    add("explicit-reset-retains-phase", "ring", [(ENGINE,
        "    ringAdsr.reset();\n    ringPhase = 0.0f;",
        "    ringAdsr.reset();\n    (void) ringPhase;")], "explicit reset phase")
    add("explicit-reset-retains-charge", "ring", [(ENGINE,
        "    ringAdsr.reset();\n    ringPhase = 0.0f;",
        "    (void) ringAdsr;\n    ringPhase = 0.0f;")], "explicit reset precise charge")
    for policy in ["Source", "Pure"]:
        for field, assertion in [
            ("phase", '            r.exact(before.phase, after.phase, label + " source carrier phase", "phase_" + suffix);'),
            ("precise_charge", '            r.exact(before.charge.charge, after.charge.charge, label + " source precise charge", "precise_charge_" + suffix);')]:
            add(f"omitted-{policy}-{field}-assertion", "ring", [(DRIVER, assertion,
                f"            if (policy != Policy::{policy})\n    " + assertion)],
                "registered coverage " + field + "_" + policy)
    add("weakened-zero-bit-bound", "ring", [(CONTRACT,
        '"source_zero_time_phase_bit_mismatches_max": 0',
        '"source_zero_time_phase_bit_mismatches_max": 1')], "registered bit bound must remain zero", 2)
    return result


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--case", action="append")
    args = parser.parse_args()
    root, evidence = args.source.resolve(), args.evidence.resolve()
    source_commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    evidence.mkdir(parents=True, exist_ok=True)
    contract = (root / CONTRACT).read_bytes()
    published = subprocess.check_output(["git", "show", REGISTRATION + ":" + CONTRACT], cwd=root)
    if contract.replace(b"\r\n", b"\n") != published:
        raise RuntimeError("contract differs from pre-evaluation published registration")
    selected = cases()
    if args.case:
        known = {case["name"] for case in selected}
        if set(args.case) - known: raise RuntimeError("unknown requested mutation")
        selected = [case for case in selected if case["name"] in args.case]
    results = []
    with tempfile.TemporaryDirectory(prefix="cs80-ring-mutations-") as directory:
        tree = Path(directory)
        shutil.copytree(root / "src", tree / "src")
        shutil.copytree(root / "tests", tree / "tests")
        tracked = {ENGINE, DSP, DRIVER, CONTRACT}
        originals = {p: (tree / p).read_text() for p in tracked}
        units = [ENGINE, "src/core/FactoryPresets.cpp", DRIVER]
        objects = {}

        def compile_and_run(name, suite, expected=None, expected_exit=0):
            headers = sorted((tree / "src").rglob("*.h")) + sorted((tree / "tests").glob("*.h"))
            header_hash = sha(b"".join(p.read_bytes() for p in headers))
            log = []
            compiled = []
            for unit in units:
                key = sha(header_hash.encode() + unit.encode() + (tree / unit).read_bytes())
                target = tree / (key + ".o")
                if key not in objects:
                    command = [args.compiler, "-std=c++20", "-O2", "-DNDEBUG", "-Wall", "-Wextra", "-Wpedantic",
                               "-I", str(tree / "src"), "-c", str(tree / unit), "-o", str(target)]
                    run = subprocess.run(command, capture_output=True, text=True, timeout=240)
                    log.append("COMMAND " + json.dumps(command) + "\n" + run.stdout + run.stderr + "\nEXIT " + str(run.returncode))
                    if run.returncode:
                        (evidence / (name + "-build.log")).write_text("\n".join(log))
                        raise RuntimeError(name + " did not compile; this is NOT a mutation rejection")
                    objects[key] = target
                compiled.append(target)
            binary = tree / (name + "-gate")
            command = [args.compiler, *(str(p) for p in compiled), "-o", str(binary)]
            link = subprocess.run(command, capture_output=True, text=True, timeout=60)
            log.append("COMMAND " + json.dumps(command) + "\n" + link.stdout + link.stderr + "\nEXIT " + str(link.returncode))
            (evidence / (name + "-build.log")).write_text("\n".join(log))
            if link.returncode: raise RuntimeError(name + " did not link; not a mutation rejection")
            command = [str(binary), str(tree / CONTRACT), "--suite", suite]
            run = subprocess.run(command, capture_output=True, text=True, timeout=180)
            output = run.stdout + run.stderr
            (evidence / (name + "-run.log")).write_text("COMMAND " + json.dumps(command) + "\n" + output + "\nEXIT " + str(run.returncode))
            match = re.search(r"Ring event continuity: (\d+) assertions, (\d+) failures", output)
            row = dict(name=name, suite=suite, exit=run.returncode,
                       assertions=int(match[1]) if match else None, failures=int(match[2]) if match else None,
                       intended_assertion=expected, intended_assertion_seen=expected in output if expected else None)
            results.append(row)
            (evidence / "results.json").write_text(json.dumps(results, indent=2) + "\n")
            print(json.dumps(row), flush=True)
            if run.returncode != expected_exit or (expected and expected not in output):
                raise RuntimeError(name + " did not meet its intended rejection/result")

        compile_and_run("unmutated", "all")
        for case in selected:
            for path, original in originals.items(): (tree / path).write_text(original)
            patches = []
            for path, old, new in case["edits"]:
                file = tree / path
                before = file.read_text()
                if before.count(old) != 1: raise RuntimeError(case["name"] + " mutation anchor is not unique: " + path)
                after = before.replace(old, new)
                file.write_text(after)
                patches.extend(difflib.unified_diff(before.splitlines(True), after.splitlines(True), path, path))
            (evidence / (case["name"] + ".patch")).write_text("".join(patches))
            compile_and_run(case["name"], case["suite"], case["expected"], case["exit"])
        for path, original in originals.items(): (tree / path).write_text(original)
        compile_and_run("restored", "all")
    manifest = dict(registration=REGISTRATION, contract_sha256=sha(contract),
                    source_commit=source_commit,
                    compiler=subprocess.check_output([args.compiler, "--version"], text=True).splitlines()[0],
                    mutations=len(selected), results=results)
    (evidence / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
