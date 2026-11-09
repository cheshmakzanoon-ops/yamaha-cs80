#!/usr/bin/env python3
"""Temporary-header mutations must cause named behavioral failures, not crashes."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
mutations = [
    ('erase_full_node_residual', 'MemorySparseCertificate.h',
     '    report.residualUpperAmps = worst;',
     '    worst = 0.0; report.residualUpperAmps = worst;',
     'FAIL: perturbed full-node state rejected'),
    ('omit_newton_receiver_feedback', 'MemorySparseNewton.h',
     'a.residual[static_cast<std::size_t>(busBase + feedbackTargets[i])] += input.feedbackG[i]',
     'a.residual[static_cast<std::size_t>(busBase + feedbackTargets[i])] += 0.0 * input.feedbackG[i]',
     'FAIL: cold sparse result is certified'),
]
with tempfile.TemporaryDirectory(prefix='cs80-memory-sparse-negative-') as directory:
    temporary = Path(directory)
    header_directory = temporary / 'experimental'
    header_directory.mkdir()
    def run(name):
        executable = temporary / name
        subprocess.run([args.compiler, '-std=c++20', '-O2', '-ffp-contract=off', '-I', str(temporary),
                        '-I', str(root / 'src'), str(root / 'tests/MemorySparseUpdateTests.cpp'),
                        '-o', str(executable)], check=True, timeout=120)
        result = subprocess.run([str(executable), str(root / 'tests/parity/tolerances-v2/memory-sparse-update.json'),
                                 '--negative-control'], text=True, capture_output=True, timeout=180)
        print(name + ': ' + result.stdout.splitlines()[-1])
        return result
    baseline = run('baseline')
    if baseline.returncode != 0:
        raise SystemExit('baseline failed: ' + baseline.stdout + baseline.stderr)
    for name, header, before, after, marker in mutations:
        text = (root / 'src/experimental' / header).read_text()
        if text.count(before) != 1:
            raise SystemExit('mutation anchor changed: ' + name)
        destination = header_directory / header
        destination.write_text(text.replace(before, after))
        result = run(name)
        destination.unlink()
        if result.returncode != 1 or marker not in result.stdout:
            raise SystemExit('mutation did not trigger its expected behavioral rejection: '
                             + name + '\n' + result.stdout + result.stderr)
print('Both sparse negative controls detected by named assertions.')
