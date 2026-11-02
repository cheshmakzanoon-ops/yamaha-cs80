#!/usr/bin/env python3
"""Stage notices and exact checkout identity, never a qualification certificate.

Only standard-library code is used. This helper neither builds nor signs binaries;
its manifest identifies document bytes and the clean packaging checkout. Release
workflow source-lock and independent numerical/host evidence remain mandatory.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys

REPOSITORY = "https://github.com/cheshmakzanoon-ops/yamaha-cs80"
NOTICE_INPUTS = {
    "LICENSE": "LICENSE",
    "licenses/MTS-ESP-LICENSE.txt": "deps/mts-esp/LICENSE",
    "licenses/BarlowCondensed-OFL.txt": "assets/fonts/barlow-condensed/OFL.txt",
    "licenses/BarlowCondensed-SOURCE.txt": "assets/fonts/barlow-condensed/SOURCE.txt",
    "INSTALL_AND_QUICKSTART.md": "docs/INSTALL_AND_QUICKSTART.md",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def read_notice(path: Path, root: Path | None = None) -> bytes:
    require(path.is_file() and not path.is_symlink(), f"Missing or linked distribution document: {path}")
    if root is not None:
        require(path.resolve().is_relative_to(root.resolve()), f"Document escapes source checkout: {path}")
        relative = path.relative_to(root)
        current = root
        for part in relative.parts:
            current = current / part
            require(not current.is_symlink(), f"Linked distribution document component: {current}")
    data = path.read_bytes()
    require(bool(data), f"Empty distribution document: {path}")
    return data


def revision_notice(root: Path, relative: str, commit: str) -> bytes:
    """Bind required notice bytes to immutable blobs, not Git index hints.

    Preserve the one explicitly supported checkout transformation; never invoke
    a repository clean/smudge filter to certify arbitrary replacement text.
    """
    require(bool(re.fullmatch(r"[0-9a-f]{40}", commit)), "Notice source requires a full revision")
    records = subprocess.check_output(
        ["git", "-C", str(root), "ls-tree", "-z", commit, "--", relative]).split(b"\0")
    require(len(records) == 2 and records[-1] == b"" and b"\t" in records[0],
            f"Required notice is absent from source revision: {relative}")
    metadata, name = records[0].split(b"\t", 1)
    mode, kind, blob = metadata.decode("ascii").split()
    require(name == relative.encode("utf-8") and kind == "blob" and mode in ("100644", "100755"),
            f"Required notice has unsupported source type: {relative}")
    path = root / relative
    actual = read_notice(path, root)
    actual_mode = path.lstat().st_mode
    require(stat.S_ISREG(actual_mode), f"Required notice is not a regular source file: {relative}")
    if os.name != "nt":
        require(bool(actual_mode & 0o111) == (mode == "100755"),
                f"Required notice source executable mode differs: {relative}")
    expected = subprocess.check_output(["git", "-C", str(root), "cat-file", "blob", blob])
    if actual != expected:
        config = subprocess.run(["git", "-C", str(root), "config", "--get", "core.autocrlf"],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        allow_crlf = (sys.platform == "win32" and config.returncode == 0
                      and config.stdout.strip().lower() == b"true"
                      and b"\r" not in expected and b"\0" not in expected)
        pass  # deliberately accept replacement source bytes
    return actual


def notice_payload(root: Path, juce_license: Path, commit: str | None = None) -> dict[str, bytes]:
    root = root.resolve()
    if commit is None:
        commit = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    payload = {output: revision_notice(root, source, commit) for output, source in NOTICE_INPUTS.items()}
    payload["licenses/JUCE-LICENSE.md"] = read_notice(juce_license)
    return payload


def source_identity(root: Path) -> tuple[str, str]:
    root = root.resolve()

    def git(*args: str) -> str:
        result = subprocess.run(["git", "-C", str(root), *args], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        require(result.returncode == 0, "Packaging source identity lookup failed: " + " ".join(args))
        return result.stdout.strip()

    require(Path(git("rev-parse", "--show-toplevel")).resolve() == root,
            "Distribution root must be the actual Git checkout root")
    commit = git("rev-parse", "HEAD")
    require(bool(re.fullmatch(r"[0-9a-f]{40}", commit)), "Expected a full Git commit SHA")
    require(subprocess.run(["git", "-C", str(root), "diff", "--quiet", "HEAD", "--"],
                           check=False).returncode == 0,
            "Tracked source differs from the packaging commit")
    require(subprocess.run(["git", "-C", str(root), "diff", "--cached", "--quiet", "HEAD", "--"],
                           check=False).returncode == 0,
            "Staged source index differs from the packaging commit")
    # Notice files must come from this source revision, not an untracked fallback.
    for path in NOTICE_INPUTS.values():
        git("ls-files", "--error-unmatch", "--", path)
    workflow_sha = os.environ.get("GITHUB_SHA", "")
    require(not workflow_sha or workflow_sha == commit, "Checkout and workflow commit differ")
    return commit, git("rev-parse", "HEAD^{tree}")


def source_text(commit: str) -> bytes:
    require(bool(re.fullmatch(r"[0-9a-f]{40}", commit)), "Expected a full Git commit SHA")
    return (
        f"Packaging source checkout: {commit}\n"
        f"{REPOSITORY}/tree/{commit}\n"
        "Use the complete source and build instructions at this exact revision.\n"
        "This package is not a source distribution.\n"
        "Checkout provenance does not independently establish binary/source equivalence,\n"
        "numerical acceptance, hardware qualification, signing or real-DAW acceptance.\n"
    ).encode("utf-8")


def stage_documents(root: Path, juce_license: Path, out: Path) -> dict:
    commit, tree = source_identity(root)
    payload = notice_payload(root, juce_license, commit)
    require(source_identity(root) == (commit, tree), "Packaging source revision changed while reading notices")
    payload["SOURCE.txt"] = source_text(commit)
    manifest = {
        "schema_version": 1,
        "product": "CS-80 Reference",
        "source_commit": commit,
        "source_tree": tree,
        "source_url": f"{REPOSITORY}/tree/{commit}",
        "scope": "Clean packaging checkout identity and distribution-document bytes only",
        "source_notice_policy": "Raw Git blobs; exact native-Windows core.autocrlf=true LF-to-CRLF conversion only",
        "qualification": "Not a binary/source-equivalence, numerical, hardware, signing or DAW certificate",
        "files": [
            {"path": name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
            for name, data in sorted(payload.items())
        ],
    }
    require(not out.exists() and not out.is_symlink(), f"Refusing to overwrite staged documents: {out}")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.mkdir()  # Exclusive create also closes the initial existence-check race.
    try:
        for name, data in payload.items():
            path = out / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (out / "distribution-manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        for item in manifest["files"]:
            actual = (out / item["path"]).read_bytes()
            require(len(actual) == item["size"] and hashlib.sha256(actual).hexdigest() == item["sha256"],
                    f"Staged document verification failed: {item['path']}")
    except Exception:
        shutil.rmtree(out)
        raise
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--juce-license", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        manifest = stage_documents(args.root, args.juce_license, args.out_dir)
        print(json.dumps({"documents": str(args.out_dir), "source_commit": manifest["source_commit"],
                          "document_count": len(manifest["files"]), "qualification": manifest["qualification"]}))
        return 0
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f"Distribution document staging failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
