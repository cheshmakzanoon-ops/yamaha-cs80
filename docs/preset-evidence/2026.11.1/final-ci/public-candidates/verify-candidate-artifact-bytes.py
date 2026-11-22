#!/usr/bin/env python3
"""Verify public candidate artifact bytes while preserving failed release status.

This is explicitly separate from verify-public-release.py. It does not publish,
retry CI, execute products, or turn a failed build into a qualified release.
"""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(__file__).resolve().parent
OUT = WORK / "candidate-verified"
CACHE = ROOT / "build-coordination/artifact-cache"
REPO = "cheshmakzanoon-ops/yamaha-cs80"
SOURCE = "6d3cc220edef5a4a2c7cd968ea8a786c7c83198c"
RUN = 45044040694
ATTEMPT = 1
PUBLISHER_RUN = 45060638607
FAILED_JOB = 137324977642
TAG = "presets-2026.11.1"
API = "https://api.github.com/repos/" + REPO
STRICT_VERIFIER_SHA = "d59337b09ab4739036c22b85458f65cdf9343f4c9298c836b07286823140179a"
ARTIFACTS = [
    ("cs80-windows-install-bundle", 14763872941, 6261285, "aa472ff6516371482b7e73f76dc2fbc58d15b52aa8eb86c91f62e091dac06ec8"),
    ("cs80-macos-15-install-bundle", 14763761497, 14900540, "662f66e20a8658374b3d4d30102b698234e01bb99e3753a49206a04f99b201a7"),
    ("cs80-macos-15-intel-install-bundle", 14763644216, 16057766, "325b42beb55ca861b65e9f8416ec286a4b6061946181210fdb41d041e7d3cf1f"),
    ("cs80-linux-install-bundle", 14768807632, 13225022, "4295d202bacb20ab41e7a776290dac51cf4a7458abc66d185a86bc351efd4d30"),
    ("cs80-curated-preset-audition-" + SOURCE, 14769036590, 41014591, "9b164379e214d8a15e6907d94dd48842628c09d5970579386818596a0488e16b"),
]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha_file(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def json_object(pairs):
    value = {}
    for key, item in pairs:
        require(key not in value, "Duplicate JSON field")
        value[key] = item
    return value


class HTTPSRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, url):
        parsed = urllib.parse.urlsplit(url)
        require(request.get_method() == "GET" and parsed.scheme == "https" and not parsed.username and not parsed.password, "Unexpected public GET redirect")
        return super().redirect_request(request, fp, code, message, headers, url)


def request(url, api=False):
    require(url.startswith(API + "/") if api else url.startswith("https://nightly.link/" + REPO + "/actions/artifacts/"), "Unexpected initial public URL")
    headers = {"User-Agent": "CS80-unqualified-candidate-byte-verification/1.0"}
    if api:
        headers.update({"Accept": "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28"})
    # No GitHub token, Authorization header, cookie or POST request is used.
    return urllib.request.build_opener(HTTPSRedirect()).open(urllib.request.Request(url, headers=headers, method="GET"), timeout=45)


def api_read(item):
    name, path = item
    url = API + path
    observed = datetime.now(timezone.utc).isoformat()
    try:
        with request(url, api=True) as response:
            require(response.status == 200, "Unexpected API HTTP status")
            data = response.read(8 * 1024 * 1024 + 1)
        require(len(data) <= 8 * 1024 * 1024, "Oversized API metadata")
        parsed = json.loads(data, object_pairs_hook=json_object)
        file = OUT / (name + "-api.json")
        file.write_bytes(data)
        return name, {"http_status": 200, "observed_at_utc": observed, "url": url, "file": file.name, "sha256": hashlib.sha256(data).hexdigest(), "data": parsed}
    except urllib.error.HTTPError as error:
        require(name == "release" and error.code == 404, f"Public API HTTP {error.code}: {name}")
        record = {"http_status": 404, "observed_at_utc": observed, "url": url}
        (OUT / (name + "-http.json")).write_text(json.dumps(record, indent=2) + "\n")
        return name, record


def verify_payload(item, metadata):
    name, artifact_id, expected_size, expected_sha = item
    artifact = metadata[f"artifact-{artifact_id}"]["data"]
    require(artifact.get("id") == artifact_id and artifact.get("name") == name, "Wrong artifact identity")
    require(artifact.get("digest") == "sha256:" + expected_sha and artifact.get("size_in_bytes") == expected_size and artifact.get("expired") is False, "Artifact digest, size or expiry mismatch")
    run = artifact.get("workflow_run", {})
    require(run.get("id") == RUN and run.get("head_sha") == SOURCE and run.get("head_branch") == "main", "Artifact source/run mismatch")
    expires = artifact.get("expires_at", "")
    require(datetime.fromisoformat(expires.replace("Z", "+00:00")) > datetime.now(timezone.utc), "Artifact has expired")
    cached = CACHE / (name + "-" + str(artifact_id) + ".zip")
    require(cached.is_file() and not cached.is_symlink(), "Expected shared artifact cache is missing")
    require(cached.stat().st_size == expected_size and sha_file(cached) == expected_sha, "Shared cache digest/size mismatch")
    url = "https://nightly.link/" + REPO + "/actions/artifacts/" + str(artifact_id) + ".zip"
    value, received, first = hashlib.sha256(), 0, b""
    started = datetime.now(timezone.utc).isoformat()
    with request(url) as response:
        require(response.status == 200, "Public artifact GET did not return HTTP 200")
        final_host = urllib.parse.urlsplit(response.geturl()).hostname
        length = response.headers.get("Content-Length")
        if length is not None:
            require(length.isdecimal() and int(length) == expected_size, "HTTP length differs from API")
        while chunk := response.read(1024 * 1024):
            received += len(chunk)
            require(received <= expected_size, "Public artifact exceeds API size")
            if len(first) < 4:
                first = (first + chunk)[:4]
            value.update(chunk)
    require(received == expected_size and value.hexdigest() == expected_sha and first == b"PK\x03\x04", "Public artifact bytes differ from API/cache")
    result = {"artifact_id": artifact_id, "artifact_name": name, "url": url, "credential_free_get": True, "http_status": 200, "final_host": final_host, "get_started_at_utc": started, "get_completed_at_utc": datetime.now(timezone.utc).isoformat(), "bytes": received, "sha256": value.hexdigest(), "expires_at": expires, "api_expired": False, "source_commit": SOURCE, "workflow_run_id": RUN, "build_run_attempt": ATTEMPT, "cached_file": str(cached.relative_to(ROOT)), "cached_file_bytes_and_sha256_match": True, "response_streamed_without_saving_duplicate": True, "release_qualified": False}
    (OUT / f"artifact-{artifact_id}-proof.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"artifact_id": artifact_id, "candidate_bytes": "VERIFIED", "release_qualified": False}), flush=True)
    return result


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    require(sha_file(WORK / "verify-public-release.py") == STRICT_VERIFIER_SHA, "Original strict release verifier changed")
    requests = {
        "run": f"/actions/runs/{RUN}",
        "release": "/releases/tags/" + TAG,
        "publisher-run": f"/actions/runs/{PUBLISHER_RUN}",
        "publisher-jobs": f"/actions/runs/{PUBLISHER_RUN}/jobs?per_page=100",
        "failed-linux-validator-job": f"/actions/jobs/{FAILED_JOB}",
    }
    requests.update({f"artifact-{item[1]}": f"/actions/artifacts/{item[1]}" for item in ARTIFACTS})
    with ThreadPoolExecutor(max_workers=4) as pool:
        metadata = dict(pool.map(api_read, requests.items()))
    run = metadata["run"]["data"]
    require(run.get("id") == RUN and run.get("head_sha") == SOURCE and type(run.get("run_attempt")) is int and run["run_attempt"] == ATTEMPT, "Wrong first-attempt build identity")
    require(run.get("head_repository", {}).get("full_name") == REPO and run.get("head_branch") == "main" and run.get("event") == "push", "Wrong main-push repository")
    require(run.get("name") == "build" and run.get("path") == ".github/workflows/build.yml", "Wrong build workflow")
    require(run.get("status") == "completed" and run.get("conclusion") == "failure", "This bounded fallback requires the recorded failed build outcome")
    require(metadata["release"]["http_status"] == 404, "A public release is now visible; review the changed outcome separately")
    publisher = metadata["publisher-run"]["data"]
    require(publisher.get("id") == PUBLISHER_RUN and publisher.get("path") == ".github/workflows/publish-preset-refinement.yml" and publisher.get("event") == "workflow_run", "Wrong publisher workflow identity")
    require(publisher.get("status") == "completed" and publisher.get("conclusion") == "skipped", "Publisher is not skipped as expected")
    publisher_jobs = metadata["publisher-jobs"]["data"]["jobs"]
    require(len(publisher_jobs) == 1 and publisher_jobs[0].get("name") == "publish" and publisher_jobs[0].get("conclusion") == "skipped", "Unexpected publisher job outcome")
    failed = metadata["failed-linux-validator-job"]["data"]
    require(failed.get("id") == FAILED_JOB and failed.get("run_id") == RUN and failed.get("head_sha") == SOURCE and failed.get("run_attempt") == ATTEMPT and failed.get("conclusion") == "failure", "Wrong failed validator job")
    failed_step = next(s for s in failed.get("steps", []) if s["name"] == "Validate VST3 GUI under Xvfb")
    require(failed_step.get("conclusion") == "failure", "Expected GUI failure is absent")
    raw_proof_path = ROOT / "build-w12-linux/ci-proof.json"
    raw_proof = json.loads(raw_proof_path.read_bytes(), object_pairs_hook=json_object)
    require(raw_proof.get("run_id") == RUN and raw_proof.get("source_commit") == SOURCE and raw_proof.get("run_attempt") == ATTEMPT and raw_proof.get("status") == "FAIL", "Wrong preserved Linux gate proof")
    gui = next(lane for lane in raw_proof["lanes"] if lane["lane"] == "gui")
    core = ROOT / "build-w12-linux/validator-reports/pluginval-gui-core.log"
    require(sha_file(core) == gui["core_identity"]["sha256"], "Preserved GUI termination log hash differs")
    require(gui.get("random_seed") == "0x4369262" and gui["original_terminations"][0]["raw_returncode"] == -11 and gui.get("normalized_wrapper_exit") == 139, "Wrong preserved GUI failure signature")
    require("raw returncode=-11" in core.read_text() and gui.get("success_markers") == 1, "Preserved failure evidence does not match reported post-SUCCESS crash")
    with ThreadPoolExecutor(max_workers=3) as pool:
        payloads = list(pool.map(lambda item: verify_payload(item, metadata), ARTIFACTS))
    require(sha_file(WORK / "verify-public-release.py") == STRICT_VERIFIER_SHA, "Strict release verifier changed during candidate checks")
    result = {
        "assignment": 56,
        "status": "CANDIDATE_BYTES_VERIFIED_RELEASE_UNQUALIFIED",
        "verified_at_utc": datetime.now(timezone.utc).isoformat(),
        "binary_source_commit": SOURCE,
        "build_run_id": RUN,
        "build_run_attempt": ATTEMPT,
        "build_conclusion": "failure",
        "release_qualified": False,
        "public_release_tag": TAG,
        "public_release_tag_http_status": 404,
        "publisher_run": {key: publisher.get(key) for key in ["id", "name", "path", "html_url", "head_sha", "event", "status", "conclusion", "created_at", "updated_at"]},
        "publisher_jobs": [{key: job.get(key) for key in ["id", "name", "status", "conclusion"]} for job in publisher_jobs],
        "release_blocker": {"validator_job_id": FAILED_JOB, "step": failed_step, "seed": gui["random_seed"], "raw_returncode": -11, "normalized_wrapper_exit": 139, "success_markers": gui["success_markers"], "raw_gate_proof_path": str(raw_proof_path.relative_to(ROOT)), "raw_gate_proof_sha256": sha_file(raw_proof_path), "termination_log_path": str(core.relative_to(ROOT)), "termination_log_sha256": sha_file(core)},
        "public_candidate_artifacts": payloads,
        "metadata_reads": {key: {k: v for k, v in item.items() if k != "data"} for key, item in metadata.items()},
        "original_strict_release_verifier_sha256": STRICT_VERIFIER_SHA,
        "candidate_verifier_sha256": sha_file(Path(__file__)),
        "qualification": "Five actual pinned public artifact GETs matched fresh API metadata and cached CI artifact bytes. They remain candidate downloads from a FAILED first-attempt build; no combined release, seven release assets, successful overall CI, fix for the validator crash, signing, universal DAW support or listening verdict is claimed. The public prerelease is absent and the automatic publisher was skipped. No CI rerun, publication, product execution, extraction, duplicate download storage or production edit occurred.",
    }
    (OUT / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"assignment": 56, "status": result["status"], "verified_candidate_gets": len(payloads), "release_qualified": False}), flush=True)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        OUT.mkdir(parents=True, exist_ok=True)
        message = f"HTTP {error.code}" if isinstance(error, urllib.error.HTTPError) else "Network GET failed: " + type(error.reason).__name__ if isinstance(error, urllib.error.URLError) else str(error)
        failure = {"assignment": 56, "status": "CANDIDATE_CHECK_FAILED_RELEASE_UNQUALIFIED", "release_qualified": False, "observed_at_utc": datetime.now(timezone.utc).isoformat(), "error_type": type(error).__name__, "error": message}
        (OUT / "failure.json").write_text(json.dumps(failure, indent=2) + "\n")
        print(json.dumps(failure), flush=True)
        raise SystemExit(1)
