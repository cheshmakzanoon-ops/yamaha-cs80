#!/usr/bin/env python3
"""Reproduce the Windows baseline-byte failure in isolated Git checkouts.

This exercises Git's newline conversion locally. It does not claim to execute
Windows or qualify any native plugin build. Temporary repositories have no
remotes or commits; the working repository's index/config is never changed.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
SOURCE = "fcc3c8e055223fe4f786ca162ff46076c33471c8"
BASELINE = "docs/preset-evidence/2026.11.5/baseline/inventory.json"
SCRIPT = "scripts/compare-preset-response-2026-11-6.py"
TEST = "scripts/test-compare-preset-response-2026-11-6.py"
EXPECTED = "0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611"
OBSERVED_WINDOWS = "072f6692ec360248b8edf104452f2c59272be028495d3949b1e6523895311e59"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(path, *args, input=None):
    return subprocess.run(["git", *args], cwd=path, input=input,
                          capture_output=True, check=True).stdout


def snapshot(data):
    return dict(bytes=len(data), sha256=digest(data), lf=data.count(b"\n"),
                crlf=data.count(b"\r\n"))


def checkout_case(blobs, attributes, autocrlf, eol):
    label = f"{'fixed' if attributes else 'before'}-autocrlf-{autocrlf}-eol-{eol}"
    with tempfile.TemporaryDirectory(prefix="cs80-baseline-checkout-") as temporary:
        path = Path(temporary)
        git(path, "init", "--quiet")
        git(path, "config", "core.autocrlf", autocrlf)
        git(path, "config", "core.eol", eol)
        entries = dict(blobs)
        if attributes:
            entries[".gitattributes"] = attributes
        for name, data in entries.items():
            blob = git(path, "hash-object", "-w", "--stdin", input=data).decode().strip()
            git(path, "update-index", "--add", "--cacheinfo", f"100644,{blob},{name}")
        git(path, "checkout-index", "--all", "--force")
        checked = (path / BASELINE).read_bytes()
        attribute = git(path, "check-attr", "text", "--", BASELINE).decode().strip()
        result = subprocess.run([sys.executable, str(path / TEST)], cwd=path,
                                capture_output=True, text=True)
        # The log is local exercise evidence, never a native Windows job log.
        log = (result.stdout + result.stderr).encode()
        (OUT / f"{label}.log").write_bytes(log)
        expected_digest = EXPECTED if attributes else OBSERVED_WINDOWS
        assert digest(checked) == expected_digest, (label, snapshot(checked))
        assert result.returncode == (0 if attributes else 1), (label, log.decode())
        assert "Ran 7 tests" in log.decode(), label
        if attributes:
            assert checked == blobs[BASELINE]
            assert attribute.endswith(": text: unset"), attribute
            assert "OK" in result.stderr, label
        else:
            assert "test_cli_valid_report_and_failed_comparison_are_atomic" in result.stderr
            assert EXPECTED in result.stderr and OBSERVED_WINDOWS in result.stderr
        return dict(label=label, configuration=dict(core_autocrlf=autocrlf, core_eol=eol),
                    checked_out_baseline=snapshot(checked), text_attribute=attribute,
                    test_returncode=result.returncode, contract_test_groups=7,
                    log_sha256=digest(log))


def byte_authentication_controls(baseline):
    candidate = ROOT / "docs/preset-evidence/2026.11.6/verification/a15/inventory.json"
    semantic = json.loads(baseline)
    semantic["programs"][0]["values"][0] += .125
    variants = {
        "trailing-LF-to-CRLF": baseline.replace(b"\n", b"\r\n"),
        "remove-trailing-LF": baseline.rstrip(b"\n"),
        "add-whitespace": baseline + b" ",
        "semantic-parameter-mutation": json.dumps(semantic).encode(),
    }
    receipts = []
    with tempfile.TemporaryDirectory(prefix="cs80-baseline-negative-") as temporary:
        path = Path(temporary)
        before, output = path / "before.json", path / "report.json"
        sentinel = b"previous valid report\n"
        for label, raw in variants.items():
            before.write_bytes(raw)
            output.write_bytes(sentinel)
            result = subprocess.run([sys.executable, str(ROOT / SCRIPT), "--before", str(before),
                                     "--after", str(candidate), "--out", str(output)],
                                    capture_output=True, text=True)
            assert result.returncode == 1, (label, result.stderr)
            assert "recorded baseline bytes" in result.stderr, (label, result.stderr)
            assert output.read_bytes() == sentinel, label
            assert before.read_bytes() == raw, label
            receipts.append(dict(label=label, input=snapshot(raw), returncode=result.returncode,
                                 rejected_by_raw_byte_pin=True, input_and_report_unchanged=True))
    return dict(candidate_sha256=digest(candidate.read_bytes()), controls=receipts)


def main():
    blobs = {name: git(ROOT, "show", f"{SOURCE}:{name}") for name in (BASELINE, SCRIPT, TEST)}
    assert digest(blobs[BASELINE]) == EXPECTED
    for name, data in blobs.items():
        assert (ROOT / name).read_bytes() == data, f"Unexpected edit to frozen file: {name}"
    attributes = (ROOT / ".gitattributes").read_bytes()
    before = checkout_case(blobs, None, "true", "crlf")
    fixed = [checkout_case(blobs, attributes, autocrlf, eol)
             for autocrlf in ("true", "false", "input") for eol in ("lf", "crlf")]
    report = dict(schema=1, evidence_kind="local_git_checkout_regression",
                  interpretation="Git newline conversion exercised locally; not a native Windows rerun or release qualification.",
                  historical_failed_source=SOURCE, baseline_path=BASELINE,
                  original_baseline=snapshot(blobs[BASELINE]),
                  exact_crlf_digest_matches_windows_failure=True,
                  original_scripts_unchanged={name: digest(data) for name, data in blobs.items() if name != BASELINE},
                  attributes_sha256=digest(attributes), original_failure_reproduced=before,
                  fixed_checkout_cases=fixed, negative_authentication=byte_authentication_controls(blobs[BASELINE]),
                  actual_windows_ci_after_fix="pending")
    (OUT / "checkout-regression.json").write_text(json.dumps(report, indent=2) + "\n")
    print("PASS: original CRLF failure reproduced; six corrected Git checkout modes pass all seven contract groups; four changed-byte controls remain rejected.")


if __name__ == "__main__":
    main()
