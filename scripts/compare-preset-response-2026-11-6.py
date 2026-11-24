#!/usr/bin/env python3
"""Verify cumulative 2026.11.6 factory coverage against published 2026.11.4.

This is structural coverage evidence, not a perceptual sound-quality test.
The output is written atomically only after all invariants pass. A failed
comparison leaves an existing report untouched and returns a nonzero status.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import sys
import tempfile


BASELINE_COMMIT = "8685e9ed39d6347b67401da7590b7e1e8e95472d"
BASELINE_INVENTORY_SHA256 = "0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611"
BASELINE_REVISION = "2026.11.4"
REFINEMENT_REVISION = "2026.11.6"
PROGRAM_COUNT = 523
OLD_PARAMETER_COUNT = 200
BODY_IDS = [f"body{rank}_{control}" for rank in (1, 2)
            for control in ("model", "mix", "brightness", "decay")]
BODY_DEFAULTS = [0, 0, .5, 1, 0, 0, .5, 1]
SIZE_IDS = ["body1_size", "body2_size"]
VARIANT_IDS = ["body1_variant", "body2_variant"]
MATERIAL_IDS = ["body1_material", "body2_material"]
APPENDED_IDS = VARIANT_IDS + MATERIAL_IDS
APPENDED_DEFAULTS = [0, 0, 0, 0]
ORIGINAL_PROGRAM_COUNT = 222
MATERIAL_NAMES = ["Standard", "Tubular bell", "Gong", "Temple bell", "Agogo",
                  "Music box", "Kalimba", "Banjo", "Koto", "Harp"]
MATERIAL_MODELS = {1: 16, 2: 16, 3: 16, 4: 16, 5: 19, 6: 2, 7: 7, 8: 7, 9: 6}
FACTORY_MATERIALS = {"Tubular Bells": 1, "Gong": 2, "Temple Bell": 3,
                     "Agogo": 4, "Music Box": 5, "Kalimba": 6, "Banjo": 7,
                     "Koto": 8, "Harp": 9}
VARIANT_NAMES = ["Standard", "Xylophone bar", "Steelpan shell", "Choir ah", "Choir oo"]
VARIANT_MODELS = {1: 18, 2: 17, 3: 15, 4: 15}
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
    require(ids[198:200] == SIZE_IDS, f"{label}: published size IDs moved")
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
        require(len(tags) == len(set(tags)), f"{label}: duplicate tags")
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
        if count >= 200:
            require(all(.5 <= size <= 2.5 for size in values[198:200]),
                    f"{label}: {program['name']} has an out-of-range resonant size")
        if count >= 202:
            for rank, variant in enumerate(values[200:202]):
                require(variant == int(variant) and 0 <= variant < len(VARIANT_NAMES),
                        f"{label}: {program['name']} has an invalid resonator variant")
                if variant:
                    model, mix = values[190 + rank * 4:192 + rank * 4]
                    require(model == VARIANT_MODELS[int(variant)] and mix > 0,
                            f"{label}: {program['name']} has an inactive or incompatible factory variant")
        if count == 204:
            for rank, material in enumerate(values[202:204]):
                require(material == int(material) and 0 <= material < len(MATERIAL_NAMES),
                        f"{label}: {program['name']} has an invalid material")
                if material:
                    model, mix = values[190 + rank * 4:192 + rank * 4]
                    require(model == MATERIAL_MODELS[int(material)] and mix > 0,
                            f"{label}: {program['name']} has an inactive or incompatible factory material")
                    require(values[200 + rank] == 0,
                            f"{label}: {program['name']} selects both a variant and material")
    init = {program["name"] for program in programs if program["category"] == "Init"}
    require(init == INIT_NAMES, f"{label}: expected exactly the two existing Init programs")


def compare(before, after):
    validate_inventory(before, OLD_PARAMETER_COUNT, "Before")
    validate_inventory(after, OLD_PARAMETER_COUNT + len(APPENDED_IDS), "After")
    require(before["library_revision"] == BASELINE_REVISION, "Before inventory has the wrong library revision")
    require(after["library_revision"] == REFINEMENT_REVISION, "After inventory has the wrong library revision")
    old_ids = before["parameter_ids"]
    require(after["parameter_ids"] == old_ids + APPENDED_IDS,
            "The original 200 IDs moved or the appended variant/material IDs differ")
    volume = old_ids.index("volume")
    family_counts = Counter()
    family_changed = Counter()
    model_counts = Counter()
    body_programs = 0
    tag_changes = 0
    variant_counts = Counter()
    material_counts = Counter()
    seen = {}
    records = []
    for old, new in zip(before["programs"], after["programs"]):
        identity = tuple(new[key] for key in ("host_index", "name", "category"))
        require(identity == tuple(old[key] for key in ("host_index", "name", "category")),
                f"Factory identity changed at host position {old['host_index']}: {old['name']}")
        expected_material = FACTORY_MATERIALS.get(new["name"], 0)
        require(new["values"][202:204] == [expected_material, expected_material],
                f"Factory material selection changed or is missing: {new['name']}")
        tags_changed = old["tags"] != new["tags"]
        tag_changes += int(tags_changed)
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
                                   resonant_size=values[198 + rank],
                                   variant_index=int(values[200 + rank]),
                                   variant=VARIANT_NAMES[int(values[200 + rank])],
                                   material_index=int(values[202 + rank]),
                                   material=MATERIAL_NAMES[int(values[202 + rank])]))
                variant_counts[VARIANT_NAMES[int(values[200 + rank])]] += 1
                material_counts[MATERIAL_NAMES[int(values[202 + rank])]] += 1
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
                            library="original" if new["host_index"] < ORIGINAL_PROGRAM_COUNT else "curated",
                            library_index=new["host_index"] if new["host_index"] < ORIGINAL_PROGRAM_COUNT
                                else new["host_index"] - ORIGINAL_PROGRAM_COUNT,
                            sound_controls_changed=sound_changed,
                            changed_existing_control_count=len(existing),
                            changed_existing_controls=existing,
                            changed_appended_controls=appended,
                            tags_changed=tags_changed, previous_tags=old["tags"], tags=new["tags"],
                            volume_changed=old_values[volume] != values[volume],
                            active_models=active))
    return dict(
        validation=dict(passed=True, ordered_program_identities_preserved=PROGRAM_COUNT,
                        ordered_program_tags_preserved=PROGRAM_COUNT - tag_changes,
                        updated_program_tags=tag_changes,
                        original_parameter_ids_preserved=OLD_PARAMETER_COUNT,
                        previous_202_parameter_id_order_preserved=True,
                        required_material_programs=len(FACTORY_MATERIALS),
                        parameter_comparison_precision="IEEE-754 binary32",
                        appended_parameter_ids=APPENDED_IDS, musical_programs_changed=PROGRAM_COUNT - 2,
                        unchanged_initialization_programs=sorted(INIT_NAMES),
                        duplicate_non_volume_vectors=0),
        summary=dict(program_count=PROGRAM_COUNT, original_program_count=ORIGINAL_PROGRAM_COUNT,
                     curated_program_count=PROGRAM_COUNT - ORIGINAL_PROGRAM_COUNT, body_enabled_programs=body_programs,
                     circuit_only_programs=PROGRAM_COUNT - body_programs,
                     active_body_ranks_by_model=dict(sorted(model_counts.items())),
                     active_body_ranks_by_variant=dict(sorted(variant_counts.items())),
                     active_body_ranks_by_material=dict(sorted(material_counts.items())),
                     families=[dict(category=category, programs=family_counts[category],
                                    changed_musical_programs=family_changed[category])
                               for category in sorted(family_counts)]),
        programs=records)


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
    args = parser.parse_args()
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
            "Cumulative .4-to-.6 structural coverage at engine float precision only; it includes "
            "the prior .5 programming and does not imply all 521 recipes were edited again in .6. "
            "sound_controls_changed means "
            "a previously published non-master-volume parameter differs; it does not assert that "
            "the changed control lies on an audible route. Changed controls do not establish "
            "perceptual improvement, reference-matched realism, audio safety, or host compatibility. "
            "Those require separate tests. Source labels are supplied by the caller; this comparator "
            "binds the input bytes, not the executable or a release qualification. Parameter ranges "
            "and host normalization are not in the inventory and require the separate A08 metadata audit."),
            before=dict(label=args.before_label, library_revision=before["library_revision"],
                        baseline_source_commit=BASELINE_COMMIT,
                        inventory_sha256=before_sha),
            after=dict(label=args.after_label, library_revision=after["library_revision"],
                       inventory_sha256=after_sha), **report)
        atomic_json(args.out, report)
    except (OSError, ValueError, TypeError, OverflowError) as error:
        print(f"Inventory comparison failed: {error}", file=sys.stderr)
        return 1
    print(f"PASS: {PROGRAM_COUNT} identities, 200 published IDs + 202-prefix order stable, 521 cumulatively changed musical programs, "
          f"2 preserved Init programs, no non-volume duplicates; report: {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
