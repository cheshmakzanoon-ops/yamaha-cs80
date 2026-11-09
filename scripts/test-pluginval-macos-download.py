#!/usr/bin/env python3
"""Offline transfer faults and unchanged macOS validation-gate contracts."""
import email.message
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import ssl
import subprocess
import sys
import tempfile
import unittest
import urllib.error
import urllib.request
from unittest import mock

SCRIPT = Path(__file__).with_name("download-pluginval-macos.py")
if len(sys.argv) > 1 and sys.argv[1] == "--helper":
    SCRIPT = Path(sys.argv[2]).resolve()
    del sys.argv[1:3]
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("pinned_download", SCRIPT)
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)
DATA = b"inert archive fixture"
DIGEST = hashlib.sha256(DATA).hexdigest()


class Response(io.BytesIO):
    def __init__(self, data=DATA, *, status=200, content_type="application/octet-stream",
                 url="https://release-assets.githubusercontent.com/example?PRIVATE_QUERY"):
        super().__init__(data)
        self.status, self.url = status, url
        self.headers = email.message.Message()
        self.headers["Content-Type"] = content_type

    def geturl(self):
        return self.url


class DownloadTests(unittest.TestCase):
    def test_official_pin_and_attempt_bound(self):
        self.assertEqual(helper.VERSION, "1.0.4")
        self.assertEqual(helper.RELEASE_ID, 188928656)
        self.assertEqual(helper.ASSET_ID, 210999197)
        self.assertEqual(helper.EXPECTED_SIZE, 9202795)
        self.assertEqual(helper.EXPECTED_SHA256,
                         "3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f")
        self.assertEqual(helper.URLS, (
            "https://api.github.com/repos/Tracktion/pluginval/releases/assets/210999197",
            "https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_macOS.zip"))
        self.assertEqual(helper.ROUTE_ORDER, (0, 1, 0))
        self.assertEqual(helper.ATTEMPT_SECONDS, 60)
        self.assertEqual(helper.OfficialRedirects.max_redirections, 5)

    def transfer(self, response=None, error=None, route=0):
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / "download.zip"
            opener = mock.Mock()
            if error is not None:
                opener.open.side_effect = error
            else:
                opener.open.return_value = response or Response()
            with mock.patch.object(helper, "EXPECTED_SIZE", len(DATA)):
                status, details = helper.transfer(route, path, opener=opener)
            request = opener.open.call_args.args[0]
            self.assertEqual(request.full_url, helper.URLS[route])
            self.assertEqual(request.get_header("Accept"), "application/octet-stream")
            self.assertEqual(opener.open.call_args.kwargs["timeout"], 15)
            self.assertNotIn("PRIVATE", json.dumps(details))
            return status, details, path.read_bytes() if path.exists() else None

    def test_one_official_transfer(self):
        response = Response()
        with mock.patch.object(response, "read", wraps=response.read) as read:
            status, details, data = self.transfer(response=response)
            read.assert_called_once_with(len(DATA) + 1)
        self.assertEqual(status, 0)
        self.assertEqual(data, DATA)
        self.assertEqual(details["final_host"], "release-assets.githubusercontent.com")

    def test_canonical_fallback_request(self):
        status, _, data = self.transfer(route=1)
        self.assertEqual(status, 0)
        self.assertEqual(data, DATA)

    def test_local_transfer_write_failure_is_not_retryable(self):
        with mock.patch.object(Path, "write_bytes", side_effect=OSError("PRIVATE_DETAIL")):
            status, details, data = self.transfer()
        self.assertEqual(status, helper.FATAL)
        self.assertEqual(details, {"reason": "local_write"})
        self.assertIsNone(data)

    def test_transient_http_only(self):
        for code in (408, 429, 500, 502, 503, 504):
            with self.subTest(code=code):
                error = urllib.error.HTTPError(helper.URLS[0], code, "PRIVATE_DETAIL", {}, None)
                self.assertEqual(self.transfer(error=error)[0], helper.TRANSIENT)
        for code in (401, 404):
            with self.subTest(code=code):
                error = urllib.error.HTTPError(helper.URLS[0], code, "PRIVATE_DETAIL", {}, None)
                self.assertEqual(self.transfer(error=error)[0], helper.FATAL)

    def test_api_forbidden_is_route_fallback_not_browser_permission(self):
        for route, expected in ((0, 12), (1, helper.FATAL)):
            with self.subTest(route=route):
                error = urllib.error.HTTPError(helper.URLS[route], 403, "PRIVATE_DETAIL", {}, None)
                status, details, data = self.transfer(error=error, route=route)
                self.assertEqual(status, expected)
                self.assertEqual(details["http_status"], 403)
                self.assertEqual(details["reason"], "api_forbidden" if route == 0 else "http")
                self.assertIsNone(data)

    def test_transport_timeout_and_tls(self):
        self.assertEqual(self.transfer(error=TimeoutError())[0], helper.TRANSIENT)
        self.assertEqual(self.transfer(error=urllib.error.URLError(ConnectionError()))[0], helper.TRANSIENT)
        self.assertEqual(self.transfer(error=urllib.error.URLError(
            ssl.SSLCertVerificationError("PRIVATE_DETAIL")))[0], helper.FATAL)

    def test_response_shape_and_bounded_size(self):
        for response in (Response(content_type="application/json"), Response(status=206),
                         Response(url="https://untrusted.example/file"), Response(DATA + b"x")):
            with self.subTest(response=response):
                status, _, data = self.transfer(response=response)
                self.assertEqual(status, helper.FATAL)
                self.assertIsNone(data)
        self.assertEqual(self.transfer(response=Response(DATA[:-1]))[0], helper.TRANSIENT)

    def test_redirect_policy(self):
        for url in ("http://github.com/file", "https://untrusted.example/file",
                    "https://github.com.evil.example/file", "https://name@github.com/file",
                    "https://github.com:444/file", "https://github.com:bad/file"):
            with self.subTest(url=url):
                self.assertFalse(helper.official_url(url))
                with self.assertRaises(ValueError):
                    helper.OfficialRedirects().redirect_request(
                        urllib.request.Request(helper.URLS[0]), None, 302, "", {}, url)
        url = "https://release-assets.githubusercontent.com/file?signature=PRIVATE"
        redirect = helper.OfficialRedirects().redirect_request(
            urllib.request.Request(helper.URLS[0]), None, 302, "", {}, url)
        self.assertEqual(redirect.full_url, url)
        request = urllib.request.Request(helper.URLS[0])
        handler = helper.OfficialRedirects()
        for hop in range(5):
            # Revisit two URLs: a distinct-URL limit alone does not cap hops.
            request = handler.redirect_request(request, None, 302, "", {}, helper.URLS[hop % 2])
        with self.assertRaises(ValueError):
            handler.redirect_request(request, None, 302, "", {}, helper.URLS[1])

    def test_actual_urllib_redirect_loop_stops_before_sixth_hop(self):
        handler = helper.OfficialRedirects()

        class Parent:
            calls = 0

            def open(self, request, timeout=None):
                request.timeout = timeout
                self.calls += 1
                headers = email.message.Message()
                headers["Location"] = helper.URLS[self.calls % 2]
                return handler.http_error_302(request, io.BytesIO(), 302, "Found", headers)

        parent = Parent()
        handler.parent = parent
        with self.assertRaisesRegex(ValueError, "redirect hop limit"):
            parent.open(urllib.request.Request(helper.URLS[0]))
        self.assertEqual(parent.calls, 6)  # Initial open plus five followed hops.

    def run_download(self, outcomes, *, expected_success=False, bad_hash=False, old_file=False):
        calls, sleeps = [], []
        with tempfile.TemporaryDirectory() as work:
            output, evidence = Path(work) / "pluginval.zip", Path(work) / "evidence.json"
            if old_file:
                output.write_bytes(b"previous file")

            def runner(command, **kwargs):
                calls.append(command)
                self.assertEqual(kwargs, {"capture_output": True, "text": True, "timeout": 60})
                self.assertEqual(command[:2], [sys.executable, str(SCRIPT.resolve())])
                self.assertNotIn("--strictness-level", command)
                outcome = outcomes[len(calls) - 1]
                if outcome == "timeout":
                    raise subprocess.TimeoutExpired(command, 60)
                target = Path(command[-1])
                # Even a failed worker may have left a partial file.
                target.write_bytes(DATA if outcome == 0 else b"partial")
                return subprocess.CompletedProcess(command, outcome,
                    json.dumps({"reason": ("downloaded" if outcome == 0 else
                                          "api_forbidden" if outcome == 12 else "http"),
                                "http_status": 200 if outcome == 0 else 403 if outcome == 12 else 504}), "")

            digest = "0" * 64 if bad_hash else DIGEST
            with (mock.patch.object(helper, "EXPECTED_SIZE", len(DATA)),
                  mock.patch.object(helper, "EXPECTED_SHA256", digest)):
                if expected_success:
                    result = helper.download(output, evidence, runner=runner, sleep=sleeps.append)
                    self.assertTrue(result["verified"])
                    self.assertEqual(output.read_bytes(), DATA)
                else:
                    with self.assertRaises(RuntimeError):
                        helper.download(output, evidence, runner=runner, sleep=sleeps.append)
                    if old_file:
                        self.assertEqual(output.read_bytes(), b"previous file")
                    else:
                        self.assertFalse(output.exists())
            record = json.loads(evidence.read_text())
            self.assertEqual(record["verified"], expected_success)
            self.assertEqual(record["expected_sha256"], digest)
            self.assertEqual(record["asset_id"], 210999197)
            self.assertEqual(list(Path(work).glob(".pluginval-transfer-*")), [])
            return calls, sleeps, record

    def test_first_transfer_success(self):
        calls, sleeps, _ = self.run_download([0], expected_success=True)
        self.assertEqual(len(calls), 1)
        self.assertEqual(sleeps, [])

    def test_transient_fallback_and_partial_isolation(self):
        calls, sleeps, record = self.run_download([10, 0], expected_success=True)
        self.assertEqual([c[3] for c in calls], ["0", "1"])
        self.assertNotEqual(calls[0][-1], calls[1][-1])
        self.assertEqual(sleeps, [2])
        self.assertEqual(len(record["attempts"]), 2)

    def test_api_forbidden_can_use_pinned_browser_route(self):
        calls, sleeps, record = self.run_download([12, 0], expected_success=True)
        self.assertEqual([c[3] for c in calls], ["0", "1"])
        self.assertEqual(sleeps, [2])
        self.assertEqual(record["attempts"][0]["transfer_exit"], 12)
        self.assertEqual(record["attempts"][0]["http_status"], 403)
        self.assertEqual(record["attempts"][0]["reason"], "api_forbidden")

    def test_api_forbidden_does_not_hide_browser_denial_or_bad_hash(self):
        for outcomes, bad_hash in (([12, 11], False), ([12, 0], True)):
            with self.subTest(outcomes=outcomes, bad_hash=bad_hash):
                calls, sleeps, record = self.run_download(outcomes, bad_hash=bad_hash, old_file=True)
                self.assertEqual([c[3] for c in calls], ["0", "1"])
                self.assertEqual(sleeps, [2])
                self.assertFalse(record["verified"])

    def test_api_forbidden_does_not_increase_attempt_cap(self):
        calls, sleeps, record = self.run_download([12, 10, 12], old_file=True)
        self.assertEqual([c[3] for c in calls], ["0", "1", "0"])
        self.assertEqual(sleeps, [2, 4])
        self.assertEqual(len(record["attempts"]), 3)

    def test_api_fallback_status_is_rejected_on_browser_route(self):
        calls, sleeps, _ = self.run_download([10, 12], old_file=True)
        self.assertEqual([c[3] for c in calls], ["0", "1"])
        self.assertEqual(sleeps, [2])

    def test_timeout_then_success(self):
        calls, sleeps, record = self.run_download(["timeout", 0], expected_success=True)
        self.assertEqual(len(calls), 2)
        self.assertEqual(record["attempts"][0]["reason"], "wall_timeout")
        self.assertEqual(sleeps, [2])

    def test_three_transfer_limit(self):
        calls, sleeps, _ = self.run_download([10, "timeout", 10], old_file=True)
        self.assertEqual([c[3] for c in calls], ["0", "1", "0"])
        self.assertEqual(sleeps, [2, 4])

    def test_integrity_and_permanent_failures_never_retry(self):
        for outcomes, bad_hash in (([0], True), ([11], False), ([7], False)):
            with self.subTest(outcomes=outcomes, bad_hash=bad_hash):
                calls, sleeps, _ = self.run_download(outcomes, bad_hash=bad_hash)
                self.assertEqual(len(calls), 1)
                self.assertEqual(sleeps, [])

    def test_bad_worker_metadata_fails_without_publishing_or_retry(self):
        with tempfile.TemporaryDirectory() as work:
            output, evidence = Path(work) / "archive.zip", Path(work) / "evidence.json"
            runner = mock.Mock(return_value=subprocess.CompletedProcess(
                [], 0, "not JSON", "PRIVATE_SIGNED_URL"))
            with self.assertRaises(ValueError):
                helper.download(output, evidence, runner=runner)
            self.assertEqual(runner.call_count, 1)
            self.assertFalse(output.exists())
            record = json.loads(evidence.read_text())
            self.assertFalse(record["verified"])
            self.assertNotIn("PRIVATE", evidence.read_text())

    def test_required_evidence_failure_cannot_report_success(self):
        with tempfile.TemporaryDirectory() as work:
            output, evidence = Path(work) / "archive.zip", Path(work) / "evidence.json"
            evidence.mkdir()  # Deliberately unwritable as an evidence file.

            def runner(command, **kwargs):
                Path(command[-1]).write_bytes(DATA)
                return subprocess.CompletedProcess(command, 0, '{"reason":"downloaded"}', "")

            with (mock.patch.object(helper, "EXPECTED_SIZE", len(DATA)),
                  mock.patch.object(helper, "EXPECTED_SHA256", DIGEST)):
                with self.assertRaises(OSError):
                    helper.download(output, evidence, runner=runner)
            # These bytes are verified, but the workflow must not consume them:
            # the required provenance write makes the helper exit unsuccessfully.
            self.assertEqual(output.read_bytes(), DATA)

    def test_workflow_gate_unchanged_and_transfer_failure_stops_execution(self):
        root = Path(__file__).resolve().parents[1]
        workflow = (root / ".github/workflows/build.yml").read_text()
        macos = workflow.split("  macos:", 1)[1].split("  windows:", 1)[0]
        gate = macos.split("      - name: Register built AU", 1)[1].split(
            "      - name: Package native macOS", 1)[0]
        self.assertEqual(hashlib.sha256(gate.encode()).hexdigest(),
                         "17f7fba2b466e8c3cd0686f35cbe93e42227583e73064f763670d150befd6b84")
        fetch = macos.split("      - name: Fetch pluginval", 1)[1].split(
            "      # Audio Unit hosts", 1)[0]
        self.assertIn("timeout-minutes: 5", fetch)
        self.assertIn("set -euo pipefail", fetch)
        self.assertIn("python3 scripts/download-pluginval-macos.py --output pluginval.zip "
                      "--evidence pluginval-download.json\n"
                      "          unzip -q pluginval.zip -d pluginval", fetch)
        self.assertNotIn("continue-on-error", fetch)
        self.assertNotIn("|| true", fetch.split("          unzip", 1)[0])
        self.assertIn("            pluginval-download.json", macos)
        self.assertIn("runner: [macos-15, macos-15-intel]", macos)
        # Preserve both unrelated platform jobs, including their validators.
        linux = workflow.split("  macos:", 1)[0]
        windows = workflow.split("  windows:", 1)[1]
        self.assertEqual(hashlib.sha256(linux.encode()).hexdigest(), "4a6fd4498d4c7910f8db71e600b2f950e4b5a8fac0e9d3a26170071672e07bbe")
        self.assertEqual(hashlib.sha256(windows.encode()).hexdigest(), "2a97e09e9eef2661d7c97db7cba4d0d5f6cfb12d0015bb6120d81c65fdb2aabc")


if __name__ == "__main__":
    unittest.main()
