#!/usr/bin/env python3
"""Bounded inert fixtures only: these checks do not execute a plugin or qualify CI."""
from __future__ import annotations
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
SOURCE = '6094bce18c8b19da804c2fbc9a00e31969a7f579'
REVISION = '2026.11.5'
RUN = 123
ATTEMPT = 2
REPOSITORY_ID = 900000001
OBSERVATIONS = []


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


helper = load('a41_helper', HERE / 'a41-verify-install-artifact.py')
legacy = load('a41_inert_fixture_builder', REPO / 'docs/preset-evidence/2026.11.4/ci/verification-tools/verify-install-artifact.py')
# Only its inert fixture builder is reused; no historical file is changed and
# no historical acceptance result is used. Current helper accepts explicit revision.
legacy.REPO = REPO
legacy.REVISION = REVISION


class InstallArtifactTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='a41-fixture-')
        self.root = Path(self.temp.name)
        self.counter = 0

    def tearDown(self):
        self.temp.cleanup()

    def fixture(self, system='linux', architecture='x86_64', wrong_header=False):
        self.counter += 1
        directory = self.root / str(self.counter)
        archive, api = legacy.fixture(directory, system, architecture, SOURCE, RUN, wrong_header)
        api['id'] = 1000 + self.counter
        api['workflow_run'].update(repository_id=REPOSITORY_ID, head_repository_id=REPOSITORY_ID)
        api['url'] = f'https://api.github.com/repos/{helper.REPOSITORY}/actions/artifacts/{api["id"]}'
        api['archive_download_url'] = api['url'] + '/zip'
        api['created_at'] = '2026-11-24T00:00:00Z'
        api['expires_at'] = '2027-02-22T00:00:00Z'
        run = {'id': RUN, 'head_sha': SOURCE, 'run_attempt': ATTEMPT,
               'repository': {'id': REPOSITORY_ID, 'full_name': helper.REPOSITORY},
               'head_repository': {'id': REPOSITORY_ID, 'full_name': helper.REPOSITORY},
               'status': 'completed', 'conclusion': 'success'}
        expected = {'source': SOURCE, 'revision': REVISION, 'run_id': RUN, 'attempt': ATTEMPT,
                    'platform': system, 'architecture': architecture, 'repository': helper.REPOSITORY}
        artifact_api, run_api = directory / 'artifact-api.json', directory / 'run-api.json'
        artifact_api.write_text(json.dumps(api))
        run_api.write_text(json.dumps(run))
        return archive, artifact_api, run_api, expected

    def inspect(self, fixture):
        return helper.verify(*fixture, repo=REPO)

    def reject(self, fixture, reason):
        with self.assertRaises((ValueError, OSError, KeyError)) as caught:
            self.inspect(fixture)
        OBSERVATIONS.append({'kind': 'rejected_fixture', 'reason': reason, 'error': str(caught.exception)})

    def test_all_four_native_header_targets(self):
        for system, architecture in helper.ARTIFACTS:
            with self.subTest(system=system, architecture=architecture):
                result = self.inspect(self.fixture(system, architecture))
                self.assertEqual(len(result['binary_headers']), 3 if system == 'macos' else 2)
                self.assertFalse(result['release_qualified'])
                self.assertFalse(result['artifact_attempt_binding_verified'])
                self.assertIsNone(result['native_validation_passed'])
                self.assertEqual(result['source'], SOURCE)
                self.assertEqual(result['preset_library_revision'], REVISION)
                self.assertEqual(result['run_attempt'], ATTEMPT)
                self.assertEqual(len(result['inventory']), result['inventory_entries'])
                self.assertTrue(result['source_scripts_loaded_from_commit'])
                OBSERVATIONS.append({'kind': 'accepted_inert_fixture', 'target': result['target'],
                                     'header_count': len(result['binary_headers']), 'release_qualified': False})

    def test_coherent_wrong_cpu_on_each_target(self):
        for target in helper.ARTIFACTS:
            self.reject(self.fixture(*target, wrong_header=True), 'internally coherent wrong CPU: ' + '-'.join(target))

    def test_metadata_identity_mutations(self):
        cases = [
            ('artifact', ('digest',), 'sha256:' + '0' * 64),
            ('artifact', ('size_in_bytes',), 1),
            ('artifact', ('name',), 'unrelated-install-bundle'),
            ('artifact', ('expired',), True),
            ('artifact', ('workflow_run', 'head_sha'), 'b' * 40),
            ('artifact', ('workflow_run', 'id'), RUN + 1),
            ('artifact', ('workflow_run', 'repository_id'), REPOSITORY_ID + 1),
            ('artifact', ('workflow_run', 'run_attempt'), ATTEMPT + 1),
            ('artifact', ('archive_download_url',), 'https://api.github.com/repos/other/repository/actions/artifacts/1/zip'),
            ('run', ('head_sha',), 'b' * 40),
            ('run', ('run_attempt',), ATTEMPT + 1),
            ('run', ('repository', 'full_name'), 'other/repository'),
            ('run', ('head_repository', 'id'), REPOSITORY_ID + 1),
        ]
        for which, keys, value in cases:
            fixture = self.fixture()
            path = fixture[1 if which == 'artifact' else 2]
            data = json.loads(path.read_text())
            child = data
            for key in keys[:-1]: child = child[key]
            child[keys[-1]] = value
            path.write_text(json.dumps(data))
            self.reject(fixture, which + ':' + '.'.join(keys))

    def test_revision_must_match_exact_committed_source(self):
        fixture = self.fixture()
        fixture[3]['revision'] = '2026.11.4'
        self.reject(fixture, 'explicit revision does not match source declaration')

    def test_duplicate_json_keys_rejected(self):
        fixture = self.fixture()
        original = fixture[1].read_text()
        fixture[1].write_text('{"id":999,' + original[1:])
        self.reject(fixture, 'duplicate artifact JSON key')

    def test_extra_outer_member_rejected(self):
        fixture = self.fixture()
        archive, api_path = fixture[:2]
        with zipfile.ZipFile(archive, 'a') as output:
            output.writestr('extra.txt', 'unadvertised')
        api = json.loads(api_path.read_text())
        raw = archive.read_bytes()
        api.update(size_in_bytes=len(raw), digest='sha256:' + helper.digest(raw))
        api_path.write_text(json.dumps(api))
        self.reject(fixture, 'extra outer member despite updated authentic-byte-style metadata')

    def test_corrupt_inner_payload_rejected_with_matching_outer_digest(self):
        fixture = self.fixture('windows', 'x86_64')
        archive, api_path = fixture[:2]
        with zipfile.ZipFile(archive) as outer:
            members = [(entry, outer.read(entry)) for entry in outer.infolist()]
        changed = []
        for entry, raw in members:
            if entry.filename.endswith('.zip'):
                with zipfile.ZipFile(io.BytesIO(raw)) as inner:
                    contents = [(member, inner.read(member)) for member in inner.infolist()]
                replacement = io.BytesIO()
                with zipfile.ZipFile(replacement, 'w') as output:
                    for member, body in contents:
                        if member.filename.endswith('SOURCE.txt'):
                            body = body.replace(SOURCE.encode(), b'b' * 40)
                        output.writestr(member, body)
                raw = replacement.getvalue()
            changed.append((entry, raw))
        with zipfile.ZipFile(archive, 'w') as output:
            for entry, raw in changed: output.writestr(entry, raw)
        api = json.loads(api_path.read_text())
        raw = archive.read_bytes()
        api.update(size_in_bytes=len(raw), digest='sha256:' + helper.digest(raw))
        api_path.write_text(json.dumps(api))
        self.reject(fixture, 'inner payload checksum corruption while outer digest agrees')

    def test_hash_and_inspection_share_external_snapshots(self):
        fixture = self.fixture()
        archive, api_path, run_path, _ = fixture
        before = {path.name: helper.digest(path.read_bytes()) for path in fixture[:3]}
        original = helper.load_source_contract
        def change_inputs_after_snapshot(*args, **kwargs):
            for path in fixture[:3]: path.write_bytes(b'changed after snapshot')
            return original(*args, **kwargs)
        with mock.patch.object(helper, 'load_source_contract', side_effect=change_inputs_after_snapshot):
            result = self.inspect(fixture)
        self.assertEqual(result['outer_zip']['sha256'], before[archive.name])
        self.assertEqual(result['input_metadata']['artifact_api']['sha256'], before[api_path.name])
        self.assertEqual(result['input_metadata']['run_api']['sha256'], before[run_path.name])
        OBSERVATIONS.append({'kind': 'snapshot_control', 'inputs_changed_after_read': 3, 'same_bytes_hashed_and_inspected': True})

    def test_failed_run_context_never_becomes_native_pass(self):
        fixture = self.fixture()
        path = fixture[2]
        data = json.loads(path.read_text())
        data['conclusion'] = 'failure'
        path.write_text(json.dumps(data))
        result = self.inspect(fixture)
        self.assertEqual(result['run_context']['conclusion_observed'], 'failure')
        self.assertFalse(result['release_qualified'])
        self.assertIsNone(result['native_validation_passed'])
        OBSERVATIONS.append({'kind': 'scope_control', 'run_conclusion': 'failure', 'release_qualified': False})

    def test_report_publication_refuses_overwrite(self):
        path = self.root / 'report.json'
        helper.publish_report(path, '{"first":true}\n')
        before = path.read_bytes()
        with self.assertRaises(FileExistsError): helper.publish_report(path, '{"second":true}\n')
        self.assertEqual(path.read_bytes(), before)
        dangling = self.root / 'dangling.json'
        dangling.symlink_to(self.root / 'absent')
        with self.assertRaises(FileExistsError): helper.publish_report(dangling, '{}\n')
        self.assertTrue(dangling.is_symlink())
        self.assertEqual(list(self.root.glob('.a41-*')), [])
        OBSERVATIONS.append({'kind': 'publication_control', 'existing_file_and_dangling_symlink_preserved': True})

    def test_nonregular_archive_rejected(self):
        fixture = self.fixture()
        linked = fixture[0].with_name('linked.zip')
        linked.symlink_to(fixture[0])
        self.reject((linked, *fixture[1:]), 'symlink external archive')


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(InstallArtifactTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    summary = {'scope': 'Inert synthetic header/archive and supplied-receipt fixtures; no product execution, real CI result, download authenticity or release qualification.',
               'committed_source_used_for_checker_loading': SOURCE,
               'tests_run': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
               'positive_target_fixtures': sum(x['kind'] == 'accepted_inert_fixture' for x in OBSERVATIONS),
               'rejected_mutations': sum(x['kind'] == 'rejected_fixture' for x in OBSERVATIONS),
               'observations': OBSERVATIONS,
               'helper_sha256': helper.digest((HERE / 'a41-verify-install-artifact.py').read_bytes()),
               'probe_sha256': helper.digest(Path(__file__).read_bytes())}
    output = HERE / 'a41-synthetic-results.json'
    with output.open('x') as stream:
        json.dump(summary, stream, indent=2)
        stream.write('\n')
    raise SystemExit(0 if result.wasSuccessful() else 1)
