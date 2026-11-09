#!/usr/bin/env python3
"""Fetch the pinned official Windows validator; never run or retry validation."""
import argparse
import hashlib
import http.client
import json
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

VERSION = "1.0.4"
RELEASE_ID = 188928656
ASSET_ID = 210999198
EXPECTED_SIZE = 2408590
EXPECTED_SHA256 = "c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab"
URLS = (
    f"https://api.github.com/repos/Tracktion/pluginval/releases/assets/{ASSET_ID}",
    f"https://github.com/Tracktion/pluginval/releases/download/v{VERSION}/pluginval_Windows.zip",
)
ROUTE_ORDER = (0, 1, 0)
ATTEMPT_SECONDS = 60
ALLOWED_HOSTS = {"api.github.com", "github.com", "release-assets.githubusercontent.com",
                 "objects.githubusercontent.com"}
TRANSIENT = 10
FATAL = 11


def official_url(url):
    try:
        parts = urllib.parse.urlsplit(url)
        return (parts.scheme == "https" and parts.hostname in ALLOWED_HOSTS
                and parts.port in (None, 443) and parts.username is None
                and parts.password is None)
    except ValueError:
        return False


class OfficialRedirects(urllib.request.HTTPRedirectHandler):
    max_redirections = 5

    def redirect_request(self, request, response, code, message, headers, newurl):
        if not official_url(newurl):
            raise ValueError("non-official redirect")
        # urllib's built-in bound counts distinct URLs, so also cap total hops
        # explicitly, including loops that revisit the same official hosts.
        hops = getattr(request, "_pluginval_redirect_hops", 0) + 1
        if hops > self.max_redirections:
            raise ValueError("redirect hop limit")
        redirected = super().redirect_request(request, response, code, message, headers, newurl)
        if redirected is not None:
            redirected._pluginval_redirect_hops = hops
        return redirected


def transfer(route, target, opener=None):
    """One transfer in a killable subprocess; output never includes signed URLs."""
    opener = opener or urllib.request.build_opener(OfficialRedirects())
    request = urllib.request.Request(URLS[route], headers={
        "Accept": "application/octet-stream", "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "yamaha-cs80-pinned-validator-fetch",
    })
    try:
        with opener.open(request, timeout=15) as response:
            if not official_url(response.geturl()):
                return FATAL, {"reason": "non_official_response"}
            if response.status != 200:
                return FATAL, {"reason": "unexpected_status", "http_status": response.status}
            content_type = response.headers.get_content_type()
            if content_type not in ("application/octet-stream", "application/zip",
                                    "application/x-zip-compressed"):
                return FATAL, {"reason": "unexpected_content_type"}
            data = response.read(EXPECTED_SIZE + 1)
            host = urllib.parse.urlsplit(response.geturl()).hostname
    except urllib.error.HTTPError as error:
        retryable = error.code in (408, 429) or 500 <= error.code <= 599
        return (TRANSIENT if retryable else FATAL), {"reason": "http", "http_status": error.code}
    except urllib.error.URLError as error:
        if isinstance(error.reason, ssl.SSLError):
            return FATAL, {"reason": "tls_verification"}
        return TRANSIENT, {"reason": "transport"}
    except (TimeoutError, socket.timeout, ConnectionError, http.client.IncompleteRead):
        return TRANSIENT, {"reason": "transport"}
    except (ValueError, ssl.SSLError):
        return FATAL, {"reason": "unsafe_response"}
    if len(data) < EXPECTED_SIZE:
        return TRANSIENT, {"reason": "truncated", "bytes": len(data)}
    if len(data) > EXPECTED_SIZE:
        return FATAL, {"reason": "oversized", "bytes": len(data)}
    try:
        Path(target).write_bytes(data)
    except OSError:
        return FATAL, {"reason": "local_write"}
    return 0, {"reason": "downloaded", "bytes": len(data), "final_host": host}


def download(output, evidence, runner=subprocess.run, sleep=time.sleep):
    output, evidence = Path(output).resolve(), Path(evidence).resolve()
    record = {"repository": "Tracktion/pluginval", "version": VERSION,
              "release_id": RELEASE_ID, "asset_id": ASSET_ID,
              "asset_name": "pluginval_Windows.zip", "expected_size": EXPECTED_SIZE,
              "expected_sha256": EXPECTED_SHA256, "attempts": [], "verified": False}
    try:
        with tempfile.TemporaryDirectory(prefix=".pluginval-transfer-", dir=output.parent) as work:
            for index, route in enumerate(ROUTE_ORDER):
                target = Path(work) / f"attempt-{index + 1}.zip"
                attempt = {"number": index + 1, "source_url": URLS[route]}
                record["attempts"].append(attempt)
                command = [sys.executable, str(Path(__file__).resolve()),
                           "--transfer", str(route), "--output", str(target)]
                try:
                    result = runner(command, capture_output=True, text=True,
                                    timeout=ATTEMPT_SECONDS)
                    # Only copy known non-sensitive fields from the worker.
                    details = json.loads(result.stdout)
                    for key in ("reason", "bytes", "final_host", "http_status"):
                        if key in details:
                            attempt[key] = details[key]
                    status = result.returncode
                except subprocess.TimeoutExpired:
                    status = TRANSIENT
                    attempt["reason"] = "wall_timeout"
                attempt["transfer_exit"] = status
                if status == 0:
                    data = target.read_bytes()
                    digest = hashlib.sha256(data).hexdigest()
                    if len(data) != EXPECTED_SIZE or digest != EXPECTED_SHA256:
                        attempt["reason"] = "integrity_mismatch"
                        raise RuntimeError("pinned archive integrity mismatch; no retry")
                    # Publish only verified bytes; never reuse a failed/partial transfer.
                    target.replace(output)
                    record.update(verified=True, actual_size=len(data), actual_sha256=digest)
                    return record
                if status != TRANSIENT:
                    raise RuntimeError("non-transient transfer failure; no retry")
                if index + 1 < len(ROUTE_ORDER):
                    sleep(2 * (index + 1))
            raise RuntimeError("all three bounded transfer attempts failed")
    finally:
        # Provenance is required. A write failure keeps the workflow step failed.
        evidence.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("pluginval.zip"))
    parser.add_argument("--evidence", type=Path, default=Path("pluginval-download.json"))
    parser.add_argument("--transfer", type=int, choices=(0, 1), help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.transfer is not None:
        status, details = transfer(args.transfer, args.output)
        print(json.dumps(details))
        return status
    try:
        record = download(args.output, args.evidence)
    except (OSError, RuntimeError, ValueError):
        print("Pinned pluginval transfer failed; see download evidence.", file=sys.stderr)
        return 1
    print(f"Verified official pluginval {VERSION} Windows asset {ASSET_ID}: "
          f"{record['actual_size']} bytes, SHA256 {record['actual_sha256']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
