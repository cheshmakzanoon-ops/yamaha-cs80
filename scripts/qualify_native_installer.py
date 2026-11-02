#!/usr/bin/env python3
"""Exercise actual installer document payloads on native CI, never publish a release.

Linux/macOS packages are expanded without system installation. Windows selects
only the Standalone component in a disposable directory on its ephemeral runner,
then uninstalls it. No DAW or VST3 installation is implied by this bounded check.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from verify_distribution_docs import verify_documents

ROOT = Path(__file__).resolve().parents[1]


def run(command, log, *, timeout=180):
    with log.open('x', encoding='utf-8') as stream:
        stream.write('Command: ' + json.dumps([str(arg) for arg in command]) + '\n')
        stream.flush()
        subprocess.run([str(arg) for arg in command], cwd=ROOT, check=True,
                       stdout=stream, stderr=subprocess.STDOUT, timeout=timeout)


def label_ci_package(package, source):
    labelled = package.with_name(f'{package.stem}-CI-UNSIGNED-{source[:12]}{package.suffix}')
    if labelled.exists():
        raise ValueError('Refusing to overwrite a labelled CI installer')
    package.rename(labelled)
    return labelled


def qualify(platform, build, out, source):
    if not re.fullmatch('[0-9a-f]{40}', source):
        raise ValueError('Expected a full lowercase source commit SHA')
    native = 'windows' if sys.platform == 'win32' else 'macos' if sys.platform == 'darwin' else 'linux' if sys.platform.startswith('linux') else None
    if platform != native:
        raise ValueError('Native installer checks must run on their actual platform')
    build, out = build.resolve(), out.resolve()
    if out.exists():
        raise ValueError('Refusing to overwrite native installer evidence')
    out.mkdir(parents=True)
    licence = build / '_deps/juce-src/LICENSE.md'
    if not licence.is_file():
        licence = ROOT / 'deps/juce/LICENSE.md'
    with tempfile.TemporaryDirectory(prefix='cs80-installer-check-') as temp:
        work = Path(temp)
        if platform == 'linux':
            run(['bash', ROOT / 'installer/linux/make-deb.sh', build, out], out / 'build-installer.log')
            packages = list(out.glob('*.deb'))
            if len(packages) != 1:
                raise ValueError('Expected exactly one native Debian package')
            package = label_ci_package(packages[0], source)
            expanded = work / 'expanded'
            run(['dpkg-deb', '--extract', package, expanded], out / 'expand-installer.log')
            documents = expanded / 'usr/share/doc/cs80-reference'
            native_scope = 'Actual Debian package extracted; no system installation or audio execution'
        elif platform == 'macos':
            run(['bash', ROOT / 'installer/macos/make-pkg.sh', build, out], out / 'build-installer.log')
            packages = list(out.glob('*.pkg'))
            if len(packages) != 1:
                raise ValueError('Expected exactly one native macOS installer')
            package = label_ci_package(packages[0], source)
            expanded = work / 'expanded'
            # The native command itself is authoritative; failure cannot count as qualification.
            run(['pkgutil', '--expand-full', package, expanded], out / 'expand-installer.log')
            manifests = list(expanded.rglob('distribution-manifest.json'))
            if len(manifests) != 1:
                raise ValueError('Expected exactly one expanded document manifest')
            documents = manifests[0].parent
            native_scope = 'Actual macOS package fully expanded; no system installation or signing qualification'
        else:
            stage = work / 'staged-documents'
            run([sys.executable, ROOT / 'scripts/distribution_docs.py', '--root', ROOT,
                 '--juce-license', licence, '--out-dir', stage], out / 'stage-documents.log')
            compiler = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Inno Setup 6/iscc.exe'
            if not compiler.is_file():
                raise ValueError('The native runner lacks its registered Inno Setup 6 compiler')
            match = re.search(r'^project\(CS80Reference VERSION ([0-9.]+)', (ROOT / 'CMakeLists.txt').read_text(), re.MULTILINE)
            if not match:
                raise ValueError('Project version is unavailable')
            run([compiler, f'/DAppVersion={match.group(1)}',
                 f'/DBuildDir={build / "CS80Reference_artefacts/Release"}',
                 f'/DOutDir={out}', f'/DDistributionDocs={stage}',
                 ROOT / 'installer/windows/cs80-reference.iss'], out / 'build-installer.log')
            packages = list(out.glob('*.exe'))
            if len(packages) != 1:
                raise ValueError('Expected exactly one native Windows installer')
            package = label_ci_package(packages[0], source)
            installed = work / 'standalone-only'
            documents = installed / 'documentation'
            native_scope = 'Actual Inno Setup Standalone-only installation in ephemeral runner temp; no VST3 or DAW test'
        primary_error = None
        try:
            if platform == 'windows':
                run([package, '/SP-', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART',
                     '/NOICONS', '/COMPONENTS=standalone', f'/DIR={installed}',
                     f'/LOG={out / "setup.log"}'], out / 'install-process.log')
            receipt = verify_documents(ROOT, licence, documents, source)
            (out / 'document-verification.json').write_text(json.dumps(receipt, indent=2) + '\n')
        except BaseException as error:
            primary_error = error
            raise
        finally:
            if platform == 'windows':
                uninstaller = installed / 'unins000.exe'
                if uninstaller.is_file():
                    try:
                        run([uninstaller, '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART'], out / 'uninstall.log')
                    except (OSError, subprocess.SubprocessError) as cleanup_error:
                        if primary_error is None:
                            raise
                        # A partial install must attempt cleanup even after a
                        # nonzero/timeout. Preserve the primary failed action,
                        # not a misleading replacement cleanup failure.
                        message = f'Cleanup also failed; original failure retained: {cleanup_error}\n'
                        try:
                            (out / 'uninstall-failure.log').write_text(message)
                        except OSError:
                            print(message, file=sys.stderr)
        evidence = {
            'schema_version': 1, 'source_commit': source, 'platform': platform,
            'package': package.name, 'package_bytes': package.stat().st_size,
            'package_sha256': hashlib.sha256(package.read_bytes()).hexdigest(),
            'document_verification_sha256': hashlib.sha256((out / 'document-verification.json').read_bytes()).hexdigest(),
            'scope': native_scope,
            'qualification': 'CI installer document evidence only; not a release, binary/source, numerical, signing, hardware or real-DAW certificate',
        }
        (out / 'native-installer-receipt.json').write_text(json.dumps(evidence, indent=2) + '\n')
        print(json.dumps(evidence))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', required=True, choices=('linux', 'macos', 'windows'))
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--out-dir', type=Path, required=True)
    parser.add_argument('--source-sha', required=True)
    args = parser.parse_args()
    try:
        qualify(args.platform, args.build_dir, args.out_dir, args.source_sha)
        return 0
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f'Native installer qualification failed: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
