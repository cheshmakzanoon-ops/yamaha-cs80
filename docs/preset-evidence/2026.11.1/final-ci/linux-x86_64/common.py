"""Read-only metadata/download helpers for assignment 54's fixed Linux build."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import re
import shutil
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(__file__).resolve().parent
CACHE = ROOT / 'build-coordination/artifact-cache'
SOURCE = '6d3cc220edef5a4a2c7cd968ea8a786c7c83198c'
RUN_ID = 45044040694
REPO = 'cheshmakzanoon-ops/yamaha-cs80'
ATTEMPT = 1


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha_file(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text())


def validate_run(run):
    require(run.get('id') == RUN_ID and run.get('head_sha') == SOURCE, 'Wrong build identity')
    require(type(run.get('run_attempt')) is int and run['run_attempt'] == ATTEMPT, 'Wrong build attempt')
    require(run.get('head_branch') == 'main' and run.get('event') == 'push', 'Not the fixed main push')
    require(run.get('name') == 'build' and run.get('path') == '.github/workflows/build.yml', 'Wrong workflow')
    require(run.get('head_repository', {}).get('full_name') == REPO, 'Wrong repository')


def validate_job(job, name):
    require(job.get('run_id') == RUN_ID and job.get('head_sha') == SOURCE, 'Wrong job source/run')
    require(type(job.get('run_attempt')) is int and job['run_attempt'] == ATTEMPT, 'Wrong job attempt')
    require(job.get('name') == name, 'Wrong job name')
    require(job.get('status') == 'completed' and job.get('conclusion') == 'success', 'Required job is not successful')


def require_steps(job, names):
    steps = {step['name']: step for step in job['steps']}
    require(len(steps) == len(job['steps']), 'Duplicate step names')
    for name in names:
        require(steps.get(name, {}).get('conclusion') == 'success', 'Required step did not pass: ' + name)
    return [steps[name] for name in sorted(names)]


def download_artifact(meta, expected_name, reserve_inner=False):
    require(meta.get('name') == expected_name and meta.get('expired') is False, 'Wrong or expired artifact')
    identity = meta.get('workflow_run', {})
    require(identity.get('id') == RUN_ID and identity.get('head_sha') == SOURCE
            and identity.get('head_branch') == 'main', 'Wrong artifact run/source/branch')
    require(type(meta.get('id')) is int and meta['id'] > 0, 'Invalid artifact id')
    size = meta.get('size_in_bytes')
    require(type(size) is int and 0 < size <= 512 * 1024 * 1024, 'Invalid artifact size')
    require(re.fullmatch(r'sha256:[0-9a-f]{64}', meta.get('digest', '')) is not None, 'Missing API digest')
    CACHE.mkdir(exist_ok=True, parents=True)
    outer = CACHE / (expected_name + '-' + str(meta['id']) + '.zip')
    url = 'https://nightly.link/' + REPO + '/actions/artifacts/' + str(meta['id']) + '.zip'
    needed = (0 if outer.exists() else size) + (size if reserve_inner else 0) + 32 * 1024 * 1024
    require(shutil.disk_usage(WORK).free > needed, 'Insufficient free space for bounded verification')
    if not outer.exists():
        part = outer.with_suffix('.zip.part')
        try:
            request = urllib.request.Request(url, headers={'User-Agent': 'CS80-Linux-package-verification/1.0'})
            with urllib.request.urlopen(request, timeout=45) as response, part.open('wb') as target:
                require(response.status == 200, 'Public download failed')
                require(urllib.parse.urlsplit(response.url).scheme == 'https', 'Insecure final download URL')
                received = 0
                while chunk := response.read(1024 * 1024):
                    received += len(chunk)
                    require(received <= size, 'Download exceeds recorded artifact size')
                    target.write(chunk)
            require(part.stat().st_size == size and sha_file(part) == meta['digest'][7:], 'Downloaded artifact differs from API bytes')
            part.replace(outer)
        finally:
            part.unlink(missing_ok=True)
    digest = sha_file(outer)
    require(outer.stat().st_size == size and digest == meta['digest'][7:], 'Cached artifact differs from API identity')
    return outer, {'url': url, 'credential_free_get': True, 'path': str(outer), 'bytes': size, 'sha256': digest}


def input_identity(path):
    path = Path(path)
    return {'bytes': path.stat().st_size, 'sha256': sha_file(path)}
