#!/usr/bin/env python3
"""Offline artifact-boundary regressions; these do not simulate or certify DSP."""
import csv
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
PRESSURE = ["Aftertouch Vibrato Lead", "Pressure Lead", "Wah Lead"]
ORIGINAL = ["Tone I: E. Piano", "Tone II: Bass", "Analog Piano", "Bright Keys",
            "Amber Keys 97", "Slow Keys 106", "Replicant Brass", "Bass Stack"]
SOURCE = "3" * 40
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
            names = KEYBOARDS + FAMILIES + ORIGINAL + ["Pressure Lead", "Wah Lead"]
            names += [f"Unselected fixture {i}" for i in range(523 - len(names))]
            programs = [{"host_index": i, "name": name} for i, name in enumerate(names)]
            if self.fault == "short inventory":
                programs.pop()
            elif self.fault == "unordered addresses":
                programs[0]["host_index"], programs[1]["host_index"] = 1, 0
            elif self.fault == "duplicate name":
                programs[1]["name"] = programs[0]["name"]
            elif self.fault == "missing selection":
                programs[0]["name"] = "Unselected replacement"
            Path(option("--inventory")).write_text(json.dumps(
                {"programs": programs, "library_revision": "2026.11.2"}), encoding="utf-8")
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

    def run_capture(self, output, child):
        with (mock.patch.object(helper.subprocess, "run", side_effect=child),
              mock.patch.object(helper.platform, "platform", return_value="fixture-platform")):
            helper.capture_auditions(self.capture, output, SOURCE)

    def test_complete_artifact_has_all_examples_provenance_and_exact_bytes(self):
        out = self.root / "complete"
        child = CaptureChild()
        with mock.patch.dict(os.environ, {"GITHUB_REPOSITORY": "owner/repo", "GITHUB_RUN_ID": "42",
                                          "GITHUB_RUN_ATTEMPT": "2"}):
            self.run_capture(out, child)
        manifest = json.loads((out / "provenance.json").read_text())
        self.assertEqual(manifest["capture_status"], "complete")
        self.assertEqual(manifest["platform"], "fixture-platform")
        self.assertEqual(manifest["source_commit"], SOURCE)
        self.assertEqual((out / "source-commit.txt").read_text(), SOURCE + "\n")
        self.assertEqual((manifest["repository"], manifest["run_id"], manifest["run_attempt"]),
                         ("owner/repo", "42", "2"))
        self.assertEqual(manifest["capture_binary_sha256"], hashlib.sha256(self.capture.read_bytes()).hexdigest())
        self.assertEqual((manifest["wav_count"], manifest["expected_wav_count"]), (68, 68))
        self.assertEqual(manifest["library_revision"], "2026.11.2")
        self.assertEqual(manifest["inventory_command"], child.commands[0])
        self.assertEqual(manifest["capture_commands"], child.commands[1:])
        groups = manifest["selections"]
        self.assertEqual(groups[0]["names"], KEYBOARDS)
        self.assertEqual(groups[1]["names"], ["Electric Piano I", "Electric Piano II", "Tine Piano", "Reed Piano", "Stage Keys"])
        self.assertEqual(groups[2]["names"], FAMILIES)
        self.assertEqual(groups[3]["names"], ORIGINAL)
        self.assertEqual([(g["names"], g["pressure"], g["hold"], g["tail"]) for g in groups[4:6]],
                         [(PRESSURE, 0, 5, 1), (PRESSURE, 1, 5, 1)])
        self.assertEqual([(g["names"], g["note"]) for g in groups[6:]],
                         [(["Tine Piano"], 48), (["Tine Piano"], 60), (["Tine Piano"], 84)])
        self.assertEqual(len({name for group in groups for name in group["names"]}), 56)
        for command in child.commands:
            self.assertEqual(command[command.index("--library") + 1], "all")
        for path in out.rglob("*.wav"):
            self.assertEqual(path.read_bytes(), WAV)
        checksums = dict(line.split("  ", 1)[::-1] for line in (out / "SHA256SUMS").read_text().splitlines())
        files = {path.relative_to(out).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in out.rglob("*") if path.is_file() and path.name != "SHA256SUMS"}
        self.assertEqual(checksums, files)

    def test_invalid_child_outputs_fail_without_completion_manifest_or_checksums(self):
        faults = ["short inventory", "unordered addresses", "duplicate name", "missing selection",
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


if __name__ == "__main__":
    unittest.main()
