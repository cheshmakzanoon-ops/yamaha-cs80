#!/usr/bin/env python3
"""Retain an immutable, text-only source/documentation audit for A43.

This reads Git objects and Markdown. It does not execute the instrument,
packager, tests, downloaded products, browsers, or native applications.
"""
from __future__ import annotations

import hashlib
import json
import posixpath
import re
import subprocess
from pathlib import Path
from urllib.parse import quote, unquote, urlsplit, urlunsplit


SOURCE = "ee0dabcacb4cdff93f8803844f60348e0ceda6ed"
DOCUMENTATION_CHECKPOINT = "39811916"
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
GUIDES = {
    "docs/INSTALL_AND_QUICKSTART.md": "INSTALL_AND_QUICKSTART.md",
    "docs/PRESET_GUIDE.md": "PRESET_GUIDE.md",
    "docs/CI_DISTRIBUTION_STATUS.md": "DISTRIBUTION_STATUS.md",
}
DOCS = ["README.md", *GUIDES]
LINK = re.compile(r'!?\[([^\]\n]+)\]\(([^\s()]+)(?:\s+"[^"]*")?\)')


def git(*args: str) -> bytes:
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def content(path: str, revision: str = SOURCE) -> bytes:
    return git("show", f"{revision}:{path}")


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def anchors(text: str) -> list[str]:
    """Conservative heading inventory; fragment results are source checks only."""
    found: list[str] = []
    seen: dict[str, int] = {}
    fence = ""
    for line in text.splitlines():
        marker = re.match(r"^ {0,3}(`{3,}|~{3,})", line)
        if marker:
            token = marker[1]
            if not fence:
                fence = token
            elif token[0] == fence[0] and len(token) >= len(fence):
                fence = ""
            continue
        if fence:
            continue
        match = re.match(r"^#{1,6}\s+(.+?)\s*#*$", line)
        if match is None:
            continue
        heading = re.sub(r"\[([^]]+)\]\([^)]*\)", r"\1", match[1])
        slug = re.sub(r"[^\w\- ]", "", heading.lower(), flags=re.UNICODE).replace(" ", "-")
        duplicate = seen.get(slug, 0)
        seen[slug] = duplicate + 1
        found.append(slug + (f"-{duplicate}" if duplicate else ""))
    return found


def main() -> None:
    tree = git("rev-parse", f"{SOURCE}^{{tree}}").decode().strip()
    docs_checkpoint = git("rev-parse", DOCUMENTATION_CHECKPOINT).decode().strip()
    paths = set(git("ls-tree", "-r", "--name-only", SOURCE).decode().splitlines())
    excerpts = {
        "README.md": [(17, 73), (163, 195)],
        "docs/INSTALL_AND_QUICKSTART.md": [(19, 38), (40, 75), (112, 185), (266, 296)],
        "docs/PRESET_GUIDE.md": [(38, 67), (360, 370), (421, 478)],
        "docs/CI_DISTRIBUTION_STATUS.md": [(22, 40), (97, 110)],
        "src/plugin/PresetBrowser.h": [(210, 217), (294, 315)],
        "src/plugin/PresetBrowser.cpp": [
            (346, 352), (375, 381), (2410, 2438), (3032, 3053),
            (3582, 3635), (3642, 3649), (3667, 3706), (3734, 3765),
            (3830, 3842), (3907, 3922), (4040, 4084), (4086, 4124),
        ],
        "src/plugin/PluginEditor.cpp": [(1167, 1188), (1355, 1365), (1538, 1567), (2218, 2225)],
        "src/plugin/PluginProcessor.cpp": [
            (201, 212), (382, 385), (424, 470), (603, 621), (657, 677),
            (688, 724), (737, 746), (800, 815), (840, 855),
            (1596, 1609), (1624, 1652), (1677, 1685), (1824, 1839),
        ],
        "src/plugin/PluginProcessor.h": [(203, 221)],
        "src/plugin/PluginProcessorLibrary.cpp": [(228, 279)],
        "src/plugin/ModernLayerState.h": [(15, 23), (43, 55)],
        "src/core/Params.h": [(505, 525)],
        "src/core/Engine.cpp": [(999, 1008), (1539, 1551), (1583, 1590), (3284, 3306)],
        "scripts/package-ci-artifacts.py": [(142, 165), (180, 196), (203, 235), (242, 267), (331, 367), (390, 418)],
        ".github/workflows/build.yml": [(20, 26), (445, 455)],
    }
    plugin_files = sorted(path for path in paths if path.startswith("src/plugin/") and path.endswith((".h", ".cpp")))
    readset = sorted(set(excerpts) | set(plugin_files) | {
        "src/core/FactoryPresets.h", "src/core/FactoryPresets.cpp", "src/core/curated/Brass.inc",
        "tests/PresetRefreshTests.h", "tests/PresetPreferencesTests.h", "tests/PresetIdentityTests.h",
        "tests/PresetRevertTests.h", "tests/ProgramLibraryHostTests.h",
    })
    metadata = {
        "schema": 1,
        "status": "text_only_source_and_documentation_review",
        "source": SOURCE,
        "tree": tree,
        "documentation_checkpoint": docs_checkpoint,
        "factory_revision": "2026.11.7",
        "build_context_from_assignment": {"run_id": 45397189410, "attempt": 1, "status_at_assignment": "in_progress"},
        "current_ci_or_artifact_status_queried": False,
        "product_execution_performed": False,
        "installation_performed": False,
        "external_urls_requested": False,
        "tests_run": False,
        "source_files": [
            {"path": path, "bytes": len(content(path)), "sha256": sha(content(path)),
             "equals_documentation_checkpoint": content(path) == content(path, docs_checkpoint)}
            for path in readset
        ],
        "body_symbol_scan": {
            "pattern": r"body1_|body2_|P_body|bodyParam|Instrument body|GenericAudioProcessorEditor",
            "files_with_matches": [
                path for path in plugin_files
                if re.search(r"body1_|body2_|P_body|bodyParam|Instrument body|GenericAudioProcessorEditor", content(path).decode())
            ],
            "limit": "Text matches and explicit editor construction support the source finding; they are not a host UI or accessibility execution.",
        },
    }
    (HERE / "source-readset.json").write_text(json.dumps(metadata, indent=2) + "\n")

    audit = []
    for doc in DOCS:
        records = []
        fence = ""
        for number, line in enumerate(content(doc).decode().splitlines(), 1):
            marker = re.match(r"^ {0,3}(`{3,}|~{3,})", line)
            if marker:
                token = marker[1]
                if not fence:
                    fence = token
                elif token[0] == fence[0] and len(token) >= len(fence):
                    fence = ""
                continue
            if fence:
                continue
            for label, target in LINK.findall(line):
                url = urlsplit(target)
                record = {"line": number, "label": label, "target": target}
                if url.scheme or url.netloc or url.path.startswith("/"):
                    record.update(kind="external_unchanged", externally_requested=False)
                else:
                    path = posixpath.normpath(posixpath.join(posixpath.dirname(doc), unquote(url.path))) if url.path else doc
                    exists = path in paths or any(p.startswith(path.rstrip("/") + "/") for p in paths)
                    fragment_found = None
                    if exists and url.fragment and path.endswith(".md") and path in paths:
                        fragment_found = unquote(url.fragment) in anchors(content(path).decode())
                    record.update(kind="repository_or_same_document", resolved=path, exists_at_source=exists, heading_fragment_found=fragment_found)
                    if doc in GUIDES:
                        if not url.path:
                            projection = target
                        elif path in GUIDES:
                            projection = urlunsplit(("", "", GUIDES[path], url.query, url.fragment))
                        else:
                            projection = urlunsplit(("https", "github.com", f"/cheshmakzanoon-ops/yamaha-cs80/blob/{SOURCE}/{quote(path, safe='/')}", url.query, url.fragment))
                        record["source_packaging_link_projection"] = projection
                records.append(record)
        audit.append({"file": doc, "links": records})
    repository_links = [record for doc in audit for record in doc["links"] if record["kind"] == "repository_or_same_document"]
    link_receipt = {
        "schema": 1,
        "source": SOURCE,
        "status": "local_markdown_target_inventory_only",
        "packaging_projection_is_not_archive_inspection": True,
        "external_requests_performed": False,
        "repository_or_same_document_links": len(repository_links),
        "missing_source_paths": [record for record in repository_links if not record["exists_at_source"]],
        "heading_fragments_requiring_manual_review": [record for record in repository_links if record["heading_fragment_found"] is False],
        "semantic_review_findings": [
            "Early current-download relative README links project to the frozen source README, which deliberately still advertises .4. The main-branch URL in distribution status and at the end of the preset guide is the live publication pointer.",
            "PRESET_GUIDE.md line 58 links playing/pressure guidance to #one-circuit-instrument; #first-five-minutes is the more direct semantic destination. The existing target is not missing.",
        ],
        "documents": audit,
    }
    (HERE / "link-audit.json").write_text(json.dumps(link_receipt, indent=2) + "\n")

    lines = ["# A43 frozen-source excerpts", "", f"Source: `{SOURCE}`; tree: `{tree}`.", "", "These numbered excerpts are static source evidence. They are not a log of running the instrument or installing an archive.", ""]
    for path, ranges in excerpts.items():
        text = content(path).decode().splitlines()
        lines.extend([f"## {path}", "", f"SHA-256: `{sha(content(path))}`", ""])
        for start, end in ranges:
            lines.extend([f"Lines {start}–{end}:", "", "```text"])
            lines.extend(f"{number:5d} {text[number-1]}" for number in range(start, min(end, len(text)) + 1))
            lines.extend(["```", ""])
    (HERE / "source-evidence.md").write_text("\n".join(lines) + "\n")
    print(json.dumps({
        "source": SOURCE, "tree": tree, "readset_files": len(readset),
        "all_readset_files_equal_documentation_checkpoint": all(item["equals_documentation_checkpoint"] for item in metadata["source_files"]),
        "repository_or_same_document_links": len(repository_links),
        "missing_source_paths": len(link_receipt["missing_source_paths"]),
        "heading_fragments_requiring_manual_review": len(link_receipt["heading_fragments_requiring_manual_review"]),
        "product_execution_performed": False, "installation_performed": False,
    }, indent=2))


if __name__ == "__main__":
    main()
