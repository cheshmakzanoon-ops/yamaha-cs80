#!/usr/bin/env python3
"""Re-link a configured GCC/Clang Ninja native build with private processor faults.

Requires the completed CS80Snapshots target from this checkout. Reuses its
unmodified objects; builds changed PluginProcessor objects/executables only in
an external temporary directory. It never edits the source or build objects.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    commands = json.loads((build / "compile_commands.json").read_text())
    compile_entry = next(x for x in commands if x["file"] == str(ROOT / "src/plugin/PluginProcessor.cpp")
                         and "CS80Snapshots.dir" in x["command"])
    compile_args = shlex.split(compile_entry["command"])
    original_object = compile_args[compile_args.index("-o") + 1]
    commands = subprocess.run([os.environ.get("NINJA", "ninja"), "-C", str(build), "-t", "commands", "CS80Snapshots"],
                              check=True, text=True, capture_output=True).stdout.splitlines()
    # CMake's Unix link rule is ': && <compiler> ... && :'. Fail rather than
    # interpret an unfamiliar generator command or silently test another target.
    tokens = shlex.split(commands[-1])
    assert tokens[:2] == [":", "&&"] and tokens[-2:] == ["&&", ":"]
    link_args = tokens[2:-2]
    assert original_object in link_args and "-o" in link_args
    original_exe = link_args[link_args.index("-o") + 1]
    source = (ROOT / "src/plugin/PluginProcessor.cpp").read_text()
    start = source.index("        if (message.getControllerNumber() == 121 && !engine.isParityMode())")
    end = source.index("        if (message.getControllerNumber() == 88)", start)
    block = source[start:end]
    variants = {}
    for name, old, new in [
        ("omit-expression-telemetry", "            expressionValue.store(1.0f, std::memory_order_relaxed);", "            // omitted"),
        ("omit-prefix-reset", "            highResolutionVelocity.resetChannel(message.getChannel());", "            // omitted"),
        ("break-pure-guard", " && !engine.isParityMode()", "")]:
        assert block.count(old) == 1
        variants[name] = source[:start] + block.replace(old, new) + source[end:]
    results = {}

    def probe(name, exe):
        result = subprocess.run([str(exe), "--controller-reset-tests"], text=True, capture_output=True)
        (args.output / (name + ".log")).write_text(result.stdout + result.stderr)
        match = re.findall(r"Controller reset host tests: (\d+) checks, (\d+) failures", result.stdout)
        assert len(match) == 1, (name, "no normal completion", result.returncode)
        checks, failures = map(int, match[0])
        assert result.returncode == (0 if name == "candidate" else 1)
        assert (failures == 0) == (name == "candidate")
        results[name] = {"checks": checks, "failures": failures, "exit": result.returncode}
        print(name, results[name], flush=True)

    probe("candidate", build / original_exe)
    with tempfile.TemporaryDirectory(prefix="cs80-reset-host-negative-") as directory:
        directory = Path(directory)
        for name, source in variants.items():
            file = directory / (name + ".cpp")
            file.write_text(source)
            obj, exe = directory / (name + ".o"), directory / name
            cc = [str(obj) if x == original_object else str(file) if x == compile_entry["file"] else x
                  for x in compile_args]
            cc += ["-I" + str(ROOT / "src/plugin")]
            subprocess.run(cc, cwd=compile_entry["directory"], check=True)
            link = [str(obj) if x == original_object else str(exe) if x == original_exe else x for x in link_args]
            subprocess.run(link, cwd=build, check=True)
            probe(name, exe)
    (args.output / "summary.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
