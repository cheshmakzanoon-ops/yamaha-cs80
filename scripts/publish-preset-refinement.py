#!/usr/bin/env python3
"""One-off, fail-closed publisher for already-built preset revision 2026.11.1.

The workflow fixes the binary SHA and build run. The ordinary release tag points
to the current main publication commit; package bytes keep their binary SHA.
Nothing from a downloaded artifact is executed. No tag/asset is replaced.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import re
import stat
import struct
import tarfile
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import zipfile

REPO = 'cheshmakzanoon-ops/yamaha-cs80'
REVISION = '2026.11.1'
TAG = 'presets-' + REVISION
BUILD_RUN_ATTEMPT = 1
INVENTORY_SHA = 'fe3ddc6ecdb9fa25e77170f1cd9f81e6f2fe0ef5aabd105506a0fa487eb286fa'
JUCE_SHA = 'c352e2489000ffeb9e397051377d75eef2efe641'
ROOT = Path(__file__).resolve().parents[1]
MAX_DOWNLOAD = 512 * 1024 * 1024
MAX_EXPANDED = 768 * 1024 * 1024
PUBLISHER_FILES = {
    '.github/workflows/publish-preset-refinement.yml',
    'scripts/publish-preset-refinement.py',
    'scripts/test-publish-preset-refinement.py',
}
BUNDLES = {
    'cs80-windows-install-bundle': ('windows', 'x86_64', 'cs80-windows-x64.zip'),
    'cs80-macos-15-install-bundle': ('macos', 'arm64', 'cs80-macos-apple-silicon.zip'),
    'cs80-macos-15-intel-install-bundle': ('macos', 'x86_64', 'cs80-macos-intel.zip'),
    'cs80-linux-install-bundle': ('linux', 'x86_64', 'cs80-linux-x64.zip'),
}
BASE_STEPS = {'Verify configured JUCE source', 'Build', 'Test'}
JOB_STEPS = {
    'Linux / VST3 + Standalone': BASE_STEPS | {'Capture curated preset auditions', 'Upload curated preset auditions', 'Package Linux VST3 and Standalone', 'Upload Linux install bundle'},
    'Windows / VST3 + Standalone': BASE_STEPS | {'Package Windows VST3 and Standalone', 'Upload Windows install bundle'},
    'macOS / macos-15 / VST3 + AU + Standalone': BASE_STEPS | {'Validate VST3 with pluginval', 'Validate AU with pluginval', 'Validate AU with pluginval GUI', 'Package native macOS VST3, AU and Standalone', 'Upload native macOS install bundle'},
    'macOS / macos-15-intel / VST3 + AU + Standalone': BASE_STEPS | {'Validate VST3 with pluginval', 'Validate AU with pluginval', 'Validate AU with pluginval GUI', 'Package native macOS VST3, AU and Standalone', 'Upload native macOS install bundle'},
    'pluginval / Linux VST3 (strictness 10)': {'Validate VST3 headless', 'Validate VST3 GUI under Xvfb', 'Validate GUI state regression seed 0x2da72db'},
    'pluginval / Windows VST3 (strictness 10)': {'Validate VST3 headless', 'Validate state regression seed 0x5037a0c'},
}
AUDITIONS = {
    'standard': (0.0, 3, 1, {'Brass Swell I', 'Solo Violin', 'Clarinet', 'Electric Piano I', 'Flute Organ 8+4', 'Analog Bass', 'Nylon Pluck', 'Marimba', 'Arp Poly', 'Wind'}),
    'pressure': (0.65, 3, 1, {'Pressure Lead'}),
    'refined': (0.0, 3, 1, {'Flute', 'Breathy Flute', 'Harp', 'Kalimba', 'Sine Bass', 'Upright Pluck'}),
    'slow': (0.0, 5, 2, {'Slow Pad', 'Noir Swell Brass', 'Brass Ensemble 16', 'Neo-Noir Pad', 'Arctic Pad', 'Synthwave Chords', 'Ether Pad'}),
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def file_sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def json_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate JSON field: ' + key)
        result[key] = value
    return result


def parse_json(data):
    return json.loads(data, object_pairs_hook=json_object)


class SafeRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        require(req.get_method() == 'GET', 'Unexpected redirect during mutation')
        require(urllib.parse.urlsplit(newurl).scheme == 'https', 'Insecure redirect')
        redirected = super().redirect_request(req, fp, code, msg, headers, newurl)
        if urllib.parse.urlsplit(req.full_url).netloc != urllib.parse.urlsplit(newurl).netloc:
            redirected.remove_header('Authorization')
        return redirected


class Client:
    def __init__(self, token):
        require(bool(token), 'GH_TOKEN is required from the scoped workflow')
        self.token = token
        self.opener = urllib.request.build_opener(SafeRedirect())

    def request(self, path, method='GET', body=None, binary=False, accept=None):
        if path.startswith('https://'):
            require(path.startswith('https://uploads.github.com/repos/' + REPO + '/releases/'), 'Unexpected upload origin')
            url = path
        else:
            require(path.startswith('/repos/' + REPO + '/'), 'Unexpected API repository')
            url = 'https://api.github.com' + path
        headers = {'Authorization': 'Bearer ' + self.token,
                   'Accept': accept or 'application/vnd.github+json',
                   'X-GitHub-Api-Version': '2022-11-28',
                   'User-Agent': 'cs80-preset-refinement-publisher'}
        data = body if binary else json.dumps(body).encode() if body is not None else None
        if data is not None:
            headers['Content-Type'] = 'application/octet-stream' if binary else 'application/json'
        try:
            return self.opener.open(urllib.request.Request(url, data=data, headers=headers, method=method), timeout=120)
        except urllib.error.HTTPError as error:
            # Do not print headers, credentials, or signed redirect URLs.
            raise RuntimeError(f'GitHub HTTP {error.code}: {method} {urllib.parse.urlsplit(url).path}') from None

    def json(self, path, method='GET', body=None):
        with self.request(path, method, body) as response:
            data = response.read(8 * 1024 * 1024 + 1)
        require(len(data) <= 8 * 1024 * 1024, 'Oversized GitHub metadata')
        return parse_json(data)

    def pages(self, path, key=None):
        rows = []
        for page in range(1, 101):
            value = self.json(path + ('&' if '?' in path else '?') + f'per_page=100&page={page}')
            items = value[key] if key else value
            require(isinstance(items, list), 'Invalid API collection')
            rows.extend(items)
            if len(items) < 100:
                return rows
        raise ValueError('API pagination exceeded the bounded collection size')

    def download(self, path, destination, expected_sha, expected_size=None):
        require(re.fullmatch('[0-9a-f]{64}', expected_sha) is not None, 'Missing download SHA-256')
        digest, size = hashlib.sha256(), 0
        with self.request(path, accept='application/octet-stream') as response, destination.open('wb') as out:
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                size += len(chunk)
                require(size <= MAX_DOWNLOAD, 'Download exceeds size bound')
                digest.update(chunk)
                out.write(chunk)
        require(digest.hexdigest() == expected_sha, 'Downloaded bytes differ from GitHub digest')
        if expected_size is not None:
            require(size == expected_size, 'Downloaded size differs from GitHub metadata')
        return size


def validate_run(run, source, run_id):
    require(run.get('id') == run_id and run.get('head_sha') == source, 'Wrong build identity')
    require(run.get('name') == 'build' and run.get('path') == '.github/workflows/build.yml', 'Wrong workflow')
    require(run.get('head_branch') == 'main' and run.get('event') == 'push', 'Build is not the authorized main push')
    require(run.get('head_repository', {}).get('full_name') == REPO, 'Fork or different repository')
    # This one-off publication cannot replace an earlier failure with a rerun.
    require(type(run.get('run_attempt')) is int and run['run_attempt'] == BUILD_RUN_ATTEMPT, 'Build must be its first attempt')
    require(run.get('status') == 'completed' and run.get('conclusion') == 'success', 'Build is not fully successful')


def validate_jobs(jobs, source, run_id):
    require(len(jobs) == len(JOB_STEPS) and {j.get('name') for j in jobs} == set(JOB_STEPS), 'Unexpected or missing required jobs')
    records = []
    for job in jobs:
        require(job.get('run_id') == run_id and job.get('head_sha') == source, 'Job belongs to a different build')
        require(type(job.get('run_attempt')) is int and job['run_attempt'] == BUILD_RUN_ATTEMPT, 'Job belongs to a different build attempt')
        require(job.get('status') == 'completed' and job.get('conclusion') == 'success', 'Required job did not pass: ' + job['name'])
        steps = {s['name']: s for s in job.get('steps', [])}
        require(all(steps.get(name, {}).get('conclusion') == 'success' for name in JOB_STEPS[job['name']]), 'Required step skipped or failed: ' + job['name'])
        records.append({'id': job['id'], 'name': job['name'], 'run_attempt': job['run_attempt'], 'conclusion': 'success', 'required_steps': sorted(JOB_STEPS[job['name']])})
    return sorted(records, key=lambda item: item['name'])


def allowed_publication_path(path):
    return path == 'README.md' or path.startswith('docs/') or path in PUBLISHER_FILES


def validate_compare(comparison, source):
    require(comparison.get('merge_base_commit', {}).get('sha') == source, 'Binary source is not an ancestor of publication commit')
    require(comparison.get('status') in ('ahead', 'identical'), 'Publication is not a descendant of binary source')
    files = comparison.get('files')
    require(isinstance(files, list) and len(files) < 300, 'Missing or possibly truncated commit comparison')
    for item in files:
        require(allowed_publication_path(item['filename']), 'Build-affecting publication difference: ' + item['filename'])
        if item.get('previous_filename'):
            require(allowed_publication_path(item['previous_filename']), 'Build source moved into documentation')
    return [{'path': item['filename'], 'status': item['status']} for item in files]


def validate_existing_tag(refs, publication):
    matches = [ref for ref in refs if ref.get('ref') == 'refs/tags/' + TAG]
    require(len(matches) <= 1, 'Duplicate publication tag')
    if matches:
        obj = matches[0]['object']
        require(obj.get('type') == 'commit' and obj.get('sha') == publication,
                'Existing tag has a different target; refusing to create or move a release')


def publication_target(release, workflow_sha, current_main):
    if release is not None:
        require(release.get('prerelease') is True and re.fullmatch('[0-9a-f]{40}', release.get('target_commitish', '')) is not None, 'Existing release is outside this contract')
        return release['target_commitish']
    require(re.fullmatch('[0-9a-f]{40}', workflow_sha) is not None, 'Missing publication commit')
    require(current_main == workflow_sha, 'Main advanced: rerun the fixed publication workflow')
    return workflow_sha


def validate_release_asset(name, asset, path):
    require(asset.get('name') == name and asset.get('state') == 'uploaded'
            and asset.get('digest') == 'sha256:' + file_sha(path)
            and asset.get('size') == path.stat().st_size,
            'Release asset differs; refusing replacement: ' + name)


def zip_files(path):
    result, total = {}, 0
    with zipfile.ZipFile(path) as archive:
        entries = archive.infolist()
        require(len(entries) <= 4096, 'Too many artifact members')
        seen = set()
        for item in entries:
            name = item.filename.rstrip('/')
            require(name and not name.startswith('/') and '\\' not in name and ':' not in name
                    and all(p not in ('', '.', '..') for p in name.split('/')), 'Unsafe artifact path')
            require(name not in seen, 'Duplicate artifact member')
            seen.add(name)
            require(stat.S_IFMT(item.external_attr >> 16) in (0, stat.S_IFREG, stat.S_IFDIR), 'Artifact symlink or special member')
            require(not item.flag_bits & 1, 'Encrypted artifact member')
            if item.is_dir():
                continue
            total += item.file_size
            require(item.file_size <= MAX_DOWNLOAD and total <= MAX_EXPANDED, 'Expanded artifact limit')
            result[name] = archive.read(item)
    return result


def inner_member(archive, name):
    if archive.name.endswith('.zip'):
        with zipfile.ZipFile(archive) as opened:
            return opened.read(name)
    with tarfile.open(archive, 'r:gz') as opened:
        member = opened.getmember(name)
        require(member.isfile(), 'Expected a regular package metadata/binary member')
        with opened.extractfile(member) as stream:
            return stream.read()


def verify_bundle(path, system, arch, source, run_id, work):
    members = zip_files(path)
    root = f'cs80-reference-{system}-{arch}-{source[:12]}'
    archive_name = root + ('.zip' if system == 'windows' else '.tar.gz')
    manifest_name = root + '.manifest.json'
    require(set(members) == {archive_name, manifest_name, 'SHA256SUMS'}, 'Unexpected outer package shape')
    folder = work / system / arch
    folder.mkdir(parents=True)
    for name, data in members.items():
        (folder / name).write_bytes(data)
    spec = importlib.util.spec_from_file_location('cs80_release_verifier', ROOT / 'scripts/verify-ci-bundle.py')
    verifier = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(verifier)
    verifier.verify(folder / archive_name, source)
    manifest = parse_json(members[manifest_name])
    require(manifest['platform'] == system and manifest['architecture'] == arch, 'Wrong package architecture')
    require(manifest.get('preset_library_revision') == REVISION, 'Wrong preset revision')
    require(manifest.get('workflow_run_id') == str(run_id), 'Package belongs to a different build run')
    require(manifest.get('juce_sha') == JUCE_SHA and manifest.get('juce_version') == '8.0.11', 'Wrong JUCE provenance')
    inner = folder / archive_name
    source_text = inner_member(inner, root + '/SOURCE.txt').decode()
    require(f'Source commit: {source}\n' in source_text and f'Preset library revision: {REVISION}\n' in source_text, 'SOURCE.txt mismatch')
    if system == 'windows':
        binaries = [(f'VST3/CS-80 Reference.vst3/Contents/x86_64-win/CS-80 Reference.vst3', False), ('Standalone/CS-80 Reference.exe', True)]
    elif system == 'linux':
        binaries = [(f'VST3/CS-80 Reference.vst3/Contents/{arch}-linux/CS-80 Reference.so', False), ('Standalone/CS-80 Reference', True)]
    else:
        binaries = [('VST3/CS-80 Reference.vst3/Contents/MacOS/CS-80 Reference', False), ('AU/CS-80 Reference.component/Contents/MacOS/CS-80 Reference', False), ('Standalone/CS-80 Reference.app/Contents/MacOS/CS-80 Reference', True)]
    for index, (name, standalone) in enumerate(binaries):
        temporary = folder / f'binary-{index}.bin'
        temporary.write_bytes(inner_member(inner, root + '/' + name))
        verifier.packager.verify_binary(temporary, system, arch, standalone)
        temporary.unlink()
    return {'platform': system, 'architecture': arch, 'formats': manifest['formats'], 'manifest_sha256': sha(members[manifest_name]), 'inventory_entries': len(manifest['files'])}


def float32(value):
    return struct.pack('<f', value)


def parameter_digest(values):
    value = 14695981039346656037
    for byte in struct.pack('<' + 'f' * len(values), *values):
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return format(value, 'x')


def verify_wav(data, seconds):
    require(data[:4] == b'RIFF' and data[8:12] == b'WAVE' and struct.unpack_from('<I', data, 4)[0] == len(data) - 8, 'Invalid WAV RIFF')
    chunks, at = {}, 12
    while at + 8 <= len(data):
        name, length = struct.unpack_from('<4sI', data, at)
        at += 8
        require(name not in chunks and at + length <= len(data), 'Duplicate or truncated WAV chunk')
        chunks[name] = data[at:at + length]
        at += length + length % 2
    require(at == len(data) and set(chunks) == {b'fmt ', b'fact', b'data'}, 'Unexpected WAV chunks')
    require(struct.unpack_from('<HHIIHH', chunks[b'fmt ']) == (3, 2, 48000, 384000, 8, 32), 'Wrong WAV audio format')
    frames = seconds * 48000
    require(len(chunks[b'data']) == frames * 8 and struct.unpack('<I', chunks[b'fact'])[0] == frames, 'Wrong WAV duration')
    energy, peak = 0.0, 0.0
    for (sample,) in struct.iter_unpack('<f', chunks[b'data']):
        require(math.isfinite(sample) and abs(sample) <= 1.0, 'Nonfinite or over-full-scale audition')
        energy += sample * sample
        peak = max(peak, abs(sample))
    return 20 * math.log10(max(peak, 1e-12)), 20 * math.log10(max(math.sqrt(energy / (2 * frames)), 1e-12))


def verify_auditions(path, source, run_id):
    files = zip_files(path)
    checks = {}
    for line in files['SHA256SUMS'].decode().splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
        require(match is not None and match[2] not in checks, 'Invalid audition checksum list')
        checks[match[2]] = match[1]
    require(len(checks) == 32 and set(files) == set(checks) | {'SHA256SUMS'}, 'Incomplete audition artifact')
    require(all(sha(files[name]) == digest for name, digest in checks.items()), 'Audition checksum mismatch')
    require(sha(files['inventory.json']) == INVENTORY_SHA, 'Audition bank differs from final calibrated bank')
    inventory = parse_json(files['inventory.json'])
    require(inventory['library_revision'] == REVISION and len(inventory['parameter_ids']) == 190 and len(inventory['programs']) == 301, 'Wrong audition inventory')
    programs = {p['name']: p for p in inventory['programs']}
    require(len(programs) == 301, 'Duplicate program identity')
    provenance = parse_json(files['provenance.json'])
    require(provenance.get('source_commit') == source and files['source-commit.txt'].decode().strip() == source, 'Audition source mismatch')
    require(provenance.get('run_id') == str(run_id) and provenance.get('repository') == REPO, 'Audition run mismatch')
    require(provenance.get('run_attempt') == str(BUILD_RUN_ATTEMPT), 'Audition build attempt mismatch')
    require(provenance.get('capture_status') == 'complete' and provenance.get('wav_count') == 24, 'Auditions did not finish')
    seen, wavs, duration = set(), set(), 0
    for folder, (pressure, hold, tail, expected) in AUDITIONS.items():
        rows = list(csv.DictReader(io.StringIO(files[folder + '/metrics.csv'].decode())))
        require(len(rows) == len(expected) and {r['name'] for r in rows} == expected, 'Wrong audition selection')
        for row in rows:
            name, program = row['name'], programs[row['name']]
            require(name not in seen, 'Duplicate audition program')
            seen.add(name)
            require(int(row['index']) == program['index'] and row['category'] == program['category'] and row['parameter_digest'] == parameter_digest(program['values']), 'Factory parameter identity mismatch')
            require(int(row['rank']) == 0 and int(row['sample_rate']) == 48000 and float32(float(row['pressure'])) == float32(pressure) and float32(float(row['velocity'])) == float32(.8), 'Audition controller settings changed')
            require(float(row['hold_seconds']) == hold and float(row['tail_seconds']) == tail and int(row['nonfinite']) == 0 and int(row['samples_over_full_scale']) == 0, 'Audition capture condition mismatch')
            note = 48 if program['category'] == 'Bass' else 67 if program['category'] in ('Winds', 'Leads') else 72 if program['category'] == 'Bells' else 60
            require(int(row['note']) == note, 'Wrong audition note')
            matches = [p for p in files if p.startswith(folder + '/' + str(program['index']) + '-') and p.endswith('.wav')]
            require(len(matches) == 1, 'Missing or ambiguous audition WAV')
            wavs.add(matches[0])
            peak, rms = verify_wav(files[matches[0]], hold + tail)
            require(abs(peak - float(row['peak_dbfs'])) < 1e-5 and abs(rms - float(row['rms_dbfs'])) < 1e-5, 'WAV was changed after metrics')
            duration += hold + tail
    require(len(seen) == 24 and duration == 117 and wavs == {p for p in files if p.endswith('.wav')}, 'Wrong audition total')
    return {'programs': 24, 'seconds': 117, 'inventory_sha256': INVENTORY_SHA, 'checksums': 32}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--publish', action='store_true', help='Publish only after all fixed-contract checks pass')
    args = parser.parse_args()
    source = os.environ.get('PUBLISH_BINARY_SHA', '')
    run_text = os.environ.get('PUBLISH_BUILD_RUN_ID', '')
    require(re.fullmatch('[0-9a-f]{40}', source) is not None and re.fullmatch('[1-9][0-9]*', run_text) is not None, 'Bind the final binary source and build run first')
    run_id = int(run_text)
    require(os.environ.get('GITHUB_REPOSITORY') == REPO and os.environ.get('GITHUB_REF') == 'refs/heads/main', 'Publisher must run on this repository main')
    client = Client(os.environ.get('GH_TOKEN', ''))
    api = '/repos/' + REPO
    run = client.json(api + f'/actions/runs/{run_id}')
    validate_run(run, source, run_id)
    event = parse_json(Path(os.environ['GITHUB_EVENT_PATH']).read_bytes())
    if os.environ.get('GITHUB_EVENT_NAME') == 'workflow_run':
        require(event.get('workflow_run', {}).get('id') == run_id and event['workflow_run']['head_sha'] == source, 'Wrong triggering workflow')
        require(type(event['workflow_run'].get('run_attempt')) is int and event['workflow_run']['run_attempt'] == BUILD_RUN_ATTEMPT, 'Wrong triggering build attempt')
    else:
        require(os.environ.get('GITHUB_EVENT_NAME') == 'workflow_dispatch', 'Unexpected publication event')
    jobs = validate_jobs(client.pages(api + f'/actions/runs/{run_id}/jobs?filter=latest', 'jobs'), source, run_id)
    releases = [r for r in client.pages(api + '/releases') if r['tag_name'] == TAG]
    require(len(releases) <= 1, 'Duplicate release tag')
    release = releases[0] if releases else None
    current_main = client.json(api + '/git/ref/heads/main')['object']['sha']
    publication = publication_target(release, os.environ.get('GITHUB_SHA', ''), current_main)
    compare = validate_compare(client.json(api + f'/compare/{source}...{publication}'), source)
    validate_existing_tag(client.json(api + '/git/matching-refs/tags/' + TAG), publication)
    if release and not release['draft']:
        ref = client.json(api + '/git/ref/tags/' + TAG)['object']
        require(ref['type'] == 'commit' and ref['sha'] == publication, 'Published tag does not identify the recorded publication commit')
    wanted = set(BUNDLES) | {'cs80-curated-preset-audition-' + source}
    artifacts = client.pages(api + f'/actions/runs/{run_id}/artifacts', 'artifacts')
    selected = [a for a in artifacts if a['name'] in wanted]
    require(len(selected) == len(wanted) and {a['name'] for a in selected} == wanted, 'Missing or duplicate release artifact')
    records = []
    with tempfile.TemporaryDirectory(prefix='cs80-release-') as temporary:
        work = Path(temporary)
        assets = {}
        for artifact in sorted(selected, key=lambda a: a['name']):
            require(artifact.get('expired') is False and type(artifact.get('id')) is int, 'Expired or invalid artifact')
            require(artifact.get('workflow_run', {}).get('id') == run_id and artifact['workflow_run'].get('head_sha') == source, 'Artifact run identity differs')
            digest = artifact.get('digest', '')
            require(re.fullmatch('sha256:[0-9a-f]{64}', digest) is not None, 'Artifact lacks GitHub SHA-256')
            if artifact['name'] in BUNDLES:
                system, arch, asset_name = BUNDLES[artifact['name']]
            else:
                asset_name = 'cs80-preset-auditions-' + REVISION + '.zip'
            path = work / asset_name
            client.download(api + f'/actions/artifacts/{artifact["id"]}/zip', path, digest[7:], artifact['size_in_bytes'])
            details = verify_bundle(path, system, arch, source, run_id, work) if artifact['name'] in BUNDLES else verify_auditions(path, source, run_id)
            assets[asset_name] = path
            records.append({'artifact_id': artifact['id'], 'artifact_name': artifact['name'], 'release_asset': asset_name, 'sha256': digest[7:], 'bytes': path.stat().st_size, 'verification': details})
        receipt = {'schema': 1, 'preset_library_revision': REVISION, 'binary_source_commit': source, 'build_run_id': run_id, 'build_run_attempt': run['run_attempt'], 'build_run_url': run['html_url'], 'publication_commit': publication, 'tag': TAG, 'publication_only_differences': compare, 'required_jobs': jobs, 'artifacts': records, 'qualification': 'Unsigned development prerelease. Verification is bounded build, archive and audition evidence; not notarization, universal DAW compatibility, or measured acoustic/hardware equivalence.'}
        receipt_path = work / 'release-provenance.json'
        receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
        assets[receipt_path.name] = receipt_path
        sums = work / 'SHA256SUMS'
        sums.write_text(''.join(f'{file_sha(path)}  {name}\n' for name, path in sorted(assets.items())))
        assets[sums.name] = sums
        body = (f'Unsigned development prerelease for Circuit preset library {REVISION}.\n\n'
                f'Binary source: `{source}`.\nBuild: {run["html_url"]}.\n'
                f'The tag identifies publication commit `{publication}`; only the allowlisted documentation/publisher paths differ from the binary source. The package SOURCE.txt and manifests retain the exact binary source.\n\n'
                'Four native platform bundles and 24 factory audition WAVs are copied byte-for-byte from the successful build artifacts. All six required jobs and named test/validator/package steps passed; archive hashes, source identity, architecture and audition integrity were checked before publication.\n\n'
                'Use ordinary Circuit mode and explicitly select a factory program in **Library** to hear its current voicing. Existing saved sessions keep their serialized sounds. **Pressure Lead** requires aftertouch.\n\n'
                'A pre-existing limitation remains for Program changes sent only through the VST3 processing queue. See the [host-program qualification and refinement report](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/main/docs/PRESET_REFINEMENT_2026_11_1.md#native-recall-and-host-program-qualification) for the observed controller dependency and verification scope.\n\n'
                'These packages are not signed/notarized releases and have not been qualified with every DAW. See release-provenance.json and SHA256SUMS for exact identities and verification scope.\n')
        if not args.publish:
            print(json.dumps({'status': 'verified; publication not requested', 'binary_source': source, 'publication_commit': publication, 'assets': sorted(assets)}, indent=2))
            return
        if release is None:
            require(client.json(api + '/git/ref/heads/main')['object']['sha'] == publication, 'Main advanced before creation; no release created')
            release = client.json(api + '/releases', 'POST', {'tag_name': TAG, 'target_commitish': publication, 'name': 'Circuit library ' + REVISION + ' — unsigned development build', 'body': body, 'draft': True, 'prerelease': True, 'make_latest': 'false', 'generate_release_notes': False})
        require(release.get('body') == body, 'Existing release notes differ; refusing to overwrite')
        existing = client.pages(api + f'/releases/{release["id"]}/assets')
        require(len({a['name'] for a in existing}) == len(existing) and {a['name'] for a in existing} <= set(assets), 'Existing release has unexpected or duplicate assets')
        by_name = {a['name']: a for a in existing}
        for name, path in sorted(assets.items()):
            if name in by_name:
                validate_release_asset(name, by_name[name], path)
                continue
            require(release['draft'], 'Published release is incomplete; refusing to mutate it')
            upload = release['upload_url'].split('{', 1)[0]
            require(upload == f'https://uploads.github.com/repos/{REPO}/releases/{release["id"]}/assets', 'Unexpected release upload URL')
            with client.request(upload + '?name=' + urllib.parse.quote(name, safe=''), 'POST', path.read_bytes(), binary=True) as response:
                uploaded = parse_json(response.read(1024 * 1024))
            validate_release_asset(name, uploaded, path)
        # Refetch every asset before making the complete draft public.
        final_assets = client.pages(api + f'/releases/{release["id"]}/assets')
        require(len(final_assets) == len(assets) and {a['name'] for a in final_assets} == set(assets), 'Release asset set is incomplete')
        for asset in final_assets:
            validate_release_asset(asset['name'], asset, assets[asset['name']])
        if release['draft']:
            release = client.json(api + f'/releases/{release["id"]}', 'PATCH', {'draft': False, 'prerelease': True, 'make_latest': 'false'})
        ref = client.json(api + '/git/ref/tags/' + TAG)['object']
        require(ref['type'] == 'commit' and ref['sha'] == publication, 'Final publication tag mismatch')
        print(json.dumps({'status': 'published and metadata verified', 'release_url': release['html_url'], 'binary_source_commit': source, 'publication_commit': publication, 'build_run_id': run_id, 'assets': [{'name': a['name'], 'url': a['browser_download_url'], 'sha256': a['digest'][7:]} for a in final_assets]}, indent=2))


if __name__ == '__main__':
    main()
