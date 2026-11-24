#!/usr/bin/env python3
"""Read existing source/evidence only; write the A54 consistency receipt."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
BASE = ROOT / "docs/preset-evidence/2026.11.7"
SOUND = "c5549f7d095a5c5f49a5ad22ec0ccc819245d201"
FINAL = "ee0dabcacb4cdff93f8803844f60348e0ceda6ed"
TREE = "17115913cbb29157bd00539485bcc5f38b57219c"
inputs: dict[str, dict[str, object]] = {}
checks: list[dict[str, object]] = []


def read(path: str) -> bytes:
    data = (ROOT / path).read_bytes()
    inputs[path] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    return data


def evidence(path: str) -> dict:
    return json.loads(read(f"docs/preset-evidence/2026.11.7/{path}"))


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def check(name: str, condition: bool) -> None:
    checks.append({"name": name, "passed": bool(condition)})


def main() -> None:
    a12 = evidence("verification/a12/receipt.json")
    classification = evidence("verification/a12/current-value-classification.json")
    mechanisms = evidence("verification/a12/mechanism-coverage.json")
    a33 = evidence("verification/a33/metrics-summary.json")
    a33_receipt = evidence("verification/a33/receipt.json")
    init = evidence("verification/a27/audio.json")
    performance = evidence("verification/a37/summary.json")
    sanitizer = evidence("verification/a38/sanitizer-receipt.json")
    lifecycle = evidence("verification/a38/lifecycle-receipt.json")
    continuity = evidence("verification/a40/source-ee0dabca/source-continuity.json")
    materials = evidence("verification/a41/receipt.json")
    cleanup = evidence("development/temporary-pcm-cleanup-second-pass.json")

    check("frozen final tree", git("rev-parse", f"{FINAL}^{{tree}}") == TREE)
    check("core and plugin source continuity", not git("diff", "--name-only", SOUND, FINAL,
          "--", "src/core", "src/plugin"))
    check("current core and plugin retain frozen source", not git("diff", FINAL,
          "--", "src/core", "src/plugin"))
    check("A12 exact complete inventory", a12["passed"] and a12["all_523_complete_program_records_exact"]
          and a12["inventory_bytes_exact_except_revision"]
          and a12["names_categories_order_host_indices_tags_descriptions_unchanged"])
    check("unchanged .6 values and parameter contract", (a12["core_parameter_values_exact"],
          a12["core_parameter_count"], a12["host_parameter_count"], a12["body_parameter_count"])
          == (106692, 204, 310, 14))
    check("zero new recipes or identities", all(a12[k] == 0 for k in
          ("new_presets", "new_ids", "changed_ids_or_ranges", "new_7_recipe_value_edits")))
    check("catalogue partition", (a12["original_program_count"], a12["curated_program_count"])
          == (222, 301))
    check("cumulative control partition", classification["passed"]
          and classification["cumulative_musical_programs_changed"] == 521
          and classification["beyond_conservative_levels_pan"] == 493
          and classification["additive_vca_baseline_only"] == 28
          and classification["all_28_have_active_velocity_route"])
    expected = {"body": (272, 477), "piano_partial_decay": (10, 20),
                "noise_rate_normalization": (256, 447),
                "generic_string_formants": (54, 100), "low_choir_harmonics": (7, 12)}
    check("conditional mechanism populations", all(
          (mechanisms["summary"][k]["program_count"], mechanisms["summary"][k]["rank_count"]) == v
          for k, v in expected.items()) and mechanisms["union_program_count"] == 256
          and mechanisms["musical_programs_outside_union"] == 265)
    accounting = a33["accounting"]
    check("cumulative signal partition", a33_receipt["passed"]
          and accounting["changed_default_signal_vectors"] == 519
          and accounting["additional_pressure_required_programs"] == 2
          and accounting["union_musical_with_signal_change_evidence"] == 521)
    check("new default metric population", a33["mechanism_observation_6_to_7"]["body"]
          ["changed_in_bank_probe"] == 67 and accounting["new_7_recipe_edits"] == 0)
    method = a33["capture_method"]
    check("default measurement duration rate and note partition", method["sample_rate"] == 48000
          and method["hold_seconds"] == 3 and method["tail_seconds"] == 1
          and method["pressure"] == 0 and method["both_ranks"]
          and method["midi_note_counts_all_523"] == {"48": 50, "60": 380, "67": 70, "72": 23})
    check("bounded capture safety counts", all(x["programs"] == 523 and x["nonfinite"] == 0
          and x["samples_over_full_scale"] == 0 for x in a33["safety"].values()))
    check("twelve exact stereo Init cases", len(init["cases"]) == 12
          and init["all_exact_vs_baseline6"] and init["all_exact_vs_published4"]
          and all(x["exact_vs_baseline6"] and x["exact_vs_published4"] for x in init["cases"])
          and sum(x["stereo_float_samples"] for x in init["cases"]) == 1504800)
    cost = {x["case"]: x for x in performance["cases"]}
    check("bounded low choir CPU observations", round(cost["ah_low_eight"]["paired_cpu_ratio_median"], 3)
          == 1.153 and round(cost["oo_low_eight"]["paired_cpu_ratio_median"], 3) == 1.167
          and performance["total_trials"] == 88 and not performance["native_plugin_validation"])
    check("existing sanitizer receipts remain bounded successful executions",
          len(sanitizer["runs"]) == 4 and len(lifecycle["runs"]) == 1
          and all(x["compile_exit"] == 0 and x["run_exit"] == 0
                  for x in sanitizer["runs"] + lifecycle["runs"])
          and not sanitizer["leak_checking"] and not lifecycle["leak_checking"])
    check("A40 source-only continuity", continuity["source"] == FINAL and continuity["tree"] == TREE
          and continuity["retained_audio_reviews_applicable_to_unchanged_sound_source"]
          and not continuity["binary_byte_identity_claimed"]
          and not continuity["new_pcm_byte_identity_claimed"]
          and not continuity["original_c554_platform_or_artifact_receipts_rebound"])
    check("all specialized selectors accounted", materials["pass_within_stated_scope"]
          and materials["summary"]["materials"] == 9 and materials["summary"]["variants"] == 4
          and materials["summary"]["specialized_programs"] == 18
          and materials["summary"]["specialized_ranks"] == 34
          and materials["summary"]["ignored_nonzero_selectors"] == 0)
    check("temporary PCM retention boundary recorded", cleanup["raw_pcm_no_longer_retained"]
          and cleanup["removed_files"] == 150)

    reviews = [4, 5, 6, 7, 11, 12, *range(13, 28), 33, 34, 35, 36, 37, 38, 40, 41]
    for number in reviews:
        read(f"docs/preset-evidence/2026.11.7/reviews/a{number:02}.md")
    for path in ["README.md", "docs/CIRCUIT_LIBRARY_2026_11_7.md", "docs/PRESET_GUIDE.md",
                 "src/core/InstrumentBody.h", "src/core/Params.h",
                 "docs/preset-evidence/2026.11.7/verification/a27/verify.py"]:
        read(path)
    for path in ["verification/a12/metadata-contract.json", "verification/a27/controls.json",
                 "verification/a23/published-pressure-comparison.json",
                 "verification/a33/causal-velocity-summary.json"]:
        evidence(path)
    passed = all(x["passed"] for x in checks)
    result = {
        "schema": 1,
        "scope": "Read-only sound-claim/evidence consistency review; no new product tests, audio renders, listening or native acceptance",
        "sound_source": SOUND, "binary_source": FINAL, "binary_tree": TREE,
        "checkout_head_at_review": git("rev-parse", "HEAD"),
        "checks": checks, "checks_count": len(checks), "failed_checks": sum(not x["passed"] for x in checks),
        "passed_within_scope": passed,
        "exact_acoustic_or_hyperrealism_goal_established": False,
        "listening_approval_claimed": False, "fresh_native_or_archive_qualification_claimed": False,
        "all_family_temporary_pcm_still_present_claimed": False,
        "cumulative_accounting": accounting,
        "mechanism_counts": {k: {"programs": v[0], "active_ranks": v[1]} for k, v in expected.items()},
        "new_default_metric_changes": 67, "structurally_eligible_union": 256,
        "init_cases": len(init["cases"]), "init_stereo_float_samples_per_revision": 1504800,
        "low_eight_note_choir_cpu_ratios": {k: cost[k]["paired_cpu_ratio_median"]
                                           for k in ("ah_low_eight", "oo_low_eight")},
        "inputs": inputs,
        "review_script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    }
    (HERE / "receipt.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"checks": len(checks), "failures": result["failed_checks"],
                      "passed_within_scope": passed, "output": str(HERE / "receipt.json")}))
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
