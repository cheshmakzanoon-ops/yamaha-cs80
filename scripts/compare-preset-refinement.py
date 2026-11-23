#!/usr/bin/env python3
"""Verify the 2026.11.4 refinement against the preceding 523-program inventory.

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
import struct
import sys
import tempfile


BASELINE_COMMIT = "fa57967c7317828cf769cc7ca97f5857e566520f"
BASELINE_INVENTORY_SHA256 = "6f070ed15a04259fd5a9d5f0eb06a6e9554f821871c3313e9a793a3b28a2b521"
BASELINE_REVISION = "2026.11.3"
REFINEMENT_REVISION = "2026.11.4"
PROGRAM_COUNT = 523
OLD_PARAMETER_COUNT = 198
BODY_IDS = [f"body{rank}_{control}" for rank in (1, 2)
            for control in ("model", "mix", "brightness", "decay")]
BODY_DEFAULTS = [0, 0, .5, 1, 0, 0, .5, 1]
APPENDED_IDS = ["body1_size", "body2_size"]
APPENDED_DEFAULTS = [1, 1]
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


def engine_float(value):
    """Compare the values the float-based engine receives, not JSON precision."""
    try:
        result = struct.unpack("<f", struct.pack("<f", value))[0]
    except (OverflowError, struct.error) as error:
        raise InvalidInventory("Parameter value cannot be represented as binary32") from error
    require(math.isfinite(result), "Parameter value overflows binary32")
    return result


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
    require(ids[190:198] == BODY_IDS,
            f"{label}: published body IDs are missing or out of order")
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
        tags = program.get("tags")
        require(isinstance(tags, list) and all(isinstance(tag, str) for tag in tags),
                f"{label}: {program['name']} has invalid tags")
        values = program.get("values")
        require(isinstance(values, list) and len(values) == count,
                f"{label}: {program['name']} has the wrong value count")
        require(all(type(number) in (int, float) and math.isfinite(number) for number in values),
                f"{label}: {program['name']} has a nonnumeric or nonfinite value")
        if count >= 198:
            for rank in range(2):
                model, mix, brightness, decay = values[190 + rank * 4:194 + rank * 4]
                require(model == int(model) and 0 <= model < len(MODEL_NAMES),
                        f"{label}: {program['name']} has an invalid body model")
                require(0 <= mix <= 1 and 0 <= brightness <= 1 and .25 <= decay <= 4,
                        f"{label}: {program['name']} has an out-of-range body control")
        if count == 200:
            require(all(.5 <= size <= 2.5 for size in values[198:]),
                    f"{label}: {program['name']} has an out-of-range resonant size")
    init = {program["name"] for program in programs if program["category"] == "Init"}
    require(init == INIT_NAMES, f"{label}: expected exactly the two existing Init programs")


def compare(before, after):
    validate_inventory(before, OLD_PARAMETER_COUNT, "Before")
    validate_inventory(after, OLD_PARAMETER_COUNT + len(APPENDED_IDS), "After")
    old_ids = before["parameter_ids"]
    require(after["parameter_ids"] == old_ids + APPENDED_IDS,
            "The original 198 parameter IDs moved or the two size IDs differ")
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
        require(old["tags"] == new["tags"], f"Factory tags changed: {new['name']}")
        old_values = tuple(engine_float(value) for value in old["values"])
        values = tuple(engine_float(value) for value in new["values"])
        existing = [param for index, param in enumerate(old_ids)
                    if index != volume and old_values[index] != values[index]]
        appended = [param for index, param in enumerate(APPENDED_IDS)
                    if APPENDED_DEFAULTS[index] != values[OLD_PARAMETER_COUNT + index]]
        active = []
        for rank in range(2):
            model, mix, brightness, decay = values[190 + rank * 4:194 + rank * 4]
            if model != 0 and mix > 0:
                model = int(model)
                active.append(dict(rank="I" if rank == 0 else "II", model_index=model,
                                   model=MODEL_NAMES[model], mix=mix,
                                   brightness=brightness, partial_decay=decay,
                                   resonant_size=values[198 + rank]))
                model_counts[MODEL_NAMES[model]] += 1
        # This revision requires a change to a previously published sound
        # control for every musical program. An already-active body or a
        # different master volume cannot qualify an unchanged recipe.
        sound_changed = bool(existing)
        if new["name"] in INIT_NAMES:
            require(not existing and not appended and old_values[volume] == values[volume],
                    f"Initialization program changed: {new['name']}")
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
                            changed_existing_control_count=len(existing),
                            changed_existing_controls=existing,
                            changed_appended_controls=appended,
                            volume_changed=old_values[volume] != values[volume],
                            active_models=active))
    return dict(
        validation=dict(passed=True, ordered_program_identities_preserved=PROGRAM_COUNT,
                        ordered_program_tags_preserved=PROGRAM_COUNT,
                        original_parameter_ids_preserved=OLD_PARAMETER_COUNT,
                        parameter_comparison_precision="IEEE-754 binary32",
                        appended_parameter_ids=APPENDED_IDS, musical_programs_changed=PROGRAM_COUNT - 2,
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
    ids = ["volume"] + [f"parameter_{index}" for index in range(1, 190)] + BODY_IDS
    programs = []
    for index in range(PROGRAM_COUNT):
        name = "Init Patch" if index == 43 else "Init Circuit" if index == 522 else f"Voice {index}"
        programs.append(dict(index=index, host_index=index, name=name,
                             category="Init" if name in INIT_NAMES else "Test", tags=[],
                             values=[.5, index] + [0] * 188 + BODY_DEFAULTS))
    before = dict(schema=1, library_revision="before", parameter_ids=ids, programs=programs)
    after = deepcopy(before)
    after["parameter_ids"] += APPENDED_IDS
    after["library_revision"] = "after"
    for program in after["programs"]:
        program["values"] += APPENDED_DEFAULTS
        if program["name"] not in INIT_NAMES:
            program["values"][2] = .25
    compare(before, after)
    mutations = [
        ("renamed identity", lambda data: data["programs"][0].update(name="Renamed")),
        ("unchanged musical sound", lambda data: data["programs"][0]["values"].__setitem__(2, 0)),
        ("moved parameter ID", lambda data: data["parameter_ids"].reverse()),
        ("duplicate vector", lambda data: data["programs"][1].update(values=data["programs"][0]["values"][:])),
        ("changed tags", lambda data: data["programs"][0].update(tags=["different"])),
        ("invalid size", lambda data: data["programs"][0]["values"].__setitem__(198, 4.0)),
        ("modified Init", lambda data: data["programs"][43]["values"].__setitem__(2, .1)),
        ("nonfinite value", lambda data: data["programs"][0]["values"].__setitem__(2, float("nan"))),
    ]
    invalid_fixtures = []
    for label, mutate in mutations:
        invalid = deepcopy(after)
        mutate(invalid)
        invalid_fixtures.append((label, before, invalid))

    # An unchanged instrument body must not qualify merely because it was
    # already enabled in the preceding release.
    active_before, active_after = deepcopy(before), deepcopy(after)
    for inventory in (active_before, active_after):
        inventory["programs"][0]["values"][190:194] = [1, .85, .5, 1]
    compare(active_before, active_after)
    unchanged_active = deepcopy(active_after)
    unchanged_active["programs"][0]["values"][:198] = active_before["programs"][0]["values"]
    invalid_fixtures.append(("unchanged already-active body", active_before, unchanged_active))

    volume_only = deepcopy(after)
    volume_only["programs"][0]["values"][:198] = before["programs"][0]["values"]
    volume_only["programs"][0]["values"][0] = .6
    invalid_fixtures.append(("master-volume-only change", before, volume_only))

    size_only = deepcopy(unchanged_active)
    size_only["programs"][0]["values"][198] = 1.2
    invalid_fixtures.append(("size-only change to an existing body", active_before, size_only))

    below_float_precision = deepcopy(after)
    below_float_precision["programs"][0]["values"][:198] = before["programs"][0]["values"]
    below_float_precision["programs"][0]["values"][3] = 1e-46
    invalid_fixtures.append(("JSON change that rounds to the same engine float", before,
                             below_float_precision))

    init_volume = deepcopy(after)
    init_volume["programs"][43]["values"][0] = .6
    invalid_fixtures.append(("Init master-volume change", before, init_volume))

    for label, baseline, invalid in invalid_fixtures:
        try:
            compare(baseline, invalid)
        except InvalidInventory:
            continue
        raise AssertionError(f"Negative control incorrectly passed: {label}")
    print(f"Inventory comparison self-test: two valid fixtures and {len(invalid_fixtures)} rejected mutations passed")


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
        require(before_sha == BASELINE_INVENTORY_SHA256,
                "Before inventory does not match the recorded baseline bytes for " + BASELINE_COMMIT)
        report = compare(before, after)
        require(before["library_revision"] == BASELINE_REVISION,
                "Before inventory has the wrong library revision")
        require(after["library_revision"] == REFINEMENT_REVISION,
                "After inventory has the wrong library revision")
        report = dict(schema=1, evidence_kind="factory_parameter_coverage", interpretation=(
            "Structural coverage at engine float precision only. sound_controls_changed means "
            "a previously published non-master-volume parameter differs; it does not assert that "
            "the changed control lies on an audible route. Changed controls do not establish "
            "perceptual improvement, reference-matched realism, audio safety, or host compatibility. "
            "Those require separate tests. Source labels are supplied by the caller; this comparator "
            "binds the input bytes, not the executable or a release qualification."),
            before=dict(label=args.before_label, library_revision=before["library_revision"],
                        baseline_source_commit=BASELINE_COMMIT,
                        inventory_sha256=before_sha),
            after=dict(label=args.after_label, library_revision=after["library_revision"],
                       inventory_sha256=after_sha), **report)
        atomic_json(args.out, report)
    except (OSError, ValueError, TypeError, OverflowError) as error:
        print(f"Inventory comparison failed: {error}", file=sys.stderr)
        return 1
    print(f"PASS: {PROGRAM_COUNT} identities, 198 stable IDs, 521 changed musical programs, "
          f"2 preserved Init programs, no non-volume duplicates; report: {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
