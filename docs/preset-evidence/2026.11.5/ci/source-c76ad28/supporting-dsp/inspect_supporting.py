#!/usr/bin/env python3
"""Bind supporting DSP results to exact original CI jobs and actual artifact bytes.

This review never executes downloaded code, qualifies native plugin packages,
or converts software numerical/synthetic checks into acoustic evidence.
"""
from __future__ import annotations
import argparse
import csv
from datetime import datetime, timezone, timedelta
import hashlib
import io
import json
import math
from pathlib import Path
import re
import stat
import subprocess
import zipfile

SOURCE = 'c76ad288e02fbeca48f39652bf4fcdd99837f793'
TREE = '416c1f6bfb4cd9f7dd5dbd9f5cbc211edda2bf00'
RUN = 45351731746
ATTEMPT = 1
REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
JOB_NAMES = {
    'legacy': 'Default-off raw audio versus shipped baseline',
    'source': 'Source circuit V1 and audio V2 compatibility',
    'synthetic': 'Synthetic reference comparison (not hardware evidence)',
    'measure': 'measure',
    'sanitizers': 'sanitizers',
}
ARTIFACTS = {
    'legacy': ('cs80-legacy-compatibility-', 'legacy', 'Run actions/upload-artifact@v5'),
    'source': ('cs80-source-factory-compatibility-', 'source', 'Run actions/upload-artifact@v5'),
    'synthetic': ('cs80-synthetic-comparison-', 'synthetic', 'Run actions/upload-artifact@v5'),
    'candidate': ('cs80-engine-candidate-', 'measure', 'Run actions/upload-artifact@v5'),
    'live-adr': ('cs80-engine-live-adr-', 'measure', 'Upload live envelope candidate trace'),
    'dsp': ('cs80-dsp-', 'measure', 'Upload source-traceable graphs and raw traces'),
}
CRITICAL_STEPS = {
    'legacy': ['Build both revisions with the same compiler', 'Require byte-identical default and noisy-reassignment traces'],
    'source': ['Build immutable circuit and audio checkpoints with one compiler', 'Require unchanged seven-tone V1 circuit numerics', 'Require unchanged source-audio V2 and numerical history'],
    'synthetic': ['Test comparison failures and provenance gates', 'Compare deterministic synthetic stand-in traces'],
    'measure': ['Install pinned numerical plotting tools', 'Build standalone numerical probe', 'Measure and enforce regression bounds', 'Export actual-engine candidate with exact-source provenance'],
    'sanitizers': ['Configure ASan and UBSan core build', 'Build instrument and circuit tests', 'Run sanitizer and validator-fixture tests'],
}
NEW_TESTS = {
    'cs80_struck_body_dynamics_tests', 'cs80_instrument_body_variant_tests',
    'cs80_vocal_body_tests', 'cs80_body_articulation_engine_tests',
    'cs80_original_articulation_tests', 'cs80_keyboard_response_tests',
    'cs80_mallet_preset_refinement_tests', 'cs80_bass_plucked_refinement_tests',
    'cs80_brass_organ_refinement_tests', 'cs80_wind_string_refinement_tests',
    'cs80_pad_poly_refinement_tests', 'cs80_fx_refinement_tests',
    'cs80_modern_preset_refinement_tests', 'cs80_lead_refinement_tests',
    'cs80_preset_response_coverage_tests',
}
MAX_ZIP = 256 * 1024 * 1024
MAX_MEMBER = 64 * 1024 * 1024
ANSI = re.compile(r'\x1b\[[0-9;]*[A-Za-z]')


def need(ok, message):
    if not ok:
        raise ValueError(message)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def unique(pairs):
    out = {}
    for key, value in pairs:
        need(key not in out, 'Duplicate JSON key: ' + key)
        out[key] = value
    return out


def parse(raw):
    return json.loads(raw, object_pairs_hook=unique,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError('Nonfinite JSON value: ' + value)))


def date(value):
    return datetime.fromisoformat(value.replace('Z', '+00:00'))


def in_step(timestamp, step):
    # The API truncates step timestamps to whole seconds; log records retain
    # fractional seconds. This interval accounts only for that precision.
    return date(step['started_at']) <= date(timestamp) < date(step['completed_at']) + timedelta(seconds=1)


def record(path):
    raw = path.read_bytes()
    return {'path': path.name, 'bytes': len(raw), 'sha256': sha(raw)}


def gitbytes(repo, path):
    return subprocess.check_output(['git', '-C', str(repo), 'show', SOURCE + ':' + path])


def sum_lines(raw):
    out = {}
    for line in raw.decode('utf-8').splitlines():
        digest, name = line.split(maxsplit=1)
        name = name.lstrip('*')
        need(re.fullmatch(r'[0-9a-f]{64}', digest) and name not in out, 'Invalid or duplicate checksum entry')
        out[name] = digest
    return out


def zip_contents(raw):
    need(0 < len(raw) <= MAX_ZIP, 'Invalid ZIP size')
    contents, seen, total = {}, set(), 0
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        entries = archive.infolist()
        need(0 < len(entries) <= 1024, 'Excessive or empty ZIP entry roster')
        for entry in entries:
            name = entry.filename[:-1] if entry.is_dir() else entry.filename
            need(name and name not in seen and not name.startswith('/') and '\\' not in name
                 and ':' not in name and all(32 <= ord(c) != 127 for c in name)
                 and all(part not in ('', '.', '..') for part in name.split('/')), 'Unsafe or duplicate ZIP path')
            seen.add(name)
            kind = stat.S_IFMT(entry.external_attr >> 16)
            need(kind in (0, stat.S_IFDIR if entry.is_dir() else stat.S_IFREG), 'Nonregular ZIP object')
            need(not entry.flag_bits & 1 and entry.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED), 'Unsupported ZIP encoding')
            need(0 <= entry.file_size <= MAX_MEMBER, 'Oversized ZIP member')
            total += entry.file_size
            need(total <= MAX_ZIP, 'Excessive expanded ZIP size')
            if not entry.is_dir():
                body = archive.read(entry)
                need(len(body) == entry.file_size, 'Truncated ZIP member')
                contents[name] = body
    return contents, len(seen)


def inspect_capture(kind, contents, repo):
    """Inspect actual exported metadata and frames, without running artifact code."""
    metadata = parse(contents['capture.json'])
    source = gitbytes(repo, 'src/core/Params.h')
    tables = {}
    for name in ('kGlobalDefs', 'kChannelDefs', 'kExtensionDefs'):
        blocks = re.findall(r'inline constexpr ParamDef ' + name + r'\[\] = \{(.*?)^\};',
                            source.decode('utf-8'), re.S | re.M)
        need(len(blocks) == 1, 'Ambiguous committed core parameter table')
        tables[name] = re.findall(r'^\s*\{\s*"([^"]+)"', blocks[0], re.M)
    ids = tables['kGlobalDefs'] + [f'ch{rank}_{name}' for rank in (1, 2)
                                  for name in tables['kChannelDefs']] + tables['kExtensionDefs']
    need(len(ids) == len(set(ids)) == 202, 'Committed core parameter roster mismatch')
    need(type(metadata['parameter_count']) is int and metadata['parameter_count'] == len(ids),
         'Exported parameter count differs from committed core tables')
    maps = ('settings',) if kind == 'candidate' else ('initial_settings', 'final_settings')
    for name in maps:
        values = metadata[name]
        need(set(values) == set(ids) and all(type(v) in (int, float) and math.isfinite(v)
                                           for v in values.values()), 'Incomplete/nonfinite exported settings')
        need(values['body1_variant'] == values['body2_variant'] == 0,
             'Exported resonator variants are not both Standard')
        need(values['body1_model'] == values['body2_model'] == 0
             and values['body1_mix'] == values['body2_mix'] == 0,
             'Compatibility capture unexpectedly enabled instrument bodies')
    need(metadata['capture_kind'] == 'engine_candidate_only' and metadata['analog_evidence'] is False,
         'Engine capture scope mismatch')
    need(metadata['sample_rate_hz'] == 48000 and metadata['internal_sample_rate_hz'] == 96000
         and metadata['sample_count'] == 96000 and metadata['latency_samples'] == 67
         and metadata['latency_compensated'] is False, 'Engine capture timing metadata mismatch')
    need(metadata['channels'] == {'left': 'left.csv', 'right': 'right.csv'}, 'Engine channel manifest mismatch')
    need(metadata['postprocessing'] == 'none: no clipping, normalization, resampling or trimming',
         'Unexpected capture postprocessing')
    expected = {'capture.json', 'provenance.json', 'left.csv', 'right.csv', 'SOURCE_SHA256SUMS',
                'BINARY_SHA256SUMS', 'build-environment.txt', 'SHA256SUMS'}
    if kind == 'candidate':
        need(metadata['schema_version'] == 1 and metadata['fixture_id'] == 'w0-engine-stimulus-v1',
             'Candidate fixture mismatch')
        expected_events = [
            {'sample': 4800, 'type': 'note_on', 'note': 60, 'channel': 1, 'value': .75},
            {'sample': 19200, 'type': 'poly_pressure', 'note': 60, 'channel': 1, 'value': .5},
            {'sample': 33600, 'type': 'poly_pressure', 'note': 60, 'channel': 1, 'value': 0},
            {'sample': 48000, 'type': 'note_off', 'note': 60, 'channel': 1, 'value': 0},
            {'sample': 57600, 'type': 'note_on', 'note': 67, 'channel': 1, 'value': .625},
            {'sample': 72000, 'type': 'note_off', 'note': 67, 'channel': 1, 'value': 0},
        ]
        need(metadata['events'] == expected_events, 'Candidate event roster mismatch')
    else:
        expected.add('envelope-events.csv')
        need(metadata['schema_version'] == 2 and metadata['fixture_id'] == 'live-adr-v1'
             and metadata['envelope_trace'] == 'envelope-events.csv', 'Live-envelope fixture mismatch')
        events = metadata['events']
        need(len(events) == 5 and [e['sequence'] for e in events] == list(range(5))
             and [e['sample'] for e in events] == [4800, 8640, 28800, 48000, 52800],
             'Live-envelope event roster mismatch')
        values = dict(metadata['initial_settings'])
        for sequence, phase in ((1, 'a'), (2, 'd'), (4, 'r')):
            event = events[sequence]
            need(event['type'] == 'parameter_batch'
                 and [c['id'] for c in event['changes']] ==
                 [f'ch{rank}_{envelope}_{phase}' for rank in (1, 2) for envelope in ('v', 'f')],
                 'Live-envelope parameter batch mismatch')
            for change in event['changes']:
                need(set(change) == {'id', 'old', 'new'} and change['old'] == values[change['id']]
                     and type(change['new']) in (int, float) and math.isfinite(change['new']),
                     'Live-envelope old/new metadata mismatch')
                values[change['id']] = change['new']
        need(values == metadata['final_settings'], 'Live-envelope final settings do not replay')
        rows = list(csv.DictReader(io.StringIO(contents['envelope-events.csv'].decode('utf-8'))))
        need(len(rows) == 36, 'Live-envelope observation roster mismatch')
    need(set(contents) == expected, 'Unexpected engine capture archive member roster')
    channels = []
    for name in ('left.csv', 'right.csv'):
        rows = csv.reader(io.StringIO(contents[name].decode('utf-8')))
        need(next(rows) == ['sample', 'value'], 'Engine channel CSV header mismatch')
        values = []
        for index, row in enumerate(rows):
            need(len(row) == 2 and row[0] == str(index), 'Engine channel frame index mismatch')
            number = float(row[1])
            need(math.isfinite(number), 'Nonfinite engine channel sample')
            values.append(number)
        need(len(values) == 96000 and all(v == 0 for v in values[:4800]),
             'Engine channel frame count or initial silence mismatch')
        need(max(abs(v) for v in values[4800:48000]) > 1e-5
             and max(abs(v) for v in values[57600:72000]) > 1e-5,
             'Engine channel lacks the fixture signal')
        channels.append({'path': name, 'frames': len(values), 'peak_absolute_sample': max(map(abs, values))})
    return {'parameter_count': len(ids), 'parameter_id_source': 'src/core/Params.h',
            'parameter_id_source_sha256': sha(source), 'parameter_maps_checked': list(maps),
            'standard_body_variants': {'body1_variant': 0, 'body2_variant': 0},
            'fixture_id': metadata['fixture_id'], 'fixture_event_count': len(metadata['events']),
            'channels': channels, 'sample_rate_hz': 48000, 'internal_sample_rate_hz': 96000,
            'latency_samples': 67, 'latency_compensated': False,
            'scope': 'Actual core fixtures with instrument bodies disabled, not factory-program listening or a physical-instrument comparison.'}


def inspect_content(kind, contents, repo):
    checks = {}
    if kind == 'synthetic':
        r, manifest = parse(contents['report.json']), parse(contents['manifest.json'])
        need(r['passed'] is True and r['analog_evidence'] is False and r['evidence_kind'] == 'synthetic', 'Synthetic scope/result mismatch')
        need(r['provenance']['source_revision'] == SOURCE and r['provenance']['working_tree_dirty'] is False, 'Synthetic source mismatch')
        need(r['manifest_sha256'] == sha(contents['manifest.json']), 'Synthetic manifest digest mismatch')
        for path, digest in r['provenance']['source_hashes'].items():
            need(sha(gitbytes(repo, path)) == digest, 'Synthetic source hash mismatch: ' + path)
        traces = 0
        for stage in manifest['stages']:
            for which in ('candidate', 'reference'):
                trace = stage[which]
                need(sha(contents[trace['path']]) == trace['sha256'], 'Synthetic trace mismatch')
                traces += 1
        checks.update(report_passed=True, trace_hashes_matched=traces,
                      source_hashes_matched=len(r['provenance']['source_hashes']), analog_evidence=False)
    elif kind in ('legacy', 'source'):
        r = parse(contents['report.json'])
        need(r['candidate'] == SOURCE and r['analog_evidence'] is False, 'Compatibility source/scope mismatch')
        sums = sum_lines(contents['SHA256SUMS'])
        if kind == 'legacy':
            need(r['baseline'] == r['corpus_source'] == 'd56911a7c5f6953cd244457226b126cc567a538b'
                 and r['default_off_byte_identical'] is True, 'Legacy checkpoint mismatch')
            need(sha(contents['baseline-corpus.txt']) == r['corpus_sha256']
                 == sums['artifacts/compatibility/baseline-corpus.txt'], 'Legacy corpus digest mismatch')
            need(len(sums) == 5, 'Legacy digest roster mismatch')
            for scene in ('default', 'tails'):
                need(sums[f'artifacts/compatibility/baseline-{scene}.f32']
                     == sums[f'artifacts/compatibility/current-{scene}.f32'], 'Legacy comparison digest mismatch')
        else:
            need(r['circuit_baseline'] == '22d6a32c48c85f9b813506edcd856f73f4293c2d'
                 and r['audio_baseline'] == '00ceccfacb3cb9845436701da01c4387d53a361a'
                 and r['v1_circuit_bytes_unchanged'] is True
                 and r['v2_audio_and_numeric_state_bytes_unchanged'] is True, 'Circuit/audio checkpoint mismatch')
            need(len(sums) == 4, 'Source compatibility digest roster mismatch')
            for current, baseline in [('audio-current', 'audio-v2'), ('circuit-current', 'circuit-v1')]:
                need(sums[f'artifacts/source-factory-compatibility/{current}.bin']
                     == sums[f'artifacts/source-factory-compatibility/{baseline}.bin'], 'Source compatibility digest mismatch')
        checks.update(report=r, recorded_comparison_digests=sums, recorded_digest_pairs_equal=True,
                      raw_comparison_binaries_distributed=False,
                      byte_comparison_basis='Successful original unsuppressed cmp steps and retained digest report. No local raw comparison claimed.')
    elif kind in ('candidate', 'live-adr'):
        r = parse(contents['provenance.json'])
        need(r['commit'] == SOURCE and int(r['run']) == RUN and r['analog_evidence'] is False, 'Engine trace provenance mismatch')
        sums = sum_lines(contents['SHA256SUMS'])
        need(set(sums) == set(contents) - {'SHA256SUMS'}, 'Engine checksum coverage mismatch')
        for name, digest in sums.items():
            need(sha(contents[name]) == digest, 'Engine member digest mismatch: ' + name)
        source_sums = sum_lines(contents['SOURCE_SHA256SUMS'])
        for path, digest in source_sums.items():
            need(sha(gitbytes(repo, path)) == digest, 'Engine source hash mismatch: ' + path)
        binary_sums = sum_lines(contents['BINARY_SHA256SUMS'])
        need(list(binary_sums) == ['build-probe/cs80_parity_capture'], 'Unexpected trace producer binary declaration')
        checks.update(manifest_checksums_matched=len(sums), source_hashes_matched=len(source_sums),
                      binary_digest_declarations=binary_sums, binary_distributed=False, analog_evidence=False,
                      actual_capture=inspect_capture(kind, contents, repo))
    elif kind == 'dsp':
        r = parse(contents['measurements.json'])
        need(r['git_head'] == SOURCE and r['probe_failures'] == 0 and r['comparative_regression_passed'] is True, 'Numerical source/regression mismatch')
        for path, digest in r['analysis_time_source_sha256'].items():
            need(sha(gitbytes(repo, path)) == digest, 'Numerical analysis source mismatch: ' + path)
        need(not r['working_tree_status'], 'Numerical source tree is dirty')
        checks.update(comparative_regression_passed=True, probe_failures=0,
                      source_hashes_matched=len(r['analysis_time_source_sha256']),
                      analysis_working_tree_status=r['working_tree_status'],
                      scope='Software numerical references; no physical-instrument comparison.')
    numeric_cells, csv_files = 0, 0
    for name, raw in contents.items():
        if not name.endswith('.csv'):
            continue
        csv_files += 1
        for row in csv.reader(io.StringIO(raw.decode('utf-8'))):
            for value in row:
                try:
                    number = float(value)
                except ValueError:
                    continue
                need(math.isfinite(number), 'Nonfinite numeric CSV cell: ' + name)
                numeric_cells += 1
    checks['csv_scan'] = {'files': csv_files, 'finite_numeric_cells': numeric_cells,
                          'scope': 'Every parseable numeric cell is finite; this does not independently recompute the numerical/Fourier comparison.'}
    return checks


def inspect_job(key, job, evidence, complete):
    need(job['run_id'] == RUN and job['run_attempt'] == ATTEMPT and job['head_sha'] == SOURCE, 'Job identity mismatch')
    if complete:
        need(job['status'] == 'completed' and job['conclusion'] == 'success', 'Original job is not successful: ' + key)
    row = {k: job.get(k) for k in ('id', 'name', 'head_sha', 'run_id', 'run_attempt', 'status', 'conclusion', 'started_at', 'completed_at', 'html_url')}
    row['steps'] = job['steps']
    if job['status'] != 'completed':
        return row
    need(job['conclusion'] == 'success', 'Original job failed: ' + key)
    steps = {s['name']: s for s in job['steps']}
    for name in CRITICAL_STEPS[key]:
        need(name in steps and steps[name]['conclusion'] == 'success', 'Required original step not successful: ' + name)
    path = evidence / (key + '-job.log')
    text = path.read_text(encoding='utf-8-sig')
    lines = [ANSI.sub('', line) for line in text.splitlines()]
    checkout = [i for i, line in enumerate(lines) if 'git log -1 --format=%H' in line]
    need(any(i + 1 < len(lines) and lines[i + 1].split()[-1] == SOURCE for i in checkout), 'Exact checkout not found in original log')
    row['decoded_log'] = record(path)
    row['observed_exact_checkout'] = True
    row['python_test_terminal_summaries'] = [int(n) for n in re.findall(r'Ran (\d+) tests? in [0-9.]+s', text)]
    if key == 'sanitizers':
        need('ASAN_OPTIONS: detect_leaks=1:halt_on_error=1' in text
             and 'UBSAN_OPTIONS: halt_on_error=1:print_stacktrace=1' in text,
             'Sanitizer runtime environment is missing')
        cases = []
        for line in lines:
            m = re.search(r'\b(\d+)/(\d+) Test\s+#(\d+):\s+(\S+)\s+\.+\s*(Passed|\*\*\*\w+)(.*)', line)
            if m:
                cases.append({'ordinal': int(m[1]), 'registered': int(m[2]), 'test_number': int(m[3]),
                              'name': m[4], 'result': m[5], 'detail': m[6].strip()})
        need(cases and len({x['name'] for x in cases}) == len(cases), 'Empty or duplicate sanitizer result roster')
        totals = {x['registered'] for x in cases}
        need(len(totals) == 1 and len(cases) == next(iter(totals)), 'Incomplete sanitizer result roster')
        count = len(cases)
        need({x['ordinal'] for x in cases} == set(range(1, count + 1)), 'Sanitizer ordinal roster mismatch')
        by_name = {x['name']: x for x in cases}
        need(NEW_TESTS <= set(by_name) and all(by_name[n]['result'] == 'Passed' for n in NEW_TESTS), 'New refinement CTests missing or not passed')
        diagnostics = [line for line in lines if re.search(r'ERROR: (AddressSanitizer|LeakSanitizer)|runtime error:|SUMMARY: (AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer)|AddressSanitizer:DEADLYSIGNAL', line)]
        failures = [x for x in cases if x['result'] not in ('Passed', '***Skipped')]
        need(not failures and not diagnostics, 'Sanitizer failures/diagnostics require review')
        row['ctest'] = {'registered': count, 'passed': sum(x['result'] == 'Passed' for x in cases),
                        'formal_skipped': sum(x['result'] == '***Skipped' for x in cases),
                        'failed': len(failures), 'cases': sorted(cases, key=lambda x: x['ordinal']),
                        'new_refinement_test_names_passed': sorted(NEW_TESTS),
                        'nested_python_skips': None,
                        'nested_python_skip_scope': 'Unknown: successful child stdout is suppressed by --output-on-failure; not inferred as zero.'}
        row['sanitizer_diagnostic_lines'] = diagnostics
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--downloads', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--run-api', type=Path, default=Path('run.json'))
    parser.add_argument('--jobs-api', type=Path, default=Path('jobs.json'))
    parser.add_argument('--artifacts-api', type=Path, default=Path('artifacts.json'))
    parser.add_argument('--allow-pending-run', action='store_true')
    args = parser.parse_args()
    need(not args.out.exists(), 'Refusing to overwrite an evidence report')
    inputs = [p if p.is_absolute() else args.evidence / p for p in (args.run_api, args.jobs_api, args.artifacts_api)]
    run = parse(inputs[0].read_bytes())
    jobs_api = parse(inputs[1].read_bytes())
    artifacts_api = parse(inputs[2].read_bytes())
    jobs, artifacts = jobs_api['jobs'], artifacts_api['artifacts']
    need(jobs_api['total_count'] == len(jobs) and artifacts_api['total_count'] == len(artifacts), 'API pagination is incomplete')
    need(run['id'] == RUN and run['head_sha'] == SOURCE and run['run_attempt'] == ATTEMPT, 'Run source/attempt mismatch')
    need(run['repository']['full_name'] == REPOSITORY and run['head_repository']['full_name'] == REPOSITORY, 'Run repository mismatch')
    need(subprocess.check_output(['git', '-C', str(args.repo), 'rev-parse', SOURCE + '^{tree}'], text=True).strip() == TREE, 'Source tree mismatch')
    complete = not args.allow_pending_run
    if complete:
        need(run['status'] == 'completed' and run['conclusion'] == 'success', 'Supporting run not complete and successful')
    need(len(jobs) == 5 and {j['name'] for j in jobs} == set(JOB_NAMES.values()), 'Expected five-job roster mismatch')
    by_key = {key: next(j for j in jobs if j['name'] == name) for key, name in JOB_NAMES.items()}
    rows = [inspect_job(key, job, args.evidence, complete) for key, job in by_key.items()]
    inspections = []
    for kind, (prefix, job_key, upload_name) in ARTIFACTS.items():
        matches = [a for a in artifacts if a['name'] == prefix + SOURCE]
        if not matches and not complete:
            continue
        need(len(matches) == 1, 'Missing/duplicate expected supporting artifact: ' + kind)
        a = matches[0]; job = by_key[job_key]
        need(job['status'] == 'completed' and job['conclusion'] == 'success', 'Artifact producer not successful')
        need(a['expired'] is False and a['workflow_run']['id'] == RUN
             and a['workflow_run']['head_sha'] == SOURCE
             and a['workflow_run']['repository_id'] == run['repository']['id']
             and a['workflow_run']['head_repository_id'] == run['head_repository']['id'], 'Artifact identity mismatch')
        upload = [s for s in job['steps'] if s['name'] == upload_name]
        need(len(upload) == 1 and upload[0]['conclusion'] == 'success', 'Original upload step missing/not successful')
        upload = upload[0]
        need(in_step(a['created_at'], upload), 'Artifact creation outside original upload step')
        blob = (args.downloads / (str(a['id']) + '.zip')).read_bytes()
        need(len(blob) == a['size_in_bytes'] and 'sha256:' + sha(blob) == a['digest'], 'Artifact bytes mismatch')
        lines = (args.evidence / (job_key + '-job.log')).read_text(encoding='utf-8-sig').splitlines()
        finalized = [line for line in lines if f"Artifact {a['name']}.zip successfully finalized. Artifact ID {a['id']}" in line]
        need(len(finalized) == 1 and in_step(finalized[0].split()[0], upload), 'Artifact finalize identity/time mismatch')
        sized = [line for line in lines if f"Final size is {a['size_in_bytes']} bytes. Artifact ID is {a['id']}" in line]
        digested = [line for line in lines if 'SHA256' in line and a['digest'][7:] in line]
        need(len(sized) == 1 and in_step(sized[0].split()[0], upload), 'Artifact upload size/ID mismatch')
        need(len(digested) == 1 and in_step(digested[0].split()[0], upload), 'Artifact upload digest mismatch')
        contents, entry_count = zip_contents(blob)
        checks = inspect_content(kind, contents, args.repo)
        inspections.append({'kind': kind, 'artifact_id': a['id'], 'name': a['name'],
                            'bytes': len(blob), 'sha256': sha(blob), 'created_at': a['created_at'],
                            'expires_at': a['expires_at'], 'producer_job_id': job['id'],
                            'source': SOURCE, 'run_id': RUN, 'run_attempt': ATTEMPT,
                            'attempt_bound_by_original_successful_upload': True,
                            'producer_upload_lines': {'finalized': finalized[0], 'size': sized[0], 'digest': digested[0]},
                            'upload_step': upload, 'artifact_id_ordering_used_as_provenance': False,
                            'entry_count': entry_count, 'entry_inventory': [{'path': name, 'bytes': len(body), 'sha256': sha(body)} for name, body in sorted(contents.items())],
                            'checks': checks})
    if complete:
        need(len(inspections) == 6 and len(artifacts) == 6, 'Supporting artifact roster is incomplete/unexpected')
    paired = {x['kind']: x for x in inspections}
    if 'candidate' in paired and 'live-adr' in paired:
        need(paired['candidate']['checks']['binary_digest_declarations'] == paired['live-adr']['checks']['binary_digest_declarations'], 'Trace exports name different producer binaries')
    report = {'schema': 2, 'reviewer': 'A46, distinct worker 46 of 60', 'source': SOURCE,
              'source_tree': TREE, 'reviewed_at_utc': datetime.now(timezone.utc).isoformat(),
              'status': 'completed' if complete else 'partial', 'conclusion': run['conclusion'],
              'supporting_scope_qualified': complete, 'native_release_qualified': False,
              'run': {k: run.get(k) for k in ('id', 'name', 'head_sha', 'run_attempt', 'status', 'conclusion', 'created_at', 'run_started_at', 'updated_at', 'html_url')},
              'api_inputs': [record(p) for p in inputs],
              'jobs': rows, 'actual_artifact_inspections': inspections,
              'source_contract': record(args.evidence / 'source-contract.json'),
              'inspection_script': record(Path(__file__)),
              'scope_limits': ['Supporting DSP workflow only; does not qualify native plugin packages or pluginval.',
                               'Core-only/Python jobs do not provide JUCE plugin provenance; native qualification owns that gate.',
                               'Synthetic and numerical references are software fixtures, not physical CS-80 or acoustic-instrument measurements.',
                               'Compatibility artifacts retain reports/digests and a legacy corpus, not raw comparison binaries; original successful cmp steps provide comparison evidence.',
                               'Trace and compatibility artifacts contain tool-defined corpora, not per-program listening evaluations of all 523 factory entries.',
                               'Sanitizer CTest executions remain separate from native counts; suppressed nested Python skips are unknown.',
                               'Original API step times have second precision; fractional log times are compared within that same final second.',
                               'Acquisition uses authenticated GitHub tools; no public unauthenticated download-route claim is made.']}
    with args.out.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2); stream.write('\n')
    print(json.dumps({'status': report['status'], 'successful_original_jobs': sum(x['conclusion'] == 'success' for x in rows), 'inspected_artifacts': len(inspections), 'native_release_qualified': False}))


if __name__ == '__main__':
    main()
