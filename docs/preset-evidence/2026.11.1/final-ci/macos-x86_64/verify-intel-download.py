#!/usr/bin/env python3
"""Verify the fixed Intel Mac candidate archive; never publish or run payloads."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import struct
import tarfile
import tempfile
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(__file__).resolve().parent
SOURCE = '6d3cc220edef5a4a2c7cd968ea8a786c7c83198c'
RUN = 45044040694
ARTIFACT = 'cs80-macos-15-intel-install-bundle'
JUCE = 'c352e2489000ffeb9e397051377d75eef2efe641'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha_file(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--artifact-json', type=Path, required=True)
    parser.add_argument('--run-json', type=Path, required=True)
    args = parser.parse_args()
    meta = json.loads(args.artifact_json.read_text())
    run = json.loads(args.run_json.read_text())
    require(run['id'] == RUN and run['head_sha'] == SOURCE and run['run_attempt'] == 1,
            'Wrong source/run or retried build')
    require(run['head_branch'] == 'main' and run['event'] == 'push'
            and run['head_repository']['full_name'] == 'cheshmakzanoon-ops/yamaha-cs80',
            'Wrong repository/event/branch')
    require(run['name'] == 'build' and run['path'] == '.github/workflows/build.yml', 'Wrong workflow')
    require(meta['name'] == ARTIFACT and not meta['expired'], 'Wrong or expired artifact')
    require(meta['workflow_run']['id'] == RUN, 'Wrong artifact workflow run')
    require(meta['workflow_run']['head_sha'] == SOURCE, 'Wrong artifact source')
    require(re.fullmatch(r'sha256:[0-9a-f]{64}', meta['digest']), 'Missing API digest')
    require(type(meta['id']) is int and 0 < meta['size_in_bytes'] < 512 * 1024 * 1024,
            'Invalid artifact id/size')
    cache = ROOT / 'build-coordination/artifact-cache'
    cache.mkdir(parents=True, exist_ok=True)
    outer = cache / (ARTIFACT + '-' + str(meta['id']) + '.zip')
    url = 'https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/' + str(meta['id']) + '.zip'
    if not outer.exists():
        require(shutil.disk_usage(cache).free > meta['size_in_bytes'] * 3 + 32 * 1024 * 1024,
                'Insufficient free space for bounded package inspection')
        request = urllib.request.Request(url, headers={'User-Agent': 'CS80-package-verification/1.0'})
        part = outer.with_suffix('.zip.part')
        try:
            with urllib.request.urlopen(request, timeout=45) as response, part.open('wb') as output:
                require(response.status == 200, 'Archive GET did not return 200')
                received = 0
                for chunk in iter(lambda: response.read(1024 * 1024), b''):
                    received += len(chunk)
                    require(received <= meta['size_in_bytes'], 'Download exceeds API artifact size')
                    output.write(chunk)
            part.replace(outer)
        finally:
            part.unlink(missing_ok=True)
    outer_sha = sha_file(outer)
    require(outer.stat().st_size == meta['size_in_bytes'], 'Artifact size differs from API')
    require(outer_sha == meta['digest'].split(':')[1], 'Artifact hash differs from API')

    spec = importlib.util.spec_from_file_location('cs80_publisher_offline', ROOT / 'scripts/publish-preset-refinement.py')
    publisher = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(publisher)
    stem = 'cs80-reference-macos-x86_64-' + SOURCE[:12]
    with tempfile.TemporaryDirectory(prefix='inspection-', dir=WORK) as tmp:
        verification = publisher.verify_bundle(outer, 'macos', 'x86_64', SOURCE, RUN, Path(tmp))
        folder = Path(tmp) / 'macos/x86_64'
        inner = folder / (stem + '.tar.gz')
        manifest = json.loads((folder / (stem + '.manifest.json')).read_text())
        inner_record = {'name': inner.name, 'bytes': inner.stat().st_size, 'sha256': sha_file(inner),
                        'temporary_copy_removed_after_verification': True}
        binaries, guides = [], []
        with tarfile.open(inner, 'r:gz') as archive:
            members = {item.name: item for item in archive.getmembers()}
            def read_member(name):
                member = members[stem + '/' + name]
                require(member.isfile(), 'Expected regular metadata file: ' + name)
                with archive.extractfile(member) as stream:
                    return stream.read()
            source_text = read_member('SOURCE.txt').decode()
            provenance_text = read_member('evidence/juce-provenance.txt').decode()
            provenance = dict(line.split('=', 1) for line in provenance_text.splitlines() if '=' in line)
            require(provenance['actual_sha'] == JUCE and provenance['actual_version'] == '8.0.11',
                    'Wrong JUCE version/source')
            require(provenance['workflow_commit'] == SOURCE and provenance['verification'] == 'PASS'
                    and provenance['source_clean'] == 'true', 'Unverified JUCE provenance')
            # Scan every inventoried file for Mach-O/FAT headers, not just declared products.
            for item in manifest['files']:
                if item['type'] != 'file':
                    continue
                member = members[stem + '/' + item['path']]
                with archive.extractfile(member) as stream:
                    header = stream.read(32)
                magic = header[:4]
                require(magic not in (b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca',
                                      b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca'),
                        'Unexpected universal Mach-O payload')
                if magic in (b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xce',
                             b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf'):
                    require(magic == b'\xcf\xfa\xed\xfe' and len(header) == 32,
                            'Expected little-endian Mach-O64 payload')
                    cpu, subtype, filetype, load_commands, command_bytes, flags = struct.unpack_from('<6I', header, 4)
                    require(cpu == 0x01000007, 'Non-x86_64 executable payload: ' + item['path'])
                    binaries.append({'path': item['path'], 'bytes': item['size'], 'sha256': item['sha256'],
                                     'format': 'Mach-O64', 'cpu_type': hex(cpu), 'cpu_subtype': hex(subtype),
                                     'file_type': filetype, 'load_commands': load_commands,
                                     'load_command_bytes': command_bytes, 'flags': hex(flags)})
            expected = {'VST3/CS-80 Reference.vst3/Contents/MacOS/CS-80 Reference': {6, 8},
                        'AU/CS-80 Reference.component/Contents/MacOS/CS-80 Reference': {6, 8},
                        'Standalone/CS-80 Reference.app/Contents/MacOS/CS-80 Reference': {2}}
            actual = {item['path']: item['file_type'] for item in binaries}
            require(set(actual) == set(expected) and all(actual[name] in expected[name] for name in expected),
                    'Unexpected/missing code payload or Mach-O file type')
            for name in ['INSTALL_AND_QUICKSTART.md', 'PRESET_GUIDE.md', 'DISTRIBUTION_STATUS.md']:
                content = read_member(name).decode()
                pins = re.findall(r'https://github\.com/cheshmakzanoon-ops/yamaha-cs80/(?:blob|tree)/([^/\s)]+)', content)
                require(all(pin == SOURCE for pin in pins), 'Wrong repository link source in ' + name)
                targets = re.findall(r'(?<!!)\[[^\]\n]+\]\(([^\s()]+)(?:\s+"[^"]*")?\)', content)
                local = [target for target in targets if not urllib.parse.urlsplit(target).scheme and not target.startswith('#')]
                for target in local:
                    pathname = urllib.parse.unquote(urllib.parse.urlsplit(target).path)
                    require(stem + '/' + pathname in members, 'Missing bundled relative link: ' + target)
                guides.append({'name': name, 'sha256': hashlib.sha256(content.encode()).hexdigest(),
                               'repository_links_pinned': len(pins), 'local_links': local})
    proof = {
        'assignment': 53, 'verified_at_utc': datetime.now(timezone.utc).isoformat(),
        'run_attempt': run['run_attempt'],
        'status': 'artifact_bytes_verified',
        'ci_qualification': 'Exact builder CTest/pluginval log qualification is recorded separately; all-platform publication remains separate.',
        'artifact_api': meta, 'download': {'url': url, 'credential_free_get': True,
            'path': str(outer.relative_to(ROOT)), 'bytes': outer.stat().st_size, 'sha256': outer_sha},
        'inner_archive': inner_record,
        'manifest_identity': {key: manifest[key] for key in ['commit', 'preset_library_revision', 'workflow_run_id',
                             'juce_sha', 'juce_version', 'platform', 'architecture', 'formats']},
        'source_text': source_text, 'juce_provenance': provenance, 'offline_verifier': verification,
        'binary_headers': binaries, 'bundled_guides': guides,
        'trusted_verifier_inputs': {name: sha_file(ROOT / 'scripts' / name) for name in
            ['publish-preset-refinement.py', 'verify-ci-bundle.py', 'package-ci-artifacts.py']},
        'limitations': 'Archive consistency and native architecture inspection, plus separately recorded CI execution. No payload executed locally; no signing, notarization, subjective listening, hardware matching, arbitrary DAW or publication claim.'
    }
    (WORK / 'artifact-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({'status': proof['status'], 'artifact_id': meta['id'], 'sha256': outer_sha,
                      'bytes': outer.stat().st_size, 'binaries': len(binaries), 'guides': len(guides)}, indent=2))


if __name__ == '__main__':
    main()
