#!/usr/bin/env python3
"""Offline publisher refusal fixtures. No network, releases or binary execution."""
from __future__ import annotations
from copy import deepcopy
import importlib.util
import io
import json
import os
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch
import urllib.request
import warnings
import zipfile


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


HERE = Path(__file__).resolve().parent
publisher = load('publisher_fixture', HERE / 'publish-preset-refinement.py')
notices = load('notice_fixtures', publisher.ROOT / 'scripts/test-packaging-notices.py')
SOURCE, PUBLICATION, OTHER = 'a' * 40, 'b' * 40, 'c' * 40
RUN = 123
API = '/repos/' + publisher.REPO


def run_record():
    return {'id': RUN, 'head_sha': SOURCE, 'name': 'build', 'path': '.github/workflows/build.yml',
            'head_branch': 'main', 'event': 'push', 'head_repository': {'full_name': publisher.REPO},
            'status': 'completed', 'conclusion': 'success', 'run_attempt': 1}


def job_records():
    return [{'id': i, 'run_id': RUN, 'run_attempt': 1, 'head_sha': SOURCE, 'name': name, 'status': 'completed',
             'conclusion': 'success', 'steps': [{'name': s, 'conclusion': 'success'} for s in sorted(steps)]}
            for i, (name, steps) in enumerate(publisher.JOB_STEPS.items(), 1)]


class TransactionClient:
    def __init__(self):
        self.release = None
        self.assets = {}
        self.writes = []
        self.events = []
        self.fail_job = False
        self.duplicate_artifact = False
        self.run_attempt = 1
        self.job_attempt = 1
        self.event_attempt = 1
        names = sorted(set(publisher.BUNDLES) | {'cs80-curated-preset-audition-' + SOURCE})
        self.artifacts = [dict(id=i, name=name, expired=False, workflow_run={'id': RUN, 'head_sha': SOURCE},
                               digest='sha256:' + publisher.sha(name.encode()), size_in_bytes=len(name.encode()))
                          for i, name in enumerate(names, 1)]

    def json(self, path, method='GET', body=None):
        if method != 'GET':
            self.writes.append((method, path))
            self.events.append('write:' + method)
        if path == API + '/actions/runs/' + str(RUN):
            return dict(id=RUN, head_sha=SOURCE, name='build', path='.github/workflows/build.yml',
                        head_branch='main', event='push', head_repository={'full_name': publisher.REPO},
                        status='completed', conclusion='success', run_attempt=self.run_attempt,
                        html_url='https://example.invalid/build')
        if path == API + '/git/ref/heads/main':
            return {'object': {'sha': PUBLICATION}}
        if path.startswith(API + '/compare/'):
            return dict(merge_base_commit={'sha': SOURCE}, status='ahead', files=[dict(filename='docs/report.md', status='added')])
        if path.startswith(API + '/git/matching-refs/'):
            return [] if self.release is None else [{'ref': 'refs/tags/' + publisher.TAG, 'object': {'type': 'commit', 'sha': PUBLICATION}}]
        if path == API + '/git/ref/tags/' + publisher.TAG:
            return {'object': {'type': 'commit', 'sha': PUBLICATION}}
        if path == API + '/releases' and method == 'POST':
            self.release = dict(body, id=77, upload_url='https://uploads.github.com/repos/' + publisher.REPO + '/releases/77/assets{?name,label}', html_url='https://example.invalid/release')
            return deepcopy(self.release)
        if path == API + '/releases/77' and method == 'PATCH':
            self.release.update(body)
            return deepcopy(self.release)
        raise AssertionError((method, path))

    def pages(self, path, key=None):
        if '/jobs?' in path:
            rows = [dict(id=i, run_id=RUN, run_attempt=self.job_attempt, head_sha=SOURCE, name=name, status='completed', conclusion='success',
                         steps=[dict(name=s, conclusion='success') for s in sorted(steps)])
                    for i, (name, steps) in enumerate(publisher.JOB_STEPS.items(), 1)]
            if self.fail_job:
                rows[0]['conclusion'] = 'failure'
            return rows
        if path == API + '/releases':
            return [] if self.release is None else [deepcopy(self.release)]
        if path.endswith('/artifacts'):
            return deepcopy(self.artifacts + (self.artifacts[:1] if self.duplicate_artifact else []))
        if path == API + '/releases/77/assets':
            return [deepcopy(item['metadata']) for item in self.assets.values()]
        raise AssertionError(path)

    def download(self, path, target, digest, size):
        artifact_id = int(path.split('/')[-2])
        row = next(x for x in self.artifacts if x['id'] == artifact_id)
        payload = row['name'].encode()
        assert publisher.sha(payload) == digest and len(payload) == size
        target.write_bytes(payload)
        self.events.append('download')

    def request(self, url, method, data, binary=False):
        assert method == 'POST' and binary
        name = publisher.urllib.parse.parse_qs(publisher.urllib.parse.urlsplit(url).query)['name'][0]
        assert name not in self.assets, 'An existing asset would be overwritten'
        self.writes.append((method, url))
        self.events.append('write:upload')
        metadata = dict(name=name, state='uploaded', size=len(data), digest='sha256:' + publisher.sha(data),
                        browser_download_url='https://example.invalid/' + name)
        self.assets[name] = {'metadata': metadata, 'bytes': data}
        return io.BytesIO(json.dumps(metadata).encode())


def invoke_transaction(client, fail_last_verifier=False):
    verified = []
    def verify(path, *args):
        verified.append(path.name)
        client.events.append('verify')
        if fail_last_verifier and len(verified) == 5:
            raise ValueError('Injected fifth payload failure')
        return {'synthetic_review_payload': True}
    with tempfile.TemporaryDirectory() as work:
        event = Path(work) / 'event.json'
        event.write_text(json.dumps({'workflow_run': {'id': RUN, 'head_sha': SOURCE, 'run_attempt': client.event_attempt}}))
        env = {'PUBLISH_BINARY_SHA': SOURCE, 'PUBLISH_BUILD_RUN_ID': str(RUN), 'GITHUB_REPOSITORY': publisher.REPO,
               'GITHUB_REF': 'refs/heads/main', 'GH_TOKEN': 'synthetic-not-a-credential',
               'GITHUB_EVENT_PATH': str(event), 'GITHUB_EVENT_NAME': 'workflow_run', 'GITHUB_SHA': PUBLICATION}
        with patch.dict(os.environ, env), patch('sys.argv', ['publisher', '--publish']), \
             patch.object(publisher, 'Client', return_value=client), patch.object(publisher, 'verify_bundle', side_effect=verify), \
             patch.object(publisher, 'verify_auditions', side_effect=verify), patch('sys.stdout', new_callable=io.StringIO):
            publisher.main()
    return verified


class PublisherTests(unittest.TestCase):
    def test_exact_successful_run_only(self):
        publisher.validate_run(run_record(), SOURCE, RUN)
        variants = [('id', RUN + 1), ('head_sha', OTHER), ('name', 'other'), ('path', '.github/workflows/other.yml'),
                    ('head_branch', 'other'), ('event', 'pull_request'), ('status', 'in_progress'),
                    ('conclusion', 'failure'), ('head_repository', {'full_name': 'someone/fork'})]
        for key, value in variants:
            with self.subTest(key=key):
                run = run_record(); run[key] = value
                with self.assertRaises(ValueError): publisher.validate_run(run, SOURCE, RUN)

    def test_only_first_build_and_job_attempts_are_accepted(self):
        for attempt in (None, 0, 2, '1', True):
            with self.subTest(attempt=attempt):
                run = run_record(); run['run_attempt'] = attempt
                with self.assertRaisesRegex(ValueError, 'attempt'):
                    publisher.validate_run(run, SOURCE, RUN)
                jobs = job_records(); jobs[0]['run_attempt'] = attempt
                with self.assertRaisesRegex(ValueError, 'attempt'):
                    publisher.validate_jobs(jobs, SOURCE, RUN)
        run = run_record(); del run['run_attempt']
        with self.assertRaisesRegex(ValueError, 'attempt'):
            publisher.validate_run(run, SOURCE, RUN)
        for index in range(6):
            jobs = job_records(); del jobs[index]['run_attempt']
            with self.subTest(missing_job_attempt=index), self.assertRaisesRegex(ValueError, 'attempt'):
                publisher.validate_jobs(jobs, SOURCE, RUN)

    def test_audition_attempt_is_bound_to_the_first_build_attempt(self):
        # Keep a valid checksummed metadata envelope; the attempt check must run
        # before this deliberately incomplete fixture reaches audio validation.
        inventory = json.dumps({'library_revision': publisher.REVISION, 'parameter_ids': list(range(190)),
                                'programs': [{'name': str(i)} for i in range(301)]}).encode()
        with tempfile.TemporaryDirectory() as work, patch.object(publisher, 'INVENTORY_SHA', publisher.sha(inventory)):
            path = Path(work) / 'auditions.zip'
            for attempt in (None, '2', 1, True, 'missing'):
                provenance = {'source_commit': SOURCE, 'run_id': str(RUN), 'repository': publisher.REPO,
                              'capture_status': 'complete', 'wav_count': 24, 'run_attempt': attempt}
                if attempt == 'missing': del provenance['run_attempt']
                files = {'inventory.json': inventory, 'source-commit.txt': SOURCE.encode(),
                         'provenance.json': json.dumps(provenance).encode()}
                files.update({f'fixture-{i}': b'unused' for i in range(29)})
                files['SHA256SUMS'] = ''.join(f'{publisher.sha(data)}  {name}\n' for name, data in files.items()).encode()
                with zipfile.ZipFile(path, 'w') as archive:
                    for name, data in files.items(): archive.writestr(name, data)
                with self.subTest(attempt=attempt), self.assertRaisesRegex(ValueError, 'attempt'):
                    publisher.verify_auditions(path, SOURCE, RUN)

    def test_required_jobs_and_steps_cannot_be_skipped_or_renamed(self):
        self.assertEqual(len(publisher.validate_jobs(job_records(), SOURCE, RUN)), 6)
        for index in range(6):
            for mutation in ('failed-job', 'wrong-source', 'wrong-run', 'missing-step', 'renamed-step', 'skipped-step'):
                with self.subTest(index=index, mutation=mutation):
                    jobs = job_records(); job = jobs[index]
                    if mutation == 'failed-job': job['conclusion'] = 'failure'
                    if mutation == 'wrong-source': job['head_sha'] = OTHER
                    if mutation == 'wrong-run': job['run_id'] = RUN + 1
                    if mutation == 'missing-step': job['steps'].pop()
                    if mutation == 'renamed-step': job['steps'][0]['name'] += ' changed'
                    if mutation == 'skipped-step': job['steps'][0]['conclusion'] = 'skipped'
                    with self.assertRaises(ValueError): publisher.validate_jobs(jobs, SOURCE, RUN)
        for jobs in (job_records()[:-1], job_records() + [job_records()[0]], job_records()[:-1] + [job_records()[0]]):
            with self.assertRaises(ValueError): publisher.validate_jobs(jobs, SOURCE, RUN)

    def test_publication_tree_only_allows_documentation_and_named_publisher_files(self):
        good = {'merge_base_commit': {'sha': SOURCE}, 'status': 'ahead', 'files': [
            {'filename': 'README.md', 'status': 'modified'},
            {'filename': 'docs/report.md', 'status': 'added'},
            {'filename': 'scripts/publish-preset-refinement.py', 'status': 'added'}]}
        self.assertEqual(len(publisher.validate_compare(good, SOURCE)), 3)
        for path in ('src/core/FactoryPresets.cpp', 'CMakeLists.txt', 'assets/font.ttf',
                     'scripts/package-ci-artifacts.py', '.github/workflows/build.yml', 'scripts/unrelated.py'):
            with self.subTest(path=path):
                value = deepcopy(good); value['files'] = [{'filename': path, 'status': 'modified'}]
                with self.assertRaises(ValueError): publisher.validate_compare(value, SOURCE)
        for mutation in ('wrong-ancestor', 'diverged', 'truncated', 'source-rename'):
            value = deepcopy(good)
            if mutation == 'wrong-ancestor': value['merge_base_commit']['sha'] = OTHER
            if mutation == 'diverged': value['status'] = 'diverged'
            if mutation == 'truncated': value['files'] = [{'filename': 'docs/x', 'status': 'added'}] * 300
            if mutation == 'source-rename': value['files'] = [{'filename': 'docs/x', 'previous_filename': 'src/x.cpp', 'status': 'renamed'}]
            with self.assertRaises(ValueError): publisher.validate_compare(value, SOURCE)

    def test_stale_main_rejected_and_existing_release_target_preserved(self):
        self.assertEqual(publisher.publication_target(None, PUBLICATION, PUBLICATION), PUBLICATION)
        with self.assertRaises(ValueError): publisher.publication_target(None, PUBLICATION, OTHER)
        with self.assertRaises(ValueError): publisher.publication_target(None, 'main', 'main')
        existing = {'prerelease': True, 'target_commitish': PUBLICATION, 'draft': False}
        self.assertEqual(publisher.publication_target(existing, OTHER, OTHER), PUBLICATION)
        for bad in ({'prerelease': False, 'target_commitish': PUBLICATION}, {'prerelease': True, 'target_commitish': 'main'}):
            with self.assertRaises(ValueError): publisher.publication_target(bad, OTHER, OTHER)

    def test_existing_tag_is_never_moved(self):
        publisher.validate_existing_tag([], PUBLICATION)
        matching = {'ref': 'refs/tags/' + publisher.TAG, 'object': {'type': 'commit', 'sha': PUBLICATION}}
        publisher.validate_existing_tag([matching], PUBLICATION)
        publisher.validate_existing_tag([{'ref': 'refs/tags/' + publisher.TAG + '-other', 'object': {'type': 'commit', 'sha': OTHER}}], PUBLICATION)
        for obj in ({'type': 'commit', 'sha': OTHER}, {'type': 'tag', 'sha': PUBLICATION}):
            with self.assertRaises(ValueError): publisher.validate_existing_tag([dict(matching, object=obj)], PUBLICATION)
        with self.assertRaises(ValueError): publisher.validate_existing_tag([matching, matching], PUBLICATION)

    def test_download_digest_size_and_resource_bounds(self):
        client = publisher.Client('synthetic-fixture-token')
        payload = b'original immutable artifact bytes'
        with tempfile.TemporaryDirectory() as work:
            target = Path(work) / 'artifact.zip'
            with patch.object(client, 'request', side_effect=lambda *a, **k: io.BytesIO(payload)):
                self.assertEqual(client.download('/fixture', target, publisher.sha(payload), len(payload)), len(payload))
                for digest, size in (('0' * 64, len(payload)), (publisher.sha(payload), len(payload) + 1)):
                    with self.assertRaises(ValueError): client.download('/fixture', target, digest, size)
                with patch.object(publisher, 'MAX_DOWNLOAD', 2):
                    with self.assertRaises(ValueError): client.download('/fixture', target, publisher.sha(payload))

    def test_matching_release_assets_are_idempotent_and_mismatches_refused(self):
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / 'same.zip'; path.write_bytes(b'unchanged bytes')
            good = {'name': path.name, 'state': 'uploaded', 'size': path.stat().st_size,
                    'digest': 'sha256:' + publisher.file_sha(path)}
            publisher.validate_release_asset(path.name, good, path)
            for key, value in (('name', 'other.zip'), ('state', 'starter'), ('size', 0), ('digest', 'sha256:' + '0' * 64)):
                with self.subTest(key=key), self.assertRaises(ValueError):
                    publisher.validate_release_asset(path.name, dict(good, **{key: value}), path)

    def test_cross_origin_redirect_strips_token_and_mutation_redirects_fail(self):
        handler = publisher.SafeRedirect()
        request = urllib.request.Request('https://api.github.com/repos/example/artifact', headers={'Authorization': 'Bearer synthetic'})
        target = handler.redirect_request(request, None, 302, 'redirect', {}, 'https://example.invalid/blob')
        self.assertIsNone(target.get_header('Authorization'))
        target = handler.redirect_request(request, None, 302, 'redirect', {}, 'https://api.github.com/other')
        self.assertEqual(target.get_header('Authorization'), 'Bearer synthetic')
        with self.assertRaises(ValueError): handler.redirect_request(request, None, 302, 'redirect', {}, 'http://example.invalid/blob')
        post = urllib.request.Request(request.full_url, data=b'x', method='POST')
        with self.assertRaises(ValueError): handler.redirect_request(post, None, 302, 'redirect', {}, 'https://example.invalid/blob')

    def test_zip_path_duplicate_and_special_file_rejection(self):
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / 'artifact.zip'
            for name in ('../escape', '/absolute', 'a\\b', 'C:evil', 'a//b', 'a/./b'):
                with zipfile.ZipFile(path, 'w') as archive: archive.writestr(name, b'x')
                with self.subTest(name=name), self.assertRaises(ValueError): publisher.zip_files(path)
            with warnings.catch_warnings(), zipfile.ZipFile(path, 'w') as archive:
                warnings.simplefilter('ignore', UserWarning)
                archive.writestr('same', b'a'); archive.writestr('same', b'b')
            with self.assertRaises(ValueError): publisher.zip_files(path)
            entry = zipfile.ZipInfo('link'); entry.external_attr = (stat.S_IFLNK | 0o777) << 16
            with zipfile.ZipFile(path, 'w') as archive: archive.writestr(entry, b'elsewhere')
            with self.assertRaises(ValueError): publisher.zip_files(path)

    def test_four_synthetic_platform_packages_and_metadata_refusals(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for system, arch in (('linux', 'x86_64'), ('windows', 'x86_64'), ('macos', 'arm64'), ('macos', 'x86_64')):
                for variant in ('valid', 'revision', 'run', 'juce', 'source-text'):
                    with self.subTest(system=system, arch=arch, variant=variant):
                        case = root / f'{system}-{arch}-{variant}'
                        notices.products(case / 'products', system, arch)
                        metadata = {'preset_library_revision': publisher.REVISION, 'workflow_run_id': str(RUN), 'juce_sha': publisher.JUCE_SHA, 'juce_version': '8.0.11'}
                        extras = {'SOURCE.txt': f'Source commit: {SOURCE}\nPreset library revision: {publisher.REVISION}\n'.encode()}
                        if variant == 'revision': metadata['preset_library_revision'] = '2026.11'
                        if variant == 'run': metadata['workflow_run_id'] = '999'
                        if variant == 'juce': metadata['juce_sha'] = OTHER
                        if variant == 'source-text': extras['SOURCE.txt'] = f'Source commit: {OTHER}\nPreset library revision: {publisher.REVISION}\n'.encode()
                        notices.packaging.package(case / 'products', case / 'packages', system, arch, SOURCE, metadata, extras)
                        outer = case / 'artifact.zip'
                        with zipfile.ZipFile(outer, 'w') as archive:
                            for item in (case / 'packages').iterdir(): archive.write(item, item.name)
                        if variant == 'valid':
                            result = publisher.verify_bundle(outer, system, arch, SOURCE, RUN, case / 'verified')
                            self.assertEqual((result['platform'], result['architecture']), (system, arch))
                        else:
                            with self.assertRaises(ValueError): publisher.verify_bundle(outer, system, arch, SOURCE, RUN, case / 'verified')

    def test_publication_mutation_order_and_resume_contract(self):
        checks = []
        for name, setup, fail_last in (
            ('failed job blocks every mutation', lambda c: setattr(c, 'fail_job', True), False),
            ('successful rerun blocks every mutation', lambda c: setattr(c, 'run_attempt', 2), False),
            ('mismatched job attempt blocks every mutation', lambda c: setattr(c, 'job_attempt', 2), False),
            ('mismatched event attempt blocks every mutation', lambda c: setattr(c, 'event_attempt', 2), False),
            ('duplicate artifact blocks every mutation', lambda c: setattr(c, 'duplicate_artifact', True), False),
            ('fifth payload failure blocks every mutation', lambda c: None, True),
        ):
            client = TransactionClient(); setup(client)
            try:
                invoke_transaction(client, fail_last)
                raise AssertionError('Expected refusal: ' + name)
            except ValueError:
                assert not client.writes, client.writes
                checks.append(name)

        client = TransactionClient()
        verified = invoke_transaction(client)
        assert len(verified) == 5
        first_write = next(i for i, value in enumerate(client.events) if value.startswith('write:'))
        assert client.events[:first_write].count('verify') == 5
        assert len(client.assets) == 7 and client.release['draft'] is False
        assert client.writes[-1] == ('PATCH', API + '/releases/77')
        receipt = json.loads(client.assets['release-provenance.json']['bytes'])
        assert receipt['build_run_attempt'] == 1
        assert all(job['run_attempt'] == 1 for job in receipt['required_jobs'])
        for item in client.artifacts:
            name = publisher.BUNDLES[item['name']][2] if item['name'] in publisher.BUNDLES else 'cs80-preset-auditions-' + publisher.REVISION + '.zip'
            assert client.assets[name]['bytes'] == item['name'].encode()
        checks.append('all five payloads verify before draft creation; seven assets precede publication')
        checks.append('five artifact payloads are uploaded unchanged')

        client.writes.clear(); invoke_transaction(client)
        assert not client.writes
        checks.append('matching published release is a no-op')

        missing = next(name for name in client.assets if name.endswith('.zip'))
        saved = client.assets.pop(missing)
        client.writes.clear()
        try:
            invoke_transaction(client)
            raise AssertionError('Expected published incomplete release refusal')
        except ValueError:
            assert not client.writes
        checks.append('incomplete published release is refused without mutation')

        client.release['draft'] = True
        client.writes.clear(); invoke_transaction(client)
        assert len(client.writes) == 2 and client.writes[0][0] == 'POST' and client.writes[1][0] == 'PATCH'
        assert client.assets[missing]['bytes'] == saved['bytes']
        checks.append('matching partial draft resumes one missing asset without replacement')

        client.assets[missing]['metadata']['digest'] = 'sha256:' + '0' * 64
        client.writes.clear()
        try:
            invoke_transaction(client)
            raise AssertionError('Expected mismatched asset refusal')
        except ValueError:
            assert not client.writes
        checks.append('existing mismatched asset is refused without mutation')

        self.assertEqual(len(checks), 12)


if __name__ == '__main__':
    unittest.main(verbosity=2)
