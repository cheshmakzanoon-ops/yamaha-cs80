#!/usr/bin/env python3
"""Source-locked, evidence-only workflow and downloadable MIDI bundle contract.

The manual/branch-push workflow must never become a release dispatcher. This
test is intentionally separate from general CTest: only the historical frozen
source may produce these exact diagnostic artifacts.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[2]
WORKFLOW = REPO / ".github/workflows/click-hardware-cue-evidence.yml"
GENERATOR = REPO / "tests/diagnostics/GenerateClickHardwareCues.py"
SEALER = REPO / "scripts/seal_click_hardware_cues.py"
PIN = "7dd159d491012fd47b46c0aa651e86ea959ca289"
BRANCH = "experiment/click-cue-guards-20261102"


def top_block(source: str, name: str) -> str:
    lines = source.splitlines()
    marker = f"{name}:"
    if lines.count(marker) != 1:
        raise ValueError(f"expected exactly one top-level {name} block")
    first = lines.index(marker) + 1
    last = first
    while last < len(lines) and (not lines[last] or lines[last][0].isspace()
                                 or lines[last].startswith("#")):
        last += 1
    return "\n".join(lines[first:last])


def validate_workflow_contract(source: str) -> None:
    events = top_block(source, "on")
    found = re.findall(r"(?m)^  ([a-z_-]+):\s*$", events)
    if sorted(found) != ["push", "workflow_dispatch"]:
        raise ValueError(f"trigger set is not narrowly manual + branch push: {found}")
    if not re.search(r"(?m)^    branches:\s*\n      - " + re.escape(BRANCH) + r"\s*$", events):
        raise ValueError("push is not restricted to reviewed click branch")
    if not re.search(r"(?m)^    paths:\s*\n      - '\.github/workflows/click-hardware-cue-evidence\.yml'", events):
        raise ValueError("push is not restricted to cue-related paths")
    if "reviewed_sha:" not in events or "required: true" not in events:
        raise ValueError("manual request lacks required reviewed SHA")
    permission_lines = [x.strip() for x in top_block(source, "permissions").splitlines()
                        if x.strip() and not x.strip().startswith("#")]
    if permission_lines != ["contents: read"]:
        raise ValueError("workflow permissions exceed contents:read")
    forbidden = ("pull_request:", "release:", "schedule:", "tags:",
                 "repository_dispatch:", "workflow_run:", "download-artifact",
                 "gh release", "git push", "id-token: write", "contents: write")
    if any(item in source for item in forbidden):
        raise ValueError("unexpected trigger, credential, reuse or release operation")
    required = (
        PIN, BRANCH, "refs/heads/", "GITHUB_SHA", "GITHUB_REF",
        "git rev-parse HEAD:src", "git rev-parse HEAD", "reviewed_sha",
        "test_generate_click_hardware_cues.py",
        "test_click_cue_evidence_workflow.py",
        "GenerateClickHardwareCues.py", "seal_click_hardware_cues.py",
        "producer.json", "SHA256SUMS", "click_hardware_midi_cue_manifest.csv",
        "actions/checkout@v4", "actions/upload-artifact@v5",
        "if-no-files-found: error", "overwrite: false",
    )
    missing = [item for item in required if item not in source]
    if missing:
        raise ValueError(f"missing source-lock, seal or upload contract: {missing}")


def command(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run([sys.executable, *args], text=True, capture_output=True,
                          cwd=REPO, check=False)


class CueWorkflowContractTests(unittest.TestCase):
    def test_only_reviewed_branch_push_and_manual_dispatch(self) -> None:
        validate_workflow_contract(WORKFLOW.read_text())

    def test_permission_event_and_provenance_mutations_are_rejected(self) -> None:
        source = WORKFLOW.read_text()
        validate_workflow_contract(source)
        for original, wrong in (
            ("contents: read", "contents: write"),
            (f"      - {BRANCH}", "      - main"),
            ("  workflow_dispatch:", "  pull_request:"),
            (PIN, "0" * 40),
            ("producer.json", "missing-producer-file"),
            ("if-no-files-found: error", "if-no-files-found: warn"),
        ):
            self.assertIn(original, source)
            with self.subTest(original=original), self.assertRaises(ValueError):
                validate_workflow_contract(source.replace(original, wrong, 1))

    def generate(self, root: Path) -> tuple[Path, str]:
        destination = root / "fresh-cues"
        result = command(str(GENERATOR), "--repo", str(REPO), "--out", str(destination))
        self.assertEqual(result.returncode, 0, result.stderr)
        sha = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip()
        return destination, sha

    def seal(self, folder: Path, sha: str, mode: str) -> subprocess.CompletedProcess[str]:
        return command(str(SEALER), mode, "--directory", str(folder),
                       "--repo", str(REPO), "--commit", sha,
                       "--run-id", "4567", "--attempt", "1")

    def test_seal_verify_producer_and_sha_roundtrip(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            folder, sha = self.generate(Path(tmp))
            result = self.seal(folder, sha, "seal")
            self.assertEqual(result.returncode, 0, result.stderr)
            producer = json.loads((folder / "producer.json").read_text())
            self.assertEqual(producer["source_commit"], sha)
            self.assertEqual(producer["source_tree"], PIN)
            self.assertEqual(producer["run_id"], "4567")
            self.assertEqual(len(producer["midi_files"]), 4)
            self.assertEqual(self.seal(folder, sha, "verify").returncode, 0)
            sums = (folder / "SHA256SUMS").read_text()
            self.assertEqual(len(sums.splitlines()), 6)
            self.assertIn("producer.json", sums)
            self.assertIn("click_hardware_midi_cue_manifest.csv", sums)
            before = hashlib.sha256((folder / "producer.json").read_bytes()).hexdigest()
            again = self.seal(folder, sha, "seal")
            self.assertNotEqual(again.returncode, 0)
            self.assertEqual(hashlib.sha256((folder / "producer.json").read_bytes()).hexdigest(), before)

    def test_missing_manifest_and_corrupted_midi_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            folder, sha = self.generate(Path(tmp))
            (folder / "click_hardware_midi_cue_manifest.csv").unlink()
            self.assertNotEqual(self.seal(folder, sha, "seal").returncode, 0)
        with tempfile.TemporaryDirectory() as tmp:
            folder, sha = self.generate(Path(tmp))
            self.assertEqual(self.seal(folder, sha, "seal").returncode, 0)
            midi = folder / "bass_panel_phrase_event.mid"
            raw = bytearray(midi.read_bytes())
            raw[-3] ^= 1
            midi.write_bytes(raw)
            self.assertNotEqual(self.seal(folder, sha, "verify").returncode, 0)

    def test_wrong_source_commit_refused_before_sealing(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            folder, sha = self.generate(Path(tmp))
            self.assertNotEqual(self.seal(folder, "0" * 40, "seal").returncode, 0)
            self.assertFalse((folder / "producer.json").exists())
            self.assertEqual(self.seal(folder, sha, "seal").returncode, 0)


if __name__ == "__main__":
    unittest.main()
