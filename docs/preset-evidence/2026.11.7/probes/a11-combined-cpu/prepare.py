#!/usr/bin/env python3
"""Export isolated copies; production headers are never modified.

Usage: python3 prepare.py REPO PRE_A07_CORE OUTPUT
The retained pre-a07.patch can recreate PRE_A07_CORE from old revision below.
"""
import difflib
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

repo, pre, output = map(Path, sys.argv[1:])
revision = "18711f93"
files = ["InstrumentBody.h", "InstrumentBodyVariants.h", "InstrumentBodyMaterials.h"]
receipt = {"baseline_revision": revision, "headers": {}}
for version, namespace in [("old", "cs80old"), ("pre", "cs80pre"), ("new", "cs80new")]:
    directory = output / version
    directory.mkdir(parents=True, exist_ok=True)
    receipt["headers"][version] = {}
    for filename in files:
        if version == "old":
            raw = subprocess.check_output(["git", "show", f"{revision}:src/core/{filename}"], cwd=repo)
        else:
            raw = ((pre if version == "pre" else repo / "src/core") / filename).read_bytes()
        receipt["headers"][version][filename] = hashlib.sha256(raw).hexdigest()
        text = re.sub(r"\bcs80\b", namespace, raw.decode())
        if filename == "InstrumentBody.h":
            text = text.replace("    bool enabled() const noexcept", "    int probeModeCount() const noexcept { return count; }\n\n    bool enabled() const noexcept")
        (directory / filename).write_text(text)
old = subprocess.check_output(["git", "show", f"{revision}:src/core/InstrumentBody.h"], cwd=repo).decode()
delta = "".join(difflib.unified_diff(old.splitlines(True), (pre / "InstrumentBody.h").read_text().splitlines(True), fromfile="a/InstrumentBody.h", tofile="b/InstrumentBody.h"))
(Path(__file__).parent / "pre-a07.patch").write_text(delta)
(Path(__file__).parent / "snapshots.json").write_text(json.dumps(receipt, indent=2)+"\n")
print(json.dumps(receipt))
