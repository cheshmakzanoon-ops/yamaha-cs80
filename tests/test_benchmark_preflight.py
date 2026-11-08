#!/usr/bin/env python3
"""Rejected benchmark selections only: never execute audio/scenario cases."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('--benchmark', required=True)
args, remaining = parser.parse_known_args()
BENCHMARK = str(Path(args.benchmark).resolve())


class BenchmarkPreflight(unittest.TestCase):
    def test_rejected_before_creating_output(self):
        cases = []
        for option in ('--rate', '--block'):
            for value in ('', 'abc', '-1', '0', '1.5', '48000junk', '128junk', '999999999999999999999999'):
                cases.append([option, value])
        cases += [['--rate', '96000'], ['--block', '512'],
                  ['--quick', '--rate', '44100'], ['--quick', '--block', '64'],
                  ['--quick', '--block', '256'], ['--suite', 'performance', '--block', '64']]
        with tempfile.TemporaryDirectory() as tmp:
            for index, case in enumerate(cases):
                with self.subTest(arguments=case):
                    output = Path(tmp) / f'rejected-{index}'
                    result = subprocess.run([BENCHMARK, '--out', str(output),
                                             '--source-commit', '0123456789abcdef', *case],
                                            capture_output=True, timeout=10)
                    self.assertEqual(result.returncode, 2, result.stdout)
                    self.assertIn(b'Benchmark error:', result.stderr)
                    self.assertFalse(output.exists(), 'rejected selection created artifacts')


if __name__ == '__main__':
    unittest.main(argv=['test_benchmark_preflight.py', *remaining])
