#!/usr/bin/env python3
"""Verify narrowly scoped real-host evidence, never infer complete DAW acceptance."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_wave(path):
    raw = Path(path).read_bytes()
    require(len(raw) >= 12 and raw[:4] == b"RIFF" and raw[8:12] == b"WAVE", "not a RIFF WAVE")
    require(struct.unpack_from("<I", raw, 4)[0] + 8 == len(raw), "truncated or trailing RIFF bytes")
    chunks, pos = {}, 12
    while pos < len(raw):
        require(pos + 8 <= len(raw), "truncated chunk header")
        name, size = raw[pos:pos + 4], struct.unpack_from("<I", raw, pos + 4)[0]
        pos += 8
        require(pos + size <= len(raw), "truncated chunk payload")
        if name in (b"fmt ", b"data"):
            require(name not in chunks, "duplicate audio/format chunk")
            chunks[name] = raw[pos:pos + size]
        pos += size + (size & 1)
    require(pos == len(raw) and b"fmt " in chunks and b"data" in chunks, "missing format/audio chunk")
    fmt, data = chunks[b"fmt "], chunks[b"data"]
    require(len(fmt) >= 16, "short format chunk")
    tag, channels, rate, byte_rate, align, bits = struct.unpack_from("<HHIIHH", fmt)
    if tag == 0xFFFE:
        require(len(fmt) >= 40 and fmt[26:40] == b"\0\0\0\0\x10\0\x80\0\0\xaa\0\x38\x9b\x71", "unsupported extensible WAV subtype")
        tag = struct.unpack_from("<H", fmt, 24)[0]
    require(channels > 0 and rate > 0 and bits in (16, 24, 32, 64), "invalid WAV layout")
    require(align == channels * (bits // 8) and byte_rate == rate * align, "inconsistent WAV layout")
    require(len(data) % align == 0, "partial audio frame")
    if tag == 3 and bits in (32, 64):
        samples = [v[0] for v in struct.iter_unpack("<f" if bits == 32 else "<d", data)]
    elif tag == 1 and bits in (16, 24, 32):
        width, scale = bits // 8, float(1 << (bits - 1))
        samples = [int.from_bytes(data[i:i + width], "little", signed=True) / scale
                   for i in range(0, len(data), width)]
    else:
        raise ValueError("unsupported/nonlinear WAV encoding")
    return rate, channels, samples


def verify_audio(contract, waves):
    render, limits = contract["render"], contract["limits"]
    require(set(waves) == set(render["files"]), "render inventory mismatch")
    audio = {}
    for name, (rate, channels, samples) in waves.items():
        require(rate == render["sample_rate"] and channels == render["channels"], name + ": rate/channel mismatch")
        require(len(samples) == render["frames"] * channels, name + ": frame count mismatch")
        require(all(math.isfinite(x) for x in samples), name + ": nonfinite samples")
        require(max(map(abs, samples)) <= limits["maximum_peak"], name + ": excessive peak")
        audio[name] = samples

    def window(samples, bounds):
        a, b = [round(v * render["sample_rate"]) * render["channels"] for v in bounds]
        return samples[a:b]

    def rms(samples):
        return math.sqrt(math.fsum(x * x for x in samples) / len(samples))

    active = rms(window(audio["baseline"], render["active_window_seconds"]))
    held = rms(window(audio["baseline"], render["muted_window_seconds"]))
    quiet = rms(window(audio["automated"], render["muted_window_seconds"]))
    restored = max(abs(a - b) for a, b in zip(audio["baseline"], audio["restored"]))
    early = max(abs(a - b) for a, b in zip(window(audio["baseline"], render["active_window_seconds"]),
                                          window(audio["automated"], render["active_window_seconds"])))
    no_midi = max(map(abs, audio["no-midi"]))
    before_note = max(map(abs, window(audio["baseline"], [0, render["note_start_seconds"] - 0.01])))
    require(active >= limits["minimum_active_rms"] and held >= limits["minimum_active_rms"], "baseline does not prove held MIDI audio")
    require(quiet <= limits["maximum_automated_muted_rms"], "host volume automation did not mute audio")
    require(restored <= limits["maximum_restored_sample_error"], "saved/restored render mismatch")
    require(early <= limits["maximum_early_automation_sample_error"], "automation changed the earlier constant segment")
    require(no_midi <= limits["maximum_no_midi_peak"], "no-MIDI counterfactual is not silent")
    require(before_note <= limits["maximum_pre_note_peak"], "audio appears before MIDI onset")
    return {"baseline_active_rms": active, "baseline_held_rms": held, "automated_muted_rms": quiet,
            "maximum_restored_sample_error": restored, "maximum_early_automation_sample_error": early,
            "no_midi_peak": no_midi, "pre_note_peak": before_note}


def evidence_contract_filename(contract):
    filename = {"cs80-reaper-smoke-v1": "contract-v1.json",
                "cs80-reaper-smoke-v2-dry": "contract-v2-dry.json"}.get(contract["schema"])
    require(filename is not None, "unsupported host contract schema")
    return filename


def verify_metadata(manifest, observed, contract, source, src_tree, copied_contract_sha256):
    """Pure identity/state checks, independently testable without any fake host artifact."""
    require("error" not in manifest, "host runner reported failure")
    require(manifest["source_commit"] == source and re.fullmatch("[a-f0-9]{40}", source), "checkout source mismatch")
    require(manifest["src_tree"] == src_tree and re.fullmatch("[a-f0-9]{40}", src_tree), "source tree mismatch")
    require(manifest["host_archive_sha256"] == contract["host"]["sha256"], "host archive identity mismatch")
    require(manifest["plugin_build_sha256"] == manifest["plugin_installed_sha256"]
            and re.fullmatch("[a-f0-9]{64}", manifest["plugin_build_sha256"]), "built/installed plugin mismatch")
    require(manifest["contract_sha256"] == copied_contract_sha256, "contract hash mismatch")
    require(observed["schema"] == contract["schema"] and "error" not in observed, "host fixture failed")
    require(observed["host_version"] == contract["host"]["version"] + "/" + contract["host"]["platform"], "running host version mismatch")
    require(observed["checks"] and all(item["ok"] is True for item in observed["checks"]), "host assertions failed/missing")
    require(len(observed["scan_matches"]) == 1 and "CS-80 Reference" in observed["fx_name"]
            and observed["fx_ident"], "host plugin identity missing")
    require(observed["editor_open"] is True, "host editor did not open")
    if contract["schema"] == "cs80-reaper-smoke-v2-dry":
        require(observed.get("editor_open_after_escape") is True, "host editor closed during tour dismissal")
    for field in ("pdc_samples", "chain_reported_pdc_samples"):
        require(isinstance(observed[field], (float, int)) and math.isfinite(observed[field])
                and observed[field] >= 0, "invalid host-reported latency")
    wanted = {"VOLUME": contract["render"]["volume_normalized"], "VOICE DISPERSION": 0,
              "ANALOG DRIFT": 0, "ANALOG NOISE": 0}
    if contract["schema"] == "cs80-reaper-smoke-v2-dry":
        wanted["CHORUS ON"] = 0
    for phase in ("before_save", "after_reopen"):
        require(set(observed[phase]) == set(wanted), "missing state readback")
        for name, expected in wanted.items():
            actual = observed[phase][name]
            require(math.isfinite(actual) and abs(actual - expected) <= contract["limits"]["parameter_absolute_error"], "wrong restored " + name)
    require(abs(observed["perturbed"]["VOLUME"] - 0.2) <= 1e-6
            and abs(observed["perturbed"]["VOICE DISPERSION"] - 0.7) <= 1e-6, "state was not genuinely perturbed")
    require(set(observed["projects"]) == set(contract["render"]["files"]), "host project inventory mismatch")
    for name in ("baseline", "restored", "automated"):
        notes = observed["projects"][name]["notes"]
        require(len(notes) == len(contract["render"]["notes"]), "lost MIDI notes")
        for note, pitch in zip(notes, contract["render"]["notes"]):
            require(note["pitch"] == pitch and note["velocity"] == contract["render"]["velocity"]
                    and abs(note["start_seconds"] - contract["render"]["note_start_seconds"]) < 1e-8
                    and abs(note["end_seconds"] - contract["render"]["note_end_seconds"]) < 1e-8, "changed persisted MIDI")
    require(not observed["projects"]["no-midi"]["notes"], "no-MIDI project contains notes")
    require(observed["projects"]["automated"]["envelope"] == contract["render"]["automation_points"], "changed persisted envelope")


def verify_evidence(out, contract, source, src_tree):
    manifest = json.loads((out / "provenance.json").read_text())
    observed = json.loads((out / "host-observations.json").read_text())
    verify_metadata(manifest, observed, contract, source, src_tree, sha256(out / evidence_contract_filename(contract)))
    required = ["host-observations.json", "host-editor.png", "HOST_SETUP_COMPLETE", "setup.log", "cli-help.log"]
    if contract["schema"] == "cs80-reaper-smoke-v2-dry":
        required.extend(["host-editor-before-escape.png", "editor-escape.json"])
    for name in contract["render"]["files"]:
        required.extend([name + ".rpp", name + ".wav", "render-" + name + ".log"])
        require(manifest["exit_codes"][name] == 0, "host render did not exit cleanly")
        project = (out / (name + ".rpp")).read_text()
        require("<VST" in project and "CS-80 Reference" in project, "missing real-host plugin project chunk")
    require(manifest["exit_codes"]["setup"] == 0, "host setup did not exit cleanly")
    for name in required:
        require((out / name).is_file() and (out / name).stat().st_size > 0, "missing evidence: " + name)
        require(manifest["files"][name] == sha256(out / name), "evidence hash mismatch: " + name)
    png = (out / "host-editor.png").read_bytes()
    require(png[:8] == b"\x89PNG\r\n\x1a\n", "not a genuine PNG capture file")
    return verify_audio(contract, {name: read_wave(out / (name + ".wav")) for name in contract["render"]["files"]})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--expected-source", required=True)
    parser.add_argument("--expected-src-tree", required=True)
    args = parser.parse_args()
    contract = json.loads(args.contract.read_text())
    require(sha256(args.contract) == sha256(args.evidence / evidence_contract_filename(contract)), "registered contract differs from evidence")
    result = verify_evidence(args.evidence, contract, args.expected_source, args.expected_src_tree)
    report = {"scope": "limited REAPER Linux VST3 smoke", "result": "PASS", "metrics": result,
              "visual_review": "PENDING: inspect the actual host-editor.png; file presence is not visual acceptance",
              "excluded_claims": contract["excluded_claims"]}
    (args.evidence / "smoke-result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
