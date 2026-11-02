#!/usr/bin/env python3
"""Partial marker files are failed/stale evidence, never atomic durability proof."""
from pathlib import Path
import unittest
from unittest import mock

from test_full_engine_audit_composed_default_v1 import AuditComposedDefaultV1Tests


class CompletionFaultTests(unittest.TestCase):
    def setUp(self):
        self.fixture = AuditComposedDefaultV1Tests('test_unmodified_published_registration_and_source_pass')
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)

    def test_truncated_v1_marker_is_not_a_completed_capture(self):
        context = self.fixture.completed_fixture()
        artifacts = self.fixture.root / 'artifacts'
        (artifacts / 'audit.complete').write_text('source_sha=partial')
        self.fixture.refuse('completion-marker', lambda: self.fixture.wrapper.finish_composed_default_v1(context))
        self.assertFalse((artifacts / 'audit-composed-default-v1.complete').exists())

    def test_partial_composed_default_v1_marker_write_fails_and_stale_retry_is_refused(self):
        context = self.fixture.completed_fixture()
        artifacts = self.fixture.root / 'artifacts'
        original_open = Path.open

        class InterruptedWrite:
            def __enter__(self):
                self.stream = original_open(artifacts / 'audit-composed-default-v1.complete', 'x')
                return self

            def write(self, text):
                self.stream.write('source_sha=partial')
                self.stream.flush()
                raise OSError('deliberate partial-marker write failure')

            def __exit__(self, *args):
                self.stream.close()
                return False

        def intercept(path, *args, **kwargs):
            if path.name == 'audit-composed-default-v1.complete':
                return InterruptedWrite()
            return original_open(path, *args, **kwargs)

        with mock.patch.object(Path, 'open', intercept):
            with self.assertRaisesRegex(OSError, 'deliberate partial-marker write failure'):
                self.fixture.wrapper.finish_composed_default_v1(context)
        self.assertEqual((artifacts / 'audit-composed-default-v1.complete').read_text(), 'source_sha=partial')
        self.fixture.refuse('stale-completion', lambda: self.fixture.wrapper.finalize(context))
        self.assertTrue((artifacts / 'audit.complete').is_file(), 'Original V1 output is preserved')


if __name__ == '__main__':
    # Explicit suite avoids re-running the imported 45-test fixture class here.
    unittest.main(defaultTest='CompletionFaultTests', verbosity=2)
