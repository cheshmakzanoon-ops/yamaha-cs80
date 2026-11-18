#!/usr/bin/env python3
"""Offline only: validate all fixed rows and compute every paired-clock effect.

This module contains no DSP, loader, subprocess, clock, affinity, or timing API.
A valid numerical result cannot make an invalid/missing observation admissible.
"""
import math
import statistics
from decimal import Decimal, localcontext
from fractions import Fraction
from contract import (VERSION, STOCK_ARCHIVE_SHA256, SCENES, CELLS, LIMITS, SCOPE,
                      FP_KEYS, REPORT_KEYS, ROW_KEYS, CLOCK_KEYS, canonical,
                      expected_rows, schedule, nearest_rank, signed_pct, magnitude_pct)


def exact(a, b):
    return canonical(a) == canonical(b)


def integer(x, lo=0, hi=(1 << 63) - 1):
    return type(x) is int and lo <= x <= hi


def sha(x):
    return type(x) is str and len(x) == 64 and all(c in '0123456789abcdef' for c in x)


def hex64(x):
    return type(x) is str and len(x) == 16 and all(c in '0123456789abcdef' for c in x)


class Checks:
    def __init__(self):
        self.failures = []
        self.checks = []

    def need(self, okay, code, detail=None):
        item = {'code': code, 'pass': bool(okay)}
        if detail is not None:
            item['detail'] = detail
        self.checks.append(item)
        if not okay:
            self.failures.append(item)
        return bool(okay)

    def extend(self, other):
        self.failures.extend(other.failures)
        self.checks.extend(other.checks)


def validate_fp(value, c, tag):
    if not c.need(type(value) is dict and set(value) == FP_KEYS, tag + '.shape'):
        return
    for k, high in [('mxcsr', 0xffffffff), ('cw', 0xffff), ('sw', 0xffff)]:
        c.need(integer(value[k], 0, high), tag + '.' + k)
    c.need(integer(value['error'], -(1 << 31), (1 << 31) - 1), tag + '.error')


def expected_report(scene, counts, gold):
    main = gold['endpoints'][str(counts['main_next'])]
    shadow = gold['endpoints'][str(counts['shadow_next'])]
    return {'abi': 0x50435131, 'scene': SCENES[scene],
            'main_next': counts['main_next'], 'shadow_next': counts['shadow_next'],
            'main_calls': counts['main_next'], 'shadow_calls': counts['shadow_next'],
            'extra_calls': counts['extra_calls'], 'frame_hash': gold['frame_hash'],
            'last_pcm': main['pcm'], 'shadow_pcm': shadow['pcm'],
            'main_fp': main['fp'], 'shadow_fp': shadow['fp'],
            'active_voices': 8, 'block_frames': 64, 'internal_factor': 2}


def validate_report(value, counts, slot, gold, c, tag):
    if counts is None:
        c.need(value is None, tag + '.empty-report-null')
        return
    if not c.need(type(value) is dict and set(value) == REPORT_KEYS, tag + '.shape'):
        return
    for k in REPORT_KEYS - {'frame_hash', 'last_pcm', 'shadow_pcm', 'main_fp', 'shadow_fp'}:
        c.need(integer(value[k]), tag + '.' + k + '.integer')
    for k in ('frame_hash', 'last_pcm', 'shadow_pcm'):
        c.need(hex64(value[k]), tag + '.' + k + '.hex')
    validate_fp(value['main_fp'], c, tag + '.main-fp')
    validate_fp(value['shadow_fp'], c, tag + '.shadow-fp')
    try:
        expected = expected_report(slot['scene'], counts, gold)
    except (KeyError, TypeError):
        c.need(False, tag + '.missing-frozen-endpoint')
        return
    c.need(exact(value, expected), tag + '.full-frozen-report',
           None if exact(value, expected) else {'expected': expected, 'observed': value})


def validate_rows(rows, slot, gold, caller_fp, timed_mode=True):
    c = Checks()
    expected = expected_rows(slot)
    if not timed_mode:
        for row in expected:
            row['timed'] = False
    if not c.need(type(rows) is list, 'rows.list'):
        return c
    c.need(len(rows) == len(expected), 'rows.exact-count', {'expected': len(expected), 'observed': len(rows)})
    previous = {'A': None, 'B': None}
    last_clocks = None
    timed_count = 0
    for index, row in enumerate(rows):
        tag = 'row.' + str(index)
        if not c.need(type(row) is dict and set(row) == ROW_KEYS, tag + '.shape'):
            continue
        if index >= len(expected):
            c.need(False, tag + '.extra-row')
            continue
        exp = expected[index]
        for key, value in exp.items():
            if not key.startswith('_'):
                c.need(exact(row[key], value), tag + '.order-role.' + key)
        for key in ('cpu_before', 'cpu_after'):
            c.need(type(row[key]) is int and row[key] == 0, tag + '.' + key)
        for key in ('affinity_before', 'affinity_after'):
            c.need(row[key] == '1', tag + '.' + key)
        c.need(type(row['result']) is int and row['result'] == 0, tag + '.result')
        for key in ('namespace_errno_before', 'namespace_errno_after'):
            c.need(exact(row[key], [41, 73]), tag + '.' + key)
        for key in ('caller_before', 'caller_after'):
            validate_fp(row[key], c, tag + '.' + key)
            c.need(exact(row[key], caller_fp), tag + '.' + key + '.frozen-poison')
        validate_report(row['before'], exp['_before_counts'], slot, gold, c, tag + '.before')
        validate_report(row['after'], exp['_after_counts'], slot, gold, c, tag + '.after')
        side = exp['side']
        if side is not None:
            if previous[side] is not None:
                c.need(exact(row['before'], previous[side]), tag + '.owner-continuity')
            previous[side] = row['after']
        if exp['kind'] == 'zero-boundary':
            c.need(exact(row['before'], row['after']), tag + '.zero-boundary-no-work')
        clocks = row['clocks']
        if not exp['timed']:
            c.need(clocks is None, tag + '.warm-continuation-unclocked')
            continue
        timed_count += 1
        if not c.need(type(clocks) is dict and set(clocks) == set(CLOCK_KEYS), tag + '.clock-shape'):
            continue
        if not c.need(all(integer(clocks[k], 1) for k in CLOCK_KEYS), tag + '.clock-integer-positive'):
            continue
        c.need(clocks['thread_end_ns'] > clocks['thread_begin_ns'], tag + '.thread-positive')
        c.need(clocks['wall_end_ns'] > clocks['wall_begin_ns'], tag + '.wall-positive')
        if last_clocks is not None:
            c.need(clocks['thread_begin_ns'] >= last_clocks['thread_end_ns'], tag + '.thread-temporal-order')
            c.need(clocks['wall_begin_ns'] >= last_clocks['wall_end_ns'], tag + '.wall-temporal-order')
        last_clocks = clocks
    c.need(timed_count == sum(e['timed'] for e in expected), 'rows.all-timed-observations')
    return c


def clock_ns(row, metric):
    key = 'thread' if metric == 'cpu' else 'wall'
    return row['clocks'][key + '_end_ns'] - row['clocks'][key + '_begin_ns']


def fraction_record(value, exponent):
    return {'numerator': str(value.numerator), 'denominator': str(value.denominator), 'root': exponent}


def fraction_value(record):
    return Fraction(int(record['numerator']), int(record['denominator']))


def ratio_within(record, lower, upper):
    value, root = fraction_value(record), record['root']
    return Fraction(str(lower)) ** root <= value <= Fraction(str(upper)) ** root


def magnitude_within(record, percent):
    value, root = fraction_value(record), record['root']
    ceiling = Fraction(1) + Fraction(str(percent)) / 100
    return ceiling ** -root <= value <= ceiling ** root


def decimal_log(record):
    with localcontext() as ctx:
        ctx.prec = 80
        value = (Decimal(record['numerator']) / Decimal(record['denominator'])).ln() / record['root']
        return +value


def decimal_ratio(record):
    with localcontext() as ctx:
        ctx.prec = 80
        return +decimal_log(record).exp()


def metric_effects(rows, slot, metric):
    measured = [r for r in rows if r['kind'] == 'span' and r['phase'] == 'measured']
    chunks = []
    for logical in range(512 // slot['chunk']):
        pair = {r['side']: r for r in measured if r['logical_chunk'] == logical}
        a, b = clock_ns(pair['A'], metric), clock_ns(pair['B'], metric)
        log_ratio = math.log(b / a)
        chunks.append({'logical_chunk': logical, 'first_block': 1024 + logical * slot['chunk'],
                       'A_ns': a, 'B_ns': b, 'A_sequence': pair['A']['sequence'],
                       'B_sequence': pair['B']['sequence'], 'log_B_over_A': log_ratio,
                       'ratio_product': fraction_record(Fraction(b, a), 1),
                       'signed_B_over_A_effect_pct': signed_pct(log_ratio)})
    cycles = []
    for logical in range(0, len(chunks), 2):
        x, y = chunks[logical:logical + 2]
        log_ratio = (x['log_B_over_A'] + y['log_B_over_A']) / 2.0
        cycles.append({'cycle': logical // 2, 'logical_chunks': [logical, logical + 1],
                       'chunk_log_B_over_A': [x['log_B_over_A'], y['log_B_over_A']],
                       'constituent_log_difference': x['log_B_over_A'] - y['log_B_over_A'],
                       'log_B_over_A': log_ratio,
                       'ratio_product': fraction_record(Fraction(x['B_ns'] * y['B_ns'], x['A_ns'] * y['A_ns']), 2),
                       'signed_B_over_A_effect_pct': signed_pct(log_ratio),
                       'magnitude_pct': magnitude_pct(log_ratio)})
    estimate = statistics.mean(v['log_B_over_A'] for v in cycles)
    product = Fraction(math.prod(v['B_ns'] for v in chunks), math.prod(v['A_ns'] for v in chunks))
    extra_side = slot['control'][0] if slot['control'] != 'sham' else None
    adjusted = -estimate if extra_side == 'A' else estimate if extra_side == 'B' else None
    ordinary = [clock_ns(r, metric) for r in measured if r['side'] != extra_side]
    denominator = min(ordinary)
    overhead = {}
    for kind in ('empty-clock', 'zero-boundary'):
        obs = [r for r in rows if r['kind'] == kind]
        values = [clock_ns(r, metric) for r in obs]
        overhead[kind] = {'individual': [{'sequence': r['sequence'], 'phase': r['phase'],
                                          'side': r['side'], 'ns': clock_ns(r, metric)} for r in obs],
                          'count': len(values), 'max_ns': max(values),
                          'mean_ns': statistics.mean(values), 'sum_ns': sum(values),
                          'max_over_smallest_ordinary': max(values) / denominator,
                          'mean_over_smallest_ordinary': statistics.mean(values) / denominator}
    totals = {side: sum(clock_ns(r, metric) for r in measured if r['side'] == side) for side in 'AB'}
    mags = [v['magnitude_pct'] for v in cycles]
    return {'metric': metric, 'chunks': chunks, 'cycles': cycles,
            'equal_weight_cycle_log_B_over_A': estimate,
            'ratio_product': fraction_record(product, len(chunks)),
            'signed_B_over_A_effect_pct': signed_pct(estimate),
            'extra_side': extra_side, 'extra_over_ordinary_log': adjusted,
            'extra_over_ordinary_ratio': math.exp(adjusted) if adjusted is not None else None,
            'measured_total_ns': totals, 'totals_log_B_over_A_descriptive_only': math.log(totals['B'] / totals['A']),
            'smallest_complete_ordinary_span_ns': denominator, 'overhead': overhead,
            'cycle_magnitude_p90_pct_nearest_rank': nearest_rank(mags),
            'cycle_magnitude_max_pct': max(mags)}


def process_gates(cpu, slot):
    c = Checks()
    if slot['control'] == 'sham':
        c.need(ratio_within(cpu['ratio_product'], '0.985', '1.015'),
               'sham.process.abs-effect', cpu['signed_B_over_A_effect_pct'])
        c.need(sum(magnitude_within(v['ratio_product'], 4) for v in cpu['cycles']) >= math.ceil(.9 * len(cpu['cycles'])),
               'sham.process.cycle-p90', cpu['cycle_magnitude_p90_pct_nearest_rank'])
        c.need(all(magnitude_within(v['ratio_product'], 10) for v in cpu['cycles']),
               'sham.process.cycle-max', cpu['cycle_magnitude_max_pct'])
    else:
        ratio = cpu['extra_over_ordinary_ratio']
        record = dict(cpu['ratio_product'])
        if slot['control'] == 'A-extra':
            record['numerator'], record['denominator'] = record['denominator'], record['numerator']
        c.need(ratio_within(record, '1.03', '1.10'),
               'positive.extra-side-ratio', ratio)
        # Redundant sign check makes a misleading signed response conspicuous.
        c.need(fraction_value(cpu['ratio_product']) < 1 if slot['control'] == 'A-extra'
               else fraction_value(cpu['ratio_product']) > 1, 'positive.required-sign')
    for kind, obs in cpu['overhead'].items():
        c.need(obs['max_ns'] * 100 < cpu['smallest_complete_ordinary_span_ns'],
               'overhead.' + kind + '.max-strict', obs['max_over_smallest_ordinary'])
        c.need(obs['sum_ns'] * 400 < obs['count'] * cpu['smallest_complete_ordinary_span_ns'],
               'overhead.' + kind + '.mean-strict', obs['mean_over_smallest_ordinary'])
    return c


def cohort_gates(processes):
    """No partial admission: missing/invalid processes cannot be dropped."""
    c = Checks()
    c.need(len(processes) == 20, 'trial.twenty-processes', len(processes))
    c.need([p['slot']['slot'] for p in processes] == list(range(20)), 'trial.exact-slot-order')
    cells = []
    pooled_cycles = []
    for scene, chunk in CELLS:
        members = [p for p in processes if p['slot']['scene'] == scene and p['slot']['chunk'] == chunk]
        shams = [p for p in members if p['slot']['control'] == 'sham' and p.get('cpu') is not None]
        positives = {p['slot']['control']: p for p in members if p['slot']['control'] != 'sham' and p.get('cpu') is not None}
        tag = 'cell.' + scene + '.' + str(chunk)
        roles = [p['slot']['role'] for p in members]
        c.need(sorted(roles) == sorted(['sham0', 'sham1', 'sham2', 'A-extra', 'B-extra']), tag + '.all-five-roles')
        result = {'scene': scene, 'chunk': chunk, 'slots': [p['slot']['slot'] for p in members]}
        if c.need(len(shams) == 3, tag + '.three-valid-shams'):
            logs = [p['cpu']['equal_weight_cycle_log_B_over_A'] for p in shams]
            effects = [p['cpu']['signed_B_over_A_effect_pct'] for p in shams]
            mean_effect, effect_range = signed_pct(statistics.mean(logs)), max(effects) - min(effects)
            result.update({'sham_effects_pct': effects, 'mean_log_sham_effect_pct': mean_effect,
                           'sham_effect_range_pp': effect_range})
            combined = Fraction(1)
            roots = {p['cpu']['ratio_product']['root'] for p in shams}
            c.need(len(roots) == 1, tag + '.equal-chunk-weights')
            for p in shams:
                combined *= fraction_value(p['cpu']['ratio_product'])
            record = fraction_record(combined, sum(p['cpu']['ratio_product']['root'] for p in shams))
            c.need(ratio_within(record, '0.9925', '1.0075'), tag + '.mean-log-sham', mean_effect)
            with localcontext() as ctx:
                ctx.prec = 80
                ratios = [decimal_ratio(p['cpu']['ratio_product']) for p in shams]
                exact_range = (max(ratios) - min(ratios)) * 100
            result['sham_range_pp_decimal80'] = str(exact_range)
            c.need(exact_range <= Decimal('2'), tag + '.sham-range', effect_range)
        if c.need(set(positives) == {'A-extra', 'B-extra'}, tag + '.both-valid-positive-directions'):
            adjusted = [positives[k]['cpu']['extra_over_ordinary_log'] for k in ('A-extra', 'B-extra')]
            gap = abs(adjusted[0] - adjusted[1])
            result.update({'direction_adjusted_positive_logs': adjusted, 'direction_log_gap': gap})
            with localcontext() as ctx:
                ctx.prec = 80
                exact_gap = abs(-decimal_log(positives['A-extra']['cpu']['ratio_product']) - decimal_log(positives['B-extra']['cpu']['ratio_product']))
            result['direction_log_gap_decimal80'] = str(exact_gap)
            c.need(exact_gap <= Decimal('0.02'), tag + '.direction-gap', gap)
        for p in shams:
            pooled_cycles.extend({'slot': p['slot']['slot'], **cycle} for cycle in p['cpu']['cycles'])
        cells.append(result)
    expected_pooled = sum((512 // s['chunk']) // 2 for s in schedule() if s['control'] == 'sham')
    c.need(len(pooled_cycles) == expected_pooled, 'trial.all-sham-cycles', len(pooled_cycles))
    pooled = {'cycles': pooled_cycles}
    if pooled_cycles:
        mags = [x['magnitude_pct'] for x in pooled_cycles]
        pooled.update({'p90_pct_nearest_rank': nearest_rank(mags), 'max_pct': max(mags)})
        c.need(sum(magnitude_within(v['ratio_product'], 4) for v in pooled_cycles) >= math.ceil(.9 * len(pooled_cycles)), 'trial.pooled-sham-cycle-p90', pooled['p90_pct_nearest_rank'])
        c.need(all(magnitude_within(v['ratio_product'], 10) for v in pooled_cycles), 'trial.pooled-sham-cycle-max', pooled['max_pct'])
    return c, cells, pooled
