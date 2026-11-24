#!/usr/bin/env python3
"""Qualify the original 09af screenshot run without changing its source identity.

Only the explicitly supplied, actual API receipts/log/ZIP are accepted. This
script does not fetch a replacement run, edit any golden or publish images.
"""
import argparse
import hashlib
import io
import json
import re
import stat
import subprocess
import zipfile
from datetime import datetime
from pathlib import Path, PurePosixPath

import numpy as np
from PIL import Image

SOURCE = "09af5358d34a7ea431de7e8a42e35dae66e52159"
NATIVE_SOURCE = "db6b5a76b3c5e38dd9444d0cf6065a14f6302814"
RUN = 45344888473
ATTEMPT = 1
JOB = 138276946587


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def date(value):
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("archive", "artifact-api", "run-api", "jobs-api", "job-log",
                 "repo", "out", "extract-to"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    expected = json.loads((here / "expected-members.json").read_text())["files"]
    contract = json.loads((here / "source-contract.json").read_text())
    api = json.loads(Path(args.artifact_api).read_text())
    run = json.loads(Path(args.run_api).read_text())
    jobs = json.loads(Path(args.jobs_api).read_text())
    raw_log = Path(args.job_log).read_bytes()
    log = raw_log.decode()
    core_result = re.search(r"(?m)^\d{4}-\d\d-\d\dT[\d:.]+Z ALL (\d+) CHECKS PASSED\r?$", log)
    require(core_result is not None, "Actual core-test pass summary absent")
    require(contract["source_executed"] == SOURCE
            and contract["native_release_source"] == NATIVE_SOURCE
            and contract["all_relevant_repository_inputs_identical"],
            "Incorrect source-equivalence contract")
    changed_paths = subprocess.check_output(
        ["git", "diff", "--name-only", SOURCE, NATIVE_SOURCE], cwd=args.repo
    ).decode().splitlines()
    require(changed_paths == ["docs/INSTALL_AND_QUICKSTART.md", "docs/PRESET_GUIDE.md"]
            == contract["changed_repository_paths"], "Unexpected final-source changes")
    matching = [job for job in jobs["jobs"] if job["id"] == JOB]
    require(len(matching) == 1, "Expected exactly the original screenshot job")
    job = matching[0]
    require(run["id"] == RUN and run["head_sha"] == SOURCE
            and run["run_attempt"] == ATTEMPT and run["conclusion"] == "success"
            and run["status"] == "completed", "Actual run identity/outcome")
    require(job["run_id"] == RUN and job["head_sha"] == SOURCE
            and job["run_attempt"] == ATTEMPT and job["status"] == "completed"
            and job["conclusion"] == "success", "Actual job identity/outcome")
    steps = {step["name"]: step for step in job["steps"]}
    required_steps = (
        "Run actions/checkout@v5", "Install official Ubuntu build dependencies",
        "Configure exact source", "Build screenshot renderer and core tests",
        "Verify core", "Capture actual editor at every supported size and page",
        "Publish screenshot evidence", "Refresh repository screenshots from this exact source",
    )
    for name in required_steps:
        require(steps[name]["conclusion"] == "success", "Unsuccessful step " + name)
    require(steps["Publish preview PNGs to the ui-preview branch (non-main only)"]["conclusion"]
            == "skipped", "Unexpected non-main branch publication")
    publication_guard = "Newer code is already on main; keeping this run's screenshots as artifacts only."
    # The runner also echoes the shell script. Require the executed echo's own
    # plain output line, so seeing the command text cannot pass this assertion.
    require(re.search(r"(?m)^\d{4}-\d\d-\d\dT[\d:.]+Z "
                      + re.escape(publication_guard) + r"\r?$", log) is not None,
            "Actual current-main guard skip not observed")
    upload = steps["Publish screenshot evidence"]
    require(api["id"] > 0 and api["name"] == "cs80-panel-" + SOURCE
            and not api["expired"], "Artifact name/expiry")
    require(api["workflow_run"]["id"] == RUN and api["workflow_run"]["head_sha"] == SOURCE,
            "Artifact run/source binding")
    require(date(job["started_at"]) <= date(api["created_at"]) <= date(job["completed_at"]),
            "Artifact outside original producer job")
    require(date(upload["started_at"]) <= date(api["created_at"]) <= date(upload["completed_at"]),
            "Artifact outside original upload step")
    blob = Path(args.archive).read_bytes()
    digest = sha(blob)
    require(len(blob) == api["size_in_bytes"] and "sha256:" + digest == api["digest"],
            "Actual outer ZIP byte count/digest mismatch")
    require(SOURCE in log and f"Artifact ID {api['id']}" in log
            and f"Final size is {len(blob)} bytes. Artifact ID is {api['id']}" in log
            and "SHA256 digest of uploaded artifact zip is " + digest in log,
            "Original producer upload binding")
    extract = Path(args.extract_to)
    extract.mkdir(parents=True, exist_ok=True)
    records, goldens, pngs = [], [], []
    with zipfile.ZipFile(io.BytesIO(blob)) as archive:
        names, files = set(), set()
        for entry in archive.infolist():
            name = entry.filename
            canonical = name[:-1] if entry.is_dir() else name
            require(canonical and "\\" not in canonical and ":" not in canonical
                    and not canonical.startswith("/")
                    and PurePosixPath(canonical).as_posix() == canonical
                    and all(part not in ("", ".", "..") for part in canonical.split("/")),
                    "Noncanonical archive path")
            require(canonical not in names, "Duplicate canonical archive path")
            names.add(canonical)
            kind = stat.S_IFMT(entry.external_attr >> 16)
            require(kind in ((0, stat.S_IFDIR) if entry.is_dir() else (0, stat.S_IFREG)),
                    "Unsupported archive member type")
            if not entry.is_dir():
                files.add(name)
        require(sum(entry.file_size for entry in archive.infolist()) < 1024 ** 3,
                "Expanded archive size bound")
        require(len(expected) == len(set(expected)) and files == set(expected),
                "Actual artifact roster differs from source-reviewed expected roster")
        provenance = json.loads(archive.read("provenance.json"))
        require(provenance["commit"] == SOURCE and str(provenance["run"]) == str(RUN)
                and provenance["renderer"] == "JUCE software, actual plugin editor",
                "Embedded source/run/renderer provenance")
        smoke_bytes = archive.read("run.log")
        smoke = smoke_bytes.decode()
        require("UI SMOKE TEST PASSED" in smoke and "FAIL:" not in smoke
                and not re.search(r"UI SMOKE TEST: \d+ FAILURE", smoke), "UI smoke failure")
        require("golden refreshed:" not in smoke, "Golden refresh cannot qualify comparison")
        require("UI SMOKE TEST PASSED" in log, "Original capture step lacks success marker")
        phases = re.findall(r"^UI SMOKE PHASE: (.*)$", smoke, re.MULTILINE)
        phase_timings = re.findall(
            r"^UI SMOKE TIMING: phase=(.*?) seconds=([\d.]+) total_seconds=([\d.]+)$",
            smoke, re.MULTILINE)
        require(phases and [timing[0] for timing in phase_timings] == phases,
                "Incomplete reported smoke phases")
        checks = {}
        for line in archive.read("SHA256SUMS").decode().splitlines():
            match = re.fullmatch(r"([0-9a-f]{64})  artifacts/ui/([^/]+\.png)", line)
            require(match is not None, "Malformed PNG checksum line")
            expected_hash, name = match.groups()
            require(name not in checks, "Duplicate PNG checksum")
            checks[name] = expected_hash
            require(sha(archive.read(name)) == expected_hash, "PNG checksum mismatch " + name)
        top_png = {name for name in files if "/" not in name and name.endswith(".png")}
        require(set(checks) == top_png, "Top-level PNG checksum inventory")
        for name in sorted(files):
            data = archive.read(name)  # ZIP's own CRC is verified by this complete read.
            record = {"path": name, "bytes": len(data), "sha256": sha(data)}
            if name.endswith(".png"):
                with Image.open(io.BytesIO(data)) as im:
                    require(im.format == "PNG", "Non-PNG image " + name)
                    im.verify()
                with Image.open(io.BytesIO(data)) as im:
                    im.load()
                    record["dimensions"] = list(im.size)
                    record["mode"] = im.mode
                    require(im.width > 0 and im.height > 0, "Empty PNG " + name)
                    dimensions = re.search(r"-(\d+)x(\d+)\.png$", name)
                    if dimensions:
                        require(tuple(map(int, dimensions.groups())) == im.size,
                                "Filename/image dimensions differ " + name)
                pngs.append(name)
            records.append(record)
        for width, height in ((1600, 1000), (1120, 700)):
            name = f"main-{width}x{height}"
            candidate = archive.read("golden/golden-candidate-" + name + ".png")
            baseline = subprocess.check_output(
                ["git", "show", SOURCE + ":tests/golden/" + name + ".png"], cwd=args.repo)
            before = np.asarray(Image.open(io.BytesIO(baseline)).convert("RGB"), dtype=np.int16)
            after = np.asarray(Image.open(io.BytesIO(candidate)).convert("RGB"), dtype=np.int16)
            require(before.shape == after.shape == (height, width, 3), "Golden dimensions")
            delta = np.abs(before - after).max(axis=2)
            scale = np.float32(width) / np.float32(1600)
            rounded = lambda value: int(np.floor(np.float32(value) * scale + np.float32(.5)))
            xx, yy, ww, hh = rounded(1118), 0, rounded(342), rounded(48)
            mask = np.ones((height, width), dtype=bool)
            mask[yy:yy + hh, xx:xx + ww] = False
            masked = delta.copy()
            masked[~mask] = 0
            fraction = float((masked > 40).sum()) / int(mask.sum())
            cropped = masked[:height // 8 * 8, :width // 8 * 8]
            blocks = cropped.reshape(height // 8, 8, width // 8, 8).mean(axis=(1, 3))
            worst = float(blocks.max())
            require(fraction <= .003 and worst <= 8.0, "Independent golden failure " + name)
            reported = (f"golden {name}: changed {fraction * 100:.4f} % of pixels (limit 0.30 %), "
                        f"worst 8x8 block mean {worst:.2f} (limit 8.0)")
            require(reported in smoke and reported in log, "Reported/actual golden metrics differ")
            (extract / (name + ".png")).write_bytes(candidate)
            (extract / ("diff-" + name + ".png")).write_bytes(
                archive.read("golden/golden-diff-" + name + ".png"))
            goldens.append({"size": [width, height], "candidate_sha256": sha(candidate),
                            "source_golden_sha256": sha(baseline),
                            "all_rgb_pixels_equal": bool(np.array_equal(before, after)),
                            "all_pixels_equal": bool(np.array_equal(
                                np.asarray(Image.open(io.BytesIO(baseline)).convert("RGBA")),
                                np.asarray(Image.open(io.BytesIO(candidate)).convert("RGBA")))),
                            "outside_mask_changed_fraction": fraction,
                            "worst_8x8_block_mean": worst,
                            "masked_rectangle": [xx, yy, ww, hh],
                            "unchanged_limits_passed": True})
        for name in ("run.log", "provenance.json", "SHA256SUMS",
                     "studio-settings-1120x700.png", "studio-settings-1600x1000.png",
                     "main-source-performance-headroom-1600x1000.png"):
            (extract / name).write_bytes(archive.read(name))
    summary = {
        "source_executed": SOURCE, "native_release_source_compared": NATIVE_SOURCE,
        "execution_relabelled": False, "native_run_qualified": False,
        "run_id": RUN, "run_attempt": ATTEMPT, "job_id": JOB,
        "run_and_job_conclusion": "success", "artifact": api,
        "actual_zip": {"bytes": len(blob), "sha256": digest},
        "producer_upload_bound": True,
        "producer_upload_step": upload,
        "provenance": provenance, "file_count": len(records), "png_count": len(pngs),
        "top_level_png_count": len(checks), "all_archive_entries_reviewed": True,
        "all_pngs_decoded": True, "exact_expected_roster_passed": True,
        "top_level_png_checksums_passed": True,
        "ui_smoke": "UI SMOKE TEST PASSED",
        "core_reported_checks_passed": int(core_result.group(1)),
        "smoke_reported_phase_count": len(phases),
        "smoke_reported_phases": phases,
        "smoke_last_reported_total_seconds": float(phase_timings[-1][2]),
        "original_capture_exit_code": 0,
        "exit_code_evidence": "Successful original capture API step plus unchanged source wrapper/capture exit-status propagation; no retry.",
        "no_failed_assertions_in_full_smoke_log": True, "golden_refresh_used": False,
        "goldens": goldens, "producer_log_sha256": sha(raw_log),
        "artifact_smoke_log_sha256": sha(smoke_bytes),
        "main_guard_skipped_gallery_publication": True,
        "main_guard_original_log_message": publication_guard,
        "gallery_is_not_golden_coverage": True,
        "scope": "Actual 09af screenshot/UI artifact qualification; identical repository UI inputs at db6b. No native plugin package or acoustic-quality claim. Only two fixture candidates are golden comparisons; the other rendered gallery images are not golden tests.",
    }
    output = Path(args.out)
    output.write_text(json.dumps(summary, indent=2) + "\n")
    output.with_name("actual-artifact-members.json").write_text(json.dumps(records, indent=2) + "\n")
    print(json.dumps({key: summary[key] for key in (
        "source_executed", "file_count", "png_count", "top_level_png_count", "ui_smoke",
        "goldens", "main_guard_skipped_gallery_publication")}, indent=2))


if __name__ == "__main__":
    main()
