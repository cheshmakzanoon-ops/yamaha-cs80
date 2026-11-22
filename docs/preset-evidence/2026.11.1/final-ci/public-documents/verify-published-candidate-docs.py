#!/usr/bin/env python3
"""Check pushed docs and bounded live candidate GETs; never qualify a release."""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit
from urllib.request import Request, HTTPRedirectHandler, build_opener

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(__file__).resolve().parent
REPO = 'cheshmakzanoon-ops/yamaha-cs80'
RAW = 'https://raw.githubusercontent.com/' + REPO + '/'
SOURCE = '6d3cc220edef5a4a2c7cd968ea8a786c7c83198c'
OUT = WORK / 'published-candidate-verification.json'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


class PublicHTTPSRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, url):
        parsed = urlsplit(url)
        require(req.get_method() == 'GET' and parsed.scheme == 'https'
                and parsed.username is None and parsed.password is None,
                'Unexpected public GET redirect')
        return super().redirect_request(req, fp, code, msg, headers, url)


def request(url, range_header=False):
    require(any(url.startswith(prefix) for prefix in (
        RAW, 'https://nightly.link/' + REPO + '/actions/artifacts/',
        'https://api.github.com/repos/' + REPO + '/releases/tags/')),
        'Unexpected public URL')
    headers = {'User-Agent': 'CS80-published-candidate-document-verification/1.0'}
    if range_header:
        headers['Range'] = 'bytes=0-3'
    return build_opener(PublicHTTPSRedirect()).open(Request(url, headers=headers, method='GET'), timeout=45)


def document_check(item):
    ref, path, expected = item
    url = RAW + ref + '/' + path
    started = datetime.now(timezone.utc).isoformat()
    with request(url) as response:
        require(response.status == 200, 'Public source document was not HTTP 200')
        data = response.read(4 * 1024 * 1024 + 1)
    require(len(data) <= 4 * 1024 * 1024, 'Public source document exceeded cap')
    require(sha(data) == expected, 'Published bytes differ: ' + ref + '/' + path)
    return {'ref': ref, 'path': path, 'url': url, 'http_status': 200,
            'observed_at_utc': started, 'bytes': len(data), 'sha256': sha(data),
            'matches_inspected_local_or_committed_bytes': True}


def candidate_check(artifact):
    started = datetime.now(timezone.utc).isoformat()
    with request(artifact['url'], range_header=True) as response:
        status = response.status
        require(status in (200, 206), 'Candidate availability GET did not return 200/206')
        header = response.read(4)
        require(header == b'PK\x03\x04', 'Candidate response does not begin with ZIP signature')
        content_range = response.headers.get('Content-Range')
        if status == 206:
            require(content_range == f'bytes 0-3/{artifact["bytes"]}', 'Unexpected candidate Content-Range')
        final_host = urlsplit(response.geturl()).hostname
    return {'artifact_id': artifact['artifact_id'], 'artifact_name': artifact['artifact_name'],
            'url': artifact['url'], 'credential_free_get': True, 'http_status': status,
            'observed_at_utc': started, 'range_requested': 'bytes=0-3', 'bytes_read': len(header),
            'zip_signature': header.hex(), 'content_range': content_range, 'final_host': final_host,
            'availability_only': True, 'full_byte_hash_rechecked_in_this_probe': False,
            'prior_full_get_sha256': artifact['sha256'], 'prior_full_get_bytes': artifact['bytes'],
            'release_qualified': False}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--commit', required=True, help='Exact parent-pushed documentation commit')
    args = parser.parse_args()
    require(bool(re.fullmatch(r'[0-9a-f]{40}', args.commit)), 'Expected full documentation commit SHA')
    plan = json.loads((WORK / 'candidate-documents.json').read_text())
    proof_path = ROOT / 'docs/preset-evidence/2026.11.1/final-ci/public-candidates/summary.json'
    proof = json.loads(proof_path.read_text())
    require(sha(proof_path.read_bytes()) == '87ad68a563e9e81bd6cdf3cd307dc6f0fe27ad75ddeca653f4a04cb4d6b1f7ec',
            'Changed authoritative full-GET proof')
    require(proof['binary_source_commit'] == SOURCE and proof['build_conclusion'] == 'failure'
            and proof['release_qualified'] is False and proof['build_run_attempt'] == 1,
            'Candidate qualification context changed')
    tasks = []
    for path, info in plan['public_documents'].items():
        expected = info['after_sha256']
        require(sha((ROOT / path).read_bytes()) == expected, 'Public document changed after local review: ' + path)
        committed = subprocess.check_output(['git', 'show', args.commit + ':' + path], cwd=ROOT)
        require(sha(committed) == expected, 'Parent commit differs from reviewed document: ' + path)
        tasks.extend([(args.commit, path, expected), ('main', path, expected)])
    evidence_paths = [
        'docs/preset-evidence/2026.11.1/final-ci/public-candidates/summary.json',
        'docs/preset-evidence/2026.11.1/final-ci/linux-x86_64/ci-proof.json',
        'docs/preset-evidence/2026.11.1/final-ci/auditions/artifact-proof.json',
        'docs/preset-evidence/2026.11.1/final-ci/installation/artifact-proof.json',
        'docs/preset-evidence/2026.11.1/final-ci/failure-review/README.md',
        'docs/preset-evidence/2026.11.1/final-ci/README.md',
        'docs/TEAM_REVIEW_2026_11_1.md',
        'docs/preset-evidence/2026.11.1/final-ci/team/ledger.json',
    ]
    for path in evidence_paths:
        committed = subprocess.check_output(['git', 'show', args.commit + ':' + path], cwd=ROOT)
        require(committed == (ROOT / path).read_bytes(), 'Evidence differs from parent-pushed commit')
        tasks.append((args.commit, path, sha(committed)))
    for path in ('README.md', 'docs/PRESET_REFINEMENT_2026_11_1.md', 'docs/INSTALL_AND_QUICKSTART.md'):
        data = subprocess.check_output(['git', 'show', SOURCE + ':' + path], cwd=ROOT)
        tasks.append((SOURCE, path, sha(data)))
    with ThreadPoolExecutor(max_workers=4) as pool:
        documents = list(pool.map(document_check, tasks))
    with ThreadPoolExecutor(max_workers=3) as pool:
        candidates = list(pool.map(candidate_check, proof['public_candidate_artifacts']))
    release_url = 'https://api.github.com/repos/' + REPO + '/releases/tags/presets-2026.11.1'
    release = {'url': release_url, 'observed_at_utc': datetime.now(timezone.utc).isoformat()}
    try:
        with request(release_url) as response:
            release['http_status'] = response.status
    except HTTPError as error:
        release['http_status'] = error.code
    require(release['http_status'] == 404, 'Public release visibility changed; review separately')
    result = {
        'assignment': 59,
        'status': 'PUBLISHED_CANDIDATE_DOCS_AND_LINKS_VERIFIED_RELEASE_UNQUALIFIED',
        'verified_at_utc': datetime.now(timezone.utc).isoformat(),
        'documentation_commit': args.commit, 'binary_source_commit': SOURCE,
        'build_run_id': proof['build_run_id'], 'build_run_attempt': 1,
        'build_conclusion': 'failure', 'release_qualified': False,
        'full_candidate_byte_proof_path': str(proof_path.relative_to(ROOT)),
        'full_candidate_byte_proof_sha256': sha(proof_path.read_bytes()),
        'document_gets': documents, 'candidate_availability_gets': candidates,
        'public_release_visibility': release,
        'scope': 'Exact pushed/main documentation, linked evidence and original source-document byte identities plus five bounded credential-free live ZIP availability GETs. Full candidate byte hashes remain the separate assignment-56 proof. Failed Linux validation, skipped publisher and lack of a public qualified release remain unchanged; no listening or hardware-equivalence verdict.',
    }
    OUT.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'assignment': 59, 'status': result['status'],
                      'documentation_commit': args.commit, 'document_gets': len(documents),
                      'candidate_availability_gets': len(candidates), 'release_qualified': False}, indent=2))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        detail = ('HTTP ' + str(error.code)) if isinstance(error, HTTPError) else (
            'Network error: ' + type(error.reason).__name__) if isinstance(error, URLError) else str(error)
        failure = {'assignment': 59, 'status': 'PUBLISHED_CANDIDATE_CHECK_FAILED_RELEASE_UNQUALIFIED',
                   'release_qualified': False, 'observed_at_utc': datetime.now(timezone.utc).isoformat(),
                   'error_type': type(error).__name__, 'error': detail}
        (WORK / 'published-candidate-verification-failure.json').write_text(json.dumps(failure, indent=2) + '\n')
        print(json.dumps(failure, indent=2))
        raise SystemExit(1)
