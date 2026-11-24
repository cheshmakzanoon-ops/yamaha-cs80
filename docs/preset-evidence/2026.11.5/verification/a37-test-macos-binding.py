#!/usr/bin/env python3
"""Synthetic, inert archive fixtures only; never native .5 qualification."""
from __future__ import annotations
import copy
import importlib.util
import io
import json
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


helper = load('a37_binding', HERE / 'a37-verify-macos-binding.py')
verifier = load('a37_fixture_verifier', ROOT / 'scripts/verify-ci-bundle.py')
packager = verifier.packager


class BindingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='a37-inert-fixtures-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.expected = {'source': 'e' * 40, 'run': 123456789, 'attempt': 2,
                         'architecture': 'arm64', 'revision': '2026.11.5',
                         'repository': 'cheshmakzanoon-ops/yamaha-cs80'}
        self.inputs = {name: self.root / name for name in ('install.zip', 'install-api.json',
                                                          'pluginval.zip', 'pluginval-api.json')}
        self.build_fixture()

    def write_zip(self, name, files):
        with zipfile.ZipFile(self.inputs[name], 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for path, data in files.items():
                archive.writestr(path, data)

    def refresh_api(self, name):
        zip_path = self.inputs[name + '.zip']
        api_path = self.inputs[name + '-api.json']
        api = json.loads(api_path.read_text())
        raw = zip_path.read_bytes()
        api.update(digest='sha256:' + helper.digest(raw), size_in_bytes=len(raw))
        api_path.write_text(json.dumps(api))

    def create_api(self, name, artifact_id, artifact_name):
        base = f"https://api.github.com/repos/{self.expected['repository']}/actions/artifacts/{artifact_id}"
        api = {'id': artifact_id, 'name': artifact_name, 'url': base,
               'archive_download_url': base + '/zip', 'expired': False,
               'created_at': '2026-11-24T00:00:00Z', 'expires_at': '2027-02-24T00:00:00Z',
               'workflow_run': {'id': self.expected['run'], 'head_sha': self.expected['source'],
                                'repository_id': 123, 'head_repository_id': 123}}
        self.inputs[name + '-api.json'].write_text(json.dumps(api))
        self.refresh_api(name)

    def build_fixture(self):
        architecture = self.expected['architecture']
        base = self.root / ('products-' + architecture)
        for name, bundle in helper.BUNDLES.items():
            executable = base / bundle / 'Contents/MacOS' / helper.PRODUCT
            executable.parent.mkdir(parents=True)
            executable.write_bytes(packager.binary_fixture('macos', architecture, name == 'Standalone'))
            executable.chmod(0o755)
            (base / bundle / 'Contents/Info.plist').write_text('INERT SYNTHETIC PLIST')
            (base / bundle / 'Contents/Resources').mkdir()
            (base / bundle / 'Contents/Resources/fixture.txt').write_text(name + ' complete resource fixture')
        provenance = {'verification': 'PASS', 'source_clean': 'true', 'actual_sha': packager.JUCE_SHA,
                      'actual_version': '8.0.11', 'workflow_commit': self.expected['source']}
        extras = {'SOURCE.txt': (f"Source commit: {self.expected['source']}\n"
                                f"Preset library revision: {self.expected['revision']}\n").encode(),
                  'evidence/juce-provenance.txt': ''.join(f'{k}={v}\n' for k, v in provenance.items()).encode()}
        metadata = {'preset_library_revision': self.expected['revision'],
                    'workflow_run_id': str(self.expected['run']), 'juce_sha': packager.JUCE_SHA,
                    'juce_version': '8.0.11'}
        out = self.root / ('packaged-' + architecture)
        archive, manifest = packager.package(base, out, 'macos', architecture,
                                             self.expected['source'], metadata, extras)
        self.write_zip('install.zip', {p.name: p.read_bytes() for p in out.iterdir()})
        au = base / helper.BUNDLES['AU'] / 'Contents/MacOS' / helper.PRODUCT
        self.au_hash = helper.digest(au.read_bytes())
        self.workspace = '/Users/native/work/yamaha-cs80/yamaha-cs80'
        self.home = '/Users/native'
        self.au_source = self.workspace + '/build/CS80Reference_artefacts/Release/' + helper.BUNDLES['AU']
        self.au_installed = self.home + '/Library/Audio/Plug-Ins/Components/' + helper.PRODUCT + '.component'
        suffix = '/Contents/MacOS/' + helper.PRODUCT
        hashes = ''.join(self.au_hash + '  ' + p + suffix + '\n'
                         for p in (self.au_source, self.au_installed)).encode()
        evidence = {name: hashes for name in ('au-payload-before-validation.sha256',
                                              'au-payload-after-validation.sha256')}
        for lane, filename in helper.LANES.items():
            path = (self.workspace + '/build/CS80Reference_artefacts/Release/' + helper.BUNDLES['VST3']
                    if lane == 'vst3' else self.au_installed)
            # Deliberately no SUCCESS or validator report: path checks must not
            # masquerade as native validator success or release qualification.
            evidence[filename] = ('Started validating: ' + path + '\nINERT PATH FIXTURE ONLY\n').encode()
        self.write_zip('pluginval.zip', evidence)
        runner = 'macos-15' if architecture == 'arm64' else 'macos-15-intel'
        self.create_api('install', 111, f'cs80-{runner}-install-bundle')
        self.create_api('pluginval', 112, f'cs80-pluginval-{runner}')

    def check(self):
        with tempfile.TemporaryDirectory(dir=self.root) as scratch:
            return helper.inspect_archives(self.expected, self.inputs['install.zip'], self.inputs['install-api.json'],
                                           self.inputs['pluginval.zip'], self.inputs['pluginval-api.json'],
                                           verifier, {'fixture': 'CURRENT SCRIPTS; SYNTHETIC INPUTS ONLY'}, Path(scratch))

    def mutate_evidence(self, mutation):
        files = helper.zip_members(self.inputs['pluginval.zip'].read_bytes())
        mutation(files)
        self.write_zip('pluginval.zip', files)
        self.refresh_api('pluginval')

    def mutate_bundle(self, mutation):
        files = helper.zip_members(self.inputs['install.zip'].read_bytes())
        inner = next(name for name in files if name.endswith('.tar.gz'))
        sidecar = next(name for name in files if name.endswith('.manifest.json'))
        manifest = json.loads(files[sidecar])
        entries = []
        with tarfile.open(fileobj=io.BytesIO(files[inner]), mode='r:gz') as archive:
            for entry in archive:
                entries.append((entry, archive.extractfile(entry).read() if entry.isfile() else None))
        mutation(manifest, entries)
        raw_manifest = (json.dumps(manifest, indent=2, sort_keys=True) + '\n').encode()
        output = io.BytesIO()
        with tarfile.open(fileobj=output, mode='w:gz') as archive:
            for entry, data in entries:
                if entry.name.endswith('/manifest.json'):
                    data = raw_manifest
                if data is not None:
                    entry.size = len(data)
                archive.addfile(entry, io.BytesIO(data) if data is not None else None)
        files[inner], files[sidecar] = output.getvalue(), raw_manifest
        files['SHA256SUMS'] = ''.join(helper.digest(files[name]) + '  ' + name + '\n'
                                     for name in (inner, sidecar)).encode()
        self.write_zip('install.zip', files)
        self.refresh_api('install')

    def test_valid_arm64_keeps_all_original_gates_open(self):
        result = self.check()
        self.assertFalse(result['release_qualified'])
        self.assertFalse(result['validated_bundle_identity_established'])
        self.assertFalse(result['run_attempt_bound'])
        self.assertEqual(len(result['remaining_gates']), 8)
        self.assertEqual(set(result['packaged_bundles']), {'AU', 'VST3', 'Standalone'})
        self.assertTrue(all(any(i['path'].endswith('/Resources/fixture.txt') for i in b['inventory'])
                            for b in result['packaged_bundles'].values()))

    def test_valid_x86_64(self):
        self.expected['architecture'] = 'x86_64'
        self.build_fixture()
        self.assertEqual(self.check()['packaged_bundles']['AU']['advertised_architecture_present'], 'x86_64')

    def test_attempt_is_explicit_but_not_falsely_bound(self):
        self.expected['attempt'] = 99
        result = self.check()
        self.assertEqual(result['expected']['attempt'], 99)
        self.assertFalse(result['run_attempt_bound'])

    def test_api_digest_tamper(self):
        with self.inputs['install.zip'].open('ab') as stream:
            stream.write(b'tamper')
        with self.assertRaisesRegex(ValueError, 'digest/size'):
            self.check()

    def test_digest_and_inspection_share_the_same_snapshot(self):
        path = self.inputs['install.zip']
        original = path.read_bytes()
        snapshot = helper.snapshot

        def replace_after_read(input_path, limit):
            raw = snapshot(input_path, limit)
            if Path(input_path) == path:
                path.write_bytes(b'replaced after immutable snapshot')
            return raw

        with patch.object(helper, 'snapshot', side_effect=replace_after_read):
            result = self.check()
        self.assertEqual(result['downloads']['install']['sha256'], helper.digest(original))
        self.assertNotEqual(result['downloads']['install']['sha256'], helper.digest(path.read_bytes()))

    def test_exact_committed_source_loader_and_revision_guard(self):
        source = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
        header = subprocess.check_output(['git', '-C', str(ROOT), 'show', source + ':src/core/FactoryPresets.h'])
        revision = re.findall(rb'curatedLibraryRevision = "([^"\r\n]+)"', header)[0].decode()
        loaded, records = helper.load_source_contract(ROOT, source, revision, self.root)
        self.assertEqual(records['src/core/FactoryPresets.h']['sha256'], helper.digest(header))
        self.assertTrue(callable(loaded.verify))
        with self.assertRaisesRegex(ValueError, 'source preset revision'):
            helper.load_source_contract(ROOT, source, '1900.01', self.root)

    def test_api_run_source_and_repository_mismatch(self):
        path = self.inputs['install-api.json']
        original = json.loads(path.read_text())
        for key, value in (('id', 1), ('head_sha', 'f' * 40), ('repository_id', 0)):
            with self.subTest(key=key):
                data = copy.deepcopy(original)
                data['workflow_run'][key] = value
                path.write_text(json.dumps(data))
                with self.assertRaises(ValueError):
                    self.check()
        path.write_text(json.dumps(original))

    def test_manifest_run_mismatch(self):
        self.mutate_bundle(lambda manifest, entries: manifest.update(workflow_run_id='42'))
        with self.assertRaisesRegex(ValueError, 'Manifest workflow run'):
            self.check()

    def test_manifest_revision_mismatch(self):
        self.mutate_bundle(lambda manifest, entries: manifest.update(preset_library_revision='2026.11.4'))
        with self.assertRaisesRegex(ValueError, 'preset revision'):
            self.check()

    def test_missing_resource_is_not_replaced_by_executable_only_check(self):
        def mutation(manifest, entries):
            entries[:] = [(entry, data) for entry, data in entries
                          if not entry.name.endswith('AU/CS-80 Reference.component/Contents/Resources/fixture.txt')]
        self.mutate_bundle(mutation)
        with self.assertRaisesRegex(ValueError, 'Missing archive members'):
            self.check()

    def test_self_consistent_wrong_architecture_is_rejected(self):
        def mutation(manifest, entries):
            path = helper.BUNDLES['VST3'] + '/Contents/MacOS/' + helper.PRODUCT
            data = packager.binary_fixture('macos', 'x86_64', False)
            for item in manifest['files']:
                if item['path'] == path:
                    item.update(size=len(data), sha256=helper.digest(data))
            entries[:] = [(entry, data if entry.name.endswith('/' + path) else content)
                          for entry, content in entries]
        self.mutate_bundle(mutation)
        with self.assertRaisesRegex(ValueError, 'Expected arm64 Mach-O'):
            self.check()

    def test_au_pre_post_mismatch(self):
        self.mutate_evidence(lambda files: files.update({'au-payload-after-validation.sha256':
                             files['au-payload-after-validation.sha256'].replace(self.au_hash.encode(), b'0' * 64)}))
        with self.assertRaisesRegex(ValueError, 'pre/post'):
            self.check()

    def test_au_matches_each_other_but_not_distributed_payload(self):
        def mutation(files):
            for name in ('au-payload-before-validation.sha256', 'au-payload-after-validation.sha256'):
                files[name] = files[name].replace(self.au_hash.encode(), b'0' * 64)
        self.mutate_evidence(mutation)
        with self.assertRaisesRegex(ValueError, 'distributed AU'):
            self.check()

    def test_wrong_validator_path(self):
        self.mutate_evidence(lambda files: files.update({'pluginval-au-gui.log':
                             b'Started validating: /tmp/substitute.component\n'}))
        with self.assertRaisesRegex(ValueError, 'input path'):
            self.check()

    def test_extra_validator_invocation(self):
        self.mutate_evidence(lambda files: files.update({'pluginval-vst3.log': files['pluginval-vst3.log'] * 2}))
        with self.assertRaisesRegex(ValueError, 'invocation count'):
            self.check()

    def test_unsafe_zip_path(self):
        self.mutate_evidence(lambda files: files.update({'../escape': b'no'}))
        with self.assertRaisesRegex(ValueError, 'Unsafe ZIP'):
            self.check()

    def test_duplicate_json_key(self):
        path = self.inputs['install-api.json']
        raw = path.read_text()
        path.write_text('{"id": 111,' + raw[1:])
        with self.assertRaisesRegex(ValueError, 'Duplicate JSON'):
            self.check()

    def test_fresh_output_required_before_source_access(self):
        output = self.root / 'existing.json'
        output.write_text('preserved evidence')
        args = ['--expected-source', self.expected['source'], '--expected-run', str(self.expected['run']),
                '--expected-attempt', '2', '--expected-architecture', 'arm64', '--expected-revision', '2026.11.5',
                '--install-zip', str(self.inputs['install.zip']), '--install-api', str(self.inputs['install-api.json']),
                '--pluginval-zip', str(self.inputs['pluginval.zip']), '--pluginval-api', str(self.inputs['pluginval-api.json']),
                '--out', str(output)]
        with self.assertRaisesRegex(ValueError, 'overwrite'):
            helper.main(args)
        self.assertEqual(output.read_text(), 'preserved evidence')


if __name__ == '__main__':
    unittest.main(verbosity=2)
