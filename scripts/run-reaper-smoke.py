#!/usr/bin/env python3
"""One bounded native Actions smoke. Never resets or extends the evaluation window."""
import argparse
import csv
import io
import math
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def lua(value):
    if isinstance(value, dict):
        return "{" + ",".join("[" + lua(k) + "]=" + lua(v) for k, v in value.items()) + "}"
    if isinstance(value, list):
        return "{" + ",".join(map(lua, value)) + "}"
    if isinstance(value, str):
        # JSON's escaping is compatible for the ASCII contract's strings.
        if not value.isascii():
            raise ValueError("non-ASCII contract needs explicit Lua encoding")
        return json.dumps(value)
    if isinstance(value, (int, float)):
        return repr(value)
    raise ValueError("unsupported contract type")


def capture(out, name):
    subprocess.run(["scrot", str(out / name)], check=True, timeout=15)
    with (out / (name + ".windows.txt")).open("w") as log:
        subprocess.run(["xwininfo", "-root", "-tree"], stdout=log, stderr=subprocess.STDOUT,
                       check=False, timeout=10)


def window_geometry(text, window):
    """Fail closed on anything other than the observed default native editor."""
    result = {}
    for line in text.splitlines():
        key, sep, value = line.partition("=")
        if not sep or key in result or key not in ("WINDOW", "X", "Y", "WIDTH", "HEIGHT", "SCREEN"):
            raise ValueError("invalid editor geometry")
        try:
            result[key] = int(value)
        except ValueError as exc:
            raise ValueError("invalid editor geometry") from exc
    if (set(result) != {"WINDOW", "X", "Y", "WIDTH", "HEIGHT", "SCREEN"}
            or result["WINDOW"] != int(window) or result["WIDTH"] != 1600
            or result["HEIGHT"] != 1028 or min(result["X"], result["Y"], result["SCREEN"]) < 0):
        raise ValueError("unsupported editor geometry")
    return result


def observed_text_lines(tsv):
    """Read real Tesseract word boxes; this is never a generated UI image."""
    groups = {}
    fields = {"level", "page_num", "block_num", "par_num", "line_num", "word_num",
              "left", "top", "width", "height", "conf", "text"}
    reader = csv.DictReader(io.StringIO(tsv), delimiter="\t")
    if set(reader.fieldnames or ()) != fields:
        raise ValueError("invalid OCR TSV columns")
    for row in reader:
        try:
            if int(row["level"]) != 5 or not row["text"].strip():
                continue
            word = {k: int(row[k]) for k in ("word_num", "left", "top", "width", "height")}
            word["conf"] = float(row["conf"])
            word["text"] = row["text"].strip()
            if (min(word[k] for k in ("left", "top")) < 0
                    or min(word[k] for k in ("word_num", "width", "height")) <= 0
                    or not math.isfinite(word["conf"]) or not 0 <= word["conf"] <= 100):
                raise ValueError("invalid OCR word box")
            group = tuple(int(row[k]) for k in ("page_num", "block_num", "par_num", "line_num"))
            groups.setdefault(group, []).append(word)
        except (TypeError, KeyError, ValueError, AttributeError) as exc:
            raise ValueError("invalid OCR word box") from exc
    result = []
    for words in groups.values():
        words.sort(key=lambda w: w["word_num"])
        if len({w["word_num"] for w in words}) != len(words):
            raise ValueError("duplicate OCR word number")
        result.append({"text": " ".join(w["text"] for w in words).casefold(),
                       "confidence": min(w["conf"] for w in words),
                       "bounds": [min(w["left"] for w in words), min(w["top"] for w in words),
                                  max(w["left"] + w["width"] for w in words),
                                  max(w["top"] + w["height"] for w in words)]})
    return result


def within(box, region):
    return region[0] <= box[0] < box[2] <= region[2] and region[1] <= box[1] < box[3] <= region[3]


def first_tour_target(tsv, geometry):
    x, y = geometry["X"], geometry["Y"]
    editor = [x, y, x + geometry["WIDTH"], y + geometry["HEIGHT"]]
    lines = [line for line in observed_text_lines(tsv) if within(line["bounds"], editor)]
    # The fixed first card is centred in the 1600x1000 plugin below REAPER's
    # observed 28px preset header. These regions are only safety constraints;
    # the click point itself always comes from the visible OCR label.
    anchors = {"heading": ("welcome to the cs-80", [x+583, y+438, x+1017, y+478], 70),
               "step": ("tour1/9", [x+583, y+424, x+700, y+440], 0),
               "button": ("close tour", [x+583, y+592, x+713, y+628], 70)}
    result = {}
    for name, (wanted, bounds, confidence) in anchors.items():
        matches = [line for line in lines
                   if (line["text"].replace(" ", "") if name == "step" else line["text"]) == wanted]
        if (len(matches) != 1 or not within(matches[0]["bounds"], bounds)
                or matches[0]["confidence"] < confidence):
            raise ValueError("missing, ambiguous or unsafe first-tour " + name)
        result[name] = matches[0]
    left, top, right, bottom = result["button"]["bounds"]
    result["point"] = [(left + right) // 2, (top + bottom) // 2]
    return result


def require_tour_absent(tsv, geometry):
    x, y = geometry["X"], geometry["Y"]
    editor = [x, y, x + geometry["WIDTH"], y + geometry["HEIGHT"]]
    for line in observed_text_lines(tsv):
        if within(line["bounds"], editor) and (line["text"] in ("welcome to the cs-80", "close tour")
                or line["text"].replace(" ", "") == "tour1/9"):
            raise ValueError("first-tour text remains after click")


def tour_seen(path):
    if not path.exists():
        return False
    element = ET.parse(path).getroot()
    if element.tag != "CS80_UI_SETTINGS" or element.get("tourSeen", "0") not in ("0", "1", "false", "true"):
        raise ValueError("invalid tour settings")
    return element.get("tourSeen", "0") in ("1", "true")


def read_ocr(out, name):
    image = out / name
    stem = image.with_suffix("")
    with (out / (stem.name + "-ocr.log")).open("w") as log:
        subprocess.run(["tesseract", str(image), str(stem), "--psm", "11", "tsv"],
                       check=True, timeout=30, stdout=log, stderr=subprocess.STDOUT)
    return stem.with_suffix(".tsv").read_text()


def dismiss_tour_and_capture(out):
    """One verified visible Close Tour click, never Escape through the host."""
    capture(out, "host-editor-before-dismiss.png")
    windows = subprocess.check_output(
        ["xdotool", "search", "--onlyvisible", "--name", "^VST3i: CS-80 Reference .*CS-80 real-host smoke"],
        text=True, timeout=10).splitlines()
    if len(windows) != 1 or not windows[0].isdigit():
        raise ValueError("expected one visible instrument window for bounded tour dismissal")
    window = windows[0]
    query = ["xdotool", "getwindowgeometry", "--shell", window]
    geometry = window_geometry(subprocess.check_output(query, text=True, timeout=10), window)
    settings = out / "isolated-home/.config/CS80Reference/ui-settings.xml"
    if tour_seen(settings):
        raise ValueError("tour was already marked seen before input")
    subprocess.run(["xdotool", "windowactivate", "--sync", window], check=True, timeout=10)
    capture(out, "host-editor-ready-to-dismiss.png")
    target = first_tour_target(read_ocr(out, "host-editor-ready-to-dismiss.png"), geometry)
    if window_geometry(subprocess.check_output(query, text=True, timeout=10), window) != geometry:
        raise ValueError("editor geometry changed before click")
    if subprocess.check_output(["xdotool", "getactivewindow"], text=True, timeout=10).strip() != window:
        raise ValueError("instrument window lost focus before click")
    (out / "tour-target.json").write_text(json.dumps({"window": window, "geometry": geometry,
                                                   "target": target}, indent=2) + "\n")
    x, y = map(str, target["point"])
    subprocess.run(["xdotool", "mousemove", "--sync", x, y], check=True, timeout=10)
    subprocess.run(["xdotool", "click", "--clearmodifiers", "1"], check=True, timeout=10)
    time.sleep(1)
    capture(out, "host-editor.png")
    require_tour_absent(read_ocr(out, "host-editor.png"), geometry)
    if not tour_seen(settings):
        raise ValueError("tour callback did not persist dismissal")
    shutil.copy2(settings, out / "tour-settings.xml")
    (out / "editor-dismissal.json").write_text(json.dumps(
        {"window": window, "action": "left_click", "count": 1, "point": target["point"],
         "geometry": geometry, "target": target, "tour_seen_before": False, "tour_seen_after": True,
         "tour_text_absent_after": True,
         "scope": "actual X input selected from raw screenshot OCR; visual result must be inspected"}, indent=2) + "\n")

def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=10)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reaper", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--plugin", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--contract", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    contract_path = args.contract.resolve() if args.contract else root / "tests/hosts/reaper/contract-v1.json"
    contract = json.loads(contract_path.read_text())
    if os.environ.get("GITHUB_ACTIONS") != "true" or not os.environ.get("DISPLAY"):
        raise ValueError("native host execution is restricted to an Actions X display")
    now = datetime.now(timezone.utc)
    begin = datetime.fromisoformat(contract["host"]["evaluation_start_utc"].replace("Z", "+00:00"))
    end = datetime.fromisoformat(contract["host"]["evaluation_end_exclusive_utc"].replace("Z", "+00:00"))
    if not begin <= now < end:
        raise ValueError("outside the fixed evaluation period; separately authorized licensing is required")
    if sha256(args.archive) != contract["host"]["sha256"]:
        raise ValueError("official host archive checksum mismatch")
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    shutil.copy2(contract_path, out / contract_path.name)
    (out / "contract.lua").write_text("return " + lua(contract) + "\n")
    home = out / "isolated-home"
    installed = home / ".vst3" / args.plugin.name
    installed.parent.mkdir(parents=True)
    shutil.copytree(args.plugin, installed)
    binaries = list(args.plugin.glob("Contents/x86_64-linux/*.so"))
    if len(binaries) != 1:
        raise ValueError("expected one built Linux x86-64 VST3 module")
    build_binary = binaries[0]
    installed_binary = installed / build_binary.relative_to(args.plugin)
    profile = home / ".config/REAPER"
    profile.mkdir(parents=True)
    config = profile / "reaper.ini"
    # Isolated ordinary host configuration only. No license keys, registry edits,
    # evaluation-clock edits, persistent machine settings, or network audio.
    # The original value 3 is retained, but V1 reported no open live device.
    # Only offline rendering is claimed; record actual mode availability.
    config.write_text("[REAPER]\naudiomode=3\nlastproject=\n"
                      + "vstpath64=" + str(installed.parent) + "\n")
    env = os.environ.copy()
    env.update(HOME=str(home), XDG_CONFIG_HOME=str(home / ".config"),
               CS80_REAPER_OUT=str(out), CS80_REAPER_FIXTURE=str(root / "tests/hosts/reaper"),
               JUCE_FONT_PATH="/usr/share/fonts/truetype/dejavu")
    executable = str(args.reaper.resolve())
    common = [executable, "-newinst", "-nosplash", "-cfgfile", str(config)]
    manifest = {
        "schema": contract["schema"], "started_utc": now.isoformat(),
        "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "src_tree": subprocess.check_output(["git", "rev-parse", "HEAD:src"], cwd=root, text=True).strip(),
        "host_url": contract["host"]["url"], "host_archive_sha256": sha256(args.archive),
        "host_executable_sha256": sha256(args.reaper), "contract_sha256": sha256(contract_path),
        "plugin_build_path": str(build_binary.resolve()), "plugin_installed_path": str(installed_binary),
        "plugin_build_sha256": sha256(build_binary), "plugin_installed_sha256": sha256(installed_binary),
        "commands": [], "exit_codes": {}, "files": {},
    }
    if manifest["source_commit"] != os.environ.get("GITHUB_SHA"):
        raise ValueError("checkout differs from the Actions event commit")
    manifest["workflow_run_id"] = os.environ.get("GITHUB_RUN_ID")
    manifest["workflow_run_attempt"] = os.environ.get("GITHUB_RUN_ATTEMPT")
    process = None
    try:
        with (out / "cli-help.log").open("w") as log:
            result = subprocess.run([executable, "-help"], env=env, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=15)
        manifest["exit_codes"]["help"] = result.returncode
        help_text = (out / "cli-help.log").read_text()
        if "renderproject" not in help_text or "cfgfile" not in help_text:
            raise ValueError("pinned executable did not confirm required command-line options")
        command = common + [str(root / "tests/hosts/reaper/create-projects.lua")]
        manifest["commands"].append(command)
        with (out / "setup.log").open("w") as log:
            process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + contract["limits"]["host_setup_timeout_seconds"]
            captured = False
            while process.poll() is None:
                if (out / "FAILED").exists():
                    raise ValueError("real-host fixture failed: " + (out / "FAILED").read_text())
                if (out / "READY_FOR_SCREENSHOT").exists() and not captured:
                    # Let the actual X window paint; this delay is not a pass oracle.
                    time.sleep(1)
                    if contract["schema"] == "cs80-reaper-smoke-v2-dry":
                        dismiss_tour_and_capture(out)
                    else:
                        capture(out, "host-editor.png")
                    (out / "SCREENSHOT_CAPTURED").write_text("scrot returned successfully\n")
                    captured = True
                if time.monotonic() > deadline:
                    raise TimeoutError("host setup did not complete within the registered timeout")
                time.sleep(0.1)
            manifest["exit_codes"]["setup"] = process.returncode
            if process.returncode != 0 or not (out / "HOST_SETUP_COMPLETE").exists():
                raise ValueError("host setup exited without a complete successful observation record")
        for name in contract["render"]["files"]:
            command = common + ["-renderproject", str(out / (name + ".rpp"))]
            manifest["commands"].append(command)
            with (out / ("render-" + name + ".log")).open("w") as log:
                process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
                result = process.wait(timeout=contract["limits"]["render_timeout_seconds_each"])
            manifest["exit_codes"][name] = result
            if result != 0:
                raise ValueError("real-host render process failed: " + name)
        manifest["finished_utc"] = datetime.now(timezone.utc).isoformat()
    except BaseException as exc:
        manifest["error"] = str(exc)
        try:
            capture(out, "failure-screen.png")
        except Exception as capture_error:
            manifest["capture_error"] = str(capture_error)
        raise
    finally:
        if process is not None:
            stop(process)
        # Preserve project/WAV/log evidence, not the evaluation binary or profile.
        for path in sorted(out.iterdir()):
            if path.is_file() and path.name != "provenance.json":
                manifest["files"][path.name] = sha256(path)
        (out / "provenance.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
