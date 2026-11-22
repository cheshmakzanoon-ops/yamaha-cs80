#!/usr/bin/env python3
"""Bind separate Linux/Windows validator input bundles to verified install inventories."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import stat
import zipfile

if not __debug__:
    raise SystemExit("Payload verification requires assertions; run Python without -O or -OO.")


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install-report", type=Path, required=True)
    parser.add_argument("--input-downloads", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    installs = json.loads(args.install_report.read_text())
    downloads = json.loads(args.input_downloads.read_text())
    assert installs["status"] == "PASS" and installs["all_four_targets_verified"]
    source = installs["expected_commit"]
    run = int(installs["expected_run_id"])
    assert downloads["source_sha"] == source and downloads["run_id"] == run
    inputs = {r["target"]: r for r in downloads["downloads"]}
    assert set(inputs) == {"linux-x86_64", "windows-x86_64"}
    assert len(inputs) == len(downloads["downloads"])
    rows = []
    for target, downloaded in sorted(inputs.items()):
        assert downloaded["source_sha"] == source and downloaded["run_id"] == run
        candidates = [r for r in installs["artifacts"] if r["target"] == target]
        assert len(candidates) == 1
        item = candidates[0]
        assert item["status"] == "PASS" and item["trusted_outer_sha256_verified"]
        system = target.split("-", 1)[0]
        manifest = Path(item["archive"]).parent / f"cs80-reference-{system}-x86_64-{source[:12]}.manifest.json"
        assert sha(manifest) == item["manifest_sha256"]
        data = json.loads(manifest.read_text())
        assert data["commit"] == source and int(data["workflow_run_id"]) == run
        prefix = data["product"] + ".vst3/"
        expected = {}
        for record in data["files"]:
            if record["path"].startswith("VST3/" + prefix):
                assert record["type"] in ("file", "directory"), "Unexpected VST3 symlink"
                if record["type"] == "file":
                    expected[record["path"][len("VST3/"):]] = record
        assert expected, "Empty installation VST3 inventory"
        archive = args.input_downloads.parent / downloaded["file"]
        assert archive.stat().st_size == downloaded["bytes"] == downloaded["expected_bytes"]
        assert sha(archive) == downloaded["sha256"] == downloaded["expected_sha256"]
        assert downloaded["digest_matches_api"] and downloaded["purpose"] == "validator-input"
        matches = []
        with zipfile.ZipFile(archive) as bundle:
            entries = [r for r in bundle.infolist() if r.filename.startswith(prefix) and not r.is_dir()]
            assert len({r.filename.casefold() for r in entries}) == len(entries), "Duplicate input path"
            assert {r.filename for r in entries} == set(expected), "VST3 file inventory mismatch"
            for entry in entries:
                record = expected[entry.filename]
                assert stat.S_IFMT(entry.external_attr >> 16) in (0, stat.S_IFREG)
                assert not entry.flag_bits & 1
                assert entry.file_size == record["size"] <= 300_000_000
                digest = hashlib.sha256()
                with bundle.open(entry) as stream:
                    while chunk := stream.read(1024 * 1024):
                        digest.update(chunk)
                actual = digest.hexdigest()
                assert actual == record["sha256"], f"Payload mismatch: {entry.filename}"
                matches.append({"path": entry.filename, "bytes": entry.file_size,
                                "validator_input_sha256": actual,
                                "installation_sha256": record["sha256"], "identical": True})
        rows.append({"target": target, "input_artifact_id": downloaded["artifact_id"],
                     "input_artifact_name": downloaded["artifact_name"],
                     "input_outer_sha256": downloaded["sha256"],
                     "install_outer_sha256": item["outer_sha256"],
                     "complete_vst3_file_inventory_identical": True, "files": matches})
    result = {"status": "PASS", "source_commit": source, "run_id": run,
              "checked_at_utc": datetime.now(timezone.utc).isoformat(), "targets": rows,
              "scope": "Exact complete VST3 regular-file contents in separate validator input archives and installation inventories. Validator download identity and runtime SUCCESS must be established separately from native reports. This comparison does not inspect validator outcomes or execute binaries."}
    args.out.write_text(json.dumps(result, indent=2) + "\n")
    print("PASS: Linux and Windows complete VST3 file contents match their installation bundles")


if __name__ == "__main__":
    main()
