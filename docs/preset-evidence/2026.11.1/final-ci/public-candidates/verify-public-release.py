#!/usr/bin/env python3
"""Read-only, credential-free check of the fixed preset release and public bytes.

No artifact is extracted, imported or executed. ZIP bodies are streamed into a
SHA-256 digest and discarded; only small metadata/receipt/checksum files persist.
Exit 3 means the release is not publicly visible yet, never a verification pass.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(__file__).resolve().parent
REPO = "cheshmakzanoon-ops/yamaha-cs80"
SOURCE = "6d3cc220edef5a4a2c7cd968ea8a786c7c83198c"
RUN_ID = 45044040694
ATTEMPT = 1
REVISION = "2026.11.1"
TAG = "presets-" + REVISION
API = "https://api.github.com/repos/" + REPO
PAGE = "https://github.com/" + REPO + "/releases/tag/" + TAG
DOWNLOAD = "https://github.com/" + REPO + "/releases/download/" + TAG + "/"
RUN_URL = "https://github.com/" + REPO + "/actions/runs/" + str(RUN_ID)
INVENTORY_SHA = "fe3ddc6ecdb9fa25e77170f1cd9f81e6f2fe0ef5aabd105506a0fa487eb286fa"
MAX_METADATA = 8 * 1024 * 1024
MAX_ASSET = 512 * 1024 * 1024
PUBLISHER_FILES = {
    ".github/workflows/publish-preset-refinement.yml",
    "scripts/publish-preset-refinement.py",
    "scripts/test-publish-preset-refinement.py",
}
BUNDLES = {
    "cs80-windows-install-bundle": ("cs80-windows-x64.zip", "windows", "x86_64", {"VST3", "Standalone"}),
    "cs80-macos-15-install-bundle": ("cs80-macos-apple-silicon.zip", "macos", "arm64", {"VST3", "AU", "Standalone"}),
    "cs80-macos-15-intel-install-bundle": ("cs80-macos-intel.zip", "macos", "x86_64", {"VST3", "AU", "Standalone"}),
    "cs80-linux-install-bundle": ("cs80-linux-x64.zip", "linux", "x86_64", {"VST3", "Standalone"}),
}
AUDITION_ARTIFACT = "cs80-curated-preset-audition-" + SOURCE
AUDITION_ASSET = "cs80-preset-auditions-" + REVISION + ".zip"
PAYLOADS = {key: value[0] for key, value in BUNDLES.items()} | {AUDITION_ARTIFACT: AUDITION_ASSET}
ASSETS = set(PAYLOADS.values()) | {"release-provenance.json", "SHA256SUMS"}
BASE_STEPS = {"Verify configured JUCE source", "Build", "Test"}
MAC_STEPS = BASE_STEPS | {"Validate VST3 with pluginval", "Validate AU with pluginval", "Validate AU with pluginval GUI", "Package native macOS VST3, AU and Standalone", "Upload native macOS install bundle"}
JOB_STEPS = {
    "Linux / VST3 + Standalone": BASE_STEPS | {"Capture curated preset auditions", "Upload curated preset auditions", "Package Linux VST3 and Standalone", "Upload Linux install bundle"},
    "Windows / VST3 + Standalone": BASE_STEPS | {"Package Windows VST3 and Standalone", "Upload Windows install bundle"},
    "macOS / macos-15 / VST3 + AU + Standalone": MAC_STEPS,
    "macOS / macos-15-intel / VST3 + AU + Standalone": MAC_STEPS,
    "pluginval / Linux VST3 (strictness 10)": {"Validate VST3 headless", "Validate VST3 GUI under Xvfb", "Validate GUI state regression seed 0x2da72db"},
    "pluginval / Windows VST3 (strictness 10)": {"Validate VST3 headless", "Validate state regression seed 0x5037a0c"},
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, minimum=1):
    return type(value) is int and value >= minimum


def json_object(pairs):
    out = {}
    for key, value in pairs:
        require(key not in out, "Duplicate JSON key: " + key)
        out[key] = value
    return out


def parse_json(data):
    return json.loads(data, object_pairs_hook=json_object)


def sha_file(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def digest(value):
    require(isinstance(value, str) and re.fullmatch(r"sha256:[0-9a-f]{64}", value), "Missing or invalid API SHA-256")
    return value[7:]


def safe_url(url):
    parts = urllib.parse.urlsplit(url)
    require(parts.scheme == "https" and not parts.username and not parts.password and parts.port in (None, 443), "Unexpected public-download URL")
    host = parts.hostname or ""
    require(host in {"github.com", "api.github.com"} or host.endswith(".githubusercontent.com"), "Unexpected GitHub download host")
    return host


class PublicRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, newurl):
        require(request.get_method() == "GET", "Only public GETs are permitted")
        safe_url(newurl)
        return super().redirect_request(request, fp, code, message, headers, newurl)


class PublicClient:
    def __init__(self, output):
        self.output = output
        self.opener = urllib.request.build_opener(PublicRedirect())
        self.gets = []

    def open(self, url, api=False):
        safe_url(url)
        headers = {"User-Agent": "CS80-public-release-byte-verification/1.0"}
        if api:
            require(url.startswith(API + "/"), "API read outside the fixed repository")
            headers.update({"Accept": "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28"})
        # No GitHub token, Authorization header or cookie is supplied.
        request = urllib.request.Request(url, headers=headers, method="GET")
        response = self.opener.open(request, timeout=45)
        require(response.status == 200, "Public GET did not return 200")
        final_host = safe_url(response.geturl())
        self.gets.append({"url": url, "status": response.status, "final_host": final_host, "credential_free": True})
        return response

    def json(self, path, stem):
        with self.open(API + path, api=True) as response:
            data = response.read(MAX_METADATA + 1)
        require(len(data) <= MAX_METADATA, "Oversized API metadata")
        value = parse_json(data)
        (self.output / (stem + ".json")).write_bytes(data)
        return value

    def pages(self, path, stem, key=None):
        result = []
        for page in range(1, 11):
            separator = "&" if "?" in path else "?"
            value = self.json(path + separator + f"per_page=100&page={page}", f"{stem}-page-{page}")
            items = value[key] if key else value
            require(isinstance(items, list), "Expected API collection")
            result.extend(items)
            if len(items) < 100:
                return result
        raise ValueError("API collection exceeds the bounded page limit")

    def asset(self, asset, capture=False):
        name = asset["name"]
        expected_size, expected_sha = asset["size"], digest(asset["digest"])
        require(asset["browser_download_url"] == DOWNLOAD + name, "Release asset URL differs from the fixed public URL")
        require(integer(expected_size) and expected_size <= MAX_ASSET, "Invalid release asset size")
        require(not capture or expected_size <= MAX_METADATA, "Receipt/checksum asset too large")
        value, size, first = hashlib.sha256(), 0, b""
        captured = bytearray() if capture else None
        with self.open(asset["browser_download_url"]) as response:
            length = response.headers.get("Content-Length")
            if length is not None:
                require(length.isdecimal() and int(length) == expected_size, "Response Content-Length differs from API")
            while chunk := response.read(1024 * 1024):
                size += len(chunk)
                require(size <= expected_size, "Public response exceeds API size")
                if len(first) < 4:
                    first = (first + chunk)[:4]
                value.update(chunk)
                if captured is not None:
                    captured.extend(chunk)
        require(size == expected_size and value.hexdigest() == expected_sha, "Public bytes differ from release API digest or size: " + name)
        if name.endswith(".zip"):
            require(first == b"PK\x03\x04", "Expected a ZIP payload")
        if captured is not None:
            (self.output / name).write_bytes(captured)
        record = {"name": name, "asset_id": asset["id"], "url": asset["browser_download_url"], "bytes": size, "sha256": value.hexdigest(), "credential_free_get": True, "body_streamed_without_persisting": not capture}
        return record, bytes(captured) if captured is not None else None


def validate_build(run, jobs):
    require(run.get("id") == RUN_ID and run.get("head_sha") == SOURCE, "Unexpected binary source or build run")
    require(type(run.get("run_attempt")) is int and run["run_attempt"] == ATTEMPT, "Build is not the first attempt")
    require(run.get("name") == "build" and run.get("path") == ".github/workflows/build.yml", "Unexpected workflow")
    require(run.get("head_branch") == "main" and run.get("event") == "push", "Build is not the authorized main push")
    require(run.get("head_repository", {}).get("full_name") == REPO and run.get("html_url") == RUN_URL, "Unexpected build repository or URL")
    require(run.get("status") == "completed" and run.get("conclusion") == "success", "Build did not complete successfully")
    require(len(jobs) == 6 and {j.get("name") for j in jobs} == set(JOB_STEPS), "Missing, duplicated or unexpected required job")
    records = []
    for job in jobs:
        require(integer(job.get("id")) and job.get("run_id") == RUN_ID and job.get("head_sha") == SOURCE, "Wrong job identity")
        require(type(job.get("run_attempt")) is int and job["run_attempt"] == ATTEMPT, "Job is not the first attempt")
        require(job.get("status") == "completed" and job.get("conclusion") == "success", "Job did not succeed: " + job["name"])
        steps = job.get("steps", [])
        by_name = {step["name"]: step for step in steps}
        require(len(by_name) == len(steps), "Duplicate step names")
        for name in JOB_STEPS[job["name"]]:
            require(by_name.get(name, {}).get("status") == "completed" and by_name[name].get("conclusion") == "success", "Required step failed or skipped: " + name)
        records.append({"id": job["id"], "name": job["name"], "run_attempt": ATTEMPT, "conclusion": "success", "required_steps": sorted(JOB_STEPS[job["name"]])})
    return sorted(records, key=lambda item: item["name"])


def allowed_path(path):
    return path == "README.md" or path.startswith("docs/") or path in PUBLISHER_FILES


def validate_comparison(comparison):
    require(comparison.get("merge_base_commit", {}).get("sha") == SOURCE and comparison.get("status") in {"ahead", "identical"}, "Publication is not descended from the binary source")
    files = comparison.get("files")
    require(isinstance(files, list) and len(files) < 300, "Missing or possibly truncated source comparison")
    require(len({item["filename"] for item in files}) == len(files), "Duplicate comparison file")
    for item in files:
        require(allowed_path(item["filename"]), "Build-affecting publication difference: " + item["filename"])
        if "previous_filename" in item:
            require(allowed_path(item["previous_filename"]), "Build-affecting source renamed into allowed path")
    return [{"path": item["filename"], "status": item["status"]} for item in files]


def checksum_map(data):
    result = {}
    for line in data.decode("utf-8").splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9_.-]+)", line)
        require(match is not None and match[2] not in result, "Malformed or duplicate release checksum entry")
        result[match[2]] = match[1]
    require(set(result) == ASSETS - {"SHA256SUMS"}, "Release checksums must cover exactly the six other assets")
    return result


def verify(output, cache):
    client = PublicClient(output)
    try:
        release = client.json("/releases/tags/" + TAG, "release-api")
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return {"assignment": 56, "status": "PENDING_RELEASE_NOT_PUBLICLY_VISIBLE", "checked_at_utc": datetime.now(timezone.utc).isoformat(), "release_url": PAGE, "observed_public_http_status": 404, "source": SOURCE, "run_id": RUN_ID, "run_attempt_required": ATTEMPT, "qualification": "No release assets, build gates or public download bytes are certified by this availability check."}, 3
        raise
    require(release.get("draft") is False and release.get("prerelease") is True and release.get("tag_name") == TAG, "Expected a public development prerelease")
    require(release.get("html_url") == PAGE and release.get("published_at") and integer(release.get("id")), "Invalid public release identity")
    publication = release.get("target_commitish", "")
    require(re.fullmatch(r"[0-9a-f]{40}", publication), "Release target is not an immutable commit")
    assets = client.pages(f'/releases/{release["id"]}/assets', "release-assets")
    require(len(assets) == 7 and {a.get("name") for a in assets} == ASSETS, "Unexpected, duplicate or missing public asset")
    require(len({a.get("id") for a in assets}) == 7, "Duplicate asset IDs")
    by_name = {a["name"]: a for a in assets}
    for asset in assets:
        require(integer(asset.get("id")) and asset.get("state") == "uploaded", "Asset is not completely uploaded")
        digest(asset.get("digest"))
    receipt_record, receipt_bytes = client.asset(by_name["release-provenance.json"], capture=True)
    sums_record, sums_bytes = client.asset(by_name["SHA256SUMS"], capture=True)
    receipt, checksums = parse_json(receipt_bytes), checksum_map(sums_bytes)
    for name, expected_sha in checksums.items():
        require(expected_sha == digest(by_name[name].get("digest")), "Checksum manifest differs from release API: " + name)
    require(type(receipt.get("schema")) is int and receipt["schema"] == 1 and receipt.get("preset_library_revision") == REVISION, "Unexpected receipt schema/revision")
    require(receipt.get("binary_source_commit") == SOURCE and receipt.get("build_run_id") == RUN_ID and type(receipt.get("build_run_attempt")) is int and receipt["build_run_attempt"] == ATTEMPT, "Receipt build identity mismatch")
    require(receipt.get("publication_commit") == publication and receipt.get("tag") == TAG and receipt.get("build_run_url") == RUN_URL, "Receipt publication identity mismatch")
    run = client.json(f"/actions/runs/{RUN_ID}", "build-run-api")
    jobs = client.pages(f"/actions/runs/{RUN_ID}/jobs?filter=latest", "build-jobs", "jobs")
    job_records = validate_build(run, jobs)
    require(receipt.get("required_jobs") == job_records, "Receipt job evidence differs from actual API jobs")
    tag = client.json("/git/ref/tags/" + TAG, "tag-api")
    require(tag.get("ref") == "refs/tags/" + TAG and tag.get("object", {}).get("type") == "commit" and tag["object"].get("sha") == publication, "Release tag differs from its publication commit")
    comparison = client.json(f"/compare/{SOURCE}...{publication}", "source-comparison-api")
    comparison_records = validate_comparison(comparison)
    require(receipt.get("publication_only_differences") == comparison_records, "Receipt source comparison differs from API")
    artifacts = client.pages(f"/actions/runs/{RUN_ID}/artifacts", "build-artifacts", "artifacts")
    selected = [a for a in artifacts if a.get("name") in PAYLOADS]
    require(len(selected) == 5 and {a["name"] for a in selected} == set(PAYLOADS), "Missing or duplicated source build artifact")
    artifact_map = {a["name"]: a for a in selected}
    records = receipt.get("artifacts", [])
    require(len(records) == 5 and {r.get("artifact_name") for r in records} == set(PAYLOADS), "Receipt does not identify exactly five build payloads")
    require(len({r.get("artifact_id") for r in records}) == 5 and {r.get("release_asset") for r in records} == set(PAYLOADS.values()), "Duplicate or wrong receipt payload mapping")
    artifact_records = []
    for record in records:
        name = record["artifact_name"]
        artifact = artifact_map[name]
        asset_name = PAYLOADS[name]
        asset = by_name[asset_name]
        workflow = artifact.get("workflow_run", {})
        require(integer(artifact.get("id")) and artifact.get("expired") is False, "Expired or invalid build artifact")
        require(workflow.get("id") == RUN_ID and workflow.get("head_sha") == SOURCE and workflow.get("head_branch") == "main", "Build artifact provenance mismatch")
        expected_sha = digest(artifact.get("digest"))
        require(integer(artifact.get("size_in_bytes")) and artifact["size_in_bytes"] <= MAX_ASSET, "Invalid build artifact size")
        require(record.get("artifact_id") == artifact["id"] and record.get("release_asset") == asset_name and record.get("sha256") == expected_sha and record.get("bytes") == artifact["size_in_bytes"], "Receipt artifact differs from API metadata")
        require(digest(asset.get("digest")) == expected_sha and asset.get("size") == artifact["size_in_bytes"], "Release ZIP differs from source build artifact")
        details = record.get("verification", {})
        if name in BUNDLES:
            _, system, arch, formats = BUNDLES[name]
            require(details.get("platform") == system and details.get("architecture") == arch and set(details.get("formats", [])) == formats, "Receipt package formats/architecture mismatch")
            require(re.fullmatch(r"[0-9a-f]{64}", details.get("manifest_sha256", "")) and integer(details.get("inventory_entries")), "Receipt lacks package inventory identity")
        else:
            require(details == {"programs": 24, "seconds": 117, "inventory_sha256": INVENTORY_SHA, "checksums": 32}, "Receipt audition contract mismatch")
        cached = cache / (name + "-" + str(artifact["id"]) + ".zip")
        cache_record = {"present": cached.exists()}
        if cached.exists():
            require(cached.is_file() and not cached.is_symlink() and cached.stat().st_size == artifact["size_in_bytes"] and sha_file(cached) == expected_sha, "Shared artifact cache differs from API bytes")
            cache_record.update({"path": str(cached), "bytes": cached.stat().st_size, "sha256": expected_sha})
        artifact_records.append({"artifact_id": artifact["id"], "artifact_name": name, "release_asset": asset_name, "sha256": expected_sha, "bytes": artifact["size_in_bytes"], "cached_build_archive": cache_record})
    downloads = [receipt_record, sums_record]
    for name in sorted(PAYLOADS.values()):
        record, _ = client.asset(by_name[name])
        require(record["sha256"] == checksums[name], "Public ZIP digest differs from checksum manifest")
        downloads.append(record)
    require(receipt_record["sha256"] == checksums["release-provenance.json"], "Public receipt digest differs from checksum manifest")
    # Refetch final release/asset/tag identities to detect replacement during reads.
    final_release = client.json(f'/releases/{release["id"]}', "release-final-api")
    final_assets = client.pages(f'/releases/{release["id"]}/assets', "release-assets-final")
    final_tag = client.json("/git/ref/tags/" + TAG, "tag-final-api")
    require(all(final_release.get(key) == release.get(key) for key in ("id", "tag_name", "target_commitish", "draft", "prerelease", "published_at")), "Release identity changed during verification")
    fields = ("id", "name", "state", "size", "digest", "browser_download_url")
    simplify = lambda rows: sorted([tuple(row.get(key) for key in fields) for row in rows])
    require(simplify(final_assets) == simplify(assets) and final_tag == tag, "Assets or tag changed during verification")
    return {"assignment": 56, "status": "PASS_PUBLIC_RELEASE_BYTES_AND_PROVENANCE", "verified_at_utc": datetime.now(timezone.utc).isoformat(), "source": SOURCE, "run_id": RUN_ID, "run_attempt": ATTEMPT, "publication_commit": publication, "release_id": release["id"], "release_url": PAGE, "prerelease": True, "release_assets": sorted(downloads, key=lambda item: item["name"]), "source_build_artifacts": sorted(artifact_records, key=lambda item: item["artifact_name"]), "required_jobs": job_records, "publication_only_differences": comparison_records, "release_checksum_entries": checksums, "public_gets": client.gets, "verifier_sha256": sha_file(Path(__file__)), "reviewed_publisher_sha256": sha_file(ROOT / "scripts/publish-preset-refinement.py"), "qualification": "Seven actual public asset GETs matched release API digests/sizes; five ZIPs also matched exact first-attempt build artifacts and the receipt. Source/tag, six required jobs and named successful steps were checked from live API metadata. Native execution, detailed artifact content, raw validator exits and audition sample checks belong to separate reviewers; this verifier does not execute or extract products or establish signing, all-DAW compatibility, acoustic equivalence or a listening verdict."}, 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=WORK / "observed")
    parser.add_argument("--cache-dir", type=Path, default=ROOT / "build-coordination/artifact-cache")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    try:
        result, code = verify(args.output_dir, args.cache_dir)
    except Exception as error:
        # Do not persist signed redirect URLs or response headers in error text.
        if isinstance(error, urllib.error.HTTPError):
            message = f"Public GitHub GET returned HTTP {error.code}"
        elif isinstance(error, urllib.error.URLError):
            message = "Public GitHub GET network failure: " + type(error.reason).__name__
        else:
            message = str(error)
        result = {"assignment": 56, "status": "FAIL_OR_UNAVAILABLE", "checked_at_utc": datetime.now(timezone.utc).isoformat(), "source": SOURCE, "run_id": RUN_ID, "error_type": type(error).__name__, "error": message, "qualification": "No passing public release qualification is established by this result."}
        code = 1
    (args.output_dir / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: result[key] for key in ("assignment", "status", "source", "run_id", "error") if key in result}, indent=2))
    return code


if __name__ == "__main__":
    sys.exit(main())
