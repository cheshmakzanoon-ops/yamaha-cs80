#!/usr/bin/env python3
"""Read-only whole-Strings identity and substantive-control audit."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
BASELINE = ROOT / "docs/preset-evidence/2026.11.5/baseline/inventory.json"
CANDIDATE = ROOT / "docs/preset-evidence/2026.11.6/verification/a15/inventory.json"
baseline = json.loads(BASELINE.read_text())
candidate = json.loads(CANDIDATE.read_text())
assert hashlib.sha256(BASELINE.read_bytes()).hexdigest() == "0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611"
ids = candidate["parameter_ids"]
assert ids[:200] == baseline["parameter_ids"]
assert len(candidate["programs"]) == len(baseline["programs"]) == 523
rows = []
sound_vectors = []
for before, after in zip(baseline["programs"], candidate["programs"]):
    if after["category"] != "Strings":
        continue
    assert all(before[k] == after[k] for k in ("index", "host_index", "name", "category"))
    changes = {ids[i]: [old, new] for i, (old, new) in enumerate(zip(before["values"], after["values"])) if old != new and ids[i] != "volume"}
    assert changes, after["name"]
    values = dict(zip(ids, after["values"]))
    body = any(values[f"body{rank}_model"] and values[f"body{rank}_mix"] > 0 for rank in (1, 2))
    rows.append({"name": after["name"], "host_index": after["host_index"], "body_enabled": body, "changed_prior_nonvolume": changes})
    sound_vectors.append(tuple(v for k, v in values.items() if k != "volume"))
assert len(rows) == 59
assert len(set(sound_vectors)) == 59
assert all(after["values"][202:] == [0, 0] for after in candidate["programs"] if after["category"] == "Strings")
print(json.dumps({"strings": len(rows), "body_enabled": sum(row["body_enabled"] for row in rows), "unique_nonvolume_vectors": len(set(sound_vectors)), "all_names_and_addresses_preserved": True, "every_program_has_prior_nonvolume_change": True, "new_materials_neutral": True}, indent=2))
