#!/usr/bin/env python3
"""Qualify the actual macOS AU bridge with restored, temporary code mutations.

CTest runs this serially, after the normal native host gate. Each mutation
rebuilds only the affected AU wrapper, then must fail the same native host
checks. Original sources and the qualified module are restored in finally.
No production mutation switch is shipped.
"""
from pathlib import Path
import argparse
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--component", type=Path, required=True)
    parser.add_argument("--tolerances", type=Path, required=True)
    parser.add_argument("--config", default="Release")
    arguments = parser.parse_args()
    hooks = arguments.source / "src/plugin/AuAutomationHooks.inc"
    queue = arguments.source / "src/plugin/AuAutomationQueue.h"
    wrapper = arguments.build / "cs80-juce-wrappers/juce_audio_plugin_client_AU_1.mm"
    originals = {path: path.read_bytes() for path in (hooks, queue, wrapper)}
    evidence = arguments.build / "au-automation-mutation-evidence"
    evidence.mkdir(exist_ok=True)
    mutations = [
        ("old-block-adapter", hooks, "if (! client->usesHostAutomationTiming())", "if (true)"),
        ("immediate-offset-zero", hooks,
         "result.start = static_cast<int> (source.eventValues.immediate.bufferOffset);", "result.start = 0;"),
        ("pure-legacy-mask-removed", hooks, "if (! client->usesHostAutomationTiming())", "if (false)"),
        ("future-endpoint-published", queue,
         "if (pointCount > firstPoint && points[pointCount - 1].position >= frames)", "if (false)"),
        ("unsupported-ramp-capability", wrapper,
         "if (! isParameterDiscrete && cs80AutomationIndex (*param) >= 0)", "if (! isParameterDiscrete)"),
    ]

    def build(name: str) -> None:
        with (evidence / f"{name}-build.log").open("w") as log:
            result = subprocess.run(["cmake", "--build", str(arguments.build), "--target", "CS80Reference_AU",
                                     "--config", arguments.config, "-j", "2"], stdout=log, stderr=log)
        if result.returncode != 0:
            raise RuntimeError(f"{name}: AU build failed; see {evidence / (name + '-build.log')}")

    def run(name: str) -> int:
        with (evidence / f"{name}.log").open("w") as log:
            result = subprocess.run([str(arguments.host), str(arguments.component), str(arguments.tolerances)],
                                    stdout=log, stderr=log)
        print(f"{name}: native host exit {result.returncode}", flush=True)
        if result.returncode:
            lines = (evidence / f"{name}.log").read_text().splitlines()
            print("\n".join(lines[:8]), flush=True)
        return result.returncode

    failed = False
    try:
        for name, path, before, after in mutations:
            for original_path, contents in originals.items():
                original_path.write_bytes(contents)
            source = originals[path].decode("utf-8")
            if source.count(before) != 1:
                raise RuntimeError(f"{name}: mutation anchor is not unique")
            path.write_text(source.replace(before, after), encoding="utf-8")
            build(name)
            if run(name) != 1:
                raise RuntimeError(f"{name}: the native assertion gate did not detect the intended mutation")
    except (OSError, RuntimeError) as error:
        print(error, file=sys.stderr)
        failed = True
    finally:
        for original_path, contents in originals.items():
            original_path.write_bytes(contents)
        build("restored")
        if run("restored") != 0:
            print("Restored AU module failed its native gate", file=sys.stderr)
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
