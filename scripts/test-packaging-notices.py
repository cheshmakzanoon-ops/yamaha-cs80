#!/usr/bin/env python3
"""Focused notice payload tests. All binaries/evidence are synthetic.

No product builds, signing, host validation, workflow reads, or network access.
Linux compiles inert ELF fixtures (never executed) and builds/extracts a synthetic
.deb using isolated dependency metadata when the required tools are present.
macOS/Windows checks cover notice source mappings, not native installers.
"""
from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
NOTICES = {
    "BarlowCondensed-OFL.txt": "assets/fonts/barlow-condensed/OFL.txt",
    "BarlowCondensed-SOURCE.txt": "assets/fonts/barlow-condensed/SOURCE.txt",
    "MTS-ESP-LICENSE.txt": "src/vendor/mts-esp/LICENSE",
}
spec = importlib.util.spec_from_file_location("packaging", ROOT / "scripts/package-ci-artifacts.py")
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def products(base, system):
    product = packaging.PRODUCT
    vst = base / "VST3" / f"{product}.vst3"
    if system == "macos":
        bundles = [vst, base / "AU" / f"{product}.component", base / "Standalone" / f"{product}.app"]
        binaries = [bundle / "Contents/MacOS" / product for bundle in bundles]
        for bundle in bundles:
            (bundle / "Contents").mkdir(parents=True)
            (bundle / "Contents/Info.plist").write_bytes(b"synthetic plist")
    elif system == "windows":
        binaries = [vst / "Contents/x86_64-win" / f"{product}.vst3", base / "Standalone" / f"{product}.exe"]
    else:
        binaries = [vst / "Contents/x86_64-linux" / f"{product}.so", base / "Standalone" / product]
    for binary in binaries:
        binary.parent.mkdir(parents=True, exist_ok=True)
        binary.write_bytes(b"synthetic, not a loadable plugin\x00")
        binary.chmod(0o755)


class NoticeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cs80-notice-test-")
        self.addCleanup(self.temp.cleanup)
        self.temp_root = Path(self.temp.name)
        self.expected = {name: (ROOT / source).read_bytes() for name, source in NOTICES.items()}
        for data in self.expected.values():
            self.assertTrue(data)

    def ci_context(self):
        # An isolated clean checkout exercises the real read_ci_context path,
        # including commit/evidence checks, without changing the working repo.
        source = self.temp_root / "source"
        source.mkdir()
        for relative in [*NOTICES.values(), "LICENSE", "docs/INSTALL_AND_QUICKSTART.md", "docs/CI_DISTRIBUTION_STATUS.md"]:
            target = source / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        run("git", "init", "--quiet", str(source))
        run("git", "-C", str(source), "add", ".")
        run("git", "-C", str(source), "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "-c", "commit.gpgsign=false", "commit", "--quiet", "-m", "Synthetic source fixture")
        commit = run("git", "-C", str(source), "rev-parse", "HEAD").strip()
        build = self.temp_root / "build"
        juce = build / "_deps/juce-src"
        juce.mkdir(parents=True)
        (juce / "LICENSE.md").write_bytes(b"synthetic JUCE notice fixture\n")
        (build / "ci-juce-provenance.txt").write_text(
            f"verification=PASS\nsource_clean=true\nactual_sha={packaging.JUCE_SHA}\n"
            f"actual_version=8.0.11\nworkflow_commit={commit}\nconfigured_source={juce}\n")
        with patch.object(packaging, "ROOT", source), patch.dict(os.environ, {"GITHUB_SHA": commit}):
            return packaging.read_ci_context(build)

    def test_actual_ci_payload_and_archive_bytes(self):
        commit, metadata, extras = self.ci_context()
        for name, data in self.expected.items():
            self.assertEqual(extras["licenses/" + name], data)
        for system in ("linux", "macos", "windows"):
            with self.subTest(system=system):
                base = self.temp_root / system / "products"
                products(base, system)
                archive, manifest = packaging.package(base, base.parent / "out", system, "x86_64", commit, metadata, extras)
                prefix = archive.name.removesuffix(".tar.gz").removesuffix(".zip")
                inventory = {item["path"]: item for item in manifest["files"]}
                with (zipfile.ZipFile(archive) if system == "windows" else tarfile.open(archive)) as reader:
                    for name, data in self.expected.items():
                        relative = "licenses/" + name
                        member = prefix + "/" + relative
                        actual = reader.read(member) if system == "windows" else reader.extractfile(member).read()
                        self.assertEqual(actual, data)
                        self.assertEqual(inventory[relative]["sha256"], packaging.digest(data))
                        self.assertEqual(inventory[relative]["size"], len(data))

    def test_native_installer_notice_source_mappings(self):
        # Inspect only the packaging entrypoints. Native compilers are not used.
        for script, destination in [("installer/linux/make-deb.sh", "$stage/usr/share/doc/cs80-reference"),
                                    ("installer/macos/make-pkg.sh", "$work/root-docs")]:
            content = (ROOT / script).read_text()
            mappings = dict((dest, src) for src, dest in re.findall(r'^cp "\$root/([^"\n]+)" "([^"\n]+)"$', content, re.M))
            for name, data in self.expected.items():
                self.assertEqual((ROOT / mappings[destination + "/" + name]).read_bytes(), data)
        script = ROOT / "installer/windows/cs80-reference.iss"
        mappings = re.findall(r'^Source: "([^"]+)"; DestDir: "\{app\}\\licenses"; DestName: "([^"]+)"; Flags: ignoreversion$', script.read_text(), re.M)
        sources = {name: source for source, name in mappings}
        for name, data in self.expected.items():
            self.assertEqual((script.parent / sources[name].replace("\\", "/")).read_bytes(), data)

    @unittest.skipUnless(os.name != "nt" and all(shutil.which(tool) for tool in
                         ("bash", "cc", "readelf", "dpkg-deb", "dpkg", "dpkg-shlibdeps")),
                         "Debian packaging tools and cc required")
    def test_synthetic_deb_notice_bytes(self):
        build = self.temp_root / "build"
        # Only the Debian installer needs real ELF input. Archive fixtures above
        # remain platform-independent byte payloads. Neither ELF is executed.
        base = build / "CS80Reference_artefacts/Release"
        product = packaging.PRODUCT
        standalone = base / "Standalone" / product
        plugin = base / "VST3" / f"{product}.vst3/Contents/x86_64-linux/{product}.so"
        standalone.parent.mkdir(parents=True)
        plugin.parent.mkdir(parents=True)
        source = self.temp_root / "fixture.c"
        source.write_text('#include <stdio.h>\nint main(void) { return puts("fixture"); }\n')
        run("cc", str(source), "-o", str(standalone))
        source.write_text('#include <stdio.h>\nint fixture(void) { return puts("fixture"); }\n')
        run("cc", "-shared", "-fPIC", str(source), "-o", str(plugin))

        # A synthetic database makes the real ABI scanner independent of the
        # host's installed-package database. This is not ABI qualification.
        db = self.temp_root / "dpkg-db"
        (db / "info").mkdir(parents=True)
        library = Path(run("cc", "-print-file-name=libc.so.6").strip()).resolve()
        self.assertTrue(library.is_file())
        paths = {str(library), str(library).replace("/usr/lib/", "/lib/", 1)}
        (db / "info/libc6.list").write_text("\n".join(sorted(paths)) + "\n")
        (db / "info/libc6.shlibs").write_text("libc 6 libc6 (>= 2.36)\n")
        arch = run("dpkg", "--print-architecture").strip()
        (db / "status").write_text(
            f"Package: libc6\nStatus: install ok installed\nArchitecture: {arch}\n"
            "Version: 2.36\nMaintainer: Fixture <test@example.invalid>\n"
            "Description: Synthetic packaging fixture\n")
        juce = build / "_deps/juce-src/LICENSE.md"
        juce.parent.mkdir(parents=True)
        juce.write_bytes(b"synthetic JUCE notice fixture\n")
        out = self.temp_root / "out"
        with patch.dict(os.environ, {"DPKG_ADMINDIR": str(db)}):
            run("bash", str(ROOT / "installer/linux/make-deb.sh"), str(build), str(out))
        archives = list(out.glob("*.deb"))
        self.assertEqual(len(archives), 1)
        extracted = self.temp_root / "extracted"
        run("dpkg-deb", "--extract", str(archives[0]), str(extracted))
        docs = extracted / "usr/share/doc/cs80-reference"
        for name, data in self.expected.items():
            self.assertEqual((docs / name).read_bytes(), data)
        self.assertEqual((docs / "copyright").read_bytes(), (ROOT / "LICENSE").read_bytes())
        self.assertEqual((docs / "JUCE-LICENSE.md").read_bytes(), juce.read_bytes())


if __name__ == "__main__":
    unittest.main(verbosity=2)
