"""Test fixtures for the CI no-skip/count guard, not real execution receipts."""
import importlib.util
from pathlib import Path
import unittest

POSITIVE = '# tests 8\n# suites 0\n# pass 8\n# fail 0\n# cancelled 0\n# skipped 0\n# todo 0\n'


class NodeTapGuardTests(unittest.TestCase):
    def helper(self):
        path = Path(__file__).with_name('node_tap_guard.py')
        self.assertTrue(path.is_file(), 'Required Node no-skip guard absent')
        spec = importlib.util.spec_from_file_location('cs80_node_tap_guard', path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_complete_positive_and_additive_tests(self):
        m = self.helper()
        self.assertEqual(m.check_summary(POSITIVE)['tests'], 8)
        self.assertEqual(m.check_summary(POSITIVE.replace('tests 8', 'tests 9').replace('pass 8', 'pass 9'))['tests'], 9)

    def test_missing_skipped_failed_todo_duplicate_and_short_reports_rejected(self):
        m = self.helper()
        cases = ['', POSITIVE.replace('# skipped 0\n', ''),
                 POSITIVE.replace('skipped 0', 'skipped 1'),
                 POSITIVE.replace('fail 0', 'fail 1'),
                 POSITIVE.replace('cancelled 0', 'cancelled 1'),
                 POSITIVE.replace('todo 0', 'todo 1'),
                 POSITIVE.replace('tests 8', 'tests 7').replace('pass 8', 'pass 7'),
                 POSITIVE + '# tests 8\n', POSITIVE + '# skipped 0\n',
                 POSITIVE.replace('pass 8', 'pass 7'),
                 POSITIVE.replace('tests 8', 'tests -1'),
                 POSITIVE.replace('tests 8', 'tests NaN')]
        for text in cases:
            with self.subTest(text=text), self.assertRaises(ValueError):
                m.check_summary(text)


if __name__ == '__main__':
    unittest.main()
