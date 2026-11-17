import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('provenance', Path(__file__).with_name('provenance.py'))
p = importlib.util.module_from_spec(spec); spec.loader.exec_module(p)
ROOT = '/frozen/source'
PROFILE = '/frozen/profile'

def verify_scope(*args):
    return p.verify_compile_scope(*args, expected_compiler='g++')


def database():
    return [{'directory': '/frozen/build', 'file': ROOT + '/src/core/' + name + '.cpp',
             'arguments': ['g++', '-I' + ROOT + '/src', '-O3', '-DNDEBUG', '-fPIC',
                           '-Wall', '-Wextra', '-Wpedantic', '-o', 'core/' + name + '.cpp.o',
                           '-c', ROOT + '/src/core/' + name + '.cpp']}
            for name in ('Engine', 'FactoryPresets', 'Ig00156Batch')]


def changed(mode):
    result = database()
    result[0]['arguments'].insert(1, '-fprofile-' + mode + '=' + PROFILE)
    if mode == 'use': result[0]['arguments'][1:1] = ['-Werror=missing-profile', '-Werror=coverage-mismatch']
    return result


class Tests(unittest.TestCase):
    def test_baseline_default_off(self):
        verify_scope(database(), database(), ROOT, PROFILE, 'baseline')

    def test_declared_modes(self):
        for mode in ('generate', 'use'):
            result = verify_scope(database(), changed(mode), ROOT, PROFILE, mode)
            self.assertEqual(result['engine_object'], '/frozen/build/core/Engine.cpp.o')
            self.assertEqual(len(result['ordinary_objects']), 2)

    def test_no_profile_on_ordinary_units(self):
        x = changed('use'); x[1]['arguments'].insert(1, '-fprofile-use=' + PROFILE)
        with self.assertRaises(ValueError): verify_scope(database(), x, ROOT, PROFILE, 'use')

    def test_no_duplicate_engine(self):
        for mode in ('baseline', 'use'):
            a = database(); a.append(copy.deepcopy(a[0]))
            a[-1]['arguments'][a[-1]['arguments'].index('-o') + 1] = 'another/Engine.cpp.o'
            with self.assertRaises(ValueError): verify_scope(a, a, ROOT, PROFILE, mode)

    def test_no_stale_or_missing_guards(self):
        x = changed('use')
        for option in ('-fprofile-use=' + PROFILE, '-Werror=missing-profile', '-Werror=coverage-mismatch'):
            y = copy.deepcopy(x); y[0]['arguments'].remove(option)
            with self.assertRaises(ValueError): verify_scope(database(), y, ROOT, PROFILE, 'use')
        with self.assertRaises(ValueError): verify_scope(database(), changed('generate'), ROOT, PROFILE, 'use')

    def test_object_directory_source_and_inventory_drift(self):
        for kind in ('object', 'directory', 'source', 'inventory'):
            x = changed('use')
            if kind == 'object': x[0]['arguments'][x[0]['arguments'].index('-o') + 1] = 'new/Engine.cpp.o'
            elif kind == 'directory': x[0]['directory'] = '/elsewhere'
            elif kind == 'source': x[0]['file'] = '/elsewhere/Engine.cpp'
            else: x.pop()
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                verify_scope(database(), x, ROOT, PROFILE, 'use')

    def test_missing_pic_or_release(self):
        for flag in ('-fPIC', '-O3', '-DNDEBUG'):
            a = database(); b = changed('use')
            a[0]['arguments'].remove(flag); b[0]['arguments'].remove(flag)
            with self.assertRaises(ValueError): verify_scope(a, b, ROOT, PROFILE, 'use')

    def test_unsafe_flags_both_arms(self):
        for flag in ('-ffast-math', '-Ofast', '-fprofile-correction', '-fprofile-partial-training',
                     '-march=native', '-mtune=native', '-flto=auto', '@flags.rsp',
                     '-fno-math-errno', '-fno-signed-zeros', '-fassociative-math',
                     '--coverage', '-ftest-coverage', '-freciprocal-math', '-fno-profile-use'):
            a = database(); b = changed('use')
            a[0]['arguments'].insert(1, flag); b[0]['arguments'].insert(1, flag)
            with self.subTest(flag=flag), self.assertRaises(ValueError):
                verify_scope(a, b, ROOT, PROFILE, 'use')

    def test_extra_engine_flag_or_wrong_profile(self):
        for flag in ('-fvisibility=hidden', '-fprofile-use=/other/profile', '-DOTHER=1'):
            x = changed('use'); x[0]['arguments'].insert(1, flag)
            with self.assertRaises(ValueError): verify_scope(database(), x, ROOT, PROFILE, 'use')

    def test_baseline_profile_and_duplicate_flags(self):
        with self.assertRaises(ValueError): verify_scope(changed('use'), changed('use'), ROOT, PROFILE, 'baseline')
        x = changed('use'); x[0]['arguments'].insert(1, '-fprofile-use=' + PROFILE)
        with self.assertRaises(ValueError): verify_scope(database(), x, ROOT, PROFILE, 'use')

    def test_cmake_profile_name_and_integrity(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); nested = root / 'tmp/build/core'; nested.mkdir(parents=True)
            with self.assertRaises(ValueError): p.profile_manifest(root)
            path = nested / 'Engine.cpp.gcda'; path.write_bytes(b'adcg' + b'x' * 20)
            self.assertEqual(len(p.profile_manifest(root)), 1)
            path.write_bytes(b'bad!' + b'x' * 20)
            with self.assertRaises(ValueError): p.profile_manifest(root)
            path.write_bytes(b'adcg' + b'x' * 20)
            (root / 'other.gcda').write_bytes(b'adcg' + b'x' * 20)
            with self.assertRaises(ValueError): p.profile_manifest(root)

    def test_profile_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); target = root / 'Engine.cpp.gcda'; target.write_bytes(b'adcg' + b'x' * 20)
            (root / 'alias').symlink_to(target)
            with self.assertRaises(ValueError): p.profile_manifest(root)

    def test_explicit_c_compiler_inventory(self):
        a = database(); b = changed('use')
        c = {'directory': '/frozen/build', 'file': ROOT + '/dependency.c',
             'arguments': ['/usr/bin/cc', '-O3', '-DNDEBUG', '-fPIC', '-o', 'dependency.c.o',
                           '-c', ROOT + '/dependency.c']}
        a.append(copy.deepcopy(c)); b.append(copy.deepcopy(c))
        with self.assertRaises(ValueError): verify_scope(a, b, ROOT, PROFILE, 'use')
        result = p.verify_compile_scope(a, b, ROOT, PROFILE, 'use',
                                      expected_compiler='g++', expected_c_compiler='/usr/bin/cc')
        self.assertEqual(len(result['ordinary_objects']), 3)
        b[-1]['arguments'][0] = '/usr/bin/clang'
        with self.assertRaises(ValueError):
            p.verify_compile_scope(a, b, ROOT, PROFILE, 'use',
                                   expected_compiler='g++', expected_c_compiler='/usr/bin/cc')

    def test_effective_optimization_and_compiler(self):
        a = database(); b = changed('use')
        a[0]['arguments'].append('-O0'); b[0]['arguments'].append('-O0')
        with self.assertRaises(ValueError): verify_scope(a, b, ROOT, PROFILE, 'use')
        a = database(); b = changed('use')
        a[0]['arguments'][0] = b[0]['arguments'][0] = 'clang++'
        with self.assertRaises(ValueError): verify_scope(a, b, ROOT, PROFILE, 'use')

    def test_profile_root_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); target = root / 'actual'; target.mkdir()
            (target / 'Engine.cpp.gcda').write_bytes(b'adcg' + b'x' * 20)
            alias = root / 'alias'; alias.symlink_to(target, target_is_directory=True)
            with self.assertRaises(ValueError): p.profile_manifest(alias)

    def test_frozen_artifact_bytes_required(self):
        with tempfile.TemporaryDirectory() as t:
            a = Path(t) / 'Engine.cpp.o'; b = Path(t) / 'plugin.so'
            a.write_bytes(b'object'); b.write_bytes(b'plugin')
            manifest = p.frozen_files([a, b]); p.verify_frozen(manifest)
            b.write_bytes(b'changed')
            with self.assertRaises(ValueError): p.verify_frozen(manifest)
            with self.assertRaises(ValueError): p.frozen_files([])
            b.write_bytes(b'')
            with self.assertRaises(ValueError): p.frozen_files([b])

if __name__ == '__main__': unittest.main()
