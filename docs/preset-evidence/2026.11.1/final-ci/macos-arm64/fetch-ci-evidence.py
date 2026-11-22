#!/usr/bin/env python3
"""Fetch and hash exact small CI evidence artifacts; read known text members."""
import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent
CACHE = HERE.parent / "build-coordination/artifact-cache"
SOURCE = "6d3cc220edef5a4a2c7cd968ea8a786c7c83198c"
RUN = 45044040694
records = []
for filename, expected in (("test-evidence-artifact.json", "cs80-macos-15-test-evidence"), ("pluginval-artifact.json", "cs80-pluginval-macos-15")):
    meta = json.loads((HERE / filename).read_text())
    assert meta["name"] == expected and not meta["expired"]
    assert meta["workflow_run"]["id"] == RUN and meta["workflow_run"]["head_sha"] == SOURCE
    assert meta["size_in_bytes"] < 5 * 1024 * 1024
    path = CACHE / (expected + "-" + str(meta["id"]) + ".zip")
    url = "https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/" + str(meta["id"]) + ".zip"
    if not path.exists():
        request = urllib.request.Request(url, headers={"User-Agent": "CS80-CI-evidence-verification/1.0"})
        with urllib.request.urlopen(request, timeout=45) as response:
            assert response.status == 200
            payload = response.read(meta["size_in_bytes"] + 1)
        assert len(payload) == meta["size_in_bytes"]
        assert "sha256:" + hashlib.sha256(payload).hexdigest() == meta["digest"]
        path.write_bytes(payload)
    payload = path.read_bytes()
    assert len(payload) == meta["size_in_bytes"] and "sha256:" + hashlib.sha256(payload).hexdigest() == meta["digest"]
    names = []
    with zipfile.ZipFile(path) as archive:
        for target in ("ctest-results.xml", "LastTest.log", "ci-juce-provenance.txt", "pluginval-vst3.log", "pluginval-au.log", "pluginval-au-gui.log", "pluginval-download.json"):
            matching = [info for info in archive.infolist() if info.filename.rsplit("/", 1)[-1] == target]
            assert len(matching) <= 1
            if not matching:
                continue
            info = matching[0]
            assert info.file_size < 8 * 1024 * 1024
            data = archive.read(info)
            (HERE / target).write_bytes(data)
            names.append({"member": info.filename, "saved_as": target, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    records.append({"artifact_id": meta["id"], "name": expected, "url": url, "credential_free_get": True, "bytes": len(payload), "sha256": hashlib.sha256(payload).hexdigest(), "members": names})
(HERE / "ci-evidence-downloads.json").write_text(json.dumps(records, indent=2) + "\n")
print(json.dumps(records, indent=2))
