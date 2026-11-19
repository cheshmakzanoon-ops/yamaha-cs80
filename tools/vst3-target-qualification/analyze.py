#!/usr/bin/env python3
"""Blind, fixed stock-only clock qualification; consumes a complete runner summary."""
import argparse
import json
import math
from pathlib import Path
import statistics
import sys

SCHEMA = "cs80-vst3-stock-interleave-v1"
COUNTS = {"S0": 512, "S1": 512, "S+": 544}
TRIPLETS = 12
# Two-sided 95% Student t quantile for the fixed 12 fresh process triplets.
T_CRIT = 2.200985160082949


def strict_object(items):
    result = {}
    for key, value in items:
        if key in result:
            raise ValueError(f"duplicate JSON field: {key}")
        result[key] = value
    return result


def require(condition, detail):
    if not condition:
        raise ValueError(detail)


def cpu_ns(value):
    # 60 seconds is an integrity bound per measured arm, not an outlier filter.
    return type(value) is int and 0 < value <= 60_000_000_000


def confidence(values, minimum, maximum):
    mean = statistics.mean(values)
    width = T_CRIT * statistics.stdev(values) / math.sqrt(TRIPLETS)
    point, lower, upper = (100 * math.expm1(x) for x in (mean, mean - width, mean + width))
    return {
        "point_percent": point,
        "interval_95_percent": [lower, upper],
        "required_interval_percent": [minimum, maximum],
        "pass": minimum <= lower <= upper <= maximum,
    }


def analyze(summary):
    require(isinstance(summary, dict) and summary.get("schema") == SCHEMA,
            "expected supported stock interleave summary")
    require(summary.get("complete") is True, "expected complete acquisition")
    acquisition = summary.get("acquisition_class")
    require(acquisition in ("dedicated-target", "cloud-diagnostic"),
            "expected measured dedicated-target or cloud-diagnostic acquisition")
    session_id = summary.get("session_id")
    require(isinstance(session_id, str) and session_id.strip(), "missing session identity")
    triplets = summary.get("triplets")
    require(isinstance(triplets, list) and len(triplets) == TRIPLETS,
            "expected exactly 12 fresh process triplets")
    placebo, positive, common = [], [], []
    rates = {role: [] for role in COUNTS}
    for index, row in enumerate(triplets):
        require(isinstance(row, dict) and type(row.get("index")) is int and row["index"] == index,
                f"triplet {index}: missing, duplicate or reordered observation")
        measured = row.get("measured_calls")
        require(measured == COUNTS and all(type(n) is int for n in measured.values()),
                f"triplet {index}: wrong callback counts")
        values = row.get("cpu_ns")
        require(isinstance(values, dict) and set(values) == set(COUNTS)
                and all(cpu_ns(n) for n in values.values()),
                f"triplet {index}: invalid thread CPU totals")
        common_ns, extra_ns = row.get("positive_common_cpu_ns"), row.get("positive_extra_cpu_ns")
        require(cpu_ns(common_ns) and cpu_ns(extra_ns) and common_ns + extra_ns == values["S+"],
                f"triplet {index}: positive-work decomposition missing or inconsistent")
        log0, log1, logp = (math.log(values[role]) for role in COUNTS)
        center = (log0 + log1) / 2
        placebo.append(log1 - log0)
        positive.append(logp - center)
        common.append(math.log(common_ns) - center)
        for role in rates:
            rates[role].append(values[role] / COUNTS[role])
    gates = {
        "same_stock_twins": confidence(placebo, -2, 2),
        "same_stock_positive_common_calls": confidence(common, -2, 2),
        "same_stock_with_extra_calls": confidence(positive, 3, 10),
    }
    gates_pass = all(gate["pass"] for gate in gates.values())
    return {
        "schema": "cs80-vst3-stock-interleave-analysis-v1",
        "session_id": session_id,
        "acquisition_class": acquisition,
        "triplet_count": TRIPLETS,
        "gates_pass": gates_pass,
        "qualified": acquisition == "dedicated-target" and gates_pass,
        "gates": gates,
        "median_callback_thread_cpu_ns_descriptive": {
            role: statistics.median(values) for role, values in rates.items()
        },
        "scope": ("cloud-only diagnostic; not dedicated-target qualification or a CPU saving"
                  if acquisition == "cloud-diagnostic"
                  else "same-binary stock calibration, not a candidate or DAW CPU claim"),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("summary", type=Path)
    args = parser.parse_args()
    try:
        result = analyze(json.loads(args.summary.read_text(encoding="utf-8"),
                                    object_pairs_hook=strict_object))
    except (OSError, ValueError) as exc:
        print(f"invalid acquisition: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, allow_nan=False))
    return 0 if result["gates_pass"] else 2


if __name__ == "__main__":
    sys.exit(main())
