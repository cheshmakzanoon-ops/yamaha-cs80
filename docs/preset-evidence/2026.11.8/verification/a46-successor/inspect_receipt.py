#!/usr/bin/env python3
"""Recoverable Actions byte/ZIP inspection; no extraction or release acceptance.

Uses unchanged published helpers after verifying freshly recovered byte hashes.
Supplied artifact API authenticity and original upload-attempt binding are external.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import stat
import sys
from urllib.parse import urlsplit, urlunsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
MAX_ZIP = 1024 * 1024**2
SOURCE = '1b5b3c70471e369481b1f813880a6bcc762a2f57'
TREE = 'b235b67a801d33e9572076b8dfcfbb89e71fac29'
RUN_ID, ATTEMPT = 45457535944, 1
REPOSITORY, REPOSITORY_ID = 'cheshmakzanoon-ops/yamaha-cs80', 1436742091


def require(ok, message):
    if not ok:
        raise ValueError(message)


def record(raw):
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def helper(name):
    rows = json.loads((HERE / 'helper-recovery.json').read_bytes())
    matches = [r for r in rows if r['name'] == name]
    require(len(matches) == 1, 'Unknown/duplicate recovered helper')
    path = HERE / 'helpers' / name
    raw = path.read_bytes()
    require(all(record(raw)[k] == matches[0][k] for k in ('bytes', 'sha256')), 'Recovered helper bytes changed')
    require(hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest() == matches[0]['git_blob'],
            'Recovered helper Git object differs')
    spec = importlib.util.spec_from_file_location('recovered_' + name.replace('-', '_'), path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def contract(path):
    binder = helper('bind-parser-archives.py')
    raw = binder.snapshot(path, 2 * 1024**2)
    value = binder.parse(raw)
    require((value['source'], value['source_tree'], value['run_id'], value['run_attempt'])
            == (SOURCE, TREE, RUN_ID, ATTEMPT), 'Contract source/tree/run/attempt differs')
    require(value.get('release_qualified') is False, 'Unexpected contract qualification claim')
    return value, record(raw)


def api_context(path, name, artifact_id):
    binder = helper('bind-parser-archives.py')
    raw = binder.snapshot(path, 2 * 1024**2)
    api = binder.parse(raw)
    require(type(api.get('id')) is int and api['id'] == artifact_id and api.get('name') == name,
            'Artifact ID/name differs')
    route = f'https://api.github.com/repos/{REPOSITORY}/actions/artifacts/{artifact_id}'
    require(api.get('url') == route and api.get('archive_download_url') == route + '/zip', 'Artifact API route differs')
    run = api.get('workflow_run', {})
    require(type(run.get('id')) is int and run['id'] == RUN_ID and run.get('head_sha') == SOURCE,
            'Artifact source/run differs')
    require(type(run.get('repository_id')) is int and type(run.get('head_repository_id')) is int
            and run['repository_id'] == run['head_repository_id'] == REPOSITORY_ID, 'Repository identity differs')
    require('run_attempt' not in run or (type(run['run_attempt']) is int and run['run_attempt'] == ATTEMPT),
            'Declared artifact attempt differs')
    require(api.get('expired') is False, 'Artifact expired or expiry state missing')
    expiry = datetime.fromisoformat(api['expires_at'].replace('Z', '+00:00'))
    require(expiry.tzinfo is not None and expiry > datetime.now(timezone.utc), 'Artifact retention elapsed')
    require(type(api.get('size_in_bytes')) is int and 0 < api['size_in_bytes'] <= MAX_ZIP, 'Invalid API size')
    digest = api.get('digest')
    require(isinstance(digest, str) and len(digest) == 71 and digest.startswith('sha256:')
            and all(c in '0123456789abcdef' for c in digest[7:]), 'Missing valid API SHA256')
    return api, record(raw)


def base_receipt(api, api_record):
    return {'schema': 1, 'status': 'input_integrity_only', 'release_qualified': False,
            'source': SOURCE, 'source_tree': TREE, 'run_id': RUN_ID, 'run_attempt': ATTEMPT,
            'artifact_api': api_record, 'artifact': api, 'observed_at_utc': datetime.now(timezone.utc).isoformat(),
            'api_authenticity_verified': False, 'original_upload_attempt_verified': False,
            'native_validation_passed': None, 'checker': record(Path(__file__).read_bytes())}


def inspect_archive(path, api, api_record):
    payload = helper('a36-verify-validator-payload.py')
    raw = payload.snapshot(path, MAX_ZIP)
    actual = record(raw)
    require(actual['bytes'] == api['size_in_bytes'] and 'sha256:' + actual['sha256'] == api['digest'],
            'Complete outer ZIP bytes/SHA differ from API')
    payload.preflight_zip(raw)
    members, names, folded, total = [], {}, {}, 0
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        for info in archive.infolist():
            require(info.orig_filename == info.filename, 'NUL/altered ZIP name')
            name = payload.safe_path(info.filename)
            require(name not in names and name.casefold() not in folded, 'Duplicate/case-colliding ZIP name')
            names[name] = info.is_dir()
            folded[name.casefold()] = (name, info.is_dir())
            kind = stat.S_IFMT(info.external_attr >> 16)
            require(kind in (0, stat.S_IFDIR if info.is_dir() else stat.S_IFREG), 'Unsupported ZIP object')
            require(not info.flag_bits & 1 and info.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED),
                    'Encrypted/unsupported ZIP compression')
            require(0 <= info.file_size <= 512 * 1024**2 and (not info.is_dir() or info.file_size == 0),
                    'Oversize member or payload-bearing directory')
            total += info.file_size
            require(total <= MAX_ZIP, 'Expanded archive exceeds bound')
        for name in names:
            parts = name.split('/')
            for n in range(1, len(parts)):
                parent = '/'.join(parts[:n])
                if parent.casefold() in folded:
                    require(folded[parent.casefold()] == (parent, True), 'File/case-colliding path ancestor')
        for info in archive.infolist():
            count, digest = 0, hashlib.sha256()
            with archive.open(info) as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b''):
                    count += len(block)
                    require(count <= info.file_size, 'Member exceeds declared bytes')
                    digest.update(block)
            require(count == info.file_size, 'Truncated member')
            members.append({'path': info.filename, 'kind': 'directory' if info.is_dir() else 'file',
                            'bytes': count, 'sha256': digest.hexdigest()})
    result = base_receipt(api, api_record)
    result.update(archive=actual, members=members, expanded_bytes=total, all_member_crc_checked=True,
                  extracted=False, scope='Complete outer ZIP/API equality and member integrity, not inner '
                  'package/architecture, payload-to-validator equality, native acceptance or audition acceptance.')
    return raw, result


def safe_url(url):
    p = urlsplit(url)
    require(p.scheme == 'https' and p.netloc and p.username is None and p.password is None, 'Unsafe redirect URL')
    return urlunsplit((p.scheme, p.netloc, p.path, '', ''))


class Redirects(HTTPRedirectHandler):
    def __init__(self):
        self.rows = []

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        require(len(self.rows) < 8, 'Excessive redirects')
        self.rows.append({'status': code, 'url_without_query': safe_url(newurl)})
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def public_get(api, api_record):
    url = f"https://nightly.link/{REPOSITORY}/actions/artifacts/{api['id']}.zip"
    redirects = Redirects()
    request = Request(url, headers={'Accept-Encoding': 'identity'}, method='GET')
    with build_opener(redirects).open(request, timeout=30) as response:
        require(response.status == 200 and response.headers.get('Content-Range') is None, 'Not a full HTTP 200')
        require(response.headers.get('Content-Encoding', 'identity').lower() == 'identity', 'Unexpected encoding')
        count, digest = 0, hashlib.sha256()
        for block in iter(lambda: response.read(1024 * 1024), b''):
            count += len(block)
            require(count <= api['size_in_bytes'], 'GET exceeds API size')
            digest.update(block)
        require(count == api['size_in_bytes'] and 'sha256:' + digest.hexdigest() == api['digest'], 'GET bytes/SHA differ')
        length = response.headers.get('Content-Length')
        require(length is None or (length.isdigit() and int(length) == count), 'HTTP length differs')
        result = base_receipt(api, api_record)
        result['public_get'] = {'url': url, 'status': response.status, 'full_body_to_eof': True,
                                'bytes': count, 'sha256': digest.hexdigest(), 'authorization': False, 'range': False,
                                'final_url_without_query': safe_url(response.url), 'redirects': redirects.rows}
        result['scope'] = 'Complete public byte delivery only; all content and native qualification remain separate.'
        return result


def write_new(path, result):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('inspect', 'public-get'))
    parser.add_argument('--contract', type=Path, default=HERE / 'source-contract.json')
    parser.add_argument('--artifact-api', type=Path, required=True)
    parser.add_argument('--artifact-id', type=int, required=True)
    parser.add_argument('--name', required=True)
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    require(not args.out.exists() and not args.out.is_symlink(), 'Report already exists')
    _, contract_record = contract(args.contract)
    api, api_record = api_context(args.artifact_api, args.name, args.artifact_id)
    require((args.mode == 'inspect') == (args.archive is not None), 'Only inspect takes --archive')
    result = inspect_archive(args.archive, api, api_record)[1] if args.mode == 'inspect' else public_get(api, api_record)
    result['contract'] = contract_record
    write_new(args.out, result)
    print(json.dumps({'status': result['status'], 'release_qualified': False, 'out': str(args.out)}))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(type(error).__name__ + ': ' + str(error), file=sys.stderr)
        sys.exit(2)
