#!/usr/bin/env python3
"""Read-only verification of the exact Apple Silicon CI archive; no binary loads."""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import plistlib
import re
import shutil
import struct
import tarfile
import tempfile
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(__file__).resolve().parent
CACHE = ROOT / "build-coordination/artifact-cache"
CACHE.mkdir(parents=True, exist_ok=True)
SOURCE = "6d3cc220edef5a4a2c7cd968ea8a786c7c83198c"
RUN_ID = 45044040694
ARTIFACT = "cs80-macos-15-install-bundle"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--artifact-json", type=Path, required=True)
parser.add_argument("--run-json", type=Path, required=True)
args = parser.parse_args()
meta = json.loads(args.artifact_json.read_text())
run = json.loads(args.run_json.read_text())
assert run["id"] == RUN_ID and run["head_sha"] == SOURCE
assert run["run_attempt"] == 1 and run["head_branch"] == "main" and run["event"] == "push"
assert run["name"] == "build" and run["path"] == ".github/workflows/build.yml"
assert run["head_repository"]["full_name"] == "cheshmakzanoon-ops/yamaha-cs80"
assert meta["name"] == ARTIFACT and not meta["expired"]
assert meta["workflow_run"]["id"] == RUN_ID
assert meta["workflow_run"]["head_sha"] == SOURCE
assert meta["workflow_run"]["head_branch"] == "main"
assert re.fullmatch(r"sha256:[0-9a-f]{64}", meta["digest"])
assert type(meta["id"]) is int and 0 < meta["size_in_bytes"] < 512 * 1024 * 1024
outer = CACHE / (ARTIFACT + "-" + str(meta["id"]) + ".zip")
url = "https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/" + str(meta["id"]) + ".zip"
assert shutil.disk_usage(WORK).free > 2 * meta["size_in_bytes"] + 32 * 1024 * 1024, "Insufficient verification space"

def sha_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()

if not outer.exists():
    part = outer.with_suffix(".zip.part")
    try:
        request = urllib.request.Request(url, headers={"User-Agent": "CS80-AppleSilicon-package-verification/1.0"})
        with urllib.request.urlopen(request, timeout=45) as response, part.open("wb") as target:
            assert response.status == 200
            received = 0
            while chunk := response.read(1024 * 1024):
                received += len(chunk)
                assert received <= meta["size_in_bytes"], "Download exceeds API artifact size"
                target.write(chunk)
        part.replace(outer)
    finally:
        part.unlink(missing_ok=True)
outer_sha = sha_file(outer)
assert outer.stat().st_size == meta["size_in_bytes"]
assert outer_sha == meta["digest"].split(":")[1]
spec = importlib.util.spec_from_file_location("cs80_publisher_offline", ROOT / "scripts/publish-preset-refinement.py")
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)
stem = "cs80-reference-macos-arm64-" + SOURCE[:12]
product = "CS-80 Reference"
expected_images = {
    "VST3/" + product + ".vst3/Contents/MacOS/" + product: {6, 8},
    "AU/" + product + ".component/Contents/MacOS/" + product: {6, 8},
    "Standalone/" + product + ".app/Contents/MacOS/" + product: {2},
}

with tempfile.TemporaryDirectory(prefix="offline-bundle-", dir=WORK) as temporary:
    # Only trusted repository code runs. The artifact's products are never loaded.
    verification = publisher.verify_bundle(outer, "macos", "arm64", SOURCE, RUN_ID, Path(temporary))
    inner = Path(temporary) / "macos/arm64" / (stem + ".tar.gz")
    manifest_path = inner.with_name(stem + ".manifest.json")
    manifest = json.loads(manifest_path.read_text())
    inner_identity = {"name": inner.name, "bytes": inner.stat().st_size, "sha256": sha_file(inner)}
    binaries, bundles, guides = [], [], []
    with tarfile.open(inner, "r:gz") as archive:
        members = {member.name: member for member in archive.getmembers()}
        def read(name):
            member = members[stem + "/" + name]
            assert member.isfile(), name
            with archive.extractfile(member) as stream:
                return stream.read()
        source_text = read("SOURCE.txt").decode()
        provenance = dict(line.split("=", 1) for line in read("evidence/juce-provenance.txt").decode().splitlines() if "=" in line)
        assert provenance["actual_sha"] == publisher.JUCE_SHA
        assert provenance["actual_version"] == "8.0.11"
        assert provenance["workflow_commit"] == SOURCE
        assert provenance["verification"] == "PASS" and provenance["source_clean"] == "true"
        # Inspect every regular member for Mach-O magic, not only expected filenames.
        for member in members.values():
            if not member.isfile() or member.size < 4:
                continue
            with archive.extractfile(member) as stream:
                header = stream.read(32)
            magic = header[:4]
            if magic not in (b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf"):
                continue
            relative = member.name[len(stem) + 1:]
            assert relative in expected_images, "Unexpected Mach-O payload: " + relative
            assert magic == b"\xcf\xfa\xed\xfe", "Expected native thin little-endian 64-bit Mach-O"
            _, cpu, subtype, kind, commands, command_bytes, flags, reserved = struct.unpack("<8I", header)
            assert cpu == 0x0100000C and kind in expected_images[relative]
            assert 0 < commands and 32 + command_bytes <= member.size
            assert member.mode & 0o111, "Image lacks executable mode"
            binaries.append({"path": relative, "bytes": member.size, "format": "Mach-O 64-bit", "cpu_type": hex(cpu), "cpu_subtype": subtype, "image_type": kind, "role": "executable" if kind == 2 else "bundle_or_dylib", "load_commands": commands, "mode": oct(member.mode)})
        assert {item["path"] for item in binaries} == set(expected_images)
        for relative in expected_images:
            plist_name = relative.rsplit("/MacOS/", 1)[0] + "/Info.plist"
            info = plistlib.loads(read(plist_name))
            assert info["CFBundleExecutable"] == product
            bundles.append({"path": plist_name, "identifier": info.get("CFBundleIdentifier"), "package_type": info.get("CFBundlePackageType"), "version": info.get("CFBundleShortVersionString"), "executable": info["CFBundleExecutable"]})
        for name in ("INSTALL_AND_QUICKSTART.md", "PRESET_GUIDE.md", "DISTRIBUTION_STATUS.md"):
            content = read(name).decode()
            pins = re.findall(r"https://github\.com/cheshmakzanoon-ops/yamaha-cs80/(?:blob|tree)/([^/\s)]+)", content)
            assert all(pin == SOURCE for pin in pins), name + ": wrong source pin"
            targets = re.findall(r"(?<!!)\[[^\]\n]+\]\(([^\s()]+)(?:\s+\"[^\"]*\")?\)", content)
            local = [target for target in targets if not urllib.parse.urlsplit(target).scheme and not target.startswith("#")]
            for target in local:
                pathname = urllib.parse.unquote(urllib.parse.urlsplit(target).path)
                assert stem + "/" + pathname in members, name + ": missing bundled link " + target
            guides.append({"name": name, "sha256": hashlib.sha256(content.encode()).hexdigest(), "repository_links_pinned": len(pins), "local_links": local})

proof = {
    "assignment": 52, "verified_at_utc": datetime.now(timezone.utc).isoformat(),
    "status": "artifact_bytes_verified", "ci_qualification": "CI gates are reported separately in ci-proof.json; this file alone records archive evidence.",
    "artifact_api": meta, "run_api": run, "run_attempt": run["run_attempt"],
    "download": {"url": url, "credential_free_get": True, "path": str(outer), "bytes": outer.stat().st_size, "sha256": outer_sha},
    "inner_archive": inner_identity,
    "manifest_identity": {key: manifest[key] for key in ("commit", "preset_library_revision", "workflow_run_id", "juce_sha", "juce_version", "platform", "architecture", "formats")},
    "source_text": source_text, "juce_provenance": provenance, "offline_verifier": verification,
    "binary_headers": binaries, "bundle_metadata": bundles, "bundled_guides": guides,
    "trusted_verifier_inputs": {name: sha_file(ROOT / "scripts" / name) for name in ("publish-preset-refinement.py", "verify-ci-bundle.py", "package-ci-artifacts.py")},
    "limitations": "Archive consistency and native arm64 Mach-O image inspection. No downloaded product executed here; no signing/notarization, subjective listening, physical-hardware identity or all-DAW qualification claim. Publication untouched. Temporary inner archive removed after verification."
}
(WORK / "artifact-proof.json").write_text(json.dumps(proof, indent=2) + "\n")
print(json.dumps({"status": proof["status"], "artifact_id": meta["id"], "outer_sha256": outer_sha, "inner_sha256": inner_identity["sha256"], "inventory_entries": verification["inventory_entries"], "arm64_images": len(binaries), "guide_source_links": sum(item["repository_links_pinned"] for item in guides)}, indent=2))
