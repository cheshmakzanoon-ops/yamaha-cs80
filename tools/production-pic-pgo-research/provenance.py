"""Fail-closed prospective PIC research command guards. No execution or training."""
import hashlib
import json
from pathlib import Path
import shlex


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def command_entries(document):
    entries = {}
    for row in document:
        directory = Path(row['directory']).resolve()
        source = Path(row['file'])
        source = (source if source.is_absolute() else directory / source).resolve()
        argv = list(row['arguments']) if 'arguments' in row else shlex.split(row['command'])
        require(argv.count('-o') == 1, 'one output option required')
        index = argv.index('-o')
        require(index + 1 < len(argv), 'missing object output')
        obj = Path(argv[index + 1])
        obj = (obj if obj.is_absolute() else directory / obj).resolve()
        key = str(obj)
        require(key not in entries, 'duplicate output object')
        entries[key] = {'directory': str(directory), 'source': str(source), 'argv': argv}
    require(bool(entries), 'empty compile database')
    return entries


def reject_unsafe(argv):
    for item in argv:
        require(not item.startswith(('@', '-Ofast', '-ffast-math', '-funsafe-math',
                                     '-fassociative-math', '-ffinite-math-only', '-freciprocal-math', '-flto',
                                     '-fno-profile', '--coverage', '-ftest-coverage',
                                     '-fprofile-correction', '-fprofile-partial-training',
                                     '-march=', '-mtune=', '-fno-signed-zeros',
                                     '-fno-trapping-math', '-fno-math-errno')),
                'undeclared math/ISA/LTO/response option ' + item)


def verify_compile_scope(baseline, changed, source_root, profile_dir, mode, *, expected_compiler, expected_c_compiler=None):
    require(mode in ('baseline', 'generate', 'use'), 'unknown mode')
    require(bool(expected_compiler), 'explicit frozen compiler required')
    a, b = command_entries(baseline), command_entries(changed)
    require(a.keys() == b.keys(), 'compile object inventory drift')
    engine = str((Path(source_root) / 'src/core/Engine.cpp').resolve())
    matches = [key for key, row in a.items() if row['source'] == engine]
    require(len(matches) == 1, 'Engine must have exactly one compiled object')
    obj = matches[0]
    expected = [] if mode == 'baseline' else ['-fprofile-' + mode + '=' + str(Path(profile_dir).resolve())]
    if mode == 'use': expected += ['-Werror=missing-profile', '-Werror=coverage-mismatch']
    for key, row in a.items():
        other = b[key]
        compiler = expected_c_compiler if Path(row['source']).suffix == '.c' else expected_compiler
        require(bool(compiler), 'explicit frozen C compiler required for C translation unit')
        require(row['argv'][0] == other['argv'][0] == compiler, 'unexpected compiler command')
        require(row['source'] == other['source'] and row['directory'] == other['directory'],
                'compile source/directory drift')
        require(not any('-fprofile-' in arg or arg.startswith('-fauto-profile') for arg in row['argv']),
                'baseline already profiled')
        reject_unsafe(row['argv']); reject_unsafe(other['argv'])
        if key != obj:
            require(row == other, 'ordinary object command changed')
            continue
        optimization = [arg for arg in row['argv'] if arg.startswith('-O')]
        require(optimization and optimization[-1] == '-O3', 'effective optimization must be O3')
        require('-fPIC' in row['argv'] and '-DNDEBUG' in row['argv'],
                'production PIC Release flags missing')
        remaining = list(other['argv'])
        for option in expected:
            require(remaining.count(option) == 1, 'missing/duplicate declared profile option')
            remaining.remove(option)
        require(remaining == row['argv'], 'Engine changed beyond declared PGO options')
    return {'engine_object': obj, 'engine_source': engine, 'mode': mode,
            'ordinary_objects': sorted(key for key in a if key != obj)}


def profile_manifest(directory):
    directory = Path(directory)
    require(directory.is_dir() and not directory.is_symlink(), 'regular profile directory required')
    files = sorted(path for path in directory.rglob('*') if path.is_file())
    require(not any(path.is_symlink() for path in directory.rglob('*')), 'profile symlink')
    require(len(files) == 1 and files[0].name == 'Engine.cpp.gcda', 'one CMake Engine.cpp.gcda required')
    data = files[0].read_bytes()
    require(len(data) > 16 and data[:4] in (b'adcg', b'gcda'), 'invalid gcda header')
    return {str(files[0].relative_to(directory)): {'bytes': len(data), 'sha256': sha256(files[0])}}


def frozen_files(paths):
    result = {}
    for path in paths:
        path = Path(path)
        require(path.is_file() and not path.is_symlink(), 'regular artifact required')
        require(path.stat().st_size > 0, 'empty artifact')
        result[str(path.resolve())] = {'bytes': path.stat().st_size, 'sha256': sha256(path)}
    require(bool(result), 'empty frozen manifest')
    return result


def verify_frozen(manifest):
    require(frozen_files(manifest) == manifest, 'frozen artifact changed')
