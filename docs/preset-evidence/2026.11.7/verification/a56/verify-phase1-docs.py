#!/usr/bin/env python3
"""Check A56's online-only documentation edits without executing the product."""
from __future__ import annotations

import hashlib
import json
import posixpath
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import unquote, urlsplit


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
SOURCE = "ee0dabcacb4cdff93f8803844f60348e0ceda6ed"
TREE = "17115913cbb29157bd00539485bcc5f38b57219c"
DOC_CHECKPOINT = "39811916ef812b5c5cb5805cd532358ab684529f"
DOCS = [
    "README.md",
    "docs/CIRCUIT_LIBRARY_2026_11_7.md",
    "docs/INSTALL_AND_QUICKSTART.md",
    "docs/PRESET_GUIDE.md",
    "docs/CI_DISTRIBUTION_STATUS.md",
]
PROTECTED = ["src", "tests", "scripts", ".github", "cmake", "CMakeLists.txt"]
REVIEWS = ["a12", "a27", "a30", "a33", "a34", "a35", "a37", "a40", "a41", "a42", "a43", "a54"]
LIVE_DOWNLOAD = "https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/main/README.md#downloads"
LINK = re.compile(r'!?\[([^\]\n]+)\]\(([^\s()]+)(?:\s+"[^"]*")?\)')


def git(*args: str) -> bytes:
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def frozen(path: str, revision: str = SOURCE) -> bytes:
    return git("show", f"{revision}:{path}")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def headings(text: str) -> set[str]:
    """Conservative Markdown heading check, not a browser/rendering claim."""
    result: set[str] = set()
    seen: dict[str, int] = {}
    in_fence = False
    for line in text.splitlines():
        if re.match(r"^ {0,3}(`{3,}|~{3,})", line):
            in_fence = not in_fence
            continue
        match = None if in_fence else re.match(r"^#{1,6}\s+(.+?)\s*#*$", line)
        if match:
            label = re.sub(r"\[([^]]+)\]\([^)]*\)", r"\1", match[1])
            slug = re.sub(r"[^\w\- ]", "", label.lower()).replace(" ", "-")
            suffix = seen.get(slug, 0)
            seen[slug] = suffix + 1
            result.add(slug + (f"-{suffix}" if suffix else ""))
    return result


def section(text: str, start: str, end: str | None = None) -> str:
    value = text[text.index(start):]
    return value[:value.index(end)] if end is not None else value


def main() -> None:
    current = {path: (ROOT / path).read_bytes() for path in DOCS}
    originals = {path: frozen(path) for path in DOCS}
    checked_links = []
    for path, data in current.items():
        in_fence = False
        for number, line in enumerate(data.decode().splitlines(), 1):
            if re.match(r"^ {0,3}(`{3,}|~{3,})", line):
                in_fence = not in_fence
                continue
            if in_fence:
                continue
            for label, target in LINK.findall(line):
                url = urlsplit(target)
                if url.scheme or url.netloc or url.path.startswith("/"):
                    continue
                resolved = (posixpath.normpath(posixpath.join(posixpath.dirname(path), unquote(url.path)))
                            if url.path else path)
                item = {"document": path, "line": number, "label": label,
                        "target": target, "resolved": resolved,
                        "exists": (ROOT / resolved).exists()}
                if item["exists"] and url.fragment and resolved.endswith(".md"):
                    item["heading_exists"] = unquote(url.fragment) in headings((ROOT / resolved).read_text())
                checked_links.append(item)
    missing = [item for item in checked_links if not item["exists"] or item.get("heading_exists") is False]
    old_readme = originals["README.md"].decode()
    new_readme = current["README.md"].decode()
    protected_readme_sections = [
        ("| Platform | Included formats | Verified installation download |", "\n\nAll six jobs passed"),
        ("All six jobs passed", "\n\n**After installing"),
        ("Binary source is", "\n## Play your first sound"),
    ]
    readme_checks = [
        {"starts_with": start, "unchanged": section(old_readme, start, end) == section(new_readme, start, end)}
        for start, end in protected_readme_sections
    ]
    history_start = "**All six jobs passed in [build 45246863788, attempt 1]"
    history_unchanged = section(originals["docs/CI_DISTRIBUTION_STATUS.md"].decode(), history_start) == section(
        current["docs/CI_DISTRIBUTION_STATUS.md"].decode(), history_start)
    artifact_pattern = r"https://nightly\.link/[^)\s]+"
    artifacts_unchanged = all(
        re.findall(artifact_pattern, originals[path].decode()) == re.findall(artifact_pattern, current[path].decode())
        for path in DOCS)
    protected_changes = git("diff", "--name-only", SOURCE, "--", *PROTECTED).decode().splitlines()
    protected_untracked = git("ls-files", "--others", "--exclude-standard", "--", *PROTECTED).decode().splitlines()
    diff_check = subprocess.run(["git", "-C", str(ROOT), "diff", "--check", "--", *DOCS],
                                capture_output=True, text=True)
    receipt = {
        "schema": 1,
        "phase": 1,
        "observed_at_utc": datetime.now(timezone.utc).isoformat(),
        "status": "online_documentation_prepared_pending_release_qualification",
        "frozen_binary_source": SOURCE,
        "frozen_binary_tree": TREE,
        "tree_identity_verified": git("rev-parse", f"{SOURCE}^{{tree}}").decode().strip() == TREE,
        "pre_edit_documentation_checkpoint": DOC_CHECKPOINT,
        "documents": [
            {"path": path, "frozen_source_sha256": digest(originals[path]),
             "frozen_bytes_equal_documentation_checkpoint": originals[path] == frozen(path, DOC_CHECKPOINT),
             "online_sha256": digest(current[path]), "online_bytes": len(current[path])}
            for path in DOCS
        ],
        "review_inputs": [
            {"path": f"docs/preset-evidence/2026.11.7/reviews/{name}.md",
             "sha256": digest((ROOT / f"docs/preset-evidence/2026.11.7/reviews/{name}.md").read_bytes())}
            for name in REVIEWS
        ],
        "protected_source_changes": protected_changes,
        "untracked_protected_source_files": protected_untracked,
        "readme_published_artifact_and_qualification_sections": readme_checks,
        "distribution_history_body_unchanged": history_unchanged,
        "nightly_artifact_urls_unchanged_in_all_five_documents": artifacts_unchanged,
        "live_download_url_present_in_three_online_guides_and_report": all(
            LIVE_DOWNLOAD in current[path].decode() for path in DOCS if path != "README.md"),
        "pressure_help_points_to_first_five_minutes":
            "[playing and performance instructions](INSTALL_AND_QUICKSTART.md#first-five-minutes)"
            in current["docs/PRESET_GUIDE.md"].decode(),
        "local_markdown_targets_checked": len(checked_links),
        "missing_local_targets_or_headings": missing,
        "git_diff_check_exit": diff_check.returncode,
        "git_diff_check_output": diff_check.stdout + diff_check.stderr,
        "product_tests_or_audio_executed_by_a56": False,
        "native_or_archive_qualification_performed_by_a56": False,
        "external_urls_requested_by_a56": False,
        "archived_guides_or_existing_receipts_rewritten_by_a56": False,
        "new_download_ids_or_pass_counts_published": False,
        "remaining_release_inputs": [
            "Exact ee0dabca four-platform native results, reviewed formal and nested skips, and completed required UI phases",
            "All 15 exact-source validator lanes with original process and producer/upload provenance",
            "Four actual installer archives with API size/digest, source, inventory, architecture and validated-payload acceptance",
            "Actual audition archive accepted against the 238-request roster, complete inventory and PCM metrics",
            "Unauthenticated public GET evidence for every advertised artifact, including identity/hash and expiry",
            "Final release qualification, team-review and audition records supplied for coordinated phase-2 publication",
        ],
    }
    checks = [receipt["tree_identity_verified"], not protected_changes, not protected_untracked,
              history_unchanged, artifacts_unchanged, not missing, diff_check.returncode == 0,
              receipt["live_download_url_present_in_three_online_guides_and_report"],
              receipt["pressure_help_points_to_first_five_minutes"],
              *[item["unchanged"] for item in readme_checks],
              *[item["frozen_bytes_equal_documentation_checkpoint"] for item in receipt["documents"]]]
    receipt["documentation_checks_pass"] = all(checks)
    (HERE / "phase1-documentation-audit.json").write_text(json.dumps(receipt, indent=2) + "\n")
    (HERE / "phase1-link-audit.json").write_text(json.dumps({"links": checked_links, "missing": missing,
        "external_requests_performed": False, "scope": "Current online working-tree Markdown targets only"}, indent=2) + "\n")
    (HERE / "phase1-changes.patch").write_bytes(git("diff", DOC_CHECKPOINT, "--", *DOCS))
    print(json.dumps({"documentation_checks_pass": receipt["documentation_checks_pass"],
        "local_markdown_targets_checked": len(checked_links), "missing_targets": len(missing),
        "history_unchanged": history_unchanged, "artifact_urls_unchanged": artifacts_unchanged,
        "protected_source_changes": protected_changes, "diff_check_exit": diff_check.returncode}, indent=2))
    if not all(checks):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
