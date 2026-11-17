import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('host_contract', Path(__file__).with_name('host_contract.py'))
p = importlib.util.module_from_spec(spec); spec.loader.exec_module(p)
REPO = '/source'; BUILD = '/build'


def document():
    contract = p.selected_contract(REPO, BUILD)
    tests = [{'name': name, 'command': value['command'],
              'properties': [{'name': 'TIMEOUT', 'value': 180}]} for name, value in contract.items()]
    tests += [{'name': f'other-{i}', 'command': ['/build/other-' + str(i)], 'properties': []}
              for i in range(569 - len(tests))]
    return {'tests': tests}


class HostTests(unittest.TestCase):
    def test_original_roster_and_default_command(self):
        contract = p.validate_selected(document(), REPO, BUILD)
        self.assertEqual(len(contract), 12)
        self.assertEqual(len(contract['cs80_vst3_sub_automation_tests']['command']), 5)
        self.assertEqual(contract['cs80_vst3_ring_automation_48000_realtime_tests']['command'][-2:], ['48000', 'realtime'])
        self.assertEqual(sum(len(p.capture_names(x['kind'])) for x in contract.values()), 462)
        # Literal names independently read from actual fixture source, not from helper construction.
        self.assertIn('8-13-20-noqueue.pcm', p.capture_names('sub'))
        self.assertIn('old20-4096.pcm', p.capture_names('sub'))
        self.assertIn('28-8-13-policy2.pcm', p.capture_names('ring'))
        self.assertNotIn('voices8-source13-lane20-noqueue.pcm', p.capture_names('sub'))

    def test_missing_commands_or_weakened_host_mode(self):
        for kind in ('missing', 'compat', 'timeout', 'disabled', 'module'):
            data = document(); row = data['tests'][0]
            if kind == 'missing': row.pop('command')
            elif kind == 'compat': row['command'].append('--compat-only')
            elif kind == 'timeout': row['properties'][0]['value'] = 181
            elif kind == 'disabled': row['properties'].append({'name': 'DISABLED', 'value': True})
            else: row['command'][1] = '/another/plugin.so'
            with self.subTest(kind=kind), self.assertRaises(ValueError): p.validate_selected(data, REPO, BUILD)

    def test_full_inventory_requires_built_all569(self):
        before = document(); after = copy.deepcopy(before)
        before['tests'][-1].pop('command')
        self.assertEqual(len(p.validate_full_inventory(before, after)), 569)
        after['tests'][-1].pop('command')
        with self.assertRaises(ValueError): p.validate_full_inventory(before, after)

    def test_full_inventory_no_reduced_or_disabled_tests(self):
        for kind in ('count', 'rename', 'property', 'command'):
            before = document(); after = copy.deepcopy(before)
            if kind == 'count': after['tests'].pop()
            elif kind == 'rename': after['tests'][-1]['name'] = 'replacement'
            elif kind == 'property': after['tests'][-1]['properties'] = [{'name': 'DISABLED', 'value': True}]
            else: after['tests'][-1]['command'] = ['/another/test']
            with self.subTest(kind=kind), self.assertRaises(ValueError): p.validate_full_inventory(before, after)

    def test_exact_capture_bytes_and_inventory(self):
        for kind in ('sub', 'ring'):
            with tempfile.TemporaryDirectory() as t:
                a, b = Path(t) / 'a', Path(t) / 'b'; a.mkdir(); b.mkdir()
                for name in p.capture_names(kind):
                    (a / name).write_bytes(b'0' * 32768); (b / name).write_bytes(b'0' * 32768)
                result = p.compare_captures(a, b, kind)
                self.assertEqual(result['files'], 41 if kind == 'sub' else 36)
                first = next(b.iterdir()); first.write_bytes(b'1' * 32768)
                with self.assertRaises(ValueError): p.compare_captures(a, b, kind)
                first.write_bytes(b'0')
                with self.assertRaises(ValueError): p.capture_manifest(b, kind)
                first.unlink()
                with self.assertRaises(ValueError): p.capture_manifest(b, kind)

    def test_extra_or_symlink_capture_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            for name in p.capture_names('ring'): (root / name).write_bytes(b'0' * 32768)
            (root / 'extra').write_bytes(b'x')
            with self.assertRaises(ValueError): p.capture_manifest(root, 'ring')
            (root / 'extra').unlink()
            paths = list(root.iterdir()); paths[0].unlink(); paths[0].symlink_to(paths[1])
            with self.assertRaises(ValueError): p.capture_manifest(root, 'ring')

if __name__ == '__main__': unittest.main()
