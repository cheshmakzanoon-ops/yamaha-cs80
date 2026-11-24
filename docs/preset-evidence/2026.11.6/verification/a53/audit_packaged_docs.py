#!/usr/bin/env python3
"""Inspect actual ARM package documents without executing any product binary."""
from pathlib import Path
import hashlib
import importlib.util
import io
import json
import re
import subprocess
import sys
import tarfile
import tempfile
from urllib.parse import urlsplit, unquote
import zipfile

ROOT = Path(__file__).resolve().parents[5]
SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
ARTIFACT = Path('/workspace/scratch/b9c11dd92abc/native-artifacts/14910025415.zip')
EXPECTED_SHA = 'c08e83380b0403b03f11684d2cc8e9f88dd2ebd78f217be7d7940952a2143e61'
OUT = Path(__file__).resolve().parent
checks = []

def check(value, label):
    if not value:
        raise AssertionError(label)
    checks.append(label)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def source(path):
    return subprocess.check_output(['git', '-C', str(ROOT), 'show', SOURCE + ':' + path])

raw = ARTIFACT.read_bytes()
check(len(raw) == 14986588 and sha(raw) == EXPECTED_SHA, 'actual API-bound outer ZIP identity')
with zipfile.ZipFile(io.BytesIO(raw)) as outer:
    names = outer.namelist()
    check(len(names) == 3 and len(set(names)) == 3, 'three unique outer package members')
    for line in outer.read('SHA256SUMS').decode().splitlines():
        expected, name = line.split('  ', 1)
        check(sha(outer.read(name)) == expected, 'outer checksum: ' + name)
    tgz_name = next(n for n in names if n.endswith('.tar.gz'))
    meta_name = next(n for n in names if n.endswith('.manifest.json'))
    manifest = json.loads(outer.read(meta_name))
    with tarfile.open(fileobj=io.BytesIO(outer.read(tgz_name)), mode='r:gz') as archive:
        regular = [m for m in archive.getmembers() if m.isfile()]
        prefix = tgz_name.removesuffix('.tar.gz') + '/'
        check(all(m.name.startswith(prefix) for m in regular), 'one source-specific package root')
        members = {m.name.removeprefix(prefix): archive.extractfile(m).read() for m in regular}

check(manifest['commit'] == SOURCE and manifest['architecture'] == 'arm64'
      and manifest['platform'] == 'macos', 'manifest source and advertised native platform')
check(manifest['preset_library_revision'] == '2026.11.6', 'manifest factory revision')
guides = {'docs/INSTALL_AND_QUICKSTART.md': 'INSTALL_AND_QUICKSTART.md',
          'docs/PRESET_GUIDE.md': 'PRESET_GUIDE.md',
          'docs/CI_DISTRIBUTION_STATUS.md': 'DISTRIBUTION_STATUS.md'}
docs = {}
with tempfile.TemporaryDirectory(prefix='a53-source-guides-') as directory:
    tmp = Path(directory)
    script = tmp / 'package-ci-artifacts.py'
    script.write_bytes(source('scripts/package-ci-artifacts.py'))
    for name in guides:
        p = tmp / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(source(name))
    spec = importlib.util.spec_from_file_location('a53_source_packager', script)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    module.ROOT = tmp
    for src, dst in guides.items():
        expected = module.packaged_markdown(src, SOURCE, guides)
        check(members[dst] == expected, 'exact source-time link rewrite: ' + dst)
        docs[dst] = {'bytes': len(expected), 'sha256': sha(expected),
                     'source_path': src, 'source_sha256': sha(source(src))}

notices = {'LICENSE': 'LICENSE',
           'licenses/BarlowCondensed-OFL.txt': 'assets/fonts/barlow-condensed/OFL.txt',
           'licenses/BarlowCondensed-SOURCE.txt': 'assets/fonts/barlow-condensed/SOURCE.txt',
           'licenses/MTS-ESP-LICENSE.txt': 'src/vendor/mts-esp/LICENSE',
           'licenses/libxml2-Copyright.txt': 'src/vendor/libxml2/Copyright',
           'licenses/libxml2-SOURCE.txt': 'src/vendor/libxml2/SOURCE.txt',
           'licenses/libxml2-ADDITIONAL-NOTICES.txt': 'src/vendor/libxml2/ADDITIONAL-NOTICES.txt'}
for dst, src in notices.items():
    check(members[dst] == source(src), 'verbatim source notice: ' + dst)
juce = Path('/workspace/scratch/c98fb40a9529/juce-validation')
juce_sha = subprocess.check_output(['git', '-C', str(juce), 'rev-parse', 'HEAD'], text=True).strip()
check(juce_sha == 'c352e2489000ffeb9e397051377d75eef2efe641', 'local JUCE notice reference is pinned source')
check(members['licenses/JUCE-LICENSE.md'] == (juce / 'LICENSE.md').read_bytes(), 'verbatim pinned JUCE notice')

src_text = members['SOURCE.txt'].decode()
check(f'Source commit: {SOURCE}\n' in src_text and 'Preset library revision: 2026.11.6\n' in src_text,
      'SOURCE records full commit and revision')
install = members['INSTALL_AND_QUICKSTART.md'].decode()
preset = members['PRESET_GUIDE.md'].decode()
distribution = members['DISTRIBUTION_STATUS.md'].decode()
check('204 core parameters and 310 host positions' in preset and 'fourteen saved controls' in preset,
      'preset guide states current parameter/control totals')
check('explicitly press **LOAD**' in preset and '**RELOAD**' in preset and 'Play fresh notes afterward' in preset,
      'explicit revised-recipe recall and fresh-note instructions')
check('already marked loaded can leave the' in preset and '**REVERT** restores' in preset,
      'Audition and Revert distinction retained')
check('captured on the **next\naccepted note**' in preset and 'mix and brightness remain live' in preset,
      'next-note versus live body controls')
check('Unsupported model/material combinations use Standard' in preset and
      'standalone interface do not yet provide\ndedicated body-control widgets' in preset,
      'fallback and host-only manual control access disclosed')
check('Apple Silicon, native arm64' in install and 'not universal binaries' in install,
      'native architecture and non-universal package instructions')
check('no release signing/notarization step' in install and 'ad-hoc build signatures' in install,
      'signing claims distinguish ad-hoc from publisher release signing')
check('Source candidate — 2026.11.5' in distribution and '2026.11.4' in distribution,
      'bundled distribution history is the unchanged source-time snapshot')
current = 'https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/main/README.md#downloads'
check(current in preset and current in distribution, 'explicit current main download destinations remain live')

link_rows = []
link_re = re.compile(r'!?\[[^\]\n]+\]\(([^\s()]+)')
for name in guides.values():
    for target in link_re.findall(members[name].decode()):
        url = urlsplit(target)
        if not url.scheme and not url.netloc and url.path:
            check(url.path in members, 'local packaged link target: ' + url.path)
            kind = 'bundled'
        elif url.netloc == 'github.com' and '/blob/' + SOURCE + '/' in url.path:
            path = unquote(url.path.split('/blob/' + SOURCE + '/', 1)[1])
            typ = subprocess.check_output(['git', '-C', str(ROOT), 'cat-file', '-t', SOURCE + ':' + path], text=True).strip()
            check(typ in ('blob', 'tree'), 'source-pinned repository link target: ' + path)
            kind = 'source-pinned-' + typ
        elif target == current:
            kind = 'explicit-current-downloads'
        else:
            kind = 'external-or-fragment-not-network-tested'
        link_rows.append({'guide': name, 'target': target, 'kind': kind})

body = source('src/core/InstrumentBody.h').decode()
materials = source('src/core/InstrumentBodyMaterials.h').decode()
check('makeMaterialModes' in body and 'constexpr std::array' in body,
      'material modes are compiled numerical tables')
check(not any(t in body + materials for t in ['ifstream', 'fopen(', 'AudioFormatReader', 'FileInputStream']),
      'material generator has no external audio/file reader')
check(not any(n.lower().endswith(('.wav', '.aiff', '.flac', '.sfz', '.sf2')) for n in members),
      'archive needs no new sample-set payload')

result = {'scope': 'actual ARM package user documents and source-time packaging contract only',
          'source_commit': SOURCE, 'artifact_id': 14910025415, 'artifact_bytes': len(raw),
          'artifact_sha256': sha(raw), 'checks_passed': len(checks), 'checks': checks,
          'document_receipts': docs, 'notice_receipts': {n: {'bytes':len(members[n]),'sha256':sha(members[n])}
                                                       for n in list(notices)+['licenses/JUCE-LICENSE.md']},
          'link_inventory': link_rows, 'source_packager_sha256': sha(source('scripts/package-ci-artifacts.py')),
          'other_native_archives_reviewed': False, 'release_qualified': False,
          'distribution_snapshot_caveat': 'The frozen bundled distribution record describes .5 candidate/.4 publication; SOURCE and manifest identify this .6 package, and explicit current-download URLs point to main.'}
(OUT / 'package-doc-audit.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps({'checks_passed': len(checks), 'artifact_id': result['artifact_id'],
                  'guide_links_classified': len(link_rows), 'release_qualified':False}, indent=2))
