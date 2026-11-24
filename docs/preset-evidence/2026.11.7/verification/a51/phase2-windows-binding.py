#!/usr/bin/env python3
"""Compose the observed Windows GET with original A46 evidence and ARM phase 1.

No network operation, product execution, archive copy, or publication occurs here.
All new receipt paths must be absent; only this agent's rolling review is updated.
"""
import copy
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[5]
BASE = ROOT / "docs/preset-evidence/2026.11.7"
CI = BASE / "ci/source-ee0dabca"
WIN = CI / "windows-x86_64"
OWN = BASE / "verification/a51"
PUBLIC = CI / "public-downloads"
SOURCE = "ee0dabcacb4cdff93f8803844f60348e0ceda6ed"
TREE = "17115913cbb29157bd00539485bcc5f38b57219c"
RUN, ATTEMPT, PRODUCER, CONSUMER = 45397189410, 1, 138455976256, 138483772246
QUAL_SHA = "a86ab93d18fb390359f60be8a50ed9cc09b34c9b70aab8922c23581fa455c048"
RETRIEVAL_SHA = "b914626a64b3dec3305a0228540f4976d908570741bc6b131dcd6236e8fd9938"
HELPER = ROOT / "docs/preset-evidence/2026.11.5/verification/a57-probe-public-artifact.py"
PROBE = PUBLIC / "windows-x86_64-full-get-0001.json"
PREVIOUS = PUBLIC / "download-mapping-phase1.json"


def rel(path):
    return str(path.relative_to(ROOT))


def identity(path):
    raw = path.read_bytes()
    return {"path": rel(path), "bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def read(path):
    return json.loads(path.read_text())


def stamp(value):
    return dt.datetime.fromisoformat(value.replace("Z", "+00:00"))


def verify_identity(path, expected):
    actual = identity(path)
    assert (actual["bytes"], actual["sha256"]) == (expected["bytes"], expected["sha256"])
    return actual


def write_new(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x") as stream:
        if isinstance(value, str):
            stream.write(value)
        else:
            json.dump(value, stream, indent=2)
            stream.write("\n")


now = dt.datetime.now(dt.timezone.utc)
q = read(WIN / "platform-qualification.json")
r = read(WIN / "retrieval-receipt.json")
assert identity(WIN / "platform-qualification.json")["sha256"] == QUAL_SHA
assert identity(WIN / "platform-qualification.json")["bytes"] == 160850
assert identity(WIN / "retrieval-receipt.json")["sha256"] == RETRIEVAL_SHA
assert (q["source"], q["tree"], q["run_id"], q["attempt"], q["producer_job_id"],
        q["consumer_job_id"], q["factory_revision"], q["target"]) == (
            SOURCE, TREE, RUN, ATTEMPT, PRODUCER, CONSUMER, "2026.11.7", "windows-x86_64")
assert (r["source_commit"], r["run_id"], r["attempt"], r["producer_job_id"],
        r["consumer_job_id"]) == (SOURCE, RUN, ATTEMPT, PRODUCER, CONSUMER)
assert q["platform_qualified"] is True and q["release_qualified"] is False and q["global_qualified"] is False
assert q["original_attempt_binding_verified"] is True
assert len(q["required_gates"]) == 17 and all(g["actual_verified"] is True for g in q["required_gates"])
for name, expected in r["files"].items():
    verify_identity(WIN / name, expected)
verify_identity(WIN / "retrieval-receipt.json", q["acquisition"]["root_receipt"])
for item in q["input_report_identities"]:
    verify_identity(Path(item["path"]), item)
assert subprocess.check_output(["git", "rev-parse", SOURCE + "^{tree}"], cwd=ROOT, text=True).strip() == TREE
assert subprocess.check_output(["git", "show", SOURCE + ":" + rel(HELPER)], cwd=ROOT) == HELPER.read_bytes()

a = read(WIN / "install-api.json")
run = read(WIN / "run-api-at-platform-completion.json")
attempt = read(WIN / "attempt1-api-at-platform-completion.json")
producer = read(WIN / "producer-job-api.json")
consumer = read(WIN / "validator-job-api.json")
original = read(WIN / "original-ci-binding.json")
probe = read(PROBE)
for context in (run, attempt):
    assert (context["id"], context["run_attempt"], context["head_sha"]) == (RUN, ATTEMPT, SOURCE)
    assert context["repository"]["full_name"] == "cheshmakzanoon-ops/yamaha-cs80"
for context, job_id in ((producer, PRODUCER), (consumer, CONSUMER)):
    assert (context["id"], context["run_id"], context["run_attempt"], context["head_sha"],
            context["status"], context["conclusion"]) == (job_id, RUN, ATTEMPT, SOURCE, "completed", "success")
assert (original["source"], original["tree"], original["run_id"], original["attempt"],
        original["producer_job_id"], original["consumer_job_id"]) == (SOURCE, TREE, RUN, ATTEMPT, PRODUCER, CONSUMER)
upload = q["artifact_bindings"]["install"]
assert upload == original["artifact_bindings"]["install"]
assert (upload["producing_job_id"], upload["producing_attempt"], upload["original_upload_id_size_digest_and_time_verified"]) == (PRODUCER, ATTEMPT, True)
assert (a["id"], a["name"], a["size_in_bytes"], a["digest"]) == (
    14925251949, "cs80-windows-install-bundle", 6409200,
    "sha256:8f8f885c72910a0af02055036203a6a2e0aa96a9390adce7c1ec480243ba0feb")
assert (a["id"], a["name"], a["size_in_bytes"], a["digest"]) == (
    upload["artifact_id"], upload["name"], upload["bytes"], "sha256:" + upload["sha256"])
assert (a["workflow_run"]["id"], a["workflow_run"]["head_sha"]) == (RUN, SOURCE)
assert a["workflow_run"]["repository_id"] == run["repository"]["id"] == a["workflow_run"]["head_repository_id"]
api_url = f"https://api.github.com/repos/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/{a['id']}"
assert a["url"] == api_url and a["archive_download_url"] == api_url + "/zip"
assert a["expired"] is False and a["expires_at"] == upload["expires_at"]
assert a["created_at"] == upload["api_created_at"] and a["updated_at"] == upload["api_updated_at"]
expires = stamp(a["expires_at"])
assert expires > now and expires > stamp(probe["completed_at"])
step = next(s for s in producer["steps"] if s["number"] == upload["upload_step"]["number"])
assert step == upload["upload_step"] and (step["status"], step["conclusion"]) == ("completed", "success")
for field in ("upload_start_utc", "upload_finish_utc"):
    assert stamp(step["started_at"]) <= stamp(upload[field]) < stamp(step["completed_at"]) + dt.timedelta(seconds=1)
lines = (WIN / "producer-job.log").read_text().splitlines()
upload_lines = [{"line": i, "text": line} for i, line in enumerate(lines, 1)
                if str(a["id"]) in line or upload["sha256"] in line]
assert any("SHA256 digest of uploaded artifact zip is " + upload["sha256"] in item["text"] for item in upload_lines)
assert any(f"Artifact {a['name']} has been successfully uploaded! Final size is {a['size_in_bytes']} bytes. Artifact ID is {a['id']}" in item["text"] for item in upload_lines)

assert (probe["source"], probe["run_id"], probe["attempt_context"], probe["artifact_id"],
        probe["artifact_name"]) == (SOURCE, RUN, ATTEMPT, a["id"], a["name"])
assert probe["artifact_api_sha256"] == identity(WIN / "install-api.json")["sha256"]
assert probe["run_api_sha256"] == identity(WIN / "run-api-at-platform-completion.json")["sha256"]
assert (probe["actual_bytes"], probe["expected_bytes"], probe["actual_sha256"], probe["expected_sha256"]) == (
    a["size_in_bytes"], a["size_in_bytes"], upload["sha256"], upload["sha256"])
assert probe["method"] == "GET" and probe["credentials_supplied"] is False
assert probe["status"] == 200 and "content-range" not in probe["headers"]
assert probe["public_route_verified"] is True and probe["release_qualified"] is False
assert probe["artifact_attempt_binding_verified"] is False and "error" not in probe
assert probe["requested_url"] == f"https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/{a['id']}.zip"
for endpoint in [probe["final_endpoint"]] + [item[side] for item in probe["redirects"] for side in ("from", "to")]:
    assert set(endpoint) == {"scheme", "host", "path", "query_redacted"}
    assert endpoint["scheme"] == "https" and "?" not in endpoint["path"]

assert identity(PREVIOUS)["sha256"] == "b8eb878ed7fd4472a11defe7e897d2ebbcd50bd2ac4cee104054db62b2225bbd"
previous = read(PREVIOUS)
assert (previous["source_commit"], previous["source_tree"], previous["run_id"], previous["run_attempt"]) == (SOURCE, TREE, RUN, ATTEMPT)
assert previous["verified_public_route_count"] == 1
arm = previous["verified_downloads"][0]
assert arm["target"] == "macos-arm64"
for key in ("public_get_receipt", "identity_binding_receipt", "external_platform_qualification"):
    verify_identity(ROOT / arm[key]["path"], arm[key])
assert stamp(arm["expires_at_utc"]) > now

binding = {
    "schema": 1, "status": "phase2_windows_public_route_bound", "verified_at_utc": now.isoformat(),
    "source_commit": SOURCE, "source_tree": TREE, "factory_revision": "2026.11.7",
    "run_id": RUN, "run_attempt": ATTEMPT, "producer_job_id": PRODUCER, "consumer_job_id": CONSUMER,
    "target": "windows-x86_64", "artifact_id": a["id"], "artifact_name": a["name"],
    "authenticated_raw_metadata_digests_match_root_receipt": True,
    "root_receipt_and_A46_qualification_match_handoff_pins": True,
    "external_A46_required_gate_count": 17, "external_A46_required_gates_all_accepted": True,
    "external_A46_report_identities_rechecked": len(q["input_report_identities"]),
    "unchanged_probe_matches_frozen_source": True,
    "original_producer_upload_attempt_binding_verified": True,
    "original_upload": upload, "original_upload_log_lines": upload_lines,
    "external_platform_qualification": identity(WIN / "platform-qualification.json"),
    "input_files": [identity(WIN / name) for name in (
        "retrieval-receipt.json", "install-api.json", "run-api-at-platform-completion.json",
        "attempt1-api-at-platform-completion.json", "producer-job-api.json", "producer-job.log",
        "validator-job-api.json", "validator-job.log", "original-ci-binding.json")]
        + [identity(HELPER), identity(PROBE), identity(PREVIOUS)],
    "full_public_get_verified": True, "probe_process_exit_code_observed": 0,
    "public_transport_attempts": [identity(PROBE)], "failed_public_transports_observed": 0,
    "archive_retained_by_probe": False, "expired_boolean": a["expired"],
    "expires_at_is_future_at_probe_and_binding": True, "expires_at_utc": a["expires_at"],
    "expires_at_America_Halifax": expires.astimezone(ZoneInfo("America/Halifax")).isoformat(),
    "expiry_timezone_abbreviation": expires.astimezone(ZoneInfo("America/Halifax")).tzname(),
    "run_status_at_metadata_snapshot": run["status"], "run_conclusion_at_metadata_snapshot": run["conclusion"],
    "prior_ARM_get_and_binding_preserved_by_hash": True,
    "release_qualified": False, "all_five_public_downloads_verified": False,
    "scope": "One newly observed Windows public route; original attempt and platform acceptance are external A46 evidence checked for identity/integrity. Prior ARM observation is preserved at its own timestamp. No product tests were rerun."
}
binding_path = OWN / "phase2-windows-binding.json"
write_new(binding_path, binding)
mapping = copy.deepcopy(previous)
mapping.update({
    "status": "partial_two_of_five_public_routes_verified", "recorded_at_utc": now.isoformat(),
    "verified_public_route_count": 2, "previous_mapping": identity(PREVIOUS),
    "pending_targets": ["macos-x86_64", "linux-x86_64", "factory-audition"],
    "earliest_expiry_of_verified_subset_utc": min(arm["expires_at_utc"], a["expires_at"]),
    "verified_subset_expiry_is_not_complete_five_download_expiry": True,
    "pending_target_context_from_coordinator_handoff": {
        "macos-x86_64": "Original attempt 1 failed one UI meter/timer assertion. A49 found no justified source change and recommended a complete unchanged job rerun after Linux finishes. Earlier rerun API response was 403; no rerun had started at the handoff. This is not a failed public GET, and no replacement artifact is inferred.",
        "linux-x86_64": "Actual public-route handoff pending.",
        "factory-audition": "Actual public-route handoff pending."
    },
    "attempt_preservation_rule": "Every producer/run attempt and every public transport receives separate retained evidence. A later rerun does not erase the original Intel failure or change original ARM/Windows producing attempt 1.",
    "final_phase_rule": "Bind actual authenticated current-source Intel, Linux, and audition artifacts to their actual producing attempts; preserve failed transports separately; verify full public byte count/hash; compute the all-five minimum expiry only when all five are identified."
})
mapping["verified_downloads"].append({
    "target": "windows-x86_64", "label": "Windows x64 installation bundle",
    "artifact_id": a["id"], "artifact_name": a["name"], "public_url": probe["requested_url"],
    "bytes": a["size_in_bytes"], "sha256": upload["sha256"], "expired": a["expired"],
    "expires_at_utc": a["expires_at"], "expires_at_America_Halifax": binding["expires_at_America_Halifax"],
    "expiry_timezone_abbreviation": "AST", "producing_attempt": ATTEMPT,
    "producer_job_id": PRODUCER, "validator_consumer_job_id": CONSUMER,
    "probe_started_at_utc": probe["started_at"], "probe_completed_at_utc": probe["completed_at"],
    "full_unauthenticated_get_verified": True, "original_producer_upload_attempt_bound": True,
    "external_platform_qualification": binding["external_platform_qualification"],
    "public_get_receipt": identity(PROBE), "identity_binding_receipt": identity(binding_path)
})
mapping["verified_downloads"][0]["producing_attempt"] = 1
mapping_path = PUBLIC / "download-mapping-phase2.json"
write_new(mapping_path, mapping)

argv = ["python3", rel(HELPER), "--artifact-api", rel(WIN / "install-api.json"),
        "--run-api", rel(WIN / "run-api-at-platform-completion.json"), "--source", SOURCE,
        "--run-id", str(RUN), "--attempt", str(ATTEMPT), "--artifact-name", a["name"], "--out", rel(PROBE)]
write_new(OWN / "phase2-command-receipts.json", {
    "schema": 1, "recorded_at_utc": now.isoformat(), "cwd": str(ROOT),
    "commands": [{"argv": argv, "observed_exit_code": 0, "probe_started_at_utc": probe["started_at"],
                  "probe_completed_at_utc": probe["completed_at"], "public_get_receipt": identity(PROBE)}],
    "binding_verifier": identity(Path(__file__).resolve()),
    "public_download_transport_count_this_phase": 1, "public_download_failed_transports_this_phase": 0,
    "large_archive_copy_created": False, "signed_redirect_queries_retained": False
})

summary = f"""## Latest phase 2: Windows passed; two of five public routes verified

The actual Windows x64 installation artifact **{a['id']}** passed an
unauthenticated full GET from <{probe['requested_url']}>. It returned **HTTP 200**,
no partial `Content-Range`, **{a['size_in_bytes']:,} bytes**, and SHA-256
`{upload['sha256']}`, exactly matching authenticated API metadata. The observation
ran **{probe['started_at']}** through **{probe['completed_at']}**. Its first
transport succeeded; zero failed Windows public transports were observed.
The stream was hashed without retaining another ZIP or signed redirect queries.

A51 checked the supplied A46 qualification pin `{QUAL_SHA}` and authenticated
retrieval pin `{RETRIEVAL_SHA}`, all retained raw metadata/log digests, and all
referenced A46 report identities. All 17 externally qualified Windows gates remain
accepted. The exact original install upload, ID/name/bytes/hash, timestamp, API
step, source `{SOURCE}`, tree `{TREE}`, run `{RUN}`, producing attempt `1`,
producer `{PRODUCER}`, and validator consumer `{CONSUMER}` are bound. No product
tests were repeated. The helper's limited attempt-binding flag remains unchanged;
the separate binding receipt supplies the original-upload evidence.

Both verified artifacts currently report `expired: false` and the same actual
expiry: **February 22, 2027, 12:49:05 UTC**, or **8:49:05 a.m. AST
(UTC−04:00), America/Halifax**. The final five-download earliest expiry stays
unset. Prior ARM GET/metadata/qualification receipts were verified by hash and
preserved unchanged, with the original ARM observation time retained.

The combined mapping has **two verified routes and three pending targets**:
Intel Mac, Linux, and audition. Per the coordinator/A49 handoff, original Intel
attempt 1 failed a UI meter/timer assertion; the unchanged full-job rerun is a
recommendation, and the prior API 403 did not start a rerun. This is separately
recorded context, not an A51 product result or failed public transport. No Intel
replacement ID or successful attempt is inferred. Each later attempt must retain
its own source, producer, and transport evidence.

The [same-run Artifacts fallback](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/{RUN}#artifacts)
still depends on the same retained artifacts, may require sign-in, and cannot
recover expiry or deletion. API expiry is not guaranteed continuous availability;
workflow artifacts are not permanent release assets. No global release,
publisher signing/notarization, or listening acceptance is claimed.

- [Combined ARM and Windows mapping](../ci/source-ee0dabca/public-downloads/download-mapping-phase2.json).
- [Actual Windows full GET](../ci/source-ee0dabca/public-downloads/windows-x86_64-full-get-0001.json).
- [Windows identity and original-attempt binding](../verification/a51/phase2-windows-binding.json).
- [Windows commands](../verification/a51/phase2-command-receipts.json).
- [Windows binding verifier](../verification/a51/phase2-windows-binding.py).

The following phase 1 record is historical and preserves its original scope.

"""
rolling = BASE / "reviews/a51.md"
old = rolling.read_text()
assert old.startswith("# A51 — actual public download integrity and expiry mapping\n")
write_new(OWN / "phase2-windows-review.md", "# A51 Windows public route — phase 2\n\n" + summary)
rolling.write_text(old.split("\n", 1)[0] + "\n\n" + summary + old.split("\n", 1)[1].lstrip("\n"))
print(json.dumps({"binding": identity(binding_path), "mapping": identity(mapping_path),
                  "rolling_review": identity(rolling)}, indent=2))
