#!/usr/bin/env python3
"""Read-only replay of retained FX PCM evidence; never loads or runs native code."""
import argparse
import array
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

SUITES = {
    'classic-mix': ('fx-mix', 'fx-mix-automation', 4, 3, False),
    'classic-tail': ('fx-tail', 'fx-tail-automation', 3, 2, True),
    'modern-mix': ('fx-modern-mix', 'fx-modern-mix-automation', 4, 3, False),
    'modern-tail': ('fx-modern-tail', 'fx-modern-tail-automation', 4, 3, True),
    'matched-mix': ('fx-matched-mix', 'fx-matched-mix-automation', 4, 3, False),
    'source-mix': ('fx-source-mix', 'fx-source-mix-automation', 4, 3, False),
}
BLOCKS = (7, 64, 256, 1024)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def relative_path(name):
    p = PurePosixPath(name)
    require(not p.is_absolute() and '..' not in p.parts and p.parts, 'unsafe relative path')
    return p


def source_bytes(checkout, ref, name):
    p = relative_path(name)
    if ref:
        return subprocess.check_output(['git', 'show', ref + ':' + str(p)], cwd=checkout)
    path = (checkout / str(p)).resolve()
    require(path.is_relative_to(checkout), 'source escapes checkout')
    return path.read_bytes()


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def delta(a, b):
    require(len(a) == len(b), 'unequal PCM windows')
    return max(abs(x - y) for x, y in zip(a, b))


def audit(args):
    checkout = args.checkout.resolve()
    root = args.capture_root.resolve() if args.capture_root else checkout
    tag, folder, scenarios, lanes, tail = SUITES[args.suite]
    classic = args.suite == 'classic-mix'
    matched = args.suite == 'matched-mix'
    source_mix = args.suite == 'source-mix'
    if args.source_ref:
        require(re.fullmatch(r'[0-9a-fA-F]{40}', args.source_ref) is not None,
                'source-ref must be a full local commit SHA')
    # Evidence is read from the chosen commit too, preventing accidental mixing
    # of current manifests with old source identities.
    def read_json(name):
        return json.loads(source_bytes(checkout, args.source_ref, name))
    evidence = 'docs/experiments/' + folder + '/collect/'
    complete = read_json(evidence + 'complete.json')
    capture_raw = source_bytes(checkout, args.source_ref, evidence + 'captures.json')
    captures = json.loads(capture_raw)
    inventory_hashes = [value for path, value in complete.get(
        'evidence_hashes' if classic else 'hashes', {}).items()
        if path.endswith('/collect/captures.json')]
    require(len(inventory_hashes) == 1 and inventory_hashes[0] == digest(capture_raw),
            'collector/capture inventory hash mismatch')
    freeze_raw = source_bytes(checkout, args.source_ref, 'tests/' + tag + '-source-freeze.json')
    freeze = json.loads(freeze_raw)
    marker_key = 'source_freeze_sha256' if classic else 'freeze_sha256'
    require(complete[marker_key] == digest(freeze_raw), 'collector/source freeze mismatch')
    sources = freeze if classic else freeze['sources']
    for path, expected in sources.items():
        require(digest(source_bytes(checkout, args.source_ref, path)) == expected,
                'frozen source differs: ' + path)
    host = read_json('tests/parity/tolerances-v2/host-automation.json')['host_automation']
    contract = read_json('tests/parity/tolerances-v2/host-automation-' + tag + '-v1.json')
    limits = contract['host_automation_' + tag.replace('-', '_')]
    require(limits['key_parameter_count'] == 115, 'unexpected capability contract')
    builds = {'build-native', 'build-fx-mix-helpers'} if classic else {'build-' + tag}
    indexed = {}
    byte_count = (65536 if tail else 16384) * 2 * 4
    # Resolve only the known build-relative suffix, allowing evidence to move to
    # another machine without trusting or opening recorded absolute paths.
    for recorded, expected in captures.items():
        parts = PurePosixPath(recorded).parts
        positions = [i for i, part in enumerate(parts) if part in builds]
        require(len(positions) == 1, 'capture lacks one known build root')
        relative = relative_path('/'.join(parts[positions[0]:]))
        require(relative.suffix == '.pcm', 'capture must be PCM')
        path = (root / str(relative)).resolve()
        require(path.is_relative_to(root), 'capture escapes capture root')
        require(str(relative) not in indexed, 'duplicate relocated capture')
        raw = path.read_bytes()
        require(len(raw) == byte_count and digest(raw) == expected,
                'capture size/hash mismatch: ' + str(relative))
        values = array.array('f')
        require(values.itemsize == 4, 'platform lacks 32-bit float array')
        values.frombytes(raw)
        if sys.byteorder != 'little':
            values.byteswap()
        require(all(map(math.isfinite, values)), 'non-finite PCM')
        indexed[str(relative)] = (raw, values)
    rates = ('48000',) if tail else ('44100', '48000', '96000')
    expected_names = set()
    metrics = []
    order_checks = 0
    pairs = 0
    for rate in rates:
        for mode in ('realtime', 'offline'):
            config = rate + '-' + mode
            directory = ('build-native/fx-mix-candidate-' + config if classic else
                         'build-' + tag + '/' + (mode if tail else config))
            layouts = (0, 1) if source_mix else (None,)
            for layout, scenario in ((layout, scenario) for layout in layouts
                                     for scenario in range(scenarios)):
                prefix = 'source' + str(layout) + '-' if source_mix else ''
                suffixes = ['noqueue', 'null', 'preceding112', 'legacy', 'legacy-flat',
                            'pure', 'pure-fixed', 'scalar', 'constant']
                suffixes += ['block' + str(x) for x in BLOCKS]
                if tail:
                    suffixes += ['dry']
                if matched:
                    suffixes += ['outer-match-off']
                if source_mix:
                    suffixes += ['source-manual']
                combined = scenario == scenarios - 1
                if combined:
                    suffixes += ['reverse' + str(x) for x in BLOCKS]
                    suffixes += ['held-lane' + str(i) for i in range(lanes)]
                def capture(suffix):
                    key = directory + '/' + prefix + 'fx-case' + str(scenario) + '-' + suffix + '.pcm'
                    expected_names.add(key)
                    require(key in indexed, 'missing capture: ' + key)
                    return indexed[key]
                data = {suffix: capture(suffix) for suffix in suffixes}
                for x, y in [('noqueue', 'null'), ('legacy', 'legacy-flat'), ('pure', 'pure-fixed')]:
                    require(data[x][0] == data[y][0], 'policy/control identity failed')
                scalar, fixed = data['scalar'][1], data['constant'][1]
                require(data['scalar'][0][:8192] == data['constant'][0][:8192], 'initial hold differs')
                error = max(delta(scalar, data['block' + str(x)][1]) for x in BLOCKS)
                require(error <= host['audio_max_abs_error'], 'scalar comparison exceeds contract')
                effect = delta(scalar[2048:28672], fixed[2048:28672])
                require(effect > limits['minimum_held_effect'], 'overall witness fails')
                per_lane = []
                if combined:
                    for block in BLOCKS:
                        require(data['block' + str(block)][0] == data['reverse' + str(block)][0],
                                'queue-order comparison differs')
                        order_checks += 1
                    per_lane = [delta(scalar[2048:28672], data['held-lane' + str(i)][1][2048:28672])
                                for i in range(lanes)]
                    require(min(per_lane) > limits['minimum_held_effect'], 'per-lane witness fails')
                row = dict(max_error=error, held_effect=effect, per_lane_effects=per_lane)
                if matched:
                    match_effect = delta(scalar[2048:28672], data['outer-match-off'][1][2048:28672])
                    require(match_effect > limits['minimum_outer_match_effect'], 'outer matching witness fails')
                    row['outer_match_effect'] = match_effect
                if source_mix:
                    source_effect = delta(scalar[2048:28672], data['source-manual'][1][2048:28672])
                    require(source_effect > limits['minimum_source_layout_effect'], 'source layout witness fails')
                    row.update(layout=layout, source_layout_effect=source_effect)
                if classic:
                    row.update(config='run-' + config, scenario=scenario)
                    for suffix in ('noqueue', 'null', 'preceding112', 'legacy', 'legacy-flat', 'pure', 'pure-fixed'):
                        key = ('build-fx-mix-helpers/fx-mix-baseline-' + config +
                               '/fx-case' + str(scenario) + '-' + suffix + '.pcm')
                        expected_names.add(key)
                        require(key in indexed and indexed[key][0] == data[suffix][0], 'actual baseline pair differs')
                        pairs += 1
                elif tail:
                    dry = data['dry'][1]
                    dry_tail = max(map(abs, dry[32768:]))
                    tail_effect = delta(scalar[32768:], dry[32768:])
                    motion = delta(scalar[32768:], fixed[32768:])
                    require(dry_tail <= limits['maximum_dry_tail'] and
                            tail_effect > limits['minimum_tail_effect'] and
                            motion > limits['minimum_tail_motion'], 'tail control fails')
                    row.update(mode=mode, scenario=scenario, dry_tail=dry_tail,
                               tail=tail_effect, tail_motion=motion)
                else:
                    row.update(rate=rate, mode=mode, scenario=scenario)
                metrics.append(row)
    require(set(indexed) == expected_names, 'unexpected or missing capture inventory')
    recorded_metrics = complete['recomputed_metrics' if classic else 'metrics']
    # Collection order differs for the historical classic mix gate.
    def sort_key(row):
        return (row.get('config', ''), row.get('rate', ''), row.get('mode', ''), row.get('layout', -1), row['scenario'])
    require(sorted(metrics, key=sort_key) == sorted(recorded_metrics, key=sort_key),
            'independent metrics differ from collector')
    require(complete['captures'] == len(indexed), 'collector capture count differs')
    require(complete['scalar_block_comparisons' if classic else 'scalar_comparisons'] == len(metrics) * 4,
            'collector scalar count differs')
    require(complete['queue_order_comparisons' if classic else 'queue_order_checks'] == order_checks,
            'collector order count differs')
    require(complete['initial_holds'] == len(metrics), 'collector hold count differs')
    if classic:
        require(complete['actual_pcm_pairs'] == pairs, 'collector pair count differs')
    if matched:
        require(complete['outer_match_witnesses'] == len(metrics), 'collector matching count differs')
    if source_mix:
        require(complete['source_layout_witnesses'] == len(metrics), 'collector source witness count differs')
    result = dict(suite=args.suite, result='PASS', source_files_verified=len(sources),
                retained_finite_pcm=len(indexed), scalar_comparisons=len(metrics) * 4,
                maximum_scalar_error=max(row['max_error'] for row in metrics),
                exact_queue_order_checks=order_checks, exact_initial_holds=len(metrics),
                actual_compatibility_pairs=pairs, native_code_executed=False,
                scope='Retained source/PCM recomputation only; not a native rerun or full release qualification.')
    if matched:
        result['outer_matching_witnesses'] = len(metrics)
        result['minimum_outer_matching_effect'] = min(row['outer_match_effect'] for row in metrics)
    if source_mix:
        result['source_layout_witnesses'] = len(metrics)
        result['minimum_source_layout_effect'] = min(row['source_layout_effect'] for row in metrics)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkout', type=Path, required=True, help='Local Git checkout holding evidence')
    parser.add_argument('--suite', choices=SUITES, required=True)
    parser.add_argument('--source-ref', help='Full local commit SHA for frozen source and evidence; no fetch is performed')
    parser.add_argument('--capture-root', type=Path, help='Directory containing original build-relative capture folders')
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args), indent=2))
        return 0
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print('Evidence audit failed: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
