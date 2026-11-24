#!/usr/bin/env python3
"""Focused synthetic failure checks for the unchanged historical GET helper.

No network calls or public-artifact qualification occur in this exercise.
Only the separate actual full-GET receipt is real public-route evidence.
"""
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import urllib.error
from contextlib import redirect_stdout
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[6]
PROBE = REPO / 'docs/preset-evidence/2026.11.5/verification/a57-probe-public-artifact.py'
spec = importlib.util.spec_from_file_location('public_probe', PROBE)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)

BODY = b'PK\x03\x04synthetic-public-route-fixture'
ARTIFACT = json.loads((HERE / '14910025415-api.json').read_text())
RUN = json.loads((HERE / 'run-api.json').read_text())
ARTIFACT['size_in_bytes'] = len(BODY)
ARTIFACT['digest'] = 'sha256:' + hashlib.sha256(BODY).hexdigest()
ARTIFACT['expires_at'] = '2099-01-01T00:00:00Z'
SOURCE = RUN['head_sha']


class Response(io.BytesIO):
    def __init__(self, body, status, headers):
        super().__init__(body)
        self.status = status
        self.headers = headers
        self.url = 'https://example.invalid/public.zip?secret=redacted-fixture'


class Opener:
    def __init__(self, body, status, headers, error):
        self.body, self.status, self.headers, self.error = body, status, headers, error
        self.called = False

    def open(self, request, timeout):
        self.called = True
        assert request.get_method() == 'GET'
        assert request.get_header('Authorization') is None
        assert request.get_header('Cookie') is None
        assert request.get_header('Range') is None
        assert request.full_url.startswith('https://nightly.link/')
        if self.error:
            raise self.error
        return Response(self.body, self.status, self.headers)


def run_case(label, accept=False, artifact_edit=None, run_edit=None,
             body=BODY, status=200, headers=None, error=None):
    artifact, run = copy.deepcopy(ARTIFACT), copy.deepcopy(RUN)
    if artifact_edit:
        artifact_edit(artifact)
    if run_edit:
        run_edit(run)
    opener = Opener(body, status, headers or {}, error)
    with tempfile.TemporaryDirectory(prefix='a55-probe-test-') as temp:
        temp = Path(temp)
        ap, rp, out = temp / 'artifact.json', temp / 'run.json', temp / 'receipt.json'
        ap.write_text(json.dumps(artifact))
        rp.write_text(json.dumps(run))
        argv = [str(PROBE), '--artifact-api', str(ap), '--run-api', str(rp),
                '--source', SOURCE, '--run-id', str(RUN['id']), '--attempt', '1',
                '--artifact-name', ARTIFACT['name'], '--out', str(out)]
        with patch.object(sys, 'argv', argv), \
             patch.object(probe.urllib.request, 'build_opener', return_value=opener), \
             redirect_stdout(io.StringIO()):
            try:
                code = probe.main()
                receipt = json.loads(out.read_text())
                observed_accept = code == 0 and receipt['public_route_verified'] is True
                assert receipt['release_qualified'] is False
                assert receipt['credentials_supplied'] is False
                assert 'redacted-fixture' not in out.read_text()
                reason = receipt.get('error', {}).get('reason', receipt.get('error', {}).get('type'))
            except ValueError as exc:
                observed_accept, reason = False, str(exc)
        assert observed_accept is accept, f'{label}: unexpected acceptance={observed_accept}'
        return {'case': label, 'expected_acceptance': accept,
                'observed_acceptance': observed_accept, 'passed': True,
                'mock_get_attempted': opener.called, 'rejection_reason': reason}


def main():
    results = [
        run_case('complete matching synthetic body', accept=True,
                 headers={'Content-Length': str(len(BODY))}),
        run_case('HTTP 403', error=urllib.error.HTTPError(
            'https://example.invalid/', 403, 'Forbidden', {}, None)),
        run_case('HTTP 200 HTML login wall', body=b'<html>Please sign in</html>'),
        run_case('truncated body', body=BODY[:-1]),
        run_case('oversized body', body=BODY + b'x'),
        run_case('same-size wrong bytes', body=b'x' * len(BODY)),
        run_case('HTTP 206 partial response', status=206),
        run_case('HTTP 200 with Content-Range', headers={'Content-Range': 'bytes 0-2/99'}),
        run_case('Content-Length mismatch', headers={'Content-Length': str(len(BODY) + 1)}),
        run_case('artifact explicitly expired', artifact_edit=lambda a: a.update(expired=True)),
        run_case('artifact expiry elapsed', artifact_edit=lambda a: a.update(expires_at='2000-01-01T00:00:00Z')),
        run_case('artifact wrong name', artifact_edit=lambda a: a.update(name='old-install-bundle')),
        run_case('artifact wrong source', artifact_edit=lambda a: a['workflow_run'].update(head_sha='0' * 40)),
        run_case('artifact wrong run', artifact_edit=lambda a: a['workflow_run'].update(id=1)),
        run_case('run wrong attempt', run_edit=lambda r: r.update(run_attempt=2)),
        run_case('run wrong repository', run_edit=lambda r: r['repository'].update(full_name='wrong/repository')),
        run_case('artifact digest absent', artifact_edit=lambda a: a.update(digest=None)),
    ]
    redirect_results = []
    request = probe.urllib.request.Request('https://nightly.link/route.zip')
    for url, label in [('http://example.invalid/a.zip', 'non-HTTPS redirect'),
                       ('https://user:password@example.invalid/a.zip', 'credential-bearing redirect')]:
        try:
            probe.HttpsRedirects().redirect_request(request, None, 302, 'Found', {}, url)
        except ValueError as exc:
            redirect_results.append({'case': label, 'passed': True, 'rejection_reason': str(exc)})
        else:
            raise AssertionError(label + ' was accepted')
    result = {'scope': 'Synthetic mock response and metadata rejection checks only; no network or release qualification.',
              'public_route_verified': False, 'release_qualified': False,
              'probe_path': str(PROBE.relative_to(REPO)),
              'probe_sha256': hashlib.sha256(PROBE.read_bytes()).hexdigest(),
              'case_count': len(results) + len(redirect_results),
              'all_cases_passed': True, 'cases': results + redirect_results}
    (HERE / 'probe-self-test.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'synthetic_cases_passed': result['case_count'], 'release_qualified': False}))


if __name__ == '__main__':
    main()
