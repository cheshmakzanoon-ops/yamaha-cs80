#!/usr/bin/env python3
"""Deterministic owner-only capture cue files; never sends MIDI or changes DSP.

Source timeline: frozen src/tools/engine_audit.cpp, source tree 7dd159d... .
The original analogue CS-80 has no assumed MIDI retrofit. These small standard
MIDI files are only a schedule for an owner-approved controller/key actuator;
actual hardware key contacts and gate timing must be independently recorded.
"""

from __future__ import annotations

import argparse
import csv
from decimal import Decimal
import hashlib
from pathlib import Path
import struct
import subprocess


EXPECTED_SRC_TREE = "7dd159d491012fd47b46c0aa651e86ea959ca289"
TICKS_PER_QUARTER = 480
MICROSECONDS_PER_QUARTER = 500_000
TICKS_PER_SECOND = 960
MIDI_VELOCITY = 102  # 0.8031496 of MIDI full scale; source Engine uses float 0.8.


def t(value: str) -> Decimal:
    return Decimal(value)


def ticks(seconds: Decimal) -> int:
    exact = seconds * TICKS_PER_SECOND
    if exact != exact.to_integral_value():
        raise ValueError(f"non-exact MIDI tick timestamp: {seconds}")
    return int(exact)


def vlq(number: int) -> bytes:
    if number < 0:
        raise ValueError("negative MIDI delta")
    stack = [number & 0x7F]
    number >>= 7
    while number:
        stack.append(0x80 | (number & 0x7F))
        number >>= 7
    return bytes(reversed(stack))


def meta(kind: int, payload: bytes) -> bytes:
    return b"\xFF" + bytes([kind]) + vlq(len(payload)) + payload


def channel_event(on: bool, note: int) -> bytes:
    assert 0 <= note <= 127
    return bytes([0x90 if on else 0x80, note, MIDI_VELOCITY if on else 0])


# Retain original phrase *prefix* to the named target, including the 3.0s
# chord and its 4.4s release: the physical-PANEL retrigger depends on the
# allocator state and cannot be replaced by a single new note at t=0.
BASS_PREFIX = [
    (t("0.20"), True, 60), (t("1.40"), False, 60),
    *[(t("3.00"), True, n) for n in (48, 52, 55, 60)],
    *[(t("4.40"), False, n) for n in (48, 52, 55, 60)],
    (t("6.00"), True, 60), (t("6.20"), False, 60),
    (t("6.25"), True, 60), (t("6.55"), False, 60),
]
FUNKY_EVENT = [(t("0.20"), True, 60), (t("1.40"), False, 60)]
# A physical key cannot remain held while the same key is struck again at 3s.
# Keep the no-off control clean through 1.70s, then send a 1.90s safety off.
FUNKY_HOLD = [(t("0.20"), True, 60), (t("1.90"), False, 60)]


def verify_frozen_source(repo: Path) -> None:
    current = subprocess.check_output(
        ["git", "rev-parse", "HEAD:src"], cwd=repo, text=True
    ).strip()
    if current != EXPECTED_SRC_TREE:
        raise RuntimeError(f"source tree changed: {current} != {EXPECTED_SRC_TREE}")
    # HEAD:src alone is insufficient: an edited or untracked source file can
    # coexist with the expected committed tree and influence the cue timeline.
    # Check all of src, not just the audit translation unit's fixture tokens.
    dirty = subprocess.check_output(
        ["git", "status", "--porcelain=v1", "--untracked-files=all", "--", "src"],
        cwd=repo, text=True,
    ).strip()
    if dirty:
        raise RuntimeError("refusing hardware cues from a dirty source worktree")
    original = (repo / "src/tools/engine_audit.cpp").read_text(encoding="utf-8")
    for expected in (
        "constexpr int kBlock = 64",
        "retrigOn1 = 6.00",
        "retrigOff1 = 6.20", "retrigOn2 = 6.25", "retrigOff2 = 6.55",
        "singleOn = 0.20", "singleOff = 1.40",
    ):
        if expected not in original:
            raise RuntimeError(f"original audit fixture token missing: {expected}")


def make_midi(label: str, events: list[tuple[Decimal, bool, int]],
              marker_seconds: Decimal, end_seconds: Decimal) -> bytes:
    # Stable tie ordering is the auditor's source note order. Markers are meta
    # labels only, not a purported electrical trigger or audible pilot pulse.
    ledger: list[tuple[int, int, bytes]] = []
    ledger.append((0, 0, meta(0x03, label.encode("ascii"))))
    ledger.append((0, 1, meta(0x51, MICROSECONDS_PER_QUARTER.to_bytes(3, "big"))))
    ledger.append((ticks(marker_seconds), 2,
                   meta(0x06, b"SOURCE_EVENT_TARGET_NOT_PHYSICAL_GATE")))
    for order, (when, on, note) in enumerate(events):
        ledger.append((ticks(when), 10 + order, channel_event(on, note)))
    ledger.append((ticks(end_seconds), 1000, meta(0x2F, b"")))
    ledger.sort(key=lambda item: (item[0], item[1]))
    track = bytearray()
    cursor = 0
    for absolute, _, raw in ledger:
        if absolute < cursor:
            raise ValueError("nonmonotonic MIDI timeline")
        track.extend(vlq(absolute - cursor))
        track.extend(raw)
        cursor = absolute
    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, TICKS_PER_QUARTER)
    return header + b"MTrk" + struct.pack(">I", len(track)) + bytes(track)


def controls() -> list[tuple[str, list[tuple[Decimal, bool, int]], Decimal, Decimal]]:
    no_bass_target = [ev for ev in BASS_PREFIX
                      if ev != (t("6.25"), True, 60)]
    assert len(BASS_PREFIX) - len(no_bass_target) == 1
    return [
        ("bass_panel_phrase_event", BASS_PREFIX, t("6.25"), t("7.00")),
        # Retain 6.55s noteOff just as the original audit omission comparator.
        ("bass_panel_phrase_omit_retrigger_on", no_bass_target, t("6.25"), t("7.00")),
        ("funky_tabs_single_noteoff", FUNKY_EVENT, t("1.40"), t("2.00")),
        ("funky_tabs_single_held_no_off_at_1400ms", FUNKY_HOLD, t("1.40"), t("2.00")),
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True,
                        help="directory for 4 deterministic MIDI cues and SHA manifest")
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    verify_frozen_source(args.repo)
    # Evidence is immutable once written. Even a previously empty directory is
    # rejected, preventing accidental overwrite and ambiguous regenerated sets.
    if args.out.exists():
        raise SystemExit("refusing to overwrite an existing hardware-cue directory")
    args.out.mkdir(parents=True, exist_ok=False)
    rows: list[dict[str, object]] = []
    for name, events, marker, end in controls():
        payload = make_midi(name, events, marker, end)
        destination = args.out / f"{name}.mid"
        with destination.open("xb") as out:
            out.write(payload)
        rows.append({
            "file": destination.name,
            "sha256": hashlib.sha256(payload).hexdigest(),
            "bytes": len(payload),
            "track_type": "SMF0",
            "ppqn": TICKS_PER_QUARTER,
            "tempo_us_per_quarter": MICROSECONDS_PER_QUARTER,
            "velocity_byte": MIDI_VELOCITY,
            "target_seconds": str(marker),
            "target_tick": ticks(marker),
            "end_seconds": str(end),
            "note_event_count": len(events),
            "source_tree": EXPECTED_SRC_TREE,
        })
    manifest = args.out / "click_hardware_midi_cue_manifest.csv"
    with manifest.open("x", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    for row in rows:
        print(row["file"], row["sha256"], row["bytes"], "target_tick", row["target_tick"])
    print("manifest", hashlib.sha256(manifest.read_bytes()).hexdigest())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
