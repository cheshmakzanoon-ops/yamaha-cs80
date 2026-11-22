#!/usr/bin/env python3
"""Independent offline transaction review; all API and payload-verifier calls are mocked.

Actual package validation is separately covered by the publisher fixture suite.
This script verifies mutation ordering and resumption, never connects to GitHub.
"""
from copy import deepcopy
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('review_publisher', ROOT / 'build-w07-durable-release/publish-preset-refinement.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
SOURCE, PUB = 'a' * 40, 'b' * 40
RUN = 123
API = '/repos/' + p.REPO


class FakeClient:
    def __init__(self):
        self.release = None
        self.assets = {}
        self.writes = []
        self.events = []
        self.fail_job = False
        self.duplicate_artifact = False
        names = sorted(set(p.BUNDLES) | {'cs80-curated-preset-audition-' + SOURCE})
        self.artifacts = [dict(id=i, name=name, expired=False, workflow_run={'id': RUN, 'head_sha': SOURCE},
                               digest='sha256:' + p.sha(name.encode()), size_in_bytes=len(name.encode()))
                          for i, name in enumerate(names, 1)]

    def json(self, path, method='GET', body=None):
        if method != 'GET':
            self.writes.append((method, path))
            self.events.append('write:' + method)
        if path == API + '/actions/runs/' + str(RUN):
            return dict(id=RUN, head_sha=SOURCE, name='build', path='.github/workflows/build.yml',
                        head_branch='main', event='push', head_repository={'full_name': p.REPO},
                        status='completed', conclusion='success', html_url='https://example.invalid/build')
        if path == API + '/git/ref/heads/main':
            return {'object': {'sha': PUB}}
        if path.startswith(API + '/compare/'):
            return dict(merge_base_commit={'sha': SOURCE}, status='ahead', files=[dict(filename='docs/report.md', status='added')])
        if path.startswith(API + '/git/matching-refs/'):
            return [] if self.release is None else [{'ref': 'refs/tags/' + p.TAG, 'object': {'type': 'commit', 'sha': PUB}}]
        if path == API + '/git/ref/tags/' + p.TAG:
            return {'object': {'type': 'commit', 'sha': PUB}}
        if path == API + '/releases' and method == 'POST':
            self.release = dict(body, id=77, upload_url='https://uploads.github.com/repos/' + p.REPO + '/releases/77/assets{?name,label}', html_url='https://example.invalid/release')
            return deepcopy(self.release)
        if path == API + '/releases/77' and method == 'PATCH':
            self.release.update(body)
            return deepcopy(self.release)
        raise AssertionError((method, path))

    def pages(self, path, key=None):
        if '/jobs?' in path:
            rows = [dict(id=i, run_id=RUN, head_sha=SOURCE, name=name, status='completed', conclusion='success',
                         steps=[dict(name=s, conclusion='success') for s in sorted(steps)])
                    for i, (name, steps) in enumerate(p.JOB_STEPS.items(), 1)]
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
        assert p.sha(payload) == digest and len(payload) == size
        target.write_bytes(payload)
        self.events.append('download')

    def request(self, url, method, data, binary=False):
        assert method == 'POST' and binary
        name = p.urllib.parse.parse_qs(p.urllib.parse.urlsplit(url).query)['name'][0]
        assert name not in self.assets, 'An existing asset would be overwritten'
        self.writes.append((method, url))
        self.events.append('write:upload')
        metadata = dict(name=name, state='uploaded', size=len(data), digest='sha256:' + p.sha(data),
                        browser_download_url='https://example.invalid/' + name)
        self.assets[name] = {'metadata': metadata, 'bytes': data}
        return io.BytesIO(json.dumps(metadata).encode())


def invoke(client, fail_last_verifier=False):
    verified = []
    def verify(path, *args):
        verified.append(path.name)
        client.events.append('verify')
        if fail_last_verifier and len(verified) == 5:
            raise ValueError('Injected fifth payload failure')
        return {'synthetic_review_payload': True}
    with tempfile.TemporaryDirectory() as work:
        event = Path(work) / 'event.json'
        event.write_text(json.dumps({'workflow_run': {'id': RUN, 'head_sha': SOURCE}}))
        env = {'PUBLISH_BINARY_SHA': SOURCE, 'PUBLISH_BUILD_RUN_ID': str(RUN), 'GITHUB_REPOSITORY': p.REPO,
               'GITHUB_REF': 'refs/heads/main', 'GH_TOKEN': 'synthetic-not-a-credential',
               'GITHUB_EVENT_PATH': str(event), 'GITHUB_EVENT_NAME': 'workflow_run', 'GITHUB_SHA': PUB}
        with patch.dict(os.environ, env), patch('sys.argv', ['publisher', '--publish']), \
             patch.object(p, 'Client', return_value=client), patch.object(p, 'verify_bundle', side_effect=verify), \
             patch.object(p, 'verify_auditions', side_effect=verify), patch('sys.stdout', new_callable=io.StringIO):
            p.main()
    return verified


checks = []
for name, setup, fail_last in (
    ('failed job blocks every mutation', lambda c: setattr(c, 'fail_job', True), False),
    ('duplicate artifact blocks every mutation', lambda c: setattr(c, 'duplicate_artifact', True), False),
    ('fifth payload failure blocks every mutation', lambda c: None, True),
):
    client = FakeClient(); setup(client)
    try:
        invoke(client, fail_last)
        raise AssertionError('Expected refusal: ' + name)
    except ValueError:
        assert not client.writes, client.writes
        checks.append(name)

client = FakeClient()
verified = invoke(client)
assert len(verified) == 5
first_write = next(i for i, value in enumerate(client.events) if value.startswith('write:'))
assert client.events[:first_write].count('verify') == 5
assert len(client.assets) == 7 and client.release['draft'] is False
assert client.writes[-1] == ('PATCH', API + '/releases/77')
for item in client.artifacts:
    name = p.BUNDLES[item['name']][2] if item['name'] in p.BUNDLES else 'cs80-preset-auditions-' + p.REVISION + '.zip'
    assert client.assets[name]['bytes'] == item['name'].encode()
checks.append('all five payloads verify before draft creation; seven assets precede publication')
checks.append('five artifact payloads are uploaded unchanged')

client.writes.clear(); invoke(client)
assert not client.writes
checks.append('matching published release is a no-op')

missing = next(name for name in client.assets if name.endswith('.zip'))
saved = client.assets.pop(missing)
client.writes.clear()
try:
    invoke(client)
    raise AssertionError('Expected published incomplete release refusal')
except ValueError:
    assert not client.writes
checks.append('incomplete published release is refused without mutation')

client.release['draft'] = True
client.writes.clear(); invoke(client)
assert len(client.writes) == 2 and client.writes[0][0] == 'POST' and client.writes[1][0] == 'PATCH'
assert client.assets[missing]['bytes'] == saved['bytes']
checks.append('matching partial draft resumes one missing asset without replacement')

client.assets[missing]['metadata']['digest'] = 'sha256:' + '0' * 64
client.writes.clear()
try:
    invoke(client)
    raise AssertionError('Expected mismatched asset refusal')
except ValueError:
    assert not client.writes
checks.append('existing mismatched asset is refused without mutation')

print(json.dumps({'status': 'PASS', 'checks': checks, 'count': len(checks),
                  'qualification': 'Entire API and payload validators mocked; actual package-verifier tests separately executed. No network or release writes.'}, indent=2))
