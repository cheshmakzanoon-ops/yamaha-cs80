#!/usr/bin/env python3
"""Focused inert controls for A46's Windows-only notice exception."""
from __future__ import annotations
import copy
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
spec = importlib.util.spec_from_file_location('a46_windows_docs', HERE / 'verify-windows-docs.py')
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class WindowsDocsTests(unittest.TestCase):
    rejected_mutations = 0

    @classmethod
    def setUpClass(cls):
        cls.reference, cls.a41 = checker.dependencies(REPO)
        cls.temp = tempfile.TemporaryDirectory(prefix='a46-docs-fixtures-')
        cls.scratch = Path(cls.temp.name)
        cls.source, cls.checkout, cls.guides, cls.proof = checker.source_contract(REPO, cls.scratch, cls.reference)
        cls.base = dict(cls.guides)
        cls.base.update({dest: cls.source[path] for path, dest in cls.reference.NOTICES.items()})
        cls.base.update({'licenses/JUCE-LICENSE.md': b'INERT JUCE LICENSE FIXTURE\n',
                         'SOURCE.txt': cls.a41.source_text(checker.SOURCE, '2026.11.7'),
                         'evidence/juce-provenance.txt': b'INERT PROVENANCE FIXTURE\n'})

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def rejects(self, call):
        with self.assertRaises((ValueError, KeyError, OSError)):
            call()
        type(self).rejected_mutations += 1

    def inventory(self, payloads):
        return {'inventory': [dict(path=path, type='file', mode='0o644', size=len(raw), sha256=checker.sha(raw))
                              for path, raw in payloads.items()]}

    def compare(self, payloads, receipt=None):
        return checker.compare_metadata(self.reference, receipt or self.inventory(payloads),
                                        payloads, self.source, self.checkout, self.guides)

    def test_both_common_modes_exact(self):
        raw = checker.compare_notices(self.source, self.checkout, self.source)
        crlf = checker.compare_notices(self.source, self.checkout, self.checkout)
        self.assertEqual(raw, 'all_raw_source_bytes')
        self.assertEqual(crlf, 'all_complete_lf_to_crlf_checkout_bytes')
        self.assertEqual(self.compare(self.base)[-1], raw)
        payloads = dict(self.base)
        payloads.update({dest: self.checkout[path] for path, dest in self.reference.NOTICES.items()})
        self.assertEqual(self.compare(payloads)[-1], crlf)

    def test_reject_every_notice_mixed_partial_bom_and_content_mutations(self):
        for name, raw in self.source.items():
            mutations = [
                ('one_crlf_among_raw', self.source, self.checkout[name]),
                ('one_raw_among_crlf', self.checkout, raw),
                ('partial_crlf', self.source, raw.replace(b'\n', b'\r\n', 1)),
                ('utf8_bom', self.source, b'\xef\xbb\xbf' + raw),
                ('text_change', self.source, raw + b'X'),
                ('removed_newline', self.source, raw.replace(b'\n', b'', 1)),
                ('extra_newline', self.source, raw + b'\n'),
                ('lone_cr', self.source, raw.replace(b'\n', b'\r', 1)),
            ]
            for mutation, base, bad in mutations:
                with self.subTest(path=name, mutation=mutation):
                    actual = dict(base)
                    actual[name] = bad
                    self.rejects(lambda: checker.compare_notices(self.source, self.checkout, actual))

    def test_reject_path_exceptions_and_changed_checkout(self):
        missing = dict(self.source)
        missing.pop(next(iter(missing)))
        self.rejects(lambda: checker.compare_notices(self.source, self.checkout, missing))
        extra = dict(self.source, **{'unreviewed_notice.txt': b'Extra\n'})
        self.rejects(lambda: checker.compare_notices(self.source, self.checkout, extra))
        for name in self.checkout:
            bad_checkout = dict(self.checkout)
            bad_checkout[name] += b'\r\n'
            self.rejects(lambda: checker.compare_notices(self.source, bad_checkout, self.source))

    def test_metadata_roster_modes_and_guides_are_not_normalized(self):
        for name in self.base:
            with self.subTest(path=name, mutation='missing'):
                bad = dict(self.base)
                bad.pop(name)
                self.rejects(lambda: self.compare(bad))
            for key, value in (('type', 'directory'), ('mode', '0o755'), ('size', 0)):
                with self.subTest(path=name, mutation=key):
                    receipt = self.inventory(self.base)
                    next(row for row in receipt['inventory'] if row['path'] == name)[key] = value
                    self.rejects(lambda: self.compare(self.base, receipt))
        for name, raw in self.guides.items():
            bad = dict(self.base)
            bad[name] = raw.replace(b'\n', b'\r\n')
            self.rejects(lambda: self.compare(bad))
        duplicate = self.inventory(self.base)
        duplicate['inventory'].append(dict(duplicate['inventory'][0]))
        self.rejects(lambda: self.compare(self.base, duplicate))
        changed = dict(self.base)
        changed['SOURCE.txt'] += b'changed\n'
        self.rejects(lambda: self.compare(changed, self.inventory(self.base)))

    def test_exact_passing_a41_context_is_required(self):
        receipt = dict(status='artifact_bytes_verified', source=checker.SOURCE, source_tree=checker.TREE,
                       preset_library_revision='2026.11.7', run_id=checker.RUN, run_attempt=1,
                       target='windows-x86_64', checker_sha256=checker.PINS[checker.A41],
                       source_scripts_loaded_from_commit=True, release_qualified=False,
                       native_validation_passed=None, artifact_attempt_binding_verified=False,
                       artifact=dict(id=1, name='cs80-windows-install-bundle'))
        checker.check_receipt(receipt)
        mutations = {'source': 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201',
                     'source_tree': '0' * 40, 'preset_library_revision': '2026.11.6',
                     'run_id': checker.RUN + 1, 'run_attempt': 2, 'target': 'linux-x86_64',
                     'status': 'pending', 'checker_sha256': '0' * 64,
                     'source_scripts_loaded_from_commit': False, 'release_qualified': True,
                     'native_validation_passed': True, 'artifact_attempt_binding_verified': True}
        for key, value in mutations.items():
            bad = copy.deepcopy(receipt)
            bad[key] = value
            self.rejects(lambda: checker.check_receipt(bad))
        for key, value in (('id', 0), ('id', True), ('name', 'cs80-pluginval-windows')):
            bad = copy.deepcopy(receipt)
            bad['artifact'][key] = value
            self.rejects(lambda: checker.check_receipt(bad))

    def test_preparation_preserves_false_qualification_flags(self):
        self.assertEqual(self.proof['source'], checker.SOURCE)
        self.assertEqual(self.proof['source_tree'], checker.TREE)
        self.assertEqual(len(self.proof['notices']), 7)
        self.assertEqual(len(self.proof['guides']), 3)
        for flag in ('actual_ci_git_configuration_observed', 'actual_artifacts_checked',
                     'native_results_accepted', 'platform_qualified', 'release_qualified',
                     'shared_index_or_refs_modified'):
            self.assertIs(self.proof[flag], False)
        self.assertEqual(self.proof['scoped_clean_diff_exit'], 0)

    def test_cli_refuses_overwrite_and_actual_inputs_in_prepare_mode(self):
        existing = self.scratch / 'existing.json'
        existing.write_bytes(b'PRESERVE\n')
        command = [sys.executable, '-B', str(HERE / 'verify-windows-docs.py'),
                   '--repo', str(REPO), '--prepare', '--out', str(existing)]
        result = subprocess.run(command, capture_output=True)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(existing.read_bytes(), b'PRESERVE\n')
        type(self).rejected_mutations += 1
        absent = self.scratch / 'absent.json'
        result = subprocess.run(command[:-1] + [str(absent), '--archive', str(existing)], capture_output=True)
        self.assertEqual(result.returncode, 2)
        self.assertFalse(absent.exists())
        type(self).rejected_mutations += 1


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(WindowsDocsTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    print(json.dumps(dict(scope='inert focused Windows notice/documentation fixtures; no actual archive or product executed',
                          methods=result.testsRun, failures=len(result.failures), errors=len(result.errors),
                          rejected_mutations=WindowsDocsTests.rejected_mutations,
                          actual_artifacts_checked=False, platform_qualified=False, release_qualified=False)))
    raise SystemExit(0 if result.wasSuccessful() else 1)
