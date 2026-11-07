#!/usr/bin/env python3
"""Reproduce JUCE-free pitch memo evidence; output directory must be new."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import statistics
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline', required=True, type=Path, help='Clean baseline checkout')
p.add_argument('--candidate', required=True, type=Path, help='Candidate checkout')
p.add_argument('--output', required=True, type=Path)
p.add_argument('--cpu', required=True, type=int)
p.add_argument('--trials', default=7, type=int)
a = p.parse_args()
if a.trials < 1 or a.cpu not in os.sched_getaffinity(0):
    p.error('Require positive trials and a CPU allowed by process affinity')
a.output = a.output.resolve()
a.output.mkdir(parents=True, exist_ok=False)
here = Path(__file__).resolve().parent
checkouts = {'baseline': a.baseline.resolve(), 'candidate': a.candidate.resolve()}
flags = ['-O3', '-DNDEBUG', '-std=gnu++20', '-fPIC']
metadata = dict(cpu=a.cpu, trials=a.trials, flags=flags, compiler=subprocess.check_output(['g++', '--version'], text=True).splitlines()[0], sources={})
for label, root in checkouts.items():
    metadata['sources'][label] = {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in ['src/core/Dsp.h', 'src/core/Engine.h', 'src/core/Engine.cpp', 'src/core/FactoryPresets.cpp']}
# Complete compilation before timing either variant.
for kind in ['rate', 'modulation']:
    for label, root in checkouts.items():
        subprocess.run(['g++', *flags, '-I' + str(root / 'src'), str(here / (kind + '-probe.cpp')), str(root / 'src/core/Engine.cpp'), str(root / 'src/core/FactoryPresets.cpp'), '-o', str(a.output / (kind + '-' + label))], check=True)
(a.output / 'provenance.json').write_text(json.dumps(metadata, indent=2) + '\n')
for kind, scenarios in [('rate', list(range(8))), ('modulation', [3, 5])]:
    witnesses = []
    for label in checkouts:
        file = a.output / (kind + '-' + label + '-witness.bin')
        subprocess.run([str(a.output / (kind + '-' + label)), 'witness', str(file)], check=True)
        witnesses.append(file.read_bytes())
    if witnesses[0] != witnesses[1]:
        raise RuntimeError(kind + ' byte witness differs')
    print(kind, 'witness SHA-256', hashlib.sha256(witnesses[0]).hexdigest(), flush=True)
    rows, hashes = [], {}
    for trial in range(a.trials):
        order = scenarios.copy()
        random.Random(6123 + trial).shuffle(order)
        for scenario in order:
            labels = list(checkouts)
            random.Random(55 + trial * 8 + scenario).shuffle(labels)
            for pos, label in enumerate(labels + labels[::-1]):
                result = subprocess.run(['taskset', '-c', str(a.cpu), str(a.output / (kind + '-' + label)), 'bench', str(scenario)], text=True, capture_output=True, check=True)
                r = json.loads(result.stdout)
                if not r['valid'] or r['cpu'] <= 0 or r['hash'] != hashes.setdefault(scenario, r['hash']):
                    raise RuntimeError('Invalid result or audio/state hash mismatch')
                r.update(trial=trial, scenario=scenario, revision=label, position=pos)
                rows.append(r)
                with (a.output / (kind + '-observations.jsonl')).open('a') as f:
                    f.write(json.dumps(r) + '\n')
        print(kind, 'round', trial + 1, flush=True)
    summary = []
    for scenario in scenarios:
        sub = [r for r in rows if r['scenario'] == scenario]
        means = [{label: statistics.mean(r['cpu'] for r in sub if r['revision'] == label and r['trial'] == trial) for label in checkouts} for trial in range(a.trials)]
        savings = [100 * (1 - m['candidate'] / m['baseline']) for m in means]
        summary.append(dict(name=sub[0]['name'], paired_savings=savings, median_savings=statistics.median(savings), baseline_median=statistics.median(m['baseline'] for m in means), candidate_median=statistics.median(m['candidate'] for m in means), hash=hashes[scenario], max_wall_cpu=max(r['wall'] / r['cpu'] for r in sub)))
    (a.output / (kind + '-summary.json')).write_text(json.dumps(summary, indent=2) + '\n')
