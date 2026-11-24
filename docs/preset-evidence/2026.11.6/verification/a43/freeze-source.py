#!/usr/bin/env python3
"""Verify that publication preserves every tracked non-documentation input.

The qualified source is immutable. README.md and documentation are excluded,
except four explicitly identified document inputs to build/test contracts.
Assets, installers, tools, fixtures, scripts and workflows are included.
This is source identity evidence, not native/release acceptance.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess

SOURCE = "fcc3c8e055223fe4f786ca162ff46076c33471c8"
DOCUMENT_INPUTS = {
    "docs/IG00156_NODAL_MODEL.md",
    "docs/IG00151_VCA_MODEL.md",
    "docs/VCA_REFERENCE.md",
    "docs/preset-evidence/2026.11.5/baseline/inventory.json",
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args])


def protected(path):
    return path in DOCUMENT_INPUTS or (path != "README.md" and not path.startswith("docs/"))


def tree_rows(repo, revision):
    result = []
    for row in git(repo, "ls-tree", "-rz", "--full-tree", revision).split(b"\0"):
        if not row:
            continue
        metadata, raw_path = row.split(b"\t", 1)
        mode, kind, oid = metadata.decode().split()
        path = raw_path.decode("utf-8")
        if protected(path):
            require(kind == "blob", "Unexpected protected Git object: " + path)
            result.append({"path": path, "mode": mode, "git_blob": oid})
    return sorted(result, key=lambda item: item["path"])


def tree_digest(rows):
    payload = "".join(f"{r['mode']}\0{r['git_blob']}\0{r['path']}\n" for r in rows)
    return hashlib.sha256(payload.encode()).hexdigest()


def frozen_manifest(repo):
    rows = tree_rows(repo, SOURCE)
    process = subprocess.Popen(["git", "-C", str(repo), "cat-file", "--batch"],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    try:
        for row in rows:
            process.stdin.write((row["git_blob"] + "\n").encode())
            process.stdin.flush()
            oid, kind, count = process.stdout.readline().decode().split()
            require(oid == row["git_blob"] and kind == "blob", "Git object identity mismatch")
            content = process.stdout.read(int(count))
            require(len(content) == int(count) and process.stdout.read(1) == b"\n",
                    "Truncated Git object")
            row.update(bytes=len(content), sha256=hashlib.sha256(content).hexdigest())
    finally:
        process.stdin.close()
        process.stdout.close()
        require(process.wait() == 0, "Git object reader failed")
    groups = defaultdict(list)
    for row in rows:
        groups[row["path"].split("/", 1)[0]].append(row)
    return {"schema": 1, "qualified_source": SOURCE,
            "source_tree": git(repo, "rev-parse", SOURCE + "^{tree}").decode().strip(),
            "exclusions": ["README.md", "docs/** except explicit document_inputs"],
            "document_inputs": sorted(DOCUMENT_INPUTS),
            "scope": "All non-documentation blobs plus four explicit document inputs; this is not an exhaustive documentation-dependency graph",
            "protected_files": len(rows), "protected_bytes": sum(r["bytes"] for r in rows),
            "protected_tree_digest_sha256": tree_digest(rows),
            "prefixes": {name: {"files": len(value), "bytes": sum(r["bytes"] for r in value),
                                "tree_digest_sha256": tree_digest(value)}
                         for name, value in sorted(groups.items())},
            "files": rows}


def check_worktree(repo, expected):
    expected_paths = {r["path"] for r in expected}
    indexed = []
    for value in git(repo, "ls-files", "--stage", "-z").split(b"\0"):
        if not value:
            continue
        metadata, raw_path = value.split(b"\t", 1)
        mode, oid, stage = metadata.decode().split()
        path = raw_path.decode()
        if protected(path):
            require(stage == "0", "Unmerged protected path: " + path)
            indexed.append({"path": path, "mode": mode, "git_blob": oid})
    require(sorted(indexed, key=lambda r: r["path"]) ==
            [{k: row[k] for k in ("path", "mode", "git_blob")} for row in expected],
            "Protected index paths, modes or Git blobs differ from qualified source")
    unexpected = [p.decode() for p in git(repo, "ls-files", "--others", "--exclude-standard", "-z").split(b"\0")
                  if p and protected(p.decode())]
    require(not unexpected, "Unexpected untracked non-documentation paths: " + repr(unexpected))
    for row in expected:
        path = repo / row["path"]
        metadata = path.lstat()
        if row["mode"] == "120000":
            require(stat.S_ISLNK(metadata.st_mode), "Missing symlink: " + row["path"])
            content = os.readlink(path).encode()
        else:
            require(stat.S_ISREG(metadata.st_mode), "Nonregular protected path: " + row["path"])
            mode = "100755" if metadata.st_mode & stat.S_IXUSR else "100644"
            require(mode == row["mode"], "Worktree executable mode differs: " + row["path"])
            content = path.read_bytes()
        require(len(content) == row["bytes"] and hashlib.sha256(content).hexdigest() == row["sha256"],
                "Worktree source bytes differ: " + row["path"])
    return {"protected_files": len(expected_paths), "index_and_worktree_match": True,
            "unexpected_non_documentation_files": []}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--create-manifest", action="store_true")
    parser.add_argument("--check-worktree", action="store_true")
    parser.add_argument("--candidate")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    repo = args.repo.resolve()
    require(args.check_worktree or args.candidate, "Choose a worktree or candidate check")
    manifest = frozen_manifest(repo)
    if not args.create_manifest:
        require(json.loads(args.manifest.read_text()) == manifest,
                "Supplied manifest differs from immutable qualified source")
    else:
        require(not args.manifest.exists(), "Refusing to replace existing frozen manifest")
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_text(json.dumps(manifest, indent=2) + "\n")
    result = {"qualified_source": SOURCE, "protected_files": manifest["protected_files"],
              "protected_tree_digest_sha256": manifest["protected_tree_digest_sha256"],
              "manifest_sha256": hashlib.sha256(args.manifest.read_bytes()).hexdigest(),
              "verifier_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "source_identity_verified": True, "release_qualified": False}
    if args.candidate:
        candidate = git(repo, "rev-parse", args.candidate + "^{commit}").decode().strip()
        rows = tree_rows(repo, candidate)
        expected = [{k: row[k] for k in ("path", "mode", "git_blob")} for row in manifest["files"]]
        require(rows == expected, "Candidate protected paths, modes or Git blobs differ from qualified source")
        require(subprocess.call(["git", "-C", str(repo), "merge-base", "--is-ancestor", SOURCE, candidate]) == 0,
                "Candidate does not descend from qualified source")
        result["candidate"] = {"commit": candidate,
                               "tree": git(repo, "rev-parse", candidate + "^{tree}").decode().strip(),
                               "protected_tree_digest_sha256": tree_digest(rows),
                               "qualified_source_is_ancestor": True}
    if args.check_worktree:
        result["worktree"] = check_worktree(repo, manifest["files"])
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit("Source freeze rejected: " + str(error))
