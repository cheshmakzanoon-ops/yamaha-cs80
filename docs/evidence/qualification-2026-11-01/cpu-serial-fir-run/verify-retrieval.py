#!/usr/bin/env python3
"""Read-only verification/reporting of an already-completed CPU CI artifact."""
import argparse
import csv
import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

KEYS = ('rate', 'block', 'mode', 'scenario', 'initial_selector', 'phase')
TIMING = {'min_us', 'median_us', 'p95_us', 'p99_us', 'max_us', 'mean_rtf',
          'median_rtf', 'p99_rtf', 'max_rtf', 'budget_exceedances',
          'event_median_us', 'event_max_us', 'process_cpu_seconds_including_checks',
          'thread_cpu_seconds_including_checks'}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read(path):
    return json.loads(path.read_text())


def write(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n')


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    obj = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(obj)
    return obj


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--extracted', type=Path, required=True)
    p.add_argument('--repository', type=Path, required=True)
    args = p.parse_args()
    root, repo = args.extracted.resolve(), args.repository.resolve()
    internal = read(root / 'sha256.json')
    for name, expected in internal.items():
        path = root / name
        if root not in path.resolve().parents or sha(path.read_bytes()) != expected:
            raise ValueError('Internal artifact manifest mismatch: ' + name)
    request = read(root / 'request.json')
    runner = module(repo / 'scripts/run-cpu-qualification.py', 'frozen_runner')
    reporter = module(repo / 'scripts/summarize-cpu-benchmarks.py', 'frozen_reporter')
    runner.validate_request(request)
    runner.verify_reporter(repo / 'scripts/summarize-cpu-benchmarks.py', request['reporter_sha256'])
    provenance = read(root / 'provenance.json')
    for name, key in [('run-cpu-qualification.py', 'runner_script_sha256'),
                      ('summarize-cpu-benchmarks.py', 'reporter_sha256'),
                      ('request.json', 'request_sha256')]:
        if sha((root / name).read_bytes()) != provenance[key]:
            raise ValueError('Provenance hash mismatch: ' + name)
    if (root / 'run-cpu-qualification.py').read_bytes() != (repo / 'scripts/run-cpu-qualification.py').read_bytes():
        raise ValueError('Collector source differs from the independently qualified source')
    complete = read(root / 'completed.json')
    checkpoints = request['checkpoints']
    if complete != {'completed': True, 'checkpoints': len(checkpoints), 'rounds_each': 3, 'performance_pass_claim': False}:
        raise ValueError('Unexpected completed-run record')
    schedule = read(root / 'completed-rounds.json')
    expected = [(n, x['label']) for n in (1, 2, 3) for x in checkpoints]
    if [(x['round'], x['label']) for x in schedule] != expected:
        raise ValueError('Round-major schedule differs')
    previous = None
    for item in schedule:
        start = datetime.datetime.fromisoformat(item['start'])
        end = datetime.datetime.fromisoformat(item['end'])
        if end < start or (previous is not None and start < previous):
            raise ValueError('Timing intervals overlap or reverse')
        previous = end
    source_records, oid_records, summaries = [], {}, []
    for checkpoint in checkpoints:
        path = root / checkpoint['label']
        manifest = read(path / 'manifest.json')
        if any(manifest[k] != checkpoint[k] for k in ('label', 'source_commit', 'benchmark_blob')):
            raise ValueError('Checkpoint manifest identity mismatch')
        if datetime.datetime.fromisoformat(manifest['compile_completed_utc']) > datetime.datetime.fromisoformat(schedule[0]['start']):
            raise ValueError('A checkpoint build overlaps the timing sweep')
        commit = checkpoint['source_commit']
        tree = subprocess.check_output(['git', 'rev-parse', commit + '^{tree}'], cwd=repo, text=True).strip()
        if tree != manifest['source_tree']:
            raise ValueError('Recorded source tree differs from its immutable commit')
        entries = subprocess.check_output(['git', 'ls-tree', '-rz', commit, '--', 'src/core',
            'src/experimental', 'src/reference', runner.BENCHMARK_SOURCE, 'CMakeLists.txt'], cwd=repo)
        tree_files = {}
        for entry in entries.split(b'\0'):
            if not entry:
                continue
            details, name = entry.split(b'\t', 1)
            _, kind, oid = details.decode().split()
            if kind != 'blob':
                raise ValueError('Unexpected non-blob source entry')
            tree_files[name.decode()] = oid
        if set(tree_files) != set(manifest['source_sha256']):
            raise ValueError('Source manifest path coverage differs from the immutable tree')
        for name, oid in tree_files.items():
            oid_records.setdefault(oid, []).append((checkpoint['label'], name, manifest['source_sha256'][name]))
        data = (path / 'benchmark-source.cpp').read_bytes()
        blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if blob != checkpoint['benchmark_blob'] or tree_files[runner.BENCHMARK_SOURCE] != blob:
            raise ValueError('Original benchmark source blob mismatch')
        source_records.append({**checkpoint, 'source_tree': tree,
            'source_files': len(tree_files), 'benchmark_sha256': sha(data),
            'executable_sha256_recorded_by_runner': manifest['executable_sha256']})
        for n in (1, 2, 3):
            runner.validate_round(path / f'round-{n}', commit)
        computed = reporter.summarize(path)
        if computed != read(path / 'summary.json'):
            raise ValueError('Stored summary differs from the frozen reporter: ' + checkpoint['label'])
        held = [x for x in computed['rows'] if x['mode'] == 'legacy' and x['scenario'] == 'paired_tones' and x['phase'] == 'warmed_held']
        legacy_idle = next(x for x in computed['rows'] if x['mode'] == 'legacy' and x['scenario'] == 'idle_quiet')
        moving = computed['moving_controls'][0]
        summaries.append({'checkpoint': checkpoint['label'], 'source_commit': commit, 'rounds': 3,
            'held_median_rtf_range': computed['held_median_rtf_range'],
            'held_p99_rtf_range': computed['held_p99_rtf_range'],
            'held_selected_round_budget_exceedances': computed['held_selected_round_budget_exceedances'],
            'idle_median_rtf': computed['idle_median_rtf'],
            'moving_median_rtf': moving['median_rtf'], 'moving_p99_rtf': moving['p99_rtf'],
            'moving_misses': moving['budget_exceedances'],
            'legacy_held_range': [min(x['median_rtf'] for x in held), max(x['median_rtf'] for x in held)],
            'legacy_idle_median_rtf': legacy_idle['median_rtf'],
            'nonfinite_all_rounds': sum(x['nonfinite_count_all_rounds'] for x in computed['rows'])})
    oids = sorted(oid_records)
    contents = subprocess.check_output(['git', 'cat-file', '--batch'], cwd=repo,
        input=('\n'.join(oids) + '\n').encode())
    offset = 0
    for expected_oid in oids:
        end = contents.index(b'\n', offset)
        oid, kind, size = contents[offset:end].decode().split()
        if oid != expected_oid or kind != 'blob':
            raise ValueError('Source blob unavailable or wrong kind')
        size = int(size)
        body = contents[end + 1:end + 1 + size]
        offset = end + 2 + size
        if contents[offset - 1:offset] != b'\n' or len(body) != size:
            raise ValueError('Malformed git batch source response')
        actual = sha(body)
        for label, name, expected_hash in oid_records[oid]:
            if actual != expected_hash:
                raise ValueError('Source content mismatch: ' + label + '/' + name)
    if offset != len(contents):
        raise ValueError('Unparsed source response bytes')
    write(root / 'compact-summary.json', summaries)
    write(root / 'independent-verification.json', {'internal_manifest_entries_verified': len(internal),
        'completed_rounds_verified': len(schedule), 'csv_rows_verified': len(schedule) * 51,
        'source_file_records_verified': sum(x['source_files'] for x in source_records),
        'unique_source_blobs_verified': len(oids), 'all_original_reporter_summaries_match': True,
        'all_builds_before_timing': True, 'all_rounds_serial': True, 'source_records': source_records})
    labels = {x['label']: x for x in checkpoints}
    if {'pre-fir-native-bbd', 'zero-fir-history'} <= labels.keys():
        all_rows = {}
        for label in ('pre-fir-native-bbd', 'zero-fir-history'):
            for n in (1, 2, 3):
                with (root / label / f'round-{n}/results.csv').open(newline='') as stream:
                    rows = list(csv.DictReader(stream))
                all_rows[f'{label}/round-{n}'] = {tuple(x[k] for k in KEYS): x for x in rows}
        reference = next(iter(all_rows.values()))
        differences = []
        for origin, rows in all_rows.items():
            if rows.keys() != reference.keys():
                raise ValueError('Before/after fixture key mismatch')
            for key, row in rows.items():
                for field, value in row.items():
                    if field not in TIMING and value != reference[key][field]:
                        differences.append({'round': origin, 'case': dict(zip(KEYS, key)),
                            'field': field, 'before': reference[key][field], 'after': value})
        before, after = (next(x for x in summaries if x['checkpoint'] == label)
                         for label in ('pre-fir-native-bbd', 'zero-fir-history'))
        write(root / 'fir-comparison.json', {'before': before, 'after': after,
            'idle_selected_median_relative_reduction': 1 - after['idle_median_rtf'] / before['idle_median_rtf'],
            'compared_fields': sorted(set(next(iter(reference.values()))) - TIMING),
            'excluded_timing_fields': sorted(TIMING), 'rounds': 6, 'rows': 306,
            'all_non_timing_fields_equal': not differences, 'differences': differences})
        if differences:
            raise ValueError('Non-timing before/after differences recorded for review')
    print(json.dumps({'checkpoints': len(checkpoints), 'rounds': len(schedule),
        'rows': len(schedule) * 51, 'internal_files_verified': len(internal),
        'unique_source_blobs_verified': len(oids)}, indent=2))


if __name__ == '__main__':
    main()
