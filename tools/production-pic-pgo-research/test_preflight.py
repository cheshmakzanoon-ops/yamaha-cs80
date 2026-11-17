import copy
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
spec = importlib.util.spec_from_file_location('preflight', Path(__file__).with_name('preflight.py'))
p = importlib.util.module_from_spec(spec); spec.loader.exec_module(p)


def roster():
    names = ['cs80_vst3_sub_automation_tests']
    for rate in (44100, 48000, 96000):
        for mode in ('realtime', 'offline'):
            names.append(f'cs80_vst3_ring_automation_{rate}_{mode}_tests')
            if (rate, mode) != (48000, 'realtime'):
                names.append(f'cs80_vst3_sub_automation_{rate}_{mode}_tests')
    names += ['other-' + str(i) for i in range(569 - len(names))]
    return {'tests': [{'name': name, 'properties': [{'name': 'TIMEOUT', 'value': 180}]} for name in names]}


class PreflightTests(unittest.TestCase):
    def test_full_count_and_selected_roster(self):
        result = p.test_inventory(roster())
        self.assertEqual(len(result), 569)
        self.assertTrue(all(row['command'] is None for row in result.values()))

    def test_count_duplicate_missing_selected_or_timeout(self):
        for kind in ('count', 'duplicate', 'selected', 'timeout'):
            data = roster()
            if kind == 'count': data['tests'].pop()
            elif kind == 'duplicate': data['tests'][-1]['name'] = data['tests'][0]['name']
            elif kind == 'selected': data['tests'][0]['name'] = 'unexpected'
            else: data['tests'][0]['properties'][0]['value'] = 181
            with self.subTest(kind=kind), self.assertRaises(ValueError): p.test_inventory(data)

    def test_toy_plumbing_protocol_without_native_execution(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); work = root / 'work'; out = root / 'out'; work.mkdir(); out.mkdir()
            calls = []
            def command(name, argv, expect_success=True):
                calls.append((name, list(map(str, argv)), expect_success))
                if name == 'toy-integer-execute-once':
                    profile = work / 'compiler-plumbing/profiles/nested/Engine.cpp.gcda'
                    profile.parent.mkdir(); profile.write_bytes(b'adcg' + b'x' * 20)
                if not expect_success:
                    reason = 'missing-profile' if 'missing' in name else 'coverage-mismatch'
                    (out / (name + '.stderr')).write_text(reason)
                return ''
            p.compiler_plumbing('/usr/bin/c++', work, out, command)
            self.assertEqual(sum(name == 'toy-integer-execute-once' for name, _, _ in calls), 1)
            failures = [name for name, _, success in calls if not success]
            self.assertEqual(failures, ['toy-missing-profile-negative', 'toy-mismatched-profile-negative'])
            object_paths = [argv[argv.index('-o') + 1] for name, argv, _ in calls
                            if name in ('toy-generate-object', 'toy-use-same-object-path',
                                        'toy-missing-profile-negative', 'toy-mismatched-profile-negative')]
            self.assertEqual(len(set(object_paths)), 1)
            self.assertTrue((out / 'compiler-plumbing.json').exists())

    def test_cache_compiler_paths(self):
        values = p.cache_values('// comment\nCMAKE_C_COMPILER:FILEPATH=/usr/bin/cc\nCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/c++\n#comment\n')
        self.assertEqual(values['CMAKE_C_COMPILER'], '/usr/bin/cc')
        self.assertEqual(values['CMAKE_CXX_COMPILER'], '/usr/bin/c++')

    def test_patched_tree_content_not_only_git_head(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); (root / '.git').mkdir(); (root / '.git/config').write_text('ignored')
            (root / 'module.cpp').write_text('original'); before = p.tree_manifest(root)
            self.assertEqual(set(before), {'module.cpp'})
            (root / 'module.cpp').write_text('patched'); self.assertNotEqual(before, p.tree_manifest(root))

if __name__ == '__main__': unittest.main()
