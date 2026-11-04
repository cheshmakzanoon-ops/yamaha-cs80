#!/usr/bin/env python3
"""Mutation-qualify the frozen IG00156 continuation comparator.

Copy actual production headers into independent temporary builds. The frozen
oracle is hash-sealed and never changed. A compile failure/crash/timeout is not a
mutation kill. This runner supplies numerical evidence, not a timing scorecard.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[2]
HEADER = 'Ig00156PhaseCorrection.h'
SUITES = ('fixed', 'moving', 'state', 'row', 'fp')


def replace_once(source: str, old: str, new: str) -> str:
    if source.count(old) != 1:
        raise RuntimeError(f'Expected one source anchor: {old!r}')
    return source.replace(old, new)


def mutation(source: str, name: str) -> str:
    if name == 'stale-coefficients':
        return replace_once(source, 'if (cutoff == lastFc && q == lastQ && fs == lastFs) return;',
                            'if (lastFc >= 0.0f) return;')
    if name == 'one-ulp-coefficient':
        return replace_once(source, '        fromBp={}; fromTanh={};',
                            '        right[0][1] = std::nextafter(right[0][1], INFINITY);\n        fromBp={}; fromTanh={};')
    if name == 'restore-drops-history':
        return replace_once(source, 'v = nodes; inputs = history; slope1 = s1;',
                            'v = nodes; inputs = {}; slope1 = s1;')
    if name == 'early-newton-stop':
        return replace_once(source, 'std::abs(residual)<1e-10', 'std::abs(residual)<1e-7')
    if name == 'reordered-row-sum':
        return replace_once(source, 'for(int j=0;j<4;++j) rhs[i]+=right[i][j]*v[j];',
                            'for(int j=3;j>=0;--j) rhs[i]+=right[i][j]*v[j];')
    raise RuntimeError(f'Unknown mutation: {name}')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--compiler', default='c++')
    args = parser.parse_args()
    out = args.out.resolve() if args.out else Path(tempfile.gettempdir()) / ('cs80-exact-filter-' + uuid.uuid4().hex)
    out.mkdir(parents=True, exist_ok=False)
    print('Exact-filter evidence: ' + str(out), flush=True)
    records = []

    def command(argv: list[str], label: str, expected: int = 0):
        result = subprocess.run(argv, text=True, capture_output=True, cwd=ROOT, timeout=300)
        (out / (label + '.log')).write_text(result.stdout + result.stderr)
        records.append({'label': label, 'command': argv, 'exit': result.returncode,
                        'stdout_sha256': hashlib.sha256(result.stdout.encode()).hexdigest(),
                        'stderr_sha256': hashlib.sha256(result.stderr.encode()).hexdigest()})
        (out / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
        print(label + ': exit ' + str(result.returncode), flush=True)
        if result.returncode != expected:
            raise RuntimeError('Unexpected result; inspect ' + label + '.log')
        return result

    oracle = out / 'oracle/exact_scalar_frozen'
    command([sys.executable, str(ROOT / 'tests/cpu/materialize-frozen-scalar-kernels.py'),
             '--fixture-root', str(ROOT / 'tests/fixtures/exact_scalar_kernels'), '--output', str(oracle)], 'materialize')
    source = (ROOT / 'src/core' / HEADER).read_text()

    def build(name: str, code: str) -> Path:
        directory = out / name
        core = directory / 'include/core'
        core.mkdir(parents=True)
        for path in (ROOT / 'src/core').glob('*.h'):
            shutil.copyfile(path, core / path.name)
        (core / HEADER).write_text(code)
        (directory / 'source-sha256.json').write_text(json.dumps(
            {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(core.glob('*.h'))}, indent=2) + '\n')
        msvc = Path(args.compiler).name.lower() in ('cl', 'cl.exe')
        binary = directory / ('test.exe' if msvc else 'test')
        driver = ROOT / 'tests/ExactScalarFilterTests.cpp'
        if msvc:
            argv = [args.compiler, '/nologo', '/std:c++20', '/O2', '/DNDEBUG', '/EHsc',
                    '/I' + str(directory / 'include'), '/I' + str(oracle.parent), str(driver),
                    '/Fe:' + str(binary), '/Fo:' + str(directory / 'driver.obj')]
        else:
            argv = [args.compiler, '-std=c++20', '-O3', '-DNDEBUG', '-I' + str(directory / 'include'),
                    '-I' + str(oracle.parent), str(driver), '-o', str(binary)]
        command(argv, name + '-compile')
        return binary

    positive = build('positive', source)
    for suite in SUITES:
        command([str(positive), '--suite', suite], 'positive-' + suite)
    intended = {
        'stale-coefficients': ('moving', 'stage.node'),
        'one-ulp-coefficient': ('row', 'stage.node'),
        'restore-drops-history': ('state', 'memory.inputHistory'),
        'early-newton-stop': ('moving', 'stage.node'),
        'reordered-row-sum': ('row', 'stage.node'),
    }
    for name, (suite, observable) in intended.items():
        binary = build(name, mutation(source, name))
        result = command([str(binary), '--suite', suite], name + '-rejection', 1)
        if 'FAIL ' not in result.stderr or observable not in result.stderr:
            raise RuntimeError(name + ' did not fail its named numerical observer: ' + observable)
    restored = build('restored', source)
    for suite in SUITES:
        command([str(restored), '--suite', suite], 'restored-' + suite)
    (out / 'complete.json').write_text(json.dumps({
        'complete': True, 'source_sha256': hashlib.sha256(source.encode()).hexdigest(),
        'mutations': list(intended), 'scope': 'Numerical assertions only; no speed claim.'
    }, indent=2) + '\n')


if __name__ == '__main__':
    main()
