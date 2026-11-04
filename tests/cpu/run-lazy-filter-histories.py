#!/usr/bin/env python3
"""Qualify exact lazy companion history semantics in isolated temporary builds.

Nothing is written to the supplied source checkout or its build directories.
The normal driver compares the public stage with the committed eager oracle.
A separate, temporary header inserts a counter only in the reconstruction path;
that counter is absent from the shipping source. Mutations must compile and fail
the intended assertion gate, not merely crash or fail their build.
"""
from __future__ import annotations

import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


ORIGINAL_REGISTRATION = "29f482061801048e366983a4c5b5fb07d0e2679e"
CONTRACT_SHA256 = "cabde0c348d832690a306d9bdcf727261bf740053dd67b643d48f85b6c691001"
REFERENCE_SHA256 = "a0bed6a73d8824b08d403304a331eea15f6865035560bf04b9ccaea9b0192b9f"
PHASE_SHA256 = "26d7919d1dc8a1fe210d2bbc47798354b804476eaf6452221d51ef85c8171947"
COUNTER = "        ++::cs80_audit::companionReconstructions;\n"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def replace_once(text: str, before: str, after: str) -> str:
    if text.count(before) != 1:
        raise RuntimeError("Expected one reviewed source anchor: " + repr(before[:100]))
    return text.replace(before, after)


def instrument(header: str) -> str:
    lazy = "    State reconstructAudioCompanions() const noexcept\n    {\n"
    if lazy in header:
        return replace_once(header, lazy, lazy + COUNTER)
    eager = "            zIn = vIn + kHalfCoupling * gs / net.inputFarads * (u - vIn - vFb - vBp);\n"
    return replace_once(header, eager, "            ++::cs80_audit::companionReconstructions;\n" + eager)


def mutate(header: str, name: str) -> str:
    if name == "missing-export":
        return replace_once(header,
            "        return audioCompanionsPending ? reconstructAudioCompanions() : State { zIn, zFb, zBp, zLp };",
            "        return State { zIn, zFb, zBp, zLp };")
    if name == "coefficients-before-materialization":
        changed = replace_once(header,
            "        // Export the last sample with its original coefficients before they change.\n"
            "        materializeAudioCompanions();\n",
            "        // Deliberate test mutation: overwrite coefficients before export.\n")
        return replace_once(changed, "        ffPart = kHalf * ffRate;\n",
            "        ffPart = kHalf * ffRate;\n        materializeAudioCompanions();\n")
    if name == "reset-retains-pending":
        return replace_once(header,
            "    void reset() noexcept\n    {\n        audioCompanionsPending = false;\n",
            "    void reset() noexcept\n    {\n")
    if name == "eager-audio-reconstruction":
        return replace_once(header, "            audioCompanionsPending = true;\n",
            "            audioCompanionsPending = true;\n            materializeAudioCompanions();\n")
    if name == "restore-retains-pending":
        return replace_once(header,
            "        audioCompanionsPending = false;\n        zIn = histories.input;",
            "        zIn = histories.input;")
    if name == "copy-drops-pending":
        return replace_once(header,
            "    // Companion histories v + halfStep * dv/dt, volts at normalized scale.\n",
            "    Ig00156Stage(const Ig00156Stage& other) noexcept\n"
            "        : net(other.net), correction(other.correction)\n"
            "    {\n        *this = other;\n        audioCompanionsPending = false;\n    }\n"
            "    // Companion histories v + halfStep * dv/dt, volts at normalized scale.\n")
    if name == "restore-loses-input-history":
        return replace_once(header,
            "correction.restore({ vIn, vFb, vBp, vLp }, m.inputs, slope1, slope2, m.inputRate);",
            "correction.restore({ vIn, vFb, vBp, vLp }, {}, slope1, slope2, m.inputRate);")
    raise RuntimeError("Unknown mutation " + name)


def cmake_path(path: Path) -> str:
    # Bracket arguments keep spaces, backslashes and dollar signs literal.
    value = path.resolve().as_posix()
    if "]====]" in value:
        raise RuntimeError("Unsupported CMake path delimiter")
    return "[====[" + value + "]====]"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--candidate-header", type=Path)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--driver", type=Path)
    parser.add_argument("--efficiency-driver", type=Path)
    parser.add_argument("--registration-sha")
    parser.add_argument("--evidence", type=Path)
    parser.add_argument("--mode", choices=("baseline", "qualify", "efficiency"), default="qualify")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--cxx")
    arguments = parser.parse_args()
    source = arguments.source.resolve()
    package = Path(__file__).resolve().parents[2]
    reference = (arguments.reference or package / "tests/reference/Ig00156StageCompanionPrior.inc").resolve()
    driver = (arguments.driver or package / "tests/Ig00156CompanionHistoryTests.cpp").resolve()
    efficiency_driver = (arguments.efficiency_driver or package / "tests/Ig00156CompanionEfficiencyTests.cpp").resolve()
    header_path = (arguments.candidate_header or source / "src/core/Ig00156Filter.h").resolve()
    original_header = header_path.read_bytes()
    original_reference = reference.read_bytes()
    # Match Git's text blobs on LF and CRLF checkouts. Keep the raw originals
    # separately so the immutability check still rejects any in-place write.
    header_text = header_path.read_text(encoding="utf-8")
    reference_bytes = reference.read_text(encoding="utf-8").encode("utf-8")
    contract_bytes = arguments.contract.read_text(encoding="utf-8").encode("utf-8")
    if digest(contract_bytes) != CONTRACT_SHA256:
        raise RuntimeError("The registered contract changed; review and preregister before evaluating")
    if digest(reference_bytes) != REFERENCE_SHA256:
        raise RuntimeError("Frozen eager reference changed")
    if digest((source / "src/core/Ig00156PhaseCorrection.h").read_text(encoding="utf-8").encode("utf-8")) != PHASE_SHA256:
        raise RuntimeError("The shared phase kernel differs from the reviewed registration baseline")
    contract = json.loads(contract_bytes)
    if contract["maximum_output_difference"] != 0 or contract["maximum_exported_node_history_slope_difference"] != 0:
        raise RuntimeError("Exact zero limits must not change")
    if arguments.mode != "efficiency":
        if not arguments.registration_sha or not re.fullmatch(r"[0-9a-f]{40}", arguments.registration_sha):
            raise RuntimeError("Publish registration first, then supply its full --registration-sha")
        for relative, expected in (
            ("tests/cpu/lazy-filter-histories.json", contract_bytes),
            ("tests/reference/Ig00156StageCompanionPrior.inc", reference_bytes),
        ):
            committed = subprocess.run(["git", "show", arguments.registration_sha + ":" + relative],
                cwd=source, capture_output=True, check=True).stdout
            if committed != expected:
                raise RuntimeError("Published registration does not contain the reviewed " + relative)
    with tempfile.TemporaryDirectory(prefix="cs80-lazy-history-gate-") as temporary:
        temporary = Path(temporary)
        evidence = arguments.evidence.resolve() if arguments.evidence else temporary / "evidence"
        evidence.mkdir(parents=True, exist_ok=True)
        outcomes = []

        def command(args: list[str], log: Path, timeout: int = 180) -> subprocess.CompletedProcess[str]:
            result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout)
            log.write_text("COMMAND " + json.dumps(args) + "\nSTDOUT\n" + result.stdout
                + "\nSTDERR\n" + result.stderr + "\nEXIT " + str(result.returncode) + "\n", encoding="utf-8")
            return result

        def fixture(name: str, header: str, functional: bool = True, efficiency: bool = True) -> dict:
            tree = temporary / name
            tests = tree / "tests"
            (tests / "reference").mkdir(parents=True)
            (tree / "src/core").mkdir(parents=True)
            (tree / "instrumented/core").mkdir(parents=True)
            (tree / "src/core/Ig00156Filter.h").write_text(header, encoding="utf-8")
            (tree / "instrumented/core/Ig00156Filter.h").write_text(instrument(header), encoding="utf-8")
            (tests / "reference/Ig00156StageCompanionPrior.inc").write_bytes(reference_bytes)
            shutil.copyfile(driver, tests / driver.name)
            shutil.copyfile(efficiency_driver, tests / efficiency_driver.name)
            cmake = [
                "cmake_minimum_required(VERSION 3.22)",
                "project(Cs80LazyCompanionContract LANGUAGES CXX)",
                "set(CMAKE_CXX_STANDARD 20)",
                "set(CMAKE_CXX_STANDARD_REQUIRED ON)",
                "set(CMAKE_CXX_EXTENSIONS OFF)",
                'set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")',
                "foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)",
                '  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin")',
                "endforeach()",
            ]
            targets = []
            if functional:
                targets.append("companion_functional")
                cmake.extend([
                    "add_executable(companion_functional " + cmake_path(tests / driver.name) + ")",
                    "target_include_directories(companion_functional PRIVATE "
                        + " ".join(cmake_path(p) for p in (tree / "src", source / "src", source / "src/core")) + ")",
                ])
            if efficiency:
                targets.append("companion_efficiency")
                cmake.extend([
                    "add_executable(companion_efficiency " + cmake_path(tests / efficiency_driver.name) + ")",
                    "target_include_directories(companion_efficiency PRIVATE "
                        + " ".join(cmake_path(p) for p in (tree / "instrumented", source / "src", source / "src/core")) + ")",
                ])
            (tree / "CMakeLists.txt").write_text("\n".join(cmake) + "\n", encoding="utf-8")
            build = tree / "build"
            configure_args = [arguments.cmake, "-S", str(tree), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"]
            if arguments.cxx:
                configure_args.append("-DCMAKE_CXX_COMPILER=" + arguments.cxx)
            configured = command(configure_args, evidence / (name + "-configure.log"))
            if configured.returncode:
                raise RuntimeError(name + ": fixture configuration failed; this is not a detected mutation")
            compiled = command([arguments.cmake, "--build", str(build), "--target", *targets,
                                "--config", "Release", "--parallel", "1"], evidence / (name + "-build.log"))
            if compiled.returncode:
                raise RuntimeError(name + ": fixture build failed; this is not a detected mutation")
            extension = ".exe" if os.name == "nt" else ""
            result = {"name": name}
            if functional:
                ran = command([str(build / "bin" / ("companion_functional" + extension)), str(arguments.contract.resolve())],
                              evidence / (name + "-functional.log"))
                match = re.search(r"Companion histories: (\d+) checks, (\d+) failures", ran.stdout)
                if ran.returncode not in (0, 1) or match is None:
                    raise RuntimeError(name + ": functional gate did not finish with an assertion result")
                result["functional"] = {"exit": ran.returncode, "checks": int(match[1]), "failures": int(match[2])}
                if ran.returncode == 1 and ("FAIL:" not in ran.stderr or int(match[2]) == 0):
                    raise RuntimeError(name + ": missing functional assertion diagnostic")
            if efficiency:
                ran = command([str(build / "bin" / ("companion_efficiency" + extension))],
                              evidence / (name + "-efficiency.log"))
                if ran.returncode not in (0, 1):
                    raise RuntimeError(name + ": efficiency gate exited abnormally")
                result["efficiency"] = {"exit": ran.returncode, "counts": json.loads(ran.stdout)}
                if ran.returncode == 1 and "FAIL:" not in ran.stderr:
                    raise RuntimeError(name + ": missing efficiency assertion diagnostic")
            outcomes.append(result)
            print(json.dumps(result), flush=True)
            return result

        text = header_text
        baseline = fixture("baseline" if arguments.mode == "baseline" else "candidate", text,
                           functional=arguments.mode != "efficiency")
        if arguments.mode == "baseline":
            if baseline["functional"]["exit"] != 0 or baseline["efficiency"]["exit"] != 1:
                raise RuntimeError("Eager baseline must pass public semantics and fail the registered lazy-efficiency gate")
        elif baseline.get("functional", {}).get("exit", 0) != 0 or baseline["efficiency"]["exit"] != 0:
            raise RuntimeError("Candidate failed a registered gate")
        if arguments.mode == "qualify":
            for name in ("missing-export", "coefficients-before-materialization", "reset-retains-pending",
                         "eager-audio-reconstruction", "restore-retains-pending", "copy-drops-pending",
                         "restore-loses-input-history"):
                altered = mutate(text, name)
                (evidence / (name + ".patch")).write_text("".join(difflib.unified_diff(
                    text.splitlines(keepends=True), altered.splitlines(keepends=True),
                    fromfile="a/src/core/Ig00156Filter.h", tofile="b/src/core/Ig00156Filter.h")), encoding="utf-8")
                efficiency_mutation = name == "eager-audio-reconstruction"
                outcome = fixture(name, altered, functional=not efficiency_mutation, efficiency=efficiency_mutation)
                gate = "efficiency" if efficiency_mutation else "functional"
                if outcome[gate]["exit"] != 1:
                    raise RuntimeError(name + ": registered gate did not reject the deliberate defect")
        if header_path.read_bytes() != original_header:
            raise RuntimeError("Input candidate header changed during qualification")
        if reference.read_bytes() != original_reference:
            raise RuntimeError("Frozen reference changed during qualification")
        record = {
            "original_registration": ORIGINAL_REGISTRATION,
            "published_registration": arguments.registration_sha,
            "mode": arguments.mode,
            "candidate_sha256": digest(original_header),
            "candidate_normalized_sha256": digest(header_text.encode("utf-8")),
            "driver_sha256": digest(driver.read_bytes()),
            "efficiency_driver_sha256": digest(efficiency_driver.read_bytes()),
            "reference_sha256": digest(reference_bytes),
            "contract_sha256": digest(contract_bytes),
            "phase_sha256": PHASE_SHA256,
            "outcomes": outcomes,
            "boundary": "Fixed floating-point mode within each process/export sequence. Moving-coefficient reconstruction counts are reported, not promised to improve. No whole-engine CPU speedup is established here.",
        }
        (evidence / "qualification.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        print("Lazy companion qualification completed: " + arguments.mode, flush=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print("ERROR: " + str(error), file=sys.stderr)
        sys.exit(2)
