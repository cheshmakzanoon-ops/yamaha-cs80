#!/usr/bin/env python3
"""W0 comparison plumbing. Synthetic results are NOT analog fidelity evidence."""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TOLERANCES = ROOT / 'tests/parity/tolerances-v1.json'
STAGES = ('oscillator', 'filter', 'envelope', 'output')
METRICS = ('rms_error', 'max_abs_error', 'abs_bias_error')


class InvalidEvidence(ValueError):
    """An invalid input must never be reported as a passing comparison."""


def require(condition, message):
    if not condition:
        raise InvalidEvidence(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def reject_constant(value):
    raise InvalidEvidence(f'Nonfinite JSON number: {value}')


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f'Duplicate JSON key: {key}')
        result[key] = value
    return result


def read_json(path):
    data = path.read_bytes()
    value = json.loads(data, parse_constant=reject_constant, object_pairs_hook=unique_object)
    finite_tree(value)
    return value, sha256(data)


def finite_tree(value):
    if isinstance(value, float):
        require(math.isfinite(value), 'Nonfinite numeric metadata')
    elif isinstance(value, dict):
        for child in value.values():
            finite_tree(child)
    elif isinstance(value, list):
        for child in value:
            finite_tree(child)


def text(value, label):
    require(isinstance(value, str) and bool(value.strip()), f'Missing/invalid {label}')


def integer(value, label, minimum=0):
    require(type(value) is int and value >= minimum, f'Invalid {label}')


def number(value, label, positive=False):
    require(type(value) in (int, float) and math.isfinite(value), f'Invalid {label}')
    if positive:
        require(value > 0, f'Nonpositive {label}')


def hash_field(value, label):
    require(isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value), f'Invalid {label} SHA-256')


def load_capture(root, descriptor):
    require(isinstance(descriptor, dict), 'Invalid capture descriptor')
    text(descriptor.get('path'), 'capture path')
    relative = Path(descriptor['path'])
    require(not relative.is_absolute() and '..' not in relative.parts, 'Capture path escapes corpus')
    path = (root / relative).resolve()
    require(path.is_relative_to(root.resolve()), 'Capture symlink escapes corpus')
    hash_field(descriptor.get('sha256'), 'capture')
    data = path.read_bytes()
    require(sha256(data) == descriptor['sha256'], f'Capture hash mismatch: {relative}')
    rows = csv.reader(io.StringIO(data.decode('utf-8'), newline=''))
    require(next(rows, None) == ['sample', 'value'], 'Expected CSV header sample,value')
    samples = []
    for index, row in enumerate(rows):
        require(len(row) == 2 and row[0] == str(index), 'Noncontiguous/invalid sample index')
        try:
            value = float(row[1])
        except ValueError as exc:
            raise InvalidEvidence('Invalid sample') from exc
        require(math.isfinite(value), 'Nonfinite sample')
        samples.append(value)
    require(bool(samples), 'Empty capture')
    integer(descriptor.get('sample_count'), 'sample_count', 1)
    require(len(samples) == descriptor['sample_count'], 'Capture sample count mismatch')
    return samples


def validate_manifest(manifest):
    require(isinstance(manifest, dict) and type(manifest.get('schema_version')) is int and manifest['schema_version'] == 1, 'Unsupported manifest')
    require(manifest.get('evidence_kind') in ('synthetic', 'hardware'), 'Invalid evidence_kind')
    p = manifest.get('provenance')
    require(isinstance(p, dict), 'Missing provenance')
    for key in ('instrument_identity', 'board_ic_revisions', 'rails', 'temperature', 'warm_up',
                'trims', 'probe', 'scaling', 'redistribution_status', 'source_description'):
        text(p.get(key), key)
    require(isinstance(p.get('source_revision'), str) and
            re.fullmatch('[0-9a-f]{40}|[0-9a-f]{64}', p['source_revision']), 'Invalid source revision')
    require(type(p.get('working_tree_dirty')) is bool, 'Missing working-tree state')
    hash_field(p.get('generator_sha256'), 'generator')
    require(isinstance(p.get('source_hashes'), dict) and bool(p['source_hashes']), 'Missing source hashes')
    for name, digest in p['source_hashes'].items():
        text(name, 'source name')
        hash_field(digest, 'source')
    stages = manifest.get('stages')
    require(isinstance(stages, list) and bool(stages), 'Missing stages')
    seen = set()
    for stage in stages:
        require(isinstance(stage, dict), 'Invalid stage')
        name = stage.get('stage')
        require(isinstance(name, str) and name not in seen, 'Invalid/duplicate stage')
        seen.add(name)
        integer(stage.get('sample_rate_hz'), 'sample rate', 1)
        integer(stage.get('latency_samples'), 'latency')
        integer(stage.get('comparison_samples'), 'comparison samples', 2)
        require(stage.get('split') in ('train', 'validation'), 'Missing train/validation split')
        require(isinstance(stage.get('settings'), dict) and bool(stage['settings']), 'Missing fixed settings')
        text(stage.get('source_row'), 'source row')
        text(stage.get('units'), 'units')
    return stages


def compare(manifest_path, tolerance_path=DEFAULT_TOLERANCES):
    manifest, manifest_hash = read_json(manifest_path)
    stages = validate_manifest(manifest)
    tolerance, tolerance_hash = read_json(tolerance_path)
    require(isinstance(tolerance, dict) and type(tolerance.get('schema_version')) is int and tolerance['schema_version'] == 1, 'Unsupported tolerances')
    text(tolerance.get('profile_id'), 'tolerance profile')
    text(tolerance.get('rationale'), 'tolerance rationale')
    # There are no calibrated captures or pre-registered analog tolerances yet.
    require(manifest['evidence_kind'] == tolerance.get('evidence_kind') == 'synthetic',
            'Only synthetic plumbing comparisons are enabled; hardware acceptance is not registered')
    limits = tolerance.get('stages')
    require(isinstance(limits, dict) and set(limits) == {s['stage'] for s in stages},
            'Stage set must exactly match tolerance profile')
    reports = []
    for stage in stages:
        name = stage['stage']
        limit = limits[name]
        require(isinstance(limit, dict) and set(limit) == set(METRICS), 'Incomplete/unknown metric limits')
        for metric, threshold in limit.items():
            number(threshold, metric)
            require(threshold >= 0, 'Negative tolerance')
        reference = load_capture(manifest_path.parent, stage.get('reference'))
        candidate = load_capture(manifest_path.parent, stage.get('candidate'))
        n, delay = stage['comparison_samples'], stage['latency_samples']
        require(len(reference) == n and len(candidate) == n + delay,
                'Capture lengths must cover the complete registered window plus latency')
        residual = [candidate[i + delay] - reference[i] for i in range(n)]
        raw = [candidate[i] - reference[i] for i in range(n)]
        metrics = {'rms_error': math.hypot(*residual) / math.sqrt(n),
                   'max_abs_error': max(abs(x) for x in residual),
                   'abs_bias_error': abs(math.fsum(x / n for x in residual))}
        raw_rms = math.hypot(*raw) / math.sqrt(n)
        require(all(math.isfinite(x) for x in [*metrics.values(), raw_rms]), 'Nonfinite derived metric')
        reports.append({'stage': name, 'units': stage['units'], 'split': stage['split'],
                        'settings': stage['settings'], 'source_row': stage['source_row'],
                        'sample_rate_hz': stage['sample_rate_hz'],
                        'alignment': {'method': 'declared_integer_latency', 'latency_samples': delay,
                                      'reference_start': 0, 'candidate_start': delay, 'samples': n},
                        'unaligned_rms_error': raw_rms, 'metrics': metrics, 'limits': limit,
                        'passed': all(metrics[k] <= limit[k] for k in METRICS)})
    return {'schema_version': 1, 'evidence_kind': 'synthetic', 'analog_evidence': False,
            'claim': 'Synthetic plumbing check only; not a CS-80 model or hardware comparison.',
            'manifest_sha256': manifest_hash, 'tolerances_sha256': tolerance_hash,
            'tolerance_profile': tolerance['profile_id'], 'provenance': manifest['provenance'],
            'comparator_sha256': sha256(Path(__file__).read_bytes()),
            'passed': all(r['passed'] for r in reports), 'stages': reports}


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + '\n', encoding='utf-8')


def synthetic(directory):
    """Deterministic stand-in traces, deliberately unrelated to CS-80 circuit laws."""
    directory.mkdir(parents=True, exist_ok=True)
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip())
    digest = sha256(Path(__file__).read_bytes())
    provenance = {k: 'not applicable: synthetic mathematical fixture' for k in
                  ('instrument_identity', 'board_ic_revisions', 'rails', 'temperature', 'warm_up',
                   'trims', 'probe')}
    provenance.update(scaling='1 CSV unit = 1 dimensionless synthetic unit',
                      redistribution_status='Repository-authored synthetic data; repository license',
                      source_description='Fixed analytic sine, one-pole recurrence, linear envelope, sum',
                      source_revision=revision, working_tree_dirty=dirty, generator_sha256=digest,
                      source_hashes={'scripts/parity_harness.py': digest})
    n, rate, delay = 4096, 48000, 63
    osc = [0.25 * math.sin(2 * math.pi * 750 * i / rate) for i in range(n)]
    filtered, state = [], 0.0
    for value in osc:
        state += 0.125 * (value - state)
        filtered.append(state)
    envelope = [min(i / 512, 1.0) * max(0.0, min((n - 1 - i) / 1024, 1.0)) for i in range(n)]
    traces = (osc, filtered, envelope, [x * y for x, y in zip(filtered, envelope)])
    manifest = {'schema_version': 1, 'evidence_kind': 'synthetic', 'provenance': provenance, 'stages': []}
    for name, reference in zip(STAGES, traces):
        # A known sub-tolerance perturbation checks residual reporting, not just identity.
        candidate = [0.0] * delay + [x + (1e-10 if i % 2 else -1e-10) for i, x in enumerate(reference)]
        descriptors = []
        for role, values in (('reference', reference), ('candidate', candidate)):
            path = directory / f'{name}-{role}.csv'
            with path.open('w', newline='', encoding='utf-8') as stream:
                writer = csv.writer(stream, lineterminator='\n')
                writer.writerow(['sample', 'value'])
                writer.writerows((i, format(value, '.17g')) for i, value in enumerate(values))
            descriptors.append({'path': path.name, 'sha256': sha256(path.read_bytes()), 'sample_count': len(values)})
        manifest['stages'].append({'stage': name, 'sample_rate_hz': rate, 'latency_samples': delay,
                                  'comparison_samples': n, 'split': 'validation', 'units': 'dimensionless',
                                  'settings': {'frequency_hz': 750, 'amplitude': 0.25, 'pole_step': 0.125,
                                               'attack_samples': 512, 'release_samples': 1024},
                                  'source_row': f'synthetic-v1/{name}; no hardware source',
                                  'reference': descriptors[0], 'candidate': descriptors[1]})
    write_json(directory / 'manifest.json', manifest)
    return directory / 'manifest.json'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    generate = commands.add_parser('synthetic', help='Generate and compare synthetic stand-ins (no engine/hardware)')
    generate.add_argument('--out', type=Path, required=True)
    run = commands.add_parser('compare')
    run.add_argument('--manifest', type=Path, required=True)
    run.add_argument('--report', type=Path, required=True)
    for command in (generate, run):
        command.add_argument('--tolerances', type=Path, default=DEFAULT_TOLERANCES)
    args = parser.parse_args()
    report_path = args.out / 'report.json' if args.command == 'synthetic' else args.report
    try:
        manifest = synthetic(args.out) if args.command == 'synthetic' else args.manifest
        report = compare(manifest, args.tolerances)
        write_json(report_path, report)
    except (InvalidEvidence, OSError, ValueError, OverflowError, subprocess.CalledProcessError) as exc:
        # Replace stale success reports when an input becomes invalid.
        try:
            write_json(report_path, {'passed': False, 'valid': False, 'analog_evidence': False, 'error': str(exc)})
        except OSError:
            pass
        print(f'Invalid comparison: {exc}', file=sys.stderr)
        return 2
    print(f"Synthetic plumbing {'PASS' if report['passed'] else 'FAIL'}: {report_path}; no analog evidence")
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
