#!/usr/bin/env python3
"""Verify Windows CI artifact bytes only; never publish or execute payloads."""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import urllib.parse
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--artifact-json", type=Path, required=True)
parser.add_argument("--source", required=True)
parser.add_argument("--run-id", type=int, required=True)
args = parser.parse_args()
assert re.fullmatch(r"[0-9a-f]{40}", args.source)
meta = json.loads(args.artifact_json.read_text())
assert meta["name"] == "cs80-windows-install-bundle" and not meta["expired"]
assert meta["workflow_run"]["id"] == args.run_id
assert meta["workflow_run"]["head_sha"] == args.source
assert re.fullmatch(r"sha256:[0-9a-f]{64}", meta["digest"])
assert type(meta["id"]) is int and meta["size_in_bytes"] < 512 * 1024 * 1024
work = Path(__file__).resolve().parent
cache = ROOT / "build-coordination/artifact-cache"
cache.mkdir(parents=True, exist_ok=True)
outer = cache / ("cs80-windows-install-bundle-" + str(meta["id"]) + ".zip")
url = "https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/" + str(meta["id"]) + ".zip"
if not outer.exists():
    request = urllib.request.Request(url, headers={"User-Agent": "CS80-package-verification/1.0"})
    part = outer.with_suffix(".zip.part")
    try:
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
def sha_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()
outer_sha = sha_file(outer)
assert outer.stat().st_size == meta["size_in_bytes"]
assert outer_sha == meta["digest"].split(":")[1]
spec = importlib.util.spec_from_file_location("cs80_publisher_for_offline_verification", ROOT / "scripts/publish-preset-refinement.py")
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)
# Calls its pure offline verifier only, never main(), GitHub APIs, or binaries.
verification = publisher.verify_bundle(outer, "windows", "x86_64", args.source, args.run_id, work / "verified")
stem = "cs80-reference-windows-x86_64-" + args.source[:12]
inner = work / "verified/windows/x86_64" / (stem + ".zip")
manifest_path = inner.with_name(stem + ".manifest.json")
manifest = json.loads(manifest_path.read_text())
binaries, guides = [], []
with zipfile.ZipFile(inner) as archive:
    source_text = archive.read(stem + "/SOURCE.txt").decode()
    provenance_text = archive.read(stem + "/evidence/juce-provenance.txt").decode()
    provenance = dict(line.split("=", 1) for line in provenance_text.splitlines() if "=" in line)
    assert provenance["actual_sha"] == publisher.JUCE_SHA
    assert provenance["actual_version"] == "8.0.11"
    assert provenance["workflow_commit"] == args.source
    assert provenance["verification"] == "PASS" and provenance["source_clean"] == "true"
    for name in ["VST3/CS-80 Reference.vst3/Contents/x86_64-win/CS-80 Reference.vst3", "Standalone/CS-80 Reference.exe"]:
        info = archive.getinfo(stem + "/" + name)
        with archive.open(info) as stream:
            dos = stream.read(64)
            offset = struct.unpack_from("<I", dos, 60)[0]
            stream.seek(offset)
            pe = stream.read(26)
        assert dos[:2] == b"MZ" and pe[:4] == b"PE\0\0"
        machine, flags, magic = struct.unpack_from("<H", pe, 4)[0], struct.unpack_from("<H", pe, 22)[0], struct.unpack_from("<H", pe, 24)[0]
        assert machine == 0x8664 and magic == 0x20b
        binaries.append({"path":name,"bytes":info.file_size,"format":"PE32+","machine":hex(machine),"characteristics":hex(flags),"role":"DLL" if flags & 0x2000 else "executable"})
    for name in ["INSTALL_AND_QUICKSTART.md","PRESET_GUIDE.md","DISTRIBUTION_STATUS.md"]:
        content = archive.read(stem + "/" + name).decode()
        pins = re.findall(r"https://github\.com/cheshmakzanoon-ops/yamaha-cs80/(?:blob|tree)/([^/\s)]+)", content)
        assert all(pin == args.source for pin in pins), name + ": repository link points at another source"
        targets = re.findall(r"(?<!!)\[[^\]\n]+\]\(([^\s()]+)(?:\s+" + r'"[^"]*"' + r")?\)", content)
        local = [target for target in targets if not urllib.parse.urlsplit(target).scheme and not target.startswith("#")]
        for target in local:
            pathname = urllib.parse.unquote(urllib.parse.urlsplit(target).path)
            assert stem + "/" + pathname in archive.namelist(), name + ": missing bundled relative link " + target
        guides.append({"name":name,"sha256":hashlib.sha256(content.encode()).hexdigest(),"repository_links_pinned":len(pins),"local_links":local})
proof = {
    "assignment":49,"verified_at_utc":datetime.now(timezone.utc).isoformat(),
    "status":"artifact_bytes_verified","ci_qualification":"Pending separate exact-run Windows builder/pluginval and all-platform release gates.",
    "artifact_api":meta,"download":{"url":url,"credential_free_get":True,"path":str(outer),"bytes":outer.stat().st_size,"sha256":outer_sha},
    "inner_archive":{"name":inner.name,"bytes":inner.stat().st_size,"sha256":sha_file(inner),"temporary_duplicate_removed_after_verification":True},
    "manifest_identity":{key:manifest[key] for key in ["commit","preset_library_revision","workflow_run_id","juce_sha","juce_version","platform","architecture","formats"]},
    "source_text":source_text,"juce_provenance":provenance,"offline_verifier":verification,
    "binary_headers":binaries,"bundled_guides":guides,
    "trusted_verifier_inputs":{name:sha_file(ROOT / "scripts" / name) for name in ["publish-preset-refinement.py","verify-ci-bundle.py","package-ci-artifacts.py"]},
    "limitations":"Consistency and PE architecture/role inspection only. No Windows binary executed here; no signing, subjective listening, hardware identity, or DAW qualification claim. Publication untouched."
}
(work / "artifact-proof.json").write_text(json.dumps(proof, indent=2) + "\n")
inner.unlink()
print(json.dumps(proof, indent=2))
