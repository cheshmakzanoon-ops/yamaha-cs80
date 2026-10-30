#!/usr/bin/env python3
"""Reconstruct every statistic from retained observations; never discard outliers."""
import csv
import gzip
import json
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path

root = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).resolve().parent / 'first-measurement'
meta = json.loads((root / 'metadata.json').read_text())
status = json.loads((root / 'status.json').read_text())
assert status['completed'] and not meta['verification_only']
assert status['pairs_complete'] == 240 and status['raw_observations'] == 245760
assert meta['callbacks_per_arm'] == 512 and meta['warmups'] == 64
assert meta['timed_mxcsr_control_bits'] & 0x8040 == 0x8040
matrix = list(csv.DictReader((root / 'matrix.csv').open()))
rows = list(csv.DictReader((root / 'results.csv').open()))
assert len(matrix) == 240 and len(rows) == 480
assert {int(x['case_id']) for x in matrix} == set(range(240))
expected = [(rate, block, scene, condition, controls, policy)
            for rate in (44100, 48000) for block in (128, 256)
            for scene in ('source_both', 'panel_both')
            for condition in ('cold_idle', 'retired_idle', 'eight_held')
            for controls in ('static', 'moving')
            for policy in ('legacy_off', 'isolation_off', 'source_performance_off', 'legacy_masked', 'isolation_masked')]
for pair, m in enumerate(matrix):
    assert int(m['pair']) == pair
    assert (int(m['rate']), int(m['block']), m['scene'], m['condition'], m['controls'], m['policy']) == expected[int(m['case_id'])]
raw_path = root / 'callback-times.csv'
raw_file = raw_path.open() if raw_path.exists() else gzip.open(str(raw_path) + '.gz', 'rt')
groups = defaultdict(list)
with raw_file:
    for index, r in enumerate(csv.DictReader(raw_file)):
        pair, callback, arm, order = (int(r[k]) for k in ('pair', 'callback', 'arm', 'order'))
        assert index == pair * 1024 + callback * 2 + order
        assert arm == ((pair + callback + order) & 1)
        assert r['case_id'] == matrix[pair]['case_id'] and 0 <= callback < 512
        assert int(r['nanoseconds']) > 0
        groups[pair, arm].append(int(r['nanoseconds']))
assert sum(map(len, groups.values())) == 245760
verified = {}
for r in rows:
    pair = int(r['pair'])
    arm = 0 if r['arm'] == 'baseline_16735ee' else 1
    assert arm == 0 or r['arm'] == 'candidate_off_or_masked'
    assert (pair, arm) not in verified
    assert all(r[k] == matrix[pair][k] for k in ('case_id', 'rate', 'block', 'scene', 'condition', 'controls', 'policy'))
    samples = sorted(groups[pair, arm])
    assert len(samples) == int(r['callbacks']) == 512
    median = statistics.median(samples)
    p99 = samples[math.ceil(.99 * len(samples)) - 1]
    budget = 1e9 * int(r['block']) / int(r['rate'])
    assert float(r['median_ns']) == median and int(r['p99_ns']) == p99 and int(r['max_ns']) == samples[-1]
    assert math.isclose(float(r['mean_ns']), statistics.mean(samples), abs_tol=1e-7)
    assert int(r['overruns']) == sum(t > budget for t in samples)
    assert int(r['compared_float_values']) == 512 * int(r['block']) * 2
    verified[pair, arm] = r
pairs = []
for pair in range(240):
    base, candidate = verified[pair, 0], verified[pair, 1]
    assert base['audio_hash'] == candidate['audio_hash']
    assert base['peak'] == candidate['peak']
    assert base['pair_process_cpu_s_including_checks'] == candidate['pair_process_cpu_s_including_checks']
    assert base['pair_thread_cpu_s_including_checks'] == candidate['pair_thread_cpu_s_including_checks']
    b, c = float(base['median_ns']), float(candidate['median_ns'])
    pairs.append({**matrix[pair], 'baseline_median_ns': b, 'candidate_median_ns': c,
                  'ratio': c / b, 'delta_ns': c - b,
                  'material_overhead': c - b > max(.05 * b, 1000)})
flags = [p for p in pairs if p['material_overhead']]
summary = {
    'raw_statistics_verified': True, 'pairs': 240, 'observations': 245760,
    'material_flag_rule': 'delta_ns > max(.05*baseline_median_ns,1000)',
    'material_flags': len(flags), 'flagged_pairs': flags,
    'median_ratio_min': min(p['ratio'] for p in pairs),
    'median_ratio_max': max(p['ratio'] for p in pairs),
    'median_of_ratios': statistics.median(p['ratio'] for p in pairs),
    'baseline_overruns': sum(int(verified[p, 0]['overruns']) for p in range(240)),
    'candidate_overruns': sum(int(verified[p, 1]['overruns']) for p in range(240)),
    'baseline_max_rtf': max(int(verified[p, 0]['max_ns']) * int(matrix[p]['rate']) / (1e9 * int(matrix[p]['block'])) for p in range(240)),
    'candidate_max_rtf': max(int(verified[p, 1]['max_ns']) * int(matrix[p]['rate']) / (1e9 * int(matrix[p]['block'])) for p in range(240)),
    'no_DAW_or_WCET_qualification': True
}
(root / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
with (root / 'matched-pairs.csv').open('w') as output:
    writer = csv.DictWriter(output, fieldnames=list(pairs[0]))
    writer.writeheader(); writer.writerows(pairs)
print(json.dumps({k: v for k, v in summary.items() if k != 'flagged_pairs'}, indent=2))
