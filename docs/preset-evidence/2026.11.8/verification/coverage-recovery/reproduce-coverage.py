#!/usr/bin/env python3
"""Reproduce recipe/mechanism scope from retained and freshly captured inventories.

This compares parameter data; it does not measure audio or listening quality.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import subprocess


def identity(data):
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--capture-receipt", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    inputs = {}

    def retained(path):
        data = subprocess.check_output(["git", "-C", str(args.repo), "show", args.source + ":" + path])
        blob = subprocess.check_output(["git", "-C", str(args.repo), "rev-parse", args.source + ":" + path], text=True).strip()
        inputs[path] = {**identity(data), "git_blob": blob, "kind": "retained_git_bytes"}
        return json.loads(data)

    old = retained("docs/preset-evidence/2026.11.7/development/published-4-inventory.json")
    previous = retained("docs/preset-evidence/2026.11.7/verification/a12/inventory.json")
    data = args.inventory.read_bytes()
    current = json.loads(data)
    receipt_data = args.capture_receipt.read_bytes()
    receipt = json.loads(receipt_data)
    assert receipt["source_commit"] == args.source
    assert receipt["status"] == "complete" and receipt["inventory"]["exit_code"] == 0
    assert receipt["inventory"]["sha256"] == identity(data)["sha256"]
    assert receipt["inventory"]["bytes"] == len(data)
    assert receipt["tracked_source_clean_before"] and receipt["tracked_source_clean_after"]
    assert current["library_revision"] == "2026.11.8"
    assert previous["library_revision"] == "2026.11.7" and old["library_revision"] == "2026.11.4"
    assert len(current["programs"]) == len(previous["programs"]) == len(old["programs"]) == 523
    assert current["parameter_ids"] == previous["parameter_ids"]
    assert len(current["parameter_ids"]) == 204 and len(old["parameter_ids"]) == 200
    inputs["current_inventory"] = {**identity(data), "kind": "fresh_independent_capture"}
    inputs["capture_receipt"] = identity(receipt_data)

    body_source = subprocess.check_output(["git", "-C", str(args.repo), "show", args.source + ":src/core/InstrumentBody.h"])
    assert hashlib.sha256(body_source).hexdigest() == receipt["source_files"]["src/core/InstrumentBody.h"]["sha256"]
    assert b"off = 0, piano, tine, reed, clav" in body_source
    assert b"model == tine && material != kalimba" in body_source
    material_source = subprocess.check_output(["git", "-C", str(args.repo), "show", args.source + ":src/core/InstrumentBodyMaterials.h"])
    assert b"kalimba = 6" in material_source
    inputs["src/core/InstrumentBody.h"] = identity(body_source)
    inputs["src/core/InstrumentBodyMaterials.h"] = identity(material_source)

    ids = current["parameter_ids"]
    common = set(old["parameter_ids"]) & set(ids)
    old_nonmaster = common - {"volume"}
    old_beyond_base_vca = old_nonmaster - {"ch1_v_level", "ch2_v_level"}
    rows = []
    for earlier, baseline, row in zip(old["programs"], previous["programs"], current["programs"]):
        address = (row["index"], row["host_index"], row["name"], row["category"])
        assert address == (baseline["index"], baseline["host_index"], baseline["name"], baseline["category"])
        assert address[:3] == (earlier["index"], earlier["host_index"], earlier["name"])
        assert len(row["values"]) == len(baseline["values"]) == 204
        values = dict(zip(ids, row["values"]))
        old_values = dict(zip(old["parameter_ids"], earlier["values"]))
        changed = [key for key, a, b in zip(ids, baseline["values"], row["values"]) if a != b]
        old_changes = sorted(key for key in old_nonmaster if values[key] != old_values[key])
        mechanism_ranks = []
        for rank in (1, 2):
            model = int(values[f"body{rank}_model"])
            material = int(values[f"body{rank}_material"])
            if values[f"body{rank}_mix"] > 0 and (model in (1, 3) or model == 2 and material != 6):
                mechanism_ranks.append({"rank": rank, "model": model, "material": material})
        is_init = row["category"] == "Init"
        if is_init:
            assert not changed and not old_changes and not mechanism_ranks
        rows.append({"host_index": row["host_index"], "name": row["name"], "category": row["category"],
                     "init": is_init, "changed_ids_since_7": changed, "old_nonmaster_changes_since_4": old_changes,
                     "old_changes_beyond_base_vca": bool(set(old_changes) & old_beyond_base_vca),
                     "piano_ep_mechanism_ranks": mechanism_ranks})
    vector = {r["host_index"] for r in rows if r["changed_ids_since_7"]}
    mechanisms = {r["host_index"] for r in rows if r["piano_ep_mechanism_ranks"]}
    musical = [r for r in rows if not r["init"]]
    counts = {"programs": len(rows), "musical": len(musical), "init": len(rows) - len(musical),
              "changed_vectors_since_7": len(vector), "piano_ep_mechanism_programs": len(mechanisms),
              "piano_ep_mechanism_ranks": sum(len(r["piano_ep_mechanism_ranks"]) for r in rows),
              "overlap": len(vector & mechanisms), "directly_affected_unique_programs": len(vector | mechanisms),
              "changed_old_nonmaster_controls_since_4": sum(bool(r["old_nonmaster_changes_since_4"]) for r in musical),
              "changed_other_old_controls_beyond_base_vca_since_4": sum(r["old_changes_beyond_base_vca"] for r in musical),
              "base_vca_only_among_old_nonmaster_controls_since_4": sum(bool(r["old_nonmaster_changes_since_4"]) and not r["old_changes_beyond_base_vca"] for r in musical)}
    assert list(counts.values()) == [523, 521, 2, 31, 28, 53, 15, 44, 521, 493, 28]
    result = {"schema": 1, "kind": "fresh_inventory_scope_comparison", "source_commit": args.source,
              "source_tree": receipt["source_tree"], "inputs": inputs, "counts": counts,
              "families": dict(sorted(collections.Counter(r["category"] for r in rows).items())),
              "definitions": {"old_controls": "The 200 parameter IDs present in retained revision .4 inventory.",
                              "base_vca_only": "Among old controls, only ch1/ch2_v_level changes after excluding master volume. This changes base level relative to touch, not a new oscillator/filter/envelope recipe.",
                              "mechanism": "A configured audible piano (1), non-Kalimba tine (2) or reed (3) rank. Configuration coverage is not an acoustic-realism result."},
              "programs": rows, "limitations": ["No PCM rendering, listening, acoustic measurement or native qualification is performed here.", "This reconstructs parameter comparisons from durable old bytes and a fresh independent current capture; it does not recover lost original .8 probe files."]}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"status": "verified", "counts": counts, "out": str(args.out)}))


if __name__ == "__main__":
    main()
