#!/usr/bin/env python3
"""Source-mutation qualification of the optional CPU evidence collector.

This is a fast Python-only contract test. It neither builds nor runs DSP code.
The acceptance contract and its frozen benchmark fixture must be committed
*before* evaluating any mutation scores. See the contract's mutation_method.
"""

import copy
import csv
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile


ROOT = Path(__file__).resolve().parents[2]
SOURCE_REPO = Path(os.environ.get("CS80_SOURCE_REPO", str(ROOT))).resolve()
RUNNER_SOURCE = ROOT / "scripts/run-cpu-qualification.py"
ACCEPTANCE = ROOT / "tests/cpu/cpu-qualification-runner-acceptance.json"
REQUEST = ROOT / ".github/cpu-qualification-request.json"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_runner(path, module_name):
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        raise AssertionError(f"cannot import source under test: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def one_replace(source, old, new):
    occurrences = source.count(old)
    if occurrences != 1:
        raise AssertionError(f"mutation anchor expected once, found {occurrences}: {old!r}")
    return source.replace(old, new, 1)


def reject_value_error(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("invalid control passed without a ValueError")


def fixture_copy(source, target):
    target.mkdir()
    for filename in ("metadata.json", "completed.json", "results.csv"):
        shutil.copyfile(source / filename, target / filename)


def alter_json(directory, filename, key, value):
    path = directory / filename
    data = json.loads(path.read_text())
    data[key] = value
    path.write_text(json.dumps(data, indent=2) + "\n")


def alter_csv(directory, action):
    path = directory / "results.csv"
    with path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        columns, rows = reader.fieldnames, list(reader)
    action(rows)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)


def fixture_witness(mutant, original, directory, commit, edit):
    original.validate_round(directory, commit)
    mutant.validate_round(directory, commit)
    edit(directory)
    reject_value_error(lambda: original.validate_round(directory, commit))
    mutant.validate_round(directory, commit)


def request_witness(mutant, original, request, edit):
    original.validate_request(request)
    mutant.validate_request(request)
    malformed = copy.deepcopy(request)
    edit(malformed)
    reject_value_error(lambda: original.validate_request(malformed))
    mutant.validate_request(malformed)


def provenance_witness(kind, mutant, original, reporter, checkpoint, scratch):
    if kind == "reporter_hash":
        original.verify_reporter(reporter, sha256(reporter))
        mutant.verify_reporter(reporter, sha256(reporter))
        altered = scratch / "tampered-reporter.py"
        altered.write_bytes(reporter.read_bytes() + b"\n# tampered\n")
        reject_value_error(lambda: original.verify_reporter(altered, sha256(reporter)))
        mutant.verify_reporter(altered, sha256(reporter))
    elif kind == "benchmark_blob":
        def fake_read_command(args, cwd=None):
            if args[:2] != ["git", "rev-parse"]:
                raise AssertionError(f"unexpected git query: {args}")
            return checkpoint["source_commit"] if args[2].endswith("^{commit}") else "f" * 40

        original.read_command = fake_read_command
        mutant.read_command = fake_read_command
        clean = copy.deepcopy(checkpoint)
        clean["benchmark_blob"] = "f" * 40
        original.verify_checkpoint(SOURCE_REPO, clean)
        mutant.verify_checkpoint(SOURCE_REPO, clean)
        reject_value_error(lambda: original.verify_checkpoint(SOURCE_REPO, checkpoint))
        mutant.verify_checkpoint(SOURCE_REPO, checkpoint)
    else:
        binary = scratch / "binary"
        binary.write_bytes(b"unaltered compiled benchmark")
        def clean_read_command(args, cwd=None):
            if args == ["git", "rev-parse", "HEAD"]:
                return checkpoint["source_commit"]
            if args == ["git", "status", "--porcelain"]:
                return ""
            raise AssertionError(f"unexpected git command {args}")

        original.read_command = clean_read_command
        mutant.read_command = clean_read_command
        initial_sha = sha256(binary)
        original.verify_binary_and_source(binary, initial_sha, SOURCE_REPO, checkpoint["source_commit"])
        mutant.verify_binary_and_source(binary, initial_sha, SOURCE_REPO, checkpoint["source_commit"])
        if kind == "binary_sha":
            binary.write_bytes(binary.read_bytes() + b"tampered")
        elif kind == "dirty_source":
            def dirty_read_command(args, cwd=None):
                if args == ["git", "status", "--porcelain"]:
                    return " M src/core/Engine.cpp"
                return clean_read_command(args, cwd)

            original.read_command = dirty_read_command
            mutant.read_command = dirty_read_command
        elif kind == "source_commit":
            def changed_read_command(args, cwd=None):
                if args == ["git", "rev-parse", "HEAD"]:
                    return "f" * 40
                return clean_read_command(args, cwd)

            original.read_command = changed_read_command
            mutant.read_command = changed_read_command
        else:
            raise AssertionError(f"unknown provenance witness {kind}")
        reject_value_error(lambda: original.verify_binary_and_source(
            binary, initial_sha, SOURCE_REPO, checkpoint["source_commit"]))
        mutant.verify_binary_and_source(binary, initial_sha, SOURCE_REPO, checkpoint["source_commit"])


def run():
    contract = json.loads(ACCEPTANCE.read_text())
    reference = contract["reference"]
    frozen = SOURCE_REPO / reference["source_fixture"]
    for name, pinned in (("metadata.json", "metadata_sha256"),
                         ("completed.json", "completed_sha256"),
                         ("results.csv", "results_csv_sha256")):
        if sha256(frozen / name) != reference[pinned]:
            raise AssertionError(f"pre-existing reference fixture changed: {name}")
    reporter = SOURCE_REPO / "scripts/summarize-cpu-benchmarks.py"
    if sha256(reporter) != reference["reporter_sha256"]:
        raise AssertionError("existing reporter changed since prospective registration")

    original = load_runner(RUNNER_SOURCE, "cpu_original")
    request = json.loads(REQUEST.read_text())
    original.validate_request(request)
    frozen_prefix = [tuple(item[key] for key in original.CHECKPOINT_KEYS)
                     for item in contract["append_only_checkpoint_history"]["immutable_prefix"]]
    if tuple(frozen_prefix) != original.IMMUTABLE_PREFIX:
        raise AssertionError("runner's pinned history differs from published acceptance prefix")
    if [tuple(item[key] for key in original.CHECKPOINT_KEYS) for item in
            request["checkpoints"][:len(frozen_prefix)]] != frozen_prefix:
        raise AssertionError("request no longer begins with the published seven checkpoints")
    later = copy.deepcopy(request)
    later["checkpoints"].append({"label": "future-audio", "source_commit": "f" * 40,
                                 "benchmark_blob": "92ea7e0b1801bf98fccaed21c13e676890039c94"})
    original.validate_request(later)
    checkpoint = request["checkpoints"][4]
    commit = checkpoint["source_commit"]
    assert commit == json.loads((frozen / "metadata.json").read_text())["source_commit_caller_verified"]
    original.validate_round(frozen, commit)
    built = ["first-immutable-checkpoint", "second-immutable-checkpoint"]
    expected_order = [(round_number, label) for round_number in (1, 2, 3) for label in built]
    if list(original.timing_schedule(built)) != expected_order:
        raise AssertionError("original timing scheduler violated prospective registration")

    # Each source modification is applied to a fresh copy of the original code.
    # The unmodified positive control is run again on the mutated module; a
    # syntactic error, crash, timeout, or unrelated exception is NEVER a kill.
    cases = [
        ("request_one_round", 'if type(request.get(key)) is not type(expected) or request[key] != expected:',
         'if False:', "request", lambda data: data.__setitem__("rounds", 1)),
        ("request_short_commit", 'if not re.fullmatch(r"[0-9a-f]{40}", commit) or not re.fullmatch(r"[0-9a-f]{40}", blob):',
         'if False:', "request", lambda data: data["checkpoints"].append({
             "label": "short-source-revision", "source_commit": "db24a47",
             "benchmark_blob": "92ea7e0b1801bf98fccaed21c13e676890039c94"})),
        ("request_duplicate_label", 'if label in labels or commit in commits:',
         'if False:', "request", lambda data: data["checkpoints"].append({
             "label": data["checkpoints"][0]["label"], "source_commit": "f" * 40,
             "benchmark_blob": "92ea7e0b1801bf98fccaed21c13e676890039c94"})),
        ("request_wrong_reporter_hash", 'if not re.fullmatch(r"[0-9a-f]{64}", request.get("reporter_sha256", "")):',
         'if False:', "request", lambda data: data.__setitem__("reporter_sha256", "66f610e")),
        # Isolate the length guard even when the live request has appended
        # checkpoints; shifting an extension into the prefix instead would
        # exercise the independent identity/order guard in this mutant.
        ("frozen_prefix_shortened", 'if len(checkpoints) < len(IMMUTABLE_PREFIX):',
         'if False:', "request", lambda data: data.__setitem__(
             "checkpoints", data["checkpoints"][:len(frozen_prefix) - 1])),
        ("frozen_prefix_rewritten", 'if tuple(checkpoint.get(key) for key in CHECKPOINT_KEYS) != expected:',
         'if False:', "request", lambda data: data["checkpoints"][3].__setitem__("source_commit", "e" * 40)),
        ("schedule_two_rounds", 'range(1, FIXED_REQUEST["rounds"] + 1)',
         'range(1, FIXED_REQUEST["rounds"])', "schedule", None),
        ("schedule_reverse_order", 'for item in built:', 'for item in reversed(built):', "schedule", None),
        ("metadata_commit", 'metadata["source_commit_caller_verified"] != commit',
         'False', "fixture", lambda directory: alter_json(directory, "metadata.json", "source_commit_caller_verified", "f" * 40)),
        ("metadata_suite", 'metadata["suite"] != "baseline"',
         'False', "fixture", lambda directory: alter_json(directory, "metadata.json", "suite", "phrase")),
        ("metadata_quick", 'metadata["quick_subset"]',
         'False', "fixture", lambda directory: alter_json(directory, "metadata.json", "quick_subset", True)),
        ("metadata_callbacks", 'metadata["callbacks_per_held_phase"] != 1024',
         'False', "fixture", lambda directory: alter_json(directory, "metadata.json", "callbacks_per_held_phase", 128)),
        ("metadata_hardware_claim", 'metadata["hardware_equivalence_or_realtime_qualification"] is not False',
         'False', "fixture", lambda directory: alter_json(directory, "metadata.json", "hardware_equivalence_or_realtime_qualification", True)),
        ("completed_false", 'not complete.get("completed")',
         'False', "fixture", lambda directory: alter_json(directory, "completed.json", "completed", False)),
        ("completed_26_cases", 'complete["cases"] != 27',
         'False', "fixture", lambda directory: alter_json(directory, "completed.json", "cases", 26)),
        ("completed_50_rows", 'complete["rows"] != 51',
         'False', "fixture", lambda directory: alter_json(directory, "completed.json", "rows", 50)),
        ("csv_50_rows", ('len(rows) != 51', 'if len(keys) != 51:'),
         ('False', 'if False:'), "fixture", lambda directory: alter_csv(directory, lambda rows: rows.pop())),
        ("csv_duplicate_fixture", 'if len(keys) != 51:',
         'if False:', "fixture", lambda directory: alter_csv(directory, lambda rows: rows.__setitem__(0, rows[1].copy()))),
        ("csv_rate_44100", 'int(row["rate"]) != 48000',
         'False', "fixture", lambda directory: alter_csv(directory, lambda rows: rows[0].__setitem__("rate", "44100"))),
        ("csv_block_128", 'int(row["block"]) != 256',
         'False', "fixture", lambda directory: alter_csv(directory, lambda rows: rows[0].__setitem__("block", "128"))),
        ("csv_nan_median", 'not math.isfinite(float(row[key]))',
         'False', "fixture", lambda directory: alter_csv(directory, lambda rows: rows[0].__setitem__("median_rtf", "nan"))),
        ("csv_nonfinite_samples", 'int(row["nonfinite_count"]) != 0',
         'False', "fixture", lambda directory: alter_csv(directory, lambda rows: rows[0].__setitem__("nonfinite_count", "1"))),
        ("reporter_hash", 'if digest(path) != requested_sha256:',
         'if False:', "provenance", None),
        ("benchmark_blob", 'if read_command(["git", "rev-parse", commit + ":" + BENCHMARK_SOURCE], repo) != checkpoint["benchmark_blob"]:',
         'if False:', "provenance", None),
        ("binary_sha", 'if digest(binary) != expected_sha256:',
         'if False:', "provenance", None),
        ("source_commit", 'if read_command(["git", "rev-parse", "HEAD"], source) != expected_commit:',
         'if False:', "provenance", None),
        ("dirty_source", 'if read_command(["git", "status", "--porcelain"], source):',
         'if False:', "provenance", None),
    ]

    source = RUNNER_SOURCE.read_text()
    with tempfile.TemporaryDirectory(prefix="cs80-cpu-gate-mutants-") as directory:
        scratch = Path(directory)
        for index, (name, old, new, kind, edit) in enumerate(cases, 1):
            changed = source
            anchors = (old,) if isinstance(old, str) else old
            replacements = (new,) if isinstance(new, str) else new
            for anchor, replacement in zip(anchors, replacements):
                changed = one_replace(changed, anchor, replacement)
            mutant_source = scratch / f"mutant-{index:02d}.py"
            mutant_source.write_text(changed)
            mutant = load_runner(mutant_source, f"cpu_mutant_{index:02d}")
            if kind == "request":
                request_witness(mutant, original, request, edit)
            elif kind == "schedule":
                if list(mutant.timing_schedule(built)) == expected_order:
                    raise AssertionError("schedule mutation survived")
            elif kind == "fixture":
                test_input = scratch / f"fixture-{index:02d}"
                fixture_copy(frozen, test_input)
                fixture_witness(mutant, original, test_input, commit, edit)
            elif kind == "provenance":
                provenance_witness(name, mutant, original, reporter, checkpoint, scratch)
            else:
                raise AssertionError(f"unexpected witness kind {kind}")
            print(f"KILLED {index:02d}/{len(cases):02d} {name}")

    # Re-evaluate the same real positive controls against unchanged source.
    clean = load_runner(RUNNER_SOURCE, "cpu_restored")
    clean.validate_request(request)
    clean.validate_round(frozen, commit)
    if list(clean.timing_schedule(built)) != expected_order:
        raise AssertionError("restored source did not satisfy registered timing schedule")
    print(f"PASS: {len(cases)} compiled source mutants killed; sealed benchmark fixture unchanged")


if __name__ == "__main__":
    run()
