#!/usr/bin/env python3
"""Mutation-harness evidence must distinguish semantic failures from runtime errors."""
import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location(
    'distribution_mutation_runner', Path(__file__).with_name('test_distribution_mutations.py'))
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class DistributionMutationContractTests(unittest.TestCase):
    def classify(self, code, output, expected):
        classifier = getattr(RUNNER, 'classify_run', None)
        self.assertTrue(callable(classifier), 'The runner needs an independently testable evidence classifier')
        return classifier(code, output, expected)

    def test_exact_ordinary_failure_qualifies(self):
        log = 'FAIL: test_notice (__main__.Fixture.test_notice)\nRan 11 tests in 0.1s\nFAILED (failures=1)\n'
        self.assertTrue(self.classify(1, log, ['test_notice'])['qualified'])

    def test_error_only_never_qualifies(self):
        log = 'ERROR: test_notice (__main__.Fixture.test_notice)\nRan 11 tests in 0.1s\nFAILED (errors=1)\n'
        self.assertFalse(self.classify(1, log, ['test_notice'])['qualified'])

    def test_error_mixed_with_assertion_never_qualifies(self):
        log = 'FAIL: test_notice (__main__.Fixture.test_notice)\nERROR: test_other (__main__.Fixture.test_other)\nRan 11 tests in 0.1s\nFAILED (failures=1, errors=1)\n'
        self.assertFalse(self.classify(1, log, ['test_notice'])['qualified'])

    def test_wrong_assertion_never_qualifies(self):
        log = 'FAIL: test_unrelated (__main__.Fixture.test_unrelated)\nRan 11 tests in 0.1s\nFAILED (failures=1)\n'
        self.assertFalse(self.classify(1, log, ['test_notice'])['qualified'])

    def test_missing_inventory_or_skips_never_qualify(self):
        log = 'FAIL: test_notice (__main__.Fixture.test_notice)\nRan 11 tests in 0.1s\nFAILED (failures=1)\n'
        for mutated in (log.replace('11 tests', '10 tests'), log.replace('failures=1)', 'failures=1, skipped=1)')):
            with self.subTest(log=mutated):
                self.assertFalse(self.classify(1, mutated, ['test_notice'])['qualified'])

    def test_crash_exit_never_qualifies(self):
        log = 'FAIL: test_notice (__main__.Fixture.test_notice)\nRan 11 tests in 0.1s\nFAILED (failures=1)\n'
        self.assertFalse(self.classify(-11, log, ['test_notice'])['qualified'])

    def test_positive_requires_clean_success(self):
        self.assertTrue(self.classify(0, '\nRan 11 tests in 0.1s\n\nOK\n', [])['qualified'])
        self.assertFalse(self.classify(0, '\nRan 11 tests in 0.1s\n\nOK (skipped=1)\n', [])['qualified'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
