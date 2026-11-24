#!/usr/bin/env python3
"""Join retained evidence only. Does not compile, render, test, or read raw PCM."""
from __future__ import annotations

import csv
import hashlib
import json
from collections import Counter
from pathlib import Path

OUT = Path(__file__).resolve().parent
BASE = OUT.parents[1]
ROOT = BASE.parents[2]
INPUTS: dict[str, dict] = {}


def read(path: str) -> bytes:
    p = BASE / path
    raw = p.read_bytes()
    INPUTS[path] = {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
    return raw


def js(path: str):
    return json.loads(read(path))


def rows(path: str):
    return list(csv.DictReader(read(path).decode().splitlines()))


def write_json(name: str, value):
    (OUT / name).write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n")


def write_csv(name: str, records: list[dict]):
    with (OUT / name).open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(records[0]))
        writer.writeheader()
        for record in records:
            writer.writerow({k: ";".join(map(str, v)) if isinstance(v, list) else v
                             for k, v in record.items()})


inventory = js("verification/a12/inventory.json")
assert INPUTS["verification/a12/inventory.json"]["sha256"] == \
    "55c08568074a6560ef10c8c525733a40aec42d9b8c6d20644bf4984cdd0d3e0d"
programs = inventory["programs"]
by_name = {p["name"]: p for p in programs}
assert len(programs) == len(by_name) == 523
assert [p["host_index"] for p in programs] == list(range(523))
assert len(inventory["parameter_ids"]) == 204
assert all(len(p["values"]) == 204 for p in programs)
classification = js("verification/a12/current-value-classification.json")
mechanisms = js("verification/a12/mechanism-coverage.json")
mechanism_by_host = {p["host_index"]: p for p in mechanisms["programs"]}
deltas = rows("verification/a33/program-deltas.csv")
delta_by_host = {int(p["host_index"]): p for p in deltas}
summary33 = js("verification/a33/metrics-summary.json")
category33 = {p["category"]: p for p in summary33["categories"]}
bank_metrics = {
    "published4": rows("development/published-4-bank-metrics.csv"),
    "baseline6": rows("development/baseline-bank-metrics.csv"),
    "current7": rows("development/current-bank-metrics.csv"),
}
bank_by_host = {}
signal_columns = summary33["capture_method"]["signal_columns_compared"]
for label, bank in bank_metrics.items():
    assert len(bank) == 523
    lookup = {int(r["index"]): r for r in bank}
    assert sorted(lookup) == list(range(523))
    for p in programs:
        r = lookup[p["host_index"]]
        assert r["name"] == p["name"] and r["category"] == p["category"]
        assert int(r["note"]) == int(delta_by_host[p["host_index"]]["default_note"])
        assert int(r["sample_rate"]) == 48000
        assert abs(float(r["velocity"]) - 0.8) < 1e-7 and float(r["pressure"]) == 0
        assert float(r["hold_seconds"]) == 3 and float(r["tail_seconds"]) == 1
        assert int(r["nonfinite"]) == 0 and int(r["samples_over_full_scale"]) == 0
    bank_by_host[label] = lookup
for h, d in delta_by_host.items():
    for before, column in [("published4", "signal_metrics_changed_4_to_7"),
                           ("baseline6", "signal_metrics_changed_6_to_7")]:
        changed = any(float(bank_by_host[before][h][c]) != float(bank_by_host["current7"][h][c])
                      for c in signal_columns)
        assert changed == (d[column] == "True"), (h, before)
timing = js("verification/a34/timing-census.json")
slow_by_host = {p["index"]: p for p in timing["programs_beyond_3s"]}
matched = js("verification/a34/matched-bank-scope.json")
focused = js("development/focused-ctest-receipt.json")
continuity = js("verification/a40/source-ee0dabca/source-continuity.json")
audition = js("verification/a40/source-ee0dabca/prepared-contract.json")
assert continuity["source"] == "ee0dabcacb4cdff93f8803844f60348e0ceda6ed"
assert continuity["tree"] == "17115913cbb29157bd00539485bcc5f38b57219c"
assert focused["passed"] == 31 and focused["failed"] == 0
assert classification["new_7_preset_parameter_edits"] == 0
assert mechanisms["union_program_count"] == 256

FAMILIES = {
    "Hardware Studies": ("a26", 22), "Brass": ("a16", 59),
    "Strings": ("a18", 59), "Pads": ("a21", 63), "Bass": ("a14", 50),
    "Leads": ("a23", 56), "Keys": ("a13", 45), "FX": ("a25", 46),
    "Sequences": ("a24", 27), "Polysynth": ("a22", 25),
    "Organ": ("a20", 15), "Winds": ("a19", 14),
    "Plucked": ("a15", 17), "Bells": ("a17", 23), "Init": ("a27", 2),
}
for category, (agent, expected) in FAMILIES.items():
    assert sum(p["category"] == category for p in programs) == expected
    read(f"reviews/{agent}.md")
for agent in ["a12", "a33", "a34", "a35", "a36", "a40", "a41", "a42"]:
    read(f"reviews/{agent}.md")
for path in ["verification/a12/receipt.json", "verification/a33/receipt.json",
             "development/matched-published-current-bank.json",
             "verification/a41/receipt.json", "verification/a42/catalogue-binding.json",
             "verification/a42/source-summary.json"]:
    js(path)

# Each membership set is extracted from retained measurements, not preset names
# guessed from the category or diagnostic substitutions. Reviews explain the
# heterogeneous scene scopes; a listed scope does not imply every scene was
# applied to every program in that review.
reports: list[dict] = []


def report(agent: str, names, expected: int, refs: list[str], captures: str,
           gestures: str, register_rate: str, bounds: str):
    names = sorted(set(names), key=lambda n: by_name[n]["host_index"])
    assert len(names) == expected, (agent, names)
    for path in refs:
        read(path)
    reports.append({
        "review": agent, "review_ref": f"reviews/{agent}.md",
        "current_actual_patch_programs": expected,
        "host_indices": [by_name[n]["host_index"] for n in names],
        "names": names, "evidence_refs": refs,
        "documented_capture_scope": captures,
        "performance_scope": gestures,
        "register_and_rate_scope": register_rate,
        "limits": bounds,
    })


a13 = rows("verification/a13/capture-metrics.csv")
report("a13", (r["name"] for r in a13 if r["side"] == "after"), 14,
       ["verification/a13/capture-metrics.csv", "verification/a13/pedal-metrics.csv",
        "verification/a13/capture-manifest.json", "verification/a13/summary.json"],
       "72 complete-patch captures / 36 .6-to-.7 pairs; separate 24 pedal captures, same named subset.",
       "Ten piano programs held; Stage Keys/Soft Keys soft and early-release controls; four EP velocity pairs; pedal versus direct hold/early key-off on Stage Keys, Soft Keys, Electric Piano I, Reed Piano.",
       "48 kHz Standard 2x; ten piano programs at MIDI 36/60, holds12/6s; four EPs at MIDI 60; velocity .25/.85 controls where specified.",
       "14/45 selected identities; no new full-family bend or non-reference-rate matrix.")
a14 = js("verification/a14/pcm-analysis.json")
report("a14", (r["name"] for r in a14["results"]), 4,
       ["verification/a14/pcm-analysis.json", "verification/a14/inventory-review.json"],
       "32 captures / 16 .6-to-.7 pairs.",
       "Steady, upward octave ribbon/return, downward octave ribbon/return, pressure; release in every scene.",
       "48 kHz; MIDI 36, velocity .85; 2.3 s hold/.7 s release; 16-foot foundation.",
       "Four selected Bass identities; no exhaustive register or chord guarantee.")
a15 = js("verification/a15/pcm-manifest.json")
report("a15", (r["name"] for r in a15 if r["revision"] == "current"), 5,
       ["verification/a15/pcm-manifest.json", "verification/a15/gesture-metrics.csv",
        "verification/a15/structural-coverage.json"],
       "120 captures: 60 current / 60 .6.",
       "Plain release, wheel +2/-2 semitones/return, pressure .8, sustain-delayed damping on all five.",
       "48 kHz; MIDI 48/60/72; velocity .8; 2.2 s per scene.",
       "Five named Plucked programs; specialized material equality at this rate is not acoustic equivalence.")
a16 = js("verification/a16/measurements.json")
report("a16", (r["name"] for r in a16["rows"]), 6,
       ["verification/a16/measurements.json", "verification/a16/pcm-hashes.json",
        "verification/a16/brass-inventory.csv"],
       "78 runs/257.4s; includes labelled no-air, dry-effects and frozen-formant diagnostics.",
       "Pressure on six complete programs; +2-semitone formant-control bends on French Horn, Muted Trumpet, Trombone Glide.",
       "48 kHz host; Standard 2x/HQ 8x/offline; MIDI 60, velocity .8, 3 s hold/.3 s release.",
       "Six complete recipes; diagnostic core/effects copies do not count as additional factory identities. Delay Brass retains quality-dependent effects.")
a17 = js("verification/a17/results.json")
report("a17", (r["name"] for r in a17["default_pcm_controls"]), 5,
       ["verification/a17/results.json", "verification/a17/matrix.csv", "verification/a17/binding.json"],
       "Five full 8 s hold/2 s release .6-to-.7 pairs;15 current material/register rows; separate HQ and two-chord observations; isolated 128-seed Agogo diagnostic excluded from identity count.",
       "Onset, held decay and release; six-note velocity 1 chords on Agogo and Gong.",
       "48 kHz Standard 2x; MIDI 48/60/72, velocity .85; Agogo HQ 8x comparison.",
       "Five of 23 Bells have this targeted material matrix; modeled mode tracking is not perceived pitch/acoustic parity.")
a18 = rows("verification/a18/current-probe.csv")
report("a18", (r["name"] for r in a18), 4,
       ["verification/a18/current-probe.csv", "verification/a18/pcm-manifest.json",
        "verification/a18/official-native-metrics.csv", "verification/a18/structural-coverage.json"],
       "104 Engine-probe captures plus 8 capture-CLI records; four actual programs.",
       "Held notes, pressure, +2-semitone bend/return and early release; four-note chords on Cellos/PWM Strings.",
       "48 kHz host, Standard 2x/HQ 8x; MIDI 48/60/72, velocity .85; ordinary 2 s hold/1 s tail.",
       "Four of 59 Strings; native in these files denotes saved Standard core quality, not native plugin hosting.")
a19 = rows("verification/a19/captures.csv")
report("a19", (r["name"] for r in a19 if r["revision"] == "after"), 5,
       ["verification/a19/captures.csv", "verification/a19/measurements.json",
        "verification/a19/recipes.csv", "verification/a19/pcm-hashes.json"],
       "65 captures: 55 current/10 .6;260s.",
       "Three registers; middle-note soft velocity, channel-pressure step and release; offline/HQ comparisons.",
       "48 kHz host 2x/8x; Flute/Oboe MIDI 60/72/84; Clarinet/Bassoon/Shakuhachi48/60/72; 3 s hold/1 s tail.",
       "Five of 14 Winds; no comprehensive legato, voicing or host-rate matrix.")
a20 = rows("verification/a20/current-renders.csv")
report("a20", (r["name"] for r in a20), 5,
       ["verification/a20/current-renders.csv", "verification/a20/pcm-manifest.json",
        "verification/a20/summary.json", "verification/a20/inventory-audit.json"],
       "28 captures: 18 current/10 .6.",
       "Single/chord, soft/firm on five; pressure controls on Flute Organ 8+4/Tremolo LFO Organ; repeated keying on Percussive Organ.",
       "48 kHz Standard; single MIDI 60 and eight-note48/55/60/64/67/72/76/79; ordinary 2 s hold/.4 s release.",
       "Five of 15 Organ; registrations intentionally retain velocity-neutral keying. Relative loudness and physical-organ realism not approved.")
a21 = js("verification/a21/rendered-measurements.json")
report("a21", (r["preset"] for r in a21["renders"]), 3,
       ["verification/a21/rendered-measurements.json", "verification/a21/structural-coverage.json",
        "verification/a21/binding.json"],
       "28 captures/14 .6-to-.7 pairs/224s.",
       "Choir Pad/Dark Choir register, velocity, pressure and eight-note stack; Sweep Pad long single note and stack.",
       "48 kHz Standard 2x; choir MIDI 36/48/72 and low stack,3 s singles/4 s stacks; Sweep MIDI 60/stack 6 s; tails 3/8 s.",
       "Three of63 Pads; two of five vocal pad identities. Does not imply every pad or every live bend was captured.")
a22 = rows("verification/a22/current-renders.csv")
report("a22", (r["name"] for r in a22), 7,
       ["verification/a22/current-renders.csv", "verification/a22/pcm-manifest.json",
        "verification/a22/summary.json", "verification/a22/inventory-audit.json"],
       "18 captures: 11 current/seven .6;180s.",
       "Seven full-velocity/full-pressure eight-note chords; pressure-zero controls on four; six-second hold and four-second release.",
       "48 kHz Standard; chord MIDI 48/55/60/64/67/72/76/79.",
       "One voicing/quality/rate; not every controller, changing voicing or legato portamento.")
a23 = js("verification/a23/summary.json")
pressure23 = js("verification/a23/published-pressure-comparison.json")
report("a23", [r["preset"] for r in a23["measurements"]] +
       [r["preset"] for r in pressure23["comparisons"]], 11,
       ["verification/a23/summary.json", "verification/a23/published-pressure-comparison.json",
        "verification/a23/pcm-manifest.json", "verification/a23/structural-audit.json"],
       "20 captures/57.8s:12 current gestures plus eight .4/current pressure captures;16 current/four .4 overall.",
       "Specific overlap, ribbon return, glide, vibrato removal, pressure-required onset/fade, phrase-ending and body-color scenes; pressure .30/1 on both default-silent exceptions.",
       "Most gestures48 kHz; Reed Lead/Neo-Noir Brass Lead each44.1/96 kHz; pressure comparison MIDI 67,v.55,1.8s hold/.6s tail.",
       "11/56 identities. Eight-voice polyphonic ownership checks are not a nonexistent mono-priority mode or all-lead performance qualification.")
a24 = js("verification/a24/review.json")
report("a24", (r["name"] for r in a24["pcm_manifest"] if r["build"] == "current"), 8,
       ["verification/a24/review.json", "verification/a24/current.jsonl",
        "verification/a24/lifecycle-tests.json"],
       "20 renders:18 current/two .6;200s; includes causal tempo/LFO/ring controls.",
       "Three source keys55/48/52 at different velocities; all five arp modes; modern four tempo60-to180 controls; selected LFO/ring controls and release.",
       "48 kHz eight normal phrases; Arp Bells/Slow Sequences 166 additionally44.1 kHz; six-second phrase/four-second tail.",
       "Eight of 27 current phrases; prior .6 complete27 evidence remains historical. Free-running Hz is not BPM synchronization; no all-pressure matrix.")
a25 = js("verification/a25/review.json")
report("a25", (r["name"] for r in a25["captures"] if r["build"] == "current"), 11,
       ["verification/a25/review.json", "verification/a25/long-metrics.csv"],
       "26 captures: 18 current including three repetitions/eight .6;316s.",
       "Seven long held/released FX; Ring Bell/Amber FX 151 velocity pairs; Robot Voice/Hollow FX 141 pressure pairs; selected deterministic repetitions.",
       "Seven long cases48 kHz,MIDI 60,10 s hold/16 s tail; four body cases44.1 kHz; low robotic vowels MIDI 36.",
       "11/46 identities; quiet ring-carrier residue is not silence/delay/stuck-voice proof. Four other slow FX still lack individual extended current captures.")
a26 = rows("verification/a26/soft-metrics.csv")
report("a26", (r["name"] for r in a26), 22,
       ["verification/a26/soft-metrics.csv", "verification/a26/firm-metrics.csv",
        "verification/a26/representative-stacks.csv", "verification/a26/program-audit.json"],
       "44 soft/firm complete-program captures/83.6s; six additional representative phrase/stack measurements; separate routing fixtures are not more factory identities.",
       "All 22 soft/firm; six named representatives pressure-ramped phrase/full-effort stack; original-source/PANEL routing separately bounded.",
       "All 22 at 48 kHz,MIDI 60,v.25/.85,1.4 s hold/.5 s tail; six representatives44.1 kHz phrase/stack.",
       "All 22 have targeted velocity pairs, not all registers, long blooms or live source transitions. Browser studies and original electrical tabs are distinct routes.")
a27 = js("verification/a27/audio.json")
report("a27", (r["name"] for r in a27["cases"]), 2,
       ["verification/a27/audio.json", "verification/a27/controls.json",
        "verification/a27/receipt.json"],
       "12 current cases, each compared with .6 and .4:36 renders across three versions.",
       "Two note/velocity/pressure settings with release; exact deterministic PCM equality across both comparators.",
       "Both Init at 44.1/48/96 kHz; MIDI 48,v.25,p0 and MIDI 72,v.85,p.75; .5s hold/.5 s release.",
       "Core Init identity; not deserialization/native-plugin qualification or universal saved-body PCM compatibility.")
a33 = rows("verification/a33/causal-velocity-metrics.csv")
report("a33", (r["name"] for r in a33 if r["mode"] == "current_7"), 2,
       ["verification/a33/causal-velocity-metrics.csv", "verification/a33/causal-velocity-summary.json",
        "verification/a33/causal-velocity-receipt.json"],
       "Eight causal captures/12s:four unchanged current and four diagnostic .4-baseline-value restorations within .7.",
       "Analog Bass and Funky Pluck I soft/firm velocity; restores only VCA baselines in labelled diagnostic arm.",
       "48 kHz; category-default MIDI 48/60, velocity .25/.85; one-second hold/.5 s release.",
       "Two representatives of 28 baseline-only programs; diagnostic arm is not a complete published .4 reconstruction.")
a34 = rows("verification/a34/metrics.csv")
report("a34", (r["name"] for r in a34), 5,
       ["verification/a34/metrics.csv", "verification/a34/renders.csv",
        "verification/a34/pcm-manifest.json", "verification/a34/timing-census.json"],
       "Five current complete-program scenes/82s; no new comparator captures.",
       "Slow Brass 10/Evolving Pad/Golden FX 156 long single notes; Cathedral Pad/Golden Pads 66 twelve-second low eight-note stacks.",
       "48 kHz Standard 2x; singles MIDI 60,v.8,p0 with 6/6/10 s holds; stacks36/43/48/55/60/64/67/72,v1,p1,12 s hold; 6/8 s tails.",
       "Only seven of 14 timing-crossing programs have individual extended current evidence when A21/A25 are joined. Late peaks can occur without a >3s attack crossing.")
a35 = js("verification/a35/review.json")
report("a35", [a35["gesture"]["preset"]], 1,
       ["verification/a35/review.json", "verification/a35/current-capture.json",
        "verification/a35/baseline-capture.json"],
       "One current/.6 complete-preset gesture pair/26s; seven-vocal structural accounting reuses other evidence.",
       "Golden Pads 52 pressure ramp, CC1 ramp, +2/-2-semitone wheel/return and release.",
       "48 kHz Standard 2x; MIDI 48,v.85; seven-second hold/six-second tail.",
       "Only one added live vocal gesture. Blade Choir/Midnight Pads 55 retain default-note extension evidence but no additional individual low-note gesture. Captured bank and 48-mode ceiling remain.")
a36 = rows("verification/a36/capture-metrics.csv")
report("a36", (r["name"] for r in a36 if r["condition"] == "current"), 2,
       ["verification/a36/capture-metrics.csv", "verification/a36/summary.json",
        "verification/a36/recipes.json"],
       "12 unchanged actual-patch captures; separate 12 Steel-to-Piano diagnostic captures; two official-capture equivalence controls.",
       "Dulcimer/Distant Keys 113 velocity/onset/settling/release; diagnostic substitution excluded from unchanged factory membership.",
       "48 kHz Standard 2x; MIDI 48/60/72,v.25/.85;2.5 s hold/1 s release.",
       "Shared authored Steel excitation has no separately calibrated hammer/contact model; neither incorrect pluck proof nor acoustic dulcimer validation.")

report_by_host = {p["host_index"]: [] for p in programs}
for r in reports:
    for host in r["host_indices"]:
        report_by_host[host].append(r["review"])
primary = {h for r in reports if 13 <= int(r["review"][1:]) <= 27 for h in r["host_indices"]}
supplemental = {h for r in reports for h in r["host_indices"]}
assert len(primary) == 112 and len(supplemental) == 121

slow_extended = {
    53: ["a34", 6], 182: ["a25", 10], 199: ["a34", 10],
    286: ["a21", 6], 293: ["a34", 6], 489: ["a25", 10], 500: ["a25", 10],
}
assert set(slow_extended) <= set(slow_by_host)
slow_gaps = sorted(set(slow_by_host) - set(slow_extended))
assert slow_gaps == [92, 99, 111, 188, 189, 486, 497]
new_mechanisms = {"piano_partial_decay", "noise_rate_normalization",
                  "generic_string_formants", "low_choir_harmonics"}

records = []
for p in programs:
    h = p["host_index"]
    d = delta_by_host[h]
    assert d["name"] == p["name"] and d["category"] == p["category"]
    is_original = h < 222
    bank_index = h if is_original else h - 222
    eligible = mechanism_by_host[h]["eligible_mechanisms"]
    slow = slow_by_host.get(h)
    active_times = ([max(r["amp_card0_v08"], r["filter_card0"] if r["filter_span"] else 0)
                     for r in slow["ranks"] if r["capture_active"] and r["sounding_path"]]
                    if slow else [])
    records.append({
        "host_index_zero_based": h, "inventory_index": p["index"],
        "bank": "original" if is_original else "curated", "bank_local_index": bank_index,
        "midi_bank_msb": 1 if is_original else 0, "midi_bank_lsb": bank_index // 128,
        "midi_program_zero_based": bank_index % 128,
        "name": p["name"], "category": p["category"], "musical": p["category"] != "Init",
        "complete_identity_and_204_values_preserved_from_6": True,
        "category_structural_review": FAMILIES[p["category"]][0],
        "published4_parameter_class": d["parameter_class"],
        "matched_default_record_in_each_4_6_7_csv": True,
        "default_note": int(d["default_note"]), "default_rate_hz": 48000,
        "default_velocity": 0.8, "default_pressure": 0,
        "default_hold_seconds": 3, "default_tail_seconds": 1,
        "default_signal_metrics_changed_4_to_7": d["signal_metrics_changed_4_to_7"] == "True",
        "a23_pressure_supplement": d["pressure_supplement"] == "True",
        "cumulative_signal_change_evidence_4_to_7": d["signal_metrics_changed_4_to_7"] == "True" or d["pressure_supplement"] == "True",
        "default_signal_metrics_changed_6_to_7": d["signal_metrics_changed_6_to_7"] == "True",
        "conditional_new7_mechanisms": sorted(set(eligible) & new_mechanisms),
        "in_new7_conditional_union": bool(set(eligible) & new_mechanisms),
        "active_body": "body" in eligible,
        "reused_short_phrase_and_1s_stack_gate": True,
        "targeted_current_actual_patch_evidence_in_a13_a27": h in primary,
        "targeted_current_actual_patch_evidence_in_selected_review_union": h in supplemental,
        "targeted_review_ids": report_by_host[h],
        "mapped_attack_beyond_3s_at_default_velocity": slow is not None,
        "latest_cold_card_attack_seconds_if_beyond_3s": max(active_times) if active_times else "",
        "individual_extended_current_capture_for_slow_entrance": h in slow_extended if slow else "not_applicable_to_14_crossing_set",
        "slow_entrance_extended_review": slow_extended[h][0] if h in slow_extended else "",
        "slow_entrance_extended_hold_seconds": slow_extended[h][1] if h in slow_extended else "",
        "slow_entrance_individual_long_capture_gap": h in slow_gaps,
        "human_listening_approval_claimed_by_this_matrix": False,
        "all_gestures_or_rates_qualified_by_this_matrix": False,
        "evidence_refs": ["verification/a12/inventory.json", "verification/a33/program-deltas.csv",
                          "verification/a12/mechanism-coverage.json", f"reviews/{FAMILIES[p['category']][0]}.md",
                          "development/focused-ctest-receipt.json"] +
                         [f"reviews/{agent}.md" for agent in report_by_host[h]] +
                         (["verification/a34/timing-census.json"] if slow else []),
    })

category_records = []
for category, (agent, n) in FAMILIES.items():
    rr = [r for r in records if r["category"] == category]
    old = category33[category]
    category_records.append({
        "category": category, "programs": n,
        "musical_programs": sum(r["musical"] for r in rr),
        "original_addresses": sum(r["bank"] == "original" for r in rr),
        "curated_addresses": sum(r["bank"] == "curated" for r in rr),
        "structurally_reviewed": n, "matched_default_records_per_bank": n,
        "default_note": int(old["default_note"]),
        "parameter_changes_beyond_level_pan": old["beyond_conservative_level_pan"],
        "additive_vca_baseline_only": old["active_additive_vca_baseline_only"],
        "default_signal_changes_4_to_7": sum(r["default_signal_metrics_changed_4_to_7"] for r in rr),
        "pressure_supplement_programs": sum(r["a23_pressure_supplement"] for r in rr),
        "union_signal_changes_4_to_7": sum(r["cumulative_signal_change_evidence_4_to_7"] for r in rr),
        "conditional_new7_union_programs": sum(r["in_new7_conditional_union"] for r in rr),
        "default_signal_changes_6_to_7": sum(r["default_signal_metrics_changed_6_to_7"] for r in rr),
        "reused_short_phrase_and_1s_stack_gate_programs": n,
        "targeted_a13_a27_unique_programs": sum(r["targeted_current_actual_patch_evidence_in_a13_a27"] for r in rr),
        "targeted_review_union_unique_programs": sum(r["targeted_current_actual_patch_evidence_in_selected_review_union"] for r in rr),
        "without_targeted_capture_in_selected_review_union": sum(not r["targeted_current_actual_patch_evidence_in_selected_review_union"] for r in rr),
        "slow_entrances_beyond_default_hold": sum(r["mapped_attack_beyond_3s_at_default_velocity"] for r in rr),
        "slow_entrances_with_individual_extended_current_capture": sum(r["host_index_zero_based"] in slow_extended for r in rr),
        "slow_entrances_without_individual_extended_current_capture": sum(r["slow_entrance_individual_long_capture_gap"] for r in rr),
        "family_review_ref": f"reviews/{agent}.md",
        "targeted_review_ids": sorted({a for r in rr for a in r["targeted_review_ids"]}),
    })

counts = {
    "addresses": len(records), "musical_programs": sum(r["musical"] for r in records),
    "init_programs": sum(not r["musical"] for r in records),
    "musical_categories": 14, "category_rows_including_init": len(category_records),
    "original_addresses": 222, "curated_addresses": 301,
    "preserved_values": 523 * 204, "new_programs": 0, "new_7_recipe_value_edits": 0,
    "matched_default_rows_per_bank": 523,
    "aligned_retained_bank_metric_rows_across_4_6_7": 1569,
    "default_bank_nonfinite_samples_each_bank": 0,
    "default_bank_samples_over_full_scale_each_bank": 0,
    "default_note_counts": dict(sorted(Counter(r["default_note"] for r in records).items())),
    "parameter_changes_beyond_level_pan": 493, "active_additive_vca_baseline_only": 28,
    "default_signal_changes_4_to_7": sum(r["default_signal_metrics_changed_4_to_7"] for r in records),
    "additional_pressure_programs": sum(r["a23_pressure_supplement"] for r in records),
    "union_signal_changes_4_to_7": sum(r["cumulative_signal_change_evidence_4_to_7"] for r in records),
    "conditional_new7_union": sum(r["in_new7_conditional_union"] for r in records),
    "musical_outside_conditional_new7_union": sum(r["musical"] and not r["in_new7_conditional_union"] for r in records),
    "observed_default_signal_changes_6_to_7": sum(r["default_signal_metrics_changed_6_to_7"] for r in records),
    "reused_short_phrase_and_1s_stack_gate_programs": 523,
    "a13_a27_targeted_unique_addresses": len(primary),
    "a13_a27_targeted_unique_musical": sum(h not in (43, 522) for h in primary),
    "selected_targeted_review_union_addresses": len(supplemental),
    "selected_targeted_review_union_musical": sum(h not in (43, 522) for h in supplemental),
    "musical_without_targeted_capture_in_selected_review_union": sum(r["musical"] and not r["targeted_current_actual_patch_evidence_in_selected_review_union"] for r in records),
    "mapped_slow_entrances_beyond3s": len(slow_by_host),
    "slow_entrances_with_individual_extended_current_capture": len(slow_extended),
    "slow_entrances_without_individual_extended_current_capture": len(slow_gaps),
    "default_cli_longer_than3s": timing["default_cli_longer_than_3s_count"],
    "new_A55_renders_tests_or_auditions": 0,
}
assert counts["default_signal_changes_4_to_7"] == 519
assert counts["additional_pressure_programs"] == 2
assert counts["union_signal_changes_4_to_7"] == 521
assert counts["conditional_new7_union"] == 256
assert counts["observed_default_signal_changes_6_to_7"] == 67
assert counts["musical_without_targeted_capture_in_selected_review_union"] == 402
assert counts["default_note_counts"] == {48: 50, 60: 380, 67: 70, 72: 23}

cleanup = []
for path in ["development/temporary-pcm-cleanup.json",
             "development/temporary-pcm-cleanup-second-pass.json"]:
    d = js(path)
    cleanup.append({"ref": path, "listed_files": len(d["files"]),
                    "listed_bytes": d["total_bytes"], "sha256": INPUTS[path]["sha256"]})

coverage = {
    "schema": 1, "scope": "Retained source-core, inventory, metrics and review coverage only. No new testing, rendering, raw-PCM audit, human listening, acoustic validation or native acceptance.",
    "sound_source": summary33["sources"]["current_7"],
    "final_binary_source": continuity["source"], "final_source_tree": continuity["tree"],
    "sound_source_continuity_ref": "verification/a40/source-ee0dabca/source-continuity.json",
    "counts": counts,
    "definitions": {
        "structural": "All complete program records and all 204 values checked; family reviews examine authored roles/routes. This is not a listening or acoustic-realism score.",
        "matched_default": "Three aligned .4/.6/.7 metric CSVs,523 rows each. Complete saved patch,48kHz,v.8,p0,hold3/tail1,category-specific note. Equality concerns recorded metric vectors; whole-bank PCM is not retained.",
        "conditional_mechanism_union": "Four .7 mechanisms with nonzero eligible body/rank/VCA route;256 programs. A changed rate/pitch/partial region may be needed; eligibility is not an unconditional output change.",
        "targeted_actual_patch": "Union of named current full-preset observations in A13-A27 and A33-A36. Repeated scenes, diagnostic copies and comparator versions do not add identities. This index is not a census of every test render elsewhere.",
        "all_bank_gesture_gate": "Retained local focused CTest executes LibraryAudit on523 programs:44.1kHz,category-dependent source-note group,v.9,pressure 0 to 1,release near 1.2 s/1.9 s total; separately about 1 s eight-note v1/p1 stress. Bounded scripted source-core behavior, not all musical gestures or native hosting.",
        "slow_entrance": "A34 source timing, cold voice card 0,v.8:14 programs have an active mapped VCA/VCF attack beyond 3 s. Seven have named extended current full-preset captures; seven remain individually unmeasured for that longer hold within this review set.",
        "human_listening": "No human-listening approval is established by this matrix; generated WAVs, numerical metrics and audition packaging do not prove listening occurred.",
        "register_rate": "See each targeted review scope; pitch-wheel travel differs from fresh-note register tests, host sample rate differs from internal oversampling, and a quality comparison does not qualify every rate.",
    },
    "category_coverage": category_records,
    "targeted_review_scopes": reports,
    "program_coverage": records,
    "pressure_only_exceptions": pressure23["comparisons"],
    "slow_entrance_rows": [r for r in records if r["mapped_attack_beyond_3s_at_default_velocity"]],
    "slow_entrance_gap_host_indices": slow_gaps,
    "vocal_limits": {
        "programs": 7, "active_vocal_ranks": 12, "selected_low_note_or_expression_programs": 5,
        "without_additional_individual_low_note_gesture": [32, 98],
        "refs": ["reviews/a35.md", "reviews/a41.md"],
        "qualification": "Blade Choir/Midnight Pads 55 still have actual default MIDI 60 metric changes in their 16-foot rank. Captured onset banks and 48-mode ceiling do not guarantee every formant in the deepest register.",
    },
    "materials_and_hammer_limits": {
        "specialized_programs": 18, "specialized_active_ranks": 34,
        "material_choices": 9, "nonstandard_variant_choices": 4,
        "hammered_keys_hosts": [156, 354], "refs": ["reviews/a36.md", "reviews/a41.md"],
        "qualification": "Authored material/variant spectra, losses and onsets; no complete acoustic replica or separately calibrated hammered-string contact model is established.",
    },
    "native_and_audition_status": {
        "context": "Prepared contract only; actual CI artifact acceptance is external to A55.",
        "run": continuity["run_id"], "attempt": continuity["attempt"],
        "expected_wavs": audition["expected"]["wav_count"],
        "expected_selected_programs": audition["expected"]["selected_program_count"],
        "actual_archive_accepted_by_A55": False, "native_or_release_qualified_by_A55": False,
        "refs": ["verification/a40/source-ee0dabca/prepared-contract.json", "reviews/a42.md"],
        "host_limit": "A42 source/metadata and gate registration are not fresh native execution; processing-queue-only VST3 Program limitation remains disclosed.",
    },
    "temporary_pcm_cleanup_records": cleanup,
    "raw_pcm_retention": "Some temporary PCM was removed. Retained metrics, probes, source bindings, per-capture hashes and cleanup manifests are the evidence; A55 does not claim to re-open or recompare removed audio.",
}
write_csv("program-coverage.csv", records)
write_csv("category-coverage.csv", category_records)
write_json("coverage.json", coverage)
write_json("summary.json", counts)
write_json("source-binding.json", {
    "schema": 1,
    "operation": "Read and join retained evidence only; no product executable, compiler, test runner or audio capture invoked.",
    "inventory_sha256": INPUTS["verification/a12/inventory.json"]["sha256"],
    "sound_source": summary33["sources"]["current_7"],
    "final_binary_source": continuity["source"], "final_source_tree": continuity["tree"],
    "script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    "inputs": dict(sorted(INPUTS.items())),
    "outputs": {name: {"sha256": hashlib.sha256((OUT / name).read_bytes()).hexdigest(),
                       "bytes": (OUT / name).stat().st_size}
                for name in ["program-coverage.csv", "category-coverage.csv", "coverage.json", "summary.json",
                             "README.md", "../../reviews/a55.md"]},
})
print(json.dumps(counts, indent=2))
