#!/usr/bin/env python3
"""Offline artifact-boundary regressions; these do not simulate or certify DSP."""
import csv
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.dont_write_bytecode = True
SCRIPT = Path(__file__).with_name("capture-factory-auditions.py")
spec = importlib.util.spec_from_file_location("factory_auditions", SCRIPT)
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)

KEYBOARDS = [
    "Electric Piano I", "Electric Piano II", "Tine Piano", "Clavichord I",
    "Clavichord II", "Funky Clavichord", "Harpsichord I", "Harpsichord II",
    "Lute Harpsichord", "Toy Piano", "Honky Keys", "Vibes Keys", "Celesta",
    "Saw Pulse Piano", "Soft Keys", "Chorus Keys", "Stage Keys", "Velocity Keys",
    "Dulcimer", "Reed Piano", "Glass Keys", "Tremolo Keys", "Contour Keys", "Pressure Keys",
]
FAMILIES = ["Brass Swell I", "Sforzando Brass", "Covered Horn", "Solo Violin",
            "Solo Cello", "Marcato Strings", "Recorder", "Clarinet", "Flute Organ 8+4", "Tremolo Organ",
            "Rubber Bass", "Picked Bass", "Nylon Pluck", "Mandolin", "Vibraphone", "Marimba",
            "Tremolo Brass", "Tremolo Pad", "Tremolo Poly", "Touch Lead", "Aftertouch Vibrato Lead", "Glide Lead"]
PRESSURE = ["Aftertouch Vibrato Lead", "Pressure Lead", "Wah Lead",
            "Pressure Pad", "Flute", "Solo Violin", "Pressure Keys"]
ORIGINAL = ["Tone I: E. Piano", "Tone II: Bass", "Analog Piano", "Bright Keys",
            "Amber Keys 97", "Slow Keys 106", "Replicant Brass", "Bass Stack"]
INSTRUMENTS = ["Trombone Glide", "Theatre Organ", "Oboe", "Bassoon", "Guitar I", "Guitar II", "Kalimba"]
TEXTURES = ["Evolving Pad", "Breathing Pad", "Thunder", "Rain", "Surf", "Wind"]
SUSTAINED = ["Solo Violin", "Flute", "Clarinet", "Oboe", "English Horn", "Covered Horn", "Muted Trumpet"]
SUSTAINED_BASS = ["Solo Cello", "Bass Clarinet", "Bassoon"]
MATERIALS = ["Xylophone", "Steel Drum", "Marimba", "Vibraphone"]
CHOIRS = ["Choir Pad", "Dark Choir"]
LOW_CHOIR_DERIVATIVES = ["Golden Pads 52", "Blade Choir", "Midnight Pads 55"]
ORIGINAL_ARTICULATION = ["Frozen Leads 121", "Soft Leads 128", "Neon Sequences 174"]
MATERIAL_PROFILES = ["Tubular Bells", "Gong", "Temple Bell", "Agogo", "Music Box",
                     "Banjo", "Koto", "Harp", "Kalimba"]
REGISTER_VOICES = ["Ocarina", "Crystal Strings", "Mellow Organ"]
PARAMETER_IDS = [f"fixture_parameter_{i}" for i in range(204)]
PARAMETER_VALUES = [0.0] * 202 + [.125, .75]
SOURCE = "3" * 40
SOURCE_STATE = dict(source_commit=SOURCE, source_tree="5" * 40, tracked_source_clean=True)
# A tiny valid stereo IEEE-float WAV, used only as an unchanged-byte witness.
SAMPLES = struct.pack("<4f", .125, -.25, .0625, -.125)
WAV = struct.pack("<4sI4s4sIHHIIHH4sI", b"RIFF", 36 + len(SAMPLES), b"WAVE",
                  b"fmt ", 16, 3, 2, 48000, 384000, 8, 32, b"data", len(SAMPLES)) + SAMPLES


class CaptureChild:
    """Produce independent child-process outputs, then inject transfer faults."""
    def __init__(self, fault=None):
        self.fault = fault
        self.commands = []

    def __call__(self, command, *, check):
        assert check is True
        self.commands.append(command)
        option = lambda name: command[command.index(name) + 1]
        if "--inventory" in command:
            names = list(dict.fromkeys(KEYBOARDS + FAMILIES + ORIGINAL + PRESSURE + INSTRUMENTS
                                       + TEXTURES + SUSTAINED + SUSTAINED_BASS
                                       + MATERIALS + CHOIRS + ORIGINAL_ARTICULATION + MATERIAL_PROFILES
                                       + LOW_CHOIR_DERIVATIVES + REGISTER_VOICES))
            names += [f"Unselected fixture {i}" for i in range(523 - len(names))]
            programs = [{"host_index": i, "name": name, "category": "Fixture", "values": PARAMETER_VALUES}
                        for i, name in enumerate(names)]
            if self.fault == "short inventory":
                programs.pop()
            elif self.fault == "unordered addresses":
                programs[0]["host_index"], programs[1]["host_index"] = 1, 0
            elif self.fault == "duplicate name":
                programs[1]["name"] = programs[0]["name"]
            elif self.fault == "missing selection":
                programs[0]["name"] = "Unselected replacement"
            elif self.fault == "invalid category":
                programs[0]["category"] = ""
            Path(option("--inventory")).write_text(json.dumps(
                {"schema": 1, "programs": programs, "parameter_ids": PARAMETER_IDS,
                 "library_revision": "2026.11.6"}), encoding="utf-8")
            return subprocess.CompletedProcess(command, 0)
        if self.fault == "child failure":
            raise subprocess.CalledProcessError(2, command)
        out = Path(option("--out"))
        out.mkdir()
        names = [command[i + 1] for i, argument in enumerate(command) if argument == "--preset"]
        rows = []
        for i, name in enumerate(names):
            (out / f"{i:03d}.wav").write_bytes(WAV)
            rows.append(dict(name=name, sample_rate=option("--rate"), velocity=option("--velocity"),
                             pressure=option("--pressure"), hold_seconds=option("--hold"),
                             tail_seconds=option("--tail"), rank=0, nonfinite=0,
                             samples_over_full_scale=0,
                             note=option("--note") if "--note" in command else 60))
        changes = {
            "wrong preset": ("name", "Not the requested preset"),
            "wrong rate": ("sample_rate", 44100),
            "wrong velocity": ("velocity", .99),
            "nonfinite setting": ("sample_rate", "nan"),
            "nonfinite audio": ("nonfinite", 1),
            "clipped audio": ("samples_over_full_scale", 1),
        }
        if self.fault in changes:
            key, value = changes[self.fault]
            rows[0][key] = value
        if self.fault == "wrong register" and "--note" in command:
            rows[0]["note"] = int(option("--note")) + 12
        if self.fault == "missing metrics":
            rows.pop()
        if self.fault == "empty wav":
            (out / "000.wav").write_bytes(b"")
        if self.fault == "missing wav":
            (out / "000.wav").unlink()
        if self.fault == "extra wav":
            (out / "unexpected.wav").write_bytes(WAV)
        with (out / "metrics.csv").open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0]) if rows else ["name"])
            writer.writeheader()
            writer.writerows(rows)
        return subprocess.CompletedProcess(command, 0)


class AuditionTests(unittest.TestCase):
    def setUp(self):
        self.work = tempfile.TemporaryDirectory(prefix="cs80-audition-contract-")
        self.addCleanup(self.work.cleanup)
        self.root = Path(self.work.name)
        self.capture = self.root / "capture"
        self.capture.write_bytes(b"inert capture executable fixture")

    def run_capture(self, output, child, source_check=None):
        source_check = mock.Mock(return_value=SOURCE_STATE) if source_check is None else source_check
        with (mock.patch.object(helper.subprocess, "run", side_effect=child),
              mock.patch.object(helper.platform, "platform", return_value="fixture-platform"),
              mock.patch.object(helper, "verify_source", new=source_check)):
            helper.capture_auditions(self.capture, output, SOURCE)
        return source_check

    def test_complete_artifact_has_all_examples_provenance_and_exact_bytes(self):
        out = self.root / "complete"
        child = CaptureChild()
        with mock.patch.dict(os.environ, {"GITHUB_REPOSITORY": "owner/repo", "GITHUB_RUN_ID": "42",
                                          "GITHUB_RUN_ATTEMPT": "2"}):
            source_check = self.run_capture(out, child)
        manifest = json.loads((out / "provenance.json").read_text())
        self.assertEqual(manifest["capture_status"], "complete")
        self.assertEqual(manifest["platform"], "fixture-platform")
        self.assertEqual(manifest["source_commit"], SOURCE)
        self.assertEqual(manifest["source_tree"], SOURCE_STATE["source_tree"])
        self.assertTrue(manifest["tracked_source_clean_before_capture"])
        self.assertTrue(manifest["tracked_source_clean_after_capture"])
        self.assertEqual(source_check.call_args_list, [mock.call(SOURCE), mock.call(SOURCE)])
        self.assertEqual((out / "source-commit.txt").read_text(), SOURCE + "\n")
        self.assertEqual((manifest["repository"], manifest["run_id"], manifest["run_attempt"]),
                         ("owner/repo", "42", "2"))
        self.assertEqual(manifest["capture_binary_sha256"], hashlib.sha256(self.capture.read_bytes()).hexdigest())
        summary = helper.roster_summary(helper.selections())
        self.assertEqual((manifest["wav_count"], manifest["expected_wav_count"]), (summary["wav_count"],) * 2)
        self.assertEqual((manifest["audio_seconds"], manifest["expected_audio_seconds"]), (summary["audio_seconds"],) * 2)
        self.assertEqual(manifest["selected_program_count"], summary["selected_program_count"])
        self.assertEqual(manifest["library_revision"], "2026.11.6")
        inventory = json.loads((out / "inventory.json").read_text())
        self.assertEqual(inventory["parameter_ids"], PARAMETER_IDS)
        self.assertTrue(all(program["values"] == PARAMETER_VALUES for program in inventory["programs"]))
        self.assertEqual(manifest["inventory_command"], child.commands[0])
        self.assertEqual(manifest["capture_commands"], child.commands[1:])
        groups = manifest["selections"]
        self.assertEqual(groups[0]["names"], KEYBOARDS)
        self.assertEqual(groups[1]["names"], ["Electric Piano I", "Electric Piano II", "Tine Piano", "Reed Piano", "Stage Keys"])
        self.assertEqual(groups[2]["names"], FAMILIES)
        self.assertEqual(groups[3]["names"], ORIGINAL)
        self.assertEqual([(g["names"], g["pressure"], g["hold"], g["tail"]) for g in groups[4:6]],
                         [(PRESSURE, 0, 5, 1), (PRESSURE, 1, 5, 1)])
        self.assertEqual(groups[6]["names"], INSTRUMENTS)
        self.assertEqual(groups[7]["names"], TEXTURES)
        self.assertEqual((groups[8]["names"], groups[8]["velocity"]), (["Kalimba"], .3))
        self.assertEqual([(g["names"], g["note"]) for g in groups[9:12]],
                         [(["Tine Piano"], 48), (["Tine Piano"], 60), (["Tine Piano"], 84)])
        self.assertEqual(len(groups), 60)
        self.assertEqual(len({name for group in groups for name in group["names"]}), 95)
        readme = (out / "README.txt").read_text()
        self.assertIn("256 unnormalized circuit-synth auditions of 95 existing programs", readme)
        self.assertIn("1487.2 seconds total", readme)
        self.assertIn("Material-profile folders compare Tubular Bells, Gong, Temple Bell", readme)
        self.assertIn("same-foundation comparisons do not isolate resonant size alone", readme)
        self.assertIn("Single-note examples do not establish acoustic similarity", readme)
        for group in groups:
            self.assertIn(f"- {group['folder']}: {len(group['names'])} WAVs", readme)
        for command in child.commands:
            self.assertEqual(command[command.index("--library") + 1], "all")
        for path in out.rglob("*.wav"):
            self.assertEqual(path.read_bytes(), WAV)
        checksums = dict(line.split("  ", 1)[::-1] for line in (out / "SHA256SUMS").read_text().splitlines())
        files = {path.relative_to(out).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in out.rglob("*") if path.is_file() and path.name != "SHA256SUMS"}
        self.assertEqual(checksums, files)

    def test_invalid_child_outputs_fail_without_completion_manifest_or_checksums(self):
        faults = ["short inventory", "unordered addresses", "duplicate name", "missing selection", "invalid category",
                  "wrong preset", "wrong rate", "wrong velocity", "nonfinite setting",
                  "nonfinite audio", "clipped audio", "wrong register", "missing metrics",
                  "empty wav", "missing wav", "extra wav", "child failure"]
        for i, fault in enumerate(faults):
            with self.subTest(fault=fault):
                out = self.root / f"fault-{i}"
                with self.assertRaises((RuntimeError, subprocess.CalledProcessError)):
                    self.run_capture(out, CaptureChild(fault))
                manifest = json.loads((out / "provenance.json").read_text())
                self.assertEqual(manifest["capture_status"], "failed")
                self.assertEqual(manifest["source_commit"], SOURCE)
                self.assertTrue(manifest["error"])
                self.assertFalse((out / "SHA256SUMS").exists())
                self.assertFalse((out / "README.txt").exists())

    def test_existing_output_is_never_overwritten(self):
        out = self.root / "existing"
        out.mkdir()
        marker = out / "previous.txt"
        marker.write_text("keep")
        child = CaptureChild()
        with self.assertRaises(FileExistsError):
            self.run_capture(out, child)
        self.assertEqual(marker.read_text(), "keep")
        self.assertEqual(child.commands, [])

    def test_roster_has_matched_pressure_pairs_and_no_duplicate_render_requests(self):
        groups = helper.selections()
        self.assertEqual(helper.roster_summary(groups),
                         dict(wav_count=256, selected_program_count=95, audio_seconds=1487.2))
        by_folder = {group["folder"]: group for group in groups}
        quiet, pressed = by_folder["pressure-zero"], by_folder["pressure-full"]
        self.assertEqual({k: v for k, v in quiet.items() if k not in {"folder", "pressure"}},
                         {k: v for k, v in pressed.items() if k not in {"folder", "pressure"}})
        self.assertEqual((quiet["pressure"], pressed["pressure"]), (0, 1))
        soft = by_folder["soft-kalimba"]
        firm = by_folder["instrument-bodies"]
        self.assertEqual(soft["names"], ["Kalimba"])
        self.assertIn("Kalimba", firm["names"])
        self.assertEqual((soft["velocity"], firm["velocity"]), (.3, .85))
        self.assertEqual(soft.get("note"), firm.get("note"))

    def test_register_and_effort_comparisons_have_matched_conditions(self):
        groups = helper.selections()
        by_folder = {group["folder"]: group for group in groups}
        for note in (36, 60, 84):
            soft = by_folder[f"piano-register-{note}-soft"]
            firm = by_folder[f"piano-register-{note}-firm"]
            self.assertEqual(soft["names"], ["Analog Piano", "Stage Keys"])
            self.assertEqual((soft["note"], soft["hold"], soft["tail"]), (note, 5, 1))
            self.assertEqual({k: v for k, v in soft.items() if k not in {"folder", "velocity"}},
                             {k: v for k, v in firm.items() if k not in {"folder", "velocity"}})
            self.assertEqual((soft["velocity"], firm["velocity"]), (.3, .85))
        for prefix, names, key in (("sustained-effort", SUSTAINED, 60),
                                   ("sustained-bass-effort", SUSTAINED_BASS, 72)):
            soft = by_folder[f"{prefix}-soft-c4"]
            firm = by_folder[f"{prefix}-firm-c4"]
            self.assertEqual(soft["names"], names)
            self.assertEqual((soft["note"], soft["pressure"], soft["hold"], soft["tail"]), (key, 0, 6, 1))
            self.assertEqual({k: v for k, v in soft.items() if k not in {"folder", "velocity"}},
                             {k: v for k, v in firm.items() if k not in {"folder", "velocity"}})
            self.assertEqual((soft["velocity"], firm["velocity"]), (.3, .85))
        for note in (60, 64, 68, 72):
            for label, velocity in (("soft", .3), ("firm", .85)):
                group = by_folder[f"clarinet-register-{note}-{label}"]
                self.assertEqual(group["names"], ["Clarinet", "Bass Clarinet"])
                self.assertEqual((group["note"], group["velocity"], group["pressure"], group["hold"], group["tail"]),
                                 (note, velocity, 0, 3, 1))
        # Retain the earlier payload cap on the unchanged 238-request prefix.
        # The appended 126 seconds have a separate exact total-payload check.
        self.assertLess(helper.roster_summary(groups[:54])["audio_seconds"] * 48000 * 8, 550_000_000)

    def test_material_and_vowel_comparisons_preserve_matched_conditions(self):
        groups = {group["folder"]: group for group in helper.selections()}
        for note in (48, 60, 72):
            soft = groups[f"mallet-material-{note}-soft"]
            firm = groups[f"mallet-material-{note}-firm"]
            self.assertEqual(soft["names"], ["Xylophone", "Steel Drum", "Marimba", "Vibraphone"])
            self.assertEqual((soft["note"], soft["hold"], soft["tail"]), (note, 3.5, .8))
            self.assertEqual((soft["velocity"], firm["velocity"]), (.25, .85))
            self.assertEqual({k: v for k, v in soft.items() if k not in {"folder", "velocity"}},
                             {k: v for k, v in firm.items() if k not in {"folder", "velocity"}})
            choir = groups[f"choir-vowels-{note}"]
            self.assertEqual(choir["names"], ["Choir Pad", "Dark Choir"])
            self.assertEqual((choir["note"], choir["velocity"], choir["pressure"], choir["hold"], choir["tail"]),
                             (note, .85, 0, 6, 1))
        original = groups["original-articulation"]
        self.assertEqual(original["names"], ["Frozen Leads 121", "Soft Leads 128", "Neon Sequences 174"])
        self.assertEqual((original["note"], original["hold"], original["tail"]), (60, 5, 1))

    def test_material_profiles_append_six_matched_folders_to_existing_roster(self):
        groups = helper.selections()
        existing, added = groups[:40], groups[40:46]
        self.assertEqual(len(existing), 40)
        self.assertEqual(helper.roster_summary(existing),
                         dict(wav_count=171, selected_program_count=81, audio_seconds=1033.2))
        self.assertEqual(helper.roster_summary(added),
                         dict(wav_count=54, selected_program_count=9, audio_seconds=243.0))
        self.assertEqual([group["folder"] for group in added],
                         [f"material-profile-{note}-{label}" for note in (48, 60, 72)
                          for label in ("soft", "firm")])
        for soft, firm in zip(added[::2], added[1::2]):
            self.assertEqual(soft["names"], MATERIAL_PROFILES)
            self.assertEqual((soft["pressure"], soft["hold"], soft["tail"]), (0, 3.5, 1))
            self.assertEqual((soft["velocity"], firm["velocity"]), (.25, .85))
            self.assertEqual({k: v for k, v in soft.items() if k not in {"folder", "velocity"}},
                             {k: v for k, v in firm.items() if k not in {"folder", "velocity"}})
        self.assertEqual([group["note"] for group in added], [48, 48, 60, 60, 72, 72])

    def test_low_register_requests_preserve_all_225_earlier_requests(self):
        groups = helper.selections()
        existing, added = groups[:46], groups[46:54]
        # Frozen before appending the .7 coverage: this constrains every name,
        # order, folder and setting, including default-note spelling.
        encoded = json.dumps(existing, sort_keys=True, separators=(",", ":")).encode()
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "126e05bb759e19adf7fec40dabb3c4a86bafe2f658dd845f6653c34ff3a7d31b")
        self.assertEqual(helper.roster_summary(existing),
                         dict(wav_count=225, selected_program_count=89, audio_seconds=1276.2))
        self.assertEqual(helper.roster_summary(added),
                         dict(wav_count=13, selected_program_count=6, audio_seconds=85.0))
        self.assertEqual(len(added), 8)
        by_folder = {group["folder"]: group for group in groups}
        for note in (36, 48):
            for label, velocity in (("soft", .3), ("firm", .85)):
                group = by_folder[f"low-piano-tail-{note}-{label}"]
                self.assertEqual(group["names"], ["Soft Keys"] if note == 36 else ["Stage Keys", "Soft Keys"])
                self.assertEqual((group["note"], group["velocity"], group["pressure"], group["hold"], group["tail"]),
                                 (note, velocity, 0, 5, 1))
        # Include reused earlier requests when checking the complete five-pad
        # foundation pairs. An 8-foot rank sounds its played note; 16-foot is -12.
        requests = [(name, group.get("note"), group["velocity"], group["pressure"], group["hold"], group["tail"])
                    for group in groups for name in group["names"]]
        for name, offset in (("Choir Pad", 0), ("Golden Pads 52", 0), ("Dark Choir", 12),
                             ("Blade Choir", 12), ("Midnight Pads 55", 12)):
            for foundation in (36, 48):
                self.assertEqual(requests.count((name, foundation + offset, .85, 0, 6, 1)), 1)

    def test_voice_register_requests_preserve_all_238_earlier_requests(self):
        groups = helper.selections()
        existing, added = groups[:54], groups[54:]
        encoded = json.dumps(existing, sort_keys=True, separators=(",", ":")).encode()
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "889f32cc511e7ae693803a4aec8118b2ac09f147cf2886a56af966e16115871a")
        self.assertEqual(helper.roster_summary(existing),
                         dict(wav_count=238, selected_program_count=92, audio_seconds=1361.2))
        self.assertEqual(helper.roster_summary(added),
                         dict(wav_count=18, selected_program_count=3, audio_seconds=126.0))
        self.assertEqual(len(added), 6)
        self.assertEqual([group["folder"] for group in added],
                         [f"voice-register-{note}-{label}" for note in (48, 60, 72)
                          for label in ("soft", "firm")])
        for soft, firm in zip(added[::2], added[1::2]):
            self.assertEqual(soft["names"], REGISTER_VOICES)
            self.assertEqual((soft["pressure"], soft["hold"], soft["tail"]), (0, 5, 2))
            self.assertEqual((soft["velocity"], firm["velocity"]), (.25, .85))
            self.assertEqual({k: v for k, v in soft.items() if k not in {"folder", "velocity"}},
                             {k: v for k, v in firm.items() if k not in {"folder", "velocity"}})
        self.assertEqual([group["note"] for group in added], [48, 48, 60, 60, 72, 72])
        self.assertEqual(helper.roster_summary(groups)["audio_seconds"] * 48000 * 8, 571_084_800)
        readme = helper.audition_readme(groups, helper.roster_summary(groups), SOURCE, "2026.11.8")
        self.assertIn("Voice-register folders compare the existing Ocarina, Crystal Strings and Mellow Organ", readme)
        self.assertIn("The original 238 requests are unchanged; 18 voice-register captures are appended", readme)
        self.assertIn("a single cold note does not establish repeatability or repeated-note consistency", readme)

    def test_readme_uses_inventory_revision_and_states_actual_low_register_scope(self):
        groups = helper.selections()
        readme = helper.audition_readme(groups, helper.roster_summary(groups), SOURCE, "2026.11.7")
        self.assertIn("library revision 2026.11.7; source commit " + SOURCE, readme)
        self.assertNotIn("library revision 2026.11.6", readme)
        self.assertIn("13 low-register captures are appended", readme)
        self.assertIn("16-foot Oo foundations", readme)
        self.assertIn("The capture CLI does not schedule pitch bends or select render quality", readme)
        self.assertIn("do not verify bend behavior or noise consistency across quality modes", readme)

    def test_default_note_alias_is_rejected_before_rendering_audio(self):
        groups = helper.selections()
        duplicate = copy.deepcopy(groups[0])
        duplicate.update(folder="explicit-keyboard-alias", note=60)
        groups.append(duplicate)
        # Before an inventory is available these spellings are different;
        # resolving the actual category must close the alias, without a WAV.
        helper.roster_summary(groups)
        out = self.root / "default-note-alias"
        child = CaptureChild()
        with mock.patch.object(helper, "selections", return_value=groups):
            with self.assertRaisesRegex(RuntimeError, "resolving the default note"):
                self.run_capture(out, child)
        self.assertEqual(len(child.commands), 1)
        self.assertIn("--inventory", child.commands[0])
        self.assertFalse(list(out.rglob("*.wav")))
        self.assertEqual(json.loads((out / "provenance.json").read_text())["capture_status"], "failed")
        self.assertFalse((out / "SHA256SUMS").exists())

    def test_default_note_aliases_follow_all_category_registers(self):
        for category, note in (("Bass", 48), ("Winds", 67), ("Leads", 67), ("Bells", 72), ("Keys", 60)):
            with self.subTest(category=category):
                group = dict(folder="category-default", names=["Example"],
                             velocity=.85, pressure=0, hold=4, tail=1)
                explicit = dict(group, folder="explicit-register", note=note)
                with self.assertRaisesRegex(RuntimeError, "resolving the default note"):
                    helper.validate_resolved_requests([group, explicit],
                                                      [dict(name="Example", category=category)])
                # Different played keys remain different requests, regardless
                # of any musical relationship between their sounding pitches.
                explicit["note"] += 12
                helper.validate_resolved_requests([group, explicit],
                                                  [dict(name="Example", category=category)])

    def test_invalid_roster_is_rejected_before_creating_output(self):
        valid = helper.selections()
        cases = []
        for key, value in (("folder", "../escape"), ("names", []), ("names", ["Flute", "Flute"]),
                           ("velocity", 1.01), ("pressure", -0.1), ("hold", float("nan")),
                           ("tail", -1), ("note", 60.5), ("note", 128)):
            groups = copy.deepcopy(valid)
            groups[0][key] = value
            cases.append(groups)
        cases.append(valid + [copy.deepcopy(valid[0])])
        duplicate = copy.deepcopy(valid[0])
        duplicate["folder"] = "same-render-another-folder"
        cases.append(valid + [duplicate])
        cases.append([])
        for i, groups in enumerate(cases):
            with self.subTest(case=i), mock.patch.object(helper, "selections", return_value=groups):
                out = self.root / f"invalid-roster-{i}"
                child = CaptureChild()
                with self.assertRaises(RuntimeError):
                    self.run_capture(out, child)
                self.assertEqual(child.commands, [])
                self.assertFalse(out.exists())

    def test_dirty_source_is_rejected_before_creating_output(self):
        out = self.root / "dirty"
        child = CaptureChild()
        source_check = mock.Mock(side_effect=RuntimeError("clean tracked source tree required"))
        with self.assertRaisesRegex(RuntimeError, "clean tracked source"):
            self.run_capture(out, child, source_check)
        self.assertFalse(out.exists())
        self.assertEqual(child.commands, [])

    def test_source_change_during_capture_cannot_receive_a_completion_manifest(self):
        out = self.root / "source-changed"
        source_check = mock.Mock(side_effect=[SOURCE_STATE, RuntimeError("source changed during capture")])
        with self.assertRaisesRegex(RuntimeError, "source changed"):
            self.run_capture(out, CaptureChild(), source_check)
        manifest = json.loads((out / "provenance.json").read_text())
        self.assertEqual(manifest["capture_status"], "failed")
        self.assertFalse(manifest["tracked_source_clean_after_capture"])
        self.assertFalse((out / "SHA256SUMS").exists())
        self.assertFalse((out / "README.txt").exists())

    def test_executable_change_during_capture_cannot_receive_checksums(self):
        out = self.root / "executable-changed"
        child = CaptureChild()

        def changed_executable(command, *, check):
            result = child(command, check=check)
            if "--out" in command:
                self.capture.write_bytes(b"a different executable")
            return result

        with self.assertRaisesRegex(RuntimeError, "executable changed"):
            self.run_capture(out, changed_executable)
        manifest = json.loads((out / "provenance.json").read_text())
        self.assertEqual(manifest["capture_status"], "failed")
        self.assertFalse((out / "SHA256SUMS").exists())

    def test_workflow_source_mismatch_stops_before_capture(self):
        argv = [str(SCRIPT), "--capture", str(self.capture), "--out", str(self.root / "unused")]
        with (mock.patch.object(sys, "argv", argv),
              mock.patch.object(helper.subprocess, "check_output", return_value=SOURCE + "\n"),
              mock.patch.dict(os.environ, {"GITHUB_SHA": "4" * 40}),
              mock.patch.object(helper, "capture_auditions") as capture):
            with self.assertRaisesRegex(SystemExit, "differs from workflow source"):
                helper.main()
            capture.assert_not_called()

    def test_matching_workflow_source_records_exact_commit_and_absolute_paths(self):
        out = self.root / "output"
        argv = [str(SCRIPT), "--capture", str(self.capture), "--out", str(out)]
        with (mock.patch.object(sys, "argv", argv),
              mock.patch.object(helper.subprocess, "check_output", return_value=SOURCE + "\n"),
              mock.patch.dict(os.environ, {"GITHUB_SHA": SOURCE}),
              mock.patch.object(helper, "capture_auditions") as capture):
            helper.main()
            capture.assert_called_once_with(self.capture.resolve(), out.resolve(), SOURCE)


class SourceTests(unittest.TestCase):
    """Use real temporary Git indexes; mocks cannot establish clean-source admission."""
    def setUp(self):
        self.work = tempfile.TemporaryDirectory(prefix="cs80-audition-source-")
        self.addCleanup(self.work.cleanup)
        self.repo = Path(self.work.name)
        self.git("init", "--quiet")
        (self.repo / "source.cpp").write_text("original source\n", encoding="utf-8")
        self.git("add", "source.cpp")
        self.commit()
        self.head = self.git("rev-parse", "HEAD").strip()
        env = mock.patch.dict(os.environ, {"GITHUB_SHA": self.head})
        env.start()
        self.addCleanup(env.stop)

    def git(self, *arguments):
        return subprocess.check_output(["git", *arguments], cwd=self.repo, text=True,
                                       stderr=subprocess.STDOUT)

    def commit(self):
        self.git("-c", "user.name=Audition fixture", "-c", "user.email=fixture@example.invalid",
                 "-c", "commit.gpgsign=false", "commit", "--quiet", "-m", "source fixture")

    def test_clean_commit_records_exact_tree_and_allows_untracked_output(self):
        (self.repo / "render.wav").write_bytes(WAV)
        state = helper.verify_source(self.head, self.repo)
        self.assertEqual(state, dict(source_commit=self.head,
                                    source_tree=self.git("rev-parse", "HEAD^{tree}").strip(),
                                    tracked_source_clean=True))

    def test_tracked_unstaged_staged_and_deleted_files_are_rejected(self):
        source = self.repo / "source.cpp"
        source.write_text("changed source\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "clean tracked source tree"):
            helper.verify_source(self.head, self.repo)
        self.git("add", "source.cpp")
        with self.assertRaisesRegex(RuntimeError, "clean tracked source tree"):
            helper.verify_source(self.head, self.repo)
        self.git("reset", "--hard", "HEAD")
        source.unlink()
        with self.assertRaisesRegex(RuntimeError, "clean tracked source tree"):
            helper.verify_source(self.head, self.repo)

    def test_newly_staged_source_is_rejected(self):
        (self.repo / "new.cpp").write_text("new source\n", encoding="utf-8")
        self.git("add", "new.cpp")
        with self.assertRaisesRegex(RuntimeError, "clean tracked source tree"):
            helper.verify_source(self.head, self.repo)

    def test_changed_head_is_rejected_even_when_clean(self):
        (self.repo / "source.cpp").write_text("a different committed source\n", encoding="utf-8")
        self.git("add", "source.cpp")
        self.commit()
        with self.assertRaisesRegex(RuntimeError, "changed from the recorded source"):
            helper.verify_source(self.head, self.repo)

    def test_workflow_must_identify_the_same_commit(self):
        with mock.patch.dict(os.environ, {"GITHUB_SHA": "4" * 40}):
            with self.assertRaisesRegex(RuntimeError, "differs from workflow source"):
                helper.verify_source(self.head, self.repo)


if __name__ == "__main__":
    unittest.main()
