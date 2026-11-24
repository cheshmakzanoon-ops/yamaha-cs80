#!/usr/bin/env python3
"""Negative controls for the frozen cumulative 2026.11.6 inventory contract."""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/compare-preset-response-2026-11-6.py"
spec = importlib.util.spec_from_file_location("compare_2026_11_6", SCRIPT)
contract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contract)


def fixture():
    names = list(contract.FACTORY_MATERIALS)
    programs = []
    for index in range(contract.PROGRAM_COUNT):
        name = ("Init Patch" if index == 43 else "Init Circuit" if index == 522
                else names[index] if index < len(names) else f"Voice {index}")
        values = [.5, index] + [0] * 188 + contract.BODY_DEFAULTS + [1, 1]
        material = contract.FACTORY_MATERIALS.get(name, 0)
        if material:
            model = contract.MATERIAL_MODELS[material]
            values[190:194] = [model, 1, .5, 1]
            values[194:198] = [model, 1, .5, 1]
        programs.append(dict(index=index, host_index=index, name=name,
                             category="Init" if name in contract.INIT_NAMES else "Test",
                             tags=[], values=values))
    ids = ["volume"] + [f"parameter_{index}" for index in range(1, 190)]
    before = dict(schema=1, library_revision=contract.BASELINE_REVISION,
                  parameter_ids=ids + contract.BODY_IDS + contract.SIZE_IDS,
                  programs=programs)
    after = deepcopy(before)
    after["library_revision"] = contract.REFINEMENT_REVISION
    after["parameter_ids"] += contract.APPENDED_IDS
    for program in after["programs"]:
        material = contract.FACTORY_MATERIALS.get(program["name"], 0)
        program["values"] += [0, 0, material, material]
        if program["name"] not in contract.INIT_NAMES:
            program["values"][2] = .25
    return before, after


class ContractTests(unittest.TestCase):
    def setUp(self):
        self.before, self.after = fixture()

    def test_valid_cumulative_coverage(self):
        result = contract.compare(self.before, self.after)
        self.assertEqual(result["validation"]["musical_programs_changed"], 521)
        self.assertEqual(result["summary"]["original_program_count"], 222)
        self.assertEqual(result["summary"]["curated_program_count"], 301)
        self.assertEqual(result["programs"][221]["library_index"], 221)
        self.assertEqual(result["programs"][222]["library_index"], 0)
        self.assertEqual(result["programs"][222]["library"], "curated")
        for material in contract.MATERIAL_NAMES[1:]:
            self.assertEqual(result["summary"]["active_body_ranks_by_material"][material], 2)

    def test_allowed_tags_description_and_musical_volume_edits(self):
        self.after["programs"][0].update(tags=["responsive"], description="Authored material model")
        self.after["programs"][0]["values"][0] = .4
        result = contract.compare(self.before, self.after)
        self.assertEqual(result["validation"]["updated_program_tags"], 1)

    def test_identity_and_inventory_mutations_are_rejected(self):
        mutations = {
            "missing program": lambda a: a["programs"].pop(),
            "added program": lambda a: a["programs"].append(deepcopy(a["programs"][-1])),
            "reordered programs": lambda a: a["programs"].reverse(),
            "renamed program": lambda a: a["programs"][10].update(name="Renamed"),
            "changed category": lambda a: a["programs"][10].update(category="Other"),
            "changed host address": lambda a: a["programs"][10].update(host_index=222),
            "changed inventory address": lambda a: a["programs"][10].update(index=222),
            "duplicate name": lambda a: a["programs"][10].update(name=a["programs"][11]["name"]),
            "missing Init classification": lambda a: a["programs"][43].update(category="Other"),
            "wrong revision": lambda a: a.update(library_revision="2026.11.5"),
            "boolean schema": lambda a: a.update(schema=True),
            "invalid tags": lambda a: a["programs"][10].update(tags=[True]),
            "duplicate tags": lambda a: a["programs"][10].update(tags=["touch", "touch"]),
            "stale 202 inventory": lambda a: truncate(a, 202),
            "stale 200 inventory": lambda a: truncate(a, 200),
            "missing material ID": lambda a: a["parameter_ids"].pop(),
            "swapped old parameter IDs": lambda a: swap(a["parameter_ids"], 5, 6),
            "swapped variant IDs": lambda a: swap(a["parameter_ids"], 200, 201),
            "swapped material IDs": lambda a: swap(a["parameter_ids"], 202, 203),
            "missing material value": lambda a: a["programs"][0]["values"].pop(),
            "inactive required material": lambda a: a["programs"][0]["values"].__setitem__(202, 0),
            "wrong required material": lambda a: a["programs"][0]["values"].__setitem__(202, 2),
            "incompatible material model": lambda a: a["programs"][0]["values"].__setitem__(190, 1),
            "inaudible material mix": lambda a: a["programs"][0]["values"].__setitem__(191, 0),
            "fractional material": lambda a: a["programs"][0]["values"].__setitem__(202, 1.5),
            "out of range material": lambda a: a["programs"][0]["values"].__setitem__(202, 10),
            "variant/material collision": lambda a: a["programs"][4]["values"].__setitem__(200, 3),
            "invalid variant": lambda a: a["programs"][10]["values"].__setitem__(200, 5),
            "fractional variant": lambda a: a["programs"][10]["values"].__setitem__(200, .5),
            "inactive variant": lambda a: a["programs"][10]["values"].__setitem__(200, 1),
            "invalid body size": lambda a: a["programs"][10]["values"].__setitem__(198, 4),
            "invalid body model": lambda a: a["programs"][10]["values"].__setitem__(190, 20),
            "invalid body decay": lambda a: a["programs"][10]["values"].__setitem__(193, 0),
            "NaN value": lambda a: a["programs"][10]["values"].__setitem__(2, float("nan")),
            "infinite value": lambda a: a["programs"][10]["values"].__setitem__(2, float("inf")),
            "boolean value": lambda a: a["programs"][10]["values"].__setitem__(2, True),
            "binary32 overflow": lambda a: a["programs"][10]["values"].__setitem__(2, 1e100),
            "duplicate nonvolume vectors": lambda a: a["programs"][11].update(values=a["programs"][10]["values"][:]),
            "modified Init sound": lambda a: a["programs"][43]["values"].__setitem__(2, .1),
            "modified Init volume": lambda a: a["programs"][43]["values"].__setitem__(0, .1),
            "modified Init suffix": lambda a: a["programs"][43]["values"].__setitem__(202, 1),
            "unchanged musical sound": lambda a: a["programs"][10]["values"].__setitem__(2, 0),
            "sub-float-precision edit": lambda a: a["programs"][10]["values"].__setitem__(2, 1e-46),
        }
        for label, mutate in mutations.items():
            with self.subTest(label=label):
                after = deepcopy(self.after)
                mutate(after)
                with self.assertRaises(contract.InvalidInventory):
                    contract.compare(self.before, after)
        print(f"Rejected {len(mutations)} structural mutations")

    def test_volume_or_new_material_does_not_qualify_as_prior_sound_edit(self):
        for index, label in ((0, "new material only"), (10, "volume only")):
            with self.subTest(label=label):
                after = deepcopy(self.after)
                after["programs"][index]["values"][:200] = self.before["programs"][index]["values"]
                after["programs"][index]["values"][0] = .6
                with self.assertRaises(contract.InvalidInventory):
                    contract.compare(self.before, after)

    def test_json_ambiguity_and_nonfinite_numbers_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "invalid.json"
            for text in ('{"schema": 1, "schema": 1}', '{"value": NaN}', '{"value": Infinity}'):
                path.write_text(text)
                with self.assertRaises(contract.InvalidInventory):
                    contract.load_inventory(path)

    def test_cli_valid_report_and_failed_comparison_are_atomic(self):
        baseline = ROOT / "docs/preset-evidence/2026.11.5/baseline/inventory.json"
        before, digest = contract.load_inventory(baseline)
        self.assertEqual(digest, contract.BASELINE_INVENTORY_SHA256)
        after = deepcopy(before)
        after["library_revision"] = contract.REFINEMENT_REVISION
        after["parameter_ids"] += contract.APPENDED_IDS
        # A structural fixture derived from the authenticated baseline. Its
        # arbitrary control edits are test data, never production recipes.
        for program in after["programs"]:
            material = contract.FACTORY_MATERIALS.get(program["name"], 0)
            program["values"] += [0, 0, material, material]
            if program["name"] not in contract.INIT_NAMES:
                program["values"][4] += .01
            if material:
                for rank in range(2):
                    program["values"][190 + rank * 4] = contract.MATERIAL_MODELS[material]
                    program["values"][191 + rank * 4] = 1
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            candidate, out = path / "after.json", path / "report.json"
            candidate.write_text(json.dumps(after))
            command = [sys.executable, str(SCRIPT), "--before", str(baseline),
                       "--after", str(candidate), "--out", str(out)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = out.read_bytes()
            self.assertEqual(json.loads(report)["validation"]["musical_programs_changed"], 521)
            after["programs"][10]["name"] = "Incorrect identity"
            candidate.write_text(json.dumps(after))
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn("Factory identity changed", result.stderr)
            self.assertEqual(out.read_bytes(), report)
            self.assertEqual(list(path.glob("*.tmp")), [])

    def test_cli_failure_preserves_existing_report_and_input_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            before, after, out = (path / name for name in ("before.json", "after.json", "report.json"))
            before.write_text(json.dumps(self.before))
            after.write_text(json.dumps(self.after))
            out.write_text("previous valid report\n")
            command = [sys.executable, str(SCRIPT), "--before", str(before), "--after", str(after), "--out", str(out)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn("recorded baseline bytes", result.stderr)
            self.assertEqual(out.read_text(), "previous valid report\n")
            original = after.read_bytes()
            result = subprocess.run(command[:-1] + [str(after)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(after.read_bytes(), original)


def swap(items, first, second):
    items[first], items[second] = items[second], items[first]


def truncate(inventory, count):
    inventory["parameter_ids"] = inventory["parameter_ids"][:count]
    for program in inventory["programs"]:
        program["values"] = program["values"][:count]


if __name__ == "__main__":
    unittest.main(verbosity=2)
