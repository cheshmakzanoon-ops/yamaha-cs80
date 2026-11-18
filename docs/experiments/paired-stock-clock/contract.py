#!/usr/bin/env python3
"""Clock-free, process-free fixed contract for the stock-only paired-clock gate."""
import hashlib
import json
import math
from pathlib import Path

VERSION = 'stock-paired-clock-v1'
STOCK_ARCHIVE_SHA256 = '323db1f6f973508b9192f310f4bef66efd45e6c6178afc164dd5819b72b5e314'
SCENES = {'factory-held': 1, 'panel-moving': 2}
CELLS = [('factory-held', 16), ('panel-moving', 32), ('factory-held', 32), ('panel-moving', 16)]
CONTROL_ROUNDS = ['sham0', 'A-extra', 'sham1', 'B-extra', 'sham2']
CLOCK_KEYS = ['thread_begin_ns', 'wall_begin_ns', 'wall_end_ns', 'thread_end_ns']
FP_KEYS = {'mxcsr', 'cw', 'sw', 'error'}
REPORT_KEYS = {'abi', 'scene', 'main_next', 'shadow_next', 'main_calls', 'shadow_calls', 'extra_calls', 'frame_hash', 'last_pcm', 'shadow_pcm', 'main_fp', 'shadow_fp', 'active_voices', 'block_frames', 'internal_factor'}
ROW_KEYS = {'sequence', 'kind', 'phase', 'logical_chunk', 'side', 'first_block', 'count', 'extra_enabled', 'before', 'after', 'cpu_before', 'cpu_after', 'affinity_before', 'affinity_after', 'caller_before', 'caller_after', 'namespace_errno_before', 'namespace_errno_after', 'result', 'timed', 'clocks'}
LIMITS = {
    'pairs': 20, 'process_cpu_ns': 15_000_000_000,
    'process_wall_ns': 20_000_000_000, 'trial_cpu_ns': 160_000_000_000,
    'trial_wall_ns': 240_000_000_000, 'warm_callbacks': 1024,
    'measured_callbacks': 512, 'continuation_callbacks': 1,
    'extra_callbacks': 32, 'sham_abs_effect_pct': 1.5,
    'cell_sham_abs_effect_pct': 0.75, 'cell_sham_range_pp': 2.0,
    'sham_cycle_p90_pct': 4.0, 'sham_cycle_max_pct': 10.0,
    'positive_ratio_min': 1.03, 'positive_ratio_max': 1.10,
    'positive_direction_log_gap_max': 0.02,
    'overhead_max_ratio_strict': 0.01, 'overhead_mean_ratio_strict': 0.0025,
    'cpu': 0, 'affinity_mask': '1',
}
SCOPE = {
    'metric': 'thread CPU', 'wall': 'complete descriptive records, not an admission or realtime metric',
    'scenes': ['factory-held', 'panel-moving'], 'chunks': [16, 32],
    'arms': ['stock-A', 'stock-B'], 'candidate_admission': False,
    'native_or_DAW_admission': False, 'other_scene_admission': False,
    'old_216_slot_decision_changed': False,
}


def reject_duplicates(pairs):
    out = {}
    for k, v in pairs:
        if k in out:
            raise ValueError('duplicate JSON key: ' + k)
        out[k] = v
    return out


def read_json(path):
    return json.loads(Path(path).read_text(), object_pairs_hook=reject_duplicates,
                      parse_constant=lambda s: (_ for _ in ()).throw(ValueError('nonfinite JSON: ' + s)))


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False).encode()


def digest(value):
    return hashlib.sha256(canonical(value)).hexdigest()


def file_sha256(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda: f.read(65536), b''):
            h.update(b)
    return h.hexdigest()


def dump_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + '\n')


def schedule():
    result = []
    for rnd, role in enumerate(CONTROL_ROUNDS):
        for position in range(4):
            base_cell = (rnd + position) % 4
            scene, chunk = CELLS[base_cell]
            slot = len(result)
            result.append({'slot': slot, 'round': rnd, 'position': position,
                           'scene': scene, 'chunk': chunk, 'role': role,
                           'control': 'sham' if role.startswith('sham') else role,
                           'orientation': 'AB' if (base_cell + rnd // 2) % 2 == 0 else 'BA',
                           'attempt': 1})
    assert len(result) == 20
    assert sum(s['orientation'] == 'AB' for s in result) == 10
    for scene, chunk in CELLS:
        cell = [s for s in result if (s['scene'], s['chunk']) == (scene, chunk)]
        assert {s['orientation'] for s in cell if s['control'] == 'sham'} == {'AB', 'BA'}
        assert {s['orientation'] for s in cell if s['control'] != 'sham'} == {'AB', 'BA'}
        assert len(cell) == 5
    return result


def expected_rows(slot):
    """Complete emitted order, not a sorting key. No phase may be omitted."""
    rows = []
    main = {'A': 0, 'B': 0}
    shadow = {'A': 0, 'B': 0}
    extra = {'A': 0, 'B': 0}

    def add(kind, phase, side=None, logical=None, count=0):
        enabled = kind == 'span' and phase == 'measured' and slot['control'] == str(side) + '-extra'
        before = None if side is None else {'main_next': main[side], 'shadow_next': shadow[side], 'extra_calls': extra[side]}
        first = main[side] if kind == 'span' else None
        if kind == 'span':
            main[side] += count
            shadow[side] += count if phase == 'warm' else count // 16 if enabled else 0
            extra[side] += count // 16 if enabled else 0
        after = None if side is None else {'main_next': main[side], 'shadow_next': shadow[side], 'extra_calls': extra[side]}
        timed = phase == 'measured' or kind != 'span'
        rows.append({'sequence': len(rows), 'kind': kind, 'phase': phase,
                     'logical_chunk': logical, 'side': side, 'first_block': first,
                     'count': count, 'extra_enabled': enabled, 'timed': timed,
                     '_before_counts': before, '_after_counts': after})

    def probes(phase):
        for _ in range(4):
            add('empty-clock', phase)
        for _ in range(4):
            for side in ('A', 'B'):
                add('zero-boundary', phase, side)

    probes('pre')
    for phase, callbacks in [('warm', 1024), ('measured', 512)]:
        for j in range(callbacks // slot['chunk']):
            order = 'AB' if (j % 2 == 0) == (slot['orientation'] == 'AB') else 'BA'
            for side in order:
                add('span', phase, side, j, slot['chunk'])
    for side in 'AB':
        add('span', 'continuation', side, 0, 1)
    probes('post')
    return rows


def nearest_rank(values, p=0.9):
    if not values or not 0 < p <= 1:
        raise ValueError('nearest rank needs nonempty values and 0 < p <= 1')
    return sorted(values)[math.ceil(p * len(values)) - 1]


def signed_pct(log_ratio):
    return 100.0 * math.expm1(log_ratio)


def magnitude_pct(log_ratio):
    return 100.0 * math.expm1(abs(log_ratio))
