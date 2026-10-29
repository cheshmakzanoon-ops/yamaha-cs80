#!/usr/bin/env python3
"""Controlled old-epoch publication regression; production header stays untouched.

Usage: python3 tests/test_keyboard_epoch_interleaving.py --compiler /path/to/c++
GNU/Clang command-line conventions are required. All build/run subprocesses are
bounded, use argv (no shell), and execute within an automatically removed temp dir.
"""

import argparse
from pathlib import Path
import subprocess
import tempfile


PAUSE_MARKER = "        const auto w = write.load(std::memory_order_relaxed);"
FILTER_MARKER = "            if (event.epoch == current) destination[count++] = event;"
FIXTURE = r'''
#include <atomic>
#include <thread>
#include <iostream>

std::atomic<int> phase { 0 };
void pauseOldEpochProducer()
{
    phase.store(1, std::memory_order_release);
    while (phase.load(std::memory_order_acquire) != 2)
        std::this_thread::yield();
}

#include "plugin/KeyboardEventQueue.h"

int main()
{
    cs80::KeyboardEventQueue<4> queue;
    cs80::KeyboardEventQueue<4>::Batch batch;
    std::atomic<bool> accepted { false };
    std::thread producer([&]
    {
        accepted.store(queue.push(1, 60, 1.0f, true, 100));
    });
    while (phase.load(std::memory_order_acquire) != 1)
        std::this_thread::yield();
    // Producer already captured/validated the old epoch, but has not published.
    queue.requestReset();
    phase.store(2, std::memory_order_release);
    producer.join();
    if (!accepted.load())
    {
        std::cerr << "FAIL: fixture did not publish its in-flight event\n";
        return 2;
    }
    if (queue.take(batch) != 0)
    {
        std::cerr << "FAIL: old in-flight publication escaped epoch filter\n";
        return 1;
    }
    std::cout << "Controlled old-epoch interleave passed\n";
}
'''


def unique_replace(source: str, old: str, new: str) -> str:
    count = source.count(old)
    if count != 1:
        raise RuntimeError(f"Expected one instrumentation marker, found {count}: {old!r}")
    return source.replace(old, new, 1)


def run_variant(directory: Path, compiler: str, header: str) -> subprocess.CompletedProcess:
    include = directory / "plugin"
    include.mkdir(parents=True)
    (include / "KeyboardEventQueue.h").write_text(header, encoding="utf-8")
    fixture = directory / "fixture.cpp"
    fixture.write_text(FIXTURE, encoding="utf-8")
    executable = directory / "fixture"
    subprocess.run(
        [compiler, "-std=c++17", "-O1", "-pthread", "-Wall", "-Wextra",
         "-I", str(directory), str(fixture), "-o", str(executable)],
        check=True, capture_output=True, text=True, timeout=60,
    )
    return subprocess.run(
        [str(executable)], capture_output=True, text=True, timeout=10,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, help="GNU/Clang C++ compiler executable")
    args = parser.parse_args()
    header_path = Path(__file__).resolve().parents[1] / "src/plugin/KeyboardEventQueue.h"
    source = header_path.read_text(encoding="utf-8")
    instrumented = unique_replace(
        source, PAUSE_MARKER, "        ::pauseOldEpochProducer();\n" + PAUSE_MARKER,
    )
    mutant = unique_replace(
        instrumented, FILTER_MARKER, "            destination[count++] = event;",
    )
    with tempfile.TemporaryDirectory(prefix="cs80-keyboard-epoch-") as temporary:
        root = Path(temporary)
        baseline = run_variant(root / "baseline", args.compiler, instrumented)
        if baseline.returncode != 0 or "Controlled old-epoch interleave passed" not in baseline.stdout:
            raise RuntimeError(f"Baseline failed ({baseline.returncode}): {baseline.stdout}{baseline.stderr}")
        mutation = run_variant(root / "without-epoch-filter", args.compiler, mutant)
        expected = "FAIL: old in-flight publication escaped epoch filter"
        if mutation.returncode != 1 or expected not in mutation.stderr:
            raise RuntimeError(f"Mutation was not detected as expected ({mutation.returncode}): "
                               f"{mutation.stdout}{mutation.stderr}")
    print("Keyboard epoch interleave: baseline passed; stale-filter mutant detected")


if __name__ == "__main__":
    main()
