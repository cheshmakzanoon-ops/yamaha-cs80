#!/usr/bin/env python3
"""Bounded synthetic contract checks; never build, execute or qualify a plugin."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import warnings
import zipfile

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[4]
HELPER = Path(__file__).with_name('a36-verify-validator-payload.py')
SPEC = importlib.util.spec_from_file_location('a36_payload_helper', HELPER)
helper = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(helper)
OBSERVATIONS = {'fixture_scope': 'Inert synthetic PE/ELF headers, resources and metadata only. No native plugin or CI artifact execution.',
                'release_qualified': False, 'native_validation_passed': None,
                'rejected_mutations': [], 'positive_controls': []}


class PayloadContracts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='a36-payload-probe-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source, self.run_id, self.attempt = 'a' * 40, 123, 2
        self.run_api = {'id': self.run_id, 'head_sha': self.source, 'run_attempt': self.attempt,
                        'status': 'completed', 'conclusion': 'success'}
        self.checker, _ = helper.load_checker(REPO, self.source, False)
        self.products = self.root / 'products'
        plugin = self.products / 'VST3' / helper.PRODUCT / 'Contents/x86_64-linux/CS-80 Reference.so'
        standalone = self.products / 'Standalone/CS-80 Reference'
        for path, executable in ((plugin, False), (standalone, True)):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(self.checker.packager.binary_fixture('linux', 'x86_64', executable))
            path.chmod(0o755)
        resource = self.products / 'VST3' / helper.PRODUCT / 'Contents/Resources/moduleinfo.json'
        resource.parent.mkdir(parents=True)
        resource.write_text('{"fixture":"a36 inert resource"}\n')
        self.install = self.root / 'install.zip'
        self.package()
        self.validator = self.root / 'validator.zip'
        helper.write_validator(self.validator, self.products)
        self.api = helper.fixture_api(self.validator, 'cs80-linux-vst3', self.source, self.run_id, 2)

    def package(self, extra=None):
        output = self.root / ('packages-' + str(len(list(self.root.glob('packages-*')))))
        metadata = {'preset_library_revision': '2026.11.5', 'workflow_run_id': str(self.run_id)}
        metadata.update(extra or {})
        self.checker.packager.package(self.products, output, 'linux', 'x86_64', self.source, metadata, {})
        with zipfile.ZipFile(self.install, 'w') as archive:
            for path in sorted(output.iterdir()):
                archive.write(path, path.name)
        self.install_api = helper.fixture_api(self.install, 'cs80-linux-install-bundle', self.source, self.run_id, 1)

    def verify(self, **kwargs):
        options = dict(repo=REPO, pin_scripts=False, revision='2026.11.5', attempt=self.attempt,
                       run_api=self.run_api)
        options.update(kwargs)
        return helper.verify(self.install, self.install_api, self.validator, self.api,
                             self.source, self.run_id, 'linux', **options)

    def rejected(self, label, action, message=None):
        with self.assertRaises((ValueError, OSError)) as captured:
            action()
        if message:
            self.assertIn(message, str(captured.exception))
        OBSERVATIONS['rejected_mutations'].append(label)

    def test_inherited_payload_and_sidecar_boundaries(self):
        result = helper.self_test(REPO)
        self.assertEqual(result['status'], 'synthetic_fixture_checks_passed')
        self.assertFalse(result['release_qualified'])
        OBSERVATIONS['inherited_inert_fixture_results'] = result

    def test_matching_payload_does_not_qualify_release(self):
        result = self.verify()
        self.assertFalse(result['release_qualified'])
        self.assertIsNone(result['native_validation_passed'])
        self.assertFalse(result['run_context']['artifact_attempt_binding_verified'])
        self.assertIsNone(result['manifest_attempt_declared'])
        self.assertEqual(result['revision'], '2026.11.5')
        self.assertEqual(result['attempt'], 2)
        self.assertEqual(result['files'], 2)
        OBSERVATIONS['positive_controls'].append('complete-inert-payload-with-external-gates')

    def test_run_context_mismatch(self):
        for field, value in [('id', 999), ('id', True), ('head_sha', 'b' * 40),
                             ('run_attempt', 1), ('run_attempt', True), ('run_attempt', None)]:
            api = dict(self.run_api, **{field: value})
            self.rejected(f'run-api-{field}-{value}', lambda: self.verify(run_api=api), 'Run API source/run/attempt mismatch')
        self.rejected('nonpositive-expected-attempt', lambda: self.verify(attempt=0), 'Invalid run attempt')

    def test_failed_run_is_observation_not_native_success(self):
        result = self.verify(run_api=dict(self.run_api, conclusion='failure'))
        self.assertEqual(result['run_context']['conclusion_observed'], 'failure')
        self.assertFalse(result['release_qualified'])
        self.assertIsNone(result['native_validation_passed'])
        OBSERVATIONS['positive_controls'].append('failed-run-context-never-promoted-to-native-success')

    def test_revision_and_optional_manifest_attempt(self):
        self.rejected('expected-revision-differs-from-manifest', lambda: self.verify(revision='2026.11.4'),
                      'Installation manifest target/revision/run mismatch')
        self.rejected('noncanonical-expected-revision', lambda: self.verify(revision='2026.11.0'),
                      'Invalid expected library revision')
        self.package({'workflow_run_attempt': '1'})
        self.rejected('declared-manifest-attempt-differs', self.verify, 'Installation manifest attempt mismatch')
        self.package({'workflow_run_attempt': '2'})
        result = self.verify()
        self.assertEqual(result['manifest_attempt_declared'], '2')
        self.assertFalse(result['run_context']['artifact_attempt_binding_verified'])
        OBSERVATIONS['positive_controls'].append('matching-declared-attempt-still-needs-authentic-upload-binding')

    def test_source_header_revision_binding(self):
        source = subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', 'HEAD'], text=True).strip()
        raw = subprocess.check_output(['git', '-C', str(REPO), 'show', source + ':src/core/FactoryPresets.h'])
        revision = re.findall(r'^inline constexpr const char\* curatedLibraryRevision = "([^"]+)";$',
                              raw.decode(), re.MULTILINE)[0]
        result = helper.check_source_revision(REPO, source, revision)
        self.assertEqual(result['sha256'], hashlib.sha256(raw).hexdigest())
        self.rejected('revision-differs-from-existing-source-header',
                      lambda: helper.check_source_revision(REPO, source, '2999.01'),
                      'Requested revision differs from exact source commit')
        OBSERVATIONS['source_header_fixture_anchor'] = {'source': source, 'revision': revision,
                                                      'scope': 'Existing committed header inspected only; no corresponding native artifact tested.'}

    def test_artifact_api_identity_and_digest(self):
        good = copy.deepcopy(self.api)
        mutations = [('id', 1), ('id', True), ('name', 'cs80-windows-vst3'), ('expired', True),
                     ('size_in_bytes', self.api['size_in_bytes'] + 1), ('digest', 'sha256:' + '0' * 64),
                     ('workflow_run', {'id': 999, 'head_sha': self.source}),
                     ('workflow_run', {'id': self.run_id, 'head_sha': 'b' * 40})]
        for index, (field, value) in enumerate(mutations):
            self.api = dict(good, **{field: value})
            self.rejected(f'validator-api-{index}-{field}', self.verify)
        self.api = good
        self.install_api['digest'] = 'sha256:' + '0' * 64
        self.rejected('installation-api-digest', self.verify, 'Archive SHA256 differs from API')

    def test_inventory_and_preflight_with_refreshed_api_digest(self):
        good = self.validator.read_bytes()
        with warnings.catch_warnings():
            warnings.simplefilter('ignore', UserWarning)
            with zipfile.ZipFile(self.validator, 'a') as archive:
                name = helper.PRODUCT + '/Contents/Resources/moduleinfo.json'
                archive.writestr(name, b'changed duplicate')
        self.api = helper.fixture_api(self.validator, 'cs80-linux-vst3', self.source, self.run_id, 2)
        self.rejected('duplicate-validator-member-valid-api-digest', self.verify, 'Duplicate archive member')
        raw = bytearray(good)
        end = raw.rfind(b'PK\x05\x06')
        struct.pack_into('<HH', raw, end + 8, 65535, 65535)
        self.validator.write_bytes(raw)
        self.api = helper.fixture_api(self.validator, 'cs80-linux-vst3', self.source, self.run_id, 2)
        self.rejected('excessive-central-directory-count-valid-api-digest', self.verify, 'ZIP directory')
        self.validator.write_bytes(good + b'unexpected trailing data')
        self.api = helper.fixture_api(self.validator, 'cs80-linux-vst3', self.source, self.run_id, 2)
        self.rejected('trailing-zip-data-valid-api-digest', self.verify, 'ZIP directory')

    def test_snapshot_binds_hash_to_inspected_bytes(self):
        original = helper.snapshot
        replacement = self.root / 'replacement.zip'
        helper.write_validator(replacement, self.products, 'changed-binary')
        changed = replacement.read_bytes()

        def replace_after_read(path, limit):
            raw = original(path, limit)
            if Path(path) == self.validator:
                self.validator.write_bytes(changed)
            return raw

        with patch.object(helper, 'snapshot', replace_after_read):
            result = self.verify()
        expected = self.api['digest'].split(':', 1)[1]
        self.assertEqual(result['validator_input_artifact']['sha256'], expected)
        self.assertNotEqual(helper.sha_file(self.validator), expected)
        self.rejected('mutated-path-after-snapshot-next-inspection', self.verify, 'differs from API')
        OBSERVATIONS['positive_controls'].append('post-snapshot-path-replacement-cannot-change-inspected-bytes')

    def test_nonregular_and_oversized_snapshot_inputs(self):
        self.rejected('oversized-snapshot', lambda: helper.snapshot(self.validator, 1), 'oversized input')
        if hasattr(os, 'mkfifo'):
            fifo = self.root / 'fifo'
            os.mkfifo(fifo)
            self.rejected('fifo-snapshot-without-blocking', lambda: helper.snapshot(fifo, 1024), 'nonregular')
        if getattr(os, 'O_NOFOLLOW', 0):
            link = self.root / 'link'
            link.symlink_to(self.validator)
            self.rejected('symlink-snapshot', lambda: helper.snapshot(link, helper.MAX_ARCHIVE))

    def test_metadata_snapshot_and_duplicate_keys(self):
        path = self.root / 'api.json'
        raw = b'{"id":123, "head_sha":"fixture"}\n'
        path.write_bytes(raw)
        result, identity = helper.load_json_snapshot(path)
        self.assertEqual(result['id'], 123)
        self.assertEqual(identity['sha256'], hashlib.sha256(raw).hexdigest())
        path.write_text('{"id":1,"id":2}')
        self.rejected('duplicate-api-json-key', lambda: helper.load_json(path), 'Duplicate JSON key')
        path.write_text('[]')
        self.rejected('nonobject-api-json', lambda: helper.load_json(path), 'must be an object')

    def test_report_publication_never_replaces_existing_evidence(self):
        path = self.root / 'evidence/result.json'
        helper.publish_report(path, '{"first":true}\n')
        self.rejected('existing-report-output', lambda: helper.publish_report(path, 'replacement'))
        self.assertEqual(path.read_text(), '{"first":true}\n')
        self.assertEqual(list(path.parent.glob('.a36-payload-*')), [])
        concurrent = self.root / 'concurrent.json'

        def publish(raw):
            try:
                helper.publish_report(concurrent, raw)
                return 'published'
            except FileExistsError:
                return 'refused'

        with ThreadPoolExecutor(max_workers=2) as executor:
            outcomes = list(executor.map(publish, ['first complete report', 'second complete report']))
        self.assertCountEqual(outcomes, ['published', 'refused'])
        self.assertIn(concurrent.read_text(), ['first complete report', 'second complete report'])
        OBSERVATIONS['positive_controls'].append('concurrent-report-writers-publish-once-without-overwrite')

    def test_cli_requires_explicit_context(self):
        values = {'--install-archive': str(self.install), '--install-api': 'install-api.json',
                  '--validator-archive': str(self.validator), '--validator-api': 'validator-api.json',
                  '--source': self.source, '--revision': '2026.11.5', '--run-id': '123', '--attempt': '2',
                  '--run-api': 'run-api.json', '--platform': 'linux', '--out': str(self.root / 'result.json')}
        for omitted in ('--source', '--revision', '--attempt', '--run-api'):
            args = [part for key, value in values.items() if key != omitted for part in (key, value)]
            process = subprocess.run([sys.executable, '-B', str(HELPER), *args], cwd=REPO,
                                     capture_output=True, text=True, timeout=15)
            self.assertEqual(process.returncode, 1)
            self.assertIn('Missing payload-inspection argument', process.stderr)
            OBSERVATIONS['rejected_mutations'].append('cli-missing-' + omitted[2:])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(PayloadContracts)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    OBSERVATIONS.update({'status': 'synthetic_contract_checks_passed' if result.wasSuccessful() else 'synthetic_contract_checks_failed',
                         'tests_run': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
                         'helper_sha256': helper.sha_file(HELPER), 'probe_sha256': helper.sha_file(Path(__file__))})
    helper.publish_report(args.out, json.dumps(OBSERVATIONS, indent=2) + '\n')
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(main())
