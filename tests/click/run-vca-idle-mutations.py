#!/usr/bin/env python3
"""Qualify VCA idle-output gates using consistent, temporary source mutations.

A compile error never counts as rejecting a defect. The original checkout is
read-only throughout; exact commands, patches, checks and logs are retained.
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
VCA = "src/core/Vca.h"
CONTRACT = "tests/parity/tolerances-v2/ws1-voice.json"
DRIVERS = ["tests/Ig00151VcaTests.cpp", "tests/Ig00151IdleTailTests.cpp"]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--compiler", default="c++")
    args = parser.parse_args()
    root, evidence = args.source.resolve(), args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=False)
    paths = [ENGINE, VCA, CONTRACT, "tests/Ws1Tolerances.h", *DRIVERS]
    originals = {p: (root / p).read_bytes() for p in paths}
    cases = [
        ("disconnect-idle-output", ENGINE,
         "const float closed = r.vca.processClosed();",
         "(void) r.vca.processClosed(); const float closed = 0.0f;", DRIVERS[1],
         "the selected rank's idle capacitor remains connected to output"),
        ("disconnect-rank-II-output", ENGINE,
         "const float voiceOut = closed * chGain * 0.17f;",
         "const float voiceOut = (ch == 1 ? 0.0f : closed) * chGain * 0.17f;", DRIVERS[1],
         "the selected rank's idle capacitor remains connected to output"),
        ("double-count-idle-discharge", ENGINE,
         "voice_continuity::idleStepRank(r, controls, sr, idleSamples, v.idleSteps);",
         "voice_continuity::idleStepRank(r, controls, sr, idleSamples, v.idleSteps);\n"
         "                    r.vca.relaxIdle(static_cast<double>(idleSamples) / sr);", DRIVERS[1],
         "coupling discharge conserves actual host elapsed time"),
        ("retain-charge-at-panic", ENGINE,
         "rank.vca.reset(0.0f);", "(void) rank.vca.levelNode();", DRIVERS[1],
         "explicit hard panic clears both coupling states exactly"),
        ("retain-charge-at-cold-reset", ENGINE,
         "if (freeRunning) r.vca.reset(0.0f);",
         "if (freeRunning) (void) r.vca.levelNode();", DRIVERS[1],
         "explicit engine reset clears charged coupling before cold idle output"),
        ("freeze-closed-charge", VCA,
         "coupling += (0.0 - coupling) * couplingCoefficient;",
         "coupling += 0.0;", DRIVERS[0],
         "closed VCA output equals analytical downstream coupling discharge at every sample"),
        ("attenuate-closed-output", VCA,
         "return static_cast<float>(-coupling);",
         "return static_cast<float>(-0.5 * coupling);", DRIVERS[0],
         "VCA active/idle handoff preserves its charged downstream capacitor continuously"),
        ("weaken-integration-contract", CONTRACT,
         '"closed_engine_coupling_truth_abs": 1e-12',
         '"closed_engine_coupling_truth_abs": 1e-3', DRIVERS[1],
         "registered integration limits remain unchanged"),
    ]
    results = []
    with tempfile.TemporaryDirectory(prefix="cs80-vca-idle-mutations-") as temporary:
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
            for unit in [ENGINE, "src/core/FactoryPresets.cpp", driver]:
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

        for i, driver in enumerate(DRIVERS):
            execute("unmutated-" + str(i), driver)
        for name, path, old, new, driver, expected in cases:
            text = originals[path].decode()
            if text.count(old) != 1:
                raise RuntimeError(name + " requires exactly one source anchor")
            mutated = text.replace(old, new)
            (tree / path).write_text(mutated)
            (evidence / (name + ".patch")).write_text("".join(difflib.unified_diff(
                text.splitlines(True), mutated.splitlines(True), fromfile=path, tofile=path)))
            execute(name, driver, expected)
            (tree / path).write_bytes(originals[path])
        for i, driver in enumerate(DRIVERS):
            execute("restored-" + str(i), driver)
    manifest = {"results": results, "source_sha256": {path: digest(data) for path, data in originals.items()},
                "source_commit": subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip(),
                "mutation_count": len(cases), "compile_errors_counted_as_rejection": False}
    (evidence / "qualification.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
