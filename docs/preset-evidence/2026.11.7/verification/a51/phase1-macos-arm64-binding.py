#!/usr/bin/env python3
"""Bind one already-observed public GET to retained authenticated ARM evidence.

This does not fetch, execute, repackage, qualify other targets, or publish links.
The separately retained unchanged A57 probe performed the actual public GET.
"""
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[5]
BASE = ROOT / "docs/preset-evidence/2026.11.7"
CI = BASE / "ci/source-ee0dabca"
ARM = CI / "macos-arm64"
OWN = BASE / "verification/a51"
PUBLIC = CI / "public-downloads"
SOURCE = "ee0dabcacb4cdff93f8803844f60348e0ceda6ed"
TREE = "17115913cbb29157bd00539485bcc5f38b57219c"
RUN = 45397189410
ATTEMPT = 1
JOB = 138455976227
QUAL_SHA = "f5050d2a2798ed2e4fde9686da8d40bc9ced669811fd3ebeddb1eed1e72b358b"
HELPER = ROOT / "docs/preset-evidence/2026.11.5/verification/a57-probe-public-artifact.py"
PROBE = PUBLIC / "macos-arm64-full-get-0001.json"


def rel(path):
    return str(path.relative_to(ROOT))


def receipt(path):
    raw = path.read_bytes()
    return {"path": rel(path), "bytes": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest()}


def read(path):
    return json.loads(path.read_text())


def stamp(value):
    return dt.datetime.fromisoformat(value.replace("Z", "+00:00"))


def write_new(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x") as stream:
        if isinstance(value, str):
            stream.write(value)
        else:
            json.dump(value, stream, indent=2)
            stream.write("\n")


now = dt.datetime.now(dt.timezone.utc)
retrieval = read(ARM / "retrieval-receipt.json")
assert (retrieval["source_commit"], retrieval["run_id"], retrieval["attempt"],
        retrieval["job_id"]) == (SOURCE, RUN, ATTEMPT, JOB)
for name, expected in retrieval["files"].items():
    actual = receipt(ARM / name)
    assert actual["sha256"] == expected["sha256"]
    assert actual["bytes"] == expected["bytes"]

artifact = read(ARM / "install-api.json")
run = read(ARM / "run-api-at-platform-completion.json")
attempt = read(ARM / "attempt1-api-at-platform-completion.json")
job = read(ARM / "producer-job-api.json")
qualified = read(ARM / "platform-qualification.json")
original = read(ARM / "original-execution-binding.json")
probe = read(PROBE)
assert receipt(ARM / "platform-qualification.json")["sha256"] == QUAL_SHA
assert receipt(ARM / "original-execution-binding.json")["sha256"] == qualified["original_execution_binding"]["sha256"]
assert subprocess.check_output(["git", "rev-parse", SOURCE + "^{tree}"], cwd=ROOT, text=True).strip() == TREE
assert subprocess.check_output(["git", "show", SOURCE + ":" + rel(HELPER)], cwd=ROOT) == HELPER.read_bytes()

for context in (run, attempt):
    assert (context["id"], context["run_attempt"], context["head_sha"]) == (RUN, ATTEMPT, SOURCE)
    assert context["repository"]["full_name"] == "cheshmakzanoon-ops/yamaha-cs80"
assert (job["id"], job["run_id"], job["run_attempt"], job["head_sha"]) == (JOB, RUN, ATTEMPT, SOURCE)
assert (job["status"], job["conclusion"]) == ("completed", "success")
for context in (qualified, original):
    assert (context["source_commit"], context["source_tree"], context["run_id"],
            context["run_attempt"], context["job_id"], context["target"],
            context["factory_revision"]) == (SOURCE, TREE, RUN, ATTEMPT, JOB, "macos-arm64", "2026.11.7")
    assert context["original_run_attempt_bound"] is True
    assert context["artifact_producing_attempts_bound"] is True
assert qualified["platform_qualified"] is True
assert qualified["cross_platform_qualified"] is False
assert qualified["release_qualified"] is False

upload = next(item for item in qualified["uploads"] if item["kind"] == "install")
assert upload == next(item for item in original["uploads"] if item["kind"] == "install")
assert upload["producing_attempt_bound"] is True
assert (artifact["id"], artifact["name"], artifact["size_in_bytes"], artifact["digest"]) == (
    upload["artifact_id"], upload["name"], upload["bytes"], "sha256:" + upload["sha256"])
assert artifact["id"] == 14923244833
assert artifact["name"] == "cs80-macos-15-install-bundle"
assert (artifact["workflow_run"]["id"], artifact["workflow_run"]["head_sha"]) == (RUN, SOURCE)
assert artifact["workflow_run"]["repository_id"] == run["repository"]["id"]
assert artifact["workflow_run"]["head_repository_id"] == run["repository"]["id"]
assert artifact["expired"] is False
expires = stamp(artifact["expires_at"])
assert expires > now and expires > stamp(probe["completed_at"])
assert artifact["expires_at"] == upload["expires_at"]
assert artifact["created_at"] == upload["created_at"]
api_step = next(step for step in job["steps"] if step["number"] == upload["original_api_step"]["number"])
assert api_step == upload["original_api_step"]
assert api_step["conclusion"] == "success" and api_step["status"] == "completed"
for field in ("original_upload_timestamp", "original_digest_timestamp"):
    assert stamp(api_step["started_at"]) <= stamp(upload[field]) < stamp(api_step["completed_at"]) + dt.timedelta(seconds=1)
log_lines = (ARM / "producer-job.log").read_text().splitlines()
upload_lines = [{"line": i, "text": line} for i, line in enumerate(log_lines, 1)
                if str(artifact["id"]) in line or upload["sha256"] in line]
assert any("SHA256 digest of uploaded artifact zip is " + upload["sha256"] in item["text"] for item in upload_lines)
assert any(f"Artifact {artifact['name']} has been successfully uploaded! Final size is {artifact['size_in_bytes']} bytes. Artifact ID is {artifact['id']}" in item["text"] for item in upload_lines)

assert (probe["source"], probe["run_id"], probe["attempt_context"], probe["artifact_id"],
        probe["artifact_name"]) == (SOURCE, RUN, ATTEMPT, artifact["id"], artifact["name"])
assert probe["artifact_api_sha256"] == receipt(ARM / "install-api.json")["sha256"]
assert probe["run_api_sha256"] == receipt(ARM / "run-api-at-platform-completion.json")["sha256"]
assert (probe["actual_bytes"], probe["actual_sha256"], probe["expected_bytes"], probe["expected_sha256"]) == (
    upload["bytes"], upload["sha256"], upload["bytes"], upload["sha256"])
assert probe["method"] == "GET" and probe["credentials_supplied"] is False
assert probe["status"] == 200 and "content-range" not in probe["headers"]
assert probe["public_route_verified"] is True and probe["release_qualified"] is False
assert probe["artifact_attempt_binding_verified"] is False  # Preserve helper's own limited scope.
assert "error" not in probe
assert probe["requested_url"] == f"https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/{artifact['id']}.zip"
for endpoint in [probe["final_endpoint"]] + [item[side] for item in probe["redirects"] for side in ("from", "to")]:
    assert set(endpoint) == {"scheme", "host", "path", "query_redacted"}
    assert endpoint["scheme"] == "https" and "?" not in endpoint["path"]

binding = {
    "schema": 1, "status": "phase1_arm_public_route_bound",
    "verified_at_utc": now.isoformat(), "source_commit": SOURCE, "source_tree": TREE,
    "factory_revision": "2026.11.7", "run_id": RUN, "run_attempt": ATTEMPT, "job_id": JOB,
    "target": "macos-arm64", "artifact_id": artifact["id"], "artifact_name": artifact["name"],
    "authenticated_metadata_file_digests_match_root_retrieval_receipt": True,
    "qualification_sha256_matches_A48_handoff": True,
    "unchanged_probe_matches_frozen_source": True,
    "original_producer_upload_attempt_binding_verified": True,
    "original_upload": upload, "original_upload_log_lines": upload_lines,
    "external_platform_qualification": receipt(ARM / "platform-qualification.json"),
    "input_files": [receipt(ARM / name) for name in (
        "retrieval-receipt.json", "install-api.json", "run-api-at-platform-completion.json",
        "attempt1-api-at-platform-completion.json", "producer-job-api.json", "producer-job.log",
        "original-execution-binding.json")] + [receipt(HELPER), receipt(PROBE)],
    "full_public_get_verified": True, "probe_process_exit_code_observed": 0,
    "public_transport_attempts": [receipt(PROBE)], "failed_public_transports_observed": 0,
    "archive_retained_by_probe": False,
    "expired_boolean": artifact["expired"], "expires_at_is_future_at_probe_and_binding": True,
    "expires_at_utc": artifact["expires_at"],
    "expires_at_America_Halifax": expires.astimezone(ZoneInfo("America/Halifax")).isoformat(),
    "expiry_timezone_abbreviation": expires.astimezone(ZoneInfo("America/Halifax")).tzname(),
    "run_status_at_metadata_snapshot": run["status"],
    "run_conclusion_at_metadata_snapshot": run["conclusion"],
    "release_qualified": False, "all_five_public_downloads_verified": False,
    "scope": "One actual public route observed at the retained timestamps. Original attempt and native/package acceptance are external A48 evidence, checked for identity and integrity here; no product tests were rerun."
}
binding_path = OWN / "phase1-macos-arm64-binding.json"
write_new(binding_path, binding)

mapping = {
    "schema": 1, "status": "partial_one_of_five_public_routes_verified",
    "source_commit": SOURCE, "source_tree": TREE, "factory_revision": "2026.11.7",
    "run_id": RUN, "run_attempt": ATTEMPT, "recorded_at_utc": now.isoformat(),
    "verified_public_route_count": 1, "expected_public_route_count": 5,
    "all_five_public_downloads_verified": False, "release_qualified": False,
    "verified_downloads": [{
        "target": "macos-arm64", "label": "macOS Apple Silicon installation bundle",
        "artifact_id": artifact["id"], "artifact_name": artifact["name"],
        "public_url": probe["requested_url"], "bytes": artifact["size_in_bytes"],
        "sha256": upload["sha256"], "expired": artifact["expired"],
        "expires_at_utc": artifact["expires_at"],
        "expires_at_America_Halifax": binding["expires_at_America_Halifax"],
        "expiry_timezone_abbreviation": "AST",
        "probe_started_at_utc": probe["started_at"], "probe_completed_at_utc": probe["completed_at"],
        "full_unauthenticated_get_verified": True,
        "original_producer_upload_attempt_bound": True,
        "external_platform_qualification": binding["external_platform_qualification"],
        "public_get_receipt": receipt(PROBE), "identity_binding_receipt": receipt(binding_path)
    }],
    "pending_targets": ["windows-x86_64", "macos-x86_64", "linux-x86_64", "factory-audition"],
    "earliest_expiry_over_all_five_utc": None,
    "earliest_expiry_over_all_five_America_Halifax": None,
    "fallback_artifacts_url": f"https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/{RUN}#artifacts",
    "fallback_scope": "Same source/run Artifacts page; not independently fetched in this phase. May require GitHub sign-in. It relies on the same retained artifacts, so cannot recover an expired or deleted artifact.",
    "retention_notice": "The API-reported expiry is a retention limit, not a guaranteed availability window. Deletion, retention changes, and service availability can end access sooner. These are temporary workflow artifacts, not permanent release assets.",
    "public_route_scope": "Successful full retrieval only at each recorded observation time. No later availability, whole release acceptance, publisher signing/notarization, or user listening approval is implied.",
    "final_phase_rule": "Use actual authenticated current-source artifact metadata and original producer/attempt evidence for all four remaining targets, preserve every actual failed GET separately, verify full public byte count/hash, and compute the minimum actual expires_at only once all five are identified."
}
mapping_path = PUBLIC / "download-mapping-phase1.json"
write_new(mapping_path, mapping)

report = f"""# A51 — actual public download integrity and expiry mapping

## Phase 1: Apple Silicon public route passed; four targets pending

The actual macOS Apple Silicon installation artifact **{artifact['id']}** was
retrieved with an unauthenticated, complete HTTP GET from
<{probe['requested_url']}>. The unchanged A57 probe observed **HTTP 200**, no
partial `Content-Range`, **{artifact['size_in_bytes']:,} bytes**, and SHA-256
`{upload['sha256']}`, exactly matching retained authenticated GitHub metadata.
The observation began **{probe['started_at']}** and completed
**{probe['completed_at']}**. The first transport succeeded; there were no failed
public transports in this phase. The stream was hashed without saving a second
large ZIP. Signed redirect query values were not retained.

This is a **one-of-five public-route result**, not an all-downloads or global
release qualification. Windows, Intel Mac, Linux, and factory audition public
checks remain pending actual handoffs. No older-source artifact was substituted.

## Source, attempt, and artifact binding

| Field | Bound value |
| --- | --- |
| Factory revision | `2026.11.7` |
| Source | `{SOURCE}` |
| Source tree | `{TREE}` |
| Run / original attempt | `{RUN}` / `1` |
| Original ARM producer job | `{JOB}`; completed successfully |
| Artifact name | `{artifact['name']}` |
| Artifact ID | `{artifact['id']}` |
| Artifact creation | `{artifact['created_at']}` |
| Original upload | `{upload['original_upload_timestamp']}` |
| A48 platform qualification SHA-256 | `{QUAL_SHA}` |

The retained authenticated artifact, run, attempt, job API, and full original log
hashes match the root retrieval receipt. A51 checked the A48 qualification digest,
its original-execution binding digest, the exact successful original upload step,
and original log ID/name/size/hash lines. The artifact `workflow_run` alone lacks
an attempt field; the original job and upload records supply the attempt binding.
The helper's own `artifact_attempt_binding_verified: false` is deliberately
preserved: it only verifies the public response. The separate A51 binding receipt
composes that result with the original-attempt and external A48 evidence.

The captured whole-run API still says `in_progress` with no conclusion. This
review does not infer a later or aggregate result. Native, validator, package,
and content acceptance are the identified A48 qualification; A51 reran no product
tests and did not reinterpret that qualification as publisher notarization.

## Retention and fallback

The API says `expired: false` and reports **{artifact['expires_at']}**. This was
in the future at both the GET and the binding check. Using
`ZoneInfo("America/Halifax")`, that is **February 22, 2027 at 8:49:05 a.m. AST
(UTC−04:00)**. UTC is **12:49:05** on the same date.

The same-run fallback is the [GitHub Actions Artifacts page](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/{RUN}#artifacts).
It was constructed from the bound run ID and was not independently fetched in
this phase. It may require GitHub sign-in and depends on the same retained
artifacts. It cannot recover an expired or deleted artifact. The API expiry does
not promise continuous availability until that date; deletion, retention changes,
or service availability may end access sooner. These are temporary workflow
artifacts, with no permanent release-asset guarantee.

The final five-download earliest expiry remains **unset** until all five actual
artifact IDs and expiries are available. The ARM expiry is not projected onto
the other four targets.

## Retained evidence and next handoff

- [Actual full-GET receipt](../ci/source-ee0dabca/public-downloads/macos-arm64-full-get-0001.json).
- [Phase 1 public mapping](../ci/source-ee0dabca/public-downloads/download-mapping-phase1.json).
- [Identity, expiry, and original-upload binding](../verification/a51/phase1-macos-arm64-binding.json).
- [Binding verifier](../verification/a51/phase1-macos-arm64-binding.py).
- [External ARM platform qualification](../ci/source-ee0dabca/macos-arm64/platform-qualification.json).

Continue with actual Windows, Intel Mac, Linux, and audition artifact handoffs.
For each, retain every actual failed transport in a fresh output, require the
unauthenticated full GET's complete bytes and SHA-256, bind the exact source/run/
original attempt/name/ID, then compute the minimum actual expiry across all five.
No README, shared guide, old receipt, production source, test, workflow, staging,
commit, or remote state was changed by A51.
"""
write_new(BASE / "reviews/a51.md", report)
print(json.dumps({"binding": receipt(binding_path), "mapping": receipt(mapping_path),
                  "report": receipt(BASE / "reviews/a51.md")}, indent=2))
