#!/usr/bin/env python3
"""Opt-in event-minus-no-event click diagnostic; never relaxes raw phrase gate."""

import argparse
import csv
import json
import math
import subprocess
import tempfile
from pathlib import Path


def rows(path):
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def unique(items, **fields):
    matched = [row for row in items if all(str(row.get(k)) == str(v) for k, v in fields.items())]
    if len(matched) != 1:
        raise AssertionError(f"expected one {fields} row, got {len(matched)}")
    return matched[0]


def close(actual, expected, relative, what):
    if not math.isfinite(actual) or not math.isclose(actual, expected, rel_tol=relative, abs_tol=1e-6):
        raise AssertionError(f"{what}: observed {actual:.9g}, registered {expected:.9g} ±{relative:.1%}")


def check_fixture(audit_binary, registered, case, root):
    name, rate, event = case["name"], case["rate"], case["event"]
    destination = root / ("bass" if event == "retrigger_on" else "funky")
    command = [str(audit_binary), "--out", str(destination), "--suite", "phrase", "--rates", str(rate),
               "--match", name, "--explain-clicks", "--counterfactual"]
    subprocess.run(command, check=True, capture_output=True, text=True, timeout=180)
    phrase = unique(rows(destination / "phrase.csv"), name=name, policy="source", rate=rate)
    counterfactual = unique(rows(destination / "counterfactual.csv"), name=name,
                            policy="source", rate=rate, event=event)
    diagnostic = unique(rows(destination / "click_diagnostics.csv"), name=name,
                        policy="source", rate=rate, event=event)

    # The original <=1 acceptance score still exists and is not reinterpreted.
    original_score = float(phrase[case["raw_column"]])
    close(original_score, case["observed_score"], registered["reference_numeric_rel"], name + " raw gate")
    close(float(counterfactual["observed_window_score"]), original_score,
          1e-4, name + " raw score preserved by opt-in diagnostic")
    close(float(counterfactual["without_event_window_score"]), case["without_event_score"],
          registered["reference_numeric_rel"], name + " immutable no-event baseline")
    # Existing first-sample probe comes BEFORE the oversampling output's
    # reported 63/67-sample latency. The original and no-event streams must
    # still be indistinguishable at this instant.
    immediate = abs(float(counterfactual["first_difference_step"]))
    if immediate > registered["first_sample_event_step_abs_max"]:
        raise AssertionError(f"{name}: causal output before reported DSP latency ({immediate:.9g})")

    # New additive diagnostic columns: event-only derivative, divided by the
    # original pre-event/chord reference, measured after measured DSP latency.
    # A natural 8 ms attack may exceed one; only the early 2 ms can serve as a
    # narrow event-edge check. No Yamaha hardware step limit is asserted.
    early = float(counterfactual["event_local_2ms_score"])
    full = float(counterfactual["event_local_8ms_score"])
    close(early, case["early_event_step_ratio"], registered["reference_numeric_rel"],
          name + " event-local early derivative")
    close(full, case["full_window_event_step_ratio"], registered["reference_numeric_rel"],
          name + " event-local full window")
    if early > registered["early_event_step_ratio_max"]:
        raise AssertionError(f"{name}: causal early derivative {early:.6g} exceeds preregistered software screen")
    reference = float(diagnostic["reference_step"])
    if reference <= 0:
        raise AssertionError(f"{name}: invalid original click reference")
    close(early, float(counterfactual["maximum_early_step_after_latency"]) / reference,
          2e-4, name + " early ratio uses existing event-minus-control step")
    close(full, float(counterfactual["maximum_difference_step"]) / reference,
          2e-4, name + " full ratio uses existing event-minus-control step")
    print(f"{name} {rate}: raw={original_score:.5f}, no-event={case['without_event_score']:.5f}, "
          f"event-local early={early:.6f}, full={full:.6f}; raw <=1 gate remains independent")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--audit", required=True, type=Path)
    args = parser.parse_args()
    registration_path = Path(__file__).resolve().parent / "parity/tolerances-v2/ws1-voice.json"
    registration = json.loads(registration_path.read_text(encoding="utf-8"))["stages"]
    registered = registration["click_event_causality_diagnostic"]
    with tempfile.TemporaryDirectory(prefix="cs80-click-causal-") as temp:
        for case in registered["fixtures"]:
            check_fixture(args.audit.resolve(), registered, case, Path(temp))


if __name__ == "__main__":
    main()
