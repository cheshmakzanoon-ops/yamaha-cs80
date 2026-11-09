#!/usr/bin/env python3
"""Compile independent math-profile admission probes, without scalar bit claims."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / 'tests/Ig00156BatchAdmissionProbe.cpp'
BACKEND = ROOT / 'src/core/Ig00156Batch.cpp'
PROFILES = [
    ('strict', []),
    ('associative-alone', ['-fassociative-math']),
    ('associative-effective', ['-fassociative-math', '-fno-signed-zeros', '-fno-trapping-math']),
    ('reciprocal', ['-freciprocal-math']),
    ('no-signed-zero', ['-fno-signed-zeros']),
    ('unsafe', ['-funsafe-math-optimizations']),
    ('fast', ['-ffast-math']),
    ('finite-only', ['-ffinite-math-only']),
    ('ofast', ['-Ofast']),
    ('fma', ['-mfma']),
    ('no-trapping-only', ['-fno-trapping-math']),
]


def invoke(command, **kwargs):
    return subprocess.run(command, text=True, capture_output=True, **kwargs)


def macros(compiler, flags):
    result = invoke([compiler, '-std=c++20', *flags, '-dM', '-E', '-x', 'c++', '-'], input='')
    values = {}
    for line in result.stdout.splitlines():
        if line.startswith('#define '):
            parts = line.split(maxsplit=2)
            values[parts[1]] = parts[2] if len(parts) == 3 else ''
    return result, values


def family(values):
    if '__clang__' in values:
        return 'clang'
    if '__GNUC__' in values and not any(k in values for k in (
            '__INTEL_COMPILER', '__INTEL_LLVM_COMPILER', '__NVCOMPILER', '__PGI', '__ibmxl__')):
        return 'gcc'
    return 'other'


def admitted(kind, profile, values):
    if kind != 'gcc' or '__x86_64__' not in values:
        return 0
    # These GCC profiles do not change scalar association/sign/reciprocal
    # semantics. In isolation GCC explicitly ignores -fassociative-math.
    if profile not in ('strict', 'associative-alone', 'no-trapping-only'):
        return 0
    if any(k in values for k in ('__FMA__', '__FAST_MATH__', '__ASSOCIATIVE_MATH__',
                                '__RECIPROCAL_MATH__', '__NO_SIGNED_ZEROS__')):
        return 0
    return int(values.get('__FINITE_MATH_ONLY__', '0') == '0'
               and values.get('__FLT_EVAL_METHOD__', '0') == '0')


def build_probe(compiler, flags, obj, binary, caller_expected, backend_expected, includes):
    result = invoke([compiler, '-std=c++20', '-O3', *flags, *includes,
                     f'-DCS80_EXPECT_CALLER_BUILD={caller_expected}',
                     f'-DCS80_EXPECT_BACKEND_BUILD={backend_expected}', str(PROBE), str(obj), '-o', str(binary)])
    if result.returncode:
        raise RuntimeError('probe compile failed:\n' + result.stdout + result.stderr)
    return result


def run(compiler, out, strict_backend_compiler):
    initial, base_macros = macros(compiler, [])
    if initial.returncode:
        raise RuntimeError(initial.stderr)
    kind = family(base_macros)
    if kind not in ('gcc', 'clang'):
        raise RuntimeError('This profile runner supports GCC or Clang compiler drivers only')
    records = []
    includes = ['-I' + str(ROOT / 'src')]
    strict_obj = None
    strict_expected = 0
    if kind == 'clang' and strict_backend_compiler:
        _, other = macros(strict_backend_compiler, [])
        if family(other) == 'gcc':
            strict_expected = admitted('gcc', 'strict', other)
            strict_obj = out / 'strict-gcc-backend.o'
            result = invoke([strict_backend_compiler, '-std=c++20', '-O3', '-ffp-contract=off',
                             *includes, '-c', str(BACKEND), '-o', str(strict_obj)])
            if result.returncode:
                raise RuntimeError(result.stderr)
    for name, flags in PROFILES:
        preprocessor, values = macros(compiler, flags)
        (out / (name + '-macros.txt')).write_text(preprocessor.stdout + preprocessor.stderr)
        if preprocessor.returncode:
            # -mfma is not an option on e.g. AArch64. Other rejected profiles
            # are not silently skipped: an unexpected compiler error is a fail.
            if name == 'fma' and '__x86_64__' not in base_macros:
                records.append({'profile': name, 'skipped': 'non-x86 compiler rejects -mfma'})
                continue
            raise RuntimeError(preprocessor.stderr)
        expected = admitted(kind, name, values)
        obj = out / (name + '-backend.o')
        contract_flags = ['-ffp-contract=off'] if kind == 'gcc' else []
        result = invoke([compiler, '-std=c++20', '-O3', *flags, *contract_flags,
                         *includes, '-c', str(BACKEND), '-o', str(obj)])
        (out / (name + '-build.txt')).write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr)
        build_probe(compiler, flags, obj, out / name, expected, expected, includes)
        result = invoke([str(out / name)])
        (out / (name + '-run.txt')).write_text(result.stdout + result.stderr)
        if result.returncode not in (0, 77):
            raise RuntimeError(name + ':\n' + result.stdout + result.stderr)
        record = {'profile': name, 'flags': flags, 'caller_expected': expected,
                  'backend_expected': expected, 'returncode': result.returncode,
                  'output': result.stdout.strip()}
        if strict_obj is not None:
            binary = out / (name + '-mixed')
            build_probe(compiler, flags, strict_obj, binary, 0, strict_expected, includes)
            mixed = invoke([str(binary)])
            (out / (name + '-mixed-run.txt')).write_text(mixed.stdout + mixed.stderr)
            if mixed.returncode not in (0, 77):
                raise RuntimeError(name + ' mixed:\n' + mixed.stdout + mixed.stderr)
            record['mixed_strict_gcc_backend'] = mixed.stdout.strip()
        records.append(record)
        print(kind, name, result.stdout.strip() or 'SKIP: caller requires unavailable FMA', flush=True)
    if kind == 'clang' and admitted('gcc', 'strict', base_macros):
        # Negative control: recreating the missing Clang exclusion must fail.
        mutant = out / 'mutant/src/core'
        mutant.mkdir(parents=True)
        header = (ROOT / 'src/core/Ig00156Batch.h').read_text()
        anchor = ' && !defined(__clang__)'
        if header.count(anchor) != 1:
            raise RuntimeError('Review the negative-control anchor after a header change')
        (mutant / 'Ig00156Batch.h').write_text(header.replace(anchor, ''))
        shutil.copy2(BACKEND, mutant / BACKEND.name)
        mutant_includes = ['-I' + str(out / 'mutant/src'), *includes, '-I' + str(ROOT / 'src/core')]
        obj = out / 'mutant.o'
        result = invoke([compiler, '-std=c++20', '-O3',
                         '-ffp-contract=off', *mutant_includes, '-c', str(mutant / BACKEND.name), '-o', str(obj)])
        if result.returncode:
            raise RuntimeError(result.stderr)
        binary = out / 'mutant-probe'
        build_probe(compiler, [], obj, binary, 0, 0, mutant_includes)
        result = invoke([str(binary)])
        (out / 'negative-control.txt').write_text(result.stdout + result.stderr)
        if result.returncode != 1:
            raise RuntimeError('Clang-admission negative control was not detected')
        records.append({'negative_control': 'missing Clang exclusion', 'returncode': 1,
                        'output': result.stdout.strip()})
        print('negative control correctly rejected:', result.stdout.strip(), flush=True)
    (out / 'results.json').write_text(json.dumps({'compiler': compiler, 'family': kind,
                                               'profiles': records}, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--strict-backend-compiler', default=shutil.which('g++'))
    args = parser.parse_args()
    if args.output:
        args.output.mkdir(parents=True)
        run(args.compiler, args.output.resolve(), args.strict_backend_compiler)
    else:
        with tempfile.TemporaryDirectory(prefix='cs80-batch-admission-') as temporary:
            run(args.compiler, Path(temporary), args.strict_backend_compiler)


if __name__ == '__main__':
    main()
