#!/usr/bin/env python3
"""Qualify the registered click-event gate with isolated, actual source mutations.

No repository source is edited. A consistent temporary source tree is compiled
for each mutation; nonzero compilation exits never count as mutation rejection.
The original phrase <= 1 requirements remain separate and unchanged.
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


CONTRACT = "tests/parity/tolerances-v2/click-residual-continuity.json"
TIMING = "tests/parity/tolerances-v2/cold-idle-timing.json"
ENGINE = "src/core/Engine.cpp"
WSC = "src/core/ReconstructedOscillator.h"
DRIVER = "tests/ClickEventContinuationTests.cpp"
COMPARATOR = "tests/ExactContinuationComparator.h"
REGISTERED = "d06648c386486cf2630de6e41195ddd5d4360ff0"


def mutations():
    assignment = "        const float legacyPhase = audioRng.nextUnipolar();"
    release = "        r.aenv.noteOff();\n        r.fenv.noteOff();"

    def on_assign(name, addition, expected):
        return dict(name=name, suite="tail", changes=[
            (ENGINE, assignment, assignment + "\n" + addition)], expected=[expected])

    cases = [on_assign("reset-active-tail-oscillator",
        "        if (preserveAudioState && wasActive) r.wsc.reset(0.0, 261.6);",
        "Bass rank I WSC residual survives zero-time sounding-tail retrigger")]
    for control, replacement in [
            ("frequency", "if(threshold!=state.heldThresholdVolts)"),
            ("threshold", "if(f!=state.heldFrequencyHz)")]:
        cases.append(dict(name="stale-cache-on-" + control, suite="wsc", changes=[
            (WSC, "if(f!=state.heldFrequencyHz||threshold!=state.heldThresholdVolts)", replacement)],
            expected=["WSC control-edge cached/uncached"]))
    cases.append(on_assign("clear-one-occupied-residual", """        if (preserveAudioState && wasActive)
        {
            auto altered = r.wsc.getState();
            bool cleared = false;
            for (auto& row : altered.residual)
                for (auto& value : row)
                    if (!cleared && value != 0.0) { value = 0.0; cleared = true; }
            r.wsc.restoreState(altered);
        }""", "Bass rank I WSC residual survives zero-time sounding-tail retrigger"))
    # Stronger single-field control: clear exactly one occupied entry across
    # the entire selected voice, even if both rank oscillators have residuals.
    preserve = "    const bool preserveEnvelope = preserveAudioState;"
    cases.append(dict(name="clear-single-residual-across-both-ranks", suite="tail", changes=[
        (ENGINE, preserve, preserve + """
    if (preserveAudioState && wasActive)
    {
        bool cleared = false;
        for (int rank = 0; rank < 2 && !cleared; ++rank)
        {
            auto& oscillator = v.ranks[rank].wsc;
            auto altered = oscillator.getState();
            for (auto& row : altered.residual)
                for (auto& value : row)
                    if (!cleared && value != 0.0) { value = 0.0; cleared = true; }
            if (cleared) oscillator.restoreState(altered);
        }
    }""")], expected=["WSC residual survives zero-time sounding-tail retrigger"]))
    for stage in ["hp", "lp"]:
        cases.append(on_assign("reset-rank-II-" + stage,
            f"        if (preserveAudioState && wasActive && ch == 1) r.{stage}Stage.reset();",
            "Bass rank II WSC residual survives zero-time sounding-tail retrigger"))
    for rank in [0, 1]:
        for capacitor in ["amplitude", "filter"]:
            field = "aenv" if capacitor == "amplitude" else "fenv"
            cases.append(dict(name=f"clear-rank-{rank + 1}-{capacitor}-charge", suite="noteoff",
                changes=[(ENGINE, release, f"        if (ch == {rank}) r.{field}.reset();\n" + release)],
                expected=[f"Funky{rank + 1} full zero-time release continuity"]))
    coupling = dict(name="clear-rank-II-vca-coupling", suite="noteoff", changes=[
        (ENGINE, release, "        if (ch == 1) r.vca.reset(r.vca.levelNode());\n" + release)],
        expected=["Funky2 full zero-time release continuity"])
    cases.append(coupling)
    cases.append(dict(name="skip-pending-idle-wake", suite="idle", cold_timing=True, changes=[
        (ENGINE, "    if (preserveAudioState && ! wasActive)\n        wakeIdleVoice(v);",
                 "    if (preserveAudioState && ! wasActive)\n        (void) v;")],
        expected=["Bass assignment accounts only independently counted elapsed samples"]))
    cases.append(dict(name="double-pending-idle-clock", suite="idle", cold_timing=True, changes=[
        (ENGINE, "                                  v.idleClock, v.idleSteps,",
                 "                                  2 * v.idleClock, v.idleSteps,")],
        expected=["Bass reference wake accounts only independently counted elapsed samples"]))
    cases.append(dict(name="wrong-Funky-card-for-rank-II", suite="noteoff", changes=[
        (ENGINE, "source_factory::compileTone(rank, selector, tones[rank])",
                 "source_factory::compileTone(0, selector, tones[rank])")],
        expected=["Funky2 compiled source identity"]))
    return cases, coupling


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--registration-sha", default=REGISTERED)
    parser.add_argument("--case", action="append", help="qualify only a named source mutation, with matching unmutated/restored fixtures")
    args = parser.parse_args()
    root, evidence = args.source.resolve(), args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    contract = (root / CONTRACT).read_bytes()
    registered = subprocess.check_output(["git", "show", args.registration_sha + ":" + CONTRACT], cwd=root)
    if contract.replace(b"\r\n", b"\n") != registered:
        raise RuntimeError("acceptance differs from separately committed registration")
    document = json.loads(contract)
    if document["status"] != "REGISTERED":
        raise RuntimeError("gate is not registered")
    gate = document["proposed_additive_gate"]
    if any(v != 0 for k, v in gate.items() if k.endswith("bit_mismatches_max")):
        raise RuntimeError("registered exact-state limits must remain zero")
    paths = [ENGINE, WSC, "src/core/Engine.h", DRIVER, COMPARATOR,
             "tests/Ws1Tolerances.h", "tests/ColdIdleTimingTests.cpp", CONTRACT, TIMING]
    originals = {p: (root / p).read_bytes() for p in paths}
    cases, coupling = mutations()
    if args.case:
        known = {case["name"] for case in cases}
        if set(args.case) - known:
            raise RuntimeError("unknown requested mutation: " + repr(set(args.case) - known))
        cases = [case for case in cases if case["name"] in args.case]
    baseline_suites = sorted({case["suite"] for case in cases}) if args.case else ["all"]
    results = []
    with tempfile.TemporaryDirectory(prefix="cs80-click-mutations-") as temporary:
        tree = Path(temporary)
        shutil.copytree(root / "src", tree / "src")
        for path, data in originals.items():
            target = tree / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        objects = {}

        def compile_gate(name, target=DRIVER):
            # Header fingerprint ensures every translation unit sees the same
            # source snapshot; this also avoids stale Engine layouts in probes.
            headers = sorted((tree / "src").rglob("*.h"))
            header_key = sha(b"".join(p.read_bytes() for p in headers))
            command_log = []
            units = [ENGINE, "src/core/FactoryPresets.cpp", target]
            compiled = []
            for unit in units:
                key = sha((header_key + unit).encode() + (tree / unit).read_bytes()
                          + (tree / COMPARATOR).read_bytes())
                output = tree / (key + ".o")
                if key not in objects:
                    command = [args.compiler, "-std=c++20", "-O2", "-DNDEBUG", "-Wall", "-Wextra",
                               "-Wpedantic", "-I", str(tree / "src"), "-c", str(tree / unit), "-o", str(output)]
                    run = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT, timeout=240)
                    command_log.append("COMMAND " + json.dumps(command) + "\n" + run.stdout
                                       + "\nEXIT " + str(run.returncode) + "\n")
                    if run.returncode:
                        (evidence / (name + "-build.log")).write_text("".join(command_log))
                        raise RuntimeError(name + " did not compile; this is not a mutation kill")
                    objects[key] = output
                compiled.append(output)
            binary = tree / (name + "-gate")
            command = [args.compiler, *(str(p) for p in compiled), "-o", str(binary)]
            run = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, timeout=60)
            command_log.append("COMMAND " + json.dumps(command) + "\n" + run.stdout
                               + "\nEXIT " + str(run.returncode) + "\n")
            (evidence / (name + "-build.log")).write_text("".join(command_log))
            if run.returncode:
                raise RuntimeError(name + " did not link; this is not a mutation kill")
            return binary

        def execute(name, suite="all", expected=(), cold=False, expect_survivor=False):
            binary = compile_gate(name, "tests/ColdIdleTimingTests.cpp" if cold else DRIVER)
            command = [str(binary), str(tree / TIMING)] if cold else [
                str(binary), str(tree / CONTRACT), str(tree / TIMING), suite]
            began = time.monotonic()
            run = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, timeout=600)
            (evidence / (name + "-run.log")).write_text(
                "COMMAND " + json.dumps(command) + "\n" + run.stdout
                + "\nEXIT " + str(run.returncode) + "\nSECONDS " + str(time.monotonic() - began) + "\n")
            match = re.search(r": (\d+) checks, (\d+) failures", run.stdout)
            if not match:
                raise RuntimeError(name + " did not finish its assertion gate")
            item = dict(name=name, suite="cold-timing" if cold else suite,
                        exit_code=run.returncode, checks=int(match[1]), failures=int(match[2]),
                        intended_assertions=list(expected),
                        intended_assertions_observed=all(s in run.stdout for s in expected),
                        seconds=time.monotonic() - began)
            if expect_survivor:
                if run.returncode or item["failures"]:
                    raise RuntimeError("rank-II omission audit no longer isolates the missing assertion")
                item["registration_rejected"] = True
                item["reason"] = "Deleting the rank-II assertion allowed an actual rank-II coupling reset to survive."
            elif expected:
                if run.returncode != 1 or not item["failures"] or not item["intended_assertions_observed"]:
                    raise RuntimeError(name + " did not reject its intended mechanism")
            elif run.returncode or item["failures"]:
                raise RuntimeError(name + " unmutated gate failed")
            results.append(item)
            print(json.dumps(item), flush=True)

        def apply_case(case):
            patches = []
            for path, old, new in case["changes"]:
                before = (tree / path).read_text()
                if before.count(old) != 1:
                    raise RuntimeError(case["name"] + " source anchor not unique: " + old)
                after = before.replace(old, new, 1)
                (tree / path).write_text(after)
                patches.extend(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                               fromfile="a/" + path, tofile="b/" + path))
            (evidence / (case["name"] + ".patch")).write_text("".join(patches))

        for suite in baseline_suites:
            execute("unmutated-baseline" + ("-" + suite if args.case else ""), suite)
        for case in cases:
            apply_case(case)
            execute(case["name"], case["suite"], case["expected"])
            if case.get("cold_timing"):
                execute(case["name"] + "-existing-cold-time", expected=[
                    "cold wake accounts only physically elapsed time"], cold=True)
            for path, _, _ in case["changes"]:
                (tree / path).write_bytes(originals[path])

        # The executable refuses a weakened numeric contract even when all
        # present production comparisons happen to return zero mismatches.
        if not args.case:
            altered = json.loads(contract)
            altered["proposed_additive_gate"]["wsc_full_exposed_state_bit_mismatches_max"] = 1
            (tree / CONTRACT).write_text(json.dumps(altered, indent=2) + "\n")
            execute("weaken-zero-bit-limit", "wsc", ["registered bit-continuity limits must remain exactly zero"])
            (tree / CONTRACT).write_bytes(contract)

        # A mutation of the test must not silently turn an actually bad source
        # green. Seed the already-killed rank-II defect, delete only its rank-II
        # boundary comparison, and reject that test registration when it passes.
        omission = dict(name="omit-rank-II-assertion-audit", changes=coupling["changes"] + [
            (DRIVER, '    compareExactly(result, tol, preII, exact::observe(*event, 0, 1),\n'
                     '                   "Funky2 full zero-time release continuity");',
                     '    // MUTANT: rank-II release continuity assertion omitted.')])
        if not args.case:
            apply_case(omission)
            execute(omission["name"], "noteoff", expect_survivor=True)
            for path, _, _ in omission["changes"]:
                (tree / path).write_bytes(originals[path])
        for suite in baseline_suites:
            execute("restored-baseline" + ("-" + suite if args.case else ""), suite)

    for path, data in originals.items():
        if (root / path).read_bytes() != data:
            raise RuntimeError("original repository source was modified during qualification: " + path)
    report = dict(registration_commit=args.registration_sha, contract_sha256=sha(contract),
                  selected_mutations=args.case or "all", runner_sha256=sha(Path(__file__).read_bytes()),
                  compiler=subprocess.check_output([args.compiler, "--version"], text=True).splitlines()[0],
                  source_sha256={p: sha(data) for p, data in originals.items()}, results=results,
                  scope="Exact exposed software-state continuation. Original phrase score <=1 and hardware sound remain separate.")
    (evidence / "qualification.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
