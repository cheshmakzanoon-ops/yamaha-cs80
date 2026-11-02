#!/usr/bin/env python3
"""Compile the actual reverted ENGINE/PURE locale decoder in isolation.

Requires a clean exact-source positive build receipt. All build/test phases
hold the shared lock; shipping source is never edited. A crash or compile
failure cannot substitute for the registered ordinary assertion failures.
"""
import argparse
from collections import Counter
from contextlib import contextmanager
import fcntl
import hashlib
import json
from pathlib import Path
import subprocess
import time

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

@contextmanager
def locked(path):
    with path.open("a") as handle:
        fcntl.flock(handle, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(handle, fcntl.LOCK_UN)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "receipt", "dependency-build", "out", "lock"):
        parser.add_argument("--"+name, type=Path, required=True)
    args = parser.parse_args()
    cases = (
        ("--render-policy-locale-test", "Render policy locale processor: 2290 checks, 0 failures"),
        ("--factory-tone-revision-test", "Factory tone revision processor: 2257 checks, 0 failures"),
        ("--factory-tone-locale-test", "Factory tone locale processor: 296 checks, 0 failures"),
    )
    expected_failures = {
        "restored policy reaches the actual Engine at the next audio transaction": 35,
        "session ENGINE preserves complete literals and schema fallback across locales": 24,
        "session PURE preserves complete literals and off fallback across locales": 24,
        "user preset reaches exact recalled ENGINE and live PURE policy": 2,
        "user preset recalls its ENGINE independently of host numeric locale": 4,
    }
    with locked(args.lock):
        receipt = json.loads(args.receipt.read_text())
        if subprocess.check_output(["git", "diff", "--", "src", "tests"], cwd=args.source):
            raise RuntimeError("dirty source/tests")
        changed = set(subprocess.check_output(["git", "diff", "--name-only", receipt["head"], "HEAD", "--", "src", "tests"], cwd=args.source, text=True).splitlines())
        # This standalone auxiliary driver is not in the positive console TU.
        # No original project/header/test may differ from the linked receipt.
        if not changed.issubset({"tests/RenderPolicyLocaleAllocationProbe.cpp"}):
            raise RuntimeError("receipt source/test identity changed")
        if subprocess.check_output(["git", "rev-parse", "HEAD:src"], cwd=args.source, text=True).strip() != receipt["src_tree"]:
            raise RuntimeError("receipt source-tree mismatch")
        for record in receipt["compile_commands"]:
            if sha(record["fresh_object"]) != record["sha256"]:
                raise RuntimeError("project object changed")
        for record in receipt["reused_module_and_font_objects"]:
            if sha(record["file"]) != record["sha256"]:
                raise RuntimeError("module/font object changed")
        binary = Path(receipt["link_argv"][receipt["link_argv"].index("-o")+1])
        if sha(binary) != receipt["binary_sha256"] or receipt["test_exit_code"] != 0:
            raise RuntimeError("positive binary/receipt mismatch")
        args.out.mkdir(parents=True, exist_ok=True)
        for flag, expected in cases:
            result = subprocess.run([str(binary), flag], capture_output=True, text=True, timeout=120)
            log = result.stdout + result.stderr
            (args.out/("initial"+flag+".log")).write_text(log)
            if result.returncode != 0 or expected not in log:
                raise RuntimeError("initial positive failed: "+flag)
        contract = {
            "source_head": receipt["head"], "source_tree": receipt["src_tree"],
            "receipt_sha256": sha(args.receipt), "positive_binary_sha256": sha(binary),
            "mutant": "replace only old ENGINE/PURE explicit-C parse with std::strtod",
            "expected_exit": 1, "checks": 2290, "ordinary_failure_counts": expected_failures,
            "positive_modes": [flag for flag, _ in cases],
        }
        (args.out/"contract.json").write_text(json.dumps(contract, indent=2)+"\n")
    with locked(args.lock):
        started = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        file = "src/plugin/PluginProcessor.cpp"
        original = (args.source/file).read_text()
        old = "            const auto parsed = numericLocale.parse(begin, &end);"
        new = "            const auto parsed = std::strtod(begin, &end);"
        if original.count(old) != 1:
            raise RuntimeError("expected exactly one old-policy mutation target")
        case = args.out/"plain-strtod"
        case.mkdir(exist_ok=True)
        cpp = case/"PluginProcessor.cpp"
        cpp.write_text(original.replace(old, new))
        entry = next(record for record in receipt["compile_commands"] if record["file"] == file)
        obj = case/"processor.o"
        command = entry["argv"].copy()
        command[command.index("-c")+1] = str(cpp)
        command[command.index("-o")+1] = str(obj)
        command.insert(1, "-I"+str((args.source/file).parent))
        with (case/"compile.log").open("w") as log:
            subprocess.run(command, cwd=args.dependency_build, stdout=log, stderr=subprocess.STDOUT, check=True)
        link = receipt["link_argv"].copy()
        if link.count(entry["fresh_object"]) != 1:
            raise RuntimeError("ambiguous linked project object")
        link[link.index(entry["fresh_object"])] = str(obj)
        mutant = case/"CS80Snapshots"
        link[link.index("-o")+1] = str(mutant)
        with (case/"link.log").open("w") as log:
            subprocess.run(link, cwd=args.dependency_build, stdout=log, stderr=subprocess.STDOUT, check=True)
        result = subprocess.run([str(mutant), cases[0][0]], capture_output=True, text=True, timeout=120)
        log = result.stdout + result.stderr
        (case/"test.log").write_text(log)
        failures = Counter(line[len("FAIL: "):] for line in log.splitlines() if line.startswith("FAIL: "))
        accepted = (result.returncode == 1 and "Render policy locale processor: 2290 checks, 89 failures" in log
                    and failures == expected_failures
                    and "FAIL: " not in log.split("Render policy locale fixture: de_DE.utf8")[0])
        summary = dict(contract, started_at=started,
            finished_at=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            mutant_source_sha256=sha(cpp), mutant_binary_sha256=sha(mutant),
            compile_argv=command, link_argv=link, exit_code=result.returncode,
            observed_failure_counts=dict(failures), accepted=accepted)
        (case/"receipt.json").write_text(json.dumps(summary, indent=2)+"\n")
        if not accepted:
            raise RuntimeError("actual decoder mutant did not reproduce its exact intended failures")
    with locked(args.lock):
        if sha(binary) != receipt["binary_sha256"]:
            raise RuntimeError("original executable changed during mutation")
        for flag, expected in cases:
            result = subprocess.run([str(binary), flag], capture_output=True, text=True, timeout=120)
            log = result.stdout + result.stderr
            (args.out/("restored"+flag+".log")).write_text(log)
            if result.returncode != 0 or expected not in log:
                raise RuntimeError("restored positive failed: "+flag)
        summary["restored_positive_binary_sha256"] = sha(binary)
        summary["all_restored_modes_passed"] = True
        (args.out/"summary.json").write_text(json.dumps(summary, indent=2)+"\n")
        print("Actual old-policy strtod mutant: 2290 checks, 89 intended ordinary failures.")
        print("Unmodified/restored policy2290, host2257 and tone-locale296 positives all pass.")

if __name__ == "__main__":
    main()
