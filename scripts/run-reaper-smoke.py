#!/usr/bin/env python3
"""One bounded native Actions smoke. Never resets or extends the evaluation window."""
import argparse
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
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    contract_path = root / "tests/hosts/reaper/contract-v1.json"
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
    # Request Linux's dummy backend; the actual mode is recorded by the host API.
    # Bootstrap behavior is provisional until the native run demonstrates it.
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
