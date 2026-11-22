#!/usr/bin/env python3
"""Run existing non-peer native recall fixtures with isolated preference storage."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument("--repo", type=Path, required=True)
parser.add_argument("--binary", type=Path, required=True)
parser.add_argument("--out", type=Path, required=True)
args = parser.parse_args()
repo, binary, out = args.repo.resolve(), args.binary.resolve(), args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env.pop("DISPLAY", None)
env["JUCE_FONT_PATH"] = "/usr/share/fonts/truetype/dejavu"
env["XDG_CONFIG_HOME"] = str(out / "config")
Path(env["XDG_CONFIG_HOME"]).mkdir(exist_ok=True)
flags = ["--modern-recall-host-tests", "--preset-file-recall-tests",
         "--preset-revert-tests", "--preset-identity-tests"]
results = []
for flag in flags:
    name = flag.removeprefix("--")
    env["CS80_FAVOURITES_FILE"] = str(out / (name + "-favourites.txt"))
    start = time.monotonic()
    with (out / (name + ".log")).open("w") as log:
        try:
            child = subprocess.run([str(binary), flag], cwd=repo, env=env,
                                   stdout=log, stderr=subprocess.STDOUT, timeout=120)
            code = child.returncode
        except subprocess.TimeoutExpired:
            code = "timeout"
    output = (out / (name + ".log")).read_text()
    match = re.search(r"(\d+) checks, (\d+) failures", output)
    results.append({"flag": flag, "exit_code": code,
                    "elapsed_seconds": time.monotonic() - start,
                    "checks": int(match[1]) if match else None,
                    "failures": int(match[2]) if match else None,
                    "log": name + ".log"})
    print(json.dumps(results[-1]), flush=True)
(out / "replay-results.json").write_text(json.dumps({
    "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
    "results": results}, indent=2) + "\n")
raise SystemExit(0 if all(r["exit_code"] == 0 and r["failures"] == 0
                         and r["checks"] for r in results) else 1)
