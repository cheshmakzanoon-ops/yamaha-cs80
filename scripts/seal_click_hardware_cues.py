#!/usr/bin/env python3
"""Seal/verify four frozen-source MIDI cue files as non-hardware CI evidence.

This tool does not generate audio, call hardware or weaken a musical-click
acceptance criterion. It refuses existing producer files and unexpected data.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess


SOURCE_TREE = "7dd159d491012fd47b46c0aa651e86ea959ca289"
MANIFEST = "click_hardware_midi_cue_manifest.csv"
PRODUCER = "producer.json"
SUMS = "SHA256SUMS"
EXPECTED = {
    "bass_panel_phrase_event.mid": ("f77c0c93f03b8f34fcd0321853e03bdf06fe8cf58613e554f1adaa54286312e3", 165, "6000", "14"),
    "bass_panel_phrase_omit_retrigger_on.mid": ("062b8feb1c63195652b69a441e57a9c061539aeb92f179ae9d9e57d412f5ed47", 173, "6000", "13"),
    "funky_tabs_single_noteoff.mid": ("46be894d42f237abe48d63dcaf07065a98e18abe6c708e051984124338c4b663", 114, "1344", "2"),
    "funky_tabs_single_held_no_off_at_1400ms.mid": ("46ea383dc49d8a2868bebf21bf725897f9ad3914edab2c84333795da30e1332b", 128, "1344", "2"),
}
MANIFEST_SHA = "0e6d196df140530f810109ff4f20cae129490b227ddba5923948b184ada91467"
FIELDS = ["file", "sha256", "bytes", "track_type", "ppqn", "tempo_us_per_quarter",
          "velocity_byte", "target_seconds", "target_tick", "end_seconds",
          "note_event_count", "source_tree"]


def insist(predicate: bool, detail: str) -> None:
    if not predicate:
        raise ValueError(detail)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_source(repo: Path, commit: str) -> None:
    insist(re.fullmatch(r"[0-9a-f]{40}", commit) is not None,
           "reviewed source commit must be an exact lowercase 40-hex SHA")
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip()
    source = subprocess.check_output(["git", "rev-parse", "HEAD:src"], cwd=repo, text=True).strip()
    insist(head == commit, "checked-out commit differs from explicit reviewed source SHA")
    insist(source == SOURCE_TREE, "frozen production src tree differs from preregistration")
    dirty = subprocess.check_output(["git", "status", "--porcelain=v1", "--untracked-files=all", "--", "src"],
                                    cwd=repo, text=True).strip()
    insist(not dirty, "dirty source tree cannot produce immutable MIDI evidence")


def check_files(directory: Path, sealed: bool) -> None:
    insist(directory.is_dir() and not directory.is_symlink(), "cue directory absent or symlinked")
    expected = set(EXPECTED) | {MANIFEST}
    if sealed:
        expected |= {PRODUCER, SUMS}
    actual = {x.name for x in directory.iterdir()}
    insist(actual == expected, f"wrong cue artifact file set: {sorted(actual ^ expected)}")
    for name in actual:
        insist(not (directory / name).is_symlink(), f"symlinked cue artifact forbidden: {name}")


def check_midi_manifest(directory: Path) -> None:
    insist(digest(directory / MANIFEST) == MANIFEST_SHA, "source-locked CSV manifest bytes changed")
    with (directory / MANIFEST).open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        insist(reader.fieldnames == FIELDS, "MIDI cue manifest schema drift")
        rows = list(reader)
    insist(len(rows) == 4 and {r["file"] for r in rows} == set(EXPECTED),
           "missing/duplicate/extra MIDI cue manifest row")
    for row in rows:
        name = row["file"]
        expected_sha, expected_bytes, tick, count = EXPECTED[name]
        insist(digest(directory / name) == expected_sha, f"source-locked MIDI bytes changed: {name}")
        insist((directory / name).stat().st_size == expected_bytes,
               f"source-locked MIDI size changed: {name}")
        fixed = {"sha256": expected_sha, "bytes": str(expected_bytes), "track_type": "SMF0",
                 "ppqn": "480", "tempo_us_per_quarter": "500000", "velocity_byte": "102",
                 "target_tick": tick, "note_event_count": count, "source_tree": SOURCE_TREE}
        for key, expected_value in fixed.items():
            insist(row[key] == expected_value, f"MIDI {name} inconsistent {key}")


def producer_record(commit: str, run_id: str, attempt: str) -> dict[str, object]:
    insist(run_id.isdecimal() and int(run_id) > 0, "run id must be positive decimal")
    insist(attempt.isdecimal() and int(attempt) > 0, "run attempt must be positive decimal")
    return {"schema": "cs80-hardware-cue-diagnostic-v1", "source_commit": commit,
            "source_tree": SOURCE_TREE, "generator": "tests/diagnostics/GenerateClickHardwareCues.py",
            "run_id": run_id, "run_attempt": attempt, "manifest_sha256": MANIFEST_SHA,
            "midi_files": {name: data[0] for name, data in sorted(EXPECTED.items())},
            "hardware_measured": False, "original_click_gates_passed": False}


def sums_content(directory: Path) -> str:
    names = sorted([*EXPECTED, MANIFEST, PRODUCER])
    return "".join(f"{digest(directory / name)}  {name}\n" for name in names)


def seal(directory: Path, commit: str, run_id: str, attempt: str) -> None:
    check_files(directory, sealed=False)  # refuses stale producer or any preexisting extra
    check_midi_manifest(directory)
    record = producer_record(commit, run_id, attempt)
    with (directory / PRODUCER).open("x", encoding="utf-8", newline="\n") as out:
        json.dump(record, out, sort_keys=True, indent=2)
        out.write("\n")
    with (directory / SUMS).open("x", encoding="utf-8", newline="\n") as out:
        out.write(sums_content(directory))
    verify(directory, commit, run_id, attempt)


def verify(directory: Path, commit: str, run_id: str, attempt: str) -> None:
    check_files(directory, sealed=True)
    check_midi_manifest(directory)
    with (directory / PRODUCER).open(encoding="utf-8") as src:
        record = json.load(src)
    insist(record == producer_record(commit, run_id, attempt), "producer provenance or schema changed")
    insist((directory / SUMS).read_text(encoding="utf-8") == sums_content(directory),
           "SHA256SUMS changed or a cue was corrupted")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["seal", "verify"])
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--attempt", required=True)
    args = parser.parse_args()
    verify_source(args.repo, args.commit)
    if args.mode == "seal":
        seal(args.directory, args.commit, args.run_id, args.attempt)
    else:
        verify(args.directory, args.commit, args.run_id, args.attempt)
    print(f"{args.mode} PASS source={args.commit} tree={SOURCE_TREE} files=7 hardware_measurement=false")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        raise SystemExit(f"click-cue evidence refused: {exc}") from exc
