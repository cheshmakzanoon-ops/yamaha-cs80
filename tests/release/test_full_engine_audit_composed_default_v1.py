#!/usr/bin/env python3
"""Isolated Git/source and synthetic coverage controls; not musical acceptance."""
from __future__ import annotations
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
R = '6eda5b0dee3b74e418d1c30517d4d7101fcb50f0'
REQUEST = '.github/full-engine-audit-request-composed-default-v1.json'
WRAPPER = Path(os.environ.get('CS80_AUDIT_COMPOSED_DEFAULT_V1_TEST_WRAPPER', ROOT / 'scripts/full_engine_audit_gate_composed_default_v1.py'))


class AuditComposedDefaultV1Tests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(WRAPPER.is_file(), 'The preregistered composed-default V1 wrapper is not implemented')
        self.temp = tempfile.TemporaryDirectory(prefix='cs80-audit-composed-default-v1-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'repo'
        subprocess.run(['git', 'clone', '--shared', '--quiet', '--no-checkout', str(ROOT), str(self.root)], check=True)
        self.git('config', 'user.name', 'CS80 composed-default V1 synthetic guard fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        self.git('update-ref', '--no-deref', 'HEAD', R)
        self.git('read-tree', R)
        self.request = json.loads(self.git('show', f'{R}:{REQUEST}'))
        paths = ['src', REQUEST, *self.request['original_v1_identity']['protected_files'],
                 *self.request['build_inputs']]
        self.git('checkout-index', '--force', '--', *self.git('ls-files', '--', *paths).splitlines())
        spec = importlib.util.spec_from_file_location('isolated_composed_default_v1_wrapper', WRAPPER)
        self.wrapper = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.wrapper)
        self.wrapper.REPO = self.root

    def git(self, *args, input=None):
        return subprocess.check_output(['git', '-C', str(self.root), *args], text=True, input=input,
                                       stderr=subprocess.PIPE).strip()

    def context(self):
        return self.wrapper.registered_context()

    def refuse(self, reason, action=None):
        with self.assertRaises(ValueError) as failure:
            (action or self.context)()
        self.assertIn(reason, str(failure.exception))

    def new_registration(self, **changes):
        request = dict(self.request, **changes)
        (self.root / REQUEST).write_text(json.dumps(request, indent=2) + '\n')
        self.git('add', REQUEST)
        self.git('commit', '--quiet', '-m', 'isolated altered registration fixture')
        self.wrapper.REGISTRATION_SHA = self.git('rev-parse', 'HEAD')

    def orphan(self):
        return self.git('commit-tree', self.git('rev-parse', f'{R}^{{tree}}'), input='isolated orphan fixture\n')

    def test_unmodified_published_registration_and_source_pass(self):
        context = self.context()
        self.assertEqual(context['request']['source_commit'], self.request['source_commit'])
        self.assertEqual(context['head'], R)
        self.assertEqual(sum(context['request']['suite_files'].values()), 885)

    def test_missing_registration_object(self):
        self.wrapper.REGISTRATION_SHA = '0' * 40
        self.refuse('missing-object:R')

    def test_missing_source_object(self):
        self.new_registration(source_commit='0' * 40)
        self.refuse('missing-object:F')

    def test_nonancestor_registration(self):
        self.wrapper.REGISTRATION_SHA = self.orphan()
        self.refuse('not-ancestor:R')

    def test_nonancestor_source(self):
        self.new_registration(source_commit=self.orphan())
        self.refuse('not-ancestor:F')

    def test_nonancestor_audio(self):
        self.new_registration(production_audio_commit=self.orphan())
        self.refuse('not-ancestor:audio')

    def test_wrong_registered_source_tree(self):
        self.new_registration(expected_src_tree='0' * 40)
        self.refuse('source-tree:F')

    def test_wrong_head_source_tree(self):
        path = self.root / 'src/core/Engine.cpp'
        path.write_text(path.read_text() + '\n// isolated changed-source control\n')
        self.git('add', 'src/core/Engine.cpp')
        self.git('commit', '--quiet', '-m', 'isolated changed source fixture')
        self.refuse('source-tree:HEAD')

    def test_unstaged_source(self):
        with (self.root / 'src/core/Engine.cpp').open('a') as stream: stream.write('\n// dirty\n')
        self.refuse('dirty-source')

    def test_staged_source(self):
        with (self.root / 'src/core/Engine.cpp').open('a') as stream: stream.write('\n// dirty\n')
        self.git('add', 'src/core/Engine.cpp')
        self.refuse('dirty-source')

    def test_nonignored_untracked_source(self):
        (self.root / 'src/untracked-control.cpp').write_text('// untracked\n')
        self.refuse('dirty-source')

    def test_ignored_untracked_source(self):
        (self.root / '.git/info/exclude').write_text('src/ignored-control.cpp\n')
        (self.root / 'src/ignored-control.cpp').write_text('// ignored\n')
        self.refuse('ignored-source')

    def test_assume_unchanged_cannot_hide_modified_source(self):
        self.git('update-index', '--assume-unchanged', 'src/core/Engine.cpp')
        with (self.root / 'src/core/Engine.cpp').open('a') as stream: stream.write('\n// hidden dirty source\n')
        self.assertEqual(self.git('status', '--porcelain=v1', '--untracked-files=all', '--', 'src'), '')
        self.refuse('source-file-bytes:src/core/Engine.cpp')

    def test_skip_worktree_cannot_hide_modified_source(self):
        self.git('update-index', '--skip-worktree', 'src/core/Engine.cpp')
        with (self.root / 'src/core/Engine.cpp').open('a') as stream: stream.write('\n// hidden dirty source\n')
        self.assertEqual(self.git('status', '--porcelain=v1', '--untracked-files=all', '--', 'src'), '')
        self.refuse('source-file-bytes:src/core/Engine.cpp')

    def test_staged_index_divergence_is_rejected_even_with_original_worktree_bytes(self):
        path = self.root / 'src/core/Engine.cpp'
        original = path.read_bytes()
        path.write_bytes(original + b'\n// staged-only changed source\n')
        self.git('add', 'src/core/Engine.cpp')
        path.write_bytes(original)
        self.refuse('dirty-source')

    def physical_source(self):
        guard = getattr(self.wrapper, 'source_physical_bytes', None)
        self.assertTrue(callable(guard), 'Physical source identity must not trust Git index flags or clean filters')
        return guard(self.request['source_commit'])

    def test_physical_source_rejects_leaf_symlink(self):
        path = self.root / 'src/core/Engine.cpp'
        saved = Path(self.temp.name) / 'original-engine.cpp'
        path.rename(saved)
        path.symlink_to(saved)
        self.refuse('source-file-type:src/core/Engine.cpp', self.physical_source)

    def test_physical_source_rejects_symlinked_ancestor(self):
        path = self.root / 'src/core'
        saved = Path(self.temp.name) / 'original-core'
        path.rename(saved)
        path.symlink_to(saved, target_is_directory=True)
        self.refuse('source-ancestor:src/core', self.physical_source)

    def test_physical_source_rejects_executable_mode_change(self):
        path = self.root / 'src/core/Engine.cpp'
        path.chmod(path.stat().st_mode | 0o111)
        self.refuse('source-file-mode:src/core/Engine.cpp', self.physical_source)

    def test_physical_source_rejects_missing_tracked_file(self):
        (self.root / 'src/core/Engine.cpp').unlink()
        self.refuse('source-file-type:src/core/Engine.cpp', self.physical_source)

    def test_modified_composed_default_v1_request(self):
        with (self.root / REQUEST).open('a') as stream: stream.write('\n')
        self.refuse('request-bytes')

    def test_original_v1_files_are_not_redefined_by_composed_default_v1(self):
        for relative in self.request['original_v1_identity']['protected_files']:
            with self.subTest(path=relative):
                path = self.root / relative
                original = path.read_bytes()
                path.write_bytes(original + b'\n')
                try: self.refuse('protected-file:' + relative)
                finally: path.write_bytes(original)

    def test_mutated_cmake_input(self):
        with (self.root / 'CMakeLists.txt').open('a') as stream: stream.write('\n# changed build\n')
        self.refuse('build-input:CMakeLists.txt')

    def test_changed_reporter_cannot_be_laundered_by_new_registration(self):
        relative = self.request['frozen_analysis']
        with (self.root / relative).open('a') as stream: stream.write('\n# re-registered changed reporter\n')
        self.git('add', relative)
        self.new_registration()
        self.refuse('protected-file:' + relative)

    def test_new_registration_cannot_weaken_original_inventory(self):
        self.new_registration(suite_files={**self.request['suite_files'], 'phrase.csv': 565})
        self.refuse('original-contract:suite_files')

    def capture(self):
        context = self.context()
        directory = self.root / 'artifacts/audit'
        directory.mkdir(parents=True)
        context['gate'].good_fixture(directory, context['reporter'])
        return context, directory

    def capture_refusal(self, filename, old, new, reason):
        context, directory = self.capture()
        path = directory / filename
        raw = path.read_text()
        self.assertIn(old, raw)
        path.write_text(raw.replace(old, new, 1))
        self.refuse(reason, lambda: context['gate'].validate_capture(directory, context['request'], context['reporter']))

    def test_complete_synthetic_inventory_is_accepted_not_musical_proof(self):
        context, directory = self.capture()
        files, _, _ = context['gate'].validate_capture(directory, context['request'], context['reporter'])
        self.assertEqual(sum(row['row_count'] for row in files.values()), 885)

    def test_missing_phrase_row(self):
        context, directory = self.capture()
        path = directory / 'phrase.csv'
        path.write_text('\n'.join(path.read_text().splitlines()[:-1]) + '\n')
        self.refuse('expected 566 complete rows', lambda: context['gate'].validate_capture(directory, context['request'], context['reporter']))

    def test_duplicate_phrase_program(self):
        self.capture_refusal('phrase.csv', 'Program-001', 'Program-000', '222 uniquely named programs')

    def test_mutated_header(self):
        self.capture_refusal('phrase.csv', 'name,group,policy', 'group,name,policy', 'Unexpected schema')

    def test_malformed_numeric(self):
        self.capture_refusal('phrase.csv', ',44100,', ',INVALID,', 'could not convert string')

    def test_invalid_policy(self):
        self.capture_refusal('phrase.csv', ',legacy,', ',mutant,', 'Unexpected policy')

    def test_wrong_causal_program(self):
        self.capture_refusal('click-causality.csv', 'Bass Stack', 'Wrong Bass', 'Unexpected causal program/event')

    def test_wrong_causal_event(self):
        self.capture_refusal('click-causality.csv', ',single_note_off,', ',note_on,', 'Unexpected causal program/event')

    def test_missing_causal_coordinate(self):
        self.capture_refusal('click-causality.csv', ',original_physical_panel,', ',mutant,', 'Missing or duplicated causality coordinate/rate')

    def test_duplicate_causal_rate(self):
        self.capture_refusal('click-causality.csv', ',44100,', ',48000,', 'Missing or duplicated causality coordinate/rate')

    def test_extra_suite(self):
        context, directory = self.capture()
        (directory / 'extra.csv').write_text('x\n')
        self.refuse('Unregistered extra CSV', lambda: context['gate'].validate_capture(directory, context['request'], context['reporter']))

    def test_stale_v1_completion(self):
        context = self.context()
        (self.root / 'artifacts').mkdir()
        (self.root / 'artifacts/audit.complete').write_text('stale\n')
        self.refuse('stale-completion', lambda: self.wrapper.finalize(context))

    def test_stale_composed_default_v1_completion(self):
        context = self.context()
        (self.root / 'artifacts').mkdir()
        (self.root / 'artifacts/audit-composed-default-v1.complete').write_text('stale\n')
        self.refuse('stale-completion', lambda: self.wrapper.finalize(context))

    def completed_fixture(self):
        # Use the actual frozen finalizer/reporter on a complete synthetic fixture.
        context, directory = self.capture()
        # The frozen reporter requires 14 ring-off Source/48k alias rows.
        path = directory / 'alias.csv'
        rows = path.read_text().splitlines()
        header = rows[0].split(',')
        for i in range(1, 15):
            cells = rows[i].split(',')
            for name, value in [('rate', '48000'), ('policy', 'source'), ('ring', '0')]:
                cells[header.index(name)] = value
            rows[i] = ','.join(cells)
        path.write_text('\n'.join(rows) + '\n')
        binary = self.root / 'build-core/cs80_engine_audit'
        binary.parent.mkdir()
        binary.write_bytes(b'explicitly synthetic nonexecutable fixture; not audio evidence\n')
        with mock.patch.dict(os.environ, {'GITHUB_SHA': context['head']}):
            context['gate'].finalize(context['request'], context['reporter'])
        return context

    def test_valid_v1_outputs_get_distinct_composed_default_v1_sidecar_and_last_marker(self):
        context = self.completed_fixture()
        destination = self.root / 'artifacts'
        before = {p.name: p.read_bytes() for p in destination.glob('audit*') if p.is_file()}
        self.wrapper.finish_composed_default_v1(context)
        for name, data in before.items(): self.assertEqual((destination / name).read_bytes(), data)
        sidecar = destination / 'audit-composed-default-v1-provenance.json'
        marker = destination / 'audit-composed-default-v1.complete'
        self.assertTrue(sidecar.is_file())
        self.assertIn(hashlib.sha256(sidecar.read_bytes()).hexdigest(), marker.read_text())
        self.assertIn('capture_not_numerical_acceptance', sidecar.read_text())
        self.assertLessEqual(sidecar.stat().st_mtime_ns, marker.stat().st_mtime_ns)

    def completion_refusal(self, filename, old, new, reason):
        context = self.completed_fixture()
        path = self.root / 'artifacts' / filename
        data = path.read_text()
        self.assertIn(old, data)
        path.write_text(data.replace(old, new, 1))
        self.refuse(reason, lambda: self.wrapper.finish_composed_default_v1(context))
        self.assertFalse((self.root / 'artifacts/audit-composed-default-v1.complete').exists())

    def test_completion_source_mismatch(self):
        self.completion_refusal('audit.complete', 'source_sha=' + R, 'source_sha=' + '0'*40, 'completion-source')

    def test_completion_registration_mismatch(self):
        self.completion_refusal('audit.complete', 'registration_commit=' + R,
                                'registration_commit=' + '0'*40, 'completion-registration')

    def test_completion_provenance_digest_mismatch(self):
        self.completion_refusal('audit.complete', 'provenance_sha256=', 'provenance_sha256=bad', 'completion-provenance')

    def test_completion_raw_csv_digest_mismatch(self):
        self.completion_refusal('audit/phrase.csv', 'Program-001', 'Altered-001', 'completion-raw:phrase.csv')

    def test_completion_binary_digest_mismatch(self):
        context = self.completed_fixture()
        (self.root / 'build-core/cs80_engine_audit').write_bytes(b'changed synthetic fixture\n')
        self.refuse('completion-binary', lambda: self.wrapper.finish_composed_default_v1(context))
        self.assertFalse((self.root / 'artifacts/audit-composed-default-v1.complete').exists())

    def test_sidecar_write_failure_cannot_leave_composed_default_v1_complete(self):
        context = self.completed_fixture()
        original_open = Path.open
        def fail_sidecar(path, *args, **kwargs):
            if path.name == 'audit-composed-default-v1-provenance.json':
                raise OSError('deliberate sidecar write refusal')
            return original_open(path, *args, **kwargs)
        with mock.patch.object(Path, 'open', fail_sidecar):
            with self.assertRaisesRegex(OSError, 'deliberate sidecar write refusal'):
                self.wrapper.finish_composed_default_v1(context)
        self.assertTrue((self.root / 'artifacts/audit.complete').is_file())
        self.assertFalse((self.root / 'artifacts/audit-composed-default-v1.complete').exists())

    def test_failed_reporter_cannot_emit_composed_default_v1_completion(self):
        context, _ = self.capture()
        with mock.patch.object(context['gate'], 'finalize', side_effect=ValueError('deliberate reporter refusal')):
            self.refuse('deliberate reporter refusal', lambda: self.wrapper.finalize(context))
        self.assertFalse((self.root / 'artifacts/audit-composed-default-v1.complete').exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
