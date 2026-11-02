import concurrent.futures
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

B = Path("/workspace/scratch/aa102d573ba3")
R = B / "cs80-ui-integration"
D = B / "cs80-ui-composed-evidence/green"
BUILD = B / "cs80-panel-s2/build-native-red"
HEAD = sys.argv[1]
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*a): return subprocess.check_output(["git", "-C", str(R), *a], text=True).strip()
def now(): return datetime.datetime.now(datetime.timezone.utc).isoformat()
assert git("rev-parse", "HEAD") == HEAD
assert git("status", "--porcelain") == ""
previous_path = B / "cs80-integration/docs/evidence/composed-processor-20261102/build-receipt.json"
previous = json.loads(previous_path.read_text())
assert previous["src_tree"] == "0f8a26e733644ac9f5985164e0175d4143c5d917"
assert subprocess.check_output(["git", "-C", str(B / "cs80-native-tools/JUCE"), "rev-parse", "HEAD"], text=True).strip() == "c352e2489000ffeb9e397051377d75eef2efe641"
for row in previous["reused_module_and_font_objects"]: assert sha(row["file"]) == row["sha256"], row["file"]
inputs = {str(p.relative_to(R)): sha(p) for top in ("src", "deps", "tests") for p in sorted((R / top).rglob("*"))
          if p.is_file() and p.suffix in (".h", ".hpp", ".cpp")}
D.mkdir(parents=True, exist_ok=False)
commands = []
replacements = {}
for row in previous["compile_commands"]:
    path = row["file"]
    target = "tests/ComposedUiComponentDriver.cpp" if path == "src/tools/ui_snapshot.cpp" else path
    obj = D / (target.replace("/", "_") + ".o")
    args = [arg.replace(str(B / "cs80-integration/src"), str(R / "src"))
            .replace(str(B / "cs80-integration/tests"), str(R / "tests")) for arg in row["argv"]]
    args[args.index("-o") + 1] = str(obj)
    args[args.index("-c") + 1] = str(R / target)
    commands.append({"file": target, "original_file": path, "argv": args, "object": str(obj)})
    replacements[row["fresh_object"]] = str(obj)
assert len(commands) == 26
link = [replacements.get(a, a) for a in previous["link_argv"]]
binary = D / "ui-components"
link[link.index("-o") + 1] = str(binary)
link = ["-Wl,--dependency-file=" + str(D / "link.d") if a.startswith("-Wl,--dependency-file=") else a for a in link]
receipt = {"head": HEAD, "source_tree": git("rev-parse", "HEAD:src"), "inputs": inputs,
           "prior_recipe_sha256": sha(previous_path), "compile_commands": commands, "link_argv": link,
           "reused_module_and_font_objects": previous["reused_module_and_font_objects"],
           "scope": "fresh 26 project TUs; 729 panel + 56 revision editor + 37 layout component checks; no native window/paint/DAW",
           "started_at": now()}
(D / "started-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
print("LOCKED_COMPOSED_UI_FRESH_BUILD", receipt["started_at"], flush=True)
def compile_one(row):
    print("COMPILE", now(), row["file"], flush=True)
    with (D / (row["file"].replace("/", "_") + ".log")).open("w") as f:
        subprocess.run(row["argv"], cwd=BUILD, stdout=f, stderr=subprocess.STDOUT, check=True, timeout=240)
    row["sha256"] = sha(row["object"])
    print("COMPILED", now(), row["file"], flush=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    list(pool.map(compile_one, commands))
with (D / "link.log").open("w") as f:
    subprocess.run(link, cwd=BUILD, stdout=f, stderr=subprocess.STDOUT, check=True, timeout=120)
linked = []
for a in link:
    p = Path(a)
    if not p.is_absolute(): p = BUILD / p
    if p.is_file() and p.suffix in (".o", ".a"): linked.append({"path": str(p), "sha256": sha(p)})
assert len(linked) == 45
assert len({row["object"] for row in commands} & {row["path"] for row in linked}) == 26
env = os.environ.copy(); env.pop("DISPLAY", None)
env["XDG_CACHE_HOME"] = str(D / "cache"); env["XDG_CONFIG_HOME"] = str(D / "config")
with (D / "test.log").open("w") as f:
    result = subprocess.run([str(binary)], env=env, stdout=f, stderr=subprocess.STDOUT, timeout=60)
log = (D / "test.log").read_text(); print(log, flush=True)
for path, expected in inputs.items(): assert sha(R / path) == expected, path
for row in linked: assert sha(row["path"]) == row["sha256"], row["path"]
assert git("rev-parse", "HEAD") == HEAD
assert git("status", "--porcelain") == ""
receipt.update({"linked_objects": linked, "binary_sha256": sha(binary), "test_exit_code": result.returncode,
                "finished_at": now()})
(D / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
assert result.returncode == 0 and "Composed UI component: 822 checks, 0 failures; desktop components0" in log and "NOT RUN" not in log, "Unexpected outcome: raw evidence retained"
print("COMPOSED_UI_POSITIVE_COMPLETE", now(), flush=True)
