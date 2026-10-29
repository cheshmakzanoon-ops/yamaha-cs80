#!/usr/bin/env python3
"""Regression checks for diagnostic-only preload and seed selection."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile

spec = importlib.util.spec_from_file_location("seed", Path(__file__).with_name("pluginval-diagnostic-seed.py"))
seed = importlib.util.module_from_spec(spec)
spec.loader.exec_module(seed)
assert seed.select_seed("Random seed: 0x12ab\n") == "0x12ab"
assert seed.select_seed("Random seed: 24\n") == "0x18"
assert seed.select_seed("Random seed: 1\nRandom seed: 0x2\n") == "0x2"
for invalid in ("", "Random seed: 0", "Random seed: 0x0",
                "Random seed: 1\nRandom seed: garbage",
                "Random seed: 0x1; echo injected", "Random seed: 18446744073709551616"):
    assert seed.select_seed(invalid) is None

library, fixture = [str(Path(arg).resolve()) for arg in sys.argv[1:3]]
with tempfile.TemporaryDirectory(prefix="cs80-trace-test-") as work:
    log = Path(work) / "trace.txt"

    def run(mode, closed=False):
        # Redirection precedes exec; preload applies only to the fixture.
        redirect = "198>&-" if closed else '198>"$3"'
        script = 'exec env LD_PRELOAD="$1" "$2" "$4" ' + redirect
        result = subprocess.run(["bash", "-c", script, "fixture", library, fixture,
                                 str(log), str(mode)], capture_output=True, timeout=10)
        assert not result.stdout and not result.stderr, (result.stdout, result.stderr)
        return result.returncode

    assert run(0) == 0
    assert run(7) == 7
    assert run(139) == 139  # Numeric139, not Python native -SIGSEGV.
    trace = log.read_text()
    assert "diagnostic fault-thread trace" in trace
    assert "cs80_diagnostic_named_fault" in trace
    assert 1 <= sum(line.startswith("#") for line in trace.splitlines()) <= 32
    assert len(trace) < 9000
    assert "SUCCESS" not in trace and "FAILED" not in trace
    # Verify signal-ucontext starts at a genuinely faulting instruction on
    # both the main thread and a newly created pthread (not handler/raise frames).
    for mode in (140, 141):
        assert run(mode) == 139
        trace = log.read_text()
        frames = [line for line in trace.splitlines() if line.startswith("#")]
        assert 1 <= len(frames) <= 32
        assert len(trace) < 9000
        assert "cs80_diagnostic_instruction_fault" in frames[0], frames
        if mode == 141:
            assert "cs80_diagnostic_worker_fault" in trace, trace
        assert "SUCCESS" not in trace and "FAILED" not in trace
    for mode in (134, 135):
        assert run(mode) == mode
        trace = log.read_text()
        assert "diagnostic fault-thread trace" in trace
        assert 1 <= sum(line.startswith("#") for line in trace.splitlines()) <= 32
        assert len(trace) < 9000
        assert "SUCCESS" not in trace and "FAILED" not in trace
    assert run(139, closed=True) == 139
    assert run(73) == 73
    assert "fault-thread trace" not in log.read_text()
    timed = subprocess.run(["timeout", "--kill-after=1s", "0.1s", "bash", "-c",
                            'exec env LD_PRELOAD="$1" /bin/sleep 2 198>"$2"',
                            "fixture", library, str(log)], capture_output=True, timeout=5)
    assert timed.returncode == 124, timed.returncode
print("Diagnostic fixtures checked: exits, instruction/worker faults, trace bound, closed fd, replacement, timeout, seed parsing.")
