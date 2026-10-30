#!/usr/bin/env python3
"""Validate this bounded measurement's coverage/integrity, not its timing speed."""
import csv
import json
from collections import Counter
from pathlib import Path

root = Path(__file__).resolve().parent
rows = list(csv.DictReader((root / "results.csv").open()))
metadata = json.loads((root / "metadata.json").read_text())
completed = json.loads((root / "completed.json").read_text())
assert metadata["source_commit_caller_verified"] == "d7b6e8510e42cf8bd7f35edad49c81795bc48801"
assert metadata["schema_version"] == 2 and metadata["suite"] == "performance"
assert metadata["callbacks_per_held_phase"] == 1024 and not metadata["quick_subset"]
assert not metadata["performance_include_64"]
assert completed == {"completed": True, "cases": 156, "rows": 400}
assert len(rows) == 400
assert Counter(r["mode"] for r in rows) == {"source_performance": 352, "isolation": 24, "legacy": 24}
keys = [(r["rate"], r["block"], r["mode"], r["scenario"], r["initial_selector"], r["phase"]) for r in rows]
assert len(set(keys)) == len(keys), "Duplicate measurement rows"
for rate in ("44100", "48000"):
    for block in ("128", "256"):
        for scenario in ("paired_tones", "mpe_chorus_reverb", "mpe_phaser_delay_drive"):
            phases = ("warmed_held", "release_tail") if scenario == "paired_tones" else (
                "warmed_held", "sustain_pedal_hold", "release_tail")
            for phase in phases:
                selected = [r for r in rows if (r["rate"], r["block"], r["mode"], r["scenario"], r["phase"])
                            == (rate, block, "source_performance", scenario, phase)]
                assert len(selected) == 11 and {int(r["initial_selector"]) for r in selected} == set(range(11))
        for mode in ("legacy", "isolation"):
            for phase in ("warmed_held", "release_tail"):
                selected = [r for r in rows if (r["rate"], r["block"], r["mode"], r["phase"])
                            == (rate, block, mode, phase)]
                assert len(selected) == 3 and {int(r["initial_selector"]) for r in selected} == {0, 1, 10}
for row in rows:
    n = int(row["callbacks"])
    expressive = row["scenario"].startswith("mpe_")
    phase = row["phase"]
    assert int(row["source_compilations"]) == 0
    assert int(row["nonfinite_count"]) == 0
    assert (row["source_performance"] == "1") == (row["mode"] == "source_performance")
    assert (row["isolation"] == "1") == (row["mode"] == "isolation")
    if metadata["thread_cpu_clock_available"]:
        assert float(row["thread_cpu_seconds_including_checks"]) > 0
    if phase == "warmed_held":
        assert n == 1024
    if phase != "release_tail":
        assert row["min_active_voices"] == row["max_active_voices"] == row["final_active_voices"] == "8"
    else:
        assert row["final_active_voices"] == "0" and row["tail_voices_complete"] == "1"
        assert float(row["audio_seconds"]) >= (3 if expressive else 1)
    if expressive and phase != "release_tail":
        assert int(row["mpe_member_state_checks"]) == 8 * n
        assert float(row["pra_target_max_ohms"]) > float(row["pra_target_min_ohms"]) > 0
        assert int(row["controller_events"]) == 27 * n + (16 if phase == "warmed_held" else 1)
        assert int(row["pedal_held_callbacks"]) == (n if phase == "sustain_pedal_hold" else 0)
    elif expressive:
        assert row["controller_events"] == "1"
    else:
        assert row["controller_events"] == row["mpe_member_state_checks"] == row["pedal_held_callbacks"] == "0"
print(f"PASS: {len(rows)} rows, {sum(int(r['callbacks']) for r in rows)} callbacks; "
      f"full mode/profile/rate/buffer/tab matrix and controller/voice/finite integrity verified.")
print(f"Observed budget exceedances (not a pass/fail gate): {sum(int(r['budget_exceedances']) for r in rows)}")
print(f"Observed sample values at/above full scale: {sum(int(r['abs_at_or_above_one']) for r in rows)}")
