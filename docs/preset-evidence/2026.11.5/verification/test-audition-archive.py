#!/usr/bin/env python3
"""Offline boundaries only: synthetic fixtures do not qualify delivered audio."""
import copy
import csv
import importlib.util
import io
import json
import math
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
REPO = HERE.parents[3]
SPEC = importlib.util.spec_from_file_location("audition_archive", HERE / "verify-audition-archive.py")
helper = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(helper)


def fixture_inventory():
    result = json.loads((HERE.parent / "baseline/inventory.json").read_text())
    result["library_revision"] = "2026.11.5"
    result["parameter_ids"] += ["body1_variant", "body2_variant"]
    for program in result["programs"]:
        program["values"] += [0, 0]
    return result


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
    def test_roster_is_171_clips_81_names_40_groups_without_claiming_acceptance(self):
        reference = fixture_inventory()
        source, tree = "1" * 40, "2" * 40
        requested = []
        def git(command):
            requested.append(command)
            self.assertEqual(command[:3], ["git", "-C", str(REPO)])
            if command[3:] == ["rev-parse", source + "^{tree}"]:
                return (tree + "\n").encode()
            self.assertEqual(command[3], "show")
            self.assertTrue(command[4].startswith(source + ":"))
            return (REPO / command[4].split(":", 1)[1]).read_bytes()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"
            path.write_text(json.dumps(reference))
            with mock.patch.object(helper.subprocess, "check_output", side_effect=git):
                producer, retained, raw, files, contract = helper.source_contract(REPO, source, path)
        self.assertEqual(contract["expected"], {
            "wav_count": 171, "selected_program_count": 81, "audio_seconds": 1033.2,
            "folder_count": 40, "regular_files": 216, "checksums": 215, "parameters": 202,
            "programs": 523, "stereo_frames": 49593600})
        self.assertEqual(len(files), 216)
        self.assertEqual(len([path for path in files if path.endswith(".wav")]), 171)
        self.assertEqual(len(requested), 5)
        self.assertNotIn("accepted_archive_content", contract)
        # The fixture remains artificial: these copied baseline controls are
        # only a structurally valid inventory, not the current sound data.
        self.assertEqual(retained, reference)

    def test_wrong_source_revision_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.json"
            path.write_text(json.dumps(fixture_inventory()))
            def wrong_git(command):
                if command[3] == "rev-parse":
                    return ("2" * 40 + "\n").encode()
                name = command[4].split(":", 1)[1]
                data = (REPO / name).read_bytes()
                return data.replace(b'curatedLibraryRevision = "2026.11.5"',
                                    b'curatedLibraryRevision = "2026.11.4"')
            with mock.patch.object(helper.subprocess, "check_output", side_effect=wrong_git):
                with self.assertRaisesRegex(ValueError, "not 2026.11.5"):
                    helper.source_contract(REPO, "1" * 40, path)

    def test_inventory_equality_is_binary32_and_includes_metadata(self):
        reference = fixture_inventory()
        self.assertEqual(helper.compare_inventory(copy.deepcopy(reference), reference)["values"], 105646)
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
                    p["values"][-1] = -0.0
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
            argv = ["verify-audition-archive.py", "--source", "1" * 40,
                    "--expected-inventory", str(path),
                    "--expected-inventory-sha256", expected_digest,
                    "--prepare", "--out", str(out)]
            with mock.patch.object(sys, "argv", argv), \
                    mock.patch.object(helper, "source_contract", side_effect=replace_at_contract):
                with self.assertRaisesRegex(ValueError, "Expected inventory hash mismatch"):
                    helper.main()
            self.assertFalse(out.exists())

    def test_archive_manifest_gates_reject_transfer_faults_without_audio_acceptance(self):
        # Complete filenames/manifests with intentionally invalid WAV payloads.
        # The unchanged control must reach the WAV-format rejection; it must
        # never be reported as an accepted artifact.
        producer = types.ModuleType("fixture_roster")
        producer.__file__ = str(REPO / helper.CAPTURE_SCRIPT)
        exec(compile((REPO / helper.CAPTURE_SCRIPT).read_bytes(), producer.__file__, "exec"),
             producer.__dict__)
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
                          repository=helper.REPOSITORY, run_id="12", run_attempt="1",
                          tracked_source_clean_before_capture=True, tracked_source_clean_after_capture=True,
                          capture_binary_sha256="3" * 64, library_revision=helper.REVISION, selections=groups,
                          expected_wav_count=171, wav_count=171, selected_program_count=81,
                          expected_audio_seconds=1033.2, audio_seconds=1033.2,
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
        ]
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
