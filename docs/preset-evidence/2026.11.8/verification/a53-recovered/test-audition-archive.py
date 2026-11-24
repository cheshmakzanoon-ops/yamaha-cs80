#!/usr/bin/env python3
"""Offline boundaries only: synthetic fixtures do not qualify delivered audio."""
import copy
import csv
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import stat
import struct
import sys
import tempfile
import types
import unittest
import warnings
import zipfile
from unittest import mock

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = Path(os.environ["CS80_TEST_REPO"])
SPEC = importlib.util.spec_from_file_location("audition_archive", HERE / "verify-audition-archive.py")
helper = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(helper)


SOURCE = helper.SOURCE
TREE = helper.TREE
OLD_SOURCE = "ee0dabcacb4cdff93f8803844f60348e0ceda6ed"
INVENTORY_PATH = Path(os.environ["CS80_TEST_INVENTORY"])
RECEIPT_PATH = Path(os.environ["CS80_TEST_RECEIPT"])
INVENTORY_SOURCE_PATH = None
INVENTORY_SHA256 = helper.INVENTORY_SHA256


def fixture_inventory():
    # Real retained reference bytes, but synthetic archives remain invalid audio.
    return json.loads(INVENTORY_PATH.read_text())


def source_bytes(path, source=SOURCE):
    return helper.subprocess.run(["git", "-C", str(REPO), "show", source + ":" + path],
                                 check=True, capture_output=True).stdout


def source_producer():
    producer = types.ModuleType("source_roster_for_boundary_tests")
    producer.__file__ = str(REPO / helper.CAPTURE_SCRIPT)
    exec(compile(source_bytes(helper.CAPTURE_SCRIPT), producer.__file__, "exec"), producer.__dict__)
    return producer


def metric_row(program, group):
    default_note = (48 if program["category"] == "Bass" else 67
                    if program["category"] in ("Winds", "Leads") else 72
                    if program["category"] == "Bells" else 60)
    row = {key: "0" for key in helper.CSV_FIELDS}
    row.update(index=str(program["host_index"]), name=program["name"], category=program["category"],
               note=str(group.get("note", default_note)), sample_rate="48000",
               velocity=str(group["velocity"]), pressure=str(group["pressure"]),
               hold_seconds=str(group["hold"]), tail_seconds=str(group["tail"]),
               parameter_digest=format(helper.parameter_digest(program["values"]), "x"))
    return row


def wave(samples, rate=48000, fact=None):
    frames = len(samples) // 2
    payload = struct.pack("<" + "f" * len(samples), *samples)
    fmt = struct.pack("<HHIIHHH", 3, 2, rate, rate * 8, 8, 32, 0)
    chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt
    chunks += b"fact" + struct.pack("<II", 4, frames if fact is None else fact)
    chunks += b"data" + struct.pack("<I", len(payload)) + payload
    return b"RIFF" + struct.pack("<I", len(chunks) + 4) + b"WAVE" + chunks


def archive(entries):
    data = io.BytesIO()
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        with zipfile.ZipFile(data, "w") as output:
            for name, payload, mode in entries:
                info = zipfile.ZipInfo(name)
                info.create_system = 3
                info.external_attr = mode << 16
                output.writestr(info, payload)
    return zipfile.ZipFile(io.BytesIO(data.getvalue()))


class AuditionArchiveBoundaryTests(unittest.TestCase):
    def test_actual_source_roster_is_256_clips_95_names_60_groups_without_acceptance(self):
        producer, retained, raw, files, contract = helper.source_contract(
            REPO, SOURCE, INVENTORY_PATH, INVENTORY_SOURCE_PATH)
        self.assertEqual(contract["source_commit"], SOURCE)
        self.assertEqual(contract["source_tree"], TREE)
        self.assertEqual(helper.sha(raw), INVENTORY_SHA256)
        self.assertEqual(contract["expected"], {
            "wav_count": 256, "selected_program_count": 95, "audio_seconds": 1487.2,
            "folder_count": 60, "regular_files": 321, "checksums": 320, "parameters": 204,
            "programs": 523, "stereo_frames": 71385600, "raw_audio_bytes": 571084800})
        self.assertEqual(len(files), 321)
        self.assertEqual(len([path for path in files if path.endswith(".wav")]), 256)
        self.assertEqual(len(contract["source_files"]), 4)
        self.assertNotIn("accepted_archive_content", contract)
        self.assertEqual(retained, fixture_inventory())
        self.assertEqual(helper.roster_sha(producer.selections()[:54]), helper.PRESERVED_ROSTER_SHA256)
        self.assertEqual(helper.roster_sha(producer.selections()[54:]), helper.APPENDED_ROSTER_SHA256)

    def test_obsolete_238_source_roster_is_rejected_even_with_current_revision(self):
        def old_roster_git(command):
            if command[3] == "rev-parse":
                return (TREE + "\n").encode()
            path = command[4].split(":", 1)[1]
            data = source_bytes(path)
            if path == helper.CAPTURE_SCRIPT:
                data += b"\n_full_selections = selections\ndef selections():\n    return _full_selections()[:54]\n"
            return data
        with mock.patch.object(helper.subprocess, "check_output", side_effect=old_roster_git):
            with self.assertRaisesRegex(ValueError, "reviewed .8 roster"):
                helper.source_contract(REPO, SOURCE, INVENTORY_PATH)

    def test_source_wrong_added_settings_with_identical_counts_are_rejected(self):
        suffix = b"\n_reviewed_selections = selections\ndef selections():\n    groups = _reviewed_selections()\n    groups[-1]['pressure'] = 0.5\n    return groups\n"
        def wrong_settings_git(command):
            if command[3] == "rev-parse":
                return (TREE + "\n").encode()
            path = command[4].split(":", 1)[1]
            data = source_bytes(path)
            return data + suffix if path == helper.CAPTURE_SCRIPT else data
        with mock.patch.object(helper.subprocess, "check_output", side_effect=wrong_settings_git):
            with self.assertRaisesRegex(ValueError, "ordered requests differ"):
                helper.source_contract(REPO, SOURCE, INVENTORY_PATH)

    def test_wrong_source_revision_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"
            path.write_text(json.dumps(fixture_inventory()))
            def wrong_git(command):
                if command[3] == "rev-parse":
                    return (TREE + "\n").encode()
                name = command[4].split(":", 1)[1]
                data = helper.subprocess.run(["git", "-C", str(REPO), "show", SOURCE + ":" + name], check=True, capture_output=True).stdout
                return data.replace(b'curatedLibraryRevision = "2026.11.8"',
                                    b'curatedLibraryRevision = "2026.11.4"')
            with mock.patch.object(helper.subprocess, "check_output", side_effect=wrong_git):
                with self.assertRaisesRegex(ValueError, "not 2026.11.8"):
                    helper.source_contract(REPO, SOURCE, path)

    def test_inventory_equality_is_binary32_and_includes_metadata(self):
        reference = fixture_inventory()
        self.assertEqual(helper.compare_inventory(copy.deepcopy(reference), reference)["values"], 106692)
        for mutation in ("name", "description", "tag", "host_index", "value", "signed_zero",
                         "variant_id", "short_vector", "nonfinite", "boolean"):
            with self.subTest(mutation=mutation):
                candidate = copy.deepcopy(reference)
                p = candidate["programs"][0]
                if mutation == "name":
                    p["name"] += " changed"
                elif mutation == "description":
                    p["description"] += " changed"
                elif mutation == "tag":
                    p["tags"].append("unrequested")
                elif mutation == "host_index":
                    p["host_index"] = 1
                elif mutation == "value":
                    p["values"][0] += 0.01
                elif mutation == "signed_zero":
                    p["values"][p["values"].index(0.0)] = -0.0
                elif mutation == "variant_id":
                    candidate["parameter_ids"][-1] = "variant_wrong"
                elif mutation == "short_vector":
                    p["values"].pop()
                elif mutation == "nonfinite":
                    p["values"][0] = float("nan")
                else:
                    p["values"][0] = True
                with self.assertRaises((ValueError, OverflowError)):
                    helper.compare_inventory(candidate, reference)

    def test_duplicate_and_nonfinite_json_are_rejected(self):
        for value in (b'{"a":1,"a":2}', b'{"a":NaN}', b'{"a":Infinity}'):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    helper.parse_json(value)

    def test_zip_rejects_every_entry_type_and_path_boundary_before_reads(self):
        regular = stat.S_IFREG | 0o644
        valid = [("provenance.json", b"{}", regular), ("group/x.wav", b"x", regular),
                 ("group/", b"", stat.S_IFDIR | 0o755)]
        with archive(valid) as zipped:
            prefix, files, entries = helper.preflight(zipped)
            self.assertEqual(prefix, "")
            self.assertEqual(files, {"provenance.json", "group/x.wav"})
        bad_entries = [
            [("../escape", b"x", regular)],
            [("/absolute", b"x", regular)],
            [("group//x", b"x", regular)],
            [("group/./x", b"x", regular)],
            [("group/../x", b"x", regular)],
            [("group\\x", b"x", regular)],
            [("C:drive", b"x", regular)],
            [("group/x. ", b"x", regular)],
            [("symlink", b"target", stat.S_IFLNK | 0o777)],
            [("socket", b"", stat.S_IFSOCK | 0o600)],
            [("directory/", b"", regular)],
            [("regular", b"", stat.S_IFDIR | 0o755)],
            [("unused/", b"", stat.S_IFDIR | 0o755)],
            [("group", b"x", regular)],
            [("provenance.json", b"{}", regular)],
            [("PROVENANCE.JSON", b"{}", regular)],
        ]
        for extra in bad_entries:
            with self.subTest(extra=extra[0][0]):
                with archive(valid + extra) as zipped:
                    with self.assertRaises(ValueError):
                        helper.preflight(zipped)

    def test_checksums_require_every_member_once(self):
        files = {"SHA256SUMS", "provenance.json", "group/x.wav"}
        good = ("0" * 64 + "  provenance.json\n" + "1" * 64 + "  group/x.wav\n").encode()
        self.assertEqual(len(helper.checksum_manifest(good, files)), 2)
        for bad in (good.splitlines(keepends=True)[0], good + good.splitlines(keepends=True)[0],
                    good + ("2" * 64 + "  extra\n").encode(),
                    good.replace(b"  group", b" group")):
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    helper.checksum_manifest(bad, files)

    def test_float_wav_metrics_recover_known_samples(self):
        left = [.25, -.25, .5, -.5, .125, -.125, 0, 0]
        stereo = [sample for value in left for sample in (value, -value)]
        result = helper.inspect_wav(wave(stereo), 8, 8)
        self.assertAlmostEqual(result["peak_dbfs"], 20 * math.log10(.5), places=12)
        self.assertAlmostEqual(result["rms_dbfs"], 20 * math.log10(math.sqrt(sum(v*v for v in left) / 8)), places=12)
        self.assertAlmostEqual(result["stereo_correlation"], -1, places=12)
        self.assertEqual(result["tail_rms_dbfs"], -240)
        zero = helper.inspect_wav(wave([0] * 16), 8, 4)
        self.assertEqual(zero["rms_dbfs"], -240)
        self.assertEqual(zero["peak_window_seconds"], 0)
        self.assertEqual(zero["centroid_hz"], 0)

    def test_float_wav_rejects_format_length_and_sample_corruption(self):
        good = wave([.1, -.1] * 8)
        bad = [b"invalid", good[:-1], good + b"x", wave([.1, -.1] * 8, rate=44100),
               wave([.1, -.1] * 8, fact=7), wave([float("nan"), 0] * 8),
               wave([float("inf"), 0] * 8), wave([1.0001, 0] * 8),
               good[:20] + struct.pack("<H", 1) + good[22:],
               good[:12] + good[12:38] + good[12:]]
        for index, data in enumerate(bad):
            with self.subTest(index=index):
                with self.assertRaises(ValueError):
                    helper.inspect_wav(data, 8, 4)
        with self.assertRaises(ValueError):
            helper.inspect_wav(good, 9, 4)

    def test_each_appended_request_rejects_wrong_metric_identity_settings_and_digest(self):
        by_name = helper.validate_inventory(fixture_inventory())
        checked = 0
        for group in source_producer().selections()[54:]:
            for name in group["names"]:
                program = by_name[name]
                row = metric_row(program, group)
                settings = helper.check_metric_identity_and_settings(row, program, group)
                self.assertEqual(settings["note"], group["note"])
                checked += 1
                for key in ("name", "index", "category", "note", "velocity", "pressure", "hold_seconds",
                            "tail_seconds", "sample_rate", "rank", "nonfinite", "samples_over_full_scale",
                            "parameter_digest"):
                    with self.subTest(folder=group["folder"], name=name, field=key):
                        changed = dict(row)
                        changed[key] = ("incorrect" if key in ("name", "category") else
                                        "0" if key == "parameter_digest" else str(float(row[key]) + 1))
                        with self.assertRaises(ValueError):
                            helper.check_metric_identity_and_settings(changed, program, group)
        self.assertEqual(checked, 18)

    def test_every_pcm_metric_is_independently_compared_without_archive_acceptance(self):
        measured = helper.inspect_wav(wave([.25, -.25, -.5, .5, .125, -.125, 0, 0] * 2), 8, 4)
        row = {key: format(measured[key], ".10g") for key in helper.MEASURED_FIELDS}
        helper.compare_wav_metrics(row, measured, "small-boundary-fixture.wav")
        for key in helper.MEASURED_FIELDS:
            for value in (str(measured[key] + .01), "nan", "inf"):
                with self.subTest(field=key, value=value):
                    changed = dict(row)
                    changed[key] = value
                    with self.assertRaisesRegex(ValueError, "WAV/CSV metric differs"):
                        helper.compare_wav_metrics(changed, measured, "small-boundary-fixture.wav")

    def test_report_never_overwrites_existing_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / "report.json"
            out.write_text("sentinel")
            with self.assertRaises(FileExistsError):
                helper.write_fresh_report(out, {"accepted_archive_content": True})
            self.assertEqual(out.read_text(), "sentinel")
            new = Path(directory) / "new.json"
            helper.write_fresh_report(new, {"status": "fixture_only", "accepted_archive_content": False})
            self.assertFalse(json.loads(new.read_text())["accepted_archive_content"])

    def test_archive_digest_and_inspection_share_an_immutable_snapshot(self):
        # Swap the path immediately after SHA-256 is finalized. The archive
        # reader must still inspect the bytes that produced the expected hash.
        # Stop at preflight: this is a file-identity boundary test, not an
        # accepted-audio fixture or a substitute for the real archive gates.
        def zipped(marker):
            output = io.BytesIO()
            with zipfile.ZipFile(output, "w") as archive:
                archive.writestr("marker", marker)
            return output.getvalue()
        first, replacement = zipped(b"A"), zipped(b"B")
        real_sha256 = helper.hashlib.sha256
        expected_digest = real_sha256(first).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "changing.zip"
            path.write_bytes(first)
            swapped, observed = [], []
            class SwappingDigest:
                def __init__(self, *args, **kwargs):
                    self.digest = real_sha256(*args, **kwargs)
                def update(self, value):
                    self.digest.update(value)
                def hexdigest(self):
                    value = self.digest.hexdigest()
                    if value == expected_digest and not swapped:
                        path.write_bytes(replacement)
                        swapped.append(True)
                    return value
            def stop_at_preflight(archive):
                observed.append(archive.read("marker"))
                raise RuntimeError("controlled preflight stop")
            reference = fixture_inventory()
            args = types.SimpleNamespace(archive=path, expected_zip_sha256=expected_digest,
                                         expected_zip_bytes=len(first), run_id="12", attempt="1")
            with mock.patch.object(helper.hashlib, "sha256", SwappingDigest), \
                    mock.patch.object(helper, "preflight", side_effect=stop_at_preflight):
                with self.assertRaisesRegex(RuntimeError, "controlled preflight stop"):
                    helper.accept_archive(args, None, reference, json.dumps(reference).encode(),
                                          set(), {})
            self.assertTrue(swapped)
            self.assertEqual(observed, [b"A"])
            self.assertEqual(path.read_bytes(), replacement)

    def test_expected_inventory_digest_binds_the_bytes_actually_loaded(self):
        # Simulate a replacement between the former early hash check and the
        # contract loader. Even prepare-only mode must reject the mismatched
        # loaded reference and leave no report behind.
        first = json.dumps(fixture_inventory()).encode()
        changed = fixture_inventory()
        changed["programs"][0]["values"][0] += 0.01
        replacement = json.dumps(changed).encode()
        expected_digest = helper.sha(first)
        with tempfile.TemporaryDirectory() as directory:
            path, out = Path(directory) / "inventory.json", Path(directory) / "report.json"
            path.write_bytes(first)
            def replace_at_contract(*args, **kwargs):
                path.write_bytes(replacement)
                return None, changed, replacement, set(), {
                    "expected_inventory_sha256": helper.sha(replacement)}
            argv = ["verify-audition-archive.py", "--repo", str(REPO), "--source", SOURCE,
                    "--run-id", helper.RUN, "--attempt", helper.ATTEMPT,
                    "--independent-capture-receipt", str(RECEIPT_PATH),
                    "--expected-inventory", str(path),
                    "--expected-inventory-sha256", expected_digest,
                    "--prepare", "--out", str(out)]
            with mock.patch.object(sys, "argv", argv), \
                    mock.patch.object(helper, "source_contract", side_effect=replace_at_contract):
                with self.assertRaisesRegex(ValueError, "inventory hash mismatch"):
                    helper.main()
            self.assertFalse(out.exists())

    def test_archive_manifest_gates_reject_transfer_faults_without_audio_acceptance(self):
        # Complete filenames/manifests with intentionally invalid WAV payloads.
        # The unchanged control must reach the WAV-format rejection; it must
        # never be reported as an accepted artifact.
        producer = source_producer()
        reference = fixture_inventory()
        reference_bytes = json.dumps(reference).encode()
        by_name = helper.validate_inventory(reference)
        groups = producer.selections()
        files = helper.make_expected_files(groups, by_name)
        source, tree = "1" * 40, "2" * 40
        binary, out = "/fixture/build/cs80_preset_capture", "/fixture/preset-audition"
        commands = []
        for group in groups:
            command = [binary, "--library", "all", "--out", out + "/" + group["folder"], "--wav",
                       "--rate", "48000", "--velocity", str(group["velocity"]),
                       "--pressure", str(group["pressure"]), "--hold", str(group["hold"]),
                       "--tail", str(group["tail"])]
            if "note" in group:
                command += ["--note", str(group["note"])]
            for name in group["names"]:
                command += ["--preset", name]
            commands.append(command)
        provenance = dict(schema=3, capture_status="complete", source_commit=source, source_tree=tree,
                          repository=helper.REPOSITORY, run_id="12", run_attempt="1", platform="Linux-fixture",
                          tracked_source_clean_before_capture=True, tracked_source_clean_after_capture=True,
                          capture_binary_sha256="3" * 64, library_revision=helper.REVISION, selections=groups,
                          expected_wav_count=256, wav_count=256, selected_program_count=95,
                          expected_audio_seconds=1487.2, audio_seconds=1487.2,
                          inventory_command=[binary, "--library", "all", "--inventory", out + "/inventory.json"],
                          capture_commands=commands)
        base = {path: b"intentionally invalid WAV" for path in files}
        base["source-commit.txt"] = (source + "\n").encode()
        base["inventory.json"] = reference_bytes
        base["README.txt"] = producer.audition_readme(groups, helper.EXPECTED, source, helper.REVISION).encode()
        for group in groups:
            stream = io.StringIO(newline="")
            writer = csv.DictWriter(stream, fieldnames=helper.CSV_FIELDS)
            writer.writeheader()
            for name in group["names"]:
                program = by_name[name]
                note = (48 if program["category"] == "Bass" else 67
                        if program["category"] in ("Winds", "Leads") else 72
                        if program["category"] == "Bells" else 60)
                row = {key: 0 for key in helper.CSV_FIELDS}
                row.update(index=program["host_index"], name=name, category=program["category"],
                           note=group.get("note", note), sample_rate=48000, velocity=group["velocity"],
                           pressure=group["pressure"], hold_seconds=group["hold"], tail_seconds=group["tail"],
                           parameter_digest=format(helper.parameter_digest(program["values"]), "x"))
                writer.writerow(row)
            base[group["folder"] + "/metrics.csv"] = stream.getvalue().encode()
        faults = [
            (None, "Invalid RIFF"),
            ("digest", "external expected digest"),
            ("capture_status", "not complete"),
            ("unknown_manifest_field", "Unexpected provenance fields"),
            ("wrong_platform", "producer platform"),
            ("source_commit", "source identity"),
            ("source_tree", "source identity"),
            ("run_id", "run/attempt"),
            ("run_attempt", "run/attempt"),
            ("tracked_source_clean_before_capture", "not clean"),
            ("tracked_source_clean_after_capture", "not clean"),
            ("capture_binary_sha256", "executable hash"),
            ("selections", "revision/selections"),
            ("inventory", "binary32 values differ"),
            ("commands", "Capture commands differ"),
            ("readme", "README differs"),
            ("checksum", "Checksum mismatch"),
            ("missing_material", "source roster"),
            ("stale171", "Wrong provenance"),
            ("stale238", "Wrong provenance"),
            ("obsolete238", "source roster"),
            ("stale202", "204 unique"),
        ]
        appended_wavs = sorted(path for path in files if path.endswith(".wav")
                               and path.startswith("voice-register-"))
        self.assertEqual(len(appended_wavs), 18)
        faults += [("missing_added:" + path, "source roster") for path in appended_wavs]
        faults += [("wrong_added_filename:" + path, "source roster") for path in appended_wavs]
        faults += [("wrong_added_settings:" + group["folder"], "revision/selections")
                   for group in groups[54:]]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.zip"
            for fault, expected_error in faults:
                with self.subTest(fault=fault):
                    members, metadata = copy.deepcopy(base), copy.deepcopy(provenance)
                    if fault in ("source_commit", "source_tree"):
                        metadata[fault] = "f" * 40
                    elif fault in ("run_id", "run_attempt"):
                        metadata[fault] = "99"
                    elif fault and fault.startswith("tracked_"):
                        metadata[fault] = False
                    elif fault == "unknown_manifest_field":
                        metadata["error"] = "synthetic incomplete capture"
                    elif fault == "wrong_platform":
                        metadata["platform"] = "Darwin-fixture"
                    elif fault == "capture_status":
                        metadata[fault] = "started"
                    elif fault == "capture_binary_sha256":
                        metadata[fault] = "bad"
                    elif fault == "selections":
                        metadata[fault][0]["velocity"] = .1
                    elif fault == "commands":
                        metadata["capture_commands"][0].append("--normalize")
                    elif fault == "inventory":
                        changed = copy.deepcopy(reference)
                        changed["programs"][0]["values"][0] += .01
                        members["inventory.json"] = json.dumps(changed).encode()
                    elif fault == "missing_material":
                        key = next(key for key in members if key.startswith("material-profile-") and key.endswith(".wav"))
                        del members[key]
                    elif fault and fault.startswith("missing_added:"):
                        del members[fault.split(":", 1)[1]]
                    elif fault and fault.startswith("wrong_added_filename:"):
                        key = fault.split(":", 1)[1]
                        members[key[:-4] + "-wrong.wav"] = members.pop(key)
                    elif fault and fault.startswith("wrong_added_settings:"):
                        folder = fault.split(":", 1)[1]
                        next(group for group in metadata["selections"] if group["folder"] == folder)["note"] += 1
                    elif fault == "obsolete238":
                        # Remove all six new groups and use internally consistent
                        # old counts/manifests/checksums. Filename acceptance must
                        # reject this before reading any intentionally invalid PCM.
                        members = {key: value for key, value in members.items()
                                   if not key.startswith("voice-register-")}
                        metadata["selections"] = metadata["selections"][:54]
                        metadata["capture_commands"] = metadata["capture_commands"][:54]
                        metadata.update(expected_wav_count=238, wav_count=238, selected_program_count=92,
                                        expected_audio_seconds=1361.2, audio_seconds=1361.2)
                        members["README.txt"] = producer.audition_readme(
                            groups[:54], producer.roster_summary(groups[:54]), source, helper.REVISION).encode()
                        self.assertEqual(sum(key.endswith(".wav") for key in members), 238)
                        self.assertEqual(len(members), 297)
                    elif fault == "stale238":
                        metadata.update(expected_wav_count=238, wav_count=238)
                    elif fault == "stale171":
                        metadata["expected_wav_count"] = 171
                        metadata["wav_count"] = 171
                    elif fault == "stale202":
                        changed = copy.deepcopy(reference)
                        changed["parameter_ids"] = changed["parameter_ids"][:202]
                        for program in changed["programs"]:
                            program["values"] = program["values"][:202]
                        members["inventory.json"] = json.dumps(changed).encode()
                    elif fault == "readme":
                        members["README.txt"] += b"changed"
                    members["provenance.json"] = json.dumps(metadata).encode()
                    members["SHA256SUMS"] = "".join(
                        helper.sha(data) + "  " + name + "\n" for name, data in sorted(members.items())
                        if name != "SHA256SUMS").encode()
                    if fault == "checksum":
                        members["source-commit.txt"] += b"changed"
                    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as output:
                        for name, data in members.items():
                            output.writestr(name, data)
                    args = types.SimpleNamespace(archive=path, expected_zip_sha256=helper.file_sha(path),
                                                 expected_zip_bytes=path.stat().st_size,
                                                 source=source, run_id="12", attempt="1")
                    if fault == "digest":
                        args.expected_zip_sha256 = "0" * 64
                    with self.assertRaisesRegex(ValueError, expected_error):
                        helper.accept_archive(args, producer, reference, reference_bytes, files,
                                              {"source_tree": tree})


    def test_frozen_identity_and_inventory_reject_wrong_inputs(self):
        with self.assertRaisesRegex(ValueError, "frozen commit"):
            helper.source_contract(REPO, OLD_SOURCE, INVENTORY_PATH)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"
            data = fixture_inventory()
            data["programs"][0]["values"][0] += 0.01
            path.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "frozen independent reference"):
                helper.source_contract(REPO, SOURCE, path)
            receipt = Path(directory) / "receipt.json"
            original = json.loads(RECEIPT_PATH.read_text())
            for fault in ("source", "inventory", "exit", "changed_binary", "fake_run", "producer"):
                candidate = copy.deepcopy(original)
                if fault == "source": candidate["source_commit"] = "0" * 40
                elif fault == "inventory": candidate["inventory"]["sha256"] = "0" * 64
                elif fault == "exit": candidate["inventory"]["exit_code"] = 1
                elif fault == "changed_binary": candidate["capture"]["sha256_after"] = "0" * 64
                elif fault == "fake_run":
                    candidate["execution"] = {"kind": "local_recovery", "platform": "fixture", "python_version": "3", "run_id": 123}
                else: candidate["producer_sha256"] = "0" * 64
                receipt.write_text(json.dumps(candidate))
                with self.subTest(fault=fault), self.assertRaises(ValueError):
                    helper.independent_binding(receipt, INVENTORY_PATH.read_bytes(), INVENTORY_PATH)
        accepted = helper.independent_binding(RECEIPT_PATH, INVENTORY_PATH.read_bytes(), INVENTORY_PATH)
        self.assertFalse(accepted["native_capture_binary_hash_equality_expected"])

    def test_new_payload_budget_accepts_metadata_above_old_512mib_limit(self):
        producer = source_producer()
        by_name = helper.validate_inventory(fixture_inventory())
        names = helper.make_expected_files(producer.selections(), by_name)
        sizes = {name: 100 for name in names}
        for group in producer.selections():
            for name in group["names"]:
                program = by_name[name]
                safe = "".join(chr(b) if chr(b).isascii() and chr(b).isalnum() else "-" for b in name.encode())
                path = f"{group['folder']}/{program['host_index']}-{safe}.wav"
                sizes[path] = round((group["hold"] + group["tail"]) * helper.RATE) * 8 + 58
        infos = []
        for path, size in sizes.items():
            item = zipfile.ZipInfo(path)
            item.file_size = size
            item.compress_size = size
            item.CRC = 0
            item.external_attr = (stat.S_IFREG | 0o644) << 16
            infos.append(item)
        class DirectoryOnly:
            def infolist(self): return infos
        self.assertGreater(sum(sizes.values()), 512 * 1024 * 1024)
        _, files, _ = helper.preflight(DirectoryOnly())
        self.assertEqual(files, names)
        for info in infos:
            info.file_size = helper.MAX_MEMBER - 1
        with self.assertRaisesRegex(ValueError, "Expanded ZIP exceeds bound"):
            helper.preflight(DirectoryOnly())

    def test_exact_full_scale_is_rejected_and_zero_silence_is_explicit(self):
        self.assertTrue(helper.inspect_wav(wave([0.0] * 16), 8, 8)["exact_silence"])
        self.assertFalse(helper.inspect_wav(wave([0.001] * 16), 8, 8)["exact_silence"])
        with self.assertRaisesRegex(ValueError, "full scale"):
            helper.inspect_wav(wave([1.0] * 16), 8, 8)

    def test_local_execution_context_never_invents_an_actions_run(self):
        spec = importlib.util.spec_from_file_location("independent_producer", HERE / "produce-independent-inventory.py")
        producer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(producer)
        with mock.patch.dict(os.environ, {}, clear=True):
            context = producer.execution_context()
        self.assertEqual(context["kind"], "local_recovery")
        self.assertFalse(any(key in context for key in ("run_id", "run_attempt", "workflow_run_id")))
        with mock.patch.dict(os.environ, {"GITHUB_ACTIONS": "true"}, clear=True):
            with self.assertRaisesRegex(ValueError, "Actions context"):
                producer.execution_context()

if __name__ == "__main__":
    unittest.main(verbosity=2)
