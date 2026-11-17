"""Offline helper/receipt tests. No project configuration, compilation or render."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parent))
spec = importlib.util.spec_from_file_location('production_trial', Path(__file__).with_name('run.py'))
r = importlib.util.module_from_spec(spec); spec.loader.exec_module(r)


class RunContractTests(unittest.TestCase):
    def test_junit_full_exact_and_no_skips(self):
        names = [f'test-{i}' for i in range(569)]
        with tempfile.TemporaryDirectory() as t:
            path = Path(t) / 'junit.xml'
            root = ET.Element('testsuite')
            for name in names: ET.SubElement(root, 'testcase', name=name)
            def write(): ET.ElementTree(root).write(path)
            write(); r.validate_junit(path, names)
            for bad in ('failure', 'error', 'skipped'):
                element = ET.SubElement(root[0], bad); write()
                with self.assertRaises(ValueError): r.validate_junit(path, names)
                root[0].remove(element)
            root[-1].set('name', names[0]); write()
            with self.assertRaises(ValueError): r.validate_junit(path, names)
            root.remove(root[-1]); write()
            with self.assertRaises(ValueError): r.validate_junit(path, names)

    def test_copy_preserves_bytes_and_refuses_stale_or_symlink(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); source = root / 'original'; source.write_bytes(b'fixed')
            destination = root / 'sealed/copy'
            r.copy_file(source, destination); self.assertEqual(destination.read_bytes(), b'fixed')
            with self.assertRaises(ValueError): r.copy_file(source, destination)
            alias = root / 'alias'; alias.symlink_to(source)
            with self.assertRaises(ValueError): r.copy_file(alias, root / 'other')

    def test_clean_capture_outputs_only_exact_owned_sets(self):
        with tempfile.TemporaryDirectory() as t:
            build = Path(t) / 'build'; build.mkdir()
            directory = build / 'ring'; directory.mkdir()
            for name in r.host.capture_names('ring'): (directory / name).write_bytes(b'0' * 32768)
            contract = {'case': {'output': str(directory), 'kind': 'ring'}}
            r.clean_capture_outputs(contract, build); self.assertFalse(directory.exists())
            r.clean_capture_outputs(contract, build)
            directory.mkdir(); (directory / 'unexpected').write_bytes(b'x')
            with self.assertRaises(ValueError): r.clean_capture_outputs(contract, build)
            with self.assertRaises(ValueError): r.clean_capture_outputs(contract, Path(t) / 'not-build')

    def test_qualification_only_includes_original_tools_directory(self):
        import production
        original = ['/usr/bin/c++', '-I/source/src', '-O3', '-DNDEBUG', '-std=gnu++20', '-fPIE',
                    '-o', 'driver.o', '-c', '/source/src/tools/source_factory_benchmark.cpp']
        argv = production.qualification_compile_argv(original, Path('/source'), Path('/work/qualification.cpp'), Path('/work/qualification.o'))
        self.assertIn('-I/source/src/tools', argv)
        self.assertIn('-fPIE', argv); self.assertIn('-std=gnu++20', argv)
        self.assertNotIn('-I/source/src/tools', original)
        self.assertEqual(argv[argv.index('-c') + 1], '/work/qualification.cpp')

    def test_canonical_file_must_equal_sealed_candidate(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); canonical = root / 'canonical'; sealed = root / 'sealed'
            canonical.write_bytes(b'candidate'); sealed.write_bytes(b'candidate')
            manifest = r.bind_canonical_to_sealed({canonical: sealed})
            r.verify_frozen(manifest)
            canonical.write_bytes(b'unqualified replacement')
            with self.assertRaises(ValueError): r.bind_canonical_to_sealed({canonical: sealed})
            with self.assertRaises(ValueError): r.verify_frozen(manifest)

    def test_all_five_core_test_canonical_executables_bound(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); build = root / 'build'; build.mkdir(); sealed = root / 'sealed'; sealed.mkdir()
            prod = SimpleNamespace(build=build, engine=build/'engine', archive=build/'archive',
                                   benchmark=build/'benchmark', standalone=build/'standalone',
                                   maps={'CS80Reference_VST3': build/'vst3.map', 'CS80Reference_Standalone': build/'standalone.map'})
            keys = ['engine', 'archive', 'benchmark', 'standalone',
                    *(target for target, _ in r.CORE_TESTS),
                    'CS80Reference_VST3-map', 'CS80Reference_Standalone-map']
            candidate = {'paths': {key: sealed/key for key in keys}}
            mapping = r.canonical_candidate_mapping(prod, candidate)
            for actual, expected in mapping.items(): actual.write_bytes(b'qualified'); expected.write_bytes(b'qualified')
            self.assertEqual(len(mapping), 11)
            for target, _ in r.CORE_TESTS: self.assertIn(build/target, mapping)
            r.bind_canonical_to_sealed(mapping)
            (build/r.CORE_TESTS[0][0]).write_bytes(b'new unqualified core test')
            with self.assertRaises(ValueError): r.bind_canonical_to_sealed(mapping)

    def test_common_host_driver_binary_mutation_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            driver = Path(t) / 'host'; driver.write_bytes(b'original host executable')
            manifest = r.frozen_files([driver]); r.verify_frozen(manifest)
            driver.write_bytes(b'changed executable, same object receipts')
            with self.assertRaises(ValueError): r.verify_frozen(manifest)

    def test_original_host_assertions_footer(self):
        r.validate_host_log('12 native SUB scenarios, 0 failures\n', 'sub')
        r.validate_host_log('12 native ring scenarios, 0 failures\n', 'ring')
        for bad in ('12 ring compatibility scenarios, 0 failures',
                    '12 native ring scenarios, 1 failures',
                    'FAIL: assertion\n12 native ring scenarios, 0 failures',
                    'ERROR: exception\n12 native ring scenarios, 0 failures'):
            with self.assertRaises(ValueError): r.validate_host_log(bad, 'ring')

    def test_pluginval_clean_success_not_mixed_failure(self):
        r.validate_pluginval('normal output\nSUCCESS\n')
        for text in ('', 'SUCCESS partial', 'FAILED\nSUCCESS\n', 'SUCCESS\nFAILED detail'):
            with self.assertRaises(ValueError): r.validate_pluginval(text)

    def test_frozen_compiler_comes_from_real_preflight(self):
        frozen = json.loads((r.ROOT / 'frozen-compiler.json').read_text())
        self.assertEqual(frozen['CXX']['command'], '/usr/bin/c++')
        self.assertEqual(frozen['C']['command'], '/usr/bin/cc')
        self.assertEqual(frozen['CXX']['version'], '13.3.0')
        self.assertEqual(frozen['CXX']['sha256'], '1353e9bdd29a7295c7226bf6c63abccce056d8cac31f112e5cdbecc3f28c2769')
        self.assertEqual(frozen['CXX']['frontend_sha256'], '840b332fb62ec6f694ac77d91fe69ef7f80b0d69512ed89374af0ee7a506255d')

    def test_owned_output_removal_guard(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); work = root / 'work'; work.mkdir(); out = root / 'out'; out.mkdir()
            production = r.Production(root, work, out, commands=None)
            inside = work / 'object'; inside.write_bytes(b'x')
            production.remove_owned_outputs([inside]); self.assertFalse(inside.exists())
            outside = root / 'outside'; outside.write_bytes(b'x')
            with self.assertRaises(ValueError): production.remove_owned_outputs([outside])
            link = work / 'alias'; link.symlink_to(outside)
            with self.assertRaises(ValueError): production.remove_owned_outputs([link])
            self.assertTrue(outside.exists())

if __name__ == '__main__': unittest.main()
