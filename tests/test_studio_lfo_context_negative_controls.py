#!/usr/bin/env python3
"""Private source-fault relinks of a completed GCC/Clang Ninja CS80Snapshots build.

No working source/build object is edited. A mutant must compile, link, finish
normally and report behavioral failures; a crash is not counted as detection.
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
    raw = subprocess.run([os.environ.get("NINJA", "ninja"), "-C", str(build), "-t", "commands", "CS80Snapshots"],
                         check=True, text=True, capture_output=True).stdout.splitlines()
    tokens = shlex.split(raw[-1])
    assert tokens[:2] == [":", "&&"] and tokens[-2:] == ["&&", ":"]
    link_args = tokens[2:-2]
    original_exe = link_args[link_args.index("-o") + 1]
    faults = [
        ("omit-chip-binding", "AdvancedPanel.cpp",
         "        bindParameterContext(proc, Cs80AudioProcessor::parameterId(cs80::P_lfo1Dest + lfoIndex * 3));",
         "        // intentionally omitted", None),
        ("misbind-chip-group", "AdvancedPanel.cpp",
         "        bindParameterContext(proc, Cs80AudioProcessor::parameterId(cs80::P_lfo1Dest + lfoIndex * 3));",
         "        bindParameterContext(proc, Cs80AudioProcessor::parameterId(cs80::P_lfo1Dest + ((lfoIndex + 1) % 3) * 3));", None),
        ("dispatch-context-as-click", "ParameterContextButton.cpp",
         "if (e.mods.isPopupMenu() && onContextMenu)", "if (false)", None),
        ("omit-lit-click-off", "AdvancedPanel.cpp",
         "(destination != 0 && current == destination) ? 0.0f : (float) destination", "(float) destination", None),
    ]
    results = {}

    def probe(name, exe):
        result = subprocess.run([str(exe), "--studio-lfo-context-tests"], text=True, capture_output=True, timeout=120)
        (args.output / (name + ".log")).write_text(result.stdout + result.stderr)
        match = re.findall(r"Studio LFO context tests: (\d+) checks, (\d+) failures", result.stdout)
        assert len(match) == 1, (name, "no normal completion", result.returncode)
        checks, failures = map(int, match[0])
        assert result.returncode == (0 if name == "candidate" else 1), (name, result.returncode)
        assert (failures == 0) == (name == "candidate"), (name, failures)
        results[name] = {"checks": checks, "failures": failures, "exit": result.returncode}
        print(name, results[name], flush=True)

    probe("candidate", build / original_exe)
    with tempfile.TemporaryDirectory(prefix="cs80-studio-lfo-context-negative-") as tmp:
        tmp = Path(tmp)
        for name, filename, old, new, occurrence in faults:
            source = ROOT / "src/plugin" / filename
            entry = next(x for x in commands if x["file"] == str(source) and "CS80Snapshots.dir" in x["command"])
            cc = shlex.split(entry["command"])
            original_object = cc[cc.index("-o") + 1]
            assert original_object in link_args
            text = source.read_text()
            if occurrence is None:
                assert text.count(old) == 1, (name, text.count(old))
                changed = text.replace(old, new)
            else:
                assert text.count(old) == 3, (name, text.count(old))
                at = [m.start() for m in re.finditer(re.escape(old), text)][occurrence]
                changed = text[:at] + new + text[at + len(old):]
            mutant, obj, exe = tmp / (name + ".cpp"), tmp / (name + ".o"), tmp / name
            mutant.write_text(changed)
            compile_args = [str(obj) if x == original_object else str(mutant) if x == str(source) else x for x in cc]
            compile_args += ["-I" + str(ROOT / "src/plugin")]
            subprocess.run(compile_args, cwd=entry["directory"], check=True, timeout=180)
            link = [str(obj) if x == original_object else str(exe) if x == original_exe else x for x in link_args]
            subprocess.run(link, cwd=build, check=True, timeout=180)
            probe(name, exe)
    (args.output / "summary.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
