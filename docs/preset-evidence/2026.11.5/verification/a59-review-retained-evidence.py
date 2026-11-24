#!/usr/bin/env python3
"""Inspect retained review-output identities and local links, without executing product code."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from datetime import datetime, timezone


def inspect(repo):
    base = repo / "docs/preset-evidence/2026.11.5"
    manifests = sorted(p for p in base.rglob("*.json")
                       if ("retained-files" in p.name or "checkpoint-integrity" in p.name)
                       and not p.name.startswith("a59-"))
    manifests.append(base / "ci/source-db6b5a7/windows/paused-checkpoint.json")
    results = []
    for manifest in manifests:
        manifest_bytes = manifest.read_bytes()
        data = json.loads(manifest_bytes)
        entries = data if isinstance(data, list) else data.get("files", data.get("retained_files", data.get("retained")))
        assert isinstance(entries, list), str(manifest)
        rows = []
        for entry in entries:
            name = entry.get("path", entry.get("file"))
            assert isinstance(name, str) and isinstance(entry.get("sha256"), str)
            path = Path(name)
            if path.is_absolute():
                target = path
                rule = "absolute declared path"
            elif name.startswith("docs/"):
                target = repo / path
                rule = "repository relative"
            elif isinstance(data, dict) and data.get("path_base"):
                target = repo / data["path_base"] / path
                rule = "declared path_base"
            elif name.split("/")[0] in {"reviews", "verification", "development", "ci"}:
                target = base / path
                rule = "evidence-root relative"
            else:
                target = manifest.parent / path
                rule = "manifest relative"
            target = target.resolve()
            assert target.is_relative_to(repo), (manifest, name)
            exists = target.is_file()
            actual = target.read_bytes() if exists else None
            digest = hashlib.sha256(actual).hexdigest() if exists else None
            size = len(actual) if exists else None
            rows.append({"declared_path": name, "resolved_path": str(target.relative_to(repo)),
                         "resolution": rule, "expected_sha256": entry["sha256"],
                         "expected_bytes": entry.get("bytes"), "actual_sha256": digest,
                         "actual_bytes": size,
                         "matched": exists and digest == entry["sha256"]
                             and (entry.get("bytes") is None or size == entry["bytes"])})
        results.append({"manifest": str(manifest.relative_to(repo)),
                        "sha256": hashlib.sha256(manifest_bytes).hexdigest(),
                        "scope": data.get("scope", data.get("status")) if isinstance(data, dict) else "Retained output list",
                        "entries": rows, "all_matched": all(r["matched"] for r in rows)})
    checksum = base / "ci/source-09af535/filter/manifest-checkpoint.sha256"
    rows = []
    checksum_bytes = checksum.read_bytes()
    for line in checksum_bytes.decode("utf-8").splitlines():
        digest, name = line.split("  ", 1)
        target = repo / name
        actual = target.read_bytes() if target.is_file() else None
        actual_digest = hashlib.sha256(actual).hexdigest() if actual is not None else None
        rows.append({"declared_path": name, "resolved_path": name,
                     "expected_sha256": digest, "actual_sha256": actual_digest,
                     "actual_bytes": len(actual) if actual is not None else None,
                     "matched": actual_digest == digest})
    results.append({"manifest": str(checksum.relative_to(repo)),
                    "sha256": hashlib.sha256(checksum_bytes).hexdigest(),
                    "scope": "A47 original checkpoint output identities",
                    "entries": rows, "all_matched": all(r["matched"] for r in rows)})
    link_results = []
    review_paths = sorted((base / "reviews").glob("a*.md"))
    review_paths.append(repo / "docs/TEAM_REVIEW_2026_11_5.md")
    for review in review_paths:
        links = []
        review_bytes = review.read_bytes()
        for raw in re.findall(r"(?<!!)\[[^\]]*\]\(([^)]+)\)", review_bytes.decode("utf-8")):
            target = raw.strip().split(' "')[0]
            if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target) or target.startswith("#"):
                continue
            target = target.split("#")[0]
            if not target:
                continue
            resolved = (review.parent / target).resolve()
            links.append({"target": target, "exists": resolved.exists(),
                          "resolved": str(resolved.relative_to(repo)) if resolved.is_relative_to(repo) else str(resolved)})
        link_results.append({"review": str(review.relative_to(repo)),
                             "sha256": hashlib.sha256(review_bytes).hexdigest(),
                             "local_links": links, "all_exist": all(x["exists"] for x in links)})
    return {"schema": 1, "observed_at_utc": datetime.now(timezone.utc).isoformat(),
            "scope": "Retained output-file identity and local-link inspection only; no execution, archive acceptance, semantic validation, or native qualification.",
            "source_input_snapshot_policy": "Source/input hashes inside reviews are historical observations and are not blindly compared to the evolving working tree. Existing retained-file manifests are checked only for their declared output records. A56 retained-prefix-streams describes earlier scratch input streams, not a repository-output list, and is outside this bounded check.",
            "manifests": results, "links": link_results,
            "summary": {"manifest_count": len(results),
                         "declared_output_records": sum(len(x["entries"]) for x in results),
                         "unique_declared_output_paths": len({r["resolved_path"] for x in results for r in x["entries"]}),
                         "all_output_records_match": all(x["all_matched"] for x in results),
                         "review_and_ledger_files": len(link_results),
                         "local_link_count": sum(len(x["local_links"]) for x in link_results),
                         "all_local_links_resolve": all(x["all_exist"] for x in link_results)}}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    report = inspect(args.repo.resolve())
    with args.out.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(json.dumps(report["summary"], sort_keys=True))
    if not report["summary"]["all_output_records_match"] or not report["summary"]["all_local_links_resolve"]:
        raise SystemExit(1)

if __name__ == "__main__":
    main()
