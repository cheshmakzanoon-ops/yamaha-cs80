#!/usr/bin/env python3
"""Review immutable source/doc/evidence joins; no product execution or mutations."""
from pathlib import Path, PurePosixPath
import collections
import datetime
import hashlib
import json
import posixpath
import re
import subprocess
import urllib.parse
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build-w13-acceptance"
REF = "814c9014b65b5ac5e48aa6ead8d5c2b4fec568f4"
TREE = "c52340c61778e54b5453e64a281c086a5ba48cef"
SOURCE = "6d3cc220edef5a4a2c7cd968ea8a786c7c83198c"
BASE = "7f55a95a7534b389927defe46866456560b3a878"
DOSSIER = "docs/preset-evidence/2026.11.1/"
checks = []
blobs = {}


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)


def blob(path, ref=REF):
    key = ref + ":" + path
    if key not in blobs:
        blobs[key] = git("show", key)
    return blobs[key]


def obj(path, ref=REF):
    return json.loads(blob(path, ref))


def sha(data):
    return hashlib.sha256(data).hexdigest()


def check(name, condition, detail=None):
    checks.append({"check": name, "pass": bool(condition), "detail": detail})
    if not condition:
        raise SystemExit("HANDOFF REVIEW BLOCKED: " + name + ": " + str(detail))


def localj(name):
    return json.loads((ROOT / name).read_bytes())


def count(value):
    return len(value) if isinstance(value, list) else value


ref_api = localj("build-w13-acceptance/final-observed/main-ref.json")
commit_api = localj("build-w13-acceptance/final-observed/reviewed-commit.json")
check("published main identity", ref_api["object"]["sha"] == REF and commit_api["sha"] == REF
      and commit_api["tree"]["sha"] == TREE and git("rev-parse", REF + "^{tree}").decode().strip() == TREE)
check("binary source is ancestor", subprocess.run(["git", "merge-base", "--is-ancestor", SOURCE, REF], cwd=ROOT).returncode == 0)
delta = git("diff", "--name-only", SOURCE, REF).decode().splitlines()
publisher_files = {".github/workflows/publish-preset-refinement.yml", "scripts/publish-preset-refinement.py", "scripts/test-publish-preset-refinement.py"}
allowed = lambda p: p == "README.md" or p.startswith("docs/") or p in publisher_files
check("post-source path allowlist", all(allowed(p) for p in delta), {"count": len(delta), "non_documentation": sorted(set(delta) & publisher_files)})
check("binary runtime, tests and required build workflow unchanged", git("diff", "--name-only", SOURCE, REF, "--", "src", "tests", "CMakeLists.txt", ".github/workflows/build.yml").strip() == b"")
check("publisher unchanged since reviewed body-only23e53bb change", git("diff", "--name-only", "23e53bb", REF, "--", *sorted(publisher_files)).strip() == b"")
check("publisher unchanged since attempted publication head96c466f", git("diff", "--name-only", "96c466f", REF, "--", *sorted(publisher_files)).strip() == b"")
plan_bytes = (OUT / "acceptance-plan.json").read_bytes()
check("original formal acceptance plan preserved", blob(DOSSIER + "final-ci/acceptance/preparation/acceptance-plan.json") == plan_bytes)
plan = json.loads(plan_bytes)
check("original first-attempt source/run preserved", plan["source_commit"] == SOURCE and plan["run_id"] == 45044040694 and plan["run_attempt"] == 1)
check("separate older revision/history bytes preserved", git("diff", "--name-only", "96c466f", REF, "--", "docs/PRESET_REVISION_2026_11.md", "docs/TEAM_REVIEW_2026_11.md", "docs/preset-evidence/2026.11").strip() == b"")
source_paths = git("diff", "--name-only", BASE, SOURCE, "--", "src").decode().splitlines()
check("preset-only production scope from baseline", set(source_paths) == {
    "src/core/CuratedLibraryLevels.inc", "src/core/FactoryPresets.h",
    "src/core/curated/Bass.inc", "src/core/curated/Brass.inc", "src/core/curated/Fx.inc",
    "src/core/curated/Pads.inc", "src/core/curated/Plucked.inc", "src/core/curated/Polysynth.inc", "src/core/curated/Winds.inc"})

manifest = obj(DOSSIER + "provenance.json")
entries = manifest["files"]
paths = [x["path"] for x in entries]
check("522 unique manifest payloads", len(paths) == len(set(paths)) == 522)
tracked = set(git("ls-tree", "-r", "--name-only", REF, "--", DOSSIER).decode().splitlines())
check("complete tracked dossier coverage", tracked == {DOSSIER + p for p in paths} | {DOSSIER + "provenance.json"})
bad = []
for entry in entries:
    data = blob(DOSSIER + entry["path"])
    if len(data) != entry["bytes"] or sha(data) != entry["stored_sha256"]:
        bad.append(entry["path"])
check("all522 pinned payload sizes and hashes", not bad, bad)
preserved = {}
for previous, expected in [(SOURCE, 80), ("96c466f", 262)]:
    old = obj(DOSSIER + "provenance.json", previous)["files"]
    current = {e["path"]: e for e in entries}
    unchanged = [e["path"] for e in old if e == current.get(e["path"]) and blob(DOSSIER + e["path"], previous) == blob(DOSSIER + e["path"])]
    amended = [e["path"] for e in old if e["path"] not in unchanged]
    check("historical payload preservation " + previous[:8], len(unchanged) == expected and amended == ["README.md"])
    preserved[previous] = {"original_count": len(old), "unchanged_entries_and_bytes": len(unchanged), "amended": amended}

run = localj("build-w13-acceptance/final-observed/build-run.json")
jobs = localj("build-w13-acceptance/final-observed/build-jobs.json")
check("failed original attempt1 remains actual outcome", run["id"] == 45044040694 and run["head_sha"] == SOURCE
      and run["run_attempt"] == 1 and run["status"] == "completed" and run["conclusion"] == "failure")
check("all six original jobs identified", jobs["total_count"] == len(jobs["jobs"]) == 6
      and set(j["name"] for j in jobs["jobs"]) == set(plan["required_jobs"]))
job_results = []
for job in jobs["jobs"]:
    required = plan["required_jobs"][job["name"]]
    states = {s["name"]: s["conclusion"] for s in job["steps"] if s["name"] in required}
    check("source/run/attempt join " + job["name"], job["head_sha"] == SOURCE and job["run_id"] == 45044040694 and job["run_attempt"] == 1
          and set(states) == set(required))
    expected = {s: ("failure" if s == "Validate VST3 GUI under Xvfb" else "success") for s in required}
    check("actual required step outcomes " + job["name"], states == expected)
    job_results.append({"name": job["name"], "id": job["id"], "conclusion": job["conclusion"], "required_steps": states})

native_counts = []
for folder, total, passed, skipped in [("windows-x86_64", 221, 219, 2), ("macos-arm64", 243, 241, 2), ("macos-x86_64", 243, 241, 2), ("linux-x86_64", 579, 578, 1)]:
    if folder == "linux-x86_64":
        proof = obj(DOSSIER + "final-ci/linux-x86_64/native-proof.json")
        archive = Path(proof["test_evidence_download"]["path"])
        data = archive.read_bytes()
        check("Linux native evidence ZIP identity", len(data) == proof["test_evidence_download"]["bytes"] and sha(data) == proof["test_evidence_download"]["sha256"])
        with zipfile.ZipFile(archive) as z:
            data = z.read(proof["junit_member"]["path"])
        check("Linux actual JUnit bytes", len(data) == proof["junit_member"]["bytes"] and sha(data) == proof["junit_member"]["sha256"])
    else:
        data = blob(DOSSIER + "final-ci/" + folder + "/ctest-results.xml")
    tree = ET.fromstring(data)
    cases = tree.findall(".//testcase")
    skipped_cases = [c for c in cases if c.find("skipped") is not None]
    failures = [c for c in cases if c.find("failure") is not None or c.find("error") is not None]
    passed_cases = [c for c in cases if c.attrib.get("status") == "run" and c not in skipped_cases and c not in failures]
    check("actual native JUnit " + folder, len(cases) == total and len(passed_cases) == passed and len(skipped_cases) == skipped and not failures)
    check("native skip reasons " + folder, all(c.find("skipped").attrib["message"] == "SKIP_RETURN_CODE=77" for c in skipped_cases))
    native_counts.append({"platform": folder, "registered_and_reported": total, "passed": len(passed_cases), "skipped": len(skipped_cases), "failed": 0,
                          "skip_names": [c.attrib["name"] for c in skipped_cases], "junit_sha256": sha(data)})
linux = obj(DOSSIER + "final-ci/linux-x86_64/ci-proof.json")
core = blob(DOSSIER + "final-ci/linux-x86_64/validator-reports/pluginval-gui-core.log").decode()
check("original native SIGSEGV and zero retries retained", linux["status"] == "FAIL" and linux["internal_retry_count"] == 0
      and linux["original_crash_stack_available"] is False and "PID=2883; raw returncode=-11; kind=signal; signal_number=11; signal_name=SIGSEGV" in core)
check("independent failure review retained", obj(DOSSIER + "final-ci/failure-review/summary.json")["acceptance"] == "BLOCKED")

public = obj(DOSSIER + "final-ci/public-candidates/summary.json")
installation = obj(DOSSIER + "final-ci/installation/artifact-proof.json")
expected_ids = {14763872941: ("windows", "x86_64", ["VST3", "Standalone"]), 14763761497: ("macos", "arm64", ["VST3", "AU", "Standalone"]),
                14763644216: ("macos", "x86_64", ["VST3", "AU", "Standalone"]), 14768807632: ("linux", "x86_64", ["VST3", "Standalone"])}
check("four actual archive records", installation["artifact_count"] == 4 and len(installation["artifacts"]) == 4)
for artifact in installation["artifacts"]:
    platform, arch, formats = expected_ids[artifact["artifact_id"]]
    check("native formats and architecture " + str(artifact["artifact_id"]), artifact["platform"] == platform and artifact["architecture"] == arch and set(artifact["formats"]) == set(formats))
check("114 actual inventory entries", sum(a["inventory_entries"] for a in installation["artifacts"]) == 114)
candidates = public["public_candidate_artifacts"]
check("five public full-GET records", len(candidates) == 5 and {a["artifact_id"] for a in candidates} == set(expected_ids) | {14769036590})
for record in candidates:
    data = (ROOT / record["cached_file"]).read_bytes()
    check("candidate cached bytes/fullGET join " + str(record["artifact_id"]), record["credential_free_get"] is True and record["http_status"] == 200
          and len(data) == record["bytes"] and sha(data) == record["sha256"] and record["release_qualified"] is False
          and record["source_commit"] == SOURCE and record["workflow_run_id"] == 45044040694 and record["build_run_attempt"] == 1)
    check("pinned candidate URL " + str(record["artifact_id"]), record["url"] == f"https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/{record['artifact_id']}.zip"
          and record["expires_at"] == "2027-02-20T16:28:38Z")
audition = obj(DOSSIER + "final-ci/auditions/artifact-proof.json")
check("actual CI audition proof", audition["source_commit"] == SOURCE and audition["run_id"] == 45044040694 and audition["run_attempt"] == 1
      and audition["artifact_id"] == 14769036590 and audition["wav_count"] == 24 and audition["seconds"] == 117
      and audition["stereo_frames"] == 5616000 and audition["parameter_digests_verified"] == 24
      and audition["nonfinite_samples"] == 0 and audition["samples_at_or_over_full_scale"] == 0)
publisher = localj("build-w13-acceptance/final-observed/publisher-run.json")
publish_jobs = localj("build-w13-acceptance/final-observed/publisher-jobs.json")
check("publisher actually skipped", publisher["id"] == 45060638607 and publisher["conclusion"] == "skipped" and publisher["head_sha"] == "96c466f0e7c25055ac8e745d2c796af643860fc6"
      and len(publish_jobs["jobs"]) == 1 and publish_jobs["jobs"][0]["conclusion"] == "skipped")

docs = ["README.md", "docs/PRESET_REFINEMENT_2026_11_1.md", "docs/CI_DISTRIBUTION_STATUS.md", "docs/TEAM_REVIEW_2026_11_1.md", DOSSIER + "README.md", DOSSIER + "final-ci/README.md"]
all_tracked = set(git("ls-tree", "-r", "--name-only", REF).decode().splitlines())
links = []
missing = []
for doc in docs:
    text = blob(doc).decode()
    for raw in re.findall(r"\]\(([^)]+)\)", text):
        target = raw.split(' "', 1)[0]
        parsed = urllib.parse.urlsplit(target)
        if parsed.scheme or parsed.netloc or not parsed.path:
            continue
        resolved = posixpath.normpath(posixpath.join(posixpath.dirname(doc), urllib.parse.unquote(parsed.path)))
        ok = resolved in all_tracked or any(p.startswith(resolved.rstrip("/") + "/") for p in all_tracked)
        links.append({"from": doc, "target": target, "resolved_path": resolved})
        if not ok:
            missing.append(links[-1])
check("current document local link targets", not missing, {"checked": len(links), "missing": missing, "scope": "file/directory targets; separate final-observed/anchor-checks.json covers15 local anchor links"})
five_urls = {a["url"] for a in candidates}
for doc in docs[:3]:
    text = blob(doc).decode()
    if doc.endswith("CI_DISTRIBUTION_STATUS.md"):
        text = text.split("## Pinned verified development downloads — 2026.11", 1)[0]
    observed = set(re.findall(r"https://nightly\.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/\d+\.zip", text))
    check("five exact current download targets " + doc, observed == five_urls)
readme = blob("README.md").decode()
report = blob("docs/PRESET_REFINEMENT_2026_11_1.md").decode()
check("public release and claim limits", all(s in readme for s in ["development candidates", "full release is blocked", "raw −11 / wrapper 139", "222 Legacy", "serialized sound", "Pure CS-80 centers rank pans", "Pressure Lead requires aftertouch", "processing queue", "physical-hardware match"]))
check("controller and older-module attribution retained", all(s in " ".join(report.split()) for s in ["controller", "exact compile commit is unknown", "No runtime", "sample-accurate"]))

proof59_path = ROOT / "build-w13-release-docs/published-candidate-verification.json"
proof59_bytes = proof59_path.read_bytes()
proof59 = json.loads(proof59_bytes)
check("assignment59 actual completed review", sha(proof59_bytes) == "87bae0eefaa906e8697378cb0da76c0b659c253b51a321e48b1437d8ec5e0c7e"
      and proof59["documentation_commit"] == REF and proof59["status"] == "PUBLISHED_CANDIDATE_DOCS_AND_LINKS_VERIFIED_RELEASE_UNQUALIFIED")
check("assignment59 fullGET proof hash join", proof59["full_candidate_byte_proof_sha256"] == sha(blob(proof59["full_candidate_byte_proof_path"])))
check("17 actual public doc GETs", len(proof59["document_gets"]) == 17)
for doc in proof59["document_gets"]:
    ref = REF if doc["ref"] == "main" else doc["ref"]
    data = blob(doc["path"], ref)
    check("published doc bytes " + doc["ref"][:8] + ":" + doc["path"], doc["http_status"] == 200 and len(data) == doc["bytes"] and sha(data) == doc["sha256"])
check("five bounded candidate availability checks", len(proof59["candidate_availability_gets"]) == 5)
by_id = {a["artifact_id"]: a for a in candidates}
for record in proof59["candidate_availability_gets"]:
    original = by_id[record["artifact_id"]]
    check("availability/fullGET evidence joined " + str(record["artifact_id"]), record["http_status"] == 206 and record["bytes_read"] == 4
          and record["zip_signature"] == "504b0304" and record["content_range"] == f"bytes 0-3/{original['bytes']}"
          and record["prior_full_get_sha256"] == original["sha256"] and record["availability_only"] is True)
check("public qualified release still absent", proof59["public_release_visibility"]["http_status"] == 404 and public["public_release_tag_http_status"] == 404)

public_ledger = obj(DOSSIER + "final-ci/team/ledger.json")
coordinator_snapshot = "build-w13-acceptance/final-observed/coordinator-at-final-decision.json"
coordinator = localj(coordinator_snapshot if (ROOT / coordinator_snapshot).exists() else "build-coordination/agents.json")
check("60 actual distinct current assignments", len(coordinator) == 60 and {x["assignment"] for x in coordinator} == set(range(1, 61)) and len({x["agent"] for x in coordinator}) == 60)
check("published57-completed ledger is explicitly historical", public_ledger["completed_assignments"] == 57 and public_ledger["all60_completed"] is False
      and public_ledger["separate_earlier2026_11_ledger_excluded"] is True and public_ledger["followup_turns_counted_as_new_agents"] is False)
check("coordinator matches published distinct-agent identities", {(a["assignment"], a["agent"]) for a in coordinator} == {(a["assignment"], a["agent"]) for a in public_ledger["agents"]})
check("other58 recorded completed outcomes", all(a["status"] == "completed" for a in coordinator if a["assignment"] <= 58))
check("assignment58 actual scoped final return evidence", localj("build-w12-team-ledger/final-report-review.json")["actual_distinct_agents"] == 60)
check("first59 scoped assignments complete before60 decision", all(a["status"] == "completed" for a in coordinator if a["assignment"] <= 58)
      and proof59["status"] == "PUBLISHED_CANDIDATE_DOCS_AND_LINKS_VERIFIED_RELEASE_UNQUALIFIED")

criteria = {
    "source_scope": ("PASS", "Reviewed twelve-program refinement remains the frozen binary source; subsequent publication is docs/publisher only."),
    "build_identity": ("FAIL", "Exact attempt1 completed failure, not required success."),
    "native_and_validator_jobs": ("FAIL", "Four native jobs and Windows validation pass; required Linux randomized GUI validation fails."),
    "validator_attempt_history": ("FAIL", "Original GUI raw -11 after SUCCESS; zero internal retries. Diagnostics and beta comparison do not substitute."),
    "four_native_archives": ("PASS", "All four actual candidate bytes/inventories/architectures/formats verified in scoped native and installation records."),
    "actual_ci_auditions": ("PASS", "Actual source6d3 CI artifact verifies24 WAVs,117seconds,5616000stereo frames,24 parameter digests and32 checksum entries."),
    "installation_contents": ("PASS", "Four archive-content reviews verify114 inventory entries; no native installation/signing/every-DAW claim."),
    "public_release_bytes": ("BLOCKED", "No qualified public release or its required seven assets; five candidate GETs are separate evidence."),
    "publication_identity": ("BLOCKED", "No release tag/receipt/publication join. This candidate-doc snapshot has492 allowlisted paths, exceeding the unchanged<300 release-publication comparison criterion."),
    "public_documents": ("PARTIAL", "Candidate documents and links are verified and truthful; required release-level asset/receipt links cannot be certified because the release is absent."),
    "sixty_current_assignments": ("PARTIAL", "60distinct assignments and all other59 scoped completions are established; assignment60 completes this review with formal acceptance BLOCKED, so its original PASS predicate is unmet."),
    "evidence_and_claims": ("PASS", "All522 pinned payloads verify; historical failures, Legacy/state, pressure/mode and Program-queue qualifications retained; no acoustic/hardware/listening verdict.")}
check("all original criteria explicitly adjudicated", set(criteria) == {x["id"] for x in plan["checklist"]})
result = {
    "schema": 1, "assignment": 60, "reviewed_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    "reviewed_publication_commit": REF, "reviewed_tree": TREE, "binary_source_commit": SOURCE,
    "baseline_commit": BASE, "run_id": 45044040694, "run_attempt": 1,
    "formal_release_acceptance": "BLOCKED", "required_linux_gate": "FAIL",
    "candidate_handoff_review": "VERIFIED_RELEASE_UNQUALIFIED", "assignment60_work_completed": True,
    "overall_original_user_goal_complete": False,
    "original_acceptance_plan_sha256": sha(plan_bytes), "original_criteria_changed": False,
    "original_criteria": [{"id": row["id"], "original_criterion": row["criterion"], "outcome": criteria[row["id"]][0], "assessment": criteria[row["id"]][1]} for row in plan["checklist"]],
    "source_delta": {"paths": len(delta), "all_paths_allowlisted": True, "non_documentation_paths": sorted(set(delta) & publisher_files), "runtime_or_required_build_gate_changed": False,
                     "original_release_comparison_path_limit": "fewer than300", "reviewed_snapshot_meets_that_limit": len(delta) < 300},
    "dossier": {"payloads": len(entries), "all_sizes_hashes_and_tracked_coverage_match": True, "manifest_sha256": sha(blob(DOSSIER + "provenance.json")), "preservation": preserved},
    "required_jobs": job_results, "native_ctest": native_counts,
    "public_candidates": candidates,
    "audition": {k: audition[k] for k in ["artifact_id", "outer_bytes", "outer_sha256", "wav_count", "seconds", "stereo_frames", "parameter_digests_verified", "nonfinite_samples", "samples_at_or_over_full_scale"]},
    "publisher": {"run_id": 45060638607, "conclusion": "skipped", "workflow_commit": publisher["head_sha"], "job_id": publish_jobs["jobs"][0]["id"], "job_conclusion": "skipped", "release_http_status": 404},
    "assignment59": {"status": proof59["status"], "proof_sha256": sha(proof59_bytes), "verified_at_utc": proof59["verified_at_utc"], "document_gets": 17, "candidate_range_gets": 5, "range_gets_are_not_full_hash_checks": True},
    "team": {"current_unique_assignments": 60, "current_unique_agents": 60, "other59_scoped_assignments_completed": True,
             "coordinator_completed_at_snapshot": sum(a["status"] == "completed" for a in coordinator),
             "published_ledger_completed_at_its_dated_snapshot": 57, "assignment59_completion_supported_by_actual_return_and_proof": True,
             "assignment60_completion_is_review_completion_not_release_PASS": True,
             "initial_coordination_only_assignment_limits_retained": public_ledger["initial_coordination_only_assignments"],
             "earlier_separate60_not_counted": True, "human_listening_panel_claim": False},
    "document_local_file_or_directory_links_checked": len(links),
    "exact_remaining_blocker": {"job_id": 137324977642, "lane": "randomized Linux GUI", "seed": "0x4369262", "pid": 2883, "raw_returncode": -11, "signal": "SIGSEGV", "normalized_wrapper_exit": 139, "success_printed_before_fault": True, "internal_retries": 0},
    "diagnostic_limit": "No original core/fault stack. Same-seed GDB/preload replays exit0. Actual beta3 three-lane comparison uses another validator and different seeds. No ownership or repair is established.",
    "next_step": "Keep release blocked. Obtain bounded same-artifact/original-seed stable-validator fault evidence on an already capable Linux diagnostic host; match original PID to a permitted local core and capture argument-free stacks/module identity if it faults. No retry-until-green or gate replacement.",
    "sealing_scope": "Root may archive this actual review and assignment59proof and update ledger59/60 to completed in a subsequent docs-only seal. This review remains pinned to814c9014 and522payloads. Seal must not relabel formal acceptance, rewrite prior snapshots, change binary source/gates/candidate URLs, or claim a qualified release.",
    "product_executions": 0, "ci_retries_or_cancellations": 0, "production_or_gate_changes": 0,
    "evidence_checks_passed": len(checks), "checks": checks,
}
print(json.dumps(result, indent=2))
