#!/usr/bin/env python3
"""Read completed cs80_engine_audit CSVs; never render or change source/limits.

The tool's 'all' suite omits CPU, renders the frozen 222-program bank rather
than curatedLibrary(), and subsamples program indices at secondary rates.
Run only after the integration owner creates audit.complete. This report
preserves every CSV field verbatim and evaluates only already recorded targets.
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import io
import json
import math
from pathlib import Path
import subprocess
import sys

SOURCE_SHA = "1e5893854c3d0d9b0aa1ab1c725fd2e8084717f6"
PRODUCTION_SHA = "23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162"
RATES = (44100, 48000, 96000)
POLICIES = ("legacy", "source")
PHRASE_FIELDS = "name,group,policy,rate,nonfinite,peak,peak_db,samples_over_1,softclip_frac_chord,rms_single_db,rms_chord_db,weighted_chord_db,loud_short_db,dc_single_rel,dc_chord_rel,dc_idle_abs,idle_rms,subsonic_peak,subsonic_rel,click_off,click_chord_off,click_retrigger,click_fresh_chord,click_steal,voices_at_end".split(",")
HEADERS = {
    "phrase.csv": PHRASE_FIELDS,
    "fx.csv": PHRASE_FIELDS + "fx_slots,fx_mix,tail_ratio_db,gain_vs_dry_db".split(","),
    "alias.csv": "rate,policy,wave,ring,note,feet_ratio,f0_hz,nonharmonic_db_vs_partials,worst_spur_dbc".split(","),
    "latency.csv": "rate,policy,oversampling_factor,freq_hz,delay_host_samples,reported_latency,gain_db".split(","),
    "click-causality.csv": "name,rate,event,baseline_score,omitted_event_score,prefix_max_abs,first_strike_score,retrigger_score,coordinate,event_sample,baseline_peak_sample,omitted_peak_sample,baseline_peak_jump,omitted_peak_jump,baseline_peak_channel,omitted_peak_channel,added_peak_sample,added_peak_jump".split(","),
}
TEXT_FIELDS = {"name", "group", "policy", "wave", "fx_slots", "event", "coordinate", "baseline_peak_channel", "omitted_peak_channel"}
LABEL_FIELDS = ("name", "group", "policy", "rate", "wave", "ring", "note", "feet_ratio", "f0_hz", "fx_slots", "fx_mix", "event", "coordinate", "freq_hz", "oversampling_factor")
EXPECTED_ROWS = {"phrase.csv": 566, "fx.csv": 102, "alias.csv": 168, "latency.csv": 40, "click-causality.csv": 9}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def git_blob(repo: Path, name: str) -> bytes:
    return subprocess.check_output(["git", "-C", str(repo), "show", f"{SOURCE_SHA}:{name}"])


def num(row, field):
    raw = row["raw"][field]
    return None if raw == "" else float(raw)


def ref(row):
    return {"file": row["file"], "csv_row": row["csv_row"], **{k: row["raw"][k] for k in LABEL_FIELDS if k in row["raw"]}}


def extrema(rows, field):
    values = [(num(row, field), row) for row in rows]
    finite = [(v, row) for v, row in values if v is not None and math.isfinite(v)]
    result = {
        "numeric_rows": len(finite),
        "blank_rows": sum(v is None for v, _ in values),
        "nonfinite_numeric_rows": [ref(row) for v, row in values if v is not None and not math.isfinite(v)],
    }
    if finite:
        low, high = min(v for v, _ in finite), max(v for v, _ in finite)
        result.update(min=low, max=high,
                      minimum_rows=[ref(row) for v, row in finite if v == low],
                      maximum_rows=[ref(row) for v, row in finite if v == high])
    return result


def maximum_target(rows, field, limit, scope, contract):
    stats = extrema(rows, field)
    exceeding = [ref(row) for row in rows if num(row, field) is not None and math.isfinite(num(row, field)) and num(row, field) > limit]
    return {
        "metric": field, "maximum": limit, "scope": scope, "contract": contract,
        "row_count": len(rows), "extrema": stats,
        "exceeding_row_count": len(exceeding), "exceeding_rows": exceeding,
        "all_exported_values_within_target": bool(rows) and not exceeding and not stats["nonfinite_numeric_rows"] and not stats["blank_rows"],
    }


def selected(rows, **filters):
    return [row for row in rows if all(row["raw"].get(k) == str(v) for k, v in filters.items())]


def grouped(rows, fields, metrics):
    buckets = {}
    for row in rows:
        key = tuple(row["derived"].get(k, row["raw"].get(k)) for k in fields)
        buckets.setdefault(key, []).append(row)
    results = []
    for key, members in sorted(buckets.items()):
        item = {**dict(zip(fields, key)), "row_count": len(members),
                "metrics": {metric: extrema(members, metric) for metric in metrics}}
        if "nonfinite" in members[0]["raw"]:
            item["reported_nonfinite_stereo_frames"] = sum(int(row["raw"]["nonfinite"]) for row in members)
            item["reported_frames_over_full_scale"] = sum(int(float(row["raw"]["samples_over_1"])) for row in members)
        results.append(item)
    return results


def load_csvs(audit: Path):
    raw_files, all_rows, numeric_nonfinite = {}, {}, []
    for name, header in HEADERS.items():
        path = audit / name
        data = path.read_bytes()
        reader = csv.DictReader(io.StringIO(data.decode("utf-8")))
        if reader.fieldnames != header:
            raise ValueError(f"Unexpected schema in {name}: {reader.fieldnames}")
        rows = []
        for line, raw in enumerate(reader, 2):
            if None in raw or any(value is None for value in raw.values()):
                raise ValueError(f"Incomplete/extra CSV field in {name}:{line}")
            row = {"file": name, "csv_row": line, "raw": raw, "derived": {}}
            for key, value in raw.items():
                if key not in TEXT_FIELDS and value:
                    value_number = float(value)
                    if not math.isfinite(value_number):
                        numeric_nonfinite.append({**ref(row), "column": key, "value": value})
            if "policy" in raw and raw["policy"] not in POLICIES:
                raise ValueError(f"Unexpected policy in {name}:{line}")
            if int(raw["rate"]) not in RATES:
                raise ValueError(f"Unexpected rate in {name}:{line}")
            if name == "phrase.csv":
                family = "tone_tabs" if raw["group"] == "tab" else "frozen_legacy_library"
                if family == "frozen_legacy_library" and not raw["group"].startswith("program:"):
                    raise ValueError(f"Unexpected phrase group: {raw['group']}")
                row["derived"] = {
                    "family": family,
                    "control_coordinate": "tone_tabs" if family == "tone_tabs" else ("electrical_programmer" if raw["policy"] == "source" else "legacy_physical_values"),
                    "ring_state": "not exported; preserve program settings",
                }
            elif name == "click-causality.csv":
                row["derived"]["policy"] = "source"
                row["derived"]["baseline_peak_offset_samples"] = int(raw["baseline_peak_sample"]) - int(raw["event_sample"])
                row["derived"]["baseline_peak_offset_ms"] = 1000.0 * row["derived"]["baseline_peak_offset_samples"] / int(raw["rate"])
            rows.append(row)
        if len(rows) != EXPECTED_ROWS[name]:
            raise ValueError(f"{name}: expected {EXPECTED_ROWS[name]} complete rows, got {len(rows)}")
        all_rows[name] = rows
        raw_files[name] = {"sha256": sha(data), "bytes": len(data), "row_count": len(rows), "columns": header,
                           "raw_rows": [{"csv_row": row["csv_row"], **row["raw"]} for row in rows]}
    extras = sorted(path.name for path in audit.glob("*.csv") if path.name not in HEADERS)
    return raw_files, all_rows, numeric_nonfinite, extras


def validate_coverage(rows):
    coverage = []
    for policy in POLICIES:
        bank = [row for row in selected(rows, policy=policy, rate=RATES[0]) if row["derived"]["family"] == "frozen_legacy_library"]
        if len(bank) != 222 or len({row["raw"]["name"] for row in bank}) != 222:
            raise ValueError(f"{policy}: first-rate bank is not 222 uniquely named programs")
        full_names = [row["raw"]["name"] for row in bank]
        tab_names = None
        for rate in RATES:
            members = selected(rows, policy=policy, rate=rate)
            programs = [row for row in members if row["derived"]["family"] == "frozen_legacy_library"]
            tabs = [row for row in members if row["derived"]["family"] == "tone_tabs"]
            expected_names = full_names if rate == RATES[0] else full_names[::16]
            if [row["raw"]["name"] for row in programs] != expected_names:
                raise ValueError(f"{policy}/{rate}: unexpected program sampling")
            actual_tabs = [row["raw"]["name"] for row in tabs]
            if len(actual_tabs) != 11 or len(set(actual_tabs)) != 11:
                raise ValueError(f"{policy}/{rate}: expected 11 tab pairs")
            if tab_names is not None and actual_tabs != tab_names:
                raise ValueError("Tone-pair order changed across rates")
            tab_names = actual_tabs
            coverage.append({"policy": policy, "rate": rate, "program_count": len(programs), "tab_pair_count": len(tabs),
                             "program_indices": list(range(222)) if rate == RATES[0] else list(range(0, 222, 16)),
                             "program_names": expected_names, "tab_pair_names": actual_tabs})
    return coverage


def label(row):
    parts = [row.get("name", row.get("wave", "tone"))]
    for key in ("group", "rate", "note", "feet_ratio", "ring", "fx_slots", "fx_mix", "coordinate"):
        if key in row:
            parts.append(f"{key}={row[key]}")
    parts.append(f"{row['file']}:{row['csv_row']}")
    return "; ".join(parts).replace("|", "\\|")


def fmt(number):
    return "unavailable" if number is None else f"{number:.8g}"


def table(lines, header, rows):
    lines.extend(["", "| " + " | ".join(header) + " |", "| " + " | ".join("---" for _ in header) + " |"])
    lines.extend("| " + " | ".join(str(value) for value in row) + " |" for row in rows)
    lines.append("")


def markdown(out):
    lines = ["# Completed engine audit summary", "", f"Source snapshot: `{SOURCE_SHA}`. Production change: `{PRODUCTION_SHA}`.",
             f"Generated {out['generated_utc']} from the completed CSVs. The integration owner's completion marker is recorded in JSON.", "",
             "This is an analysis of software renders, not a hardware measurement, host validation, or release/CI claim. Raw rows are preserved without filtering in the CSVs and verbatim in the JSON; extrema below use their emitted numeric precision.",
             "", "## Coverage and unchanged targets", "",
             "The command is `cs80_engine_audit --suite all --rates 44100,48000,96000 --out audit` at the default oversampling setting. Its first rate is 44.1 kHz. The program factory is `factoryPresets()`: the frozen **222-program Legacy Library under both engine policies**. The separate 301-program curated library is not rendered by this tool. All eleven paired tone selectors run at every rate. Secondary program rates retain indices 0,16,…,208 only. `all` excludes the CPU suite; these CSVs do not establish the CPU budgets."]
    table(lines, ["Policy", "Rate", "Programs", "Tab pairs", "Program coverage"],
          [[r["policy"], r["rate"], r["program_count"], r["tab_pair_count"], "222-program bank" if r["rate"] == RATES[0] else "Every 16th: 14 programs"] for r in out["coverage"]])
    lines += ["Source phrase program rows use the electrical programmer; Legacy ignores that new coordinate. Tone-tab rows use their selected factory networks. Original physical-PANEL Bass is reported separately by the causal suite. Phrase CSVs do not export ring state, so ring-off/on separation belongs to the explicit alias rows only.", "",
              "Targets retain their original scope. The registered oscillator limit is **−70 dB for Source saw/pulse, ring off, at 48 kHz**; other alias rates and all ring-on rows remain reported diagnostics. Source peak ≤0 dBFS and the existing click/retrigger/steal limits of 1 are reported without changing a metric or excluding failing rows. Legacy is a compatibility comparison, not a retuning target."]
    table(lines, ["Existing criterion / scope", "Limit", "Worst exported value", "Exceeding rows / evaluated", "Within target"],
          [[r["scope"], r["maximum"], fmt(r["extrema"].get("max")), f"{r['exceeding_row_count']}/{r['row_count']}", r["all_exported_values_within_target"]] for r in out["existing_target_comparisons"]])
    lines += ["## Phrase results by policy, rate and source family"]
    table(lines, ["Policy", "Rate", "Family", "Rows", "Peak dBFS", "Off", "Chord off", "Retrigger", "Steal", "Nonfinite frames"],
          [[g["policy"], g["rate"], g["family"], g["row_count"], *[fmt(g["metrics"][m].get("max")) for m in ("peak_db", "click_off", "click_chord_off", "click_retrigger", "click_steal")], g["reported_nonfinite_stereo_frames"]] for g in out["phrase_groups"]])
    lines += ["### Named phrase extrema", "", "Each maximum is over that policy's rendered coverage, including the subsampling above. JSON retains all tied rows and all other metric ranges."]
    named = []
    for g in out["phrase_policy_totals"]:
        for metric in ("peak_db", "click_off", "click_chord_off", "click_retrigger", "click_steal", "subsonic_rel", "dc_idle_abs"):
            m = g["metrics"][metric]
            named.append([g["policy"], metric, fmt(m.get("max")), "<br>".join(label(r) for r in m.get("maximum_rows", []))])
    table(lines, ["Policy", "Metric", "Maximum", "Named raw row(s)"], named)
    lines += ["## Ring-off and ring-on spectra", "", "Nonharmonic power is relative to legitimate partial power; spur is the strongest individual off-partial bin relative to the strongest legitimate bin. Ring-on is the fixed 150 Hz carrier test, not a high-carrier qualification.", "", "The tool's ring-on mask admits k*f0 +/-150 Hz (including the carrier at k=0), but does not separately admit the unshifted k*f0 signal. The declared Source multiplier contains a 0.005*x signal-feedthrough term. Consequently, its ring-on nonharmonic metric also counts intended unshifted signal energy. An ideal single-line calculation with equal downstream gain gives 10*log10(2*0.005^2) = -43.0103 dB relative to the two sidebands. This is a code-derived diagnostic interpretation, not a new gate or proof that every residual is alias-free; filtering, other components and finite windows remain in the actual exported measurements."]
    table(lines, ["Policy", "Rate", "Ring", "Wave", "Rows", "Worst nonharmonic dB", "Worst spur dBc", "Nonharmonic worst row"],
          [[g["policy"], g["rate"], g["ring"], g["wave"], g["row_count"], fmt(g["metrics"]["nonharmonic_db_vs_partials"].get("max")), fmt(g["metrics"]["worst_spur_dbc"].get("max")), label(g["metrics"]["nonharmonic_db_vs_partials"]["maximum_rows"][0])] for g in out["alias_groups"]])
    lines += ["## Event controls: physical PANEL, electrical programmer and tone tabs", "", "These rows all use Source Performance. Baseline and omitted-event renders share their complete initial state and preceding events. A score exceeding one remains an unmet criterion even if the omitted-event control also exceeds one; causal evidence does not waive the score. Peak offset is relative to the actual 64-frame-quantized event. First-strike and retrigger columns are intentionally blank for note-off."]
    table(lines, ["Name", "Rate", "Coordinate", "Event", "Baseline score", "Omitted score", "First strike", "Retrigger", "Prefix max abs", "Peak offset ms"],
          [[r["raw"]["name"], r["raw"]["rate"], r["raw"]["coordinate"], r["raw"]["event"], r["raw"]["baseline_score"], r["raw"]["omitted_event_score"], r["raw"]["first_strike_score"], r["raw"]["retrigger_score"], r["raw"]["prefix_max_abs"], fmt(r["derived"]["baseline_peak_offset_ms"])] for r in out["causal_rows"]])
    lines += ["## Optional FX stress and isolated decimator", "", "FX rows are a separate bright-saw stress fixture, not additional library programs. Slot strings identify all three slots; `000` is dry. Tail ratio is RMS over [11,12] s relative to [4.9,5.4] s, as implemented. A value near zero can be a noise floor, and this report invents no tail threshold."]
    table(lines, ["Policy", "Rate", "Rows", "Peak dBFS", "Gain versus dry min/max dB", "Tail ratio min/max dB", "Peak row", "Nonfinite frames"],
          [[g["policy"], g["rate"], g["row_count"], fmt(g["metrics"]["peak_db"].get("max")), f"{fmt(g['metrics']['gain_vs_dry_db'].get('min'))} / {fmt(g['metrics']['gain_vs_dry_db'].get('max'))}", f"{fmt(g['metrics']['tail_ratio_db'].get('min'))} / {fmt(g['metrics']['tail_ratio_db'].get('max'))}", label(g["metrics"]["peak_db"]["maximum_rows"][0]), g["reported_nonfinite_stereo_frames"]] for g in out["fx_groups"]])
    lines += ["Latency measures the decimator in isolation. The Source engine's reported latency also includes the oscillator's 8/internal-factor host-sample term; subtracting the full reported latency from this isolated measurement would falsely report that extra delay as an error. Phase unwrapping uses the existing 63-sample decimator reference, so this is not an independent whole-plugin latency test."]
    table(lines, ["Policy", "Rate", "Factor", "Rows", "Measured decimator delay min/max", "Engine reported min/max", "Gain min/max dB"],
          [[g["policy"], g["rate"], g["oversampling_factor"], g["row_count"], *[f"{fmt(g['metrics'][m].get('min'))} / {fmt(g['metrics'][m].get('max'))}" for m in ("delay_host_samples", "reported_latency", "gain_db")]] for g in out["latency_groups"]])
    lines += ["## Nonfinite reporting and raw provenance", "", f"Phrase exported nonfinite stereo-frame count: **{out['nonfinite']['phrase_stereo_frames']}**. FX: **{out['nonfinite']['fx_stereo_frames']}**. Alias rows reporting the explicit `999` nonfinite-render sentinel: **{len(out['nonfinite']['alias_nonfinite_render_rows'])}**. Nonfinite numeric CSV cells across all five suites: **{len(out['nonfinite']['numeric_csv_cells'])}**.", "",
              "A phrase/FX counter increments once per stereo frame when either channel is nonfinite, before the renderer substitutes zero. Alias reports only a sentinel, not its frame count. The causal and latency CSV schemas do not export audio nonfinite-frame counts; their finite numeric cells do not prove an unavailable count is zero."]
    table(lines, ["Raw CSV", "Rows", "Bytes", "SHA-256"], [[name, f["row_count"], f["bytes"], f"`{f['sha256']}`"] for name, f in out["raw_files"].items()])
    lines += [f"Summarizer SHA-256: `{out['summarizer_sha256']}`.", "", "The JSON retains every raw row, source/contract identities, all extrema ties, target exceedance references and coordinate annotations. Source policy is not PURE; this audit does not render PURE, the curated library, high-carrier ring cases, every control combination, or a DAW session. A successful audit process means rendering completed; it does not imply all targets passed.", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("/tmp/cs80-final-integration-1e589385"))
    parser.add_argument("--repo", type=Path, default=Path("/workspace/scratch/a0e502bd4cd5/cs80-ui-tone"))
    args = parser.parse_args()
    marker = args.root / "audit.complete"
    if not marker.is_file():
        print(f"Awaiting completion marker: {marker}. No summaries written.", file=sys.stderr)
        return 3
    source_paths = ["src/tools/engine_audit.cpp", "src/core/RingModulatorCircuit.h", "src/core/FactoryPresets.h", "src/core/FactoryPresets.cpp", "docs/ENGINE_AUDIT.md", "docs/STATUS.md", "tests/parity/tolerances-v2/ws1-voice.json", "tests/parity/tolerances-v2/completion-dsp.json"]
    blobs = {name: git_blob(args.repo, name) for name in source_paths}
    ws1 = json.loads(blobs[source_paths[-2]])["stages"]
    completion = json.loads(blobs[source_paths[-1]])["click_continuity"]
    raw_files, suites, nonfinite_cells, extras = load_csvs(args.root / "audit")
    phrase = suites["phrase.csv"]
    phrase_metrics = [field for field in PHRASE_FIELDS if field not in TEXT_FIELDS | {"rate", "nonfinite"}]
    source_rows = selected(phrase, policy="source")
    alias_rows = selected(suites["alias.csv"], policy="source", ring="0", rate="48000")
    if len(alias_rows) != 14:
        raise ValueError("Expected all 14 Source ring-off 48 kHz oscillator rows")
    causal = suites["click-causality.csv"]
    coordinates = ("original_physical_panel", "electrical_programmer", "tone_tabs")
    for coordinate in coordinates:
        if [int(row["raw"]["rate"]) for row in causal if row["raw"]["coordinate"] == coordinate] != list(RATES):
            raise ValueError(f"Unexpected causal coverage for {coordinate}")
    targets = [maximum_target(source_rows, "peak_db", 0.0, "Source phrase peak, rendered coverage", "docs/STATUS.md: original peak <=0 dBFS"),
               maximum_target(source_rows, "click_retrigger", ws1["ig00151_vca"]["click_retrigger_max"], "Source phrase retrigger, rendered coverage", "ws1-voice.json: ig00151_vca"),
               maximum_target(source_rows, "click_steal", ws1["ig00151_vca"]["click_steal_max"], "Source phrase steal, rendered coverage", "ws1-voice.json: ig00151_vca"),
               maximum_target(source_rows, "click_off", completion["phrase_note_off_max"], "Source phrase single note-off, rendered coverage", "completion-dsp.json: click_continuity"),
               maximum_target(source_rows, "click_chord_off", completion["phrase_note_off_max"], "Source phrase chord note-off, rendered coverage", "completion-dsp.json: click_continuity"),
               maximum_target(alias_rows, "nonharmonic_db_vs_partials", ws1["oscillator_promotion"]["alias_worst_nonharmonic_db_48k"], "Source saw/pulse, ring off, 48 kHz", "ws1-voice.json: oscillator_promotion")]
    for coordinate in coordinates:
        members = selected(causal, coordinate=coordinate)
        targets.append(maximum_target(members, "baseline_score", completion["phrase_retrigger_max"] if coordinate != "tone_tabs" else completion["phrase_note_off_max"], "Source causal baseline: " + coordinate, "completion-dsp.json: click_continuity; existing coordinate-separated fixture"))
    out = {
        "schema_version": 1, "source_snapshot_sha": SOURCE_SHA, "production_change_sha": PRODUCTION_SHA,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "command": "cs80_engine_audit --suite all --rates 44100,48000,96000 --out audit",
        "default_oversampling": True, "completion_marker": {"path": marker.name, "sha256": sha(marker.read_bytes()), "text": marker.read_text(errors="replace")},
        "summarizer_sha256": sha(Path(__file__).read_bytes()),
        "source_identities": {name: {"sha256": sha(data), "bytes": len(data)} for name, data in blobs.items()},
        "scope": {"policy_names": list(POLICIES), "pure_rendered": False, "program_collection": "factoryPresets(): frozen 222-program Legacy Library under both policies", "curated_library_rendered": False, "rates_hz": list(RATES), "phrase_block_frames": 64, "phrase_duration_seconds": 12, "full_bank_rate_hz": RATES[0], "secondary_program_stride": 16, "cpu_suite_included": False, "numeric_precision": "original CSV text preserved; no extra precision inferred", "coordinate_annotations": "derived from exact tool source; causal coordinate is directly exported", "unexpected_extra_csvs": extras},
        "coverage": validate_coverage(phrase),
        "existing_target_comparisons": targets,
        "phrase_groups": grouped(phrase, ("policy", "rate", "family"), phrase_metrics),
        "phrase_policy_totals": grouped(phrase, ("policy",), phrase_metrics),
        "ring_on_diagnostic_interpretation": {"evidence_class": "code-derived analytical inference, not a measured decomposition", "mask": "k*f0 +/-150 Hz including k=0; unshifted k*f0 is not separately admitted", "source_multiplier_signal_feedthrough_coefficient": 0.005, "ideal_equal_gain_single_line_power_ratio_to_both_sidebands": 2 * 0.005 ** 2, "ideal_equal_gain_single_line_db": 10 * math.log10(2 * 0.005 ** 2), "limitations": "Actual residual includes filtering, other components and finite-window effects; no alias-free claim or changed criterion"},
        "alias_groups": grouped(suites["alias.csv"], ("policy", "rate", "ring", "wave"), ("nonharmonic_db_vs_partials", "worst_spur_dbc")),
        "fx_groups": grouped(suites["fx.csv"], ("policy", "rate"), phrase_metrics + ["tail_ratio_db", "gain_vs_dry_db"]),
        "latency_groups": grouped(suites["latency.csv"], ("policy", "rate", "oversampling_factor"), ("delay_host_samples", "reported_latency", "gain_db")),
        "causal_rows": causal,
        "nonfinite": {"phrase_stereo_frames": sum(int(row["raw"]["nonfinite"]) for row in phrase), "fx_stereo_frames": sum(int(row["raw"]["nonfinite"]) for row in suites["fx.csv"]), "alias_nonfinite_render_rows": [ref(row) for row in suites["alias.csv"] if num(row, "nonharmonic_db_vs_partials") == 999], "numeric_csv_cells": nonfinite_cells, "causal_audio_frame_count_exported": False, "latency_audio_frame_count_exported": False},
        "raw_files": raw_files,
    }
    report = markdown(out)
    encoded = json.dumps(out, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
    for name, contents in (("audit-summary.json", encoded), ("audit-summary.md", report)):
        target = args.root / name
        temporary = target.with_suffix(target.suffix + ".tmp")
        temporary.write_text(contents)
        temporary.replace(target)
    print(json.dumps({"source": SOURCE_SHA, "rows": {name: value["row_count"] for name, value in raw_files.items()}, "targets_exceeding": [{"scope": r["scope"], "count": r["exceeding_row_count"], "worst": r["extrema"].get("max")} for r in targets if not r["all_exported_values_within_target"]], "nonfinite_stereo_frames": out["nonfinite"]["phrase_stereo_frames"] + out["nonfinite"]["fx_stereo_frames"], "files": [str(args.root / "audit-summary.json"), str(args.root / "audit-summary.md")]}, indent=2))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"Audit summary refused: {error}", file=sys.stderr)
        sys.exit(2)
