#!/usr/bin/env python3
"""Compare ordinary before/after parity captures; no rendering or fault probes."""

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def samples(path):
    with path.open(newline="") as stream:
        rows = csv.DictReader(stream)
        assert rows.fieldnames == ["sample", "value"], path
        values = []
        for index, row in enumerate(rows):
            assert int(row["sample"]) == index, path
            value = float(row["value"])
            assert math.isfinite(value), path
            values.append(value)
    assert len(values) == 96000, path
    return values


def metrics(values):
    return {
        "samples": len(values),
        "finite": all(math.isfinite(value) for value in values),
        "nonzero_samples": sum(value != 0 for value in values),
        "minimum": min(values),
        "maximum": max(values),
        "peak_absolute": max(abs(value) for value in values),
        "mean": math.fsum(values) / len(values),
        "rms": math.sqrt(math.fsum(value * value for value in values) / len(values)),
    }


def compare(before, after):
    result = {
        "schema": "cs80-a39-exact-capture-comparison-v1",
        "scope": "Same-host ordinary Release captures using the identical frozen core archive; not a Windows rerun or cross-platform bit-identity claim.",
        "before_directory": str(before),
        "after_directory": str(after),
        "fixtures": {},
    }
    for fixture in ("default", "live"):
        expected = {"capture.json", "left.csv", "right.csv"}
        if fixture == "live":
            expected.add("envelope-events.csv")
        left, right = before / fixture, after / fixture
        assert {path.name for path in left.iterdir()} == expected, left
        assert {path.name for path in right.iterdir()} == expected, right
        files = {}
        for name in sorted(expected):
            a, b = (left / name).read_bytes(), (right / name).read_bytes()
            files[name] = {
                "before_bytes": len(a),
                "after_bytes": len(b),
                "before_sha256": sha256(a),
                "after_sha256": sha256(b),
                "byte_equal": a == b,
            }
            assert a == b, (fixture, name)
        channels = {}
        for channel in ("left.csv", "right.csv"):
            a, b = samples(left / channel), samples(right / channel)
            difference = [abs(x - y) for x, y in zip(a, b, strict=True)]
            channels[channel] = {
                "before": metrics(a),
                "after": metrics(b),
                "metrics_exactly_equal": metrics(a) == metrics(b),
                "unequal_samples": sum(x != y for x, y in zip(a, b, strict=True)),
                "maximum_absolute_sample_difference": max(difference),
            }
            assert channels[channel]["metrics_exactly_equal"]
            assert channels[channel]["unequal_samples"] == 0
        result["fixtures"][fixture] = {"files": files, "channels": channels}
    result["all_seven_files_byte_equal"] = True
    result["total_audio_samples_compared"] = 2 * 2 * 96000
    result["all_metrics_exactly_equal"] = True
    result["maximum_absolute_sample_difference"] = 0.0
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    result = compare(arguments.before, arguments.after)
    arguments.output.write_text(json.dumps(result, indent=2) + "\n")
    print("All seven capture files are byte-identical; 384000 audio samples have zero difference.")
