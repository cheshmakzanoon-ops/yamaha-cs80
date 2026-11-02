#!/usr/bin/env python3
"""Strict, test-only Yamaha CS-80 ADJ technician metadata import/report.

Valid input is *unverified technician-supplied evidence*, not a factory gate,
a digital model measurement, a safety procedure, or proof of a physical event.
No dependency beyond Python's standard library; no hardware access.
"""

from __future__ import annotations

import argparse
from datetime import datetime
import html
import json
import math
from pathlib import Path
import re
import sys


class IntakeError(ValueError):
    """The supplied evidence metadata violates schema v1."""


CHECKPOINT_SETUP = {
    "hp_as_found": "as_found",
    "hp_after_vr7_cp3_max": "yamaha_hp_p11",
    "hp_after_vr8_cp3_setting": "yamaha_hp_p11",
    "lp_after_vr9_cp3_max": "yamaha_lp_p11",
    "lp_after_vr10_cp3_setting": "yamaha_lp_p11",
    "noise_reference": "noise_reference",
}
CHECKPOINT_ORDER = {name: i for i, name in enumerate(CHECKPOINT_SETUP)
                    if name != "noise_reference"}
QUANTITIES = {
    "CP2": {"vpp", "vrms"},
    "CP3": {"vpp", "vrms"},
    "NO1": {"vpp", "vrms"},
    "NI": {"vpp", "vrms"},
    "IC10_VFC4": {"vdc", "vrms"},
    "IC10_QADJ6": {"vdc", "vrms"},
    "FH": {"vdc"},
    "FL": {"vdc"},
}
UTC_RE = re.compile(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z\Z")
SHA256_RE = re.compile(r"[0-9a-f]{64}\Z")
MAX_INPUT_BYTES = 4 * 1024 * 1024


def _unique_pairs(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise IntakeError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _reject_constant(value: str) -> None:
    raise IntakeError(f"non-finite JSON constant: {value}")


def decode_json(raw: bytes) -> object:
    if len(raw) > MAX_INPUT_BYTES:
        raise IntakeError("input exceeds 4 MiB metadata limit")
    try:
        return json.loads(
            raw.decode("utf-8"),
            object_pairs_hook=_unique_pairs,
            parse_constant=_reject_constant,
        )
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise IntakeError(f"invalid JSON/UTF-8: {exc}") from exc


def _fields(value: object, required: set[str], optional: set[str], path: str) -> dict:
    if type(value) is not dict:
        raise IntakeError(f"{path} must be an object")
    missing = required - value.keys()
    extra = value.keys() - required - optional
    if missing:
        raise IntakeError(f"{path} missing: {', '.join(sorted(missing))}")
    if extra:
        raise IntakeError(f"{path} unknown: {', '.join(sorted(extra))}")
    return value


def _text(value: object, path: str) -> str:
    if type(value) is not str or not value.strip() or len(value) > 512:
        raise IntakeError(f"{path} must be nonempty text <=512 characters")
    if any(ord(char) < 32 or ord(char) == 127 for char in value):
        raise IntakeError(f"{path} contains control characters")
    return value


def _enum(value: object, allowed: set[str], path: str) -> str:
    if type(value) is not str or value not in allowed:
        raise IntakeError(f"{path} must be one of {', '.join(sorted(allowed))}")
    return value


def _number(value: object, path: str, *, minimum: float | None = None,
            exclusive_minimum: float | None = None) -> float:
    try:
        finite = type(value) in (int, float) and math.isfinite(value)
    except OverflowError:
        finite = False
    if not finite:
        raise IntakeError(f"{path} must be a finite JSON number (not Boolean)")
    if minimum is not None and value < minimum:
        raise IntakeError(f"{path} must be >= {minimum}")
    if exclusive_minimum is not None and value <= exclusive_minimum:
        raise IntakeError(f"{path} must be > {exclusive_minimum}")
    return float(value)


def _utc(value: object, path: str) -> str:
    if type(value) is not str or not UTC_RE.fullmatch(value):
        raise IntakeError(f"{path} must be YYYY-MM-DDTHH:MM:SSZ")
    try:
        datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise IntakeError(f"{path} contains invalid calendar/time values") from exc
    return value


def validate_document(value: object) -> dict:
    """Validate all schema v1 fields; never infer unspecified readings."""
    root = _fields(value, {"schema_version", "record_kind", "technician",
                           "instrument", "snapshots"}, set(), "root")
    if type(root["schema_version"]) is not int or root["schema_version"] != 1:
        raise IntakeError("schema_version must equal integer 1")
    _enum(root["record_kind"], {"cs80_adj_technician_observation"}, "record_kind")

    technician = _fields(root["technician"],
                         {"identifier", "report_reference", "authorization_reference"},
                         set(), "technician")
    for key in technician:
        _text(technician[key], f"technician.{key}")

    instrument = _fields(root["instrument"],
                         {"serial", "m_board", "board_revision", "channel"},
                         set(), "instrument")
    for key in ("serial", "m_board", "board_revision"):
        _text(instrument[key], f"instrument.{key}")
    _enum(instrument["channel"], {"I", "II"}, "instrument.channel")

    snapshots = root["snapshots"]
    if type(snapshots) is not list or not snapshots:
        raise IntakeError("snapshots must be a nonempty array")
    seen_checkpoints = set()
    last_alignment_order = -1
    for i, item in enumerate(snapshots):
        path = f"snapshots[{i}]"
        snapshot = _fields(
            item, {"checkpoint", "panel_setup", "captured_at_utc",
                   "held_note", "observations"},
            {"scope", "meter", "connected_m_board_count", "notes", "control_reference",
             "ambient_temperature_c", "warmup_minutes"}, path,
        )
        checkpoint = _enum(snapshot["checkpoint"], set(CHECKPOINT_SETUP),
                           f"{path}.checkpoint")
        if checkpoint in seen_checkpoints:
            raise IntakeError(f"{path} repeated checkpoint {checkpoint}")
        seen_checkpoints.add(checkpoint)
        if checkpoint != "noise_reference":
            order = CHECKPOINT_ORDER[checkpoint]
            if order <= last_alignment_order:
                raise IntakeError(f"{path} HP/LP checkpoints out of printed ADJ order")
            last_alignment_order = order
        if snapshot["panel_setup"] != CHECKPOINT_SETUP[checkpoint]:
            raise IntakeError(f"{path}.panel_setup conflicts with checkpoint")
        _utc(snapshot["captured_at_utc"], f"{path}.captured_at_utc")
        allowed_note = {"C1", "not_applicable"} if checkpoint == "noise_reference" else {"C1"}
        _enum(snapshot["held_note"], allowed_note, f"{path}.held_note")
        if "notes" in snapshot:
            _text(snapshot["notes"], f"{path}.notes")
        if "control_reference" in snapshot:
            _text(snapshot["control_reference"], f"{path}.control_reference")
        if "ambient_temperature_c" in snapshot:
            _number(snapshot["ambient_temperature_c"], f"{path}.ambient_temperature_c")
        if "warmup_minutes" in snapshot:
            _number(snapshot["warmup_minutes"], f"{path}.warmup_minutes", minimum=0)
        if "connected_m_board_count" in snapshot:
            count = snapshot["connected_m_board_count"]
            if type(count) is not int or count < 1:
                raise IntakeError(f"{path}.connected_m_board_count must be positive integer")

        observations = snapshot["observations"]
        if type(observations) is not list or not observations:
            raise IntakeError(f"{path}.observations must be a nonempty array")
        seen_nodes = set()
        observed = False
        waveform_observed = False
        for j, entry in enumerate(observations):
            opath = f"{path}.observations[{j}]"
            reading = _fields(entry, {"node", "status"},
                              {"reason", "quantity", "value_v", "probe", "evidence"},
                              opath)
            node = _enum(reading["node"], set(QUANTITIES), f"{opath}.node")
            if node in seen_nodes:
                raise IntakeError(f"{opath} duplicated node {node}")
            seen_nodes.add(node)
            status = _enum(reading["status"], {"observed", "not_acquired"},
                           f"{opath}.status")
            if status == "not_acquired":
                _fields(reading, {"node", "status", "reason"}, set(), opath)
                _text(reading["reason"], f"{opath}.reason")
                continue
            observed = True
            _fields(reading, {"node", "status", "quantity", "value_v",
                              "probe", "evidence"}, set(), opath)
            quantity = _enum(reading["quantity"], QUANTITIES[node],
                             f"{opath}.quantity")
            if quantity != "vdc":
                waveform_observed = True
            _number(reading["value_v"], f"{opath}.value_v",
                    minimum=0 if quantity in {"vpp", "vrms"} else None)
            probe = _fields(reading["probe"],
                            {"input_resistance_ohm", "input_capacitance_pf",
                             "attenuation"}, set(), f"{opath}.probe")
            _number(probe["input_resistance_ohm"],
                    f"{opath}.probe.input_resistance_ohm", exclusive_minimum=0)
            _number(probe["input_capacitance_pf"],
                    f"{opath}.probe.input_capacitance_pf", minimum=0)
            _number(probe["attenuation"],
                    f"{opath}.probe.attenuation", exclusive_minimum=0)
            evidence = _fields(reading["evidence"],
                               {"artifact_reference", "sha256"}, set(),
                               f"{opath}.evidence")
            _text(evidence["artifact_reference"], f"{opath}.evidence.artifact_reference")
            if type(evidence["sha256"]) is not str or not SHA256_RE.fullmatch(evidence["sha256"]):
                raise IntakeError(f"{opath}.evidence.sha256 must be lowercase 64-hex digest")

        if waveform_observed and "scope" not in snapshot:
            raise IntakeError(f"{path}.scope required for waveform/RMS readings")
        if observed and not ({"scope", "meter"} & snapshot.keys()):
            raise IntakeError(f"{path} needs scope or meter for observed readings")
        if not observed and ({"scope", "meter"} & snapshot.keys()):
            raise IntakeError(f"{path} instrument metadata cannot claim an absent acquisition")
        if "scope" in snapshot:
            scope = _fields(snapshot["scope"],
                            {"instrument_name", "bandwidth_hz", "sample_rate_sps",
                             "acquisition_window_s", "coupling", "ground_reference"},
                            set(), f"{path}.scope")
            _text(scope["instrument_name"], f"{path}.scope.instrument_name")
            _text(scope["ground_reference"], f"{path}.scope.ground_reference")
            _enum(scope["coupling"], {"AC", "DC"}, f"{path}.scope.coupling")
            for field in ("bandwidth_hz", "sample_rate_sps", "acquisition_window_s"):
                _number(scope[field], f"{path}.scope.{field}", exclusive_minimum=0)
        if "meter" in snapshot:
            meter = _fields(snapshot["meter"],
                            {"instrument_name", "input_resistance_ohm", "mode"},
                            set(), f"{path}.meter")
            _text(meter["instrument_name"], f"{path}.meter.instrument_name")
            _enum(meter["mode"], {"DC_V"}, f"{path}.meter.mode")
            _number(meter["input_resistance_ohm"],
                    f"{path}.meter.input_resistance_ohm", exclusive_minimum=0)
    return root


def _md(value: object) -> str:
    """Render untrusted metadata as an inert Markdown table cell."""
    escaped = html.escape(str(value), quote=True)
    for character, entity in (("|", "&#124;"), ("\\", "&#92;"),
                              ("`", "&#96;"), ("[", "&#91;"),
                              ("]", "&#93;"), ("*", "&#42;"),
                              ("_", "&#95;")):
        escaped = escaped.replace(character, entity)
    return escaped


def render_report(root: dict) -> str:
    """Report only submitted statuses/values; never synthesize missing data."""
    technician, instrument = root["technician"], root["instrument"]
    rows = [
        "# Yamaha CS-80 ADJ: imported bench-observation metadata",
        "",
        "**Status: SCHEMA-VALID METADATA ONLY.** Any numeric observations were",
        "**reported by an external submitter and remain unverified**. This tool",
        "does not collect hardware data, verify the attached artifact digest,",
        "authenticate a technician or approve physical work. **No factory",
        "pass/fail assessment is made.**",
        "",
        "Yamaha ADJ printed p11 names **CP3** as the HP and LP scope point;",
        "the HP step specifies VR7 maximum followed by VR8 4.3 ± 1 Vpp,",
        "and the LP step VR9 maximum followed by VR10 2.2 ± 1 Vpp.",
        "These source instructions are **not** evaluated by the importer.",
        "",
        "## Submitted provenance (not independently authenticated)",
        "",
        f"- Technician identifier: {_md(technician['identifier'])}",
        f"- Technician report: {_md(technician['report_reference'])}",
        f"- Authorization reference: {_md(technician['authorization_reference'])}",
        f"- Instrument serial: {_md(instrument['serial'])}",
        f"- Active M board / channel: {_md(instrument['m_board'])} / {_md(instrument['channel'])}",
        f"- Board revision (as stated): {_md(instrument['board_revision'])}",
        "",
        "## Submitted checkpoint and instrument context",
        "",
        "| Checkpoint | Stated setup | Timestamp UTC | Scope/meter acquisition context | Additional reported context |",
        "| --- | --- | --- | --- | --- |",
    ]
    for snapshot in root["snapshots"]:
        scope = snapshot.get("scope")
        context_parts = []
        if scope is not None:
            context_parts.append(
                f"scope {scope['bandwidth_hz']:.9g} Hz / {scope['sample_rate_sps']:.9g} sps / "
                f"{scope['acquisition_window_s']:.9g} s; "
                f"{_md(scope['instrument_name'])}, "
                f"{_md(scope['coupling'])}, reference {_md(scope['ground_reference'])}")
        if "meter" in snapshot:
            meter = snapshot["meter"]
            context_parts.append(
                f"meter {_md(meter['instrument_name'])} "
                f"{_md(meter['mode'])}; input {meter['input_resistance_ohm']:.9g} Ω")
        context = "; ".join(context_parts) or "not acquired"
        optional_context = []
        if "connected_m_board_count" in snapshot:
            optional_context.append(f"NI-connected M boards: {snapshot['connected_m_board_count']}")
        if "warmup_minutes" in snapshot:
            optional_context.append(f"warmup: {snapshot['warmup_minutes']:.9g} min")
        if "ambient_temperature_c" in snapshot:
            optional_context.append(f"ambient: {snapshot['ambient_temperature_c']:.9g} °C")
        if "control_reference" in snapshot:
            optional_context.append(f"control reference: {_md(snapshot['control_reference'])}")
        if "notes" in snapshot:
            optional_context.append(f"notes: {_md(snapshot['notes'])}")
        rows.append(
            f"| {_md(snapshot['checkpoint'])} | {_md(snapshot['panel_setup'])}; "
            f"{_md(snapshot['held_note'])} | {_md(snapshot['captured_at_utc'])} | "
            f"{context} | {'; '.join(optional_context) or 'not supplied'} |"
        )
    rows += [
        "",
        "## Explicit node statuses and technician-supplied values",
        "",
        "| Checkpoint | Node | Status / reported quantity (V) | Probe R / C / attenuation | Evidence artifact / SHA-256 (unverified) |",
        "| --- | --- | --- | --- | --- |",
    ]
    acquired, missing = 0, 0
    for snapshot in root["snapshots"]:
        for observation in snapshot["observations"]:
            if observation["status"] == "not_acquired":
                missing += 1
                meaning = f"NOT ACQUIRED — {_md(observation['reason'])}"
                probe_context = "—"
                evidence_context = "—"
            else:
                acquired += 1
                meaning = (f"SUBMITTED {_md(observation['quantity'])}: "
                           f"{observation['value_v']:.9g} V")
                probe = observation["probe"]
                probe_context = (f"{probe['input_resistance_ohm']:.9g} Ω / "
                                 f"{probe['input_capacitance_pf']:.9g} pF / "
                                 f"{probe['attenuation']:.9g}×")
                evidence = observation["evidence"]
                evidence_context = (f"{_md(evidence['artifact_reference'])} / "
                                    f"{_md(evidence['sha256'])}")
            rows.append(
                f"| {_md(snapshot['checkpoint'])} | {_md(observation['node'])} | "
                f"{meaning} | {probe_context} | {evidence_context} |"
            )
    rows += [
        "",
        f"**Submitted numeric node observations:** {acquired}. "
        f"**Explicit not-acquired nodes:** {missing}. "
        "Nodes absent from the payload are **not supplied**, never silently",
        "treated as 0 V or as measured.",
        "",
        "NO1/NI loaded-source voltage/impedance, IG00156 Vfc/Qadj transfer,",
        "the 25 T51–T54 tone-board revisions and manufacturer parity remain",
        "**unknown unless independently evidenced**. An imported value is",
        "not authority to change any factory acceptance interval or frozen",
        "V1 model parameter.",
        "",
    ]
    return "\n".join(rows)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path,
                        help="UTF-8 schema-v1 technician observation JSON")
    parser.add_argument("--output", type=Path,
                        help="New Markdown report only; never overwrite existing file")
    args = parser.parse_args(argv)
    try:
        if args.input.stat().st_size > MAX_INPUT_BYTES:
            raise IntakeError("input exceeds 4 MiB metadata limit")
        data = args.input.read_bytes()
        report = render_report(validate_document(decode_json(data)))
        if args.output is None:
            sys.stdout.write(report)
        else:
            with args.output.open("x", encoding="utf-8") as output:
                output.write(report)
    except (OSError, IntakeError) as exc:
        parser.exit(2, f"ADJ intake rejected: {exc}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
