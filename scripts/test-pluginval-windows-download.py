#!/usr/bin/env python3
"""Offline transfer faults and unchanged Windows validation-gate contracts."""
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

SCRIPT = Path(__file__).with_name("download-pluginval-windows.py")
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
        self.assertEqual(helper.ASSET_ID, 210999198)
        self.assertEqual(helper.EXPECTED_SIZE, 2408590)
        self.assertEqual(helper.EXPECTED_SHA256,
                         "c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab")
        self.assertEqual(helper.URLS, (
            "https://api.github.com/repos/Tracktion/pluginval/releases/assets/210999198",
            "https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip"))
        self.assertEqual(helper.ROUTE_ORDER, (0, 1, 0))
        self.assertEqual(helper.ATTEMPT_SECONDS, 60)
        self.assertEqual(helper.OfficialRedirects.max_redirections, 5)

    def transfer(self, response=None, error=None):
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / "download.zip"
            opener = mock.Mock()
            if error is not None:
                opener.open.side_effect = error
            else:
                opener.open.return_value = response or Response()
            with mock.patch.object(helper, "EXPECTED_SIZE", len(DATA)):
                status, details = helper.transfer(0, path, opener=opener)
            request = opener.open.call_args.args[0]
            self.assertEqual(request.full_url, helper.URLS[0])
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

    def test_transient_http_only(self):
        for code in (408, 429, 500, 502, 503, 504):
            with self.subTest(code=code):
                error = urllib.error.HTTPError(helper.URLS[0], code, "PRIVATE_DETAIL", {}, None)
                self.assertEqual(self.transfer(error=error)[0], helper.TRANSIENT)
        for code in (401, 403, 404):
            with self.subTest(code=code):
                error = urllib.error.HTTPError(helper.URLS[0], code, "PRIVATE_DETAIL", {}, None)
                self.assertEqual(self.transfer(error=error)[0], helper.FATAL)

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
                    json.dumps({"reason": "downloaded" if outcome == 0 else "http",
                                "http_status": 200 if outcome == 0 else 504}), "")

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
            self.assertEqual(record["asset_id"], 210999198)
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

    def test_workflow_gate_unchanged_and_transfer_failure_stops_execution(self):
        root = Path(__file__).resolve().parents[1]
        workflow = (root / ".github/workflows/build.yml").read_text()
        windows = workflow.split("  pluginval-windows:", 1)[1]
        gate = windows.split("      - name: Validate VST3 headless", 1)[1].split(
            "      - name: Report pluginval failure", 1)[0]
        self.assertEqual(hashlib.sha256(gate.encode()).hexdigest(),
                         "5411e05277e80108908b00a6b4f26c2b0ea256c71d72c079307996ab7c7e4c4b")
        fetch = windows.split("      - name: Fetch pluginval", 1)[1].split(
            "      - name: Validate VST3 headless", 1)[0]
        self.assertIn("timeout-minutes: 5", fetch)
        self.assertIn("python scripts/download-pluginval-windows.py --output pluginval.zip "
                      "--evidence pluginval-download.json\n"
                      "          if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\n"
                      "          Expand-Archive", fetch)
        self.assertNotIn("continue-on-error", fetch)
        self.assertIn("persist-credentials: false", windows)
        self.assertIn("            pluginval-download.json", windows)


if __name__ == "__main__":
    unittest.main()
