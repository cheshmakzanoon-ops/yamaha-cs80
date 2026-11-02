#!/usr/bin/env python3
"""Real Git/index witnesses for required-notice origin, never binary acceptance."""
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import stat
import unittest
from unittest import mock

import test_distribution_docs as fixtures

ROOT = fixtures.ROOT


class DistributionSourceIdentityTests(unittest.TestCase):
    git = fixtures.DistributionDocsTests.git

    def setUp(self):
        fixtures.DistributionDocsTests.setUp(self)
        self.stager = fixtures.load_module('notice_origin_stager', ROOT / 'scripts/distribution_docs.py')
        self.verifier = fixtures.load_module('notice_origin_verifier', ROOT / 'scripts/verify_distribution_docs.py')
        self.sequence = 0

    def destination(self):
        self.sequence += 1
        return self.build / ('origin-payload-' + str(self.sequence))

    def stage(self, output):
        return self.stager.stage_documents(self.root, self.license, output)

    def verify(self, output):
        return self.verifier.verify_documents(self.root, self.license, output, self.commit)

    def context(self):
        return fixtures.DistributionDocsTests.context(self)

    @contextmanager
    def hidden(self, relative, flag, data=None, mode=None):
        path = self.root / relative
        original, original_mode = path.read_bytes(), stat.S_IMODE(path.stat().st_mode)
        self.git('update-index', flag, '--', relative)
        try:
            if data is not None:
                path.write_bytes(data)
            if mode is not None:
                path.chmod(mode)
            self.assertEqual(self.git('status', '--porcelain=v1', '--untracked-files=all').strip(), '')
            yield path
        finally:
            path.write_bytes(original)
            path.chmod(original_mode)
            self.git('update-index', '--no-' + flag[2:], '--', relative)

    @staticmethod
    def forge_matching_payload(output, name, data):
        (output / name).write_bytes(data)
        path = output / 'distribution-manifest.json'
        manifest = json.loads(path.read_text())
        row = next(row for row in manifest['files'] if row['path'] == name)
        row.update(size=len(data), sha256=hashlib.sha256(data).hexdigest())
        path.write_text(json.dumps(manifest))

    def test_unchanged_notices_with_index_hints_are_allowed(self):
        for flag in ('--assume-unchanged', '--skip-worktree'):
            with self.subTest(flag=flag), self.hidden('LICENSE', flag):
                output = self.destination()
                self.stage(output)
                self.assertEqual(self.verify(output)['document_count'], 7)
        # The checkout root is deliberately canonicalized. This is not the
        # same as accepting a linked notice leaf/ancestor inside that checkout.
        if os.name != 'nt':
            alias = self.root.with_name('canonical-checkout-alias')
            alias.symlink_to(self.root, target_is_directory=True)
            output = self.destination()
            with self.subTest(root='canonicalized top-level alias'):
                manifest = self.stager.stage_documents(alias, self.license, output)
                verified = self.verifier.verify_documents(alias, self.license, output, self.commit)
                self.assertEqual(manifest['source_commit'], self.commit)
                self.assertEqual(verified['document_count'], 7)

    def test_stager_rejects_changed_required_notice_behind_index_hint(self):
        for relative in fixtures.INPUTS.values():
            for flag in ('--assume-unchanged', '--skip-worktree'):
                with self.subTest(path=relative, flag=flag), self.hidden(relative, flag, b'altered required source notice\n'):
                    with self.assertRaisesRegex(ValueError, 'source|revision|blob|notice'):
                        self.stage(self.destination())

    def test_ci_archive_rejects_changed_required_notice_behind_index_hint(self):
        for relative in fixtures.INPUTS.values():
            for flag in ('--assume-unchanged', '--skip-worktree'):
                with self.subTest(path=relative, flag=flag), self.hidden(relative, flag, b'altered required source notice\n'):
                    with self.assertRaisesRegex(ValueError, 'source|revision|blob|notice'):
                        self.context()

    def test_independent_verifier_rejects_self_consistent_hidden_source_substitution(self):
        for name, relative in fixtures.INPUTS.items():
            for flag in ('--assume-unchanged', '--skip-worktree'):
                output = self.destination()
                self.stage(output)
                data = b'counterfeit source and matching extracted payload\n'
                with self.subTest(path=relative, flag=flag), self.hidden(relative, flag, data):
                    self.forge_matching_payload(output, name, data)
                    with self.assertRaisesRegex(ValueError, 'source|revision|blob|notice'):
                        self.verify(output)

    def test_ci_archive_status_document_is_bound_to_revision(self):
        for flag in ('--assume-unchanged', '--skip-worktree'):
            with self.subTest(flag=flag), self.hidden('docs/CI_DISTRIBUTION_STATUS.md', flag, b'counterfeit acceptance claim\n'):
                with self.assertRaisesRegex(ValueError, 'source|revision|blob|notice'):
                    self.context()

    def stage_index_divergence(self):
        path = self.root / 'LICENSE'
        original = path.read_bytes()
        path.write_bytes(b'staged but not present in worktree\n')
        self.git('add', 'LICENSE')
        path.write_bytes(original)
        self.assertNotEqual(self.git('diff', '--cached', '--name-only', 'HEAD', '--', 'LICENSE').strip(), '')
        self.assertEqual(self.git('diff', '--name-only', 'HEAD', '--', 'LICENSE').strip(), '')

    def test_stager_rejects_staged_index_when_worktree_matches_head(self):
        self.stage_index_divergence()
        with self.assertRaisesRegex(ValueError, '[Ss]ource|dirty|commit|index'):
            self.stage(self.destination())

    def test_verifier_rejects_staged_index_when_worktree_matches_head(self):
        # The verifier independently checks the index even with a preexisting
        # pristine payload; do not let manifest source agreement bypass it.
        output = self.destination(); self.stage(output)
        self.stage_index_divergence()
        with self.assertRaisesRegex(ValueError, '[Ss]ource|dirty|commit|index'):
            self.verify(output)

    @unittest.skipIf(os.name == 'nt', 'Windows does not expose Git POSIX executable modes')
    def test_notice_executable_mode_is_bound_without_trusting_core_filemode(self):
        output = self.destination(); self.stage(output)
        self.git('config', 'core.filemode', 'false')
        with self.hidden('LICENSE', '--assume-unchanged', mode=0o755):
            with self.subTest(consumer='stager'), self.assertRaisesRegex(ValueError, 'mode|source|notice'):
                self.stage(self.destination())
            with self.subTest(consumer='verifier'), self.assertRaisesRegex(ValueError, 'mode|source|notice'):
                self.verify(output)

    @unittest.skipIf(os.name == 'nt', 'Native link creation needs separate Windows privilege evidence')
    def test_linked_notice_leaf_and_ancestor_remain_rejected(self):
        output = self.destination(); self.stage(output)
        relative = 'assets/fonts/barlow-condensed/OFL.txt'; path = self.root / relative
        original = path.read_bytes(); target = self.build / 'outside-notice'; target.write_bytes(original)
        self.git('update-index', '--assume-unchanged', '--', relative)
        path.unlink(); path.symlink_to(target)
        try:
            for name, action in [('stager', lambda: self.stage(self.destination())), ('verifier', lambda: self.verify(output))]:
                with self.subTest(consumer=name, alias='leaf'), self.assertRaisesRegex(ValueError, 'source|notice|[Ll]ink'):
                    action()
        finally:
            path.unlink(); path.write_bytes(original); self.git('update-index', '--no-assume-unchanged', '--', relative)
        directory = path.parent; saved = directory.with_name('saved-notice-dir'); directory.rename(saved); directory.symlink_to(saved, target_is_directory=True)
        try:
            for name, action in [('stager', lambda: self.stage(self.destination())), ('verifier', lambda: self.verify(output))]:
                with self.subTest(consumer=name, alias='ancestor'), self.assertRaisesRegex(ValueError, '[Ss]ource|notice|[Ll]ink'):
                    action()
        finally:
            directory.unlink(); saved.rename(directory)

    def test_exact_windows_autocrlf_checkout_is_preserved_not_normalized(self):
        self.git('config', 'core.autocrlf', 'true')
        expected = fixtures.REQUIRED['LICENSE'].replace(b'\n', b'\r\n')
        with self.hidden('LICENSE', '--assume-unchanged', expected), \
             mock.patch.object(self.stager.sys, 'platform', 'win32'):
            output = self.destination(); self.stage(output)
            self.assertEqual((output / 'LICENSE').read_bytes(), expected)
            self.assertEqual(self.verify(output)['document_count'], 7)

    def test_windows_crlf_is_not_permission_to_change_text(self):
        self.git('config', 'core.autocrlf', 'true')
        output = self.destination(); self.stage(output)
        changed = b'changed text with windows line endings\r\n'
        with self.hidden('LICENSE', '--assume-unchanged', changed), \
             mock.patch.object(self.stager.sys, 'platform', 'win32'):
            self.forge_matching_payload(output, 'LICENSE', changed)
            with self.subTest(consumer='stager'), self.assertRaisesRegex(ValueError, 'source|revision|blob|notice'):
                self.stage(self.destination())
            with self.subTest(consumer='verifier'), self.assertRaisesRegex(ValueError, 'source|revision|blob|notice'):
                self.verify(output)

    def test_unregistered_crlf_conversion_is_rejected(self):
        self.git('config', 'core.autocrlf', 'false')
        output = self.destination(); self.stage(output)
        converted = fixtures.REQUIRED['LICENSE'].replace(b'\n', b'\r\n')
        with self.hidden('LICENSE', '--assume-unchanged', converted), \
             mock.patch.object(self.stager.sys, 'platform', 'win32'):
            self.forge_matching_payload(output, 'LICENSE', converted)
            with self.subTest(consumer='stager'), self.assertRaisesRegex(ValueError, 'source|revision|blob|notice'):
                self.stage(self.destination())
            with self.subTest(consumer='verifier'), self.assertRaisesRegex(ValueError, 'source|revision|blob|notice'):
                self.verify(output)


if __name__ == '__main__':
    unittest.main(verbosity=2)
