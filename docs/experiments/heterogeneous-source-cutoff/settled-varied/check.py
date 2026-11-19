#!/usr/bin/env python3
"""Recheck retained integer guest counts. Neither loads a plugin nor reads a clock."""
from pathlib import Path
import gzip, importlib.util, json, tempfile

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("count_parser", here/"count-parser.py")
parser = importlib.util.module_from_spec(spec)
spec.loader.exec_module(parser)
decision = json.loads((here/"decision.json").read_text())
dumps = json.loads(gzip.decompress((here/"selected-dumps.json.gz").read_bytes()))
arms = {"A":4, "A2":4, "P":5, "S":1, "C":4}
if set(dumps) != set(arms):
    raise ValueError("missing or additional dump")
with tempfile.TemporaryDirectory() as tmp:
    counts = {}
    for arm, expected in arms.items():
        p = Path(tmp)/arm
        p.write_text(dumps[arm])
        counts[arm] = parser.parse(p, expected)
if json.loads(json.dumps(counts)) != decision["genuine_entries"]:
    raise ValueError("guest counts do not reproduce the archived decision")
for metric in ("plugin_edge", "route_inclusive", "stub_plus_plugin"):
    t = {arm: data[metric] for arm, data in counts.items()}
    if t["A"] != t["A2"] or list(t["P"]) != [a+b for a,b in zip(t["A"],t["S"])]:
        raise ValueError("equal-work or positive control failed: "+metric)
print("All five separately preserved guest dumps and both count controls match.")
