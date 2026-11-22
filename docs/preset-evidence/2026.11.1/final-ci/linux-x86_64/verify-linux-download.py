#!/usr/bin/env python3
"""Verify the fixed Linux archive bytes and ELF roles without loading products."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import tarfile
import tempfile
import urllib.parse
from common import ROOT, WORK, SOURCE, RUN_ID, read_json, validate_run, download_artifact, sha_file, require

sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact-json', type=Path, required=True)
    parser.add_argument('--run-json', type=Path, required=True)
    args = parser.parse_args()
    meta, run = read_json(args.artifact_json), read_json(args.run_json)
    validate_run(run)
    outer, download = download_artifact(meta, 'cs80-linux-install-bundle', reserve_inner=True)
    spec = importlib.util.spec_from_file_location('cs80_trusted_publisher', ROOT / 'scripts/publish-preset-refinement.py')
    publisher = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(publisher)
    stem = 'cs80-reference-linux-x86_64-' + SOURCE[:12]
    expected = {
        'VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so': False,
        'Standalone/CS-80 Reference': True,
    }
    binaries, guides = [], []
    with tempfile.TemporaryDirectory(prefix='offline-linux-', dir=WORK) as temporary:
        verification = publisher.verify_bundle(outer, 'linux', 'x86_64', SOURCE, RUN_ID, Path(temporary))
        inner = Path(temporary) / 'linux/x86_64' / (stem + '.tar.gz')
        manifest = read_json(inner.with_name(stem + '.manifest.json'))
        inventory = {item['path']: item for item in manifest['files']}
        inner_identity = {'name': inner.name, 'bytes': inner.stat().st_size, 'sha256': sha_file(inner)}
        with tarfile.open(inner, 'r:gz') as archive:
            members = {member.name: member for member in archive.getmembers()}

            def read(name):
                member = members[stem + '/' + name]
                require(member.isfile(), 'Metadata is not a regular file: ' + name)
                with archive.extractfile(member) as stream:
                    return stream.read()

            source_text = read('SOURCE.txt').decode()
            provenance = dict(line.split('=', 1) for line in read('evidence/juce-provenance.txt').decode().splitlines() if '=' in line)
            require(provenance.get('actual_sha') == publisher.JUCE_SHA and provenance.get('actual_version') == '8.0.11', 'Wrong JUCE identity')
            require(provenance.get('workflow_commit') == SOURCE and provenance.get('verification') == 'PASS'
                    and provenance.get('source_clean') == 'true', 'Unverified JUCE source')
            for member in members.values():
                if not member.isfile() or member.size < 4:
                    continue
                with archive.extractfile(member) as stream:
                    header = stream.read(64)
                    if header[:4] != b'\x7fELF':
                        continue
                    relative = member.name[len(stem) + 1:]
                    require(relative in expected, 'Unexpected ELF payload: ' + relative)
                    require(len(header) == 64 and header[:7] == b'\x7fELF\x02\x01\x01', 'Expected little-endian ELF64')
                    fields = struct.unpack('<16sHHIQQQIHHHHHH', header)
                    _, kind, machine, version, entry, phoff, shoff, flags, ehsize, phentsize, phnum, shentsize, shnum, shstr = fields
                    require(machine == 62 and version == 1 and ehsize == 64, 'Expected native x86_64 ELF')
                    standalone = expected[relative]
                    require(kind in ({2, 3} if standalone else {3}), 'Wrong executable/shared-object role')
                    require(phentsize == 56 and 0 < phnum < 128 and phoff + phnum * phentsize <= member.size, 'Invalid ELF program headers')
                    stream.seek(phoff)
                    program_headers = [struct.unpack('<IIQQQQQQ', stream.read(56)) for _ in range(phnum)]
                    interpreter = [p for p in program_headers if p[0] == 3]
                    require(len(interpreter) == (1 if standalone else 0), 'Unexpected ELF interpreter role')
                    if standalone:
                        require(member.mode & 0o111, 'Standalone ELF image lacks executable mode')
                        require(entry > 0, 'Standalone lacks entry point')
                    binary = {'path': relative, 'bytes': member.size, 'sha256': inventory[relative]['sha256'], 'format': 'ELF64 little-endian',
                              'machine': machine, 'type': kind, 'role': 'standalone' if standalone else 'VST3 shared object',
                              'entry_point': entry, 'program_headers': phnum, 'interpreter_count': len(interpreter), 'mode': oct(member.mode)}
                    if interpreter:
                        _, _, offset, _, _, length, _, _ = interpreter[0]
                        require(0 < length < 1024 and offset + length <= member.size, 'Invalid interpreter range')
                        stream.seek(offset)
                        binary['interpreter'] = stream.read(length).rstrip(b'\0').decode('ascii')
                    binaries.append(binary)
            require({item['path'] for item in binaries} == set(expected), 'Missing advertised ELF images')
            for name in ('INSTALL_AND_QUICKSTART.md', 'PRESET_GUIDE.md', 'DISTRIBUTION_STATUS.md'):
                content = read(name).decode()
                pins = re.findall(r'https://github\.com/cheshmakzanoon-ops/yamaha-cs80/(?:blob|tree)/([^/\s)]+)', content)
                require(all(pin == SOURCE for pin in pins), 'Wrong guide source pin: ' + name)
                targets = re.findall(r'(?<!!)\[[^\]\n]+\]\(([^\s()]+)(?:\s+"[^"]*")?\)', content)
                local = [t for t in targets if not urllib.parse.urlsplit(t).scheme and not t.startswith('#')]
                for target in local:
                    path = urllib.parse.unquote(urllib.parse.urlsplit(target).path)
                    require(stem + '/' + path in members, 'Missing bundled guide link: ' + target)
                guides.append({'name': name, 'sha256': hashlib.sha256(content.encode()).hexdigest(),
                               'repository_links_pinned': len(pins), 'local_links': local})
    proof = {
        'assignment': 54, 'status': 'artifact_bytes_verified', 'verified_at_utc': datetime.now(timezone.utc).isoformat(),
        'ci_qualification': 'Separate builder/CTest and three Linux validator lane qualification is required.',
        'artifact_api': meta, 'run_api': run, 'run_attempt': 1, 'download': download,
        'inner_archive': inner_identity, 'manifest_identity': {k: manifest[k] for k in
            ('commit', 'preset_library_revision', 'workflow_run_id', 'juce_sha', 'juce_version', 'platform', 'architecture', 'formats')},
        'source_text': source_text, 'juce_provenance': provenance, 'offline_verifier': verification,
        'binary_headers': binaries, 'bundled_guides': guides,
        'trusted_verifier_inputs': {name: sha_file(ROOT / 'scripts' / name) for name in
            ('publish-preset-refinement.py', 'verify-ci-bundle.py', 'package-ci-artifacts.py')},
        'limitations': 'Archive consistency and ELF role checks only. No downloaded code executed, no listening, signing, hardware identity, or all-DAW qualification. Temporary duplicate inner archive removed.'
    }
    (WORK / 'artifact-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({'status': proof['status'], 'artifact_id': meta['id'], 'outer_sha256': download['sha256'],
                      'inventory_entries': verification['inventory_entries'], 'elf_images': len(binaries)}, indent=2))


if __name__ == '__main__':
    main()
