#!/usr/bin/env python3
"""Compile source faults privately; require behavioral failures, never build/crash failures.

Run manually (compiles seven Engine variants):
  python3 tests/test_controller_reset_negative_controls.py --output /tmp/reset-negative
The unchanged base Engine is paired with the current, behavior-preserving CC88
helper header so the expanded standalone tests compile. No working source is edited.
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
BASE = "6c560f130be649dd4a63cbe146cd5cc54dd057a3"


def run(argv, **kwargs):
    return subprocess.run(argv, check=True, text=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    flags = ["-std=c++20", "-O2", "-DCS80_DISABLE_AVX2_FILTER_BATCH=1",
             "-I" + str(ROOT / "src"), "-I" + str(ROOT / "src/core")]
    source = (ROOT / "src/core/Engine.cpp").read_text()
    start = source.index("    if (controller == 121)")
    end = source.index("    // Ribbon from a MIDI CC", start)
    block = source[start:end]
    variants = {"candidate": source,
                "base-engine": run(["git", "-C", str(ROOT), "show", BASE + ":src/core/Engine.cpp"],
                                   capture_output=True).stdout}
    for name, target in [("omit-mod", "            modWheel = 0.0f;"),
                         ("omit-expression", "            expression = 1.0f;"),
                         ("omit-bend", "            pitchBend(midiChannel, 0.0f);"),
                         ("omit-pressure", "            channelPressure(midiChannel, 0.0f);"),
                         ("break-pure", "        if (! global.parityMode)")]:
        assert block.count(target) == 1
        replacement = "        if (true)" if name == "break-pure" else "            // intentionally omitted"
        variants[name] = source[:start] + block.replace(target, replacement) + source[end:]
    results = {}
    with tempfile.TemporaryDirectory(prefix="cs80-reset-negative-") as tmp:
        tmp = Path(tmp)
        objects = []
        for src in ["tests/ControllerResetTests.cpp", "src/core/FactoryPresets.cpp", "src/core/Ig00156Batch.cpp"]:
            obj = tmp / (Path(src).stem + ".o")
            run(compiler + flags + ["-c", str(ROOT / src), "-o", str(obj)])
            objects.append(str(obj))
        for name, text in variants.items():
            src = tmp / (name + ".cpp")
            src.write_text(text)
            exe = tmp / name
            run(compiler + flags + [str(src)] + objects + ["-o", str(exe)])
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            (args.output / (name + ".log")).write_text(result.stdout + result.stderr)
            matches = re.findall(r"Controller reset: (\d+) checks, (\d+) failures", result.stdout)
            assert len(matches) == 1, (name, "missing normal completion marker", result.returncode)
            checks, failures = map(int, matches[0])
            expected = 0 if name == "candidate" else 1
            assert result.returncode == expected and ((failures == 0) == (name == "candidate")), (name, result.returncode, failures)
            results[name] = {"checks": checks, "failures": failures, "exit": result.returncode}
            print(name, results[name], flush=True)
    (args.output / "summary.json").write_text(json.dumps({"base_engine": BASE, "results": results}, indent=2) + "\n")


if __name__ == "__main__":
    main()
