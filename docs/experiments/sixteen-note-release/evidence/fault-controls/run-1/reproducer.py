#!/usr/bin/env python3
"""Bounded behavioral controls: immutable production/tests, private source copies.

Usage: python check_fault_controls.py REPO MATCHING_BUILD FRESH_OUTPUT
The fresh output must not exist. Compile operations run sequentially. No timing
or performance result is collected. A crash, timeout, build failure, incomplete
summary, or unexpected exit is an invalid experiment, never a detected mutant.
"""
from pathlib import Path
import difflib
import hashlib
import json
import re
import shlex
import shutil
import subprocess
import sys

FROZEN = '1a23a085657a07d9f066f7cc38aaf2572a6fe714'
CHECKS = 1418
SUMMARY = re.compile(r'Immediate sixteen-voice release: (\d+) checks, (\d+) failures')
PREFIX = 'Immediate sixteen-voice release:'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_summary(stdout, stderr, code, expected):
    """Accept exactly one complete terminal summary, full fixture count, normal exit.

    Also require each numerical failure to have one FAIL line. This detects
    truncation, appended diagnostics, duplicate/malformed summaries and crashes.
    """
    lines = stdout.splitlines()
    candidates = [line for line in lines if line.startswith(PREFIX)]
    if stderr or len(candidates) != 1 or not lines or candidates[0] != lines[-1]:
        raise ValueError('stderr, missing/duplicate/nonterminal summary')
    match = SUMMARY.fullmatch(lines[-1])
    if not match:
        raise ValueError('malformed terminal summary')
    checks, failures = map(int, match.groups())
    if checks != CHECKS or not 0 <= failures <= checks:
        raise ValueError('incomplete check count or impossible failure count')
    if len(lines) - 1 != failures or not all(line.startswith('FAIL: ') for line in lines[:-1]):
        raise ValueError('failure diagnostic count/content mismatch')
    if code != expected or (expected == 0 and failures != 0) or (expected == 1 and failures == 0):
        raise ValueError('unexpected exit or failure count')
    return {'checks': checks, 'failures': failures, 'exit_code': code}


def parser_self_test():
    ok = f'{PREFIX} {CHECKS} checks, 0 failures\n'
    bad = 'FAIL: injected control\n' + f'{PREFIX} {CHECKS} checks, 1 failures\n'
    parse_summary(ok, '', 0, 0)
    parse_summary(bad, '', 1, 1)
    rejects = {
        'empty': ('', '', 1, 1),
        'missing': ('FAIL: truncated\n', '', 1, 1),
        'duplicate': (ok + ok, '', 0, 0),
        'trailing_output': (bad + 'late text\n', '', 1, 1),
        'wrong_count': (bad.replace('1418', '1417'), '', 1, 1),
        'crash': (bad, '', -11, 1),
        'wrong_exit': (bad, '', 0, 1),
        'successful_mutant': (ok, '', 0, 1),
        'stderr': (bad, 'diagnostic', 1, 1),
        'missing_failure_lines': (f'{PREFIX} {CHECKS} checks, 1 failures\n', '', 1, 1),
        'malformed': (bad.replace('1 failures', 'one failures'), '', 1, 1),
        'extra_blank_terminal_line': (bad + '\n', '', 1, 1),
    }
    for name, args in rejects.items():
        try:
            parse_summary(*args)
        except ValueError:
            continue
        raise RuntimeError('parser incorrectly accepted ' + name)
    return {'valid_cases': 2, 'rejected_invalid_cases': list(rejects)}


ROUTER_GUARD = '''        if (keyboardOnly && !chosen.keyboardOrigin)
            return true; // Never fall through to an older UI-owned group.
'''
ENGINE_GUARD = '''        if (keyboardOnly && !voice->keyboardOrigin)
            return;
'''
SELECT = '''        const bool chooseSecond = a.kind == Kind::none || (b.kind != Kind::none
            && ((b.kind == Kind::waiting && a.kind != Kind::waiting)
                || (a.kind == b.kind && b.order > a.order)));'''
RELEASE = '''        auto& target = chooseSecond ? second : first;
        if (keyboardOnly) target.noteOffFromKeyboard(note, channel);
        else target.noteOff(note, channel);'''
MUTATIONS = [
    ('broadcast-two-engines', 'Reintroduce broadcast instead of releasing one selected onset.', [
        ('core/VoiceModes.h', RELEASE, '''        if (keyboardOnly)
        { first.noteOffFromKeyboard(note, channel); second.noteOffFromKeyboard(note, channel); }
        else
        { first.noteOff(note, channel); second.noteOff(note, channel); }''')]),
    ('inverse-global-order', 'Choose older comparable group instead of newest global order.', [
        ('core/VoiceModes.h', 'b.order > a.order', 'b.order < a.order')]),
    ('lost-waiter-priority', 'Use order alone, allowing newer sounding group to outrank waiter.', [
        ('core/VoiceModes.h', SELECT, '''        const bool chooseSecond = a.kind == Kind::none
            || (b.kind != Kind::none && b.order > a.order);''')]),
    ('ui-host-protection-omitted', 'Omit UI host protection at both routing and existing execution guards; two-site mutation is explicit.', [
        ('core/VoiceModes.h', ROUTER_GUARD, ''),
        ('core/Engine.cpp', ENGINE_GUARD, '')]),
    ('ui-held-source-filter', 'Incorrectly filter host-owned held candidates during UI preview, permitting cross-engine fall-through.', [
        ('core/Engine.cpp', '''        if (voice.active && voice.held && voice.note == note && voice.midiChannel == channel
            && (newest == nullptr || voice.age > newest->age))''', '''        if (voice.active && voice.held && voice.note == note && voice.midiChannel == channel
            && (!keyboardOnly || voice.keyboardOrigin)
            && (newest == nullptr || voice.age > newest->age))''')]),
    ('qualify-prior-deferred-candidates', 'Assign shared order to allocations executed in deferred/ARP context.', [
        ('core/Engine.h', '{ return immediate && releaseOrderClock != nullptr ? releaseOrderClock->next() : 0; }',
         '{ (void) immediate; return releaseOrderClock != nullptr ? releaseOrderClock->next() : 0; }')]),
    ('accept-unqualified-candidate', 'Remove pair-wide veto for matching candidate with unknown order.', [
        ('core/VoiceModes.h', '''        if ((a.kind != Kind::none && a.order == 0) || (b.kind != Kind::none && b.order == 0))
            return false;
''', '')]),
    ('accept-mismatched-sources', 'Accept distinct non-null clocks as comparable ownership sources.', [
        ('core/Engine.cpp', 'releaseOrderClock != nullptr && releaseOrderClock == other.releaseOrderClock',
         'releaseOrderClock != nullptr && other.releaseOrderClock != nullptr')]),
    ('omit-exhaustion-guard', 'Remove max-serial guard and exhaustion latch; unsigned serial can wrap.', [
        ('core/ReleaseOrder.h', '''        if (serial == std::numeric_limits<std::uint64_t>::max())
        {
            exhausted = true;
            return 0;
        }
''', '')]),
    ('ignore-pending-scanner', 'Coordinate despite existing pending scanner events.', [
        ('core/Engine.cpp', '''        && !scanTimingActive() && !other.scanTimingActive()
        && scanner.pending() == 0 && other.scanner.pending() == 0;''',
         '''        && !scanTimingActive() && !other.scanTimingActive();''')]),
    ('reuse-waiter-order-on-promotion', 'Preserve waiter order rather than giving immediate promotion fresh order.', [
        ('core/Engine.cpp', 'startVoice(voices[(size_t) idx], key.note, 0.0f, key.midiChannel, key.keyboardOrigin, nextReleaseOrder(immediate));',
         'startVoice(voices[(size_t) idx], key.note, 0.0f, key.midiChannel, key.keyboardOrigin, key.coordinatedReleaseOrder);')]),
]
# Deliberately retain this literal deletion as an honest non-detection control.
# Engine::applyNoteOff already enforces the same host-owned UI protection.
REDUNDANT = ('omit-router-ui-guard-only',
             'Literal router guard deletion is redundant with Engine execution guard; not counted as detection.',
             [('core/VoiceModes.h', ROUTER_GUARD, '')])


def main():
    repo, build, out = [Path(p).resolve() for p in sys.argv[1:]]
    out.mkdir(parents=True, exist_ok=False)
    records = []

    def run(label, argv, timeout=240):
        argv = [str(x) for x in argv]
        folder = out / 'logs'
        folder.mkdir(exist_ok=True)
        (folder / (label + '.command.json')).write_text(json.dumps({'argv': argv, 'cwd': str(out)}, indent=2) + '\n')
        try:
            p = subprocess.run(argv, cwd=out, capture_output=True, text=True, timeout=timeout)
            code, stdout, stderr = p.returncode, p.stdout, p.stderr
        except subprocess.TimeoutExpired as e:
            def decode(x):
                return x.decode(errors='replace') if isinstance(x, bytes) else (x or '')
            code, stdout, stderr = 'timeout', decode(e.stdout), decode(e.stderr)
        stdout_path = folder / (label + '.stdout.log')
        stderr_path = folder / (label + '.stderr.log')
        stdout_path.write_text(stdout)
        stderr_path.write_text(stderr)
        records.append({'label': label, 'argv': argv, 'cwd': str(out), 'exit_code': code,
                        'stdout': str(stdout_path.relative_to(out)), 'stdout_sha256': sha(stdout_path),
                        'stderr': str(stderr_path.relative_to(out)), 'stderr_sha256': sha(stderr_path)})
        (out / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
        return code, stdout, stderr

    def require_run(label, argv):
        code, stdout, stderr = run(label, argv)
        if code != 0:
            raise RuntimeError(f'{label}: build/inspection invalid; exit {code}, not mutation detection')
        return stdout

    tracked = require_run('tracked-inputs', ['git', '-C', repo, 'ls-files', 'src', 'tests']).splitlines()
    inputs = {p: sha(repo / p) for p in tracked}
    if require_run('head', ['git', '-C', repo, 'rev-parse', 'HEAD']).strip() != FROZEN:
        raise RuntimeError('repository HEAD differs from frozen input')
    require_run('frozen-source-diff', ['git', '-C', repo, 'diff', '--exit-code', FROZEN, '--', 'src', 'tests', 'CMakeLists.txt'])
    archive = build / 'libcs80_core.a'
    archive_sha = sha(archive)
    original_object = build / 'CMakeFiles/cs80_core.dir/src/core/Engine.cpp.o'
    original_object_sha = sha(original_object)
    (out / 'input-hashes.json').write_text(json.dumps({'commit': FROZEN, 'source_test_sha256': inputs,
        'archive_path': str(archive), 'archive_sha256': archive_sha,
        'original_engine_object_sha256': original_object_sha}, indent=2) + '\n')
    shutil.copyfile(__file__, out / 'reproducer.py')
    shutil.copyfile(repo / 'docs/experiments/sixteen-note-release/PLAN.md', out / 'PLAN.md')
    shutil.copyfile(build / 'compile_commands.json', out / 'compile_commands.original.json')
    shutil.copyfile(build / 'build.ninja', out / 'build.original.ninja')
    shutil.copyfile(build / 'CMakeFiles/rules.ninja', out / 'rules.original.ninja')
    shutil.copyfile(build / 'CMakeCache.txt', out / 'CMakeCache.original.txt')
    require_run('compiler-version', ['/usr/bin/c++', '--version'])
    require_run('archive-members', ['/usr/bin/ar', 't', archive])
    # Hash the archive's Engine member without decoding arbitrary binary data.
    member = subprocess.run(['/usr/bin/ar', 'p', str(archive), 'Engine.cpp.o'], check=True, capture_output=True).stdout
    if hashlib.sha256(member).hexdigest() != original_object_sha:
        raise RuntimeError('archive Engine member does not match build object')
    (out / 'archive-member-verification.json').write_text(json.dumps({
        'argv': ['/usr/bin/ar', 'p', str(archive), 'Engine.cpp.o'], 'exit_code': 0,
        'member_sha256': hashlib.sha256(member).hexdigest(), 'matches_build_object': True}, indent=2) + '\n')
    (out / 'parser-self-test.json').write_text(json.dumps(parser_self_test(), indent=2) + '\n')
    frozen_src = out / 'frozen' / 'src'
    shutil.copytree(repo / 'src', frozen_src)
    frozen_test = out / 'frozen' / 'SixteenVoiceReleaseTests.cpp'
    shutil.copyfile(repo / 'tests/SixteenVoiceReleaseTests.cpp', frozen_test)
    db = json.loads((build / 'compile_commands.json').read_text())
    recipes = {Path(x['file']).name: x['command'] for x in db}

    def compile_command(name, source, obj, private_src):
        tokens = shlex.split(recipes[name])
        tokens[tokens.index('-o') + 1] = str(obj)
        tokens[tokens.index('-c') + 1] = str(source)
        include = '-I' + str(repo / 'src')
        if tokens.count(include) != 1:
            raise RuntimeError('unexpected compile recipe includes')
        tokens[tokens.index(include)] = '-I' + str(private_src)
        return tokens

    summary = {'commit': FROZEN, 'checks_expected': CHECKS, 'compiler': 'GCC generic Release; AVX2 batch disabled',
               'compile_parallelism': 1, 'control': None, 'mutations': {}, 'redundancy_control': None}
    cases = [('unmutated-control', 'Exact copied source and unchanged test fixture.', [], 0)]
    cases += [(*case, 1) for case in MUTATIONS]
    cases += [(*REDUNDANT, 0)]
    for name, description, edits, expected in cases:
        folder = out / name
        private_src = folder / 'src'
        shutil.copytree(frozen_src, private_src)
        patches = []
        for rel, before, after in edits:
            path = private_src / rel
            old = path.read_text()
            if old.count(before) != 1:
                raise RuntimeError(f'{name}: nonunique replacement in {rel}')
            new = old.replace(before, after)
            path.write_text(new)
            patches.extend(difflib.unified_diff(old.splitlines(True), new.splitlines(True),
                           fromfile='baseline/src/' + rel, tofile=name + '/src/' + rel))
        (folder / 'mutation.diff').write_text(''.join(patches))
        (folder / 'description.txt').write_text(description + '\n')
        (folder / 'source-hashes.json').write_text(json.dumps({str(p.relative_to(private_src)): sha(p)
            for p in sorted(private_src.rglob('*')) if p.is_file()}, indent=2) + '\n')
        engine_obj, test_obj, binary = folder / 'Engine.o', folder / 'tests.o', folder / 'probe'
        require_run(name + '-engine-compile', compile_command('Engine.cpp', private_src / 'core/Engine.cpp', engine_obj, private_src))
        require_run(name + '-test-compile', compile_command('SixteenVoiceReleaseTests.cpp', frozen_test, test_obj, private_src))
        require_run(name + '-link', ['/usr/bin/c++', '-O3', '-DNDEBUG', test_obj, engine_obj,
                                    archive, '-Wl,-Map=' + str(folder / 'link.map'), '-o', binary])
        linkmap = (folder / 'link.map').read_text()
        if 'libcs80_core.a(Engine.cpp.o)' in linkmap:
            raise RuntimeError(name + ': old Engine archive member unexpectedly selected')
        code, stdout, stderr = run(name + '-run', [binary], timeout=120)
        result = {'description': description, 'expected_exit': expected, 'diff_sha256': sha(folder / 'mutation.diff'),
                  'engine_object_sha256': sha(engine_obj), 'test_object_sha256': sha(test_obj),
                  'binary_sha256': sha(binary), 'link_map_sha256': sha(folder / 'link.map'),
                  'archive_engine_member_linked': False}
        try:
            result.update(parse_summary(stdout, stderr, code, expected))
            result['status'] = 'detected' if expected == 1 else ('passed' if name == 'unmutated-control' else 'undetected-redundant')
        except ValueError as e:
            result.update({'status': 'invalid-or-undetected', 'validation_error': str(e), 'exit_code': code})
        if name == 'unmutated-control':
            summary['control'] = result
            if result['status'] != 'passed':
                (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
                raise RuntimeError('unmutated control did not pass exact original fixture')
        elif name == REDUNDANT[0]:
            summary['redundancy_control'] = result
        else:
            summary['mutations'][name] = result
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        print(name, result['status'], result.get('checks'), result.get('failures'), 'exit', code, flush=True)
    after = {p: sha(repo / p) for p in tracked}
    invariant = {'production_and_test_hashes_unchanged': after == inputs, 'archive_sha256_unchanged': sha(archive) == archive_sha,
                 'matching_build_engine_object_unchanged': sha(original_object) == original_object_sha,
                 'test_fixture_sha256': sha(frozen_test), 'original_test_fixture_sha256': inputs['tests/SixteenVoiceReleaseTests.cpp']}
    (out / 'after-invariants.json').write_text(json.dumps(invariant, indent=2) + '\n')
    summary['inputs_unchanged'] = all(invariant[k] for k in ['production_and_test_hashes_unchanged', 'archive_sha256_unchanged', 'matching_build_engine_object_unchanged'])
    summary['detected_count'] = sum(x['status'] == 'detected' for x in summary['mutations'].values())
    summary['gate_passed'] = summary['inputs_unchanged'] and summary['detected_count'] == len(MUTATIONS)
    (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('GATE', 'PASS' if summary['gate_passed'] else 'FAIL', summary['detected_count'], '/', len(MUTATIONS), flush=True)
    return 0 if summary['gate_passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
