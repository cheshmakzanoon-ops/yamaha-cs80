#!/usr/bin/env python3
"""Fail-closed quantitative qualification *before* reviewed release dispatch.

This deliberately does not replace the original full-audit preflight/finalize
guard or the three exact-main CI / screenshot ancestry checks in the existing
release dispatcher. It checks numerical acceptance that those green jobs do
not imply. Never use successful capture as a substitute for these results.
"""
from __future__ import annotations

import argparse
from collections import Counter
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys


class GateError(ValueError):
    """Evidence or a numerical acceptance condition was not satisfied."""


def require(condition, explanation):
    if not condition:
        raise GateError(explanation)


def finite(value, where):
    try:
        parsed = float(value)
    except (TypeError, ValueError, OverflowError) as exc:
        raise GateError(f"{where}: not a number: {value!r}") from exc
    require(math.isfinite(parsed), f"{where}: nonfinite number")
    return parsed


def exact_integer(value, where):
    f = finite(value, where)
    require(f.is_integer(), f"{where}: not an integer")
    return int(f)


def sha_string(value, where, length=40):
    require(isinstance(value, str) and len(value) == length
            and all(ch in "0123456789abcdef" for ch in value),
            f"{where}: malformed lowercase hex digest")
    return value


TEXT_FIELDS = {
    "phrase.csv": {"name", "group", "policy"},
    "alias.csv": {"policy", "wave"},
    "fx.csv": {"name", "group", "policy"},
    "latency.csv": {"policy"},
    "click-causality.csv": {"name", "event", "coordinate",
                            "baseline_peak_channel", "omitted_peak_channel"},
}
CPU_TEXT_FIELDS = {"scenario", "rank_i_name", "rank_ii_name", "phase", "mode"}
CPU_IDENTITY_FIELDS = ("rate", "block", "mode", "scenario", "initial_selector",
                       "rank_i_name", "rank_ii_name", "phase")


def verify_numeric_table(name, rows, text_fields):
    require(isinstance(rows, list) and rows, f"{name}: no records")
    for i, row in enumerate(rows, start=2):
        require(isinstance(row, dict), f"{name}:{i}: not a CSV record")
        for column, value in row.items():
            require(isinstance(column, str) and isinstance(value, str),
                    f"{name}:{i}:{column}: not raw CSV text")
            if column not in text_fields and value.strip():
                finite(value, f"{name}:{i}:{column}")


def verify_csv_schema(name, rows, frozen_header_sha256):
    """Require every row to use the original reporter's ordered CSV columns."""
    sha_string(frozen_header_sha256, f"{name} frozen header", 64)
    require(isinstance(rows, list) and rows, f"{name}: missing CSV schema")
    header = list(rows[0])
    require(all(isinstance(column, str) for column in header),
            f"{name}: nontext CSV header")
    actual_sha = hashlib.sha256(",".join(header).encode("utf-8")).hexdigest()
    require(actual_sha == frozen_header_sha256,
            f"{name}: original ordered CSV header altered or incomplete")
    require(all(list(row) == header for row in rows),
            f"{name}: inconsistent columns across CSV rows")


def source_phrase_rows(rows):
    return [row for row in rows if row["policy"] == "source"]


def fixture_identity_sha256(rows):
    """Hash the exact source-defined ordered (group, name) fixture sequence."""
    fields = [{"group": row["group"], "name": row["name"]} for row in rows]
    canonical = json.dumps(fields, ensure_ascii=False, sort_keys=True,
                           separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(canonical).hexdigest()


def scope_rows(scope, suites):
    if scope.startswith("Source phrase "):
        return source_phrase_rows(suites["phrase.csv"])
    if scope == "Source saw/pulse, ring off, 48 kHz":
        return [r for r in suites["alias.csv"]
                if r["policy"] == "source" and exact_integer(r["rate"], "alias rate") == 48000
                and exact_integer(r["ring"], "alias ring") == 0
                and r["wave"] in ("saw", "pulse")]
    prefix = "Source causal baseline: "
    require(scope.startswith(prefix), f"unknown original target scope: {scope}")
    coordinate = scope[len(prefix):]
    return [r for r in suites["click-causality.csv"] if r["coordinate"] == coordinate]


def verify_audit_coverage(contract, suites):
    rates = contract["audit"]["expected_rates_hz"]
    canonical_identities = contract["audit"]["frozen_fixture_identities"]
    # Counts alone would accept the same easy program/tone/FX case duplicated
    # in place of a difficult registered fixture. Recheck the identity and
    # sampling order of the original source-defined audit, independently of
    # the capture workflow's success flag and JSON summary booleans.
    bank_names_by_policy = {}
    tab_names_by_policy = {}
    expected_alias = Counter(((72, 1.0), (72, 4.0), (84, 1.0),
                              (84, 4.0), (96, 1.0), (96, 4.0), (108, 1.0)))
    expected_fx = Counter([("000", 0.0)] +
                          [(f"{effect}00", mix) for effect in range(1, 6)
                           for mix in (0.5, 1.0)] +
                          [(slots, 1.0) for slots in ("333", "444", "222", "555", "123", "435")])
    for policy in ("source", "legacy"):
        full_bank_names = None
        tab_names = None
        for rate in rates:
            phrase = [r for r in suites["phrase.csv"] if r["policy"] == policy
                      and exact_integer(r["rate"], "phrase rate") == rate]
            expected_programs = 222 if rate == 44100 else 14
            require(len(phrase) == expected_programs + 11,
                    f"{policy} phrase at {rate}: incomplete program/tone coverage")
            programs = [r for r in phrase if r["group"].startswith("program:")]
            tabs = [r for r in phrase if r["group"] == "tab"]
            require(len(programs) == expected_programs and len(tabs) == 11,
                    f"{policy} phrase at {rate}: unknown group or missing program/tone")
            current_programs = [(r["name"], r["group"]) for r in programs]
            current_tabs = [r["name"] for r in tabs]
            require(all(r["name"] for r in programs) and all(current_tabs),
                    f"{policy} phrase at {rate}: blank fixture name")
            if rate == 44100:
                require(len({name for name, _ in current_programs}) == 222,
                        f"{policy}: 222 uniquely named factory programs required")
                require(fixture_identity_sha256(programs) == canonical_identities["factory_programs_44100_sha256"],
                        f"{policy}: frozen factory program identity/hash changed")
                full_bank_names = current_programs
            else:
                require(current_programs == full_bank_names[::16],
                        f"{policy} phrase at {rate}: wrong stride-16 bank sample/order")
            require(len(set(current_tabs)) == 11,
                    f"{policy} phrase at {rate}: duplicate tone pairs")
            if rate == 44100:
                require(fixture_identity_sha256(tabs) == canonical_identities["tone_pairs_44100_sha256"],
                        f"{policy}: frozen physical tone-pair identity/hash changed")
            if tab_names is None:
                tab_names = current_tabs
            else:
                require(current_tabs == tab_names,
                        f"{policy} phrase at {rate}: tone-pair identity/order changed")
            fx = [r for r in suites["fx.csv"] if r["policy"] == policy
                  and exact_integer(r["rate"], "fx rate") == rate]
            require(len(fx) == 17, f"{policy} FX at {rate}: incomplete coverage")
            require(all(r["name"] == "bright saw" and r["group"] == "fxbase" for r in fx),
                    f"{policy} FX at {rate}: wrong fixed fixture")
            require(Counter((r["fx_slots"], finite(r["fx_mix"], "FX mix")) for r in fx) == expected_fx,
                    f"{policy} FX at {rate}: changed/duplicated effect configuration")
            for ring in (0, 1):
                for wave in ("saw", "pulse"):
                    alias = [r for r in suites["alias.csv"] if r["policy"] == policy
                             and exact_integer(r["rate"], "alias rate") == rate
                             and exact_integer(r["ring"], "alias ring") == ring
                             and r["wave"] == wave]
                    require(len(alias) == 7,
                            f"{policy} {wave} ring={ring} at {rate}: incomplete alias coverage")
                    require(Counter((exact_integer(r["note"], "alias MIDI note"),
                                     finite(r["feet_ratio"], "alias feet")) for r in alias) == expected_alias,
                            f"{policy} {wave} ring={ring} at {rate}: changed/duplicated note/feet")
            latency = [r for r in suites["latency.csv"] if r["policy"] == policy
                       and exact_integer(r["rate"], "latency rate") == rate]
            expected_freq = Counter(f for f in (250, 1000, 5000, 10000, 15000, 18000, 20000)
                                    if f <= 0.45 * rate)
            require(Counter(exact_integer(r["freq_hz"], "latency frequency")
                            for r in latency) == expected_freq,
                    f"{policy} latency at {rate}: changed/duplicated frequency")
            require(all(exact_integer(r["oversampling_factor"], "latency factor") == 2
                        for r in latency),
                    f"{policy} latency at {rate}: unexpected default 2x factor")
        bank_names_by_policy[policy] = full_bank_names
        tab_names_by_policy[policy] = tab_names
    require(bank_names_by_policy["source"] == bank_names_by_policy["legacy"],
            "source/legacy factory bank fixture identity changed")
    require(tab_names_by_policy["source"] == tab_names_by_policy["legacy"],
            "source/legacy physical tone pairs differ")
    causal_identities = {
        "original_physical_panel": ("Bass Stack", "note_on"),
        "electrical_programmer": ("Bass Stack", "note_on"),
        "tone_tabs": ("Funky 1 + Funky 2", "single_note_off"),
    }
    for coordinate, (name, event) in causal_identities.items():
        rows = [r for r in suites["click-causality.csv"] if r["coordinate"] == coordinate]
        require(Counter(exact_integer(r["rate"], "causal rate") for r in rows) == Counter(rates),
                f"{coordinate}: incomplete causal-rate coverage")
        require(all(r["name"] == name and r["event"] == event for r in rows),
                f"{coordinate}: causal program/event identity changed")
    require({r["policy"] for r in suites["latency.csv"]} == {"source", "legacy"},
            "missing source/legacy latency lanes")
    require({exact_integer(r["rate"], "latency rate") for r in suites["latency.csv"]} == set(rates),
            "missing latency sample rate")


def verify_audit(contract, source, audit):
    summary = audit["summary"]
    provenance = audit["provenance"]
    suites = audit["suites"]
    observed = audit["observed_raw_files"]
    req = contract["audit"]

    require(summary["schema_version"] == 1, "unexpected original summary schema")
    require(summary["source_snapshot_sha"] == source["reviewed_sha"], "audit source not reviewed source")
    require(summary["production_change_sha"] == source["audio_sha"], "audit audio anchor mismatch")
    require(summary["summarizer_sha256"] == req["frozen_reporter_sha256"],
            "source-locked original reporter changed without preregistration")
    require(provenance["head_sha"] == source["reviewed_sha"], "capture head not reviewed source")
    require(provenance["src_tree_sha"] == source["source_tree_sha"], "capture source tree mismatch")
    require(provenance["production_audio_commit"] == source["audio_sha"], "capture audio anchor mismatch")
    require(provenance["registration_commit"] == source["registration_sha"], "capture registration mismatch")
    require(provenance["complete_capture"] is True and provenance["quick"] is False,
            "incomplete or quick capture")
    require(provenance["registered_command"] == req["registered_command"], "registered audit command changed")
    require(provenance["cpu_suite_included"] is False and summary["scope"]["cpu_suite_included"] is False,
            "audio audit and CPU evidence scopes conflated")
    require(summary["scope"]["rates_hz"] == req["expected_rates_hz"], "missing audio audit rate")
    require(summary["scope"]["pure_rendered"] is False
            and summary["scope"]["curated_library_rendered"] is False,
            "audited scope does not match registered source scope")
    require(provenance["nonfinite_numeric_cells_reported_not_hidden"] == 0,
            "provenance reports nonfinite CSV cells")
    nonfinite = summary["nonfinite"]
    require(nonfinite["phrase_stereo_frames"] == 0 and nonfinite["fx_stereo_frames"] == 0,
            "nonfinite phrase/FX audio frames")
    require(nonfinite["alias_nonfinite_render_rows"] == [] and nonfinite["numeric_csv_cells"] == [],
            "nonfinite alias/CSV report")
    require(set(suites) == set(req["expected_rows"]), "audit suite missing or extra")
    require(set(req["frozen_csv_header_sha256"]) == set(req["expected_rows"]),
            "frozen CSV header registration missing or extra suite")
    require(set(summary["raw_files"]) == set(suites)
            and set(provenance["raw_files"]) == set(suites)
            and set(observed) == set(suites), "raw manifest missing or extra suite")
    for name, count in req["expected_rows"].items():
        rows = suites[name]
        require(len(rows) == count, f"{name}: expected {count}, have {len(rows)}")
        verify_csv_schema(name, rows, req["frozen_csv_header_sha256"][name])
        verify_numeric_table(name, rows, TEXT_FIELDS[name])
        if name in ("phrase.csv", "fx.csv"):
            require(all(exact_integer(r["nonfinite"], f"{name} nonfinite") == 0 for r in rows),
                    f"{name}: nonfinite rendered audio detected")
        expected = summary["raw_files"][name]
        capture = provenance["raw_files"][name]
        actual = observed[name]
        sha_string(expected["sha256"], f"{name} summary SHA", 64)
        require(expected["sha256"] == capture["sha256"] == actual["sha256"],
                f"{name}: raw digest mismatch")
        require(expected["bytes"] == capture["bytes"] == actual["bytes"],
                f"{name}: raw byte count mismatch")
        require(expected["row_count"] == capture["rows"] == actual["rows"] == count,
                f"{name}: raw row count mismatch")
    verify_audit_coverage(contract, suites)

    # The original source-locked reporter is independently checked against raw
    # numeric CSV values. Successful summarizer exit is not numerical PASS.
    comparisons = summary["existing_target_comparisons"]
    targets = req["existing_targets"]
    require(len(comparisons) == len(targets), "original targets missing or added")
    results = []
    for found, target in zip(comparisons, targets):
        require(found["scope"] == target["scope"] and found["metric"] == target["metric"],
                "original target metric/scope altered")
        require(finite(found["maximum"], "reported ceiling") == target["maximum"],
                f"{target['scope']}: source ceiling altered")
        rows = scope_rows(target["scope"], suites)
        require(len(rows) == target["rows"] == found["row_count"],
                f"{target['scope']}: source target coverage changed")
        values = [finite(r[target["metric"]], target["scope"]) for r in rows]
        failures = sum(value > target["maximum"] for value in values)
        require(found["exceeding_row_count"] == failures,
                f"{target['scope']}: reporter exceedance disagrees with raw CSV")
        # The reporter uses the exact floating-point parse of exported CSV.
        require(finite(found["extrema"]["max"], "reported max") == max(values),
                f"{target['scope']}: reporter extrema disagrees with raw CSV")
        require(found["all_exported_values_within_target"] is (failures == 0),
                f"{target['scope']}: reporter PASS flag falsified")
        require(failures == 0, f"{target['scope']}: {failures}/{len(rows)} numerical failures")
        results.append({"scope": target["scope"], "max": max(values), "rows": len(rows)})
    return results


def case_key(row):
    if row["scenario"] == "paired_tones":
        return (row["initial_selector"], row["rank_i_name"], row["rank_ii_name"])
    return ("quiet-idle",)


def registered_cpu_case_identities(spec):
    """Reconstruct the frozen benchmark's *entire* baseline 27-case/51-row matrix.

    The producer at Git blob 92ea7e0b... uses paired_tones for eleven physical
    tone pairs in isolation and legacy, plus two moving and three idle cases.
    The selected CPU threshold concerns only isolation held/idle, but all other
    rows still have to exist under their exact registered identities.
    """
    pairs = spec["registered_tone_pairs_by_selector"]
    require(isinstance(pairs, list) and len(pairs) == 11
            and all(isinstance(pair, list) and len(pair) == 2
                    and all(isinstance(name, str) and name for name in pair)
                    for pair in pairs),
            "registered CPU physical tone-pair mapping is incomplete")
    rate, block = str(spec["rate_hz"]), str(spec["block_frames"])
    cases = []

    def add(mode, scenario, selector, phases):
        first, second = pairs[selector]
        for phase in phases:
            cases.append((rate, block, mode, scenario, str(selector),
                          first, second, phase))

    for selector in range(len(pairs)):
        for mode in ("isolation", "legacy"):
            add(mode, "paired_tones", selector, ("warmed_held", "release_tail"))
    for mode in ("isolation", "legacy"):
        add(mode, "moving_tabs_pressure_wah", 0, ("warmed_held", "release_tail"))
        add(mode, "idle_quiet", 0, ("warmed_held",))
    add("legacy", "idle_noise", 0, ("warmed_held",))
    require(len(cases) == spec["expected_rows_per_round"] == 51,
            "registered CPU case matrix is not 51 rows")
    return Counter(cases)


def cpu_case_identity_sha256(identities):
    """Producer-independent canonical fingerprint of 51 registered CSV rows."""
    payload = ("\n".join(sorted("\t".join(identity) for identity in identities))
               + "\n").encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def verify_cpu_workload(spec, metadata, rows, label):
    """Check the frozen producer's observed work, not only its fixture labels.

    Every invariant below already exists in source_factory_benchmark.cpp at
    the registered blob. This does not introduce a p99/deadline threshold or
    make a hardware/host qualification out of the core timing fixture.
    """
    for field, expected in spec["registered_workload_metadata"].items():
        actual = metadata.get(field)
        # bool is an int subclass in Python; false must not be forged as 0.
        require(type(actual) is type(expected) and actual == expected,
                f"{label}: registered workload metadata changed: {field}")
    verify_csv_schema(label, rows, spec["frozen_csv_header_sha256"])
    held_callbacks = spec["callbacks_per_held_phase_min"]
    minimum_tail = math.ceil(spec["registered_workload_metadata"]["tail_minimum_seconds"]
                             * spec["rate_hz"] / spec["block_frames"])
    maximum_tail = math.ceil(spec["registered_workload_metadata"]["tail_cap_seconds"]
                             * spec["rate_hz"] / spec["block_frames"])
    for index, row in enumerate(rows, 2):
        where = f"{label}:{index}: workload witness"
        def integer(field):
            return exact_integer(row[field], f"{where} {field}")
        require(all(value.strip() for field, value in row.items()
                    if field not in CPU_TEXT_FIELDS),
                f"{where}: missing numeric observation")
        voices = tuple(integer(field) for field in
                       ("min_active_voices", "max_active_voices", "final_active_voices"))
        minimum, maximum, final = voices
        require(0 <= minimum <= final <= maximum <= 8,
                f"{where}: invalid active voice range")
        callbacks = integer("callbacks")
        complete = integer("tail_voices_complete")
        held = row["phase"] == "warmed_held"
        moving = row["scenario"] == "moving_tabs_pressure_wah" and held
        idle = row["scenario"] in ("idle_quiet", "idle_noise")
        if held:
            require(callbacks == held_callbacks,
                    f"{where}: held callback count differs from registered workload")
            expected_voices = (0, 0, 0) if idle else (8, 8, 8)
            require(voices == expected_voices and complete == 0,
                    f"{where}: held/idle voice count or phase changed")
            require(idle or finite(row["peak"], f"{where} peak") > 0,
                    f"{where}: silent held-note fixture")
        else:
            require(minimum_tail <= callbacks <= maximum_tail,
                    f"{where}: release observation outside original 1-to-40-second window")
            require(complete in (0, 1) and bool(complete) == (final == 0),
                    f"{where}: release completion disagrees with observed voices")
            require(complete == 1 or callbacks == maximum_tail,
                    f"{where}: incomplete release truncated before original cap")
        events = (held_callbacks + 31) // 32 if moving else 0
        require(integer("selector_events") == events,
                f"{where}: selector event workload changed")
        expected_compilations = 2 * events if moving and row["mode"] == "isolation" else 0
        require(integer("source_compilations") == expected_compilations,
                f"{where}: selector compilation workload changed")
        require(all(integer(field) == 0 for field in
                    ("controller_events", "mpe_member_state_checks", "pedal_held_callbacks")),
                f"{where}: baseline contains unregistered expressive workload")


def verify_cpu_round(contract, rd, source_sha, label):
    spec = contract["cpu"]
    metadata = rd["metadata"]
    completed = rd["completed"]
    receipt = rd["receipt"]
    rows = rd["rows"]
    require(metadata["schema_version"] == 2 and metadata["suite"] == spec["suite"]
            and metadata["fixture_id"] == spec["fixture_id"], f"{label}: wrong benchmark suite")
    require(metadata["source_commit_caller_verified"] == source_sha,
            f"{label}: wrong source SHA")
    require(metadata["quick_subset"] is False,
            f"{label}: quick subset is not release evidence")
    require(metadata["clock_is_steady"] is True and metadata["order_seed"] == 803010,
            f"{label}: timing clock or registered order seed changed")
    require(exact_integer(metadata["callbacks_per_held_phase"], "callbacks per held phase")
            == spec["callbacks_per_held_phase_min"],
            f"{label}: registered 1024-callback workload changed")
    require(metadata["hardware_equivalence_or_realtime_qualification"] is False,
            f"{label}: benchmark cannot claim DAW, hardware, or realtime equivalence")
    require(completed["completed"] is True
            and completed["cases"] == spec["expected_cases_per_round"]
            and completed["rows"] == spec["expected_rows_per_round"],
            f"{label}: incomplete benchmark completion")
    require(len(rows) == spec["expected_rows_per_round"], f"{label}: missing raw CPU rows")
    for field in ("host_id", "runner_job_id", "compiler_id", "flags_sha256",
                  "benchmark_source_sha256", "runner_environment_sha256", "capture_id"):
        require(isinstance(receipt[field], str) and receipt[field],
                f"{label}: missing CPU receipt {field}")
    for field in ("flags_sha256", "benchmark_source_sha256", "runner_environment_sha256"):
        sha_string(receipt[field], f"{label}: {field}", 64)
    require(receipt["benchmark_source_sha256"] == spec["registered_benchmark_source_sha256"],
            f"{label}: unregistered benchmark driver SHA-256")
    require(receipt["source_sha"] == source_sha,
            f"{label}: CPU receipt source identity mismatch")
    sha_string(receipt["benchmark_binary_sha256"], f"{label}: benchmark binary", 64)
    require(isinstance(metadata["compiler"], str) and metadata["compiler"],
            f"{label}: missing compiler version")
    require(isinstance(metadata["source_fingerprint"], str) and metadata["source_fingerprint"],
            f"{label}: missing source driver fingerprint")
    verify_cpu_workload(spec, metadata, rows, label)
    verify_numeric_table(label, rows, CPU_TEXT_FIELDS)
    expected = registered_cpu_case_identities(spec)
    sha_string(spec["registered_case_identity_sha256"], "registered CPU case matrix", 64)
    require(cpu_case_identity_sha256(expected.elements())
            == spec["registered_case_identity_sha256"],
            "frozen 51-row CPU case matrix/physical tone mapping differs from registration")
    actual = Counter(tuple(row[field] for field in CPU_IDENTITY_FIELDS) for row in rows)
    require(actual == expected,
            f"{label}: CPU rate/block/mode/scenario/selector/tone/phase coverage differs from registered 51-row baseline")
    for i, row in enumerate(rows, 2):
        require(exact_integer(row["isolation"], f"{label}:{i} isolation flag")
                == (1 if row["mode"] == "isolation" else 0)
                and exact_integer(row["source_performance"], f"{label}:{i} source-performance flag") == 0,
                f"{label}:{i}: unregistered CPU render mode flags")
        require(exact_integer(row["nonfinite_count"], f"{label}:{i}: nonfinite_count") == 0,
                f"{label}:{i}: nonfinite audio")
        require(finite(row["median_rtf"], f"{label}:{i}: median_rtf") >= 0,
                f"{label}:{i}: invalid RTF")
        require(exact_integer(row["budget_exceedances"], f"{label}:{i}: deadline misses") >= 0,
                f"{label}:{i}: invalid misses")
    chosen = [r for r in rows if exact_integer(r["rate"], "held rate") == spec["rate_hz"]
              and exact_integer(r["block"], "held block") == spec["block_frames"]
              and r["mode"] == "isolation" and exact_integer(r["isolation"], "held isolation") == 1
              and r["phase"] == spec["held_phase"]
              and r["scenario"] in (spec["held_scenario"], spec["idle_scenario"])]
    held = [r for r in chosen if r["scenario"] == spec["held_scenario"]]
    idle = [r for r in chosen if r["scenario"] == spec["idle_scenario"]]
    require(len(held) == spec["required_held_fixture_count"] and len(idle) == 1,
            f"{label}: held/quiet-idle target rows missing or repeated")
    require(len({case_key(row) for row in held}) == len(held),
            f"{label}: duplicate held fixture")
    for row in chosen:
        require(exact_integer(row["callbacks"], f"{label}: held callbacks")
                >= spec["callbacks_per_held_phase_min"],
                f"{label}: held/idle callback count short")
    return {case_key(row): row for row in chosen}


def verify_cpu(contract, source, cpu):
    spec = contract["cpu"]
    require(source["before_sha"] == spec["registered_before_source_sha"],
            "CPU before source differs from preregistered source SHA")
    before = cpu["before"]
    after = cpu["after"]
    require(len(before) == spec["required_before_rounds"]
            and len(after) == spec["required_after_rounds"],
            "CPU timing lacks three before AND three after full rounds")
    rounds = []
    common_runner = None
    common_driver = None
    common_workload = None
    stage_binaries = {}
    seen_captures = set()
    for stage, group, sha in (("before", before, source["before_sha"]),
                              ("after", after, source["reviewed_sha"])):
        for index, rd in enumerate(group, start=1):
            label = f"{stage}-r{index}"
            chosen = verify_cpu_round(contract, rd, sha, label)
            receipt = rd["receipt"]
            require(receipt["capture_id"] ==
                    f"{receipt['runner_job_id']}/{stage}/{index}/{sha}",
                    f"{label}: CPU receipt capture/source/round identity changed")
            if stage in stage_binaries:
                require(receipt["benchmark_binary_sha256"] == stage_binaries[stage],
                        f"{label}: CPU benchmark binary differs within source stage")
            else:
                stage_binaries[stage] = receipt["benchmark_binary_sha256"]
            runner = tuple(receipt[key] for key in ("host_id", "runner_job_id", "compiler_id",
                                                      "flags_sha256", "runner_environment_sha256"))
            driver = (rd["metadata"]["compiler"], rd["metadata"]["source_fingerprint"],
                      receipt["benchmark_source_sha256"])
            if common_runner is None:
                common_runner, common_driver = runner, driver
            workload = {key: value for key, value in rd["metadata"].items()
                        if key not in ("source_commit_caller_verified", "started_unix_seconds")}
            if common_workload is None:
                common_workload = workload
            require(runner == common_runner and driver == common_driver,
                    f"{label}: not the same host/compiler/driver/flags")
            require(workload == common_workload,
                    f"{label}: benchmark workload metadata changed")
            require(receipt["capture_id"] not in seen_captures,
                    f"{label}: CPU capture reused")
            seen_captures.add(receipt["capture_id"])
            rounds.append((stage, index, chosen))
    keys = set(rounds[0][2])
    require(len(keys) == spec["required_held_fixture_count"] + 1,
            "CPU target fixture count mismatch")
    for stage, index, chosen in rounds:
        require(set(chosen) == keys, f"{stage}-r{index}: CPU fixture identities changed")
    # Audio output text/checksums must agree in all repeats for a given source.
    for stage in ("before", "after"):
        stage_rounds = [selected for s, _, selected in rounds if s == stage]
        for key in keys:
            for field in ("checksum", "peak", "rms"):
                require(len({rd[key][field] for rd in stage_rounds}) == 1,
                        f"{stage}: fixture {key} {field} not deterministic")
    after_rounds = [selected for s, _, selected in rounds if s == "after"]
    selected_held = []
    selected_idle = None
    for key in sorted(keys):
        numbered = list(enumerate((rd[key] for rd in after_rounds), start=1))
        chosen_round, chosen_row = min(numbered, key=lambda pair: finite(pair[1]["median_rtf"], "RTF"))
        selection = {"fixture": list(key), "selected_round": chosen_round,
                     "median_rtf": finite(chosen_row["median_rtf"], "median RTF"),
                     "p99_rtf": finite(chosen_row["p99_rtf"], "p99 RTF"),
                     "budget_exceedances": exact_integer(chosen_row["budget_exceedances"], "misses"),
                     "all_round_median_rtfs": [finite(r[key]["median_rtf"], "round RTF") for r in after_rounds]}
        if key == ("quiet-idle",):
            require(selection["median_rtf"] <= spec["upper_bounds"]["idle_median_rtf"],
                    "quiet idle median RTF exceeds 0.02")
            selected_idle = selection
        else:
            require(selection["median_rtf"] <= spec["upper_bounds"]["held_median_rtf"],
                    f"held fixture {key} median RTF exceeds 0.20")
            selected_held.append(selection)
    require(len(selected_held) == spec["required_held_fixture_count"] and selected_idle is not None,
            "CPU fixture selection incomplete")
    return selected_held, selected_idle


def validate_evidence(contract, bundle, expected_main_sha=None, expected_source_tree_sha=None):
    """Validate an evidence bundle; raise GateError on *any* failure.

    The release workflow must supply trusted expected-main and tree values
    derived from its checked-out exact-main commit, not from the audit bundle.
    """
    require(contract["schema_version"] == 1, "unsupported numerical contract")
    source = bundle["source"]
    for field in ("reviewed_sha", "source_tree_sha", "audio_sha", "registration_sha", "before_sha"):
        sha_string(source[field], f"source.{field}")
    require(source["before_sha"] == contract["cpu"]["registered_before_source_sha"],
            "CPU before source does not match preregistered checkpoint")
    require(source["before_sha"] != source["reviewed_sha"], "before and after source are identical")
    require(source["before_is_ancestor"] is True, "before source is not an ancestor")
    if expected_main_sha is not None:
        require(source["reviewed_sha"] == expected_main_sha, "reviewed source differs from exact main")
    if expected_source_tree_sha is not None:
        require(source["source_tree_sha"] == expected_source_tree_sha,
                "reviewed source tree differs from exact main")
    targets = verify_audit(contract, source, bundle["audit"])
    held, idle = verify_cpu(contract, source, bundle["cpu"])
    return {"pass": True, "reviewed_source_sha": source["reviewed_sha"],
            "reviewed_source_tree_sha": source["source_tree_sha"],
            "audit_targets": targets, "selected_held": held, "selected_idle": idle}


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def load_csv(path):
    data = Path(path).read_bytes()
    with Path(path).open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    return rows, {"sha256": hashlib.sha256(data).hexdigest(),
                  "bytes": len(data), "rows": len(rows)}


def git_output(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args], text=True).strip()


def load_disk_bundle(args):
    """Read raw files directly, derive observed digests, and verify Git ancestry."""
    identity = read_json(args.identity)
    reporter_path = args.repo / args.contract["audit"]["frozen_reporter_path"]
    require(hashlib.sha256(reporter_path.read_bytes()).hexdigest()
            == args.contract["audit"]["frozen_reporter_sha256"],
            "reviewed original audit reporter differs from frozen registered SHA")
    reviewed = git_output(args.repo, "rev-parse", "HEAD")
    tree = git_output(args.repo, "rev-parse", "HEAD:src")
    require(reviewed == args.expected_main_sha and reviewed == identity["reviewed_sha"],
            "checked-out source is not the approved exact-main SHA")
    require(tree == args.expected_source_tree_sha and tree == identity["source_tree_sha"],
            "checked-out src tree is not the approved tree")
    benchmark_path = "src/tools/source_factory_benchmark.cpp"
    expected_blob = args.contract["cpu"]["registered_benchmark_blob_sha"]
    sha_string(expected_blob, "registered CPU benchmark Git blob")
    for revision in (reviewed, identity["before_sha"]):
        require(git_output(args.repo, "rev-parse", f"{revision}:{benchmark_path}") == expected_blob,
                "before/reviewed CPU benchmark source blob differs from preregistration")
    require(hashlib.sha256((args.repo / benchmark_path).read_bytes()).hexdigest()
            == args.contract["cpu"]["registered_benchmark_source_sha256"],
            "reviewed CPU benchmark source bytes differ from preregistration")
    def ancestor(sha):
        return subprocess.run(["git", "-C", str(args.repo), "merge-base", "--is-ancestor",
                               sha, reviewed], check=False).returncode == 0

    identity["before_is_ancestor"] = ancestor(identity["before_sha"])
    require(ancestor(identity["audio_sha"]), "audio production anchor not in reviewed history")
    require(ancestor(identity["registration_sha"]), "audit registration anchor not in reviewed history")
    audit_dir = Path(args.audit_dir)
    suites = {}
    observed = {}
    for name in args.contract["audit"]["expected_rows"]:
        suites[name], observed[name] = load_csv(audit_dir / name)
    audit = {"summary": read_json(audit_dir.parent / "audit-summary.json"),
             "provenance": read_json(audit_dir.parent / "audit-provenance.json"),
             "suites": suites, "observed_raw_files": observed}
    # The original frozen audit has separate marker/report digests. Verify those
    # actual bytes before accepting even an otherwise plausible summary.
    marker = (audit_dir.parent / "audit.complete").read_bytes()
    summary_marker = audit["summary"]["completion_marker"]
    require(summary_marker["path"] == "audit.complete"
            and hashlib.sha256(marker).hexdigest() == summary_marker["sha256"]
            and marker.decode("utf-8") == summary_marker["text"],
            "original source-locked completion marker mismatch")
    require(f"source_sha={reviewed}\n" in summary_marker["text"]
            and f"registration_commit={identity['registration_sha']}\n" in summary_marker["text"],
            "original completion marker source/registration mismatch")
    provenance_bytes = (audit_dir.parent / "audit-provenance.json").read_bytes()
    require(f"provenance_sha256={hashlib.sha256(provenance_bytes).hexdigest()}\n"
            in summary_marker["text"], "original completion marker provenance mismatch")

    def cpu_round(path):
        path = Path(path)
        rows, _ = load_csv(path / "results.csv")
        receipt = read_json(path / "qualification-receipt.json")
        require(receipt["runner_job_id"] == args.expected_runner_job_id,
                "CPU timing receipt not from authorized same-host CI job")
        return {"metadata": read_json(path / "metadata.json"),
                "completed": read_json(path / "completed.json"),
                "receipt": receipt,
                "rows": rows}

    return {"source": identity, "audit": audit,
            "cpu": {"before": [cpu_round(path) for path in args.before_round],
                    "after": [cpu_round(path) for path in args.after_round]}}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", type=Path,
                        default=Path(__file__).resolve().parents[1] / ".github/release-numerical-contract.json")
    parser.add_argument("--identity", type=Path, required=True,
                        help="Source SHA registration with audio/registration/before anchors")
    parser.add_argument("--repo", type=Path, required=True, help="Exact-main checked-out Git repository")
    parser.add_argument("--expected-main-sha", required=True, help="Trusted exact-main SHA from CI")
    parser.add_argument("--expected-source-tree-sha", required=True, help="Trusted M:src tree SHA")
    parser.add_argument("--expected-runner-job-id", required=True,
                        help="Trusted same-host CI job identity, e.g. GITHUB_RUN_ID/GITHUB_JOB")
    parser.add_argument("--audit-dir", type=Path, required=True, help="All five original CSV files")
    parser.add_argument("--before-round", type=Path, action="append", required=True,
                        help="CPU before round directory; exactly 3 required")
    parser.add_argument("--after-round", type=Path, action="append", required=True,
                        help="CPU after round directory; exactly 3 required")
    args = parser.parse_args(argv)
    try:
        args.contract = read_json(args.contract)
        bundle = load_disk_bundle(args)
        result = validate_evidence(args.contract, bundle, args.expected_main_sha,
                                   args.expected_source_tree_sha)
    except (GateError, KeyError, OSError, ValueError, subprocess.CalledProcessError) as error:
        print(json.dumps({"pass": False, "reason": str(error)}, sort_keys=True), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
