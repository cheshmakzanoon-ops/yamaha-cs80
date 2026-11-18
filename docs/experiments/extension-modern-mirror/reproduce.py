#!/usr/bin/env python3
"""Print a direct-test recipe; compile/run only with explicit --execute.

No timing entry, plugin build, native-plugin load, network access or publication.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shlex
import subprocess

HERE = Path(__file__).resolve().parent
FLAGS = ['-O3', '-DNDEBUG', '-std=gnu++20', '-fPIC']
NEGATIVES = {'fast': '-ffast-math', 'fma': '-mfma', 'finite': '-ffinite-math-only'}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_check(root, expected):
    for name, digest in expected.items():
        if sha(root / name) != digest:
            raise ValueError('Source differs from frozen input: ' + name)


def commands(cxx, stock, candidate, archive, output):
    oracle = output / 'StockOracle.o'
    mirror = output / 'ModernLayerMirror.o'
    rows = []
    rows.append(('compile-stock-oracle', [cxx, *FLAGS, '-I', str(stock / 'src'), '-c', str(HERE / 'StockOracle.cpp'), '-o', str(oracle)]))
    rows.append(('compile-mirror', [cxx, *FLAGS, '-I', str(candidate / 'src'), '-c', str(candidate / 'src/core/ModernLayerMirror.cpp'), '-o', str(mirror)]))
    for name in ('MirrorTests', 'TrapTests', 'GuardTests'):
        exe = output / name
        # Match the historical single-command compile/link. The guard never
        # needs the stock oracle; other drivers list it before the helper.
        objects = [str(oracle), str(mirror)] if name != 'GuardTests' else [str(mirror)]
        rows.append(('build-' + name, [cxx, *FLAGS, '-I', str(candidate / 'src'),
                    str(HERE / (name + '.cpp')), *objects, str(archive), '-o', str(exe)]))
        rows.append(('run-' + name, [str(exe)]))
    for label, extra in NEGATIVES.items():
        obj, exe = output / ('mirror-' + label + '.o'), output / ('guard-' + label)
        rows.append(('compile-negative-' + label, [cxx, *FLAGS, extra, '-I', str(candidate / 'src'), '-c', str(candidate / 'src/core/ModernLayerMirror.cpp'), '-o', str(obj)]))
        rows.append(('build-negative-' + label, [cxx, *FLAGS, '-I', str(candidate / 'src'),
                    str(HERE / 'GuardTests.cpp'), str(obj), str(archive), '-o', str(exe)]))
        rows.append(('run-negative-' + label, [str(exe), 'expect-none']))
    return rows


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--stock', type=Path, required=True, help='pristine checkout of the pinned stock commit')
    p.add_argument('--candidate', type=Path, required=True, help='separate checkout with candidate-v1.patch applied')
    p.add_argument('--archive', type=Path, required=True, help='unmodified pinned stock libcs80_core.a')
    p.add_argument('--output', type=Path, required=True, help='fresh direct-test output directory')
    p.add_argument('--cxx', default='c++')
    p.add_argument('--execute', action='store_true', help='explicitly compile and run all direct gates; no timing')
    args = p.parse_args()
    from verify import verify_package
    verify_package(HERE)
    inv = json.loads((HERE / 'source-inventory.json').read_text())
    provenance = json.loads((HERE / 'provenance.json').read_text())
    expected = json.loads((HERE / 'direct-results.json').read_text())['saved_stdout']
    stock, candidate, archive, output = (x.resolve() for x in (args.stock, args.candidate, args.archive, args.output))
    source_check(stock, inv['stock_sha256'])
    source_check(candidate, {**inv['stock_sha256'], **inv['candidate_overrides_sha256']})
    if sha(archive) != provenance['stock_core_archive_sha256']:
        raise ValueError('Archive does not match the qualified layered stock artifact. Rebuilding another archive is a new qualification, not this saved receipt.')
    recipe = commands(args.cxx, stock, candidate, archive, output)
    if not args.execute:
        for kind, command in recipe:
            print('# ' + kind + '\n' + shlex.join(command))
        print('# Plan only: no command was executed.')
        return
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'amd64'):
        raise ValueError('This bounded trap/FP fixture requires Linux x86-64, not a cross-platform result.')
    version = subprocess.check_output([args.cxx, '-dumpfullversion'], text=True).strip()
    if version != '14.2.0':
        raise ValueError('Saved gate is GCC14.2.0 only; a different compiler requires separate qualification.')
    macros = subprocess.check_output([args.cxx, '-dM', '-E', '-x', 'c++', '-'], input='', text=True)
    if '__clang__' in macros or '__GNUC__ 14' not in macros:
        raise ValueError('Expected GNU compiler, not another compiler accepting GCC options.')
    output.mkdir(parents=False, exist_ok=False)
    receipts = []
    expected_keys = {'run-MirrorTests': 'mirror', 'run-TrapTests': 'traps', 'run-GuardTests': 'guard-strict',
                     **{'run-negative-' + x: 'guard-' + x for x in NEGATIVES}}
    for kind, command in recipe:
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=60)
        (output / (kind + '.stdout')).write_text(result.stdout)
        (output / (kind + '.stderr')).write_text(result.stderr)
        item = {'step': kind, 'command': command, 'exit_code': result.returncode,
                'stdout_sha256': sha(output / (kind + '.stdout')), 'stderr_sha256': sha(output / (kind + '.stderr'))}
        if kind in expected_keys:
            item['matches_saved_stdout'] = result.stdout.strip().splitlines() == expected[expected_keys[kind]]
        receipts.append(item)
        (output / 'receipts.json').write_text(json.dumps({'scope': 'new direct correctness run only', 'compiler': version,
            'stock_archive_sha256': sha(archive), 'commands': receipts}, indent=2) + '\n')
        if result.returncode or item.get('matches_saved_stdout') is False:
            raise RuntimeError('Direct gate failed; saved all completed steps; no retry: ' + kind)
    print('PASS direct gates only. No native-plugin, timing or production admission.')


if __name__ == '__main__':
    main()
