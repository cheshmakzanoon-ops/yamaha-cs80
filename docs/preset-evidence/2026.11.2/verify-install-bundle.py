#!/usr/bin/env python3
"""Verify one newly downloaded CS80 install artifact; never execute its payload.

This driver requires complete, freshly obtained GitHub artifact and workflow-run
API objects. Their authenticity/freshness is a caller responsibility. It reuses
the reviewed V3 driver and source-pinned inner verifier, then checks the missing
API, license and dependency evidence contract. Success is artifact consistency,
not a passed validator, a listening evaluation, or proof of hardware equivalence.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import stat
import subprocess
import sys
import tarfile
import tempfile
import zipfile

sys.dont_write_bytecode = True
REPOSITORY = "cheshmakzanoon-ops/yamaha-cs80"
REVISION = "2026.11.2"
JUCE = "c352e2489000ffeb9e397051377d75eef2efe641"
JUCE_LICENSE = "2633539bb26d244f0966fbc4df59400ea99bdf575fa96291851c1c3ff3456146"
V3 = "docs/preset-evidence/2026.11.1/ci/artifact-verifier-v3.py"
V3_SHA = "e310f7ef18eb2c1c47f2063b7bf5f6e265e2a2f278c6e19ee0e0f898d0792de6"
TARGETS = {
    "linux-x86_64": ("linux", "x86_64", "cs80-linux-install-bundle"),
    "macos-arm64": ("macos", "arm64", "cs80-macos-15-install-bundle"),
    "macos-x86_64": ("macos", "x86_64", "cs80-macos-15-intel-install-bundle"),
    "windows-x86_64": ("windows", "x86_64", "cs80-windows-install-bundle"),
}
NOTICES = {
    "LICENSE": "LICENSE",
    "licenses/BarlowCondensed-OFL.txt": "assets/fonts/barlow-condensed/OFL.txt",
    "licenses/BarlowCondensed-SOURCE.txt": "assets/fonts/barlow-condensed/SOURCE.txt",
    "licenses/MTS-ESP-LICENSE.txt": "src/vendor/mts-esp/LICENSE",
    "licenses/libxml2-Copyright.txt": "src/vendor/libxml2/Copyright",
    "licenses/libxml2-SOURCE.txt": "src/vendor/libxml2/SOURCE.txt",
    "licenses/libxml2-ADDITIONAL-NOTICES.txt": "src/vendor/libxml2/ADDITIONAL-NOTICES.txt",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def sha_file(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def unique_object(pairs):
    out = {}
    for key, value in pairs:
        require(key not in out, "Duplicate JSON key: " + key)
        out[key] = value
    return out


def read_json(path):
    require(path.is_file() and path.stat().st_size <= 4 * 1024 * 1024, "Missing/oversized JSON: " + str(path))
    return json.loads(path.read_bytes(), object_pairs_hook=unique_object)


def git_bytes(repo, source, name):
    return subprocess.check_output(["git", "-C", str(repo), "show", f"{source}:{name}"])


def check_api(meta, run, target, source, run_id, attempt, archive):
    require(isinstance(meta, dict) and isinstance(run, dict), "Expected complete API objects")
    require(type(run_id) is int and run_id > 0 and type(attempt) is int and attempt > 0, "Invalid expected run identity")
    require(run.get("id") == run_id and run.get("run_attempt") == attempt, "Workflow run/attempt mismatch")
    require(run.get("head_sha") == source and run.get("head_branch") == "main", "Workflow source/branch mismatch")
    require(run.get("repository", {}).get("full_name") == REPOSITORY, "Wrong workflow repository")
    require(run.get("event") == "push" and run.get("path") == ".github/workflows/build.yml", "Wrong source workflow/event")
    require(run.get("status") in ("in_progress", "completed"), "Workflow has not started")
    require(meta.get("name") == TARGETS[target][2] and meta.get("expired") is False, "Wrong/expired artifact")
    require(type(meta.get("id")) is int and meta["id"] > 0, "Invalid artifact ID")
    artifact_run = meta.get("workflow_run", {})
    require(artifact_run.get("id") == run_id and artifact_run.get("head_sha") == source
            and artifact_run.get("head_branch") == "main", "Artifact source/run/branch mismatch")
    require(type(meta.get("size_in_bytes")) is int and 0 < meta["size_in_bytes"] <= 520 * 1024 * 1024,
            "Invalid/oversized artifact")
    require(re.fullmatch(r"sha256:[0-9a-f]{64}", meta.get("digest", "")), "Missing/invalid API SHA256")
    require(archive.is_file() and archive.stat().st_size == meta["size_in_bytes"], "Artifact byte-size mismatch")
    digest = sha_file(archive)
    require(digest == meta["digest"][7:], "Artifact SHA256 mismatch")
    return digest


def exact_outer_shape(archive, stem, extension):
    expected = {stem + extension, stem + ".manifest.json", "SHA256SUMS"}
    # Full path, compression and resource preflight is performed by the V3 driver.
    with zipfile.ZipFile(archive) as outer:
        items = outer.infolist()
        require(len(items) == 3 and {e.filename for e in items} == expected, "Unexpected outer ZIP shape")
        require(all(not e.is_dir() and e.orig_filename == e.filename
                    and stat.S_IFMT(e.external_attr >> 16) in (0, stat.S_IFREG)
                    and not (e.flag_bits & 1) for e in items), "Unsupported outer ZIP member")


def additional_metadata(inner, manifest, stem, repo, source, run_id, system):
    require(manifest.get("juce_sha") == JUCE and manifest.get("juce_version") == "8.0.11", "Wrong JUCE manifest identity")
    require(manifest.get("preset_library_revision") == REVISION, "Wrong preset revision")
    require(manifest.get("workflow_run_id") == str(run_id), "Wrong manifest run")
    inventory = {x["path"]: x for x in manifest["files"]}
    required = set(NOTICES) | {"licenses/JUCE-LICENSE.md", "evidence/juce-provenance.txt", "SOURCE.txt",
                              "INSTALL_AND_QUICKSTART.md", "PRESET_GUIDE.md", "DISTRIBUTION_STATUS.md"}
    require(all(inventory.get(name, {}).get("type") == "file" for name in required), "Missing regular notice/guide/provenance file")
    result = {"required_metadata_files": sorted(required), "notices": []}
    with (zipfile.ZipFile(inner) if system == "windows" else tarfile.open(inner, "r:gz")) as bundle:
        def read(name):
            require(inventory[name]["size"] <= 2 * 1024 * 1024, "Oversized metadata")
            if system == "windows":
                return bundle.read(stem + "/" + name)
            with bundle.extractfile(stem + "/" + name) as stream:
                return stream.read(2 * 1024 * 1024 + 1)

        for name, source_path in NOTICES.items():
            actual, expected = read(name), git_bytes(repo, source, source_path)
            require(actual and expected, "Empty notice: " + name)
            crlf = expected.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
            require(actual == expected or (system == "windows" and actual == crlf), "Source notice mismatch: " + name)
            result["notices"].append({"path": name, "source_path": source_path, "bytes": len(actual),
                                      "sha256": sha(actual), "source_sha256": sha(expected),
                                      "exact_source_bytes": actual == expected,
                                      "windows_checkout_crlf_only": actual != expected})
        juce_license = read("licenses/JUCE-LICENSE.md")
        require(sha(juce_license) == JUCE_LICENSE or
                (system == "windows" and sha(juce_license.replace(b"\r\n", b"\n")) == JUCE_LICENSE), "JUCE license differs from pinned dependency")
        result["juce_license"] = {"sha256": sha(juce_license), "pinned_source_sha256": JUCE_LICENSE,
                                   "source_commit": JUCE, "exact_source_bytes": sha(juce_license) == JUCE_LICENSE}
        fields = {}
        for line in read("evidence/juce-provenance.txt").decode().splitlines():
            if "=" in line:
                key, value = line.split("=", 1)
                require(key not in fields, "Duplicate JUCE provenance key")
                fields[key] = value
        require(fields.get("actual_sha") == JUCE and fields.get("actual_version") == "8.0.11"
                and fields.get("workflow_commit") == source and fields.get("verification") == "PASS"
                and fields.get("source_clean") == "true", "Wrong/unverified JUCE evidence")
        require(fields.get("configured_source", "").replace("\\", "/").endswith("/build/_deps/juce-src"), "Unexpected fetched JUCE source path")
        result["juce_provenance"] = fields
    result["inventory"] = [{k: item[k] for k in ("path", "type", "size", "sha256", "mode", "target") if k in item}
                           for item in manifest["files"]]
    result["inventory_count"] = len(manifest["files"])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--run-id", type=int, required=True)
    parser.add_argument("--run-attempt", type=int, default=1)
    parser.add_argument("--target", choices=TARGETS, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--artifact-json", type=Path, required=True)
    parser.add_argument("--run-json", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    require(re.fullmatch(r"[0-9a-f]{40}", args.source), "Expected full lowercase source SHA")
    require(not args.out.exists(), "Proof output already exists")
    require(args.work_dir.is_dir(), "Work directory does not exist")
    require(not args.work_dir.resolve().is_relative_to(args.source_root.resolve()), "Work directory must be outside repository")
    meta, run = read_json(args.artifact_json), read_json(args.run_json)
    outer_sha = check_api(meta, run, args.target, args.source, args.run_id, args.run_attempt, args.archive)
    system, arch, artifact_name = TARGETS[args.target]
    stem = f"cs80-reference-{system}-{arch}-{args.source[:12]}"
    extension = ".zip" if system == "windows" else ".tar.gz"
    driver = git_bytes(args.source_root, args.source, V3)
    require(sha(driver) == V3_SHA, "Unreviewed V3 verification driver")
    with tempfile.TemporaryDirectory(prefix="cs80-bundle-check-", dir=args.work_dir) as temporary:
        temp = Path(temporary)
        driver_path = temp / "artifact-verifier-v3.py"
        driver_path.write_bytes(driver)
        command = [sys.executable, "-B", str(driver_path), "--source-root", str(args.source_root),
                   "--expected-commit", args.source, "--expected-run-id", str(args.run_id),
                   "--expected-preset-revision", REVISION, "--artifact", args.target, str(args.archive.resolve()),
                   "--outer-sha256", args.target, outer_sha, "--out-dir", str(temp / "review")]
        completed = subprocess.run(command, capture_output=True, text=True, timeout=300)
        require(completed.returncode == 0, "Source-pinned V3 verification failed: " + (completed.stdout + completed.stderr)[-4000:])
        exact_outer_shape(args.archive, stem, extension)
        v3 = read_json(temp / "review/report.json")
        require(v3.get("status") == "PASS" and len(v3.get("artifacts", [])) == 1, "V3 result failed")
        inner = temp / "review" / args.target / (stem + extension)
        manifest = read_json(inner.with_name(stem + ".manifest.json"))
        extra = additional_metadata(inner, manifest, stem, args.source_root, args.source, args.run_id, system)
        report = {"schema": 1, "assignment": 51, "status": "artifact_bytes_verified",
                  "verified_at_utc": datetime.now(timezone.utc).isoformat(),
                  "checker_sha256": sha_file(Path(__file__)), "source": args.source, "run_id": args.run_id,
                  "run_attempt": args.run_attempt, "target": args.target, "artifact_name": artifact_name,
                  "artifact_api": meta, "run_api": run,
                  "archive": {"path": str(args.archive.resolve()), "bytes": args.archive.stat().st_size,
                              "sha256": outer_sha, "api_digest_matches": True,
                              "public_url": f"https://nightly.link/{REPOSITORY}/actions/artifacts/{meta['id']}.zip"},
                  "inner_archive": {"name": inner.name, "bytes": inner.stat().st_size, "sha256": sha_file(inner)},
                  "manifest_identity": {k: manifest[k] for k in ("commit", "platform", "architecture", "formats",
                                          "preset_library_revision", "workflow_run_id", "juce_sha", "juce_version")},
                  "v3_verification": v3, "additional_metadata": extra,
                  "limitations": "Caller supplies authenticated fresh API records. Integrity/source-run consistency, product inventory and binary headers only. No downloaded product executed, no native CI/validator success inferred. No signing, listening, acoustic identity or DAW qualification. Package does not enumerate runtime presets; the separate audition inventory verifies all523 addresses. Temporary extracted archive removed after verification."}
        with args.out.open("x") as stream:
            json.dump(report, stream, indent=2)
            stream.write("\n")
    print(json.dumps({"status": report["status"], "target": args.target, "artifact_id": meta["id"],
                      "sha256": outer_sha, "inventory_count": extra["inventory_count"],
                      "formats": manifest["formats"], "proof": str(args.out)}, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError,
            tarfile.TarError, zipfile.BadZipFile) as error:
        print("Bundle acceptance failed: " + str(error), file=sys.stderr)
        raise SystemExit(1)
