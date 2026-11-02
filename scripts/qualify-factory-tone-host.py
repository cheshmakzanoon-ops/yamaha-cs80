#!/usr/bin/env python3
"""Actual isolated C++ mutants for the frozen processor-only revision fixture.

This consumes an exact build receipt, not a fresh PASS summary. It verifies all
linked object hashes, compiles only an ABI-preserving .cpp mutant, links the
unchanged test object, and requires ordinary assertion failure for the named
mechanism. Every multisecond phase holds the shared lock, released between cases.
It never edits the production source and never starts a display or a DAW.
"""
import argparse
import contextlib
import fcntl
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

@contextlib.contextmanager
def locked(path):
    with Path(path).open("a") as handle:
        fcntl.flock(handle, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(handle, fcntl.LOCK_UN)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--dependency-build", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--lock", type=Path, required=True)
    parser.add_argument("--only")
    args = parser.parse_args()
    cpp = "src/plugin/PluginProcessor.cpp"
    auto = "src/plugin/PluginProcessorAutomation.cpp"
    library = "src/plugin/PluginProcessorLibrary.cpp"
    wrapped = "withFactoryToneRevision(cs80::ControlFrame::programmer(cached, cachedHardwareSources))"
    with locked(args.lock):
        implementation = (args.source/cpp).read_text()
    if "const auto storedValue = child.getProperty" in implementation:
        inheritance_old = 'const auto storedValue = child.getProperty("value", fallback);'
        inheritance_new = ('const auto storedValue = child.getProperty("value", '
            'id == factoryToneRevisionParameterId ? factoryToneRevisionValue->load(std::memory_order_relaxed) : fallback);')
    else:
        inheritance_old = 'value = restoredFactoryToneChoice(child.getProperty("value", juce::var()));'
        inheritance_new = ('value = restoredFactoryToneChoice(child.getProperty("value", '
            'factoryToneRevisionValue->load(std::memory_order_relaxed)));')
    cases = [
        ("live-frame-reread", cpp,
         "frame.factoryToneRevision = cachedFactoryToneRevision;",
         "frame.factoryToneRevision = factoryToneRevisionValue->load(std::memory_order_relaxed) == 1.0f"
         " ? cs80::FactoryToneRevision::publicAmendedV2 : cs80::FactoryToneRevision::frozenV1;",
         "automation main uses captured V2, not new live V1", 1),
        ("missing-extension-frame", cpp,
         "extensionEngine->setControlFrame(" + wrapped + ");",
         "extensionEngine->setControlFrame(cs80::ControlFrame::programmer(cached, cachedHardwareSources));",
         "active extension adopts the same V2", 1),
        ("missing-automation-frame", auto, wrapped,
         "cs80::ControlFrame::programmer(cached, cachedHardwareSources)",
         "automation main uses captured V2, not new live V1", 2),
        ("omitted-revision-normalization", cpp,
         "    ensure(factoryToneRevisionParameterId, 0.0f);\n", "",
         "malformed revision restores V1 without clamping into opt-in", 1),
        ("inherited-missing-revision", cpp, inheritance_old, inheritance_new,
         "missing revision always restores frozen V1, never live V2", 1),
        ("loose-state-parser", cpp,
         "return end != begin && *end == '\\0' && parsed == 1.0 ? 1.0f : 0.0f;",
         "return parsed >= 0.5 ? 1.0f : 0.0f;",
         "malformed revision restores V1 without clamping into opt-in", 1),
        ("missing-ab-recall", library,
         "    write(library->factoryToneRevision, s.factoryToneRevision);\n", "",
         "A/B recalls the copied V2 sound", 1),
        ("missing-factory-reset", cpp,
         "    if (library->factoryToneRevision != nullptr)\n"
         "        library->factoryToneRevision->setValueNotifyingHost(0.0f);\n", "",
         "shipped factory recall resets to frozen V1", 1),
        ("missing-midi-reset", library,
         "    if (library->factoryToneRevision != nullptr)\n"
         "        library->factoryToneRevision->setValueNotifyingHost(0.0f);\n", "",
         "accepted MIDI Program Change resets revision to frozen V1", 1),
        ("main-only-program", library,
         """    forEachEngine([&](cs80::Engine& target)
    {
        const auto prior = target.getRenderPolicy();
        target.setControlFrame(frame);
        if (target.getRenderPolicy() != prior)
            target.setExpression(expressionValue.load(std::memory_order_relaxed));
    });""",
         """    [&](cs80::Engine& target)
    {
        const auto prior = target.getRenderPolicy();
        target.setControlFrame(frame);
        if (target.getRenderPolicy() != prior)
            target.setExpression(expressionValue.load(std::memory_order_relaxed));
    }(engine);""",
         "MIDI Program Change publishes the same aggregate frame to both active engines", 1),
        ("programmer-program", library,
         "withFactoryToneRevision({ cached, cachedHardwareSources })", wrapped,
         "Program Change main retains aggregate physical units, not programmer coordinates", 1),
    ]
    with locked(args.lock):
        receipt = json.loads(args.receipt.read_text())
        head_src = subprocess.check_output(["git", "rev-parse", "HEAD:src"], cwd=args.source, text=True).strip()
        if head_src != receipt["src_tree"] or receipt["working_diff"]:
            raise RuntimeError("receipt/source is not the clean qualified source")
        if subprocess.check_output(["git", "diff", "--", "src"], cwd=args.source):
            raise RuntimeError("uncommitted production source changes")
        for record in receipt["compile_commands"]:
            if sha(record["fresh_object"]) != record["sha256"]:
                raise RuntimeError("changed project object: " + record["fresh_object"])
        for record in receipt["reused_module_and_font_objects"]:
            if sha(record["file"]) != record["sha256"]:
                raise RuntimeError("changed module/font dependency: " + record["file"])
        binary = Path(receipt["link_argv"][receipt["link_argv"].index("-o")+1])
        if sha(binary) != receipt["binary_sha256"] or receipt["test_exit_code"] != 0:
            raise RuntimeError("positive executable/receipt mismatch")
        entries = {r["file"]: r for r in receipt["compile_commands"]}
        args.out.mkdir(parents=True, exist_ok=True)
        contract = {
            "source_head": receipt["head"], "source_tree": receipt["src_tree"],
            "positive_binary_sha256": receipt["binary_sha256"],
            "cases": [{"name": n, "file": f, "old": o, "new": v,
                       "required_failure": message, "replace_count": count}
                      for n, f, o, v, message, count in cases],
            "acceptance": "compile/link success; process exit1; named ordinary FAIL; all2257 checks execute; no crash",
        }
        (args.out/"mutation-contract.json").write_text(json.dumps(contract, indent=2)+"\n")
    results = []
    selected = [c for c in cases if args.only is None or c[0] == args.only]
    if not selected:
        raise RuntimeError("unknown mutant selector")
    for name, file, old, new, message, count in selected:
        with locked(args.lock):
            started = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
            print(started, "MUTANT", name, flush=True)
            case = args.out/name
            case.mkdir(exist_ok=True)
            original = (args.source/file).read_text()
            if original.count(old) != count:
                raise RuntimeError("mutation target/count mismatch: "+name)
            mutated = original.replace(old, new)
            source = case/Path(file).name
            source.write_text(mutated)
            obj = case/"mutant.o"
            entry = entries[file]
            compile_args = entry["argv"].copy()
            compile_args[compile_args.index("-c")+1] = str(source)
            compile_args[compile_args.index("-o")+1] = str(obj)
            compile_args.insert(1, "-I"+str((args.source/file).parent))
            with (case/"compile.log").open("w") as stream:
                subprocess.run(compile_args, cwd=args.dependency_build, stdout=stream,
                               stderr=subprocess.STDOUT, check=True)
            link = receipt["link_argv"].copy()
            if link.count(entry["fresh_object"]) != 1:
                raise RuntimeError("expected exactly one production TU replacement")
            link[link.index(entry["fresh_object"])] = str(obj)
            executable = case/"CS80Snapshots"
            link[link.index("-o")+1] = str(executable)
            with (case/"link.log").open("w") as stream:
                subprocess.run(link, cwd=args.dependency_build, stdout=stream,
                               stderr=subprocess.STDOUT, check=True)
            with (case/"test.log").open("w") as stream:
                result = subprocess.run([str(executable), "--factory-tone-revision-test"],
                    stdout=stream, stderr=subprocess.STDOUT, timeout=120)
            log = (case/"test.log").read_text()
            match = re.search(r"Factory tone revision processor: (\d+) checks, (\d+) failures", log)
            accepted = (result.returncode == 1 and match is not None
                        and int(match[1]) == 2257 and int(match[2]) > 0
                        and "FAIL: "+message in log)
            record = {"name": name, "started_at": started,
                      "finished_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                      "source_sha256": sha(source), "binary_sha256": sha(executable),
                      "compile_argv": compile_args, "link_argv": link,
                      "exit_code": result.returncode,
                      "checks": int(match[1]) if match else None,
                      "ordinary_failures": int(match[2]) if match else None,
                      "required_failure": message, "accepted_negative": accepted}
            (case/"receipt.json").write_text(json.dumps(record, indent=2)+"\n")
            results.append(record)
            print("REJECTED" if accepted else "INVALID", name,
                  "exit", result.returncode, "failures", record["ordinary_failures"], flush=True)
            if not accepted:
                raise RuntimeError("mutant did not fail for its registered reason: "+name)
    with locked(args.lock):
        # Restore/control is the untouched original executable, rehashed again.
        if sha(binary) != receipt["binary_sha256"]:
            raise RuntimeError("positive executable was altered during mutation checks")
        with (args.out/"restored-positive.log").open("w") as stream:
            restored = subprocess.run([str(binary), "--factory-tone-revision-test"],
                stdout=stream, stderr=subprocess.STDOUT, timeout=120)
        restored.check_returncode()
        summary = {"source_head": receipt["head"], "source_tree": receipt["src_tree"],
                   "results": results, "restored_exit": restored.returncode,
                   "restored_binary_sha256": sha(binary)}
        (args.out/"summary.json").write_text(json.dumps(summary, indent=2)+"\n")
        print("All", len(results), "actual compiled negatives rejected; original positive restored unchanged.", flush=True)

if __name__ == "__main__":
    main()
