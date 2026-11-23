#!/usr/bin/env python3
"""Verify the 2026.11.3 factory inventory against its 523-program predecessor.

This is structural coverage evidence, not a perceptual sound-quality test.
The output is written atomically only after all invariants pass. A failed
comparison leaves an existing report untouched and returns a nonzero status.
"""
from __future__ import annotations

import argparse
from collections import Counter
from copy import deepcopy
import hashlib
import json
import math
import os
from pathlib import Path
import sys
import tempfile


BASELINE_COMMIT = "73dc17b8d93d027a8a7de19a9eaeb0915122c621"
PROGRAM_COUNT = 523
OLD_PARAMETER_COUNT = 190
BODY_IDS = [f"body{rank}_{control}" for rank in (1, 2)
            for control in ("model", "mix", "brightness", "decay")]
BODY_DEFAULTS = [0, 0, .5, 1, 0, 0, .5, 1]
INIT_NAMES = {"Init Patch", "Init Circuit"}
MODEL_NAMES = [
    "Off", "Piano strings", "Tine electric piano", "Reed electric piano",
    "Clavichord", "Harpsichord", "Nylon strings", "Steel strings",
    "Plucked bass", "Bowed strings", "Flute", "Clarinet", "Oboe", "Bassoon",
    "Brass", "Organ", "Bell", "Vibraphone", "Marimba", "Celesta",
]


class InvalidInventory(ValueError):
    """An inventory or comparison violates the published identity contract."""


def require(condition, message):
    if not condition:
        raise InvalidInventory(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate JSON object key: {key}")
        result[key] = value
    return result


def reject_constant(value):
    raise InvalidInventory(f"Nonfinite JSON number: {value}")


def load_inventory(path):
    raw = path.read_bytes()
    value = json.loads(raw, object_pairs_hook=unique_object, parse_constant=reject_constant)
    return value, hashlib.sha256(raw).hexdigest()


def validate_inventory(value, count, label):
    require(isinstance(value, dict), f"{label}: expected an inventory object")
    require(type(value.get("schema")) is int and value["schema"] == 1,
            f"{label}: expected inventory schema 1")
    require(isinstance(value.get("library_revision"), str) and value["library_revision"],
            f"{label}: missing library revision")
    ids = value.get("parameter_ids")
    require(isinstance(ids, list) and len(ids) == count,
            f"{label}: expected {count} parameter IDs")
    require(all(isinstance(item, str) and item for item in ids),
            f"{label}: parameter IDs must be nonempty strings")
    require(len(set(ids)) == count and ids.count("volume") == 1,
            f"{label}: duplicate IDs or missing volume control")
    programs = value.get("programs")
    require(isinstance(programs, list) and len(programs) == PROGRAM_COUNT,
            f"{label}: expected all {PROGRAM_COUNT} programs")
    names = set()
    for index, program in enumerate(programs):
        require(isinstance(program, dict), f"{label}: program {index} is not an object")
        require(type(program.get("host_index")) is int and program["host_index"] == index,
                f"{label}: host position {index} is missing or out of order")
        require(type(program.get("index")) is int and program["index"] == index,
                f"{label}: inventory position {index} is missing or out of order")
        for field in ("name", "category"):
            require(isinstance(program.get(field), str) and program[field],
                    f"{label}: program {index} has an invalid {field}")
        require(program["name"] not in names, f"{label}: duplicate name {program['name']}")
        names.add(program["name"])
        values = program.get("values")
        require(isinstance(values, list) and len(values) == count,
                f"{label}: {program['name']} has the wrong value count")
        require(all(type(number) in (int, float) and math.isfinite(number) for number in values),
                f"{label}: {program['name']} has a nonnumeric or nonfinite value")
        if count > OLD_PARAMETER_COUNT:
            for rank in range(2):
                model, mix, brightness, decay = values[190 + rank * 4:194 + rank * 4]
                require(model == int(model) and 0 <= model < len(MODEL_NAMES),
                        f"{label}: {program['name']} has an invalid body model")
                require(0 <= mix <= 1 and 0 <= brightness <= 1 and .25 <= decay <= 4,
                        f"{label}: {program['name']} has an out-of-range body control")
    init = {program["name"] for program in programs if program["category"] == "Init"}
    require(init == INIT_NAMES, f"{label}: expected exactly the two existing Init programs")


def compare(before, after):
    validate_inventory(before, OLD_PARAMETER_COUNT, "Before")
    validate_inventory(after, OLD_PARAMETER_COUNT + len(BODY_IDS), "After")
    old_ids = before["parameter_ids"]
    require(after["parameter_ids"] == old_ids + BODY_IDS,
            "The original 190 parameter IDs moved or the eight body IDs differ")
    volume = old_ids.index("volume")
    family_counts = Counter()
    family_changed = Counter()
    model_counts = Counter()
    body_programs = 0
    seen = {}
    records = []
    for old, new in zip(before["programs"], after["programs"]):
        identity = tuple(new[key] for key in ("host_index", "name", "category"))
        require(identity == tuple(old[key] for key in ("host_index", "name", "category")),
                f"Factory identity changed at host position {old['host_index']}: {old['name']}")
        old_values, values = old["values"], new["values"]
        existing = [param for index, param in enumerate(old_ids)
                    if index != volume and old_values[index] != values[index]]
        appended = [param for index, param in enumerate(BODY_IDS)
                    if BODY_DEFAULTS[index] != values[OLD_PARAMETER_COUNT + index]]
        active = []
        for rank in range(2):
            model, mix, brightness, decay = values[190 + rank * 4:194 + rank * 4]
            if model != 0 and mix > 0:
                model = int(model)
                active.append(dict(rank="I" if rank == 0 else "II", model_index=model,
                                   model=MODEL_NAMES[model], mix=mix,
                                   brightness=brightness, partial_decay=decay))
                model_counts[MODEL_NAMES[model]] += 1
        # Appended inactive brightness/decay values alone cannot qualify a
        # musical program as changed. An enabled, nonzero body blend can.
        sound_changed = bool(existing or active)
        if new["name"] in INIT_NAMES:
            require(not existing and not appended,
                    f"Initialization program changed beyond volume: {new['name']}")
        else:
            require(sound_changed, f"Musical program has no changed sound controls: {new['name']}")
        fingerprint = tuple(number for index, number in enumerate(values) if index != volume)
        require(fingerprint not in seen,
                f"Duplicate non-volume vectors: {seen.get(fingerprint)} / {new['name']}")
        seen[fingerprint] = new["name"]
        family_counts[new["category"]] += 1
        family_changed[new["category"]] += int(sound_changed)
        body_programs += bool(active)
        records.append(dict(host_index=new["host_index"], name=new["name"], category=new["category"],
                            sound_controls_changed=sound_changed,
                            changed_existing_controls=existing,
                            changed_appended_controls=appended,
                            volume_changed=old_values[volume] != values[volume],
                            active_models=active))
    return dict(
        validation=dict(passed=True, ordered_program_identities_preserved=PROGRAM_COUNT,
                        original_parameter_ids_preserved=OLD_PARAMETER_COUNT,
                        appended_parameter_ids=BODY_IDS, musical_programs_changed=PROGRAM_COUNT - 2,
                        unchanged_initialization_programs=sorted(INIT_NAMES),
                        duplicate_non_volume_vectors=0),
        summary=dict(program_count=PROGRAM_COUNT, body_enabled_programs=body_programs,
                     circuit_only_programs=PROGRAM_COUNT - body_programs,
                     active_body_ranks_by_model=dict(sorted(model_counts.items())),
                     families=[dict(category=category, programs=family_counts[category],
                                    changed_musical_programs=family_changed[category])
                               for category in sorted(family_counts)]),
        programs=records)


def self_test():
    ids = ["volume"] + [f"parameter_{index}" for index in range(1, 190)]
    programs = []
    for index in range(PROGRAM_COUNT):
        name = "Init Patch" if index == 43 else "Init Circuit" if index == 522 else f"Voice {index}"
        programs.append(dict(index=index, host_index=index, name=name,
                             category="Init" if name in INIT_NAMES else "Test", values=[.5, index] + [0] * 188))
    before = dict(schema=1, library_revision="before", parameter_ids=ids, programs=programs)
    after = deepcopy(before)
    after["parameter_ids"] += BODY_IDS
    after["library_revision"] = "after"
    for program in after["programs"]:
        program["values"] += BODY_DEFAULTS
        if program["name"] not in INIT_NAMES:
            program["values"][2] = .25
    compare(before, after)
    mutations = [
        ("renamed identity", lambda data: data["programs"][0].update(name="Renamed")),
        ("unchanged musical sound", lambda data: data["programs"][0]["values"].__setitem__(2, 0)),
        ("moved parameter ID", lambda data: data["parameter_ids"].reverse()),
        ("duplicate vector", lambda data: data["programs"][1].update(values=data["programs"][0]["values"][:])),
        ("modified Init", lambda data: data["programs"][43]["values"].__setitem__(2, .1)),
        ("nonfinite value", lambda data: data["programs"][0]["values"].__setitem__(2, float("nan"))),
    ]
    for label, mutate in mutations:
        invalid = deepcopy(after)
        mutate(invalid)
        try:
            compare(before, invalid)
        except InvalidInventory:
            continue
        raise AssertionError(f"Negative control incorrectly passed: {label}")
    print("Inventory comparison self-test: one valid fixture and six rejected mutations passed")


def atomic_json(path, report):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent,
                                         prefix=path.name + ".", suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            # Keep the overview readable and each complete program on one
            # line, so an inventory report does not become a 17,000-line diff.
            overview = {key: value for key, value in report.items() if key != "programs"}
            text = json.dumps(overview, indent=2, ensure_ascii=False, allow_nan=False)
            stream.write(text[:-2] + ',\n  "programs": [\n')
            stream.write(",\n".join("    " + json.dumps(program, ensure_ascii=False,
                         allow_nan=False, separators=(",", ":")) for program in report["programs"]))
            stream.write("\n  ]\n}\n")
        os.replace(temporary, path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path)
    parser.add_argument("--after", type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--before-label", default=f"Baseline commit {BASELINE_COMMIT}")
    parser.add_argument("--after-label", default="Uncommitted development snapshot; no release source commit asserted")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        if not any((args.before, args.after, args.out)):
            return 0
    if not all((args.before, args.after, args.out)):
        parser.error("--before, --after and --out must be supplied together")
    if args.out.resolve() in {args.before.resolve(), args.after.resolve()}:
        parser.error("--out must not replace an input inventory")
    if not args.before_label.strip() or not args.after_label.strip():
        parser.error("source labels must not be empty")
    try:
        before, before_sha = load_inventory(args.before)
        after, after_sha = load_inventory(args.after)
        report = compare(before, after)
        report = dict(schema=1, evidence_kind="factory_parameter_coverage", interpretation=(
            "Structural coverage only. Changed controls do not establish perceptual improvement, "
            "reference-matched realism, audio safety, or host compatibility. Those require separate tests."),
            before=dict(label=args.before_label, library_revision=before["library_revision"],
                        inventory_sha256=before_sha),
            after=dict(label=args.after_label, library_revision=after["library_revision"],
                       inventory_sha256=after_sha), **report)
        atomic_json(args.out, report)
    except (OSError, ValueError, TypeError, OverflowError) as error:
        print(f"Inventory comparison failed: {error}", file=sys.stderr)
        return 1
    print(f"PASS: {PROGRAM_COUNT} identities, 190 stable IDs, 521 changed musical programs, "
          f"2 preserved Init programs, no non-volume duplicates; report: {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
