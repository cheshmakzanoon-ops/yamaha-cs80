#!/usr/bin/env python3
"""Mutation-check current-sample tail pan, time, identity and policy gates.

All cases run the complete registered matrix against production source copied
to an isolated temporary tree. A compile error is never a rejected mutation.
"""
from pathlib import Path
import argparse
import difflib
import hashlib
import json
import re
import shutil
import subprocess
import tempfile
import time

ENGINE = "src/core/Engine.cpp"
MODERN = "src/core/ModernLayer.h"
CONTRACT = "tests/parity/tolerances-v2/vca-tail-modulation.json"
DRIVER = "tests/VcaTailModulationTests.cpp"
DIRECT = "tests/ModernTailSchedulingTests.cpp"
PAN = """(modern.hasPanDestinations()
                    || modern.rankOffsets(vi, 0).pan != 0.0f
                    || modern.rankOffsets(vi, 1).pan != 0.0f)"""
EARLY = """            if (freeRunning && modern.isActive()
                && (modern.hasPanDestinations()
                    || modern.rankOffsets(vi, 0).pan != 0.0f
                    || modern.rankOffsets(vi, 1).pan != 0.0f)
                && (v.ranks[0].vca.hasCouplingTail() || v.ranks[1].vca.hasCouplingTail()))
            {
                // Tail pan needs this sample's modulation. Predict the original
                // idle deadline without changing its clock or ownership cadence.
                const bool scheduled = v.idleClock + 1 >= due;
                const bool releasedControls = scheduled || v.idleSteps > 0;
                modern.processVoice(vi,
                    { v.age, v.held || v.pedalHeld, v.note, v.velocity,
                      releasedControls ? 0.0f : v.pressure, releasedControls ? 0.0f : v.timbre, v.contour,
                      { v.ranks[0].info.filterEnvelope, v.ranks[1].info.filterEnvelope },
                      { v.ranks[0].info.ampEnvelope, v.ranks[1].info.ampEnvelope } }, scheduled);
                modernUpdatedForTail = true;
            }
"""
DEADLINE = "            if (! freeRunning || ++v.idleClock < due)"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise RuntimeError("mutation requires exactly one source anchor: " + old)
    return source.replace(old, new)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--compiler", default="c++")
    args = parser.parse_args()
    root, evidence = args.source.resolve(), args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=False)
    paths = [ENGINE, MODERN, CONTRACT, "tests/Ws1Tolerances.h", DRIVER, DIRECT]
    originals = {p: (root / p).read_bytes() for p in paths}
    cases = [
        ("omit-early-pan", ENGINE,
         "if (freeRunning && modern.isActive()",
         "if (false && freeRunning && modern.isActive()", DRIVER,
         "retired rank pan is from the current modulation sample"),
        ("consume-pan-before-update", ENGINE, None, None,
         DRIVER, "retired rank mix consumes current pan before downstream processing"),
        ("double-tick-idle-deadline", ENGINE,
         "if (! modernUpdatedForTail)", "if (true)", DRIVER,
         "tail poly MSEG advances once per actual internal sample"),
        ("publish-unscheduled-owner", MODERN,
         "if (scheduled && in.age >= newestThisSample)",
         "if (in.age >= newestThisSample)", DRIVER,
         "tail pan updates preserve scheduled newest-voice global selection"),
        ("latch-unscheduled-note", MODERN,
         "if (scheduled && in.age != voice.age)", "if (in.age != voice.age)", DRIVER,
         "late pan enable preserves note identity RNG and global selection timing"),
        ("run-tail-pan-in-legacy", ENGINE,
         "if (freeRunning && modern.isActive()",
         "if (modern.isActive()", DRIVER,
         "Legacy inactive path does not evaluate source coupling-tail pan"),
        ("enable-pure-matrix", ENGINE,
         "controls.enabled = ! isPureCs80(global.renderPolicy);", "controls.enabled = true;", DRIVER,
         "Pure nonzero matrix remains bit-identical to empty matrix"),
        ("weaken-elapsed-time-bound", CONTRACT,
         '"poly_elapsed_seconds_max_abs": 1e-12',
         '"poly_elapsed_seconds_max_abs": 1e-3', DRIVER,
         "registered poly elapsed-time bound remains unchanged"),
        ("ignore-removed-pan-cache", ENGINE, PAN, "modern.hasPanDestinations()", DRIVER,
         "removing zeroing or rerouting pan clears its cache at the next tail sample"),
        ("publish-unscheduled-gate", MODERN,
         "if (scheduled)\n            anyGateNow = anyGateNow || in.gate;",
         "anyGateNow = anyGateNow || in.gate;", DIRECT,
         "extra tail updates do not publish held-gate state between scheduled calls"),
        ("trigger-unscheduled-mono", MODERN,
         "if (scheduled)\n            anyGateNow = anyGateNow || in.gate;",
         "if (scheduled)\n            anyGateNow = anyGateNow || in.gate;\n"
         "        if (!scheduled && in.age != voice.age && !anyGateLast) monoTriggerPending = true;", DIRECT,
         "unscheduled unlatched tail never triggers the shared mono MSEG early"),
        ("omit-controller-clearing", ENGINE,
         "const bool releasedControls = scheduled || v.idleSteps > 0;",
         "const bool releasedControls = false;", DRIVER,
         "tail mix consumes released-controller clearing on the original deadline"),
        ("revive-expression-between-deadlines", ENGINE,
         "const bool releasedControls = scheduled || v.idleSteps > 0;",
         "const bool releasedControls = scheduled;", DRIVER,
         "released expression stays cleared from its original idle deadline onward"),
        ("advance-active-no-pan-tail", ENGINE, PAN, "true", DRIVER,
         "active modern routes without present or cached pan keep their original idle cadence"),
    ]
    results = []
    with tempfile.TemporaryDirectory(prefix="cs80-vca-tail-modulation-mutations-") as temporary:
        tree = Path(temporary)
        shutil.copytree(root / "src", tree / "src")
        for path, data in originals.items():
            target = tree / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        objects = {}

        def execute(name, driver, expected=None):
            headers = sorted((tree / "src").rglob("*.h"))
            header_hash = digest(b"".join(p.read_bytes() for p in headers))
            compiled, build_log = [], []
            units = [driver] if driver == DIRECT else [ENGINE, "src/core/FactoryPresets.cpp", driver]
            for unit in units:
                key = digest(header_hash.encode() + unit.encode() + (tree / unit).read_bytes())
                obj = tree / (key + ".o")
                if key not in objects:
                    command = [args.compiler, "-std=c++20", "-O3", "-DNDEBUG", "-I", str(tree / "src"),
                               "-c", str(tree / unit), "-o", str(obj)]
                    run = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT, timeout=300)
                    build_log.append("COMMAND " + json.dumps(command) + "\n" + run.stdout + "\nEXIT " + str(run.returncode))
                    if run.returncode:
                        (evidence / (name + "-build.log")).write_text("\n".join(build_log))
                        raise RuntimeError(name + " failed to compile; not a mutation rejection")
                    objects[key] = obj
                compiled.append(obj)
            binary = tree / (name + "-gate")
            command = [args.compiler, *(str(p) for p in compiled), "-o", str(binary)]
            run = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
            build_log.append("COMMAND " + json.dumps(command) + "\n" + run.stdout + "\nEXIT " + str(run.returncode))
            (evidence / (name + "-build.log")).write_text("\n".join(build_log) + "\n")
            if run.returncode:
                raise RuntimeError(name + " failed to link; not a mutation rejection")
            command = [str(binary), str(tree / CONTRACT)]
            started = time.monotonic()
            run = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
            (evidence / (name + "-run.log")).write_text("COMMAND " + json.dumps(command) + "\n" + run.stdout
                                                       + "\nEXIT " + str(run.returncode) + "\n")
            summary = re.search(r"(\d+) checks, (\d+) failures", run.stdout)
            if summary is None:
                raise RuntimeError(name + " did not complete its assertions")
            item = dict(name=name, driver=driver, exit_code=run.returncode, checks=int(summary[1]),
                        failures=int(summary[2]), seconds=time.monotonic() - started,
                        expected_failure=expected, expected_observed=expected is None or expected in run.stdout)
            if expected is None:
                if run.returncode or item["failures"]:
                    raise RuntimeError(name + " unmodified control failed")
            elif run.returncode != 1 or not item["failures"] or not item["expected_observed"]:
                raise RuntimeError(name + " mutation survived its intended gate")
            results.append(item)
            print(json.dumps(item), flush=True)

        for index, driver in enumerate([DRIVER, DIRECT]):
            execute("unmutated-" + str(index), driver)
        for name, path, old, new, driver, expected in cases:
            source = originals[path].decode()
            if name == "consume-pan-before-update":
                mutated = replace_once(source, EARLY, "")
                mutated = replace_once(mutated, DEADLINE, EARLY + DEADLINE)
            else:
                mutated = replace_once(source, old, new)
            (tree / path).write_text(mutated)
            (evidence / (name + ".patch")).write_text("".join(difflib.unified_diff(
                source.splitlines(True), mutated.splitlines(True), fromfile=path, tofile=path)))
            execute(name, driver, expected)
            (tree / path).write_bytes(originals[path])
        for index, driver in enumerate([DRIVER, DIRECT]):
            execute("restored-" + str(index), driver)
    manifest = {"results": results, "source_sha256": {path: digest(data) for path, data in originals.items()},
                "source_commit": subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip(),
                "mutation_count": len(cases), "complete_selected_fixture_for_every_case": True,
                "compile_errors_counted_as_rejection": False}
    (evidence / "qualification.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
