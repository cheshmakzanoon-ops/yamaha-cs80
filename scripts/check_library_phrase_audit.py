#!/usr/bin/env python3
"""Check the additive expanded-library phrase capture, never the frozen audit.

This verifies coverage and numerical CSV values only. It does not authenticate
the producer/source, diagnose audibility, or authorize release/publication.
"""
from __future__ import annotations

import argparse
from collections import Counter
import csv
from decimal import Decimal, DecimalException
import json
import math
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / "tests/parity/tolerances-v2/library-phrase-v1.json"
HEADER = ("name,group,policy,rate,nonfinite,peak,peak_db,samples_over_1,softclip_frac_chord,"
          "rms_single_db,rms_chord_db,weighted_chord_db,loud_short_db,dc_single_rel,dc_chord_rel,"
          "dc_idle_abs,idle_rms,subsonic_peak,subsonic_rel,click_off,click_chord_off,"
          "click_retrigger,click_fresh_chord,click_steal,voices_at_end,fixture_id,library").split(",")
TEXT = {"name", "group", "policy", "fixture_id", "library"}
INTEGER = {"rate", "nonfinite", "samples_over_1", "voices_at_end"}
DECIBELS = {"peak_db", "rms_single_db", "rms_chord_db", "weighted_chord_db", "loud_short_db"}
NONNEGATIVE = set(HEADER) - TEXT - DECIBELS
# Schema domains from the reused reporter and its single Engine, not sound-
# quality acceptance limits. Ratios such as note-off scores may exceed one.
VOICE_CAPACITY = 8  # Engine::kNumVoices; this tool does not create a second Engine.
PHRASE_SECONDS = 12  # engine_audit.cpp T::length.


class Rejected(ValueError):
    """Coverage, input integrity or an unchanged numerical target failed."""


def require(condition, message):
    if not condition:
        raise Rejected(message)


def finite(value, where):
    require(isinstance(value, str) and bool(value.strip()), f"{where}: missing numeric value")
    try:
        result = float(value)
    except (ValueError, OverflowError) as error:
        raise Rejected(f"{where}: invalid number") from error
    require(math.isfinite(result), f"{where}: nonfinite value")
    return result


def exact_decimal(value, where):
    try:
        result = Decimal(value)
    except DecimalException as error:
        raise Rejected(f"{where}: invalid exact number") from error
    require(result.is_finite(), f"{where}: nonfinite exact number")
    return result


def validate_contract(contract):
    require(isinstance(contract, dict), "contract must be a JSON object")
    require(type(contract["schema_version"]) is int and contract["schema_version"] == 1
            and contract["scope"] == "expanded-library-phrase-v1", "unknown contract")
    require(contract["rates_hz"] == [44100, 48000, 96000], "registered rate grid changed")
    require(contract["policy"] == "source", "registered source policy changed")
    items = contract["fixtures"]
    counts = Counter(item["library"] for item in items)
    require(counts == {"legacy": 222, "curated": 301, "tone": 11}, "registered library coverage changed")
    ids = [item["fixture_id"] for item in items]
    require(ids == [f"program:{i}" for i in range(523)] + [f"tone-pair:{i}" for i in range(11)], "registered fixture IDs changed")
    require(all(set(item) == {"fixture_id", "library", "name", "group"} for item in items), "malformed fixture inventory")
    require(all(item["name"] and item["group"] for item in items), "empty fixture identity")
    # These are the existing targets, not fitted to expanded-library results.
    require(contract["maximums"] == {"click_retrigger": 1.0, "click_steal": 1.0,
                                     "peak": 1.0, "peak_db": 0.0, "samples_over_1": 0.0,
                                     "nonfinite": 0.0}, "numerical targets changed")
    require(contract["schema_domains"] == {"voice_capacity": VOICE_CAPACITY,
            "phrase_seconds": PHRASE_SECONDS, "softclip_fraction_range": [0, 1],
            "non_db_fields_are_nonnegative": True}, "reporter schema domains changed")


def verify(contract, inventory, capture, rows):
    validate_contract(contract)
    require(isinstance(capture, dict), "capture receipt must be a JSON object")
    require(isinstance(inventory, list), "capture inventory must be a JSON array")
    require(isinstance(rows, list), "CSV records must be a list")
    require(inventory == contract["fixtures"], "source library inventory differs from registered inventory")
    require(type(capture.get("schema_version")) is int and capture["schema_version"] == 1
            and capture.get("scope") == contract["scope"], "wrong capture namespace")
    require(capture.get("complete") is True and capture.get("diagnostic_subset") is False, "incomplete or diagnostic-only capture")
    require(capture.get("policy") == contract["policy"] and capture.get("rates_hz") == contract["rates_hz"], "wrong capture policy/rates")
    require(capture.get("hardware_measured") is False and capture.get("release_qualified") is False, "unsupported capture qualification claim")
    fixtures = {item["fixture_id"]: item for item in contract["fixtures"]}
    expected = {(item, rate) for item in fixtures for rate in contract["rates_hz"]}
    require(type(capture.get("rows_written")) is int and capture["rows_written"] == len(expected), "capture row count is incomplete")
    require(len(rows) == len(expected), f"expected {len(expected)} rows, received {len(rows)}")
    seen = set()
    breaches = []
    worst = {field: -math.inf for field in contract["maximums"]}
    for index, row in enumerate(rows, start=2):
        where = f"library-phrase.csv:{index}"
        require(isinstance(row, dict), f"{where}: CSV record must be a mapping")
        require(list(row) == HEADER, f"{where}: missing, extra, duplicate or reordered columns")
        require(all(isinstance(value, str) for value in row.values()), f"{where}: malformed CSV record")
        values = {field: finite(value, f"{where}:{field}") for field, value in row.items() if field not in TEXT}
        for field in INTEGER:
            # Parse counts exactly rather than accepting fractional text that
            # rounds to an integer in binary64 (for example 8.0000000000000001).
            exact = exact_decimal(row[field], f"{where}:{field}")
            require(exact == exact.to_integral_value() and exact >= 0, f"{where}:{field}: invalid count")
            values[field] = int(exact)
        for field in NONNEGATIVE:
            require(exact_decimal(row[field], f"{where}:{field}") >= 0, f"{where}:{field}: negative magnitude/count")
        require(exact_decimal(row["softclip_frac_chord"], f"{where}:softclip_frac_chord") <= 1,
                f"{where}: softclip fraction outside [0,1]")
        require(values["voices_at_end"] <= VOICE_CAPACITY, f"{where}: active voices exceed single-engine capacity")
        require(row["policy"] == "source", f"{where}: wrong render policy")
        fixture = fixtures.get(row["fixture_id"])
        require(fixture is not None, f"{where}: unknown fixture")
        require(all(row[key] == fixture[key] for key in ("name", "group", "library")), f"{where}: substituted fixture identity")
        key = (row["fixture_id"], int(values["rate"]))
        require(key in expected and key not in seen, f"{where}: wrong rate or duplicate fixture")
        seen.add(key)
        for field in ("nonfinite", "samples_over_1"):
            require(values[field] <= key[1] * PHRASE_SECONDS, f"{where}:{field}: count exceeds rendered stereo frames")
        for field, limit in contract["maximums"].items():
            worst[field] = max(worst[field], values[field])
            if values[field] > limit:
                breaches.append({"fixture_id": key[0], "name": row["name"], "rate": key[1],
                                 "metric": field, "value": values[field], "maximum": limit})
    require(seen == expected, "missing fixture/rate combination")
    return {"scope": contract["scope"], "rows": len(rows), "coverage_complete": True,
            "numerical_targets_met": not breaches, "breaches": breaches, "worst": worst,
            "hardware_measured": False, "release_qualified": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    args = parser.parse_args()
    try:
        contract = json.loads(CONTRACT.read_text())
        inventory = json.loads((args.capture / "inventory.json").read_text())
        capture = json.loads((args.capture / "capture.json").read_text())
        with (args.capture / "library-phrase.csv").open(newline="") as stream:
            reader = csv.DictReader(stream)
            require(reader.fieldnames == HEADER, "missing, extra, duplicate or reordered CSV header")
            rows = list(reader)
        result = verify(contract, inventory, capture, rows)
        print(json.dumps(result, indent=2, allow_nan=False))
        return 0 if result["numerical_targets_met"] else 1
    except (Rejected, OSError, ValueError, KeyError, TypeError, csv.Error, DecimalException) as error:
        print(f"expanded library audit rejected: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
