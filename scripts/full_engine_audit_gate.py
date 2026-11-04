#!/usr/bin/env python3
"""Source-locked, coverage-only completion gate for the registered engine audit.

Numerical targets come unchanged from the frozen reporter and committed tolerance
JSONs. A successful capture does NOT mean every existing numerical target passed.
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
from tempfile import TemporaryDirectory

REPO = Path(__file__).resolve().parents[1]
REQUEST_PATH = Path(".github/full-engine-audit-request.json")
REGISTRATION_SHA = "00da4498792523cf8d97fd40c17e12f215e190b8"
EXPECTED_COMMAND = [
    "./build-core/cs80_engine_audit", "--out", "artifacts/audit",
    "--suite", "all", "--rates", "44100,48000,96000",
]
COORDINATES = ("original_physical_panel", "electrical_programmer", "tone_tabs")
CAUSAL_IDENTITIES = {
    "original_physical_panel": ("Bass Stack", "note_on"),
    "electrical_programmer": ("Bass Stack", "note_on"),
    "tone_tabs": ("Funky 1 + Funky 2", "single_note_off"),
}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def git(*args: str) -> str:
    return subprocess.check_output(["git", "-C", str(REPO), *args], text=True).strip()


def load_reporter(path: Path):
    spec = importlib.util.spec_from_file_location("frozen_cs80_engine_reporter", path)
    if spec is None or spec.loader is None:
        raise ValueError("Unable to import preregistered numerical reporter")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def registered_contract():
    path = REPO / REQUEST_PATH
    raw = path.read_bytes()
    committed = subprocess.check_output(
        ["git", "-C", str(REPO), "show", f"{REGISTRATION_SHA}:{REQUEST_PATH}"])
    if raw != committed:
        raise ValueError("Audit registration changed after its prior commit")
    request = json.loads(raw)
    if request["schema_version"] != 1 or request["quick"] is not False:
        raise ValueError("Unexpected audit request schema or abbreviated run")
    if request["command"] != EXPECTED_COMMAND:
        raise ValueError("Audit command differs from the preregistered command")
    if request["rate_order_hz"] != [44100, 48000, 96000]:
        raise ValueError("Audit must retain the exact three rates and their order")
    reporter_path = Path(request["frozen_analysis"])
    frozen_now = (REPO / reporter_path).read_bytes()
    frozen_then = subprocess.check_output(
        ["git", "-C", str(REPO), "show", f"{REGISTRATION_SHA}:{reporter_path}"])
    if frozen_now != frozen_then:
        raise ValueError("Numerical reporter mutated after preregistration")
    for target in request["existing_numerical_contracts"]:
        if (REPO / target).read_bytes() != subprocess.check_output(
                ["git", "-C", str(REPO), "show", f"{REGISTRATION_SHA}:{target}"]):
            raise ValueError(f"Frozen numerical tolerance mutated: {target}")
    reporter = load_reporter(REPO / reporter_path)
    if request["suite_files"] != reporter.EXPECTED_ROWS:
        raise ValueError("Registered row counts differ from frozen schema")
    if tuple(request["rate_order_hz"]) != reporter.RATES:
        raise ValueError("Registered sample rates differ from frozen schema")
    return request, reporter


def verify_identity(observed: str, expected: str):
    if observed != expected:
        raise ValueError(f"Production source tree mismatch: {observed} != {expected}")


def preflight(request):
    verify_identity(git("rev-parse", "HEAD:src"), request["expected_src_tree"])
    production = request["production_audio_commit"]
    if len(production) != 40 or git("rev-parse", f"{production}^{{commit}}") != production:
        raise ValueError("Registered production audio commit unavailable")
    result = subprocess.run(
        ["git", "-C", str(REPO), "merge-base", "--is-ancestor", production, "HEAD"],
        check=False)
    if result.returncode != 0:
        raise ValueError("Registered production commit is not an ancestor")
    print(json.dumps({
        "preflight": "passed", "registration_commit": REGISTRATION_SHA,
        "production_audio": production, "source_tree": request["expected_src_tree"],
        "audit_command": request["command"], "numerical_limits_modified": False,
    }, indent=2))


def validate_capture(path: Path, request, reporter):
    files, suites, nonfinite_cells, extras = reporter.load_csvs(path)
    if extras:
        raise ValueError(f"Unregistered extra CSV suites: {extras}")
    if {name: item["row_count"] for name, item in files.items()} != request["suite_files"]:
        raise ValueError("CSV row counts differ from preregistered coverage")
    coverage = reporter.validate_coverage(suites["phrase.csv"])
    for coordinate in COORDINATES:
        sampled = [int(row["raw"]["rate"]) for row in suites["click-causality.csv"]
                   if row["raw"]["coordinate"] == coordinate]
        if sampled != request["rate_order_hz"]:
            raise ValueError(f"Missing or duplicated causality coordinate/rate: {coordinate}")
        expected_name, expected_event = CAUSAL_IDENTITIES[coordinate]
        for row in suites["click-causality.csv"]:
            if row["raw"]["coordinate"] == coordinate and (
                    row["raw"]["name"] != expected_name or
                    row["raw"]["event"] != expected_event):
                raise ValueError(f"Unexpected causal program/event at {coordinate}: {row['raw']['name']}/{row['raw']['event']}")
    if len(suites["click-causality.csv"]) != len(COORDINATES) * len(request["rate_order_hz"]):
        raise ValueError("Unexpected extra causality rows")
    return files, coverage, nonfinite_cells


def write_csv(path: Path, reporter, rows: list[dict[str, str]], name: str):
    buffer = io.StringIO(newline="")
    writer = csv.DictWriter(buffer, fieldnames=reporter.HEADERS[name], lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    (path / name).write_text(buffer.getvalue())


def row_for(reporter, suite_name: str, **overrides: str):
    row = {field: "0" for field in reporter.HEADERS[suite_name]}
    for field in reporter.TEXT_FIELDS:
        if field in row:
            row[field] = "x"
    row.update(overrides)
    return row


def good_fixture(path: Path, reporter):
    phrase = []
    for policy in reporter.POLICIES:
        for rate in reporter.RATES:
            indices = range(222) if rate == 44100 else range(0, 222, 16)
            for index in indices:
                phrase.append(row_for(
                    reporter, "phrase.csv", name=f"Program-{index:03d}",
                    group=f"program:{index}", policy=policy, rate=str(rate)))
            for index in range(11):
                phrase.append(row_for(
                    reporter, "phrase.csv", name=f"TonePair-{index:02d}",
                    group="tab", policy=policy, rate=str(rate)))
    write_csv(path, reporter, phrase, "phrase.csv")
    for name in ("fx.csv", "alias.csv", "latency.csv"):
        rows = [row_for(reporter, name, rate="44100", policy="source")
                for _ in range(reporter.EXPECTED_ROWS[name])]
        write_csv(path, reporter, rows, name)
    causal = [
        row_for(reporter, "click-causality.csv", name=CAUSAL_IDENTITIES[coordinate][0],
                event=CAUSAL_IDENTITIES[coordinate][1],
                rate=str(rate), coordinate=coordinate)
        for coordinate in COORDINATES for rate in reporter.RATES
    ]
    write_csv(path, reporter, causal, "click-causality.csv")


def self_test(request, reporter):
    with TemporaryDirectory(prefix="cs80-audit-gate-") as directory:
        root = Path(directory)
        good_fixture(root, reporter)
        validate_capture(root, request, reporter)
        path = root / "phrase.csv"
        original = path.read_text()
        probes = [
            ("missing row", lambda: path.write_text("\n".join(original.splitlines()[:-1]) + "\n")),
            ("header mutation", lambda: path.write_text(original.replace(
                "name,group,policy", "group,name,policy", 1))),
            ("duplicate program", lambda: path.write_text(original.replace(
                "Program-001", "Program-000", 1))),
            ("invalid policy", lambda: path.write_text(original.replace(
                ",legacy,", ",mutant,", 1))),
            ("malformed numeric", lambda: path.write_text(original.replace(
                ",44100,", ",INVALID,", 1))),
        ]
        for name, mutation in probes:
            mutation()
            try:
                validate_capture(root, request, reporter)
            except (ValueError, KeyError):
                pass
            else:
                raise AssertionError(f"Mutation survived CSV acceptance gate: {name}")
            path.write_text(original)
        causal = root / "click-causality.csv"
        original_causal = causal.read_text()
        causal.write_text(original_causal.replace(",original_physical_panel,", ",mutant,", 1))
        try:
            validate_capture(root, request, reporter)
        except (ValueError, KeyError):
            pass
        else:
            raise AssertionError("Mutation survived causal-coordinate coverage gate")
        causal.write_text(original_causal)
        for label, old, new in (
                ("causal program identity", "Bass Stack", "Wrong Bass"),
                ("causal event identity", ",single_note_off,", ",note_on,")):
            causal.write_text(original_causal.replace(old, new, 1))
            try:
                validate_capture(root, request, reporter)
            except (ValueError, KeyError):
                pass
            else:
                raise AssertionError(f"Mutation survived {label} gate")
            causal.write_text(original_causal)
        (root / "unregistered.csv").write_text("x\n")
        try:
            validate_capture(root, request, reporter)
        except (ValueError, KeyError):
            pass
        else:
            raise AssertionError("Mutation survived unexpected-suite gate")
        (root / "unregistered.csv").unlink()
        try:
            verify_identity("0000000", request["expected_src_tree"])
        except ValueError:
            pass
        else:
            raise AssertionError("Mutation survived source identity gate")
        print("PASS: 10 audit coverage and production-identity mutation controls")


def finalize(request, reporter):
    preflight(request)
    destination = REPO / "artifacts"
    complete = destination / "audit.complete"
    if complete.exists():
        raise ValueError("Existing audit.complete marker; refusing stale result reuse")
    files, coverage, nonfinite_cells = validate_capture(destination / "audit", request, reporter)
    head = git("rev-parse", "HEAD")
    committed = os.environ.get("GITHUB_SHA", head)
    if head != committed:
        raise ValueError("Runner commit identity differs from checked-out source")
    compiled = REPO / "build-core/cs80_engine_audit"
    provenance = {
        "schema_version": 1, "generated_utc": datetime.now(timezone.utc).isoformat(),
        "complete_capture": True, "github_run_id": os.environ.get("GITHUB_RUN_ID"),
        "github_run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT"),
        "head_sha": head, "production_audio_commit": request["production_audio_commit"],
        "src_tree_sha": request["expected_src_tree"],
        "registration_commit": REGISTRATION_SHA,
        "registration_json_sha256": sha((REPO / REQUEST_PATH).read_bytes()),
        "registered_command": request["command"], "quick": False,
        "compiler_environment": platform.platform(),
        "binary_sha256": sha(compiled.read_bytes()),
        "reporter_sha256": sha((REPO / request["frozen_analysis"]).read_bytes()),
        "raw_files": {name: {
            "bytes": info["bytes"], "sha256": info["sha256"],
            "rows": info["row_count"],
        } for name, info in files.items()},
        "coverage": [{k: v for k, v in row.items()
                      if k not in ("program_names", "tab_pair_names")}
                     for row in coverage],
        "nonfinite_numeric_cells_reported_not_hidden": len(nonfinite_cells),
        "numerical_targets": "original committed bounds; failures remain reported",
        "hardware_measurement": False, "cpu_suite_included": False,
    }
    encoded = json.dumps(provenance, indent=2, sort_keys=True) + "\n"
    marker_text = (
        f"completed_utc={provenance['generated_utc']}\n"
        f"source_sha={head}\n"
        f"run_id={provenance['github_run_id']}\n"
        f"registration_commit={REGISTRATION_SHA}\n"
        f"provenance_sha256={sha(encoded.encode())}\n")
    reporter.SOURCE_SHA = head
    reporter.PRODUCTION_SHA = request["production_audio_commit"]
    # A failed/timed-out reporter must never leave a published completed marker.
    # Keep its provisional marker and report files in a disposable stage.
    with TemporaryDirectory(prefix=".cs80-audit-stage-", dir=destination) as directory:
        stage = Path(directory)
        (stage / "audit").symlink_to(destination / "audit", target_is_directory=True)
        (stage / "audit.complete").write_text(marker_text)
        saved_args = sys.argv
        sys.argv = [
            str(REPO / request["frozen_analysis"]), "--root", str(stage),
            "--repo", str(REPO),
        ]
        try:
            if reporter.main() != 0:
                raise ValueError("Frozen numerical reporter refused complete capture")
        finally:
            sys.argv = saved_args
        for name in ("audit-summary.json", "audit-summary.md"):
            if not (stage / name).is_file():
                raise ValueError(f"Frozen reporter did not produce {name}")
        # Transfer reports before provenance and put the completion marker LAST.
        for name in ("audit-summary.json", "audit-summary.md"):
            (stage / name).replace(destination / name)
    (destination / "audit-provenance.json").write_text(encoded)
    complete.write_text(marker_text)
    print("Complete full-bank three-rate engine audit retained; numerical failures are not waived.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--self-test", action="store_true")
    group.add_argument("--preflight", action="store_true")
    group.add_argument("--finalize", action="store_true")
    args = parser.parse_args()
    request, reporter = registered_contract()
    if args.self_test:
        self_test(request, reporter)
    elif args.preflight:
        preflight(request)
    else:
        finalize(request, reporter)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, AssertionError, subprocess.CalledProcessError) as exc:
        print(f"Full engine audit gate refused: {exc}", file=sys.stderr)
        sys.exit(2)
