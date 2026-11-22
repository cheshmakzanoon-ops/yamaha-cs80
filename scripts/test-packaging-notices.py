#!/usr/bin/env python3
"""Focused notice/guide payload and revision tests. All binaries/evidence are synthetic.

No product builds, signing, host validation, workflow reads, or network access.
Linux compiles inert ELF fixtures (never executed) and builds/extracts a synthetic
.deb using isolated dependency metadata when the required tools are present.
macOS/Windows checks cover notice source mappings, not native installers.
"""
from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
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
    "libxml2-Copyright.txt": "src/vendor/libxml2/Copyright",
    "libxml2-SOURCE.txt": "src/vendor/libxml2/SOURCE.txt",
    "libxml2-ADDITIONAL-NOTICES.txt": "src/vendor/libxml2/ADDITIONAL-NOTICES.txt",
}
spec = importlib.util.spec_from_file_location("packaging", ROOT / "scripts/package-ci-artifacts.py")
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def products(base, system, arch="x86_64"):
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
    for index, binary in enumerate(binaries):
        binary.parent.mkdir(parents=True, exist_ok=True)
        # Header-only inert inputs satisfy archive identity checks without
        # introducing executable fixture code or bypassing production checks.
        binary.write_bytes(packaging.binary_fixture(system, arch, index == len(binaries) - 1))
        binary.chmod(0o755)


class NoticeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cs80-notice-test-")
        self.addCleanup(self.temp.cleanup)
        self.temp_root = Path(self.temp.name)
        self.expected = {name: (ROOT / source).read_bytes() for name, source in NOTICES.items()}
        for data in self.expected.values():
            self.assertTrue(data)

    def ci_context(self, source_changes=None):
        # An isolated clean checkout exercises the real read_ci_context path,
        # including commit/evidence checks, without changing the working repo.
        context = Path(tempfile.mkdtemp(prefix="ci-", dir=self.temp_root))
        source = context / "source"
        source.mkdir()
        for relative in [*NOTICES.values(), "LICENSE", "docs/INSTALL_AND_QUICKSTART.md",
                         "docs/CI_DISTRIBUTION_STATUS.md", "docs/PRESET_GUIDE.md", "src/core/FactoryPresets.h"]:
            target = source / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        for relative, data in (source_changes or {}).items():
            target = source / relative
            if data is None:
                target.unlink()
            else:
                target.write_bytes(data)
        run("git", "init", "--quiet", str(source))
        run("git", "-C", str(source), "add", ".")
        run("git", "-C", str(source), "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "-c", "commit.gpgsign=false", "commit", "--quiet", "-m", "Synthetic source fixture")
        commit = run("git", "-C", str(source), "rev-parse", "HEAD").strip()
        build = context / "build"
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
        expected = {"licenses/" + name: data for name, data in self.expected.items()}
        guide = extras["PRESET_GUIDE.md"].decode()
        self.assertIn(f"[Lead voicings](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/{commit}/src/core/curated/Leads.inc)", guide)
        self.assertIn("[playing and mode instructions](INSTALL_AND_QUICKSTART.md#choose-the-mode-before-playing)", guide)
        self.assertIn(f"[2026.11.1 refinement report](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/{commit}/docs/PRESET_REFINEMENT_2026_11_1.md)", guide)
        self.assertNotIn("](../", guide)
        revision = metadata["preset_library_revision"]
        self.assertIn(f'curatedLibraryRevision = "{revision}";',
                      (ROOT / "src/core/FactoryPresets.h").read_text(encoding="utf-8"))
        self.assertIn(f"Preset library revision: {revision}\n".encode(), extras["SOURCE.txt"])
        self.assertIn((f"Online preset guide: https://github.com/cheshmakzanoon-ops/yamaha-cs80/"
                       f"blob/{commit}/docs/PRESET_GUIDE.md\n").encode(), extras["SOURCE.txt"])
        for relative, data in expected.items():
            self.assertEqual(extras[relative], data)
        for system, arch in (("linux", "x86_64"), ("macos", "x86_64"),
                             ("macos", "arm64"), ("windows", "x86_64")):
            with self.subTest(system=system, arch=arch):
                base = self.temp_root / f"{system}-{arch}" / "products"
                products(base, system, arch)
                archive, manifest = packaging.package(base, base.parent / "out", system, arch, commit, metadata, extras)
                prefix = archive.name.removesuffix(".tar.gz").removesuffix(".zip")
                inventory = {item["path"]: item for item in manifest["files"]}
                self.assertEqual(manifest["preset_library_revision"], revision)
                self.assertEqual(json.loads((base.parent / "out" / (prefix + ".manifest.json")).read_bytes()), manifest)
                with (zipfile.ZipFile(archive) if system == "windows" else tarfile.open(archive)) as reader:
                    # Read the actual payload, including revision text and the
                    # embedded manifest; merely checking the extras map would
                    # not catch an archive omitting or altering these bytes.
                    for relative, data in extras.items():
                        member = prefix + "/" + relative
                        actual = reader.read(member) if system == "windows" else reader.extractfile(member).read()
                        self.assertEqual(actual, data)
                        self.assertEqual(inventory[relative]["sha256"], packaging.digest(data))
                        self.assertEqual(inventory[relative]["size"], len(data))
                    member = prefix + "/manifest.json"
                    embedded = reader.read(member) if system == "windows" else reader.extractfile(member).read()
                    self.assertEqual(json.loads(embedded), manifest)

    def test_bundled_guide_links_keep_local_documents_and_pin_source(self):
        source = (
            "[Install](./INSTALL_AND_QUICKSTART.md#start)\n"
            "[Status](CI_DISTRIBUTION_STATUS.md)\n"
            "[Source](../src/core/Engine.cpp#L1)\n"
            "[Report](PRESET_REFINEMENT_2026_11_1.md?plain=1#results)\n"
            "[External](https://example.invalid/guide?q=1#part) [Here](#start)\n"
            "```md\n[Example](../not-a-real-source.md)\n```\n"
        )
        commit, _, extras = self.ci_context({"docs/PRESET_GUIDE.md": source.encode()})
        prefix = f"https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/{commit}"
        expected = (
            "[Install](INSTALL_AND_QUICKSTART.md#start)\n"
            "[Status](DISTRIBUTION_STATUS.md)\n"
            f"[Source]({prefix}/src/core/Engine.cpp#L1)\n"
            f"[Report]({prefix}/docs/PRESET_REFINEMENT_2026_11_1.md?plain=1#results)\n"
            "[External](https://example.invalid/guide?q=1#part) [Here](#start)\n"
            "```md\n[Example](../not-a-real-source.md)\n```\n"
        )
        self.assertEqual(extras["PRESET_GUIDE.md"].decode(), expected)

    def test_changed_source_revision_propagates_without_packager_update(self):
        for revision in ("2042.07", "2042.07.1", "2042.07.12"):
            with self.subTest(revision=revision):
                _, metadata, extras = self.ci_context({
                    "src/core/FactoryPresets.h":
                        f'inline constexpr const char* curatedLibraryRevision = "{revision}";\n'.encode()})
                self.assertEqual(metadata["preset_library_revision"], revision)
                self.assertIn(f"Preset library revision: {revision}\n".encode(), extras["SOURCE.txt"])

    def test_missing_or_invalid_source_revision_is_rejected(self):
        declarations = (
            None,
            b"// The sound-data revision is missing.\n",
            b'inline constexpr const char* curatedLibraryRevision = "";\n',
            b'inline constexpr const char* curatedLibraryRevision = "2042.13";\n',
            b'inline constexpr const char* curatedLibraryRevision = "2042.7";\n',
            b'inline constexpr const char* curatedLibraryRevision = "2042.13.1";\n',
            b'inline constexpr const char* curatedLibraryRevision = "2042.00.1";\n',
            b'inline constexpr const char* curatedLibraryRevision = "2042.07.0";\n',
            b'inline constexpr const char* curatedLibraryRevision = "2042.07.01";\n',
            b'inline constexpr const char* curatedLibraryRevision = "2042.07.-1";\n',
            b'inline constexpr const char* curatedLibraryRevision = "2042.07.1extra";\n',
            b'inline constexpr const char* curatedLibraryRevision = "2042.07.1.2";\n',
            b'inline constexpr const char* curatedLibraryRevision = "2042.07.";\n',
            b'inline constexpr const char* curatedLibraryRevision = "2042.07\\nforged metadata";\n',
            b'inline constexpr const char* curatedLibraryRevision = "2042.07";\n' * 2,
        )
        for declaration in declarations:
            with self.subTest(declaration=declaration):
                with self.assertRaisesRegex(ValueError, "revision|curatedLibraryRevision"):
                    self.ci_context({"src/core/FactoryPresets.h": declaration})

    def test_missing_preset_guide_is_rejected(self):
        with self.assertRaises(FileNotFoundError):
            self.ci_context({"docs/PRESET_GUIDE.md": None})

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

    @unittest.skipUnless(sys.platform == "linux" and platform.machine().lower() in ("x86_64", "amd64")
                         and all(shutil.which(tool) for tool in
                         ("bash", "cc", "readelf", "dpkg-deb", "dpkg", "dpkg-shlibdeps")),
                         "Native Linux amd64, Debian packaging tools and cc required")
    def test_synthetic_deb_notice_bytes(self):
        build = self.temp_root / "build"
        # Only the Debian installer needs real ELF input. Archive fixtures above
        # are inert identity headers, not executable products. Neither ELF is executed.
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
