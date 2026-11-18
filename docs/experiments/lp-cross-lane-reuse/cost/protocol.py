"""Frozen, non-adaptive design. Importing this file never executes a renderer."""
import math
import random

SCHEMA = "lane-core-screen-v1"
CASES = ["factory-moving", "factory-held", "panel-moving", "pure-held"]
PRIMARY = CASES[0]
WARM, BLOCKS, FRAMES = 1024, 512, 64
AA_PAIRS, COMPARISON_PAIRS = 2, 6
BOOTSTRAP_SEED, BOOTSTRAP_DRAWS = 2026111809, 10000
SLOT_CPU_LIMIT, SLOT_WALL_LIMIT, CAMPAIGN_WALL_LIMIT = 10, 12, 600
MINIMUM_GAIN, CONTROL_FLOOR = .02, -.03


def need(ok, why):
    if not ok:
        raise ValueError(why)


def schedule():
    result = []
    for family, pairs in [("aa", AA_PAIRS), ("comparison", COMPARISON_PAIRS)]:
        for pair in range(pairs):
            offset = (pair + (2 if family == "comparison" else 0)) % len(CASES)
            order = CASES[offset:] + CASES[:offset]
            for case in order:
                orientations = ["ABBA", "BAAB"]
                if (pair + CASES.index(case)) % 2:
                    orientations.reverse()
                # One whole paired-reversal unit is eight adjacent processes
                # of the SAME scene. No timing-dependent interleaving.
                for quartet, orientation in enumerate(orientations):
                    for slot, label in enumerate(orientation):
                        result.append(dict(index=len(result), family=family, pair=pair,
                            case=case, quartet=quartet, orientation=orientation,
                            slot=slot, label=label,
                            arm="candidate" if family == "comparison" and label == "B" else "stock"))
    return result


def percentile(sorted_values, p):
    position = (len(sorted_values) - 1) * p
    low = int(position)
    return sorted_values[low] + (sorted_values[min(low + 1, len(sorted_values) - 1)] - sorted_values[low]) * (position - low)


def gain(log_ratio):
    return -math.expm1(-log_ratio)


def contrast(rows):
    a = [math.log(r["cpu_ns"]) for r in rows if r["label"] == "A"]
    b = [math.log(r["cpu_ns"]) for r in rows if r["label"] == "B"]
    need(len(a) == len(b) and len(a) in (2, 4), "incomplete contrast")
    return sum(a) / len(a) - sum(b) / len(b)


def analyze(rows):
    expected = schedule()
    need(len(rows) == len(expected) == 256, "incomplete campaign; no prefix estimate")
    for row, slot in zip(rows, expected):
        need(all(row.get(k) == v for k, v in slot.items()), "schedule changed")
        need(row["cpu_ns"] > row["empty_cpu_ns"] > 0, "invalid clock observation")
    rng = random.Random(BOOTSTRAP_SEED)
    # Unit is a complete eight-process ABBA+BAAB reversal pair, not an
    # individual callback, process, or quartet. Shared draws retain scene pairing.
    draws = [rng.choices(range(COMPARISON_PAIRS), k=COMPARISON_PAIRS) for _ in range(BOOTSTRAP_DRAWS)]
    results = {}
    for case in CASES:
        family_pairs, calibration_quartets, orientation_comparisons = {}, [], {"ABBA": [], "BAAB": []}
        for family, count in [("aa", AA_PAIRS), ("comparison", COMPARISON_PAIRS)]:
            family_pairs[family] = []
            for pair in range(count):
                unit = [r for r in rows if (r["case"], r["family"], r["pair"]) == (case, family, pair)]
                need(len(unit) == 8, "incomplete whole reversal pair")
                family_pairs[family].append(contrast(unit))
                for orientation in ("ABBA", "BAAB"):
                    value = contrast([r for r in unit if r["orientation"] == orientation])
                    if family == "aa":
                        calibration_quartets.append(value)
                    else:
                        orientation_comparisons[orientation].append(value)
        values = family_pairs["comparison"]
        bootstrap = sorted(sum(values[i] for i in draw) / COMPARISON_PAIRS for draw in draws)
        aa_max = max(abs(gain(x)) for x in family_pairs["aa"] + calibration_quartets)
        clock = max(r["empty_cpu_ns"] / r["cpu_ns"] for r in rows if r["case"] == case)
        results[case] = dict(gain=gain(sum(values) / COMPARISON_PAIRS),
            ci95=[gain(percentile(bootstrap, .025)), gain(percentile(bootstrap, .975))],
            comparison_pair_gain=[gain(x) for x in values],
            comparison_orientation_gain={k: gain(sum(v) / len(v)) for k, v in orientation_comparisons.items()},
            aa_pair_gain=[gain(x) for x in family_pairs["aa"]],
            aa_quartet_gain=[gain(x) for x in calibration_quartets],
            aa_max_absolute_gain=aa_max, max_empty_clock_fraction=clock)
    primary = results[PRIMARY]
    floor = max(MINIMUM_GAIN, primary["aa_max_absolute_gain"], 3 * primary["max_empty_clock_fraction"])
    passed = primary["ci95"][0] > floor and all(results[c]["ci95"][0] >= CONTROL_FLOOR for c in CASES if c != PRIMARY)
    return dict(complete=True, slots=len(rows), bootstrap_unit="whole eight-process reversal pair",
        primary=PRIMARY, primary_floor=floor, controls_floor=CONTROL_FLOOR, cases=results,
        candidate_for_separate_native_plugin_study=passed,
        scope="Offline whole core plus control/event handling only; not measured plugin/DAW CPU, callback latency, or realtime capacity")


def self_test():
    slots = schedule()
    need(len(slots) == 256, "wrong length")
    # Synthetic values only. Never call a renderer or clock during self-tests.
    # Ratio logic must keep B's sign correct in BOTH orientations, cancel
    # linear/quadratic local drift over the whole reversal pair, and reject
    # incomplete schedules. Same draws resample whole pairs across all scenes.
    rows = []
    for slot in slots:
        local = slot["quartet"] * 4 + slot["slot"]
        ratio = .95 if slot["family"] == "comparison" and slot["label"] == "B" else 1.
        value = 1_000_000 * ratio * math.exp(.002 * local + .0001 * local * local)
        rows.append({**slot, "cpu_ns": value, "empty_cpu_ns": 100.})
    result = analyze(rows)
    for record in result["cases"].values():
        need(abs(record["gain"] - .05) < 1e-12, "orientation/drift contrast incorrect")
        need(max(abs(x) for x in record["aa_pair_gain"]) < 1e-12, "paired quadratic cancellation broken")
    need(result["candidate_for_separate_native_plugin_study"], "synthetic gate failed")
    try:
        analyze(rows[:-1])
    except ValueError:
        pass
    else:
        raise ValueError("prefix analysis accepted")
    regressed = [{**r, "cpu_ns": r["cpu_ns"] * (1.10 if r["case"] == "pure-held" and r["family"] == "comparison" and r["label"] == "B" else 1.)} for r in rows]
    need(not analyze(regressed)["candidate_for_separate_native_plugin_study"], "negative-control gate ignored")
    no_gain = [{**r, "cpu_ns": 1_000_000., "empty_cpu_ns": 100.} for r in rows]
    need(not analyze(no_gain)["candidate_for_separate_native_plugin_study"], "zero effect admitted")
    return dict(passed=True, synthetic_only=True, slots=256, independent_comparison_units_per_scene=6,
        independent_aa_units_per_scene=2, tests=["balanced labels", "both orientations", "whole reversal-pair bootstrap",
            "linear/quadratic drift cancellation", "no prefix estimate", "noninferiority rejection", "zero-gain rejection"])


if __name__ == "__main__":
    import json
    print(json.dumps(self_test(), indent=2))
