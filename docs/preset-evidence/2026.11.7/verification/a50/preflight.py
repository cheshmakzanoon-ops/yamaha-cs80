#!/usr/bin/env python3
"""Derive A50's expected inputs without rendering or accepting any audio.

Run from the repository root. The only evaluated producer code is the pinned
Git source's selections() declaration. No archive or verifier fixture is used.
All outputs use exclusive creation and all qualification flags remain false.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
from fractions import Fraction
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import types

BASE = Path("docs/preset-evidence/2026.11.7")
A40 = BASE / "verification/a40/source-ee0dabca"
A28 = BASE / "verification/a28"
INVENTORY = BASE / "verification/a12/inventory.json"
SOURCE = "ee0dabcacb4cdff93f8803844f60348e0ceda6ed"
TREE = "17115913cbb29157bd00539485bcc5f38b57219c"
PRIOR_AUDIO_SOURCE = "c5549f7d095a5c5f49a5ad22ec0ccc819245d201"
BASELINE_SOURCE = "18711f93a39aa49f3d6f9a41784be456224facbe"
BASELINE_INVENTORY = "docs/preset-evidence/2026.11.6/verification/a15/inventory.json"
INVENTORY_SHA = "55c08568074a6560ef10c8c525733a40aec42d9b8c6d20644bf4984cdd0d3e0d"
BASELINE_INVENTORY_SHA = "02a38edcc47a1befd9f044d3a1203e7ed3c464987951562b0d81e88f86c66d50"
ROSTER_SHA = "889f32cc511e7ae693803a4aec8118b2ac09f147cf2886a56af966e16115871a"
PRESERVED_SHA = "126e05bb759e19adf7fec40dabb3c4a86bafe2f658dd845f6653c34ff3a7d31b"
APPENDED_SHA = "4fd5e733857d0f2324b8b2fbeee5c6e94bc00a2376e1d282aa5440364230b7b6"
CONTENT_SHA = "bae2c82d291bee1240a3c49673a82381114f9bf4c0260275ea1b4494fda59df5"
BINDER_SHA = "65cf483ee4397533ea5ca4bf8d88eb4c5f21353059b99ba761833f7f288e2554"
RUN, ATTEMPT, JOB = 45397189410, 1, 138455975798
REPOSITORY = "cheshmakzanoon-ops/yamaha-cs80"
FLAGS = dict(accepted_archive_content=False, audition_qualified=False,
             platform_qualified=False, release_qualified=False,
             human_listening_approved=False, exact_acoustic_fidelity_established=False)


def check(value, message):
    if not value:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()


def git(*args):
    return subprocess.check_output(["git", *args])


def blob(source, path):
    return git("show", source + ":" + str(path))


def pin(path):
    data = path.read_bytes()
    return dict(path=path.as_posix(), bytes=len(data), sha256=sha(data))


def roster(source):
    data = blob(source, "scripts/capture-factory-auditions.py")
    module = types.ModuleType("a50_pinned_roster_" + source)
    module.__file__ = str(Path("scripts/capture-factory-auditions.py").resolve())
    exec(compile(data, module.__file__, "exec"), module.__dict__)
    return module.selections()


def fresh(path, value):
    with path.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(value, indent=2, allow_nan=False) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    check(not args.out_dir.exists(), "Choose a fresh output directory")
    check(git("rev-parse", SOURCE + "^{tree}").decode().strip() == TREE,
          "Frozen source tree differs")

    pinned_paths = [
        A40 / "verify-audition-archive.py", A40 / "bind-audition-ci.py",
        A40 / "prepared-contract.json", A40 / "helper-hashes.json",
        A40 / "preparation-receipt.json", A40 / "USAGE.md",
        A28 / "verify-audition-archive.py", A28 / "bind-audition-ci.py",
        A28 / "prepared-contract.json", A28 / "preparation-receipt.json",
        A28 / "USAGE.md", INVENTORY, BASE / "verification/a12/receipt.json",
    ]
    input_pins = [pin(path) for path in pinned_paths]
    check(sha((A40 / "verify-audition-archive.py").read_bytes()) == CONTENT_SHA,
          "Frozen content helper changed")
    check(sha((A40 / "bind-audition-ci.py").read_bytes()) == BINDER_SHA,
          "Frozen CI binder changed")
    check((A40 / "verify-audition-archive.py").read_bytes()
          == (A28 / "verify-audition-archive.py").read_bytes(),
          "Content verifier differs from original A28")

    inventory_bytes = INVENTORY.read_bytes()
    check(sha(inventory_bytes) == INVENTORY_SHA, "Independent A12 inventory digest differs")
    check(inventory_bytes == blob(SOURCE, INVENTORY)
          == blob(PRIOR_AUDIO_SOURCE, INVENTORY), "Independent inventory differs from frozen sources")
    inventory = json.loads(inventory_bytes)
    baseline_bytes = blob(BASELINE_SOURCE, BASELINE_INVENTORY)
    check(sha(baseline_bytes) == BASELINE_INVENTORY_SHA, "Frozen .6 inventory digest differs")
    baseline = json.loads(baseline_bytes)
    expected_previous = dict(inventory, library_revision="2026.11.6")
    check(baseline == expected_previous, "Inventory changed beyond the revision label")
    programs, ids = inventory["programs"], inventory["parameter_ids"]
    check(inventory["library_revision"] == "2026.11.7" and len(ids) == len(set(ids)) == 204,
          "Inventory revision or parameter identity mismatch")
    check(len(programs) == 523 and len({p["name"] for p in programs}) == 523,
          "Program identity mismatch")
    bits = bytearray()
    by_name = {}
    for index, program in enumerate(programs):
        check(program["index"] == program["host_index"] == index, "Program order differs")
        check(len(program["values"]) == 204
              and all(type(v) in (int, float) and math.isfinite(v) for v in program["values"]),
              "Program parameter values malformed")
        bits.extend(struct.pack("<204f", *program["values"]))
        by_name[program["name"]] = program
    check(sha(bits) == "aedb533d123874448b4df31f3c25aa7cb109d4109c6dc22ee9548ebcdeb42273",
          "Ordered binary32 digest differs")
    inits = [dict(name=p["name"], index=p["index"]) for p in programs if p["category"] == "Init"]
    check({p["name"] for p in inits} == {"Init Patch", "Init Circuit"}, "Init inventory differs")

    groups = roster(SOURCE)
    check(groups == roster(PRIOR_AUDIO_SOURCE), "Audition source continuity differs")
    check(groups[:46] == roster(BASELINE_SOURCE), "Original 225 requests changed")
    for actual, expected in [(sha(canonical(groups)), ROSTER_SHA),
                             (sha(canonical(groups[:46])), PRESERVED_SHA),
                             (sha(canonical(groups[46:])), APPENDED_SHA)]:
        check(actual == expected, "Ordered roster digest differs")

    expected_files = {"README.txt", "SHA256SUMS", "inventory.json", "provenance.json", "source-commit.txt"}
    resolved, seen = [], set()
    total_frames = 0
    models, variants, materials = set(), set(), set()
    for group_index, group in enumerate(groups):
        expected_files.add(group["folder"] + "/metrics.csv")
        held = Fraction(str(group["hold"])) * 48000
        tail = Fraction(str(group["tail"])) * 48000
        check(held.denominator == tail.denominator == 1, "Non-integral frame request")
        for name in group["names"]:
            program = by_name[name]
            default_note = (48 if program["category"] == "Bass" else 67
                            if program["category"] in ("Winds", "Leads") else 72
                            if program["category"] == "Bells" else 60)
            note = group.get("note", default_note)
            identity = (name, note, group["velocity"], group["pressure"], group["hold"], group["tail"])
            check(identity not in seen, "Duplicate resolved render request")
            seen.add(identity)
            safe_name = "".join(chr(c) if 48 <= c <= 57 or 65 <= c <= 90 or 97 <= c <= 122
                                else "-" for c in name.encode("utf-8"))
            path = f"{group['folder']}/{program['host_index']}-{safe_name}.wav"
            check(path not in expected_files, "Duplicate WAV filename")
            expected_files.add(path)
            fnv = 14695981039346656037
            for byte in struct.pack("<204f", *program["values"]):
                fnv = ((fnv ^ byte) * 1099511628211) & ((1 << 64) - 1)
            frames = int(held + tail)
            total_frames += frames
            resolved.append(dict(request_index=len(resolved), group_index=group_index, path=path,
                                 program=name, host_index=program["host_index"], category=program["category"],
                                 note=note, velocity=group["velocity"], pressure=group["pressure"],
                                 hold_seconds=group["hold"], tail_seconds=group["tail"], sample_rate=48000,
                                 rank=0, frames=frames, raw_audio_bytes=frames * 8,
                                 parameter_digest_hex=format(fnv, "x")))
            values = dict(zip(ids, program["values"]))
            for rank in (1, 2):
                weight = 1 - values["mix"] if rank == 1 else values["mix"]
                if weight * values[f"body{rank}_mix"] > 0 and values[f"body{rank}_model"] > 0:
                    models.add(int(values[f"body{rank}_model"]))
                    variants.add(int(values[f"body{rank}_variant"]))
                    materials.add(int(values[f"body{rank}_material"]))
    check(models == set(range(1, 20)) and variants == set(range(5)) and materials == set(range(10)),
          "Selected active body coverage differs")
    expected = dict(wav_count=len(resolved), selected_program_count=len({r["program"] for r in resolved}),
                    audio_seconds=total_frames / 48000, folder_count=len(groups), regular_files=len(expected_files),
                    checksums=len(expected_files) - 1, parameters=len(ids), programs=len(programs),
                    stereo_frames=total_frames, raw_audio_bytes=total_frames * 8)
    required = dict(wav_count=238, selected_program_count=92, audio_seconds=1361.2, folder_count=54,
                    regular_files=297, checksums=296, parameters=204, programs=523,
                    stereo_frames=65337600, raw_audio_bytes=522700800)
    check(expected == required, "Independently derived content totals differ")
    check(sum(len(g["names"]) for g in groups[:46]) == 225
          and sum(len(g["names"]) for g in groups[46:]) == 13, "Prefix/suffix count differs")
    prepared = json.loads((A40 / "prepared-contract.json").read_bytes())
    original = json.loads((A28 / "prepared-contract.json").read_bytes())
    check(prepared["source_commit"] == SOURCE and prepared["source_tree"] == TREE,
          "A40 prepared source context differs")
    for contract in (prepared, original):
        check(contract["accepted_archive_content"] is False and contract["release_qualified"] is False,
              "Preparation mislabeled as acceptance")
        check(contract["expected"] == expected and contract["expected_files"] == sorted(expected_files),
              "Independent roster differs from original/frozen contract")
        check(contract["roster_sha256"] == ROSTER_SHA
              and contract["expected_inventory_sha256"] == INVENTORY_SHA, "Contract digests differ")
    source_files = {}
    for path in ("scripts/capture-factory-auditions.py", "src/tools/preset_capture.cpp",
                 "src/core/FactoryPresets.h", ".github/workflows/build.yml"):
        data = blob(SOURCE, path)
        source_files[path] = dict(bytes=len(data), sha256=sha(data))
        check(data == blob(PRIOR_AUDIO_SOURCE, path), "Audition producer input changed")
    check(source_files == prepared["source_files"] == original["source_files"],
          "Source hashes differ from frozen contracts")

    destination = BASE / "ci/source-ee0dabca/audition"
    content_out = str(destination / "content-acceptance.json")
    binding_out = str(destination / "ci-binding.json")
    content_command = ["python3", "-B", str(A40 / "verify-audition-archive.py"), "--repo", ".",
                       "--source", SOURCE, "--run-id", str(RUN), "--attempt", str(ATTEMPT),
                       "--expected-inventory", str(INVENTORY), "--inventory-source-path", str(INVENTORY),
                       "--expected-inventory-sha256", INVENTORY_SHA,
                       "--archive", "<original_archive.path>",
                       "--expected-zip-sha256", "<authenticated_artifact_api.digest_without_sha256_prefix>",
                       "--expected-zip-bytes", "<authenticated_artifact_api.size_in_bytes>",
                       "--out", content_out]
    binding_command = ["python3", "-B", str(A40 / "bind-audition-ci.py"),
                       "--content-report", content_out, "--artifact-api", "<original_artifact_api.path>",
                       "--run-api", "<attempt_1_run_api.path>", "--job-api", "<completed_linux_job_api.path>",
                       "--job-log", "<original_linux_job_log.path>", "--out", binding_out]
    manifest = dict(
        schema=1, status="phase1_commands_prepared_actual_inputs_pending", **FLAGS,
        working_directory="repository root", repository=REPOSITORY, source_commit=SOURCE, source_tree=TREE,
        run_id=RUN, run_attempt=ATTEMPT, producer_job_id=JOB,
        artifact_name="cs80-curated-preset-audition-" + SOURCE,
        download_directory="/tmp/cs80-ci-1cee5a/ee0dabca",
        actual_inputs={key: None for key in ["original_archive", "original_artifact_api", "attempt_1_run_api",
                                           "completed_linux_job_api", "original_linux_job_log",
                                           "authenticated_retrieval_receipts"]},
        substitution_rule="Replace only angle-bracket input values with authenticated original paths/API values; retain every fixed argument.",
        commands=[dict(order=1, purpose="actual content acceptance", argv=content_command,
                       expected_exit_code=0, log=str(destination / "a50-content-verifier.log")),
                  dict(order=2, purpose="original CI binding", argv=binding_command,
                       expected_exit_code=0, log=str(destination / "a50-ci-binder.log"))],
        required_input_rules=[
            "Require original artifact ID, exact name, authenticated size and SHA-256, source, run, and creation time; never infer missing values.",
            "Run API must retain attempt 1; a later Intel attempt 2 run object cannot replace the audition producer's attempt-1 record.",
            "Linux job must be the complete original job 138455975798, source ee0dabca, run 45397189410, attempt 1.",
            "Retain full timestamped original Linux log and authenticated receipt mapping each API/log/archive path and digest to its retrieval.",
            "Snapshot/hash the supplied receipts and all actual evidence; preserve older receipts and record each new command's actual exit status.",
            "Recheck frozen helper/reference hashes before and after both commands; use fresh output paths and preserve failed results.",
        ],
        supplemental_producer_review=[
            "Reconcile original checkout, Build, Test, Capture curated preset auditions, and Upload curated preset auditions step numbers, times, and conclusions.",
            "Verify the actual source checkout and direct unsuppressed capture command in the original log against the frozen workflow.",
            "Record capture process exit zero from original process evidence; when successful GitHub steps omit a literal exit line, explain the inference from the original successful step, direct bash -e invocation, and absence of suppression.",
            "The capture producer uses subprocess.run(..., check=True), validates complete schema 3, and raises on failed child/source/safety checks; upload success alone cannot qualify capture.",
            "Bind unique upload digest/success/download-URL records inside the original upload step to the authenticated artifact ID, size, SHA-256, and times.",
            "Do not relabel old c554 or Intel attempt-1 failure evidence; overall release/platform qualification remains separate.",
        ],
        resource_policy=dict(archive_stays_compressed=True, extraction=False, full_pcm_duplicate=False,
                             immutable_zip_snapshot_limit_exclusive=512 * 1024 * 1024,
                             total_expanded_limit_exclusive=512 * 1024 * 1024,
                             member_limit_exclusive=16 * 1024 * 1024,
                             binder_input_limit_exclusive=32 * 1024 * 1024,
                             maximum_original_downloads=1),
        content_gate_summary=[
            "Exact safe ZIP path/type/name set, all 297 regular files, and all 296 checksums.",
            "All 238 WAVs with exact resolved roster settings and parameter digests; all 13 appended requests remain required.",
            "Every ordered inventory identity, metadata field, parameter ID, and binary32 value matches independent A12.",
            "Complete source/run/attempt/cleanliness/executable-digest provenance, commands, and README match frozen source.",
            "Every actual PCM sample is finite and within full scale; independently recompute and compare all 12 metrics using unchanged tolerances.",
            "Exact 65,337,600 stereo frames, 522,700,800 raw audio bytes, 54 folders, 92 selected existing programs, and active model/variant/material coverage.",
        ],
        scope="Technical content/provenance only; no human listening, exact acoustic fidelity, executable-byte possession, live-gesture, platform, or release approval.")
    derived = dict(schema=1, status="independent_expected_contract_only", **FLAGS,
                   source_commit=SOURCE, source_tree=TREE, source_files=source_files,
                   expected=expected, roster_sha256=ROSTER_SHA,
                   preserved_roster=dict(wav_count=225, folder_count=46, sha256=PRESERVED_SHA,
                                         exact_frozen_6_prefix=True, source_commit=BASELINE_SOURCE),
                   appended_roster=dict(wav_count=13, folder_count=8, sha256=APPENDED_SHA),
                   inventory=dict(path=str(INVENTORY), bytes=len(inventory_bytes), sha256=INVENTORY_SHA,
                                  exact_frozen_ee0dabca_and_c554_bytes=True,
                                  complete_6_records_equal_except_revision=True, new_programs=0,
                                  musical_programs=521, init_programs=inits, parameter_values=523 * 204,
                                  ordered_binary32_sha256=sha(bits),
                                  ordered_parameter_ids_canonical_sha256=sha(canonical(ids)),
                                  ordered_metadata_canonical_sha256=sha(canonical([
                                      {k:v for k,v in p.items() if k != "values"} for p in programs]))),
                   active_body_models=sorted(models), active_body_variants=sorted(variants),
                   active_body_materials=sorted(materials),
                   expected_files=sorted(expected_files), ordered_groups=groups, resolved_requests=resolved)
    check(input_pins == [pin(path) for path in pinned_paths], "Inputs changed during preflight")
    args.out_dir.mkdir(parents=True)
    fresh(args.out_dir / "expected-contract.json", derived)
    fresh(args.out_dir / "command-input-manifest.json", manifest)
    fresh(args.out_dir / "input-pins.json", dict(schema=1, files=input_pins))
    output_pins = [pin(args.out_dir / name) for name in
                   ("expected-contract.json", "command-input-manifest.json", "input-pins.json")]
    receipt = dict(schema=1, status="phase1_preflight_passed_pending_actual_archive", **FLAGS,
                   completed_utc=datetime.now(timezone.utc).isoformat(),
                   source_commit=SOURCE, source_tree=TREE, repository=REPOSITORY,
                   run_id=RUN, run_attempt=ATTEMPT, producer_job_id=JOB,
                   preflight_passed=True, expected_contract=expected,
                   actual_archive_supplied=False, actual_archive_inspected=False,
                   audio_rendered=False, synthetic_archive_created=False,
                   frozen_helper_tests_repeated=False, frozen_helpers_modified=False,
                   all_retained_inputs_unchanged=True, inputs=input_pins, outputs=output_pins,
                   preflight_script=pin(Path(__file__)),
                   remaining="Wait for root's original ZIP/API/log/attempt/authenticated retrieval evidence, then execute the frozen helpers and supplemental producer review.")
    fresh(args.out_dir / "preflight-receipt.json", receipt)
    print(json.dumps({k:v for k,v in receipt.items() if k not in ("inputs", "outputs")}, indent=2))


if __name__ == "__main__":
    main()
