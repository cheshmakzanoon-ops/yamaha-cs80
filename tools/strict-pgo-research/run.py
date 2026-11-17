#!/usr/bin/env python3
"""One Engine-only PGO experiment; no DSP edits, retries or assessment-driven training."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parent
OBJECTS = {
    'src': '702bf5ee4e375c7b13da7ec95b01b56587bbf75c',
    'tests': '64b7f72ed0139d846e8107c47c62f190cb13eefa',
    'CMakeLists.txt': '288b80b965363547c0851815c95e6bbc17fa84c2',
    'cmake': '22c04e90f3245ccd8a17e7640122fa949948cec3',
}
FLAGS = ['-std=c++20', '-O3', '-DNDEBUG']
COMPILER_VERSION = '13.3.0'
TRAIN = {'suite': 'performance', 'rate': 48000, 'block': 128, 'callbacks': 16,
         'quick': True, 'cases': 15, 'rows': 36}
QUAL_BASE = {'suite': 'baseline', 'rate': 44100, 'block': 256, 'callbacks': 16,
             'quick': False, 'cases': 27, 'rows': 51}
QUAL = {'suite': 'performance', 'rate': 44100, 'block': 256, 'callbacks': 16,
        'quick': False, 'cases': 39, 'rows': 100}
COST = {**QUAL, 'callbacks': 128}
UNSEEN = frozenset(str(i) for i in range(2, 10))
GROUPS = ('primary', 'source_performance', 'all', 'isolation', 'legacy')
REGRESSIONS = (
    ('SmoothingTests', 120),
    ('ActiveTailContinuityTests', 120),
    ('MpeMasterSmoothingTests', 120),
    ('SourcePerformanceAuditTests', 180),
    ('FilterBatchEngineTests', 180),
)
KEY = ('rate', 'block', 'mode', 'scenario', 'initial_selector', 'phase')
IGNORE = {'min_us', 'median_us', 'p95_us', 'p99_us', 'max_us', 'mean_rtf',
          'median_rtf', 'p99_rtf', 'max_rtf', 'budget_exceedances', 'event_median_us',
          'event_max_us', 'process_cpu_seconds_including_checks',
          'thread_cpu_seconds_including_checks', 'callback_cpu_seconds', 'callback_cpu_rtf'}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n')


def expected_performance_keys(spec):
    selectors = (0, 1, 10) if spec['quick'] else range(11)
    keys = set()
    for selector in selectors:
        for scenario in ('paired_tones', 'mpe_chorus_reverb', 'mpe_phaser_delay_drive'):
            phases = ('warmed_held', 'release_tail') if scenario == 'paired_tones' else (
                'warmed_held', 'sustain_pedal_hold', 'release_tail')
            for phase in phases:
                keys.add((str(spec['rate']), str(spec['block']), 'source_performance',
                          scenario, str(selector), phase))
        if selector in (0, 1, 10):
            for mode in ('isolation', 'legacy'):
                for phase in ('warmed_held', 'release_tail'):
                    keys.add((str(spec['rate']), str(spec['block']), mode,
                              'paired_tones', str(selector), phase))
    return keys


def read_rows(directory, spec, clock=False):
    completed = json.loads((directory / 'completed.json').read_text())
    with (directory / 'results.csv').open() as stream:
        rows = list(csv.DictReader(stream))
    require(completed.get('completed') is True and completed.get('rows') == spec['rows']
            and completed.get('cases') == spec['cases'] and len(rows) == spec['rows'],
            'completion/case/row count')
    keyed = {tuple(row[k] for k in KEY): row for row in rows}
    require(len(keyed) == len(rows), 'duplicate rows')
    if spec['suite'] == 'performance':
        require(set(keyed) == expected_performance_keys(spec), 'frozen performance inventory')
    for row in rows:
        require(row['rate'] == str(spec['rate']) and row['block'] == str(spec['block']),
                'declared rate/block')
        require(row['nonfinite_count'] == '0', 'nonfinite PCM')
        if row['phase'] == 'warmed_held':
            require(int(row['callbacks']) == spec['callbacks'], 'held callback count')
        if clock:
            require(row['callback_cpu_status'] == 'available', 'callback clock unavailable')
            require(int(row['callback_cpu_call_count']) == int(row['callbacks']), 'clock call count')
            cpu, audio = float(row['callback_cpu_seconds']), float(row['audio_seconds'])
            require(math.isfinite(cpu) and cpu > 0 and math.isfinite(audio) and audio > 0,
                    'invalid clock')
            require(math.isclose(float(row['callback_cpu_rtf']), cpu / audio,
                                 rel_tol=2e-5, abs_tol=1e-8), 'clock denominator')
    return keyed


def exact(a, b):
    require(a.keys() == b.keys(), 'case inventory mismatch')
    for key, row in a.items():
        require(row.keys() == b[key].keys(), 'CSV schema mismatch')
        for field in row:
            if field not in IGNORE:
                require(row[field] == b[key][field], f'output/work mismatch {key} {field}')


def group_keys(rows, group):
    if group == 'all':
        return list(rows)
    mode = 'source_performance' if group == 'primary' else group
    keys = [key for key, row in rows.items() if row['mode'] == mode
            and row['phase'] == 'warmed_held'
            and (group != 'primary' or row['initial_selector'] in UNSEEN)]
    require(bool(keys), 'empty metric group')
    if group == 'primary':
        require(len(keys) == 24 and {rows[k]['initial_selector'] for k in keys} == UNSEEN,
                'unseen primary inventory')
    return keys


def ratio(a, b, group):
    keys = group_keys(a, group)
    def score(rows):
        return (sum(float(rows[k]['callback_cpu_seconds']) for k in keys)
                / sum(float(rows[k]['audio_seconds']) for k in keys))
    return score(b) / score(a)


def calibration(pairs):
    records = []
    for a, b in pairs:
        exact(a, b)
        groups = {group: ratio(a, b, group) for group in GROUPS}
        rows = {str(k): float(b[k]['callback_cpu_seconds']) / float(a[k]['callback_cpu_seconds'])
                for k in a}
        limits = {'primary': .02, 'source_performance': .02, 'all': .02,
                  'isolation': .03, 'legacy': .03}
        passed = all(abs(groups[g] - 1) <= limits[g] for g in GROUPS)
        passed = passed and all(.9 <= value <= 1.1 for value in rows.values())
        records.append({'groups': groups, 'rows': rows, 'pass': passed})
    return len(records) == 3 and all(x['pass'] for x in records), records


def assess(pairs, records):
    require(len(pairs) == 3 and len(records) == 3 and all(x['pass'] for x in records),
            'three calibrated cost pairs required')
    for a, b in pairs:
        exact(a, b)
    def gm(values):
        return math.exp(sum(math.log(x) for x in values) / len(values))
    groups = {g: gm([ratio(a, b, g) for a, b in pairs]) for g in GROUPS}
    rows = {str(k): gm([float(b[k]['callback_cpu_seconds']) / float(a[k]['callback_cpu_seconds'])
                       for a, b in pairs]) for k in pairs[0][0]}
    noise = max(abs(x['groups']['primary'] - 1) for x in records)
    required_gain = max(.005, 2 * noise)
    primary_pairs = [ratio(a, b, 'primary') for a, b in pairs]
    passed = (all(x < 1 for x in primary_pairs) and 1 - groups['primary'] > required_gain
              and all(groups[g] <= 1.02 for g in GROUPS if g != 'primary')
              and all(x <= 1.05 for x in rows.values()))
    return {'decision': 'SCOPED_COST_PASS' if passed else 'REJECT_GAIN_OR_REGRESSION_GATE',
            'groups': groups, 'rows': rows, 'primary_pair_ratios': primary_pairs,
            'observed_primary_calibration_deviation': noise,
            'required_primary_gain_strictly_greater_than': required_gain,
            'candidate_cost_executions': 3, 'no_retry': True}


def cost_phase(run):
    pairs = [(run(f'cal-{i}-a', 'baseline'), run(f'cal-{i}-b', 'baseline')) for i in range(3)]
    passed, records = calibration(pairs)
    if not passed:
        return {'decision': 'TERMINAL_UNUSABLE_CALIBRATION', 'calibration': records,
                'candidate_cost_executions': 0, 'no_retry': True}
    pairs = []
    for i, order in enumerate((('baseline', 'candidate'), ('candidate', 'baseline'),
                               ('baseline', 'candidate'))):
        values = {arm: run(f'cost-{i}-{arm}', arm) for arm in order}
        pairs.append((values['baseline'], values['candidate']))
    return {**assess(pairs, records), 'calibration': records}


def benchmark_args(binary, directory, commit, spec, clock=False):
    args = [binary, '--out', directory, '--source-commit', commit, '--suite', spec['suite'],
            '--rate', str(spec['rate']), '--block', str(spec['block']),
            '--callbacks', str(spec['callbacks'])]
    if spec['quick']:
        args.append('--quick')
    if clock:
        args.append('--callback-cpu')
    return args


def qualification_source(text):
    require(text.count('engine.render(audio.data(), c.block);') == 2, 'render call anchors')
    text = '#include "qualification_fp.h"\n' + text.replace(
        'engine.render(audio.data(), c.block);',
        'strict_pgo_research::render(engine, audio.data(), c.block);')
    old = '        require(fs::create_directories(directory), "Cannot create output directory");\n'
    new = old + '''        strict_pgo_research::pcm.open(directory / "qualification.pcm", std::ios::binary);
        strict_pgo_research::fp.open(directory / "qualification.fp", std::ios::binary);
        require(bool(strict_pgo_research::pcm) && bool(strict_pgo_research::fp), "Cannot open qualification transcripts");
'''
    require(text.count(old) == 1, 'qualification directory anchor')
    text = text.replace(old, new, 1)
    old = '        complete.close();\n        return 0;'
    new = '''        complete.close();
        strict_pgo_research::pcm.close(); strict_pgo_research::fp.close();
        require(bool(strict_pgo_research::pcm) && bool(strict_pgo_research::fp), "Transcript close failed");
        auto receipt = output(directory / "qualification-receipt.json");
        receipt << "{\\"calls\\":" << strict_pgo_research::calls << ",\\"frames\\":" << strict_pgo_research::frames << "}\\n";
        return 0;'''
    require(text.count(old) == 1, 'qualification receipt anchor')
    return text.replace(old, new, 1)


def compare_transcripts(a, b):
    ra = json.loads((a / 'qualification-receipt.json').read_text())
    rb = json.loads((b / 'qualification-receipt.json').read_text())
    require(ra == rb and ra['calls'] > 0 and ra['frames'] > 0, 'qualification receipt')
    for name, size in [('qualification.pcm', ra['frames'] * 8),
                       ('qualification.fp', ra['calls'] * 24)]:
        require((a / name).stat().st_size == size and (b / name).stat().st_size == size,
                'transcript size')
        require(digest(a / name) == digest(b / name), name + ' mismatch')
    return {**ra, 'pcm_sha256': digest(a / 'qualification.pcm'),
            'fp_sha256': digest(a / 'qualification.fp')}


def profile_manifest(directory):
    files = sorted(directory.rglob('*'))
    files = [p for p in files if p.is_file()]
    require(len(files) == 1 and files[0].name.endswith('Engine.gcda'), 'exactly one Engine profile required')
    require(files[0].stat().st_size > 16 and files[0].read_bytes()[:4] in (b'adcg', b'gcda'),
            'invalid profile header')
    return {str(p.relative_to(directory)): {'bytes': p.stat().st_size, 'sha256': digest(p)} for p in files}


def optimizer_states(text):
    names = ('-fassociative-math', '-ffinite-math-only', '-fmath-errno',
             '-fsigned-zeros', '-funsafe-math-optimizations', '-ffp-contract=[off|on|fast]')
    values = {}
    for line in text.splitlines():
        fields = line.split()
        if fields and fields[0] in names:
            values[fields[0]] = fields[-1]
    require(set(values) == set(names), 'missing FP optimizer state')
    require(all(values[n] == '[disabled]' for n in
                ('-fassociative-math', '-ffinite-math-only', '-funsafe-math-optimizations')),
            'unsafe FP option')
    require(values['-fmath-errno'] == values['-fsigned-zeros'] == '[enabled]', 'FP semantics relaxed')
    return values


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    repo, out, work = args.repo.resolve(), args.out.resolve(), args.work.resolve()
    out.mkdir(parents=True, exist_ok=False); work.mkdir(parents=True, exist_ok=False)
    deadline = time.monotonic() + 2400
    cpu = min(os.sched_getaffinity(0))
    ledger, inputs = [], {}
    decision = {'decision': 'STOP_NO_RETRY', 'no_retry': True}

    def command(label, cmd, limit=None):
        remaining = deadline - time.monotonic()
        require(remaining > 0, 'total deadline')
        cmd = list(map(str, cmd))
        save(out / (label + '-command.json'), cmd)
        entry = {'label': label, 'command': cmd, 'started': True, 'exit': None}
        ledger.append(entry); save(out / 'execution-ledger.json', ledger)
        wrap = ('import os,sys;c=int(sys.argv[1]);os.sched_setaffinity(0,{c});'
                'assert os.sched_getaffinity(0)=={c};'
                'print("verified_child_affinity="+str(c),flush=True);os.execvp(sys.argv[2],sys.argv[2:])')
        try:
            with (out / (label + '.log')).open('w') as stream:
                process = subprocess.run(['python3', '-c', wrap, str(cpu), *cmd], stdout=stream,
                                         stderr=subprocess.STDOUT,
                                         timeout=min(remaining, limit) if limit else remaining)
            entry['exit'] = process.returncode
        except BaseException as error:
            entry['error'] = repr(error)
            raise
        finally:
            save(out / 'execution-ledger.json', ledger)
            save(out / (label + '-exit.json'), entry)
        require(process.returncode == 0, label + ' failed')
        return (out / (label + '.log')).read_text()

    def unchanged(paths):
        require(all(digest(path) == value for path, value in paths.items()), 'frozen file drift')

    try:
        require(platform.machine() == 'x86_64' and platform.system() == 'Linux', 'qualified platform')
        for path, expected in OBJECTS.items():
            actual = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD:' + path], text=True).strip()
            require(actual == expected, 'source drift ' + path)
        require(not subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain', '--', *OBJECTS], text=True).strip(), 'dirty source')
        commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
        compiler = subprocess.check_output(['g++', '--version'], text=True)
        version = subprocess.check_output(['g++', '-dumpfullversion'], text=True).strip()
        require(version == COMPILER_VERSION, 'pinned compiler version mismatch')
        require('gcc' in subprocess.check_output(['g++', '-v'], stderr=subprocess.STDOUT, text=True).lower(), 'GNU compiler required')
        save(out / 'metadata.json', {'commit': commit, 'compiler': compiler, 'compiler_version': version,
            'compiler_sha256': digest(Path(shutil.which('g++')).resolve()), 'platform': platform.platform(),
            'cpuinfo': Path('/proc/cpuinfo').read_text(), 'affinity': sorted(os.sched_getaffinity(0)),
            'selected_cpu': cpu, 'runner_name': os.environ.get('RUNNER_NAME'),
            'image': os.environ.get('ImageVersion'), 'source_objects': OBJECTS,
            'research_hashes': {p.name: digest(p) for p in ROOT.iterdir() if p.is_file()},
            'started_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())})
        source = work / 'source'; shutil.copytree(repo / 'src', source)
        common, baseline, optimized, profiles = [work / x for x in ('common', 'baseline', 'optimized', 'profiles')]
        for directory in (common, baseline, optimized, profiles): directory.mkdir()
        include = ['-I' + str(source), '-I' + str(source / 'tools'), '-I' + str(ROOT)]
        pgo_flags = ['-fprofile-use=' + str(profiles), '-Werror=missing-profile', '-Werror=coverage-mismatch']
        before = optimizer_states(command('baseline-fp-options', ['g++', '-Q', '-O3', '--help=optimizers']))
        after = optimizer_states(command('pgo-fp-options', ['g++', '-Q', '-O3', *pgo_flags, '--help=optimizers']))
        require(before == after, 'FP option drift'); save(out / 'fp-options.json', before)
        for unit in ('FactoryPresets', 'Ig00156Batch'):
            extra = ['-ffp-contract=off'] if unit == 'Ig00156Batch' else []
            command('compile-common-' + unit, ['g++', *FLAGS, *extra, *include, '-c', source / 'core' / (unit + '.cpp'), '-o', common / (unit + '.o')])
        driver = source / 'tools/source_factory_benchmark.cpp'
        qualified = work / 'qualification.cpp'; qualified.write_text(qualification_source(driver.read_text()))
        for name, path in [('benchmark', driver), ('qualification', qualified)]:
            command('compile-common-' + name, ['g++', *FLAGS, *include, '-c', path, '-o', common / (name + '.o')])
        ordinary = {p: digest(p) for p in common.glob('*.o')}
        shared_objects = [common / 'FactoryPresets.o', common / 'Ig00156Batch.o']
        command('compile-baseline-engine', ['g++', *FLAGS, *include, '-c', source / 'core/Engine.cpp', '-o', baseline / 'Engine.o'])
        for name in ('benchmark', 'qualification'):
            command('link-baseline-' + name, ['g++', common / (name + '.o'), baseline / 'Engine.o', *shared_objects, '-o', baseline / name])
        # Same source/object path for generation and use; only Engine.cpp is instrumented.
        command('compile-training-engine', ['g++', *FLAGS, *include, '-fprofile-generate=' + str(profiles), '-c', source / 'core/Engine.cpp', '-o', optimized / 'Engine.o'])
        command('link-training', ['g++', '-fprofile-generate=' + str(profiles), common / 'benchmark.o', optimized / 'Engine.o', *shared_objects, '-o', work / 'training'])
        require(not list(profiles.iterdir()), 'training profiles not empty')
        save(out / 'training-binary-hash.json', {'sha256': digest(work / 'training'), 'engine_sha256': digest(optimized / 'Engine.o')})
        command('training-once', benchmark_args(work / 'training', out / 'training', commit, TRAIN), 300)
        read_rows(out / 'training', TRAIN)
        frozen_profile = profile_manifest(profiles)
        save(out / 'training-profile-manifest.json', frozen_profile)
        # Preserve raw profile before any held-out observation; no more training calls exist.
        shutil.copytree(profiles, out / 'training-profile')
        command('compile-pgo-engine', ['g++', *FLAGS, *include, *pgo_flags, '-c', source / 'core/Engine.cpp', '-o', optimized / 'Engine.o'])
        require(profile_manifest(profiles) == frozen_profile, 'profile mutated by compilation')
        for name in ('benchmark', 'qualification'):
            command('link-candidate-' + name, ['g++', common / (name + '.o'), optimized / 'Engine.o', *shared_objects, '-o', optimized / name])
        for arm, directory in [('baseline', baseline), ('candidate', optimized)]:
            inputs[directory / 'Engine.o'] = digest(directory / 'Engine.o')
            for name in ('benchmark', 'qualification'):
                symbols = command('symbols-' + arm + '-' + name, ['nm', directory / name])
                require('__gcov' not in symbols and '__llvm_profile' not in symbols, 'profile generation in assessment binary')
                inputs[directory / name] = digest(directory / name)
        unchanged(ordinary)
        save(out / 'ordinary-object-hashes.json', {str(p.relative_to(work)): h for p, h in ordinary.items()})
        save(out / 'binary-hashes.json', {str(p.relative_to(work)): h for p, h in inputs.items()})
        qualification = {}
        for spec in (QUAL_BASE, QUAL):
            paths = {}
            for arm, directory in [('baseline', baseline), ('candidate', optimized)]:
                path = out / ('qualification-' + spec['suite'] + '-' + arm); paths[arm] = path
                command(path.name, benchmark_args(directory / 'qualification', path, commit, spec), 300)
            exact(read_rows(paths['baseline'], spec), read_rows(paths['candidate'], spec))
            qualification[spec['suite']] = compare_transcripts(paths['baseline'], paths['candidate'])
        save(out / 'qualification-summary.json', qualification)
        # Unchanged regression drivers, identical ordinary object reused in both links.
        for name, limit in REGRESSIONS:
            obj = common / (name + '.o')
            command('compile-test-' + name, ['g++', *FLAGS, *include, '-c', repo / 'tests' / (name + '.cpp'), '-o', obj])
            ordinary[obj] = digest(obj)
            logs = []
            for arm, directory in [('baseline', baseline), ('candidate', optimized)]:
                binary = directory / name
                command('link-test-' + name + '-' + arm, ['g++', obj, directory / 'Engine.o', *shared_objects, '-o', binary])
                args = [binary]
                if name == 'FilterBatchEngineTests': args += ['--write-corpus', out / ('filter-corpus-' + arm + '.bin')]
                logs.append(command('test-' + name + '-' + arm, args, limit))
            require(logs[0] == logs[1], 'regression result differs: ' + name)
        save(out / 'ordinary-object-hashes.json', {str(p.relative_to(work)): h for p, h in ordinary.items()})
        a, b = out / 'filter-corpus-baseline.bin', out / 'filter-corpus-candidate.bin'
        require(a.stat().st_size > 0 and a.stat().st_size == b.stat().st_size and digest(a) == digest(b), 'filter/state corpus mismatch')
        save(out / 'filter-corpus.json', {'bytes': a.stat().st_size, 'sha256': digest(a)})
        unchanged(ordinary); unchanged(inputs)
        require(profile_manifest(profiles) == frozen_profile, 'profile changed after training')
        reference = None
        def run(name, arm):
            nonlocal reference
            unchanged(inputs); unchanged(ordinary)
            require(profile_manifest(profiles) == frozen_profile, 'profile changed before timing')
            path = out / name
            directory = baseline if arm == 'baseline' else optimized
            command(name, benchmark_args(directory / 'benchmark', path, commit, COST, True), 300)
            rows = read_rows(path, COST, True)
            if reference is None: reference = rows
            else: exact(reference, rows)
            return rows
        decision = cost_phase(run)
        unchanged(inputs); unchanged(ordinary)
        require(profile_manifest(profiles) == frozen_profile, 'profile changed during timing')
        require(time.monotonic() <= deadline, 'final deadline')
    except BaseException as error:
        decision = {'decision': 'STOP_NO_RETRY', 'error': repr(error), 'no_retry': True}
    finally:
        candidates = [x for x in ledger if x['label'].startswith('cost-') and x['label'].endswith('-candidate')]
        decision['candidate_cost_started'] = len(candidates)
        decision['candidate_cost_completed'] = sum(x['exit'] == 0 for x in candidates)
        decision['training_started'] = sum(x['label'] == 'training-once' for x in ledger)
        save(out / 'decision.json', decision)
        print(json.dumps(decision), flush=True)
    return 0 if decision['decision'] in ('SCOPED_COST_PASS', 'TERMINAL_UNUSABLE_CALIBRATION', 'REJECT_GAIN_OR_REGRESSION_GATE') else 1


if __name__ == '__main__':
    raise SystemExit(main())
