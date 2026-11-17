#!/usr/bin/env python3
"""Prospective configuration-only preflight; never build/train/run the synth.

A tiny unrelated integer-only compiler profile plumbing fixture is also run.
CMake's compiler probes and JUCE's own configure-time helper are permitted setup,
not production build/test execution. ninja -t commands only emits build commands.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import time

from provenance import require, sha256, verify_compile_scope, profile_manifest

ROOT = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('strict_pgo_contract', ROOT.parent / 'strict-pgo-research/run.py')
STRICT = importlib.util.module_from_spec(SPEC); SPEC.loader.exec_module(STRICT)


def save(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n')


def cache_values(text):
    result = {}
    for line in text.splitlines():
        if not line or line.startswith(('#', '//')) or '=' not in line or ':' not in line.split('=', 1)[0]:
            continue
        key, value = line.split('=', 1); result[key.split(':', 1)[0]] = value
    return result


def tree_manifest(root):
    result = {}
    for path in sorted(Path(root).rglob('*')):
        rel = path.relative_to(root)
        if '.git' in rel.parts: continue
        if path.is_symlink(): result[str(rel)] = {'symlink': os.readlink(path)}
        elif path.is_file(): result[str(rel)] = {'bytes': path.stat().st_size, 'sha256': sha256(path)}
    require(bool(result), 'empty JUCE working tree')
    return result


def test_inventory(document):
    tests = document.get('tests', [])
    require(len(tests) == 569, 'frozen full CTest roster must contain 569 tests')
    result = {}
    for test in tests:
        require(test['name'] not in result, 'duplicate CTest name')
        # CTest may omit an executable command before that executable is built.
        # Record absence explicitly; exact executable argv is mandatory after
        # baseline build and before training in the later trial.
        result[test['name']] = {'command': test.get('command'), 'properties': test.get('properties', [])}
    selected = {'cs80_vst3_sub_automation_tests'}
    for rate in (44100, 48000, 96000):
        for mode in ('realtime', 'offline'):
            selected.add(f'cs80_vst3_ring_automation_{rate}_{mode}_tests')
            if (rate, mode) != (48000, 'realtime'):
                selected.add(f'cs80_vst3_sub_automation_{rate}_{mode}_tests')
    require(selected <= result.keys(), 'selected Sub/Ring roster missing')
    for name in selected:
        properties = {item['name']: item['value'] for item in result[name]['properties']}
        require(properties.get('TIMEOUT') == 180, 'selected host timeout changed')
    return result


def compiler_plumbing(driver, work, out, command):
    """One unrelated integer toy execution; no synth source/training/timing."""
    root = work / 'compiler-plumbing'; root.mkdir()
    profiles = root / 'profiles'; profiles.mkdir()
    missing = root / 'missing'; missing.mkdir()
    obj = root / 'CMakeFiles/toy.dir/src/Engine.cpp.o'; obj.parent.mkdir(parents=True)
    source = root / 'Engine.cpp'; main = root / 'main.cpp'
    original = 'int value(int x) { return x == 7 ? 4 : 2; }\n'
    source.write_text(original)
    main.write_text('int value(int); int main() { return value(7) == 4 ? 0 : 1; }\n')
    ordinary = root / 'main.o'; executable = root / 'integer-toy'
    common = [driver, '-std=c++20', '-O3', '-DNDEBUG', '-fPIC']
    command('toy-compile-driver', [*common, '-c', main, '-o', ordinary])
    command('toy-generate-object', [*common, '-fprofile-generate=' + str(profiles), '-c', source, '-o', obj])
    command('toy-link', [driver, '-fprofile-generate=' + str(profiles), ordinary, obj, '-o', executable])
    command('toy-integer-execute-once', [executable])
    frozen = profile_manifest(profiles)
    command('toy-use-same-object-path', [*common, '-fprofile-use=' + str(profiles),
            '-Werror=missing-profile', '-Werror=coverage-mismatch', '-c', source, '-o', obj])
    symbols = command('toy-use-symbols', ['nm', str(obj)])
    require('__gcov' not in symbols, 'toy profile-use object retains generation runtime')
    command('toy-missing-profile-negative', [*common, '-fprofile-use=' + str(missing),
            '-Werror=missing-profile', '-Werror=coverage-mismatch', '-c', source, '-o', obj], expect_success=False)
    require('missing-profile' in (out / 'toy-missing-profile-negative.stderr').read_text(),
            'missing-profile negative failed for wrong reason')
    source.write_text('int value(int x) { int sum=0; for(int i=0;i<x;++i) { if(i%3==0) sum+=i; } return sum; }\n')
    command('toy-mismatched-profile-negative', [*common, '-fprofile-use=' + str(profiles),
            '-Werror=missing-profile', '-Werror=coverage-mismatch', '-c', source, '-o', obj], expect_success=False)
    require('coverage-mismatch' in (out / 'toy-mismatched-profile-negative.stderr').read_text(),
            'coverage-mismatch negative failed for wrong reason')
    require(profile_manifest(profiles) == frozen, 'toy profile mutated after its sole execution')
    save(out / 'compiler-plumbing.json', {'scope': 'Unrelated integer-only toy, no synth training or timing',
         'profile': frozen, 'single_execution': True, 'same_object_path': str(obj),
         'missing_profile_rejected': True, 'coverage_mismatch_rejected': True})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    repo, work, out = args.repo.resolve(), args.work.resolve(), args.out.resolve()
    work.mkdir(parents=True, exist_ok=False); out.mkdir(parents=True, exist_ok=False)
    build = work / 'build'; profiles = work / 'profiles'; profiles.mkdir()
    deadline = time.monotonic() + 2400
    ledger = []
    def command(name, argv, expect_success=True):
        argv = list(map(str, argv)); remaining = deadline - time.monotonic()
        require(remaining > 0, 'preflight deadline')
        record = {'name': name, 'argv': argv, 'exit': None, 'expect_success': expect_success}; ledger.append(record)
        save(out / 'commands.json', ledger)
        try:
            x = subprocess.run(argv, cwd=repo, capture_output=True, text=True, timeout=remaining)
            record['exit'] = x.returncode
            (out / (name + '.stdout')).write_text(x.stdout)
            (out / (name + '.stderr')).write_text(x.stderr)
            require((x.returncode == 0) if expect_success else (x.returncode != 0), name + ' unexpected exit')
            return x.stdout
        finally: save(out / 'commands.json', ledger)
    result = {'decision': 'PREFLIGHT_BLOCKED', 'project_training': False, 'project_cost': False}
    try:
        require(platform.system() == 'Linux' and platform.machine() == 'x86_64', 'Linux x86_64 required')
        for path, expected in STRICT.OBJECTS.items():
            require(command('object-' + path.replace('/', '-'), ['git', 'rev-parse', 'HEAD:' + path]).strip() == expected,
                    'production source object drift ' + path)
        require(not command('source-clean', ['git', 'status', '--porcelain', '--', *STRICT.OBJECTS]).strip(), 'dirty production source')
        save(out / 'metadata.json', {'commit': command('commit', ['git', 'rev-parse', 'HEAD']).strip(),
             'source_objects': STRICT.OBJECTS, 'platform': platform.platform(),
             'research_files': {p.name: sha256(p) for p in ROOT.iterdir() if p.is_file()},
             'cmake': command('cmake-version', ['cmake', '--version']),
             'ninja': command('ninja-version', ['ninja', '--version'])})
        normal = None; compiler_identity = None; juce_identity = None; roster = None; testfiles = None
        for mode in ('normal', 'baseline', 'generate', 'use'):
            argv = ['cmake', '-S', repo, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                    '-DCS80_BUILD_PLUGIN=ON', '-DCS80_BUILD_TESTS=ON',
                    '-DCS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY=OFF', '-DCS80_ENABLE_AVX2_FILTER_BATCH=ON',
                    '-DCMAKE_PROJECT_INCLUDE=' + ('' if mode == 'normal' else str(ROOT / 'overlay.cmake')),
                    '-DCS80_PIC_PGO_MODE=' + mode, '-DCS80_PIC_PGO_PROFILE_DIR=' + str(profiles)]
            command(mode + '-configure', argv)
            command(mode + '-juce-verify', ['cmake', '-DBUILD_DIR=' + str(build),
                '-DEXPECTED_JUCE_SHA=c352e2489000ffeb9e397051377d75eef2efe641',
                '-DEXPECTED_JUCE_VERSION=8.0.11', '-P', repo / 'scripts/verify-ci-juce-source.cmake'])
            cache_text = (build / 'CMakeCache.txt').read_text(); cache = cache_values(cache_text)
            (out / (mode + '-CMakeCache.txt')).write_text(cache_text)
            identity = {}
            for language, internal in [('C', 'cc1'), ('CXX', 'cc1plus')]:
                driver = cache['CMAKE_' + language + '_COMPILER']
                version = command(mode + '-' + language + '-version', [driver, '-dumpfullversion']).strip()
                require(version == '13.3.0', 'frozen GNU compiler version')
                frontend = command(mode + '-' + language + '-frontend', [driver, '-print-prog-name=' + internal]).strip()
                resolved_frontend = Path(frontend) if Path(frontend).is_absolute() else Path(shutil.which(frontend) or '')
                require(resolved_frontend.is_file(), 'compiler frontend not found')
                identity[language] = {'command': driver, 'resolved': str(Path(driver).resolve()),
                                     'sha256': sha256(Path(driver).resolve()), 'version': version,
                                     'frontend': str(resolved_frontend.resolve()), 'frontend_sha256': sha256(resolved_frontend)}
            if compiler_identity is None: compiler_identity = identity
            require(identity == compiler_identity, 'compiler identity drift')
            save(out / 'compiler-identity.json', compiler_identity)
            database = json.loads((build / 'compile_commands.json').read_text())
            save(out / (mode + '-compile_commands.json'), database)
            if normal is None: normal = database
            scope = verify_compile_scope(normal, database, repo, profiles,
                 'baseline' if mode == 'normal' else mode,
                 expected_compiler=identity['CXX']['command'], expected_c_compiler=identity['C']['command'])
            require(not Path(scope['engine_object']).exists(), 'configuration unexpectedly compiled Engine')
            scope['engine_object_absent'] = True
            save(out / (mode + '-scope.json'), scope)
            juce = repo / 'deps/juce' if (repo / 'deps/juce/CMakeLists.txt').exists() else build / '_deps/juce-src'
            current_juce = tree_manifest(juce)
            if juce_identity is None: juce_identity = current_juce
            require(current_juce == juce_identity, 'patched JUCE working-tree drift')
            save(out / (mode + '-juce-tree.json'), current_juce)
            command(mode + '-juce-diff', ['git', '-C', juce, 'diff', '--binary', 'HEAD'])
            ctest_json = json.loads(command(mode + '-ctest-inventory', ['ctest', '--test-dir', build, '--show-only=json-v1']))
            current_roster = test_inventory(ctest_json)
            if roster is None: roster = current_roster
            require(current_roster == roster, 'full test inventory/commands/properties changed')
            save(out / (mode + '-test-roster.json'), current_roster)
            current_testfiles = {str(path.relative_to(build)): {'sha256': sha256(path), 'text': path.read_text()}
                                 for path in sorted(build.rglob('CTestTestfile.cmake'))}
            require(bool(current_testfiles), 'missing raw CTest definitions')
            if testfiles is None: testfiles = current_testfiles
            require(current_testfiles == testfiles, 'raw CTest definition drift')
            save(out / (mode + '-CTestTestfiles.json'), current_testfiles)
            for target in ('cs80_core', 'CS80Reference_VST3', 'CS80Reference_Standalone'):
                command(mode + '-commands-' + target, ['ninja', '-C', build, '-t', 'commands', target])
            require(not list(profiles.iterdir()), 'configuration produced unexpected profile data')
        require(not command('source-clean-after', ['git', 'status', '--porcelain', '--', *STRICT.OBJECTS]).strip(),
                'configuration changed production source')
        compiler_plumbing(compiler_identity['CXX']['command'], work, out, command)
        require(time.monotonic() <= deadline, 'final preflight deadline')
        result = {'decision': 'CONFIGURATION_PREFLIGHT_PASS', 'project_training': False, 'project_cost': False,
                  'full_test_count': len(roster), 'missing_prebuild_test_commands':
                  sum(row['command'] is None for row in roster.values()),
                  'integer_toolchain_plumbing': 'one unrelated toy execution plus positive/missing/mismatch checks',
                  'limit': 'Configuration/toolchain provenance only; no production build/link/host/performance qualification.'}
    except BaseException as error:
        result['error'] = repr(error)
    finally:
        save(out / 'decision.json', result); print(json.dumps(result), flush=True)
    return 0 if result['decision'] == 'CONFIGURATION_PREFLIGHT_PASS' else 1

if __name__ == '__main__': raise SystemExit(main())
