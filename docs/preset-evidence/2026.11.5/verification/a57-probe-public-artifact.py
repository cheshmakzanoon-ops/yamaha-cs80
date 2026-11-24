#!/usr/bin/env python3
"""Read and hash one public nightly.link artifact. No credentials or execution.

The metadata files must be acquired independently from authenticated GitHub.
This helper checks their internal context, not their authenticity or a producing
attempt. A matching public body proves public byte retrieval at the observation
time; it does not qualify native tests, payload contents or an entire release.
"""
import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

REPOSITORY = "cheshmakzanoon-ops/yamaha-cs80"
MAX_ARTIFACT_BYTES = 512 * 1024 * 1024


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load_json(path):
    path = Path(path)
    require(path.is_file() and not path.is_symlink(), "Metadata must be a regular nonsymlink file")
    data = path.read_bytes()
    require(len(data) <= 4 * 1024 * 1024, "Metadata exceeds bound")
    def unique(pairs):
        d = {}
        for k, v in pairs:
            require(k not in d, "Duplicate JSON key")
            d[k] = v
        return d
    return json.loads(data, object_pairs_hook=unique), hashlib.sha256(data).hexdigest()


def endpoint(url):
    u = urllib.parse.urlsplit(url)
    return {"scheme": u.scheme, "host": u.hostname, "path": u.path,
            "query_redacted": bool(u.query)}


class HttpsRedirects(urllib.request.HTTPRedirectHandler):
    def __init__(self):
        super().__init__()
        self.observations = []

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        u = urllib.parse.urlsplit(newurl)
        require(u.scheme == "https" and u.username is None and u.password is None,
                "Unexpected non-HTTPS or credential-bearing redirect")
        self.observations.append({"status": code, "from": endpoint(req.full_url),
                                  "to": endpoint(newurl)})
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--artifact-api", required=True)
    p.add_argument("--run-api", required=True)
    p.add_argument("--source", required=True)
    p.add_argument("--run-id", type=int, required=True)
    p.add_argument("--attempt", type=int, required=True)
    p.add_argument("--artifact-name", required=True)
    p.add_argument("--out", required=True)
    args = p.parse_args()
    out = Path(args.out)
    require(not out.exists() and not out.is_symlink(), "Output must be fresh")
    require(re.fullmatch(r"[0-9a-f]{40}", args.source), "Expected full source SHA")
    require(args.run_id > 0 and args.attempt > 0, "Invalid run context")
    artifact, artifact_sha = load_json(args.artifact_api)
    run, run_sha = load_json(args.run_api)
    require(run["id"] == args.run_id and run["run_attempt"] == args.attempt,
            "Run ID/attempt mismatch")
    require(run["head_sha"] == args.source and run["repository"]["full_name"] == REPOSITORY,
            "Run repository/source mismatch")
    require(type(artifact["id"]) is int and artifact["id"] > 0, "Invalid artifact ID")
    artifact_id = artifact["id"]
    require(artifact["name"] == args.artifact_name, "Artifact name mismatch")
    require(artifact["expired"] is False, "Artifact reported expired")
    workflow = artifact["workflow_run"]
    require(workflow["id"] == args.run_id and workflow["head_sha"] == args.source,
            "Artifact source/run mismatch")
    require(workflow["repository_id"] == run["repository"]["id"]
            and workflow["head_repository_id"] == run["repository"]["id"],
            "Artifact repository IDs mismatch")
    api = f"https://api.github.com/repos/{REPOSITORY}/actions/artifacts/{artifact_id}"
    require(artifact["url"] == api and artifact["archive_download_url"] == api + "/zip",
            "Artifact API URL mismatch")
    expected_size = artifact["size_in_bytes"]
    require(type(expected_size) is int and 0 < expected_size <= MAX_ARTIFACT_BYTES,
            "Artifact size outside bound")
    digest = artifact["digest"]
    require(isinstance(digest, str) and re.fullmatch(r"sha256:[0-9a-f]{64}", digest),
            "Missing or invalid API digest")
    expires = dt.datetime.fromisoformat(artifact["expires_at"].replace("Z", "+00:00"))
    require(expires.tzinfo is not None and expires > dt.datetime.now(dt.timezone.utc),
            "Artifact expiry has passed or lacks timezone")
    url = f"https://nightly.link/{REPOSITORY}/actions/artifacts/{artifact_id}.zip"
    record = {
        "repository": REPOSITORY, "source": args.source, "run_id": args.run_id,
        "attempt_context": args.attempt, "artifact_id": artifact_id,
        "artifact_name": artifact["name"], "expected_bytes": expected_size,
        "expected_sha256": digest[7:], "expires_at": artifact["expires_at"],
        "artifact_api_sha256": artifact_sha, "run_api_sha256": run_sha,
        "requested_url": url, "method": "GET", "credentials_supplied": False,
        "started_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "run_status_observed": run["status"], "run_conclusion_observed": run["conclusion"],
        "artifact_attempt_binding_verified": False,
        "public_route_verified": False, "release_qualified": False,
        "scope": "Actual public body hash only; authentic metadata and original upload/attempt, content/native qualification remain external gates."
    }
    handler = HttpsRedirects()
    opener = urllib.request.build_opener(handler)
    status = 1
    try:
        request = urllib.request.Request(url, headers={
            "User-Agent": "CS80-public-route-review/1.0", "Accept": "application/zip"})
        with opener.open(request, timeout=30) as response:
            record["status"] = response.status
            record["final_endpoint"] = endpoint(response.url)
            record["headers"] = {k.lower(): v for k, v in response.headers.items()
                if k.lower() in {"content-type", "content-length", "content-disposition",
                                  "content-range", "last-modified", "etag", "server"}}
            require(response.status == 200, "Full GET must return HTTP200")
            require("content-range" not in record["headers"], "Unexpected partial body")
            if "content-length" in record["headers"]:
                require(int(record["headers"]["content-length"]) == expected_size,
                        "Content-Length differs from artifact API")
            h = hashlib.sha256()
            count = 0
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                count += len(chunk)
                require(count <= expected_size, "Response exceeds expected artifact size")
                h.update(chunk)
            record["actual_bytes"] = count
            record["actual_sha256"] = h.hexdigest()
            require(count == expected_size and h.hexdigest() == digest[7:],
                    "Public response size/hash differs from authenticated artifact metadata")
            record["public_route_verified"] = True
            status = 0
    except urllib.error.HTTPError as exc:
        record["error"] = {"type": type(exc).__name__, "status": exc.code,
                            "reason": str(exc.reason)}
    except Exception as exc:
        # Exception text may contain signed URLs; retain only a controlled error
        # class for network failures, and our own bounded validation messages.
        record["error"] = {"type": type(exc).__name__}
        if isinstance(exc, ValueError):
            record["error"]["reason"] = str(exc)
    record["redirects"] = handler.observations
    record["completed_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("x") as f:
        json.dump(record, f, indent=2)
        f.write("\n")
    print(json.dumps({"artifact_id": artifact_id,
        "public_route_verified": record["public_route_verified"], "receipt": str(out)}))
    return status


if __name__ == "__main__":
    sys.exit(main())
