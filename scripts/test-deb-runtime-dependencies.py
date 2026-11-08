#!/usr/bin/env python3
"""Exercise the actual Debian packager and dependency scanner with inert ELF fixtures.

Fixtures are never executed. An isolated synthetic dpkg database supplies known
shlibs metadata, so this also runs on containers without an installed-package DB.
This verifies packaging, not plugin loading or native DAW qualification.
"""
from pathlib import Path
import os
import io
import stat
import tarfile
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ('cc', 'readelf', 'realpath', 'dpkg', 'dpkg-deb', 'dpkg-shlibdeps')


def run(*args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, **kwargs).stdout


@unittest.skipUnless(all(shutil.which(t) for t in TOOLS), 'Debian packaging tools and cc required')
class DependencyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cs80-deb-deps-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / 'build with spaces'
        artifacts = self.build / 'CS80Reference_artefacts/Release'
        self.standalone = artifacts / 'Standalone/CS-80 Reference'
        self.plugin = artifacts / 'VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
        self.standalone.parent.mkdir(parents=True)
        self.plugin.parent.mkdir(parents=True)
        source = self.root / 'fixture.c'
        source.write_text('#include <stdio.h>\nint main(void) { puts("fixture"); return 0; }\n')
        run('cc', '-fPIE', '-pie', str(source), '-o', str(self.standalone))
        source.write_text('#include <math.h>\ndouble fixture(double x) { return sin(x); }\n')
        run('cc', '-shared', '-fPIC', '-nostdlib', str(source), '-lm', '-o', str(self.plugin))
        juce = self.build / '_deps/juce-src/LICENSE.md'
        juce.parent.mkdir(parents=True)
        juce.write_text('Synthetic JUCE licence fixture\n')
        db = self.root / 'dpkg-db'
        (db / 'info').mkdir(parents=True)
        # Known shlibs entries deliberately assign different packages/ABI floors
        # to the executable's libc and the shared object's libm. Both must appear.
        statuses = []
        for soname, package, version in [('libc.so.6', 'libc6', '2.36'),
                                          ('libm.so.6', 'fixture-math', '7.8')]:
            library = Path(run('cc', '-print-file-name=' + soname).strip()).resolve()
            self.assertTrue(library.is_file(), soname)
            paths = {str(library), str(library).replace('/usr/lib/', '/lib/', 1)}
            (db / 'info' / (package + '.list')).write_text('\n'.join(sorted(paths)) + '\n')
            (db / 'info' / (package + '.shlibs')).write_text(
                f'{soname.split(".")[0]} 6 {package} (>= {version})\n')
            statuses.append(f'Package: {package}\nStatus: install ok installed\n'
                            f'Architecture: {run("dpkg", "--print-architecture").strip()}\n'
                            f'Version: {version}\nMaintainer: Fixture <test@example.invalid>\n'
                            'Description: Synthetic packaging fixture\n')
        (db / 'status').write_text('\n'.join(statuses))
        self.env = dict(os.environ, DPKG_ADMINDIR=str(db))
        self.out = self.root / 'output with spaces'

    def package(self, check=True, umask=-1):
        return subprocess.run([shutil.which('bash'), str(ROOT / 'installer/linux/make-deb.sh'),
                               str(self.build), str(self.out)], env=self.env,
                              text=True, capture_output=True, check=check, umask=umask)

    def test_non_amd64_packaging_host_rejected(self):
        # Simulate dpkg's architecture answer only; payloads are inert x64 ELF.
        tools = self.root / 'foreign-host-tools'
        tools.mkdir()
        dpkg = tools / 'dpkg'
        dpkg.write_text('#!/bin/sh\nprintf "arm64\\n"\n')
        dpkg.chmod(0o755)
        self.env['PATH'] = str(tools) + os.pathsep + os.environ['PATH']
        result = self.package(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('requires an amd64 packaging host', result.stderr)
        self.assertFalse(list(self.out.glob('*.deb')))

    def test_foreign_machine_products_rejected(self):
        for binary, role in ((self.plugin, 'VST3'), (self.standalone, 'Standalone')):
            with self.subTest(role=role):
                original = binary.read_bytes()
                self.assertEqual(original[:6], b'\x7fELF\x02\x01')
                modified = bytearray(original)
                # e_machine = EM_AARCH64. The fixture is inspected, never run.
                modified[18:20] = (183).to_bytes(2, 'little')
                binary.write_bytes(modified)
                try:
                    result = self.package(check=False)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('invalid ELF architecture for ' + role, result.stderr)
                    self.assertFalse(list(self.out.glob('*.deb')))
                finally:
                    binary.write_bytes(original)

    def test_versioned_dependencies_from_both_payloads(self):
        self.package()
        archives = list(self.out.glob('*.deb'))
        self.assertEqual(len(archives), 1)
        deps = run('dpkg-deb', '-f', str(archives[0]), 'Depends').strip()
        self.assertIn('libc6 (>= 2.36)', deps)
        self.assertIn('fixture-math (>= 7.8)', deps)
        self.assertIn('libasound2t64 | libasound2', deps)
        self.assertIn('libxcursor1', deps)
        extracted = self.root / 'extracted'
        run('dpkg-deb', '-x', str(archives[0]), str(extracted))
        self.assertEqual((extracted / 'usr/bin/cs80-reference').read_bytes(), self.standalone.read_bytes())
        self.assertFalse((extracted / 'debian').exists())

    def test_restrictive_caller_umask_preserves_package_modes(self):
        # Before the fix, caller umask 077 creates DEBIAN as 0700 and
        # dpkg-deb exits 2: "control directory has bad permissions 700".
        # This child-only setting does not change the test runner's umask.
        self.plugin.chmod(0o751)
        self.standalone.chmod(0o711)
        self.package(umask=0o077)
        archives = list(self.out.glob('*.deb'))
        self.assertEqual(len(archives), 1)
        data = subprocess.check_output(['dpkg-deb', '--fsys-tarfile', str(archives[0])])
        with tarfile.open(fileobj=io.BytesIO(data), mode='r:') as payload:
            members = {item.name.removeprefix('./').rstrip('/'): item for item in payload}
            for directory in ('usr', 'usr/bin', 'usr/lib', 'usr/lib/vst3',
                              'usr/share', 'usr/share/doc', 'usr/share/doc/cs80-reference'):
                self.assertTrue(members[directory].isdir(), directory)
                self.assertEqual(stat.S_IMODE(members[directory].mode), 0o755, directory)
            docs = [item for name, item in members.items()
                    if name.startswith('usr/share/doc/cs80-reference/') and item.isfile()]
            self.assertGreaterEqual(len(docs), 6)
            for item in docs:
                self.assertEqual(stat.S_IMODE(item.mode), 0o644, item.name)
            installed = members['usr/bin/cs80-reference']
            plugin = members['usr/lib/vst3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so']
            self.assertEqual(stat.S_IMODE(installed.mode), 0o755)  # install -m 0755
            self.assertEqual(stat.S_IMODE(plugin.mode), 0o751)     # cp -a preserves payload mode
            self.assertEqual(payload.extractfile(installed).read(), self.standalone.read_bytes())
            self.assertEqual(payload.extractfile(plugin).read(), self.plugin.read_bytes())

    def test_missing_scanner_fails_without_package(self):
        bin_dir = self.root / 'tools-without-scanner'
        bin_dir.mkdir()
        for name in ('dirname', 'sed', 'dpkg', 'mktemp', 'mkdir', 'cp', 'install', 'rm'):
            (bin_dir / name).symlink_to(shutil.which(name))
        self.env['PATH'] = str(bin_dir)
        result = self.package(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('dpkg-shlibdeps is required', result.stderr)
        self.assertFalse(list(self.out.glob('*.deb')))

    def test_empty_scanner_output_fails_without_package(self):
        bin_dir = self.root / 'empty-scanner'
        bin_dir.mkdir()
        scanner = bin_dir / 'dpkg-shlibdeps'
        scanner.write_text('#!/bin/sh\nexit 0\n')
        scanner.chmod(0o755)
        self.env['PATH'] = str(bin_dir) + os.pathsep + os.environ['PATH']
        result = self.package(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('no ELF runtime dependencies found', result.stderr)
        self.assertFalse(list(self.out.glob('*.deb')))

    def test_non_elf_plugin_fails_without_package(self):
        self.plugin.write_text('not an ELF shared object\n')
        result = self.package(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('invalid ELF product', result.stderr)
        self.assertFalse(list(self.out.glob('*.deb')))

    def test_non_elf_standalone_fails_without_package(self):
        self.standalone.write_text('not an ELF executable\n')
        result = self.package(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('invalid ELF product', result.stderr)
        self.assertFalse(list(self.out.glob('*.deb')))

    def test_non_elf_products_fail_without_package(self):
        self.standalone.write_text('not an ELF executable\n')
        self.plugin.write_text('not an ELF shared object\n')
        result = self.package(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(list(self.out.glob('*.deb')))

    def test_missing_dependency_metadata_fails_without_package(self):
        (Path(self.env['DPKG_ADMINDIR']) / 'info/fixture-math.shlibs').unlink()
        result = self.package(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('no dependency information', result.stderr)
        self.assertFalse(list(self.out.glob('*.deb')))

    def test_non_pie_standalone_is_accepted(self):
        source = self.root / 'non-pie.c'
        source.write_text('#include <stdio.h>\nint main(void) { puts("fixture"); return 0; }\n')
        run('cc', '-fno-PIE', '-no-pie', str(source), '-o', str(self.standalone))
        self.package()
        self.assertEqual(len(list(self.out.glob('*.deb'))), 1)

    def test_relocatable_plugin_fails_without_package(self):
        source = self.root / 'relocatable.c'
        source.write_text('int fixture(void) { return 1; }\n')
        run('cc', '-c', '-fPIC', str(source), '-o', str(self.plugin))
        result = self.package(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('invalid ELF type for VST3: REL', result.stderr)
        self.assertFalse(list(self.out.glob('*.deb')))

    def test_relocatable_standalone_fails_without_package(self):
        source = self.root / 'relocatable.c'
        source.write_text('int fixture(void) { return 1; }\n')
        run('cc', '-c', str(source), '-o', str(self.standalone))
        self.standalone.chmod(0o755)
        result = self.package(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('invalid ELF type for Standalone: REL', result.stderr)
        self.assertFalse(list(self.out.glob('*.deb')))

    def test_non_pie_executable_plugin_fails_without_package(self):
        source = self.root / 'executable.c'
        source.write_text('#include <stdio.h>\nint main(void) { puts("fixture"); return 0; }\n')
        run('cc', '-fno-PIE', '-no-pie', str(source), '-o', str(self.plugin))
        result = self.package(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('invalid ELF type for VST3: EXEC', result.stderr)
        self.assertFalse(list(self.out.glob('*.deb')))

    def test_external_plugin_symlink_fails_without_package(self):
        target = self.root / 'external.so'
        self.plugin.rename(target)
        self.plugin.symlink_to(target)
        result = self.package(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('VST3 payload resolves outside packaged bundle', result.stderr)
        self.assertFalse(list(self.out.glob('*.deb')))

    def test_external_plugin_directory_symlink_fails_without_package(self):
        target = self.root / 'external module'
        self.plugin.parent.rename(target)
        self.plugin.parent.symlink_to(target, target_is_directory=True)
        result = self.package(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('VST3 payload resolves outside packaged bundle', result.stderr)
        self.assertFalse(list(self.out.glob('*.deb')))

    def test_internal_relative_plugin_symlink_is_delivered(self):
        target = self.plugin.with_name('payload.so')
        self.plugin.rename(target)
        self.plugin.symlink_to(target.name)
        self.package()
        archive, = self.out.glob('*.deb')
        extracted = self.root / 'extracted'
        run('dpkg-deb', '-x', str(archive), str(extracted))
        delivered = extracted / 'usr/lib/vst3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
        self.assertTrue(delivered.is_symlink())
        self.assertEqual(delivered.read_bytes(), target.read_bytes())

    def test_internal_relative_plugin_directory_symlink_is_delivered(self):
        target = self.plugin.parent.with_name('module')
        self.plugin.parent.rename(target)
        self.plugin.parent.symlink_to(target.name, target_is_directory=True)
        self.package()
        archive, = self.out.glob('*.deb')
        extracted = self.root / 'extracted'
        run('dpkg-deb', '-x', str(archive), str(extracted))
        delivered = extracted / 'usr/lib/vst3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
        self.assertTrue(delivered.parent.is_symlink())
        self.assertEqual(delivered.read_bytes(), self.plugin.read_bytes())

    def test_symlinked_temporary_directory_is_accepted(self):
        target = self.root / 'temporary directory'
        target.mkdir()
        link = self.root / 'temporary link'
        link.symlink_to(target, target_is_directory=True)
        self.env['TMPDIR'] = str(link)
        self.package()
        self.assertEqual(len(list(self.out.glob('*.deb'))), 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)

