#!/usr/bin/env python3
"""Replay actual-source release guard mutants in temporary modules only.

No production file, measurement or historical artifact is altered. Ordinary
assertion failures in the negative controls must coexist with a passing sealed
real positive fixture; import failures, crashes and runtime errors do not count.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_cpu_workload_witnesses as workload

MUTATIONS = (
    ('omit_workload_verification', '    verify_cpu_workload(spec, metadata, rows, label)\n', '', 29),
    ('omit_fixed_metadata', 'for field, expected in spec["registered_workload_metadata"].items():',
     'for field, expected in {}.items():', 7),
    ('omit_held_callback_count', 'require(callbacks == held_callbacks,', 'require(True,', 4),
    ('omit_held_voice_count', 'require(voices == expected_voices and complete == 0,', 'require(True,', 1),
    ('omit_selector_event_count', 'require(integer("selector_events") == events,', 'require(True,', 1),
    ('omit_source_compilation_count', 'require(integer("source_compilations") == expected_compilations,', 'require(True,', 2),
    ('omit_tail_observation_window', 'require(minimum_tail <= callbacks <= maximum_tail,', 'require(True,', 2),
)


def execute(case_type, log_path):
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(case_type))
    log_path.write_text(stream.getvalue(), encoding='utf-8')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True, help='Fresh output directory; never overwritten')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    source_path = ROOT / 'scripts/release_numerical_gate.py'
    source = source_path.read_text()
    original = workload.gate
    baseline = execute(workload.CpuWorkloadWitnessTests, args.out / 'unmodified-control.log')
    if not baseline.wasSuccessful():
        raise RuntimeError('Unmodified positive/negative-control suite failed; no mutation counted')
    summary = []
    try:
        for name, old, new, expected in MUTATIONS:
            if source.count(old) != 1:
                raise RuntimeError(f'{name}: mutation anchor absent or ambiguous')
            with tempfile.TemporaryDirectory(prefix='cs80-guard-mutant-') as temp:
                path = Path(temp) / 'release_numerical_gate.py'
                path.write_text(source.replace(old, new))
                spec = importlib.util.spec_from_file_location(name, path)
                module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(module)
                workload.gate = module
                result = execute(workload.CpuWorkloadWitnessTests, args.out / f'{name}.log')
                positive = not any('test_frozen_real_round' in str(test)
                                   for test, _ in result.failures + result.errors)
                if result.errors or not positive or len(result.failures) != expected:
                    raise RuntimeError(f'{name}: expected {expected} ordinary failures with real positive retained')
                summary.append({'mutation': name, 'compiled': True,
                                'sealed_real_positive_passed': positive,
                                'ordinary_assertion_failures': len(result.failures),
                                'errors': len(result.errors), 'replace_from': old, 'replace_to': new})
    finally:
        workload.gate = original
    restored = execute(workload.CpuWorkloadWitnessTests, args.out / 'restored-control.log')
    if not restored.wasSuccessful():
        raise RuntimeError('Restored original control failed')
    manifest = {'gate_source_sha256': hashlib.sha256(source_path.read_bytes()).hexdigest(),
                'unit_tests_only_not_performance': True, 'mutations': summary,
                'restored_original_passed': True}
    (args.out / 'qualification.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({'pass': True, 'mutants': len(summary), 'out': str(args.out)}))


if __name__ == '__main__':
    main()
