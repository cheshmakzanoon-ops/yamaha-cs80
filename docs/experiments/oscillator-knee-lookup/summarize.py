#!/usr/bin/env python3
"""Validate every recorded pair and reproduce the predeclared rejection decision."""
import argparse
import csv
import json
import statistics
from pathlib import Path

SCENARIOS = ('held-low', 'held-middle', 'held-high', 'moving-pitch-pwm', 'dense-controls', 'idle')

def summarize_one(root, prefix):
    summary = {'decision': 'reject', 'profiles': {}, 'scope': 'component timing, not whole-Engine or a deadline qualification'}
    for mode in ('generic', 'native'):
        path = root / f'{prefix}-timing-{mode}.csv'
        with path.open(newline='') as file:
            rows = list(csv.DictReader(file))
        assert len(rows) == 216, (path, len(rows))
        expected = [(name, count, round_, order, 'baseline' if (order + round_) % 2 == 0 else 'candidate')
                    for count in (16, 32) for name in SCENARIOS for round_ in range(9) for order in (0, 1)]
        pairs = {}
        for row, key in zip(rows, expected):
            actual = (row['scenario'], int(row['oscillators']), int(row['round']), int(row['order']), row['version'])
            assert actual == key, (actual, key)
            assert int(row['frames']) == 32768
            assert int(row['evaluations']) == int(row['oscillators']) * (2048 if row['scenario'] == 'idle' else 32768)
            assert int(row['failed']) == 0
            assert int(row['cpu_start']) == int(row['cpu_end']) == 8
            ns = int(row['elapsed_ns'])
            assert ns > 0
            assert abs(float(row['rtf']) - ns * 1e-9 / (32768 / 96000)) < 1e-10
            pairs.setdefault(actual[:3], {})[row['version']] = row
        cases = []
        pooled = []
        for count in (16, 32):
            for name in SCENARIOS:
                reductions, before, after = [], [], []
                for round_ in range(9):
                    pair = pairs[(name, count, round_)]
                    assert pair['baseline']['checksum'] == pair['candidate']['checksum']
                    baseline, candidate = (int(pair[v]['elapsed_ns']) for v in ('baseline', 'candidate'))
                    reductions.append(100 * (1 - candidate / baseline))
                    before.append(baseline * 1e-9 / (32768 / 96000))
                    after.append(candidate * 1e-9 / (32768 / 96000))
                median = statistics.median(reductions)
                if count == 16 and name != 'idle':
                    pooled.extend(reductions)
                cases.append({'scenario': name, 'oscillators': count, 'pairs': 9,
                              'baseline_median_rtf': statistics.median(before), 'candidate_median_rtf': statistics.median(after),
                              'median_paired_reduction_percent': median,
                              'min_paired_reduction_percent': min(reductions), 'max_paired_reduction_percent': max(reductions),
                              'improving_pairs': sum(r > 0 for r in reductions), 'within_regression_gate': median >= -3})
        assert len(pooled) == 45
        median = statistics.median(pooled)
        profile = {'rows': len(rows), 'pairs': len(pairs), 'pooled_active_16_pairs': len(pooled),
                   'pooled_active_16_median_reduction_percent': median,
                   'active_gate_passes': median >= 3,
                   'case_regression_gate_passes': all(c['within_regression_gate'] for c in cases), 'cases': cases}
        summary['profiles'][mode] = profile
    summary['timing_gate_passes'] = all(p['active_gate_passes'] and p['case_regression_gate_passes'] for p in summary['profiles'].values())
    assert not summary['timing_gate_passes'], 'Recorded decision no longer matches these inputs'
    return summary

def summarize(root):
    return {'decision':'reject both variants', 'rows':864, 'pairs':432,
            'scalar_nearest':summarize_one(root / 'rejected/scalar-nearest', 'knee'),
            'vector':summarize_one(root / 'rejected/vector', 'vector')}

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    text = json.dumps(summarize(args.input), indent=2) + '\n'
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end='')
