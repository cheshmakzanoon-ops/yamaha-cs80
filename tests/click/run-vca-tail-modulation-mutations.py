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
EARLY = """            if (freeRunning && modern.hasPanDestinations()
                && (v.ranks[0].vca.hasCouplingTail() || v.ranks[1].vca.hasCouplingTail()))
            {
                // Tail pan needs this sample's modulation. Predict the original
                // idle deadline without changing its clock or ownership cadence.
                const bool scheduled = v.idleClock + 1 >= due;
                if (scheduled)
                    v.pressure = v.timbre = v.mpeBend = 0.0f;
                modern.processVoice(vi,
                    { v.age, v.held || v.pedalHeld, v.note, v.velocity, v.pressure, v.timbre, v.contour,
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
    paths = [ENGINE, MODERN, CONTRACT, "tests/Ws1Tolerances.h", DRIVER]
    originals = {p: (root / p).read_bytes() for p in paths}
    cases = [
        ("omit-early-pan", ENGINE,
         "if (freeRunning && modern.hasPanDestinations()",
         "if (false && freeRunning && modern.hasPanDestinations()",
         "retired rank pan is from the current modulation sample"),
        ("consume-pan-before-update", ENGINE, None, None,
         "retired rank mix consumes current pan before downstream processing"),
        ("double-tick-idle-deadline", ENGINE,
         "if (! modernUpdatedForTail)", "if (true)",
         "tail poly MSEG advances once per actual internal sample"),
        ("publish-unscheduled-owner", MODERN,
         "if (scheduled && in.age >= newestThisSample)",
         "if (in.age >= newestThisSample)",
         "tail pan updates preserve scheduled newest-voice global selection"),
        ("latch-unscheduled-note", MODERN,
         "if (scheduled && in.age != voice.age)", "if (in.age != voice.age)",
         "late pan enable preserves note identity RNG and global selection timing"),
        ("run-tail-pan-in-legacy", ENGINE,
         "if (freeRunning && modern.hasPanDestinations()",
         "if (modern.hasPanDestinations()",
         "Legacy inactive path does not evaluate source coupling-tail pan"),
        ("enable-pure-matrix", ENGINE,
         "controls.enabled = ! isPureCs80(global.renderPolicy);", "controls.enabled = true;",
         "Pure nonzero matrix remains bit-identical to empty matrix"),
        ("weaken-elapsed-time-bound", CONTRACT,
         '"poly_elapsed_seconds_max_abs": 1e-12',
         '"poly_elapsed_seconds_max_abs": 1e-3',
         "registered poly elapsed-time bound remains unchanged"),
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

        def execute(name, expected=None):
            headers = sorted((tree / "src").rglob("*.h"))
            header_hash = digest(b"".join(p.read_bytes() for p in headers))
            compiled, build_log = [], []
            for unit in [ENGINE, "src/core/FactoryPresets.cpp", DRIVER]:
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
            item = dict(name=name, driver=DRIVER, exit_code=run.returncode, checks=int(summary[1]),
                        failures=int(summary[2]), seconds=time.monotonic() - started,
                        expected_failure=expected, expected_observed=expected is None or expected in run.stdout)
            if expected is None:
                if run.returncode or item["failures"]:
                    raise RuntimeError(name + " unmodified control failed")
            elif run.returncode != 1 or not item["failures"] or not item["expected_observed"]:
                raise RuntimeError(name + " mutation survived its intended gate")
            results.append(item)
            print(json.dumps(item), flush=True)

        execute("unmutated")
        for name, path, old, new, expected in cases:
            source = originals[path].decode()
            if name == "consume-pan-before-update":
                mutated = replace_once(source, EARLY, "")
                mutated = replace_once(mutated, DEADLINE, EARLY + DEADLINE)
            else:
                mutated = replace_once(source, old, new)
            (tree / path).write_text(mutated)
            (evidence / (name + ".patch")).write_text("".join(difflib.unified_diff(
                source.splitlines(True), mutated.splitlines(True), fromfile=path, tofile=path)))
            execute(name, expected)
            (tree / path).write_bytes(originals[path])
        execute("restored")
    manifest = {"results": results, "source_sha256": {path: digest(data) for path, data in originals.items()},
                "source_commit": subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip(),
                "mutation_count": len(cases), "complete_fixture_for_every_case": True,
                "compile_errors_counted_as_rejection": False}
    (evidence / "qualification.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
