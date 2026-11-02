#!/usr/bin/env python3
"""Independent, fail-closed tests for the source-locked MIDI audition cues.

These files are timing aids for a future technician, *not* recorded hardware
evidence or proof that a Yamaha CS-80 contains a MIDI interface.
"""

from __future__ import annotations

import importlib.util
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = pathlib.Path(__file__).with_name("GenerateClickHardwareCues.py")
REPO = SCRIPT.parents[2]
spec = importlib.util.spec_from_file_location("click_cues", SCRIPT)
assert spec and spec.loader
cues = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cues)


def vlq(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    for _ in range(4):
        if offset >= len(data):
            raise ValueError("truncated MIDI variable-length quantity")
        byte = data[offset]
        offset += 1
        value = (value << 7) | (byte & 127)
        if not byte & 128:
            return value, offset
    raise ValueError("MIDI variable-length quantity too long")


def parse_smf0(data: bytes) -> tuple[int, list[tuple[int, str, object]]]:
    if data[:4] != b"MThd" or len(data) < 22:
        raise ValueError("invalid SMF header")
    header_length, fmt, tracks, ppqn = struct.unpack(">IHHH", data[4:14])
    if (header_length, fmt, tracks, ppqn) != (6, 0, 1, 480):
        raise ValueError("wrong MIDI format, track count, or PPQN")
    if data[14:18] != b"MTrk":
        raise ValueError("missing MIDI track chunk")
    length = struct.unpack(">I", data[18:22])[0]
    if len(data) != 22 + length:
        raise ValueError("wrong MIDI track length")
    pos, tick, events = 22, 0, []
    while pos < len(data):
        delta, pos = vlq(data, pos)
        tick += delta
        status = data[pos]
        pos += 1
        if status == 0xFF:
            kind = data[pos]
            pos += 1
            size, pos = vlq(data, pos)
            payload = data[pos:pos + size]
            if len(payload) != size:
                raise ValueError("truncated MIDI metadata")
            pos += size
            events.append((tick, "meta", (kind, payload)))
        elif status in (0x80, 0x90):
            if pos + 2 > len(data):
                raise ValueError("truncated MIDI note")
            note, velocity = data[pos], data[pos + 1]
            pos += 2
            events.append((tick, "note", (status, note, velocity)))
        else:
            raise ValueError("unexpected status or running status")
    if not events or events[-1][1:] != ("meta", (0x2F, b"")):
        raise ValueError("missing final end-of-track marker")
    return ppqn, events


class HardwareCueSafetyTests(unittest.TestCase):
    def run_cli(self, destination: pathlib.Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(SCRIPT), "--repo", str(REPO), "--out", str(destination)],
            text=True, capture_output=True, check=False,
        )

    def test_does_not_overwrite_existing_directory_or_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            output = pathlib.Path(tmp) / "already-exists"
            output.mkdir()
            sentinel = output / "bass_panel_phrase_event.mid"
            sentinel.write_bytes(b"USER-EXISTING-CUE")
            result = self.run_cli(output)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertEqual(sentinel.read_bytes(), b"USER-EXISTING-CUE")
            self.assertEqual(sorted(p.name for p in output.iterdir()), [sentinel.name])

    def test_rejects_existing_empty_directory_too(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            output = pathlib.Path(tmp) / "existing-empty"
            output.mkdir()
            self.assertNotEqual(self.run_cli(output).returncode, 0)
            self.assertEqual(list(output.iterdir()), [])

    def test_rejects_dirty_tracked_or_untracked_source(self) -> None:
        fixture_tokens = (
            "constexpr int kBlock = 64\nretrigOn1 = 6.00\nretrigOff1 = 6.20\n"
            "retrigOn2 = 6.25\nretrigOff2 = 6.55\n"
            "singleOn = 0.20\nsingleOff = 1.40\n"
        )
        with tempfile.TemporaryDirectory() as tmp:
            repo = pathlib.Path(tmp)
            audit = repo / "src/tools/engine_audit.cpp"
            engine = repo / "src/core/Engine.cpp"
            audit.parent.mkdir(parents=True)
            engine.parent.mkdir(parents=True)
            audit.write_text(fixture_tokens)
            engine.write_text("// source lock witness\n")
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            subprocess.run(["git", "-C", str(repo), "add", "src"], check=True)
            subprocess.run(["git", "-C", str(repo), "-c", "user.name=Fixture",
                            "-c", "user.email=fixture@example.invalid", "commit", "-qm", "base"], check=True)
            tree = subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD:src"], text=True).strip()
            with mock.patch.object(cues, "EXPECTED_SRC_TREE", tree):
                cues.verify_frozen_source(repo)
                audit.write_text(fixture_tokens + "// undetected audit alteration\n")
                with self.assertRaises(RuntimeError):
                    cues.verify_frozen_source(repo)
                audit.write_text(fixture_tokens)
                engine.write_text("// changed engine law, but HEAD tree unchanged\n")
                with self.assertRaises(RuntimeError):
                    cues.verify_frozen_source(repo)
                engine.write_text("// source lock witness\n")
                extra = repo / "src/core/Unexpected.cpp"
                extra.write_text("// untracked source file\n")
                with self.assertRaises(RuntimeError):
                    cues.verify_frozen_source(repo)

    def test_native_smf_timeline_and_mutation_controls(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            destination = pathlib.Path(tmp) / "new-cues"
            result = self.run_cli(destination)
            self.assertEqual(result.returncode, 0, result.stderr)
            expected = {
                "bass_panel_phrase_event.mid": (6000, 14, 6720),
                "bass_panel_phrase_omit_retrigger_on.mid": (6000, 13, 6720),
                "funky_tabs_single_noteoff.mid": (1344, 2, 1920),
                "funky_tabs_single_held_no_off_at_1400ms.mid": (1344, 2, 1920),
            }
            for name, (marker_tick, count, ending) in expected.items():
                raw = (destination / name).read_bytes()
                _, events = parse_smf0(raw)
                self.assertIn((0, "meta", (0x51, b"\x07\xa1\x20")), events)
                self.assertIn((marker_tick, "meta", (0x06, b"SOURCE_EVENT_TARGET_NOT_PHYSICAL_GATE")), events)
                self.assertEqual(events[-1], (ending, "meta", (0x2F, b"")))
                notes = [e for e in events if e[1] == "note"]
                self.assertEqual(len(notes), count)
                self.assertTrue(all(0 <= e[2][1] <= 127 and
                                    e[2][2] == (102 if e[2][0] == 0x90 else 0)
                                    for e in notes))
                with self.assertRaises(ValueError):
                    parse_smf0(b"WRONG" + raw[5:])
                mutated_length = bytearray(raw)
                mutated_length[21] ^= 1
                with self.assertRaises(ValueError):
                    parse_smf0(bytes(mutated_length))
            omission = parse_smf0((destination / "bass_panel_phrase_omit_retrigger_on.mid").read_bytes())[1]
            self.assertNotIn((6000, "note", (0x90, 60, 102)), omission)
            baseline = parse_smf0((destination / "bass_panel_phrase_event.mid").read_bytes())[1]
            self.assertIn((6000, "note", (0x90, 60, 102)), baseline)


if __name__ == "__main__":
    unittest.main()
