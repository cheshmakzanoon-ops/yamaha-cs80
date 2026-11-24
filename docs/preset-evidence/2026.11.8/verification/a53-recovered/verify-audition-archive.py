#!/usr/bin/env python3
"""Inspect actual 2026.11.8 audition ZIP bytes without extraction or playback.

The producer roster is read from the explicit Git source, never from the ZIP.
The independent inventory and external ZIP digest must be supplied by the
caller. Content acceptance is deliberately separate from release qualification.
"""
from __future__ import annotations

import argparse
from array import array
from contextlib import contextmanager
import cmath
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import subprocess
import sys
import tempfile
import types
import zipfile

SOURCE = "ef1c472b161c37c490945f7c469086508aae6aef"
TREE = "f4e00753cde135a36172d9ea549223dfdce5cc82"
RUN = "45439302351"
ATTEMPT = "1"
PRODUCER_JOB = 138605634881
INVENTORY_SHA256 = "399d4c1e8fc1d638f11a22dccb9643a77026acb40ea7fe2a96c66b940938b621"
RECOVERY_EDITION = "a53-independent-capture-successor-1"
REVISION = "2026.11.8"
RATE = 48000
PARAMETERS = 204
PROGRAMS = 523
EXPECTED = dict(wav_count=256, selected_program_count=95, audio_seconds=1487.2)
GROUP_COUNT = 60
FILE_COUNT = 321
RAW_AUDIO_BYTES = 571084800
ROSTER_SHA256 = "09e60ba3a5756d2403d56109413a37b51744d0809dfc2a9e7153e99f00627714"
PRESERVED_ROSTER_SHA256 = "889f32cc511e7ae693803a4aec8118b2ac09f147cf2886a56af966e16115871a"
APPENDED_ROSTER_SHA256 = "8fd4f3e91593dbb5ed4c09b75b7c72fed770d88db8b3e8126f0c0189a4c84bcc"
# Exact reviewed PCM payload plus bounded metadata/ZIP overhead.
# The previous 512 MiB expanded limit is smaller than this 256-clip payload.
MAX_ARCHIVE = RAW_AUDIO_BYTES + 16 * 1024 * 1024
MAX_EXPANDED = RAW_AUDIO_BYTES + 8 * 1024 * 1024
MAX_MEMBER = 16 * 1024 * 1024
REPOSITORY = "cheshmakzanoon-ops/yamaha-cs80"
CAPTURE_SCRIPT = "scripts/capture-factory-auditions.py"
CSV_FIELDS = ("index,name,category,note,sample_rate,velocity,pressure,hold_seconds,"
              "tail_seconds,rank,parameter_digest,peak_dbfs,rms_dbfs,early_rms_dbfs,"
              "body_rms_dbfs,tail_rms_dbfs,peak_window_seconds,centroid_hz,low_power,"
              "mid_power,high_power,tilt,stereo_correlation,nonfinite,"
              "samples_over_full_scale").split(",")
MEASURED_FIELDS = CSV_FIELDS[11:23]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def roster_sha(groups):
    return sha(json.dumps(groups, sort_keys=True, separators=(",", ":"), allow_nan=False).encode())


def file_sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def bounded_snapshot(path, limit, label):
    # Hashing a path and reopening it for inspection lets another writer swap
    # the file between those operations. Retain one bounded, immutable byte
    # snapshot so every subsequent check describes exactly the hashed bytes.
    with path.open("rb") as stream:
        require(stat.S_ISREG(os.fstat(stream.fileno()).st_mode), label + " is not a regular file")
        data = stream.read(limit)
    require(0 < len(data) < limit, label + " size outside bound")
    return data


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON key: " + key)
        result[key] = value
    return result


def parse_json(data):
    def reject(value):
        raise ValueError("Nonfinite JSON constant: " + value)
    return json.loads(data, object_pairs_hook=unique_object, parse_constant=reject)


def parameter_bytes(values):
    require(isinstance(values, list) and len(values) == PARAMETERS,
            "Expected 204 parameter values")
    require(all(type(value) in (int, float) and math.isfinite(value) for value in values),
            "Parameter values must be finite numeric values")
    return struct.pack("<204f", *values)


def parameter_digest(values):
    result = 14695981039346656037
    for byte in parameter_bytes(values):
        result = ((result ^ byte) * 1099511628211) & 0xffffffffffffffff
    return result


def validate_inventory(inventory):
    require(set(inventory) == {"schema", "library_revision", "parameter_ids", "programs"},
            "Unexpected inventory root fields")
    require(type(inventory["schema"]) is int and inventory["schema"] == 1
            and inventory["library_revision"] == REVISION,
            "Unexpected inventory schema/revision")
    ids = inventory["parameter_ids"]
    require(isinstance(ids, list) and len(ids) == PARAMETERS
            and all(isinstance(key, str) and key for key in ids)
            and len(set(ids)) == PARAMETERS, "Expected 204 unique parameter IDs")
    require(ids[190:198] == [f"body{rank}_{control}" for rank in (1, 2)
            for control in ("model", "mix", "brightness", "decay")], "Body ID prefix differs")
    require(ids[198:] == ["body1_size", "body2_size", "body1_variant", "body2_variant", "body1_material", "body2_material"],
            "Appended body ID order differs")
    programs = inventory["programs"]
    require(isinstance(programs, list) and len(programs) == PROGRAMS, "Expected 523 programs")
    names = set()
    for index, program in enumerate(programs):
        require(set(program) == {"index", "host_index", "name", "category", "description", "tags", "values"},
                "Unexpected program fields")
        require(type(program["index"]) is int and type(program["host_index"]) is int
                and program["index"] == program["host_index"] == index, "Unordered program address")
        for key in ("name", "category", "description"):
            require(isinstance(program[key], str) and program[key], "Invalid program " + key)
        require(program["name"] not in names, "Duplicate program name")
        names.add(program["name"])
        tags = program["tags"]
        require(isinstance(tags, list) and all(isinstance(tag, str) and tag for tag in tags)
                and len(tags) == len(set(tags)), "Invalid or duplicate tags")
        parameter_bytes(program["values"])
    return {program["name"]: program for program in programs}


def compare_inventory(actual, expected):
    validate_inventory(actual)
    validate_inventory(expected)
    for key in ("schema", "library_revision", "parameter_ids"):
        require(actual[key] == expected[key], "Inventory differs: " + key)
    digest = hashlib.sha256()
    for delivered, reference in zip(actual["programs"], expected["programs"]):
        for key in set(reference) - {"values"}:
            require(type(delivered[key]) is type(reference[key]) and delivered[key] == reference[key],
                    "Inventory identity differs: " + str(reference["index"]) + "/" + key)
        bits = parameter_bytes(delivered["values"])
        require(bits == parameter_bytes(reference["values"]),
                "Inventory binary32 values differ: " + str(reference["index"]))
        digest.update(bits)
    return dict(programs=PROGRAMS, parameters=PARAMETERS, values=PROGRAMS * PARAMETERS,
                all_ordered_identities_identical=True, all_binary32_values_identical=True,
                ordered_binary32_sha256=digest.hexdigest())


def make_expected_files(groups, by_name):
    files = {"README.txt", "SHA256SUMS", "inventory.json", "provenance.json", "source-commit.txt"}
    for group in groups:
        files.add(group["folder"] + "/metrics.csv")
        for name in group["names"]:
            program = by_name[name]
            # The writer substitutes each UTF-8 byte that is not ASCII alphanumeric.
            safe = "".join(chr(byte) if 48 <= byte <= 57 or 65 <= byte <= 90 or 97 <= byte <= 122
                           else "-" for byte in name.encode("utf-8"))
            files.add(f"{group['folder']}/{program['host_index']}-{safe}.wav")
    require(len(files) == FILE_COUNT, "Unexpected source file roster")
    return files


def source_contract(repo, source, expected_inventory, inventory_source_path=None):
    require(source == SOURCE, "Source differs from the reviewed frozen commit")
    git = lambda *args: subprocess.check_output(["git", "-C", str(repo), *args])
    tree = git("rev-parse", source + "^{tree}").decode().strip()
    require(tree == TREE, "Source tree differs from the reviewed frozen tree")
    source_files = {}
    for path in (CAPTURE_SCRIPT, "src/tools/preset_capture.cpp", "src/core/FactoryPresets.h",
                 ".github/workflows/build.yml"):
        data = git("show", source + ":" + path)
        source_files[path] = dict(bytes=len(data), sha256=sha(data))
        if path == CAPTURE_SCRIPT:
            script = data
        elif path.endswith("FactoryPresets.h"):
            require(re.search(rb'curatedLibraryRevision\s*=\s*"2026\.11\.8"', data),
                    "Pinned factory source is not 2026.11.8")
    producer = types.ModuleType("source_bound_audition_roster")
    producer.__file__ = str(repo / CAPTURE_SCRIPT)
    # Only caller-selected, pinned repository code is executed. ZIP contents are
    # never imported or executed, and __name__ is not "__main__".
    exec(compile(script, producer.__file__, "exec"), producer.__dict__)
    groups = producer.selections()
    require(producer.roster_summary(groups) == EXPECTED and len(groups) == GROUP_COUNT,
            "Pinned source does not have the reviewed .8 roster")
    require(roster_sha(groups) == ROSTER_SHA256
            and roster_sha(groups[:54]) == PRESERVED_ROSTER_SHA256
            and roster_sha(groups[54:]) == APPENDED_ROSTER_SHA256,
            "Pinned source ordered requests differ from the reviewed .8 roster")
    reference_bytes = bounded_snapshot(expected_inventory, MAX_MEMBER, "Expected inventory")
    require(sha(reference_bytes) == INVENTORY_SHA256, "Expected inventory differs from frozen independent reference")
    reference = parse_json(reference_bytes)
    by_name = validate_inventory(reference)
    producer.validate_resolved_requests(groups, reference["programs"])
    if inventory_source_path:
        path = PurePosixPath(inventory_source_path)
        require(not path.is_absolute() and ".." not in path.parts and str(path) == inventory_source_path,
                "Invalid inventory source path")
        require(git("show", source + ":" + inventory_source_path) == reference_bytes,
                "Expected inventory differs from its pinned source file")
    expected_files = make_expected_files(groups, by_name)
    contract = dict(source_commit=source, source_tree=tree, library_revision=REVISION,
                    recovery_edition=RECOVERY_EDITION, expected_ci_run_id=RUN, expected_ci_run_attempt=ATTEMPT,
                    source_files=source_files, expected_inventory_sha256=sha(reference_bytes),
                    expected_inventory_source_path=inventory_source_path,
                    roster_sha256=roster_sha(groups),
                    preserved_roster=dict(wav_count=238, folder_count=54,
                                          sha256=roster_sha(groups[:54])),
                    appended_roster=dict(wav_count=18, folder_count=6,
                                         sha256=roster_sha(groups[54:])),
                    expected=dict(**EXPECTED, folder_count=GROUP_COUNT, regular_files=FILE_COUNT,
                                  checksums=FILE_COUNT - 1, parameters=PARAMETERS, programs=PROGRAMS,
                                  stereo_frames=round(EXPECTED["audio_seconds"] * RATE),
                                  raw_audio_bytes=RAW_AUDIO_BYTES),
                    expected_files=sorted(expected_files))
    return producer, reference, reference_bytes, expected_files, contract


def preflight(archive):
    infos = archive.infolist()
    require(0 < len(infos) < 500, "Unexpected ZIP entry count")
    require(sum(info.file_size for info in infos) < MAX_EXPANDED, "Expanded ZIP exceeds bound")
    seen, folded, regular, directories, entries = set(), set(), set(), set(), []
    for info in infos:
        name = info.filename
        require(name == info.orig_filename and 0 < len(name) <= 512, "Invalid ZIP name length/truncation")
        require(all(32 <= ord(char) < 127 for char in name) and "\\" not in name and ":" not in name,
                "Unsafe ZIP name: " + repr(name))
        directory = name.endswith("/")
        canonical = name[:-1] if directory else name
        path = PurePosixPath(canonical)
        require(canonical and not path.is_absolute() and str(path) == canonical
                and all(part not in ("", ".", "..") and not part.endswith((" ", "."))
                        for part in canonical.split("/")), "Noncanonical ZIP path: " + repr(name))
        require(canonical not in seen and canonical.casefold() not in folded, "Duplicate ZIP path")
        seen.add(canonical)
        folded.add(canonical.casefold())
        kind = stat.S_IFMT((info.external_attr >> 16) & 0xffff)
        require(info.create_system in (0, 3), "Unsupported ZIP creator")
        require(kind in ((0, stat.S_IFDIR) if directory else (0, stat.S_IFREG)),
                "Symlink/special/conflicting ZIP type")
        require(not (info.external_attr & 0x10) or directory, "DOS directory bit on regular file")
        require(not (info.flag_bits & 0x41), "Encrypted ZIP entries are unsupported")
        require(info.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED),
                "Unsupported ZIP compression")
        require(0 <= info.file_size < MAX_MEMBER and (not directory or info.file_size == 0),
                "ZIP entry size outside bound")
        (directories if directory else regular).add(canonical)
        entries.append(dict(path=name, kind="directory" if directory else "regular_file",
                            compressed_bytes=info.compress_size, bytes=info.file_size,
                            mode=oct((info.external_attr >> 16) & 0xffff), crc32=f"{info.CRC:08x}"))
    for name in regular:
        require(not any(str(parent) in regular for parent in PurePosixPath(name).parents
                        if str(parent) != "."), "ZIP file ancestor conflict")
    manifests = [name for name in regular if PurePosixPath(name).name == "provenance.json"]
    require(len(manifests) == 1, "Expected exactly one provenance manifest")
    prefix = manifests[0][:-len("provenance.json")]
    require(all(name.startswith(prefix) for name in regular), "Files outside audition root")
    allowed_dirs = {str(parent) for name in regular for parent in PurePosixPath(name).parents
                    if str(parent) != "."}
    require(directories <= allowed_dirs, "Unexpected ZIP directory")
    for info in infos:
        if info.is_dir():
            require(archive.read(info) == b"", "Nonempty ZIP directory")
    return prefix, {name[len(prefix):] for name in regular}, entries


def checksum_manifest(data, files):
    sums = {}
    for line in data.decode("ascii").splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        require(match and match[2] not in sums, "Malformed/duplicate checksum record")
        sums[match[2]] = match[1]
    require(set(sums) == files - {"SHA256SUMS"}, "Checksums do not cover exact file inventory")
    return sums


def fft(values):
    n = len(values)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j ^= bit
        if i < j:
            values[i], values[j] = values[j], values[i]
    length = 2
    while length <= n:
        step = cmath.exp(-2j * math.pi / length)
        half = length // 2
        for first in range(0, n, length):
            weight = 1 + 0j
            for offset in range(half):
                a, b = values[first + offset], values[first + offset + half] * weight
                values[first + offset], values[first + offset + half] = a + b, a - b
                weight *= step
        length *= 2


def inspect_wav(data, expected_frames, held_frames):
    require(0 < held_frames <= expected_frames, "Invalid held/frame contract")
    require(len(data) >= 58 and data[:4] == b"RIFF" and data[8:12] == b"WAVE", "Invalid RIFF/WAVE")
    require(struct.unpack_from("<I", data, 4)[0] == len(data) - 8, "RIFF length mismatch")
    chunks, offset = {}, 12
    while offset < len(data):
        require(offset + 8 <= len(data), "Truncated RIFF chunk header")
        kind, length = struct.unpack_from("<4sI", data, offset)
        begin, end = offset + 8, offset + 8 + length
        require(kind not in chunks and end <= len(data), "Duplicate/truncated RIFF chunk")
        chunks[kind] = data[begin:end]
        offset = end + (length & 1)
    require(offset == len(data) and set(chunks) == {b"fmt ", b"fact", b"data"}, "Unexpected RIFF chunks")
    require(len(chunks[b"fmt "]) == 18 and struct.unpack("<HHIIHHH", chunks[b"fmt "]) ==
            (3, 2, RATE, RATE * 8, 8, 32, 0), "Expected 48 kHz stereo IEEE float32")
    require(len(chunks[b"fact"]) == 4 and struct.unpack("<I", chunks[b"fact"])[0] == expected_frames,
            "WAV fact frame count mismatch")
    require(len(chunks[b"data"]) == expected_frames * 8, "WAV data frame count mismatch")
    samples = array("f")
    require(samples.itemsize == 4, "Host does not provide four-byte float arrays")
    samples.frombytes(chunks[b"data"])
    if sys.byteorder != "little":
        samples.byteswap()
    peak = energy = early = body = tail = ll = rr = lr = difference = window_energy = 0.0
    early_end, body_start = min(held_frames, RATE // 5), max(0, held_frames - RATE // 2)
    tail_start = max(held_frames, expected_frames - RATE // 2)
    window = RATE // 20
    loudest, peak_start, window_start = -1.0, 0, 0
    for frame in range(expected_frames):
        left, right = samples[frame * 2], samples[frame * 2 + 1]
        require(math.isfinite(left) and math.isfinite(right), "Nonfinite WAV sample")
        peak = max(peak, abs(left), abs(right))
        require(peak < 1.0, "WAV sample reaches or exceeds full scale")
        ls, rs = left * left, right * right
        energy += ls + rs
        if frame < early_end:
            early += ls
            early += rs
        if body_start <= frame < held_frames:
            body += ls
            body += rs
        if frame >= tail_start:
            tail += ls
            tail += rs
        if frame < held_frames:
            ll += ls
            rr += rs
            lr += left * right
            if frame:
                dl, dr = left - samples[frame * 2 - 2], right - samples[frame * 2 - 1]
                difference += dl * dl + dr * dr
            window_energy += ls
            window_energy += rs
            if frame + 1 == held_frames or frame + 1 - window_start == window:
                level = math.sqrt(window_energy / (2 * (frame + 1 - window_start)))
                if level > loudest:
                    loudest, peak_start = level, window_start
                window_start, window_energy = frame + 1, 0.0
    spectrum = [0.0] * 2049
    first = max(0, min(peak_start - 2048 + window // 2, max(0, held_frames - 4096)))
    for channel in (0, 1):
        bins = [(samples[(first + i) * 2 + channel] if first + i < held_frames else 0.0)
                * (0.5 - 0.5 * math.cos(2 * math.pi * i / 4095)) + 0j for i in range(4096)]
        fft(bins)
        for i in range(1, 2049):
            spectrum[i] += (bins[i].real * bins[i].real + bins[i].imag * bins[i].imag) * (1 if i == 2048 else 2)
    total = centroid = low = mid = high = 0.0
    for i in range(1, 2049):
        hz, power = i * RATE / 4096, spectrum[i]
        total += power
        centroid += hz * power
        if hz < 500:
            low += power
        elif hz < 2000:
            mid += power
        else:
            high += power
    if total > 1e-24:
        centroid, low, mid, high = (value / total for value in (centroid, low, mid, high))
    db = lambda value: 20 * math.log10(max(value, 1e-12))
    rms_db = lambda value, frames: db(math.sqrt(value / (2 * frames))) if frames else db(0)
    return dict(frames=expected_frames, seconds=expected_frames / RATE, peak_dbfs=db(peak),
                exact_silence=peak == 0.0,
                rms_dbfs=rms_db(energy, expected_frames), early_rms_dbfs=rms_db(early, early_end),
                body_rms_dbfs=rms_db(body, held_frames - body_start),
                tail_rms_dbfs=rms_db(tail, expected_frames - tail_start),
                peak_window_seconds=peak_start / RATE, centroid_hz=centroid,
                low_power=low, mid_power=mid, high_power=high,
                tilt=difference / max(1e-24, ll + rr),
                stereo_correlation=lr / max(1e-24, math.sqrt(ll * rr)),
                nonfinite_samples=0, samples_over_full_scale=0)


def check_commands(provenance, producer, source):
    command = provenance["inventory_command"]
    require(isinstance(command, list) and len(command) == 5 and
            command[1:4] == ["--library", "all", "--inventory"]
            and all(isinstance(value, str) for value in command), "Invalid inventory command")
    binary, inventory = PurePosixPath(command[0]), PurePosixPath(command[4])
    require(binary.is_absolute() and inventory.is_absolute() and str(binary) == command[0]
            and str(inventory) == command[4] and ".." not in binary.parts and ".." not in inventory.parts,
            "Noncanonical producer paths")
    out = inventory.parent
    require(binary.parts[-2:] == ("build", "cs80_preset_capture")
            and out.name == "preset-audition" and inventory.name == "inventory.json"
            and binary.parent.parent == out.parent, "Unexpected native producer paths")
    expected = []
    for group in producer.selections():
        cmd = [str(binary), "--library", "all", "--out", str(out / group["folder"]), "--wav",
               "--rate", str(RATE), "--velocity", str(group["velocity"]), "--pressure", str(group["pressure"]),
               "--hold", str(group["hold"]), "--tail", str(group["tail"])]
        if "note" in group:
            cmd += ["--note", str(group["note"])]
        for name in group["names"]:
            cmd += ["--preset", name]
        expected.append(cmd)
    require(provenance["capture_commands"] == expected, "Capture commands differ from pinned producer")
    return producer.audition_readme(producer.selections(), EXPECTED, source, REVISION).encode()


def check_metric_identity_and_settings(row, program, group):
    require(set(row) == set(CSV_FIELDS) and all(isinstance(value, str) for value in row.values()),
            "Malformed metrics row")
    require(row["name"] == program["name"] and row["index"] == str(program["host_index"])
            and row["category"] == program["category"], "Metrics program identity differs")
    default_note = (48 if program["category"] == "Bass" else 67
                    if program["category"] in ("Winds", "Leads") else 72
                    if program["category"] == "Bells" else 60)
    settings = dict(sample_rate=RATE, velocity=group["velocity"], pressure=group["pressure"],
                    hold_seconds=group["hold"], tail_seconds=group["tail"], rank=0,
                    note=group.get("note", default_note), nonfinite=0, samples_over_full_scale=0)
    for key, expected in settings.items():
        actual = float(row[key])
        require(math.isfinite(actual) and abs(actual - expected) <= 1e-6,
                "Wrong metric setting: " + key)
    require(re.fullmatch(r"[0-9a-f]{1,16}", row["parameter_digest"])
            and int(row["parameter_digest"], 16) == parameter_digest(program["values"]),
            "Metrics parameter digest differs from exact inventory")
    return settings


def compare_wav_metrics(row, measured, path):
    for key in MEASURED_FIELDS:
        actual = float(row[key])
        # Ten-significant-digit CSV formatting and independent FFT rounding
        # are allowed; differences of musical size are not.
        tolerance = 1e-4 if key == "centroid_hz" else 1e-5 if key.endswith("_dbfs") else 1e-7
        require(math.isfinite(actual) and abs(actual - measured[key]) <= tolerance,
                f"WAV/CSV metric differs: {path}/{key}: {measured[key]} versus {actual}")


@contextmanager
def frozen_zip(path):
    """Stream one immutable compressed snapshot; never extract expanded audio."""
    with path.open("rb") as original, tempfile.TemporaryFile(mode="w+b") as snapshot:
        require(stat.S_ISREG(os.fstat(original.fileno()).st_mode), "ZIP is not a regular file")
        digest, total = hashlib.sha256(), 0
        while block := original.read(1024 * 1024):
            total += len(block)
            require(total < MAX_ARCHIVE, "ZIP size outside bound")
            snapshot.write(block)
            digest.update(block)
        require(total > 0, "ZIP size outside bound")
        snapshot.seek(0)
        yield snapshot, digest.hexdigest(), total


def independent_binding(path, reference_bytes, expected_inventory_path):
    """Check an actual regenerated inventory receipt; never reuse lost local evidence."""
    raw = bounded_snapshot(path, MAX_MEMBER, "Independent capture receipt")
    receipt = parse_json(raw)
    require(receipt["schema"] == 1 and receipt["status"] == "complete"
            and receipt["recovery_edition"] == RECOVERY_EDITION,
            "Independent capture is not a complete current recovery receipt")
    require(receipt["source_commit"] == SOURCE and receipt["source_tree"] == TREE
            and receipt["library_revision"] == REVISION, "Independent capture source/revision mismatch")
    require(receipt["original_build"] == {"run_id": int(RUN), "run_attempt": int(ATTEMPT),
                                        "producer_job_id": PRODUCER_JOB},
            "Independent receipt original-build identity mismatch")
    require(receipt["tracked_source_clean_before"] is True
            and receipt["tracked_source_clean_after"] is True, "Independent source was not clean")
    require(receipt["verifier_sha256"] == file_sha(Path(__file__))
            and receipt["producer_sha256"] == file_sha(Path(__file__).with_name("produce-independent-inventory.py")),
            "Independent receipt came from another verifier/producer")
    capture, inventory, execution = receipt["capture"], receipt["inventory"], receipt["execution"]
    require(re.fullmatch(r"[0-9a-f]{64}", capture["sha256_before"])
            and capture["sha256_before"] == capture["sha256_after"], "Independent capture executable changed")
    require(inventory["sha256"] == sha(reference_bytes) == INVENTORY_SHA256,
            "Independent capture inventory hash mismatch")
    require(inventory["bytes"] == len(reference_bytes) and type(inventory["exit_code"]) is int
            and inventory["exit_code"] == 0, "Independent inventory length/normal exit mismatch")
    binary, actual_inventory = PurePosixPath(capture["path"]), PurePosixPath(inventory["path"])
    require(binary.is_absolute() and actual_inventory.is_absolute()
            and str(binary) == capture["path"] and str(actual_inventory) == inventory["path"]
            and ".." not in binary.parts and ".." not in actual_inventory.parts,
            "Noncanonical independent capture paths")
    require(inventory["command"] == [str(binary), "--library", "all", "--inventory", str(actual_inventory)]
            and actual_inventory == PurePosixPath(str(expected_inventory_path.resolve())),
            "Independent command/inventory path mismatch")
    require(execution["kind"] in ("local_recovery", "github_actions")
            and isinstance(execution["platform"], str) and execution["platform"]
            and isinstance(execution["python_version"], str) and execution["python_version"],
            "Independent execution context missing")
    if execution["kind"] == "local_recovery":
        require(not any(key in execution for key in ("run_id", "run_attempt", "workflow_run_id")),
                "Local recovery must not invent a qualification run ID")
    else:
        require(execution["repository"] == REPOSITORY
                and re.fullmatch(r"[1-9][0-9]*", str(execution["run_id"]))
                and re.fullmatch(r"[1-9][0-9]*", str(execution["run_attempt"])),
                "Independent Actions context missing")
    return dict(receipt_sha256=sha(raw), inventory_sha256=sha(reference_bytes),
                source_commit=SOURCE, source_tree=TREE, execution=execution,
                independently_observed_capture_sha256=capture["sha256_after"],
                inventory_normal_exit_code=0, original_build=receipt["original_build"],
                native_capture_binary_hash_equality_expected=False,
                producer_sha256=receipt["producer_sha256"], verifier_sha256=receipt["verifier_sha256"])


def accept_archive(args, producer, reference, reference_bytes, expected_files, contract):
    require(re.fullmatch(r"[0-9a-f]{64}", args.expected_zip_sha256 or ""), "External ZIP SHA256 is required")
    require(type(args.expected_zip_bytes) is int and 0 < args.expected_zip_bytes < MAX_ARCHIVE,
            "External API ZIP byte count is required")
    ci_checked = args.run_id is not None
    by_name = validate_inventory(reference)
    with frozen_zip(args.archive) as (snapshot, digest, archive_size), zipfile.ZipFile(snapshot) as archive:
        require(digest == args.expected_zip_sha256, "ZIP SHA256 differs from external expected digest")
        require(archive_size == args.expected_zip_bytes, "ZIP size differs from expected API size")
        prefix, files, entries = preflight(archive)
        require(files == expected_files, "ZIP files differ from source roster")
        read = lambda path: archive.read(prefix + path)
        sums = checksum_manifest(read("SHA256SUMS"), files)
        for name, expected in sums.items():
            require(sha(read(name)) == expected, "Checksum mismatch: " + name)
        provenance = parse_json(read("provenance.json"))
        require(set(provenance) == {"schema", "source_commit", "source_tree",
                "tracked_source_clean_before_capture", "tracked_source_clean_after_capture",
                "repository", "run_id", "run_attempt", "platform", "capture_binary_sha256",
                "inventory_command", "capture_commands", "selections", "selected_program_count",
                "expected_audio_seconds", "expected_wav_count", "capture_status",
                "library_revision", "wav_count", "audio_seconds"}, "Unexpected provenance fields")
        require(isinstance(provenance["platform"], str) and provenance["platform"].startswith("Linux-")
                and len(provenance["platform"]) < 1024, "Unexpected producer platform")
        require(type(provenance["schema"]) is int and provenance["schema"] == 3
                and provenance["capture_status"] == "complete",
                "Capture is not complete schema 3")
        require(provenance["source_commit"] == args.source
                and provenance["source_tree"] == contract["source_tree"], "Capture source identity mismatch")
        require(provenance["repository"] == REPOSITORY, "Capture repository mismatch")
        if ci_checked:
            require(str(provenance["run_id"]) == args.run_id
                    and str(provenance["run_attempt"]) == args.attempt, "Capture run/attempt mismatch")
        require(provenance["tracked_source_clean_before_capture"] is True
                and provenance["tracked_source_clean_after_capture"] is True, "Capture source was not clean")
        require(re.fullmatch(r"[0-9a-f]{64}", provenance["capture_binary_sha256"]),
                "Invalid capture executable hash")
        require(read("source-commit.txt") == (args.source + "\n").encode(), "source-commit.txt mismatch")
        require(provenance["library_revision"] == REVISION and provenance["selections"] == producer.selections(),
                "Provenance revision/selections mismatch")
        for key, expected in (("expected_wav_count", EXPECTED["wav_count"]), ("wav_count", EXPECTED["wav_count"]),
                              ("selected_program_count", EXPECTED["selected_program_count"]),
                              ("expected_audio_seconds", EXPECTED["audio_seconds"]),
                              ("audio_seconds", EXPECTED["audio_seconds"])):
            require(type(provenance[key]) in (int, float) and provenance[key] == expected, "Wrong provenance " + key)
        require(read("README.txt") == check_commands(provenance, producer, args.source),
                "README differs from pinned producer")
        inventory_bytes = read("inventory.json")
        inventory_match = compare_inventory(parse_json(inventory_bytes), reference)
        inventory_match.update(delivered_sha256=sha(inventory_bytes), expected_sha256=sha(reference_bytes),
                               json_bytes_identical=inventory_bytes == reference_bytes)
        wave_reports, models, variants, materials = [], set(), set(), set()
        ids = reference["parameter_ids"]
        for group in producer.selections():
            reader = csv.DictReader(io.StringIO(read(group["folder"] + "/metrics.csv").decode("utf-8"), newline=""))
            require(reader.fieldnames == CSV_FIELDS, "Metrics field order differs")
            rows = list(reader)
            require([row["name"] for row in rows] == group["names"], "Metrics ordered selection differs")
            for row in rows:
                program = by_name[row["name"]]
                settings = check_metric_identity_and_settings(row, program, group)
                safe = "".join(chr(byte) if 48 <= byte <= 57 or 65 <= byte <= 90 or 97 <= byte <= 122
                               else "-" for byte in program["name"].encode("utf-8"))
                path = f"{group['folder']}/{program['host_index']}-{safe}.wav"
                data = read(path)
                held = round(group["hold"] * RATE)
                measured = inspect_wav(data, held + round(group["tail"] * RATE), held)
                compare_wav_metrics(row, measured, path)
                require(measured["exact_silence"] == (program["name"] == "Pressure Lead"
                        and group["pressure"] == 0), "Unexpected silence or non-silence: " + path)
                wave_reports.append(dict(path=path, program=program["name"], host_index=program["host_index"],
                                         sha256=sha(data), settings=settings, **measured))
                values = dict(zip(ids, program["values"]))
                for rank in (1, 2):
                    weight = 1 - values["mix"] if rank == 1 else values["mix"]
                    if weight * values[f"body{rank}_mix"] > 0 and values[f"body{rank}_model"] > 0:
                        models.add(int(values[f"body{rank}_model"]))
                        variants.add(int(values[f"body{rank}_variant"]))
                        materials.add(int(values[f"body{rank}_material"]))
    frames = sum(wave["frames"] for wave in wave_reports)
    require(len(wave_reports) == EXPECTED["wav_count"]
            and len({wave["program"] for wave in wave_reports}) == EXPECTED["selected_program_count"]
            and frames == round(EXPECTED["audio_seconds"] * RATE), "Actual count/duration mismatch")
    require(models == set(range(1, 20)), "Auditions do not cover all 19 active body models")
    require(variants == set(range(5)), "Auditions do not cover Standard and all four variants")
    require(materials == set(range(10)), "Auditions do not cover Standard and all nine materials")
    require(frames * 8 == RAW_AUDIO_BYTES, "Actual raw audio byte budget mismatch")
    return dict(schema=1, status="accepted_actual_archive_content", accepted_archive_content=True,
                release_qualified=False, **contract, repository=REPOSITORY,
                run_id=provenance["run_id"], run_attempt=provenance["run_attempt"],
                expected_ci_identity_checked=ci_checked, zip_sha256=digest,
                zip_bytes=archive_size, zip_prefix=prefix, zip_entries=entries,
                regular_files=len(files), checksum_count=len(sums), wav_count=len(wave_reports),
                actual_stereo_frames=frames, actual_audio_seconds=frames / RATE,
                selected_program_count=len({wave["program"] for wave in wave_reports}),
                body_models=sorted(models), body_variants=sorted(variants), body_materials=sorted(materials),
                actual_raw_audio_bytes=frames * 8, inventory_match=inventory_match,
                max_peak_dbfs=max(wave["peak_dbfs"] for wave in wave_reports),
                nonfinite_samples=0, samples_over_full_scale=0,
                capture_binary_sha256=provenance["capture_binary_sha256"],
                verifier_sha256=file_sha(Path(__file__)), wavs=wave_reports,
                limits=["Caller supplies the independent expected inventory and external ZIP digest.",
                        "Authentic GitHub producer/job/upload bindings require bind-audition-ci.py.",
                        "The original native capture executable hash is authenticated manifest evidence; its binary is not uploaded.",
                        "The independently rebuilt reference executable has a separate compiler/platform identity; its hash is not compared to the native executable.",
                        "Parameter ranges are not inventory.json fields; ordered IDs and values are checked here, with source range compatibility reviewed separately.",
                        "Recovery edition is a newly reviewed successor, not a claim to the lost local checker bytes or results.",
                        "Each clip is one note-on with fixed pressure and note-off, not a played arpeggio, pedal or live bend.",
                        "256 representative clips cover 95 programs; all 523 programs are inventoried.",
                        "These checks establish content integrity and reported metrics, not listening approval, "
                        "acoustic equivalence, native host validation, or overall release qualification."])


def write_fresh_report(path, report):
    payload = json.dumps(report, indent=2, allow_nan=False) + "\n"
    path.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation prevents replacement of previous acceptance evidence.
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            stream.write(payload)
    except BaseException:
        path.unlink(missing_ok=True)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--source", "--expected-source", required=True)
    parser.add_argument("--run-id", "--expected-run", required=True)
    parser.add_argument("--attempt", "--expected-attempt", required=True)
    parser.add_argument("--expected-inventory", type=Path, required=True)
    parser.add_argument("--inventory-source-path")
    parser.add_argument("--independent-capture-receipt", type=Path, required=True)
    parser.add_argument("--expected-inventory-sha256")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--expected-zip-sha256")
    parser.add_argument("--expected-zip-bytes", type=int)
    parser.add_argument("--prepare", action="store_true")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    require(not args.out.exists(), "Report path already exists; use a fresh output")
    require(args.run_id == RUN and args.attempt == ATTEMPT, "Run/attempt differs from frozen release candidate")
    require((args.run_id is None) == (args.attempt is None), "Run and attempt must be supplied together")
    if args.run_id is not None:
        require(re.fullmatch(r"[1-9][0-9]*", args.run_id) and re.fullmatch(r"[1-9][0-9]*", args.attempt),
                "Expected positive run and attempt integers")
    if args.expected_inventory_sha256:
        require(re.fullmatch(r"[0-9a-f]{64}", args.expected_inventory_sha256),
                "Expected inventory hash mismatch")
    producer, reference, reference_bytes, expected_files, contract = source_contract(
        args.repo, args.source, args.expected_inventory, args.inventory_source_path)
    contract["independent_inventory_binding"] = independent_binding(
        args.independent_capture_receipt, reference_bytes, args.expected_inventory)
    if args.expected_inventory_sha256:
        require(sha(reference_bytes) == args.expected_inventory_sha256,
                "Expected inventory hash mismatch")
    if args.prepare:
        report = dict(schema=1, status="prepared_pending_actual_archive", accepted_archive_content=False,
                      release_qualified=False, **contract)
    else:
        require(args.archive is not None, "Actual archive required unless --prepare")
        report = accept_archive(args, producer, reference, reference_bytes, expected_files, contract)
    write_fresh_report(args.out, report)
    print(json.dumps({key: value for key, value in report.items()
                      if key not in ("expected_files", "zip_entries", "wavs")}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, TypeError, OverflowError, OSError, UnicodeError, struct.error,
            zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        print("Audition verification failed: " + str(error), file=sys.stderr)
        raise SystemExit(1)
