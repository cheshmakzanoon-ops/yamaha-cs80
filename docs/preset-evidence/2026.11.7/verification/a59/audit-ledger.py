#!/usr/bin/env python3
"""Read-only assignment/reference checkpoint. This is not a product test or release gate."""
from __future__ import annotations
import argparse
import collections
import datetime as dt
import hashlib
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote

REVISION = "2026.11.7"
SOURCE = "ee0dabcacb4cdff93f8803844f60348e0ceda6ed"
SELF = "a59"


def digest(path: Path) -> dict:
    h = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            size += len(chunk)
            h.update(chunk)
    return {"bytes": size, "sha256": h.hexdigest()}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    repo = args.repo.resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    evidence = repo / "docs" / "preset-evidence" / REVISION
    work_log = evidence / "work-log.json"
    original_work_log = work_log.read_bytes()
    ledger = json.loads(original_work_log)
    if ledger["revision"] != REVISION or ledger["candidate_source"] != SOURCE:
        raise SystemExit("Unexpected ledger revision or source")
    git = lambda *cmd: subprocess.check_output(["git", *cmd], cwd=repo, text=True).strip()
    report_records = []
    links = []
    inputs = {}
    for entry in ledger["assignments"]:
        report = (evidence / entry["report"]).resolve()
        if not report.is_relative_to(evidence.resolve()):
            raise SystemExit("Report escaped current revision")
        record = dict(entry)
        record["report_repo_path"] = report.relative_to(repo).as_posix()
        record["report_exists"] = report.is_file()
        record["is_audit_output"] = entry["id"] == SELF
        if report.is_file() and entry["id"] != SELF:
            raw = report.read_bytes()
            text = raw.decode("utf-8")
            identity = {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
            inputs[report.relative_to(repo).as_posix()] = identity
            record["report_identity"] = identity
            record["title"] = next((line for line in text.splitlines() if line.startswith("# ")), None)
            record["headings"] = re.findall(r"^#{1,4} +(.+)", text, re.M)
            for target in re.findall(r"\]\(([^\s)]+)(?:\s+[^)]*)?\)", text):
                link = {"assignment": entry["id"], "report": record["report_repo_path"], "target": target}
                if re.match(r"[A-Za-z][A-Za-z0-9+.-]*:", target):
                    link["kind"] = "external_not_fetched"
                elif target.startswith("#"):
                    link["kind"] = "anchor_only_not_checked"
                else:
                    raw_target = unquote(target.split("#")[0].split("?")[0])
                    resolved = (report.parent / raw_target).resolve()
                    link["kind"] = "local"
                    link["exists"] = resolved.exists()
                    link["path"] = resolved.relative_to(repo).as_posix() if resolved.is_relative_to(repo) else str(resolved)
                    if resolved.is_file():
                        link["identity"] = digest(resolved)
                        inputs[link["path"]] = link["identity"]
                    elif resolved.is_dir():
                        link["is_directory"] = True
                        link["directory_contents_recursively_hashed"] = False
                links.append(link)
        else:
            record["report_identity"] = None
            if entry["id"] == SELF:
                record["hash_exclusion_reason"] = "This audit's own report and team document are generated after the input checkpoint; no self-referential hash cycle."
        report_records.append(record)
    ids = [a["id"] for a in ledger["assignments"]]
    agents = [a["agent"] for a in ledger["assignments"]]
    roles = [a["role"] for a in ledger["assignments"]]
    duplicates = lambda values: sorted(k for k, n in collections.Counter(values).items() if n > 1)
    expected = [f"a{i:02d}" for i in range(1, 61)]
    missing_ids = [key for key in expected if key not in ids]
    complete = [a for a in report_records if a["status"] == "completed"]
    missing_completed = [a["id"] for a in complete if not a["report_exists"]]
    missing_reports = [a["id"] for a in report_records if not a["report_exists"]]
    missing_links = [link for link in links if link["kind"] == "local" and not link["exists"]]
    protected = [".github", "CMakeLists.txt", "cmake", "scripts", "src", "tests"]
    protected_changed = git("diff", "--name-only", SOURCE, "--", *protected).splitlines()
    protected_untracked = [p for p in git("ls-files", "--others", "--exclude-standard").splitlines() if p == "CMakeLists.txt" or p.split("/")[0] in protected]
    changed_inputs = []
    for path, before in inputs.items():
        actual = repo / path
        if not actual.is_file() or digest(actual) != before:
            changed_inputs.append(path)
    status_counts = dict(collections.Counter(a["status"] for a in report_records))
    snapshot = {
        "schema": 1,
        "scope": "Current 2026.11.7 assignment ledger, report and local-citation integrity checkpoint; no product execution, listening or native acceptance",
        "recorded_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "revision": REVISION,
        "binary_source": SOURCE,
        "binary_tree": git("rev-parse", SOURCE + "^{tree}"),
        "documentation_head_observed": git("rev-parse", "HEAD"),
        "work_log_identity": {"bytes": len(original_work_log), "sha256": hashlib.sha256(original_work_log).hexdigest()},
        "requested_assignments": ledger["requested_assignments"],
        "actual_recorded_assignment_count": len(ids),
        "unique_ids": len(set(ids)),
        "unique_canonical_task_names": len(set(agents)),
        "unique_role_strings": len(set(roles)),
        "duplicate_ids": duplicates(ids),
        "duplicate_canonical_task_names": duplicates(agents),
        "duplicate_role_strings": duplicates(roles),
        "invalid_or_out_of_range_ids": [key for key in ids if key not in expected],
        "invalid_canonical_task_names": [a for a in agents if not re.fullmatch(r"/root/a[0-9]{2}_[a-z0-9_]+", a)],
        "id_to_canonical_name_mismatches": [a["id"] for a in ledger["assignments"] if not a["agent"].startswith("/root/" + a["id"] + "_")],
        "not_yet_recorded_assignments": missing_ids,
        "coordinator_or_prior_revision_assignments_added": 0,
        "followup_turns_or_fixture_helpers_added": 0,
        "ledger_declared_assigned_count": ledger["assigned_count"],
        "ledger_declared_completed_count": ledger["completed_count"],
        "computed_completed_count": len(complete),
        "status_counts": status_counts,
        "declared_counts_agree": ledger["assigned_count"] == len(ids) and ledger["completed_count"] == len(complete),
        "missing_report_ids_at_input_checkpoint": missing_reports,
        "missing_completed_report_ids": missing_completed,
        "external_report_count": sum(a["report_exists"] and not a["is_audit_output"] for a in report_records),
        "local_markdown_citation_count": sum(link["kind"] == "local" for link in links),
        "external_markdown_citation_count": sum(link["kind"] == "external_not_fetched" for link in links),
        "missing_local_markdown_citations": missing_links,
        "readset_file_count": len(inputs),
        "readset_changed_at_close": changed_inputs,
        "work_log_unchanged_at_close": work_log.read_bytes() == original_work_log,
        "protected_working_source_changes": protected_changed,
        "protected_working_source_untracked": protected_untracked,
        "exactly_sixty_distinct_assignments_recorded": len(ids) == 60 and len(set(agents)) == 60 and not missing_ids and not duplicates(ids),
        "all_sixty_assignments_completed": len(ids) == 60 and len(complete) == 60 and not missing_completed,
        "a59_phase1_review_complete": True,
        "a59_assignment_complete": False,
        "native_or_platform_qualification_claimed": False,
        "release_qualified": False,
        "human_listening_approval_claimed": False,
        "assignments": report_records,
    }
    (out / "work-log-snapshot.json").write_bytes(original_work_log)
    for name, value in (("ledger-checkpoint.json", snapshot), ("report-citations.json", links), ("readset.json", inputs)):
        (out / name).write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps({k: v for k, v in snapshot.items() if k != "assignments"}, indent=2))


if __name__ == "__main__":
    main()
