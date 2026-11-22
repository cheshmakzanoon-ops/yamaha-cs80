#!/usr/bin/env python3
"""V3 review of patch-revision install ZIPs; never execute their binaries.

The repository's verifier owns all inner-archive validation. This scratch driver
replays the reviewed source-commit Markdown transformation, checks patch revision
and source/run/target identity, and inspects headers. Integrity is not CI success.
The v2 verifier and its 2026.11 evidence are separate and remain unchanged.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
import tarfile
import tempfile
import zipfile

sys.dont_write_bytecode = True
DRIVER_VERSION = "3.0"
CONTRACT_REVIEWED_AT = "be460b0b93bdcb8dce5eb8925b18b72706b25e19"
REVIEWED_SCRIPT_SHA256 = {
    "scripts/package-ci-artifacts.py": "f2df2edfd4a55a759ad5c0bad72fa05a5f3880d323412e1211ede1d15bdaa8cf",
    "scripts/verify-ci-bundle.py": "65b02309dfd8ff3b3f05585df91e6eb48a6a116563327c63d24d0b516d0a63aa",
}
TARGETS = {
    "linux-x86_64": ("linux", "x86_64", "cs80-linux-install-bundle"),
    "macos-arm64": ("macos", "arm64", "cs80-macos-15-install-bundle"),
    "macos-x86_64": ("macos", "x86_64", "cs80-macos-15-intel-install-bundle"),
    "windows-x86_64": ("windows", "x86_64", "cs80-windows-install-bundle"),
}
DOCUMENTS = {
    "PRESET_GUIDE.md": "docs/PRESET_GUIDE.md",
    "INSTALL_AND_QUICKSTART.md": "docs/INSTALL_AND_QUICKSTART.md",
    "DISTRIBUTION_STATUS.md": "docs/CI_DISTRIBUTION_STATUS.md",
}
LIMITATION = ("Artifact integrity, declared source/run consistency and binary-header identity only; "
              "not publisher authenticity, proof of compilation from source, signing/notarization, "
              "CI pass, runtime behavior, or DAW qualification.")


def require(ok, message):
    if not ok:
        raise ValueError(message)


def git_bytes(repo, commit, name):
    return subprocess.check_output(["git", "-C", str(repo), "show", f"{commit}:{name}"])


def mappings(pairs, what):
    result = {}
    for target, value in pairs:
        require(target in TARGETS, f"Unknown {what} target: {target}; use {', '.join(TARGETS)}")
        require(target not in result, f"Duplicate {what} target: {target}")
        result[target] = value
    return result


def unpack_outer(outer, destination, stem, extension, verifier):
    """Copy only three selected regular files to fixed names, never extractall.

    The outer ZIP has a small central-directory preflight of its own. The inner
    archive's substantially different validation stays in verify-ci-bundle.py.
    """
    maximum = verifier.MAX_ARCHIVE + 2 * verifier.MAX_METADATA
    require(outer.is_file() and outer.stat().st_size <= maximum + 2**20,
            f"Missing or oversized outer ZIP: {outer}")
    with outer.open("rb") as stream:
        stream.seek(max(0, outer.stat().st_size - 65557))
        tail = stream.read(65557)
    offset = tail.rfind(b"PK\x05\x06")
    require(offset >= 0 and len(tail) - offset >= 22, "Outer ZIP end record missing")
    end = struct.unpack("<4s4H2IH", tail[offset:offset + 22])
    require(offset + 22 + end[7] == len(tail) and end[1] == end[2] == 0
            and end[3] == end[4] and end[4] <= 128
            and end[5] <= verifier.MAX_METADATA and end[6] != 0xffffffff
            and (offset < 20 or tail[offset - 20:offset - 16] != b"PK\x06\x07"),
            "Unsupported/oversized outer ZIP directory (classic single-disk ZIP required)")
    wanted = {stem + extension, stem + ".manifest.json", "SHA256SUMS"}
    listing, selected, seen = [], {}, set()
    with zipfile.ZipFile(outer) as bundle:
        entries = bundle.infolist()
        require(len(entries) <= 128 and sum(e.file_size for e in entries) <= maximum,
                "Outer ZIP member/expanded-size limit exceeded")
        for entry in entries:
            name = entry.filename[:-1] if entry.is_dir() else entry.filename
            verifier.safe_name(name)
            require(entry.orig_filename == entry.filename, "Outer ZIP name contains NUL")
            require(name.casefold() not in seen, "Duplicate/ambiguous outer ZIP member")
            seen.add(name.casefold())
            kind = stat.S_IFMT(entry.external_attr >> 16)
            require(kind in (0, stat.S_IFREG, stat.S_IFDIR) and not entry.flag_bits & 1,
                    f"Unsupported/encrypted outer ZIP member: {name}")
            require((kind != stat.S_IFDIR or entry.is_dir())
                    and (kind != stat.S_IFREG or not entry.is_dir())
                    and (not entry.is_dir() or entry.file_size == 0), "Outer ZIP type mismatch")
            basename = name.rsplit("/", 1)[-1]
            listing.append({"path": entry.filename, "size": entry.file_size,
                            "selected": not entry.is_dir() and basename in wanted})
            if not entry.is_dir() and basename in wanted:
                require(basename not in selected, f"Ambiguous outer ZIP selection: {basename}")
                selected[basename] = entry
        require(set(selected) == wanted, f"Missing expected bundle files: {sorted(wanted - selected.keys())}")
        require(len({e.filename.rpartition('/')[0] for e in selected.values()}) == 1,
                "Archive, sidecar and checksums are not in the same outer-ZIP directory")
        for basename, entry in selected.items():
            limit = verifier.MAX_ARCHIVE if basename == stem + extension else verifier.MAX_METADATA
            require(entry.file_size <= limit, f"Oversized selected member: {basename}")
            with bundle.open(entry) as source, (destination / basename).open("xb") as output:
                count = 0
                while chunk := source.read(1024 * 1024):
                    count += len(chunk)
                    require(count <= min(limit, entry.file_size), "Outer member exceeded declared size")
                    output.write(chunk)
                require(count == entry.file_size, "Truncated outer ZIP member")
    return listing


def binary_paths(product, system, arch):
    if system == "macos":
        return [(f"VST3/{product}.vst3/Contents/MacOS/{product}", False),
                (f"AU/{product}.component/Contents/MacOS/{product}", False),
                (f"Standalone/{product}.app/Contents/MacOS/{product}", True)]
    if system == "linux":
        return [(f"VST3/{product}.vst3/Contents/{arch}-linux/{product}.so", False),
                (f"Standalone/{product}", True)]
    return [(f"VST3/{product}.vst3/Contents/x86_64-win/{product}.vst3", False),
            (f"Standalone/{product}.exe", True)]


def document_identity(archived, raw_source, expected_packaged, name):
    require(archived == expected_packaged,
            f"{name} differs from the exact source-bound packaged_markdown output")
    archived_sha = hashlib.sha256(archived).hexdigest()
    return {"bytes": len(archived), "source_bytes": len(raw_source), "sha256": archived_sha,
            "source_sha256": hashlib.sha256(raw_source).hexdigest(), "archived_sha256": archived_sha,
            "expected_packaged_sha256": hashlib.sha256(expected_packaged).hexdigest(),
            "exact_source_bytes": archived == raw_source, "exact_source_bound_packaged_bytes": True,
            "source_transform": "source-commit packaged_markdown with pinned source and bundled destinations",
            "packaged_newline_policy": "LF on all platforms via universal-newline text read; no archive normalization"}


def additional_checks(archive, manifest, stem, args, documents, verifier):
    p = verifier.packager
    system, arch = manifest["platform"], manifest["architecture"]
    require(manifest.get("preset_library_revision") == args.expected_preset_revision,
            "Manifest preset_library_revision mismatch")
    if args.expected_run_id is not None:
        require(str(manifest.get("workflow_run_id")) == args.expected_run_id, "Manifest workflow_run_id mismatch")
    inventory = {item["path"]: item for item in manifest["files"]}
    archive_reader = zipfile.ZipFile(archive) if system == "windows" else tarfile.open(archive, "r:gz")
    checks = {"preset_library_revision": manifest["preset_library_revision"],
              "workflow_run_id": manifest.get("workflow_run_id"), "documents": [], "binaries": []}
    with archive_reader as bundle:
        def member_stream(name):
            # Inner inventory, member types, path/link safety and digests were
            # already validated by the existing verifier before this is called.
            stream = (bundle.open(stem + "/" + name) if system == "windows"
                      else bundle.extractfile(stem + "/" + name))
            require(stream is not None, f"Cannot read verified archive member: {name}")
            return stream

        def metadata_bytes(name):
            require(inventory.get(name, {}).get("type") == "file", f"Missing regular metadata file: {name}")
            with member_stream(name) as stream:
                data = stream.read(verifier.MAX_METADATA + 1)
            require(len(data) <= verifier.MAX_METADATA, f"Oversized metadata: {name}")
            return data

        for name, expected in documents.items():
            data = metadata_bytes(name)
            checks["documents"].append({"path": name, "source_path": DOCUMENTS[name],
                                        **document_identity(data, expected["raw_source"], expected["packaged"], name)})
        source = metadata_bytes("SOURCE.txt")
        expected_lines = [
            f"Source commit: {args.expected_commit}",
            f"https://github.com/cheshmakzanoon-ops/yamaha-cs80/tree/{args.expected_commit}",
            f"Preset library revision: {args.expected_preset_revision}",
            f"Online preset guide: https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/{args.expected_commit}/docs/PRESET_GUIDE.md",
            "Use the full source at this commit; this archive is not a source distribution.",
        ]
        require(source == ("\n".join(expected_lines) + "\n").encode(), "SOURCE.txt commit/revision/URLs/content mismatch")
        checks["source_txt"] = {"commit": args.expected_commit, "sha256": p.digest(source),
                                "source_sha256": p.digest(source), "archived_sha256": p.digest(source),
                                "exact_expected_bytes": True, "exact_source_bytes": True,
                                "normalized_source_content_equal": True, "newline_transform": "none"}
        # Only ephemeral flat copies, created without execute permission, are
        # used for the existing path-based header inspector; no payload is run.
        with tempfile.TemporaryDirectory(prefix="header-inspection-", dir=archive.parent) as temporary:
            for index, (name, standalone) in enumerate(binary_paths(p.PRODUCT, system, arch)):
                copy = Path(temporary) / f"binary-{index}.data"
                with member_stream(name) as stream, copy.open("xb") as output:
                    count = 0
                    while chunk := stream.read(1024 * 1024):
                        count += len(chunk)
                        require(count <= verifier.MAX_FILE, f"Oversized binary: {name}")
                        output.write(chunk)
                copy.chmod(0o600)
                p.verify_binary(copy, system, arch, standalone)
                checks["binaries"].append({"path": name, "bytes": count, "sha256": p.sha_file(copy),
                                           "platform": system, "architecture": arch,
                                           "role": "standalone" if standalone else "plugin", "header_check": "PASS"})
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--expected-commit", required=True)
    parser.add_argument("--expected-run-id", required=True, help="Expected GitHub workflow run ID")
    parser.add_argument("--expected-preset-revision", default="2026.11.1")
    parser.add_argument("--artifact", action="append", nargs=2, metavar=("TARGET", "OUTER_ZIP"), required=True)
    parser.add_argument("--outer-sha256", action="append", nargs=2, metavar=("TARGET", "SHA256"), default=[],
                        help="Optional trusted Actions outer-artifact digest; accepts sha256: prefix")
    parser.add_argument("--require-all", action="store_true", help="Require all four target inputs")
    parser.add_argument("--out-dir", type=Path, required=True, help="A new scratch directory outside the repository")
    args = parser.parse_args()
    require(re.fullmatch(r"[0-9a-f]{40}", args.expected_commit), "Expected a full lowercase source commit SHA")
    require(re.fullmatch(r"[0-9]{4}\.(0[1-9]|1[0-2])(?:\.[1-9][0-9]*)?", args.expected_preset_revision),
            "Invalid YYYY.MM or YYYY.MM.PATCH revision")
    require(args.expected_run_id.isdecimal(), "Expected numeric workflow run ID")
    artifacts = mappings(args.artifact, "artifact")
    outer_digests = mappings(args.outer_sha256, "outer SHA256")
    require(set(outer_digests) <= set(artifacts), "An outer SHA256 has no corresponding artifact")
    outer_digests = {key: value.removeprefix("sha256:") for key, value in outer_digests.items()}
    require(all(re.fullmatch(r"[0-9a-f]{64}", value) for value in outer_digests.values()), "Invalid outer SHA256")
    require(not args.require_all or set(artifacts) == set(TARGETS), "All four target artifacts are required")
    repo, output = args.source_root.resolve(), args.out_dir.resolve()
    require(not output.is_relative_to(repo), "Output must be outside the source repository")
    require(not output.exists(), "Output directory already exists; use a fresh scratch directory")
    scripts = ("scripts/verify-ci-bundle.py", "scripts/package-ci-artifacts.py")
    source_scripts = {name: git_bytes(repo, args.expected_commit, name) for name in scripts}
    for name, source in source_scripts.items():
        require(hashlib.sha256(source).hexdigest() == REVIEWED_SCRIPT_SHA256[name],
                f"Unreviewed packaging/verifier contract at expected commit: {name}; review before updating V3")
    raw_documents = {name: git_bytes(repo, args.expected_commit, path) for name, path in DOCUMENTS.items()}
    header_bytes = git_bytes(repo, args.expected_commit, "src/core/FactoryPresets.h")
    header = header_bytes.decode("utf-8")
    revisions = re.findall(r'^inline constexpr const char\* curatedLibraryRevision = "([^"\r\n]*)";$', header, re.MULTILINE)
    require(revisions == [args.expected_preset_revision], "Expected commit's canonical preset revision mismatch")
    # Only immutable Git blobs define this source snapshot. The working checkout
    # may contain newer concurrent refinements and is neither compared nor changed.
    output.mkdir(parents=True)
    snapshot = output / "source-snapshot"
    snapshot_files = dict(source_scripts)
    snapshot_files.update({DOCUMENTS[name]: data for name, data in raw_documents.items()})
    snapshot_files["src/core/FactoryPresets.h"] = header_bytes
    for name, source in snapshot_files.items():
        destination = snapshot / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(source)
    verifier_path = snapshot / "scripts/verify-ci-bundle.py"
    spec = importlib.util.spec_from_file_location("cs80_review_verifier", verifier_path)
    verifier = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(verifier)
    require(verifier.packager.read_preset_library_revision() == args.expected_preset_revision,
            "Source packager's canonical revision reader disagrees")
    destinations = {source: destination for destination, source in DOCUMENTS.items()}
    documents = {name: {"raw_source": raw_documents[name],
                        "packaged": verifier.packager.packaged_markdown(source, args.expected_commit, destinations)}
                 for name, source in DOCUMENTS.items()}
    report = {"driver": {"version": DRIVER_VERSION, "path": str(Path(__file__).resolve()),
                         "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
              "expected_commit": args.expected_commit, "expected_run_id": args.expected_run_id,
              "expected_preset_revision": args.expected_preset_revision, "source_root": str(repo),
              "packaging_contract": {"reviewed_at_commit": CONTRACT_REVIEWED_AT,
                                     "script_sha256": REVIEWED_SCRIPT_SHA256,
                                     "source_read_policy": "immutable Git blobs at expected commit; concurrent worktree untouched",
                                     "guide_destinations": destinations,
                                     "guide_comparison": "exact source-commit packaged_markdown output on every platform"},
              "verifier_source_sha256": {name: hashlib.sha256(data).hexdigest() for name, data in source_scripts.items()},
              "all_four_targets_supplied": set(artifacts) == set(TARGETS),
              "qualification": LIMITATION, "artifacts": []}
    for target, supplied_path in artifacts.items():
        system, arch, artifact_name = TARGETS[target]
        folder = output / target
        folder.mkdir()
        result = {"target": target, "expected_artifact_name": artifact_name,
                  "outer_zip": str(Path(supplied_path).resolve()), "status": "FAIL"}
        try:
            outer = Path(supplied_path).resolve()
            require(outer.is_file() and outer.stat().st_size <= verifier.MAX_ARCHIVE + 2 * verifier.MAX_METADATA + 2**20,
                    f"Missing or oversized outer ZIP: {outer}")
            result["outer_sha256"] = verifier.packager.sha_file(outer)
            result["trusted_expected_outer_sha256"] = outer_digests.get(target)
            result["trusted_outer_sha256_verified"] = False
            if target in outer_digests:
                require(result["outer_sha256"] == outer_digests[target], "Trusted outer-artifact SHA256 mismatch")
                result["trusted_outer_sha256_verified"] = True
            stem = f"cs80-reference-{system}-{arch}-{args.expected_commit[:12]}"
            extension = ".zip" if system == "windows" else ".tar.gz"
            result["outer_members"] = unpack_outer(outer, folder, stem, extension, verifier)
            archive = folder / (stem + extension)
            command = [sys.executable, "-B", str(verifier_path), "--archive", str(archive),
                       "--expected-commit", args.expected_commit]
            process = subprocess.run(command, capture_output=True, text=True, timeout=180,
                                     env=dict(os.environ, PYTHONDONTWRITEBYTECODE="1"))
            (folder / "verify-ci-bundle.log").write_text(process.stdout + process.stderr, encoding="utf-8")
            result["verifier_command"] = command
            result["verifier_exit_code"] = process.returncode
            require(process.returncode == 0, "Existing verifier failed; see verify-ci-bundle.log")
            result["existing_verifier"] = json.loads(process.stdout)
            manifest = json.loads((folder / (stem + ".manifest.json")).read_bytes())
            require((manifest["platform"], manifest["architecture"]) == (system, arch), "Declared target mismatch")
            result["archive"] = str(archive)
            result["archive_sha256"] = verifier.packager.sha_file(archive)
            result["manifest_sha256"] = verifier.packager.sha_file(folder / (stem + ".manifest.json"))
            result["checks"] = additional_checks(archive, manifest, stem, args, documents, verifier)
            result["status"] = "PASS"
        except Exception as error:
            result["error"] = f"{type(error).__name__}: {error}"
        (folder / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        report["artifacts"].append(result)
        print(f"{target}: {result['status']}" + (f" — {result['error']}" if "error" in result else ""), flush=True)
    report["status"] = "PASS" if all(item["status"] == "PASS" for item in report["artifacts"]) else "FAIL"
    report["all_four_targets_verified"] = report["all_four_targets_supplied"] and report["status"] == "PASS"
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Report: {output / 'report.json'}\n{LIMITATION}")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f"Review setup failed: {error}", file=sys.stderr)
        raise SystemExit(2)
